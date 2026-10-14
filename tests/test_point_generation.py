#!/usr/bin/env python3
"""World-space point plans, C++ float3 parity and owned failure cleanup."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import geometry
from DPyTools.scene import attribute_values


class Port:
    def __init__(self, node, name):
        self.node, self.name = node, name
    def __getitem__(self, index):
        return Port(self.node, self.name + '[{}]'.format(index))


class Dag:
    def __init__(self, kind='transform', position=(20, 0, 0)):
        self.kind, self.position, self.instance = kind, position, False
    def isInstanced(self):
        return self.instance
    def attr(self, name):
        return Port(self, name)


class Transform(Dag):
    pass


class Component:
    def __init__(self, owner, position=(7, 8, 9)):
        self.owner, self.position = owner, position
    def node(self):
        return self.owner


class PointGenerationTests(unittest.TestCase):
    def setUp(self):
        self.source = Transform()
        self.selected = [self.source]
        self.created, self.cleaned = [], []
        test = self
        class Owner:
            def __init__(self):
                self.nodes = []
            def create(self, kind, **kwargs):
                node = Dag(kind)
                self.nodes.append(node)
                test.created.append(node)
                return node
            def cleanup(self, **kwargs):
                test.cleaned.extend(reversed(self.nodes))
        self.pm = NS(ls=Mock(side_effect=lambda **kw: self.selected), PyNode=lambda item: item,
                     general=NS(Component=Component), nt=NS(DagNode=Dag, Transform=Transform),
                     xform=Mock(side_effect=lambda item, **kw: item.position),
                     getAttr=Mock(side_effect=lambda port, **kw: 'matrix' if port.name.startswith('worldMatrix') else 'float3'),
                     setAttr=Mock(), connectAttr=Mock(), warning=Mock())
        self.scope = functions('python/DPyTools/features/point_generation.py', {
            'pm': self.pm, 'sys': sys, 'geometry': geometry,
            'attribute_values': attribute_values, 'ownership': NS(SceneNodes=Owner),
            'MAX_INPUT_POINTS': 10000})

    def create(self, live=False):
        return self.scope['create_from_selection'](live)

    def test_snapshot_uses_complete_world_xyz_and_float3(self):
        self.assertEqual(self.create().kind, 'DPAddPoint')
        self.pm.xform.assert_called_once_with(self.source, q=True, ws=True, t=True)
        self.assertEqual(self.pm.setAttr.call_args.args[1:], (20, 0, 0))
        self.assertEqual(self.pm.setAttr.call_args.kwargs, {'type': 'float3'})

    def test_live_transform_and_joint_use_world_matrix_not_local_translate(self):
        self.selected.append(Transform('joint'))
        self.assertEqual(self.create(True).kind, 'DPAddPoint')
        self.assertEqual([node.kind for node in self.created], ['DPAddPoint', 'decomposeMatrix', 'decomposeMatrix'])
        sources = [call.args[0].name for call in self.pm.connectAttr.call_args_list]
        self.assertEqual(sources, ['worldMatrix[0]', 'outputTranslate', 'worldMatrix[0]', 'outputTranslate'])
        self.assertTrue(all(not call.kwargs.get('force') for call in self.pm.connectAttr.call_args_list))

    def test_component_snapshot_is_supported_but_live_does_not_silently_fall_back(self):
        self.selected = [Component(Dag('mesh'))]
        self.assertTrue(self.create())
        self.created[:] = []
        self.assertFalse(self.create(True))
        self.assertFalse(self.created)

    def test_later_bad_xyz_float_overflow_or_instance_fails_before_creation(self):
        for position in ((1, 2), (float('nan'), 0, 0), (1e39, 0, 0)):
            self.selected = [self.source, Transform(position=position)]
            self.assertFalse(self.create())
        later = Transform()
        later.instance = True
        self.selected = [self.source, later]
        self.assertFalse(self.create())
        self.assertFalse(self.created)

    def test_empty_duplicate_oversized_and_nonboolean_options_are_refused(self):
        for selected in ([], [self.source, self.source], [self.source] * 10001):
            self.selected = selected
            self.assertFalse(self.create())
        self.assertFalse(self.create(1))
        self.assertFalse(self.created)

    def test_wrong_live_matrix_or_cpp_attribute_type_is_refused(self):
        self.pm.getAttr.return_value = 'double3'
        self.pm.getAttr.side_effect = None
        self.assertFalse(self.create(True))
        self.assertFalse(self.created)
        self.assertFalse(self.create(False))
        self.assertEqual(self.cleaned, list(reversed(self.created)))

    def test_later_position_queries_finish_before_first_write(self):
        self.selected.append(Transform(position=(10, 11, 12)))
        def write(*args, **kwargs):
            self.assertEqual(self.pm.xform.call_count, 2)
        self.pm.setAttr.side_effect = write
        self.assertTrue(self.create())

    def test_native_failure_cleans_known_new_nodes_not_sources(self):
        self.pm.connectAttr.side_effect = [None, RuntimeError('refused')]
        self.assertFalse(self.create(True))
        self.assertEqual(self.cleaned, list(reversed(self.created)))
        self.assertNotIn(self.source, self.cleaned)

    def test_interruption_is_preserved_while_known_candidate_is_cleaned(self):
        self.pm.setAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.create()
        self.assertEqual(self.cleaned, list(reversed(self.created)))

    def test_ui_delegates_status_and_reports_option_query_failure(self):
        feature = NS(create_from_selection=Mock(return_value=False))
        pm = NS(checkBox=Mock(return_value=True), warning=Mock())
        callback = _callback('python/DPyTools/dpoint/generate/DPointAddOp.py',
                             'openWin', 'runCreateCB', {'pm': pm, 'point_generation': feature})
        self.assertFalse(callback(NS(connectCheckBox='option')))
        feature.create_from_selection.assert_called_once_with(True)
        pm.checkBox.side_effect = RuntimeError('deleted control')
        self.assertFalse(callback(NS(connectCheckBox='option')))


if __name__ == '__main__':
    unittest.main()
