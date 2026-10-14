#!/usr/bin/env python3
"""Pure Node Info snapshots, literal filtering and safe table coordinates."""

import unittest
import sys
from pathlib import Path
from types import SimpleNamespace as NS

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.ui import node_table, table_selection


def attribute(name='value', source=None, destinations=None):
    return NS(m_name=name, m_type='double', m_size=1, m_array_size=0,
              m_src_node=source, m_dest_nodes=destinations, m_val=3.0)


class NodeTableTests(unittest.TestCase):
    def test_snapshot_preserves_seven_column_text_and_unicode(self):
        row = node_table.snapshot([attribute('属性', '名前空間:入力.out', ['出力.attr'])])[0]
        self.assertEqual(row.cells, ('属性', 'double', '1', '', '名前空間:入力.out', '出力.attr', '3.0'))
        self.assertEqual(table_selection.display_cell((row.cells,), 1, 1), '属性')

    def test_display_and_connections_do_not_change_after_public_data_mutation(self):
        data = attribute('value', 'source.out', ['target.a'])
        row = node_table.snapshot([data])[0]
        data.m_name, data.m_src_node = 'changed', 'changed.out'
        data.m_dest_nodes.append('another.b')
        self.assertEqual(row.cells[0], 'value')
        self.assertEqual(node_table.connection_targets(row, 5), ('source',))
        self.assertEqual(node_table.connection_targets(row, 6), ('target',))

    def test_zero_match_and_connected_filters_are_explicit_empty_results(self):
        rows = node_table.snapshot([attribute('abc')])
        self.assertEqual(node_table.filter_rows(rows, 'missing'), ())
        self.assertEqual(node_table.filter_rows(rows, connected_only=True), ())
        self.assertEqual(node_table.filter_rows(rows, 'ABC'), rows)

    def test_row_and_destination_budgets_reject_overflow(self):
        data = attribute()
        self.assertEqual(len(node_table.snapshot([data] * table_selection.MAX_TEXT_ITEMS)), 10000)
        with self.assertRaises(ValueError):
            node_table.snapshot([data] * 10001)
        with self.assertRaises(ValueError):
            node_table.snapshot([attribute(destinations=['target.a'] * 10001)])

    def test_invalid_models_and_connection_containers_fail_before_returning_a_prefix(self):
        for data in (None, 'data', iter([attribute()])):
            with self.assertRaises(ValueError):
                node_table.snapshot(data)
        for value in ('target.a', [True], ['target.a', None]):
            with self.assertRaises((TypeError, ValueError)):
                node_table.snapshot([attribute(), attribute(destinations=value)])

    def test_filter_admission_rejects_nontext_and_nonboolean_options(self):
        rows = node_table.snapshot([attribute()])
        for value in (None, True, 1):
            with self.assertRaises(TypeError):
                node_table.filter_rows(rows, value)
        for value in (None, 0, 1, 'true'):
            with self.assertRaises(TypeError):
                node_table.filter_rows(rows, connected_only=value)

    def test_cell_coordinates_reuse_one_based_admission(self):
        for value in (None, [], (), [0, 5], [1, 0]):
            self.assertEqual(node_table.selected_cell(value, 2), ())
        self.assertEqual(node_table.selected_cell([2, 6], 2), (2, 6))
        for value in ([True, 5], [1, True], [-1, 5], [3, 5], [1, 8], [1], [1, 5, 2, 6]):
            with self.assertRaises(ValueError):
                node_table.selected_cell(value, 2)

    def test_connection_targets_preserve_full_paths_namespaces_and_first_order(self):
        row = node_table.snapshot([attribute(destinations=['|root|ns:node.a', 'other.b', '|root|ns:node.c'])])[0]
        self.assertEqual(node_table.connection_targets(row, 6), ('|root|ns:node', 'other'))
        self.assertEqual(node_table.connection_targets(row, 1), ())

    def test_malformed_or_wildcard_connection_refuses_the_whole_target_batch(self):
        for value in ('node', '.attr', 'node.', '*.attr', 'node?.attr', 'node[0].attr', 'bad\nnode.attr'):
            row = node_table.snapshot([attribute(destinations=['valid.attr', value])])[0]
            with self.subTest(value=value), self.assertRaises(ValueError):
                node_table.connection_targets(row, 6)

    def test_array_count_zero_empty_connections_and_none_values_keep_display_contract(self):
        data = attribute()
        data.m_val, data.m_array_size = None, 3
        row = node_table.snapshot([data])[0]
        self.assertEqual(row.cells[3:], ('3', '', '', 'None'))
        self.assertEqual(node_table.connection_targets(row, 5), ())
        self.assertEqual(node_table.connection_targets(row, 6), ())


if __name__ == '__main__':
    unittest.main()
