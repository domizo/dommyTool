#!/usr/bin/env python3
"""Read-only tracked edge admission and its actual historical UI consumers."""

import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import components


class ComponentIndexTests(unittest.TestCase):
    def test_native_integer_domain_boundaries(self):
        for value in (0, 1, components.MAX_COMPONENT_INDEX):
            self.assertEqual(components.component_index(value), value)

    def test_booleans_floats_strings_and_out_of_range_values_fail(self):
        for value in (True, False, 1.0, '1', None, -1,
                      components.MAX_COMPONENT_INDEX + 1, 10 ** 400):
            with self.assertRaises((TypeError, ValueError)):
                components.component_index(value)


class Edge:
    def __init__(self, mesh, index):
        self.mesh, self.value = mesh, index
    def node(self):
        return self.mesh
    def index(self):
        return self.value


class EdgeSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = functions('python/DPyTools/scene/selection.py',
                              {'components': components})

    def setUp(self):
        self.mesh = SimpleNamespace(numEdges=lambda: 10)
        self.selection = [Edge(self.mesh, 7), Edge(self.mesh, 2)]
        self.pref = Mock(return_value=True)
        self.pm = SimpleNamespace(ls=Mock(side_effect=lambda **kw: self.selection),
                                  selectPref=self.pref, general=SimpleNamespace(MeshEdge=Edge))
        self.scope.update(pm=self.pm, resolve_shape=lambda value: value)

    def query(self, target=None):
        return self.scope['getSelectedEdgeIdsList'](target)

    def test_selection_order_is_preserved_and_preferences_are_only_queried(self):
        self.assertEqual(self.query(), [7, 2])
        self.pm.ls.assert_called_once_with(orderedSelection=True, flatten=True)
        self.pref.assert_called_once_with(q=True, trackSelectionOrder=True)

    def test_tracking_off_refuses_input_without_turning_it_on(self):
        self.pref.return_value = False
        with self.assertRaisesRegex(ValueError, 'reselect'):
            self.query()
        self.pref.assert_called_once_with(q=True, trackSelectionOrder=True)

    def test_empty_selection_stays_a_noop_even_with_tracking_off(self):
        self.selection = []
        self.pref.return_value = False
        self.assertEqual(self.query(), [])
        self.pref.assert_not_called()

    def test_later_nonedge_duplicate_or_stale_id_cannot_return_prefix(self):
        for value in (SimpleNamespace(index=lambda: 3), Edge(self.mesh, 7),
                      Edge(self.mesh, -1), Edge(self.mesh, 10),
                      Edge(self.mesh, True), Edge(self.mesh, 1.5)):
            self.selection = [Edge(self.mesh, 7), value]
            with self.assertRaises((TypeError, ValueError)):
                self.query()
        self.assertTrue(all(call.kwargs.get('q') for call in self.pref.call_args_list))

    def test_mixed_meshes_and_wrong_target_are_rejected(self):
        other = SimpleNamespace(numEdges=lambda: 10)
        self.selection.append(Edge(other, 3))
        with self.assertRaises(ValueError):
            self.query()
        self.selection = self.selection[:1]
        with self.assertRaises(ValueError):
            self.query(other)

    def test_size_limit_is_admitted_before_per_edge_queries(self):
        self.selection = [Edge(self.mesh, 1)] * (components.MAX_COMPONENT_COUNT + 1)
        with self.assertRaises(ValueError):
            self.query()

    def test_query_failure_cannot_change_tracking_state(self):
        self.pref.side_effect = RuntimeError('preference query failed')
        with self.assertRaises(RuntimeError):
            self.query()
        self.assertTrue(all(call.kwargs.get('q') for call in self.pref.call_args_list))


class EdgeConsumerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = {}
        cls.eyelash = staticmethod(functions(
            'python/DPyTools/features/rigging/eyelashes.py', cls.scope)['createEyelash'])
        path = 'python/DPyTools/rigging/DFaceRigOp.py'
        cls.top = staticmethod(_callback(path, 'DFaceRiggingTool', 'setTopStickyCB', cls.scope))
        cls.bottom = staticmethod(_callback(path, 'DFaceRiggingTool', 'setBotStickyCB', cls.scope))

    def setUp(self):
        self.edges = Mock(side_effect=ValueError('Enable tracking then reselect'))
        self.pm = SimpleNamespace(warning=Mock(), createNode=Mock(), textField=Mock(),
                                  ls=Mock(return_value=[]), general=SimpleNamespace(Component=Edge))
        self.scope.update(pm=self.pm, selOp=SimpleNamespace(getSelectedEdgeIdsList=self.edges),
                          selection=SimpleNamespace(ordered_edge_plan=self.edges),
                          edges=SimpleNamespace(MAX_EDGE_CURVE_EDGES=1000,
                                                plan_edge_curve=Mock(side_effect=ValueError('empty chain'))),
                          ownership=SimpleNamespace(SceneNodes=lambda: SimpleNamespace(cleanup=Mock())),
                          sys=__import__('sys'))
        self.ui = SimpleNamespace(topStickyText='top', botStickyText='bottom',
                                  m_top_sticky_edge_list=[3], m_bot_sticky_edge_list=[4])

    def test_bad_edge_selection_cannot_create_eyelash_nodes(self):
        self.assertFalse(self.eyelash())
        self.pm.createNode.assert_not_called()
        self.pm.warning.assert_called_once()

    def test_empty_selection_cannot_create_eyelash_nodes(self):
        self.edges.side_effect = None
        self.edges.return_value = []
        self.assertFalse(self.eyelash())
        self.pm.createNode.assert_not_called()

    def test_bad_selection_preserves_both_sticky_edge_fields_and_models(self):
        self.assertFalse(self.top(self.ui))
        self.assertFalse(self.bottom(self.ui))
        self.assertEqual(self.ui.m_top_sticky_edge_list, [3])
        self.assertEqual(self.ui.m_bot_sticky_edge_list, [4])
        self.pm.textField.assert_not_called()

    def test_valid_or_empty_selection_keeps_historical_sticky_field_behavior(self):
        self.edges.side_effect = None
        self.edges.return_value = [7, 2]
        self.top(self.ui)
        self.assertEqual(self.ui.m_top_sticky_edge_list, [7, 2])
        self.pm.textField.assert_called_with('top', e=True, tx='7 2')
        self.edges.return_value = []
        self.bottom(self.ui)
        self.assertEqual(self.ui.m_bot_sticky_edge_list, [])
        self.pm.textField.assert_called_with('bottom', e=True, tx='')


if __name__ == '__main__':
    unittest.main()
