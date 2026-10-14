#!/usr/bin/env python3
"""Actual procedural graph entry points with admitted shapes and owned nodes."""

import sys
import unittest
import uuid
from types import SimpleNamespace
from unittest.mock import Mock

from test_scene_node_ownership import Scene, DagNode, definitions
from test_python_rig_safety import functions
from DPyTools.core import strings


class Shape(DagNode):
    def getParent(self):
        return self.parent
    def __getattr__(self, name):
        if name in ('worldMesh', 'worldSpace'):
            return self.attr(name)
        return super().__getattr__(name)


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.scene = Scene()
        parent = DagNode('|group_l|char_l:upper_lip')
        self.curve = Shape('curveShape', 'nurbsCurve', parent)
        self.base = Shape('baseShape', 'mesh', DagNode('base'))
        self.target = Shape('targetShape', 'mesh', parent)
        self.scene.nodes.extend([parent, self.curve, self.base, self.target])
        owner_scope = definitions('python/DPyTools/scene/ownership.py',
                                  {'uuid': uuid, 'pm': self.scene.pm})
        self.resolve = Mock(side_effect=self.resolve_shape)
        self.scene.pm.setAttr = Mock(side_effect=lambda plug, value: plug.set(value))
        self.scene.pm.sets = Mock()
        self.scope = functions('python/DPyTools/features/generators.py',
                               {'pm': self.scene.pm, 'sys': sys, 'strings': strings,
                                'resolve_shape': self.resolve,
                                'ownership': SimpleNamespace(SceneNodes=owner_scope['SceneNodes'])})

    def resolve_shape(self, node, kinds):
        if not isinstance(node, Shape) or node.kind not in kinds or node.instanced:
            raise ValueError('invalid or ambiguous geometry')
        return node

    def test_empty_wrong_extra_and_ambiguous_selection_cannot_create_nodes(self):
        for selection in ([], [DagNode('empty')], [self.base], [self.curve, self.curve]):
            self.scene.selection = selection
            self.assertFalse(self.scope['createSpiralCurve']())
            self.assertFalse(self.scope['createPolyWire']())
        self.scene.pm.createNode.assert_not_called()

    def test_spiral_and_wire_use_registered_attributes_without_force_or_selection_clear(self):
        self.scene.selection = [self.curve]
        for function, kind, input_name, output_name in (
                ('createSpiralCurve', 'DSpiralCurve', 'inputCurve', 'outputCurve'),
                ('createPolyWire', 'DPolyWire', 'inCurve', 'outMesh')):
            generator, outputs = self.scope[function]()
            self.assertEqual(generator.kind, kind)
            self.assertEqual(len(outputs), 1)
            self.assertTrue(any(target.name == input_name for _, target, _ in self.scene.connections))
            self.assertTrue(any(source.name == output_name for source, _, _ in self.scene.connections))
        self.assertTrue(all(not kwargs.get('force') for _, _, kwargs in self.scene.connections))
        self.assertEqual(self.scene.selection, [self.curve])

    def test_flip_preserves_ordinary_word_and_namespace(self):
        self.scene.selection = [self.base, self.target]
        generator, outputs = self.scope['createBSFlipShape']()
        self.assertEqual(generator.kind, 'DBSFlipMesh')
        self.assertEqual(outputs[0].name(), ':char_l:upper_lip_flip')

    def test_flip_uses_bounded_diagonal_side_and_keeps_parent_namespace_out_of_match(self):
        self.target.parent.label = '|group_l|char_l:eye_ul_jnt'
        self.scene.selection = [self.base, self.target]
        self.assertEqual(self.scope['createBSFlipShape']()[1][0].name(), ':char_l:eye_ur_jnt')

    def test_lr_and_ud_keep_registered_outputs_and_historical_parameters(self):
        self.scene.selection = [self.base, self.target]
        generator, outputs = self.scope['createLRShape']()
        self.assertEqual([node.name() for node in outputs],
                         [':char_l:upper_lip_l', ':char_l:upper_lip_r'])
        self.assertEqual(generator.values['Distance'], 2)
        self.assertEqual(generator.values['BlendRamp[1].BlendRamp_Position'], 1.0)
        generator, outputs = self.scope['createUDShape']()
        self.assertEqual([node.name() for node in outputs],
                         [':char_l:upper_lip_u', ':char_l:upper_lip_d'])
        self.assertEqual(generator.values['Invert'], 0)

    def test_name_collision_or_ambiguous_side_fails_before_any_creation(self):
        self.scene.selection = [self.base, self.target]
        self.scene.nodes.append(DagNode(':char_l:upper_lip_flip'))
        self.assertFalse(self.scope['createBSFlipShape']())
        self.target.parent.label = 'bad_l_r'
        self.assertFalse(self.scope['createLRShape']())
        self.scene.pm.createNode.assert_not_called()

    def test_later_connection_failure_cleans_only_generated_graph(self):
        self.scene.selection = [self.base, self.target]
        self.scene.pm.connectAttr.side_effect = [None, None, RuntimeError('output connection failed')]
        self.assertFalse(self.scope['createBSFlipShape']())
        self.assertTrue(all(not node.exists for node in self.scene.created))
        self.assertTrue(self.base.exists and self.target.exists)

    def test_creation_failure_after_output_parent_is_cleaned(self):
        self.scene.selection = [self.curve]
        original = self.scene.pm.createNode.side_effect
        def fail(kind, **kwargs):
            if kind == 'mesh':
                raise RuntimeError('mesh allocation failed')
            return original(kind, **kwargs)
        self.scene.pm.createNode.side_effect = fail
        self.assertFalse(self.scope['createPolyWire']())
        self.assertTrue(all(not node.exists for node in self.scene.created))

    def test_interruption_is_not_masked_and_known_nodes_are_cleaned(self):
        self.scene.selection = [self.curve]
        self.scene.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.scope['createSpiralCurve']()
        self.assertTrue(all(not node.exists for node in self.scene.created))


if __name__ == '__main__':
    unittest.main()
