#!/usr/bin/env python3
"""Actual complete component admission and bounded rivet graph construction."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_edge_selection import Edge
from DPyTools.core import components


class Face:
    def __init__(self, mesh, index):
        self.owner, self.value = mesh, index
    def node(self):
        return self.owner
    def index(self):
        return self.value


class Port:
    def __init__(self, node, name):
        self.node, self.name = node, name
    def __getitem__(self, index):
        return Port(self.node, self.name + '[{}]'.format(index))


class Node:
    def __init__(self, kind):
        self.kind = kind
    def attr(self, name):
        return Port(self, name)
    def numEdges(self):
        return 10
    def numFaces(self):
        return 6


class RivetTests(unittest.TestCase):
    def setUp(self):
        self.mesh = Node('mesh')
        self.selected = [Edge(self.mesh, 3), Edge(self.mesh, 5)]
        self.created, self.cleaned = [], []
        test = self
        class Owner:
            def __init__(self):
                self.nodes = []
            def create(self, kind, **kwargs):
                if kind == 'locator' and 'parent' not in kwargs:
                    raise AssertionError('Locator parent must be explicitly owned')
                node = Node(kind)
                self.nodes.append(node)
                test.created.append(node)
                return node
            def cleanup(self, **kwargs):
                test.cleaned.extend(reversed(self.nodes))
        self.pm = NS(ls=lambda **kw: self.selected,
                     general=NS(MeshEdge=Edge, MeshFace=Face),
                     setAttr=Mock(), connectAttr=Mock(), warning=Mock())
        self.selection_scope = functions('python/DPyTools/scene/selection.py', {
            'pm': self.pm, 'components': components, 'resolve_shape': lambda mesh: mesh})
        self.selection_scope['resolve_shape'] = lambda mesh: mesh
        self.scope = functions('python/DPyTools/features/rigging/rivets.py', {
            'pm': self.pm, 'sys': sys, 'selection': NS(**self.selection_scope),
            'ownership': NS(SceneNodes=Owner), 'MAX_RIVETS': 10000})

    def test_all_selected_edges_are_created_with_correct_ports_and_owned_locators(self):
        result = self.scope['createEdgeRivet']()
        self.assertEqual(len(result), 2)
        self.assertEqual([call.args[1] for call in self.pm.setAttr.call_args_list], [3, 5])
        self.assertTrue(all(call.args[0].name == 'EdgeNumber' for call in self.pm.setAttr.call_args_list))
        self.assertEqual(len(self.created), 6)
        self.assertTrue(all(not call.kwargs.get('force') for call in self.pm.connectAttr.call_args_list))
        self.assertEqual(self.pm.connectAttr.call_args_list[0].args[0].name, 'worldMesh[0]')

    def test_face_components_use_face_count_and_face_node_contract(self):
        self.selected = [Face(self.mesh, 2), Face(self.mesh, 4)]
        self.assertEqual(len(self.scope['createFaceRivet']()), 2)
        self.assertEqual(self.created[0].kind, 'DPolyFaceToMat')
        self.assertTrue(all(call.args[0].name == 'FaceNumber' for call in self.pm.setAttr.call_args_list))

    def test_later_wrong_type_mixed_owner_duplicate_or_stale_id_cannot_create_prefix(self):
        for bad in (object(), Edge(Node('mesh'), 4), Edge(self.mesh, 3),
                    Edge(self.mesh, 10), Edge(self.mesh, True)):
            self.selected = [Edge(self.mesh, 3), bad]
            self.assertFalse(self.scope['createEdgeRivet']())
        self.assertFalse(self.created)

    def test_empty_and_oversized_selections_stop_before_creation(self):
        self.selected = []
        self.assertFalse(self.scope['createEdgeRivet']())
        self.selected = [Edge(self.mesh, 3)] * 10001
        self.assertFalse(self.scope['createEdgeRivet']())
        self.assertFalse(self.created)

    def test_component_limits_kind_and_explicit_target_are_checked(self):
        plan = self.selection_scope['mesh_component_plan']
        for limit in (0, True, 100001):
            with self.assertRaises((TypeError, ValueError)):
                plan(self.selected, limit=limit)
        with self.assertRaises(ValueError):
            plan(self.selected, kind='vertex')
        with self.assertRaises(ValueError):
            plan(self.selected, target_mesh=Node('otherMesh'))

    def test_native_failure_cleans_only_known_outputs(self):
        self.pm.connectAttr.side_effect = [None, None, None, RuntimeError('later failure')]
        self.assertFalse(self.scope['createEdgeRivet']())
        self.assertEqual(self.cleaned, list(reversed(self.created)))
        self.assertNotIn(self.mesh, self.cleaned)

    def test_interruption_is_preserved_with_known_cleanup(self):
        self.pm.setAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.scope['createEdgeRivet']()
        self.assertEqual(self.cleaned, list(reversed(self.created)))

    def test_historical_facade_propagates_both_entry_point_results(self):
        feature = NS(createEdgeRivet=Mock(return_value=False), createFaceRivet=Mock(return_value=('result',)))
        scope = functions('python/DPyTools/rigging/DConstrainOp.py', {'rivets': feature})
        self.assertFalse(scope['createEdgeRivet']())
        self.assertEqual(scope['createFaceRivet'](), ('result',))


if __name__ == '__main__':
    unittest.main()
