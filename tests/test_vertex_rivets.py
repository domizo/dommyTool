#!/usr/bin/env python3
"""Actual vertex-rivet plans, owned follicle factories and cleanup failures."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_small_generators import BuilderGraph
from test_python_rig_safety import functions
from DPyTools.core import geometry, strings
from DPyTools.core.numeric import finite_number


class VertexGraph(BuilderGraph):
    def __init__(self):
        super().__init__()
        self.mesh = self.create('mesh', name='borrowed_mesh')
        self.selected = [self.mesh]
        self.created[:] = []
        self.pm.createNode.reset_mock()
        self.follicles = NS(**functions('python/DPyTools/features/rigging/follicles.py', {
            'pm': self.pm, 'sys': sys, 'finite_number': finite_number, 'leaf_name': strings.leaf_name,
            'resolve_shape': self.selection.resolve_shape, 'groups': self.groups,
            'ownership': self.ownership, 'parameters': self.parameters}))
        self.nodes_api = NS(**functions('python/DPyTools/scene/nodes.py', {
            'pm': self.pm, 'sys': sys, 'parameters': self.parameters,
            'ownership': self.ownership, 'dattr': self.attributes}))
        self.operation = functions('python/DPyTools/features/rigging/rivets.py', {
            'pm': self.pm, 'sys': sys, 'geometry': geometry, 'strings': strings,
            'finite_number': finite_number, 'groups': self.groups, 'nodes': self.nodes_api,
            'follicles': self.follicles, 'ownership': self.ownership, 'parameters': self.parameters,
            'selection': self.selection, 'MAX_RIVETS': 10000})['createVertexRivets']
    def create(self, kind, **kwargs):
        node = super().create(kind, **kwargs)
        if kind in ('transform', 'joint'):
            node.add_port('inheritsTransform', 'bool', True)
        if kind == 'mesh':
            node.add_port('outMesh', 'mesh')
            node.add_port('worldMatrix', 'matrix')
            node.add_port('worldMatrix[0]', 'matrix')
            node.add_port('intermediateObject', 'bool', False)
            node.getAttr = lambda name: node.attr(name).value
        if kind == 'follicle':
            for name in ('inputMesh', 'inputSurface', 'inputWorldMatrix', 'outRotate', 'outTranslate', 'parameterU', 'parameterV'):
                node.add_port(name)
        for name, port in node.ports.items():
            port.set = lambda value, port=port: self.pm.setAttr(port, value)
            port.connect = lambda target, port=port: self.pm.connectAttr(port, target)
            if '.' not in name and '[' not in name and not hasattr(node, name):
                setattr(node, name, port)
        return node
    def run(self, positions=((1, 2, 3), (4, 5, 6)), uvs=((.1, .2), (.3, .4)), rotate=False):
        return self.operation(self.mesh, positions, uvs, rotate)


class VertexRivetTests(unittest.TestCase):
    def setUp(self): self.graph = VertexGraph()
    def test_success_returns_roots_explicit_follicles_and_joints_without_borrowed_reparenting(self):
        fol_group, joint_group, follicles, joints = self.graph.run()
        self.assertEqual((fol_group.name(), joint_group.name()), (':rivet_fol_grp', ':rivet_jnt_grp'))
        self.assertEqual(len(follicles), 2)
        self.assertEqual(len(joints), 2)
        self.assertIsNone(self.graph.mesh.parent)
        for follicle, joint in zip(follicles, joints):
            self.assertIs(follicle.parent.parent, fol_group)
            self.assertFalse(follicle.parent.inheritsTransform.value)
            self.assertIs(joint.parent.parent, joint_group)
            self.assertFalse(joint.parent.inheritsTransform.value)
            self.assertEqual(joint.parent.rotate.incoming, [])
            self.assertEqual(follicle.inputMesh.incoming, [self.graph.mesh.outMesh])
            self.assertEqual(follicle.inputWorldMatrix.incoming, [self.graph.mesh.attr('worldMatrix[0]')])
        self.assertEqual(self.graph.selected, [self.graph.mesh])
        self.assertTrue(all(call.kwargs['skipSelect'] for call in self.graph.pm.createNode.call_args_list))
    def test_rotation_link_uses_confirmed_transform_ports_and_tiled_uvs_remain_allowed(self):
        result = self.graph.run(uvs=((-2, 3), (.3, .4)), rotate=True)
        for follicle, joint in zip(result[2], result[3]):
            self.assertEqual(joint.parent.rotate.incoming, [follicle.parent.rotate])
        self.assertEqual((result[2][0].parameterU.value, result[2][0].parameterV.value), (-2.0, 3.0))
    def test_every_late_name_collision_precedes_any_generated_prefix(self):
        for name in ('rivet_jnt_grp', 'rivet_fol_2Shape', 'rivet_ofset_2', 'rivet_jnt_2'):
            self.setUp()
            borrowed = self.graph.create('transform', name=name)
            self.graph.created[:] = []
            self.graph.pm.createNode.reset_mock()
            with self.subTest(name=name): self.assertFalse(self.graph.run())
            self.graph.pm.createNode.assert_not_called()
            self.assertTrue(borrowed.exists)
    def test_bad_late_position_or_uv_prevents_all_creation(self):
        for positions, uvs in ((((1, 2, 3), (float('nan'), 0, 0)), ((0, 0), (0, 0))),
                               (((1, 2, 3), (4, 5, 6)), ((0, 0), (True, 0))),
                               (((1, 2, 3),), ((0,),))):
            self.assertFalse(self.graph.run(positions, uvs))
        self.graph.pm.createNode.assert_not_called()
    def test_empty_mismatched_nonsequence_overlimit_or_nonboolean_batches_fail(self):
        for positions, uvs, flag in (((), (), False), (((1, 2, 3),), (), False),
                                    ('123', ((0, 0),), False), ([(0, 0, 0)] * 10001, [(0, 0)] * 10001, False),
                                    (((1, 2, 3),), ((0, 0),), 1)):
            self.assertFalse(self.graph.run(positions, uvs, flag))
        self.graph.pm.createNode.assert_not_called()
    def test_instanced_or_wrong_geometry_fails_but_readonly_reference_source_is_allowed(self):
        self.graph.mesh.instanced = True
        self.assertFalse(self.graph.run())
        self.graph.mesh.instanced, self.graph.mesh.kind = False, 'nurbsCurve'
        self.assertFalse(self.graph.run())
        self.graph.pm.createNode.assert_not_called()
        self.graph.mesh.kind = 'mesh'
        self.graph.mesh.referenced = self.graph.mesh.locked = True
        self.assertTrue(self.graph.run())
    def test_second_group_failure_cleans_the_preceding_group(self):
        native = self.graph.pm.xform.side_effect
        def dispatch(node, **kwargs):
            if node.name() == ':rivet_jnt_grp': raise RuntimeError('move failed')
            return native(node, **kwargs)
        self.graph.pm.xform.side_effect = dispatch
        self.assertFalse(self.graph.run())
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.assertTrue(self.graph.mesh.exists)
    def test_second_follicle_configuration_failure_cleans_complete_confirmed_prefix(self):
        native = self.graph.pm.connectAttr.side_effect
        def dispatch(source, target, **kwargs):
            if target.owner.name() == ':rivet_fol_2Shape': raise RuntimeError('second follicle failed')
            return native(source, target, **kwargs)
        self.graph.pm.connectAttr.side_effect = dispatch
        self.assertFalse(self.graph.run())
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.assertTrue(self.graph.mesh.exists)
        self.assertIn('after 1 completed rivets', self.graph.pm.warning.call_args.args[0])
    def test_joint_position_failure_cleans_follicles_offsets_and_joint(self):
        native = self.graph.pm.xform.side_effect
        self.graph.pm.xform.side_effect = lambda node, **kw: (_ for _ in ()).throw(RuntimeError('joint move failed')) if node.kind == 'joint' else native(node, **kw)
        self.assertFalse(self.graph.run())
        self.assertTrue(all(not node.exists for node in self.graph.created))
    def test_unconfirmed_follicle_result_cannot_be_reported_as_success(self):
        self.graph.follicles.create_follicle = Mock(return_value=False)
        self.assertFalse(self.graph.run())
        self.assertTrue(all(not node.exists for node in self.graph.created))
    def test_unknown_native_child_blocks_parent_cleanup_instead_of_deleting_borrowed_node(self):
        unknown = []
        def dispatch(source, target, **kwargs):
            group = next(node for node in self.graph.created if node.name() == ':rivet_jnt_grp')
            unknown.append(self.graph.create('mesh', parent=group))
            raise RuntimeError('unknown side effect')
        self.graph.pm.connectAttr.side_effect = dispatch
        self.assertFalse(self.graph.run())
        self.assertTrue(unknown[0].exists and unknown[0].parent.exists)
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)
    def test_interruption_cleans_known_graph_and_restores_selection(self):
        self.graph.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.run()
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.assertEqual(self.graph.selected, [self.graph.mesh])
    def test_owner_factory_requires_explicit_owner_for_names_and_checks_owner_type(self):
        with self.assertRaises(ValueError): self.graph.follicles.create_follicle(self.graph.mesh, name='named')
        with self.assertRaises(TypeError): self.graph.follicles.create_follicle(self.graph.mesh, owner=object(), name='named')
        self.graph.pm.createNode.assert_not_called()


if __name__ == '__main__': unittest.main()
