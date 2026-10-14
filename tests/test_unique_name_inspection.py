#!/usr/bin/env python3
"""Actual read-only duplicate inspection with preserved legacy rename source."""

import ast
import contextlib
import io
import textwrap
import unittest
from collections import Counter
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_asset_browser import methods
from test_python_rig_safety import functions
from test_python_table_selection import _tree


class Node:
    def __init__(self, label):
        self.label = label
        self.string_calls = 0

    def __str__(self):
        self.string_calls += 1
        return self.label


class UniqueNameInspectionTests(unittest.TestCase):
    def setUp(self):
        self.pm = NS(ls=Mock(return_value=[]), rename=Mock())
        self.counter = Mock(side_effect=Counter)
        self.scope = functions('python/DPyTools/general/nameActions.py',
                               {'pm': self.pm, '_Counter': self.counter})
        self.inspect = self.scope['checkUniqueName']

    def assert_read_only(self):
        self.pm.ls.assert_called_with(shapes=0)
        self.pm.rename.assert_not_called()

    def test_empty_native_list_is_an_empty_inspection(self):
        self.assertEqual(self.inspect(), [])
        self.counter.assert_called_once_with([])
        self.assert_read_only()

    def test_none_false_and_legacy_falsey_modes_keep_read_only_results(self):
        self.pm.ls.return_value = ['|left|same', '|right|same', '|root|unique']
        for mode in (None, False, 0):
            self.assertEqual(self.inspect(isRename=mode), ['same', 'same'])
        self.assert_read_only()

    def test_duplicate_results_keep_multiplicity_and_original_node_order(self):
        self.pm.ls.return_value = ['|left|a', '|right|b', '|left|unique', '|right|a',
                                   '|left|b', '|third|a']
        self.assertEqual(self.inspect(), ['a', 'b', 'a', 'b', 'a'])
        self.assert_read_only()

    def test_complete_namespace_case_and_unicode_names_keep_legacy_comparison(self):
        self.pm.ls.return_value = ['|one|charA:手', '|two|charB:手', '|three|charA:手',
                                   '|one|Node', '|two|node', '|three|Node']
        self.assertEqual(self.inspect(), ['charA:手', 'charA:手', 'Node', 'Node'])
        self.assert_read_only()

    def test_each_native_node_is_stringified_only_once(self):
        nodes = [Node('|left|same'), Node('|right|same'), Node('|root|unique')]
        self.pm.ls.return_value = nodes
        self.assertEqual(self.inspect(), ['same', 'same'])
        self.assertEqual([node.string_calls for node in nodes], [1, 1, 1])
        self.counter.assert_called_once_with(['same', 'same', 'unique'])
        self.assert_read_only()

    def test_large_unique_inventory_is_counted_once_not_scanned_per_node(self):
        self.pm.ls.return_value = ['|root|node{0}'.format(index) for index in range(10000)]
        self.assertEqual(self.inspect(), [])
        self.counter.assert_called_once()
        self.assertEqual(len(self.counter.call_args[0][0]), 10000)
        self.assert_read_only()
        # Structural work assertions are deterministic, unlike timing thresholds.
        tree = _tree('python/DPyTools/general/nameActions.py')
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                        and node.name == 'checkUniqueName')
        read_branch = next(node for node in function.body if isinstance(node, ast.If))
        self.assertFalse(any(isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                             and node.func.attr in ('count', 'index', 'rename')
                             for node in ast.walk(read_branch)))

    def test_borrowed_native_query_list_and_nodes_are_not_modified(self):
        nodes = [Node('|left|same'), Node('|right|same')]
        original = tuple(nodes)
        self.pm.ls.return_value = nodes
        self.inspect()
        self.assertEqual(tuple(nodes), original)
        self.assertEqual([node.label for node in nodes], ['|left|same', '|right|same'])
        self.assert_read_only()

    def test_query_failure_and_interruption_propagate_without_rename(self):
        for error in (RuntimeError('query failed'), KeyboardInterrupt()):
            self.pm.ls.side_effect = error
            with self.assertRaises(type(error)) as caught:
                self.inspect()
            self.assertIs(caught.exception, error)
        self.counter.assert_not_called()
        self.pm.rename.assert_not_called()

    def test_counter_allocation_failure_never_changes_scene(self):
        self.pm.ls.return_value = ['|left|same', '|right|same']
        primary = MemoryError('counter allocation failed')
        self.counter.side_effect = primary
        with self.assertRaises(MemoryError) as caught:
            self.inspect()
        self.assertIs(caught.exception, primary)
        self.assert_read_only()

    def test_actual_manager_inspection_keeps_sorted_distinct_display_and_no_rename(self):
        self.pm.ls.return_value = ['|left|b', '|left|a', '|right|b', '|right|a']
        display_pm = NS(warning=Mock())
        scope = methods('python/DPyTools/pipeline/assets/manager.py', 'DAssetManager',
                        {'pm': display_pm, 'dname': NS(checkUniqueName=self.inspect)}, ('checkProblemCB',))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(scope['checkProblemCB'](NS()), ['a', 'b'])
        display_pm.warning.assert_called_once()
        self.assert_read_only()

    def test_legacy_mutating_loop_and_prefix_helper_are_source_unchanged(self):
        tree = _tree('python/DPyTools/general/nameActions.py')
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                        and node.name == 'checkUniqueName')
        expected = ast.parse(textwrap.dedent('''
            not_unique_list = []
            for i in range(len(short_names)):
                startIndex = 0
                copyCount = short_names.count(short_names[i])
                if copyCount > 1:
                    dup = short_names[i]
                    not_unique_list.append(dup)
                    if isRename:
                        for j in range(copyCount):
                            index = short_names.index(dup, startIndex)
                            short_names[index] = pm.rename(objs[index], dup + '_' + str(j))
                            startIndex = index + 1
            return not_unique_list
        '''))
        actual = ast.Module(body=function.body[-3:], type_ignores=[])
        self.assertEqual(ast.dump(actual), ast.dump(expected))
        prefix = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                      and node.name == 'objectNameAppendPrefix')
        self.assertEqual([argument.arg for argument in prefix.args.args], ['objects', 'prefix'])
        expected_prefix = ast.parse(textwrap.dedent('''
            if objects is None:
                objects = []
            for obj in objects:
                pm.rename(obj, prefix + str(obj))
        '''))
        actual_prefix = ast.Module(body=prefix.body[1:], type_ignores=[])
        self.assertEqual(ast.dump(actual_prefix), ast.dump(expected_prefix))
        self.pm.rename.assert_not_called()


if __name__ == '__main__':
    unittest.main()
