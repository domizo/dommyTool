#!/usr/bin/env python3
"""Actual Node Info View preparation, filtering and selection with UI doubles."""

import unittest
from contextlib import redirect_stdout
from io import StringIO
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_scene_node_ownership import definitions
from test_python_rig_safety import functions
from DPyTools.core import components
from DPyTools.ui import node_table, table_selection


class NodeInfoFixture(unittest.TestCase):
    def setUp(self):
        self.filter_text, self.connected, self.cells = '', False, None
        self.row_count, self.view, self.reenter = 0, None, False
        self.fail_rows = None
        self.pending_cell = None
        self.attributes = ['first', 'second']
        self.indices = {}
        self.values = {'first': 1.0, 'second': 2.0}
        self.sources = {'second': 'incoming.output'}
        self.destinations = {'second': ['target.attrA', 'target.attrB']}
        self.selection = ['node']

        def field(control=None, **kwargs):
            if control is None:
                return 'filter'
            if kwargs.get('q') or kwargs.get('query'):
                return self.filter_text
            self.filter_text = kwargs['text']
            if self.reenter and self.view:
                self.view.changeSortText()

        def checkbox(control=None, **kwargs):
            if control is None:
                return 'connected'
            if kwargs.get('q') or kwargs.get('query'):
                return self.connected
            if 'value' in kwargs:
                self.connected = kwargs['value']
                if self.reenter and self.view:
                    self.view.changeSortText()

        def table(control=None, **kwargs):
            if control is None:
                return 'table'
            if kwargs.get('q') or kwargs.get('query'):
                return self.cells
            if kwargs.get('clearTable'):
                self.row_count = 0
            if 'rows' in kwargs:
                if self.fail_rows is not None:
                    raise self.fail_rows
                self.row_count = kwargs['rows']
                if self.view:
                    self.pending_cell = self.view.getCellVal(1, 1)
            if self.reenter and self.view:
                self.view.changeCellSelect()

        def get_attr(fullname, **kwargs):
            attr = fullname.partition('.')[2]
            if kwargs.get('multiIndices'):
                result = self.indices.get(attr)
                if isinstance(result, BaseException):
                    raise result
                return result
            if kwargs.get('type'):
                return 'double'
            if kwargs.get('size'):
                return 1
            return self.values.get(attr, 0.0)

        def connection(fullname, **kwargs):
            attr = fullname.partition('.')[2]
            if kwargs.get('isDestination'):
                return bool(self.sources.get(attr))
            if kwargs.get('isSource'):
                return bool(self.destinations.get(attr))
            if kwargs.get('sourceFromDestination'):
                return self.sources.get(attr)
            return self.destinations.get(attr)

        self.pm = NS(window=Mock(return_value='window'), formLayout=Mock(return_value='form'),
            button=Mock(return_value='button'), textField=Mock(side_effect=field),
            checkBox=Mock(side_effect=checkbox), scriptTable=Mock(side_effect=table),
            showWindow=Mock(), warning=Mock(), select=Mock(),
            ls=Mock(side_effect=lambda **kwargs: self.selection),
            listAttr=Mock(side_effect=lambda node: self.attributes),
            getAttr=Mock(side_effect=get_attr), connectionInfo=Mock(side_effect=connection),
            PyNode=Mock(side_effect=lambda name: 'resolved:' + name))
        params = functions('python/DPyTools/scene/parameters.py',
                           {'pm': self.pm, 'MAX_CONNECTIONS': 10000})
        self.resolve_nodes = Mock(wraps=params['resolve_nodes'])
        self.scope = definitions('python/DPyTools/node/DNodeOp.py',
            {'pm': self.pm, 'cmds': self.pm, 'components': components,
             'table_selection': table_selection, 'node_table': node_table,
             'parameters': NS(resolve_nodes=self.resolve_nodes),
             'ui_windows': NS(delete_window=Mock())})
        self.view = self.scope['nodeInfoView']()

    def refresh(self):
        with redirect_stdout(StringIO()):
            return self.view.updateList()


class NodeInfoViewTests(NodeInfoFixture):
    def test_zero_match_filter_remains_empty_in_view_and_active_model(self):
        self.refresh()
        self.filter_text = 'missing'
        self.view.changeSortText()
        self.assertEqual(self.row_count, 0)
        self.assertEqual(self.view.getActiveDataArray(), [])
        self.assertEqual(self.view.getCellVal(1, 1), '')

    def test_connected_filter_with_zero_results_does_not_fall_back_to_all(self):
        self.sources, self.destinations = {}, {}
        self.refresh()
        self.connected = True
        self.view.changeSortText()
        self.assertEqual(self.row_count, 0)

    def test_filter_is_case_insensitive_and_keeps_original_order(self):
        self.refresh()
        self.filter_text, self.connected = 'SECOND', True
        self.view.changeSortText()
        self.assertEqual(self.row_count, 1)
        self.assertEqual(self.view.getCellVal(1, 1), 'second')

    def test_invalid_display_coordinates_return_blank_without_negative_indexing(self):
        self.refresh()
        for row in (None, 0, -1, 3, True, False, 1.5, '1', []):
            with self.subTest(row=row):
                self.assertEqual(self.view.getCellVal(row, 1), '')
        for column in (None, 0, -1, 8, True, 1.5, '1', []):
            with self.subTest(column=column):
                self.assertEqual(self.view.getCellVal(1, column), '')

    def test_empty_malformed_or_out_of_range_selection_cannot_change_scene_selection(self):
        self.refresh()
        for cells in (None, [], [0, 5], [-1, 5], [3, 5], [True, 5], [1, 0],
                      [1, 8], [1], [1, 5, 2, 6], '15'):
            self.cells = cells
            with self.subTest(cells=cells):
                self.assertFalse(self.view.changeCellSelect())
        self.pm.select.assert_not_called()

    def test_source_selection_resolves_then_replaces_once_without_clearing_first(self):
        self.refresh()
        self.cells = [2, 5]
        self.assertTrue(self.view.changeCellSelect())
        self.resolve_nodes.assert_called_once_with(('incoming',), require_same_type=False)
        self.pm.select.assert_called_once_with(['resolved:incoming'], replace=True)

    def test_destination_ports_on_one_node_are_deduplicated_before_selection(self):
        self.refresh()
        self.cells = [2, 6]
        self.assertTrue(self.view.changeCellSelect())
        self.pm.select.assert_called_once_with(['resolved:target'], replace=True)

    def test_later_stale_target_prevents_any_selection_mutation(self):
        self.destinations['second'] = ['target.attr', 'missing.attr']
        self.refresh()
        self.pm.PyNode.side_effect = lambda name: (_ for _ in ()).throw(ValueError('stale')) if name == 'missing' else name
        self.cells = [2, 6]
        self.assertFalse(self.view.changeCellSelect())
        self.pm.select.assert_not_called()

    def test_connection_selection_uses_the_rendered_snapshot_after_external_mutation(self):
        self.refresh()
        self.view.m_attr_data_array[1].m_src_node = 'changed.output'
        self.view.m_attr_data_array[1].m_name = 'changed'
        self.cells = [2, 5]
        self.assertEqual(self.view.getCellVal(2, 1), 'second')
        self.assertTrue(self.view.changeCellSelect())
        self.pm.select.assert_called_once_with(['resolved:incoming'], replace=True)

    def test_failed_attribute_fetch_preserves_model_controls_and_node_then_allows_retry(self):
        self.refresh()
        self.filter_text = 'first'
        self.view.changeSortText()
        old_data = list(self.view.m_attr_data_array)
        self.selection = ['next_node']
        self.pm.listAttr.side_effect = RuntimeError('query failed')
        self.assertFalse(self.refresh())
        self.assertEqual(self.view.selected_node, 'node')
        self.assertEqual(self.view.m_attr_data_array, old_data)
        self.assertEqual(self.filter_text, 'first')
        self.pm.listAttr.side_effect = lambda node: self.attributes
        self.assertTrue(self.refresh())
        self.assertEqual(self.view.selected_node, 'next_node')

    def test_explicit_update_of_same_node_reads_current_values_again(self):
        self.assertTrue(self.refresh())
        self.values['first'] = 42
        self.assertTrue(self.refresh())
        self.assertEqual(self.pm.listAttr.call_count, 2)
        self.assertEqual(self.view.getCellVal(1, 7), '42')

    def test_late_multi_query_failure_preserves_previous_complete_snapshot(self):
        self.refresh()
        old_data = list(self.view.m_attr_data_array)
        self.indices['second'] = RuntimeError('multi query failed')
        self.assertFalse(self.refresh())
        self.assertEqual(self.view.m_attr_data_array, old_data)

    def test_sparse_multi_indices_keep_query_order_and_none_is_empty(self):
        self.indices['first'] = [9, 2]
        self.assertTrue(self.refresh())
        self.assertEqual([data.m_name for data in self.view.getActiveDataArray()],
                         ['first', 'first[9]', 'first[2]', 'second'])

    def test_bad_sparse_indices_and_over_budget_rows_cannot_replace_old_snapshot(self):
        self.refresh()
        old_data = list(self.view.m_attr_data_array)
        for indices in ([True], [-1], [2147483648], [1, 1]):
            self.indices['second'] = indices
            with self.subTest(indices=indices):
                self.assertFalse(self.refresh())
                self.assertEqual(self.view.m_attr_data_array, old_data)
        self.indices = {}
        self.attributes = ['attr{0}'.format(index) for index in range(10001)]
        self.assertFalse(self.refresh())
        self.assertEqual(self.view.m_attr_data_array, old_data)

    def test_none_selection_and_attribute_list_have_explicit_empty_outcomes(self):
        self.selection = None
        self.assertFalse(self.refresh())
        self.selection, self.attributes = ['node'], None
        self.assertTrue(self.refresh())
        self.assertEqual(self.row_count, 0)

    def test_partial_native_view_failure_disables_selection_and_retry_repairs_view(self):
        self.refresh()
        self.fail_rows = RuntimeError('table write failed')
        self.assertFalse(self.refresh())
        self.cells = [2, 5]
        self.assertFalse(self.view.changeCellSelect())
        self.assertEqual(self.view.getCellVal(1, 1), '')
        self.pm.select.assert_not_called()
        self.fail_rows = None
        self.assertTrue(self.refresh())
        self.assertEqual(self.view.getCellVal(1, 1), 'first')

    def test_native_update_reentry_is_suppressed_and_cells_use_prepared_rows(self):
        self.reenter, self.cells = True, [2, 5]
        self.assertTrue(self.refresh())
        self.assertEqual(self.pending_cell, 'first')
        self.pm.select.assert_not_called()
        self.assertEqual(self.row_count, 2)

    def test_query_and_commit_interruptions_propagate_and_reset_guard(self):
        for phase in ('query', 'commit'):
            self.fail_rows = None
            self.pm.listAttr.side_effect = lambda node: self.attributes
            error = KeyboardInterrupt()
            if phase == 'query':
                self.pm.listAttr.side_effect = error
            else:
                self.fail_rows = error
            with self.subTest(phase=phase), self.assertRaises(KeyboardInterrupt) as caught:
                self.refresh()
            self.assertIs(caught.exception, error)
            self.assertFalse(self.view._updating_view)

    def test_filtered_row_selection_matches_the_visible_snapshot_not_the_original_index(self):
        self.refresh()
        self.filter_text = 'second'
        self.view.changeSortText()
        self.cells = [1, 5]
        self.assertTrue(self.view.changeCellSelect())
        self.pm.select.assert_called_once_with(['resolved:incoming'], replace=True)

    def test_legacy_sparse_row_entry_point_keeps_model_and_filter_in_sync(self):
        self.refresh()
        self.filter_text = 'first'
        self.view.changeSortText()
        self.indices['first'] = [7]
        self.assertTrue(self.view.checkMultiIndexAttr('first'))
        self.assertEqual([data.m_name for data in self.view.getActiveDataArray()], ['first', 'first[7]'])
        self.assertEqual(self.row_count, 2)
        self.assertEqual(len(self.view.m_attr_data_array), 3)

    def test_empty_or_invalid_legacy_sparse_query_is_not_a_partial_update(self):
        self.refresh()
        old_data = list(self.view.m_attr_data_array)
        self.assertFalse(self.view.checkMultiIndexAttr('first'))
        self.indices['first'] = [2, 2]
        self.assertFalse(self.view.checkMultiIndexAttr('first'))
        self.assertEqual(self.view.m_attr_data_array, old_data)

    def test_selection_interruption_propagates_and_restores_reentry_guard(self):
        self.refresh()
        self.cells = [2, 5]
        self.pm.select.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.view.changeCellSelect()
        self.assertFalse(self.view._updating_view)


if __name__ == '__main__':
    unittest.main()
