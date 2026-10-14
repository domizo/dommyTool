#!/usr/bin/env python3
"""Ordered edge directions and the actual live eyelash input/output graph."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_rivet_planning import Port
from DPyTools.core import components, geometry
from DPyTools.core.numeric import finite_number


class EdgeDirectionTests(unittest.TestCase):
    def test_single_and_forward_chain(self):
        self.assertEqual(components.ordered_edge_directions([(0, 1)]), (False,))
        self.assertEqual(components.ordered_edge_directions([(0, 1), (1, 2), (2, 3)]), (False, False, False))

    def test_reversed_first_and_subsequent_edges(self):
        self.assertEqual(components.ordered_edge_directions([(0, 1), (2, 0), (2, 3)]), (True, True, False))

    def test_final_closure_is_allowed(self):
        self.assertEqual(components.ordered_edge_directions([(0, 1), (1, 2), (2, 0)]), (False, False, False))

    def test_disconnected_duplicate_collapsed_and_early_closure_are_refused(self):
        for pairs in ([(0, 1), (2, 3)], [(0, 1), (1, 0)], [(0, 0)],
                      [(0, 1), (1, 2), (2, 0), (0, 3)], [(True, 1)], [(0, -1)]):
            with self.subTest(pairs=pairs), self.assertRaises((TypeError, ValueError)):
                components.ordered_edge_directions(pairs)

    def test_invalid_shapes_and_count_are_refused(self):
        for pairs in (None, [], ['edge'], [(0, 1, 2)], [(0, 1)] * 100001):
            with self.assertRaises(ValueError):
                components.ordered_edge_directions(pairs)


class Component:
    pass


class Vertex(Component):
    def __init__(self, index, position):
        self.value, self.position = index, position
    def index(self):
        return self.value


class Edge(Component):
    def __init__(self, mesh, index, vertices):
        self.owner, self.value, self.vertices = mesh, index, vertices
    def node(self):
        return self.owner
    def index(self):
        return self.value
    def connectedVertices(self):
        return list(self.vertices)


class Node:
    def __init__(self, kind):
        self.kind, self.length = kind, 3.0
        self.e = []
    def attr(self, name):
        return Port(self, name)
    def numEdges(self):
        return len(self.e)


class EyelashGraphTests(unittest.TestCase):
    def setUp(self):
        self.mesh = Node('mesh')
        self.vertices = [Vertex(index, (index, 0, 0)) for index in range(4)]
        self.mesh.e = [Edge(self.mesh, index, self.vertices[index:index + 2]) for index in range(3)]
        self.selected = list(self.mesh.e)
        self.created, self.cleaned = [], []
        test = self
        class Owner:
            def __init__(self):
                self.nodes = []
            def create(self, kind, **kwargs):
                node = Node(kind)
                self.nodes.append(node)
                test.created.append(node)
                return node
            def cleanup(self, **kwargs):
                test.cleaned.extend(reversed(self.nodes))
        self.owner = Owner
        self.pm = NS(ls=Mock(side_effect=lambda **kw: self.selected),
                     general=NS(Component=Component, MeshEdge=Edge),
                     selectPref=Mock(return_value=True),
                     xform=Mock(side_effect=lambda vertex, **kw: vertex.position),
                     arclen=lambda curve: curve.length,
                     setAttr=Mock(), connectAttr=Mock(), getAttr=Mock(return_value=None), warning=Mock())
        self.selection_scope = functions('python/DPyTools/scene/selection.py', {'pm': self.pm, 'components': components})
        def resolve(value, shape_types=('mesh',)):
            if value.kind not in shape_types:
                raise ValueError('wrong shape')
            return value
        self.selection_scope['resolve_shape'] = resolve
        self.edge_scope = functions('python/DPyTools/features/curves/edges.py', {
            'pm': self.pm, 'components': components, 'geometry': geometry, 'MAX_EDGE_CURVE_EDGES': 1000})
        self.scope = functions('python/DPyTools/features/rigging/eyelashes.py', {
            'pm': self.pm, 'sys': sys, 'finite_number': finite_number,
            'selection': NS(**self.selection_scope), 'edges': NS(**self.edge_scope),
            'ownership': NS(SceneNodes=Owner)})

    def create(self):
        return self.scope['createEyelash']()

    def test_edges_feed_a_real_curve_and_variable_group_evaluates_without_dummy_curve(self):
        generator, group = self.create()
        self.assertEqual((generator.kind, group.kind), ('DHEyelashGen', 'curveVarGroup'))
        destinations = [call.args[1].name for call in self.pm.connectAttr.call_args_list]
        self.assertIn('inCurve', destinations)
        self.assertIn('inMesh', destinations)
        self.assertEqual(len(self.created), 7)
        self.assertFalse(any(node.kind == 'nurbsCurve' for node in self.created))
        self.pm.getAttr.assert_called_once()
        self.assertEqual(self.pm.getAttr.call_args.kwargs, {'silent': True})
        self.assertTrue(all(not call.kwargs.get('force') for call in self.pm.connectAttr.call_args_list))

    def test_direct_curve_does_not_require_edge_tracking_and_is_borrowed(self):
        curve = Node('nurbsCurve')
        self.selected = [curve]
        self.assertTrue(self.create())
        self.pm.selectPref.assert_not_called()
        first = self.pm.connectAttr.call_args_list[0]
        self.assertEqual((first.args[0].name, first.args[1].name), ('worldSpace[0]', 'inCurve'))
        self.assertEqual(len(self.created), 2)
        self.assertNotIn(curve, self.created)

    def test_one_edge_is_supported_without_an_attach_node(self):
        self.selected = self.selected[:1]
        self.assertTrue(self.create())
        self.assertEqual([node.kind for node in self.created], ['DHEyelashGen', 'curveFromMeshEdge', 'curveVarGroup'])

    def test_later_disconnected_or_degenerate_edge_fails_before_creation(self):
        self.selected = [self.mesh.e[0], self.mesh.e[2]]
        self.assertFalse(self.create())
        self.selected = list(self.mesh.e)
        self.vertices[3].position = self.vertices[2].position
        self.assertFalse(self.create())
        self.assertFalse(self.created)

    def test_empty_wrong_shape_tracking_off_and_zero_curve_are_refused(self):
        self.selected = []
        self.assertFalse(self.create())
        self.selected = [Node('mesh')]
        self.assertFalse(self.create())
        self.selected = list(self.mesh.e)
        self.pm.selectPref.return_value = False
        self.assertFalse(self.create())
        curve = Node('nurbsCurve')
        curve.length = 0
        self.selected = [curve]
        self.assertFalse(self.create())
        self.assertFalse(self.created)

    def test_reversed_first_edge_is_explicit_in_attach_parameters(self):
        self.mesh.e[1].vertices = [self.vertices[0], self.vertices[2]]
        self.selected = self.mesh.e[:2]
        self.assertTrue(self.create())
        flags = [call.args[1] for call in self.pm.setAttr.call_args_list if call.args[0].name == 'reverse1']
        self.assertEqual(flags, [True])

    def test_malformed_construction_plan_cannot_create_prefix(self):
        for indices, directions in (([0, 1], [False]), ([0, 0], [False, False]), ([0], [1])):
            with self.assertRaises(ValueError):
                self.edge_scope['edge_curve_output'](self.owner(), self.mesh, indices, directions)
        self.assertFalse(self.created)

    def test_native_evaluation_failure_cleans_known_graph_not_borrowed_mesh(self):
        self.pm.getAttr.side_effect = RuntimeError('evaluation refused')
        self.assertFalse(self.create())
        self.assertEqual(self.cleaned, list(reversed(self.created)))
        self.assertNotIn(self.mesh, self.cleaned)

    def test_interruption_is_not_hidden_and_known_graph_is_cleaned(self):
        self.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.create()
        self.assertEqual(self.cleaned, list(reversed(self.created)))

    def test_legacy_eyelash_entry_point_delegates_status(self):
        feature = NS(createEyelash=Mock(return_value=False))
        scope = functions('python/DPyTools/create/HumanGenOp.py', {'eyelashes': feature})
        self.assertFalse(scope['createEyelash']())
        feature.createEyelash.assert_called_once_with()


if __name__ == '__main__':
    unittest.main()
