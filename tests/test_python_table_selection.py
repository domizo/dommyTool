#!/usr/bin/env python3
"""Selection admission and actual user/mailing callbacks with controlled stubs."""

import ast
import sys
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.pipeline.assets import user_data
from DPyTools.ui import table_selection


def _tree(relative):
    filename = ROOT / relative
    converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
    return ast.parse(str(converter.refactor_string(
        filename.read_text(encoding='utf-8') + '\n', str(filename))))


def _callback(relative, class_name, method, namespace, helpers=()):
    tree = _tree(relative)
    body = [node for node in tree.body
            if isinstance(node, ast.FunctionDef) and node.name in helpers]
    ui = next(node for node in tree.body
              if isinstance(node, ast.ClassDef) and node.name == class_name)
    body.extend(node for node in ui.body
                if isinstance(node, ast.FunctionDef) and node.name == method)
    module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
    exec(compile(module, str(ROOT / relative), 'exec'), namespace)
    return namespace[method]


class TableSelectionTests(unittest.TestCase):
    def test_empty_single_and_multiple_selection(self):
        for value in (None, 0):
            self.assertEqual(table_selection.selected_rows(value, 3), ())
        for value in (None, [], ()):
            self.assertEqual(table_selection.selected_rows(value, 0, multiple=True), ())

    def test_valid_rows_keep_display_order(self):
        self.assertEqual(table_selection.selected_rows(2, 3), (2,))
        self.assertEqual(table_selection.selected_rows([3, 1], 3, multiple=True), (3, 1))

    def test_invalid_single_rows_and_counts_fail(self):
        for value in (-1, 4, True, False, 1.5, '1', [1]):
            with self.subTest(value=value), self.assertRaises((TypeError, ValueError)):
                table_selection.selected_rows(value, 3)
        for count in (-1, True, 1.0, '1'):
            with self.subTest(count=count), self.assertRaises(ValueError):
                table_selection.selected_rows(1, count)

    def test_invalid_or_duplicate_multiple_rows_fail(self):
        for value in (1, '1', [0], [-1], [True], [4], [1, 1], [1, 2, 3, 4]):
            with self.subTest(value=value), self.assertRaises((TypeError, ValueError)):
                table_selection.selected_rows(value, 3, multiple=True)

    def test_cell_text_preserves_full_unicode_identity(self):
        for value in ('artist_b', ['artist_b'], ('artist_b',), '担当者_b'):
            self.assertEqual(table_selection.cell_text(value),
                             value if isinstance(value, str) else value[0])

    def test_ambiguous_or_nontext_cells_fail(self):
        for value in (None, [], ['a', 'b'], [True], 1, {'id': 'a'}):
            with self.subTest(value=value), self.assertRaises(TypeError):
                table_selection.cell_text(value)


class UserSelectionCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.namespace = {'table_selection': table_selection, 'user_data': user_data}
        cls.callback = staticmethod(_callback(
            'python/DPyTools/pipeline/assets/users.py', 'DUserSelectWin', 'addUserCB',
            cls.namespace, helpers=('_selected_user_ids',)))

    def setUp(self):
        self.rows = [1]
        self.cells = {1: 'artist_b', 2: ['artist_a']}
        self.queries = []
        self.closed = Mock()
        self.warnings = []
        self.receiver = Mock(return_value=True)

        def table(unused, **kwargs):
            self.queries.append(kwargs)
            if kwargs.get('selectedRows'):
                return self.rows
            return self.cells[kwargs['cellIndex'][0]]

        self.table = Mock(side_effect=table)
        self.namespace.update(cmds=SimpleNamespace(scriptTable=self.table),
                              pm=SimpleNamespace(warning=self.warnings.append,
                                                 deleteUI=self.closed))
        self.ui = SimpleNamespace(table='table', userWin='window',
                                  m_list={'artist_a': {}, 'artist_b': {}, '担当者_b': {}},
                                  w_cntlr=SimpleNamespace(addUserFromuserIds=self.receiver))

    def test_plain_and_one_item_cells_forward_complete_ids(self):
        for cell in ('artist_b', ['artist_b'], ('artist_b',), '担当者_b'):
            with self.subTest(cell=cell):
                self.cells[1] = cell
                self.assertTrue(self.callback(self.ui))
                self.receiver.assert_called_with(
                    userids=[cell if isinstance(cell, str) else cell[0]])
        self.assertEqual(self.closed.call_count, 4)

    def test_multiple_rows_are_preflighted_and_keep_display_order(self):
        self.rows = [1, 2]
        self.assertTrue(self.callback(self.ui))
        self.receiver.assert_called_once_with(userids=['artist_b', 'artist_a'])
        self.assertEqual(list(self.queries[1]), ['cellIndex', 'q', 'cellValue'])

    def test_bad_second_cell_does_not_apply_first_user(self):
        self.rows = [1, 2]
        for cell in (None, [], ['a', 'b'], [True], '', 'unknown', 'artist_b'):
            with self.subTest(cell=cell):
                self.cells[2] = cell
                self.assertFalse(self.callback(self.ui))
        self.receiver.assert_not_called()
        self.closed.assert_not_called()

    def test_invalid_rows_cannot_apply_or_close(self):
        for rows in (-1, '1', [0], [-1], [True], [4], [1, 1]):
            with self.subTest(rows=rows):
                self.rows = rows
                self.assertFalse(self.callback(self.ui))
        self.receiver.assert_not_called()
        self.closed.assert_not_called()

    def test_empty_selection_keeps_dialog_open(self):
        for rows in (None, [], ()):
            self.rows = rows
            self.assertFalse(self.callback(self.ui))
        self.receiver.assert_not_called()
        self.closed.assert_not_called()

    def test_query_failure_does_not_apply_or_close(self):
        self.table.side_effect = RuntimeError('query failed')
        self.assertFalse(self.callback(self.ui))
        self.receiver.assert_not_called()
        self.closed.assert_not_called()
        self.assertTrue(self.warnings)

    def test_missing_or_failing_receiver_keeps_dialog_open(self):
        for receiver in (None, SimpleNamespace(addUserFromuserIds=1)):
            self.ui.w_cntlr = receiver
            self.assertFalse(self.callback(self.ui))
        self.ui.w_cntlr = SimpleNamespace(addUserFromuserIds=self.receiver)
        self.receiver.side_effect = OSError('save failed')
        self.assertFalse(self.callback(self.ui))
        self.closed.assert_not_called()

    def test_unconfirmed_save_cannot_close_selector_as_success(self):
        for result in (None, False, 1, 'saved', []):
            self.receiver.return_value = result
            self.assertFalse(self.callback(self.ui))
        self.closed.assert_not_called()

    def test_cancel_button_targets_defined_method(self):
        tree = _tree('python/DPyTools/pipeline/assets/users.py')
        ui = next(node for node in tree.body
                  if isinstance(node, ast.ClassDef) and node.name == 'DUserSelectWin')
        methods = {node.name for node in ui.body if isinstance(node, ast.FunctionDef)}
        targets = [node.attr for node in ast.walk(ui)
                   if isinstance(node, ast.Attribute) and node.attr.lower() == 'cancelcb']
        self.assertEqual(targets, ['cancelCB'])
        self.assertIn(targets[0], methods)


class MailingSelectionCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.namespace = {'table_selection': table_selection, 'user_data': user_data}
        cls.callback = staticmethod(_callback(
            'python/DPyTools/integrations/smtp/mail.py', 'DMailingListManager',
            'delUserCB', cls.namespace))

    def setUp(self):
        self.row = 2
        self.warnings = []
        self.table = Mock(side_effect=lambda *args, **kwargs: self.row)
        self.delete = Mock(return_value=True)
        self.namespace.update(cmds=SimpleNamespace(scriptTable=self.table),
                              pm=SimpleNamespace(warning=self.warnings.append),
                              dasset=SimpleNamespace(deleteUserMailningList=self.delete))
        self.ui = SimpleNamespace(table='table', m_list=['first', 'middle', 'last'],
                                  refleshCB=Mock(), _project_path='/project-A')

    def test_valid_row_deletes_exact_cached_id_and_refreshes_after_commit(self):
        self.callback(self.ui)
        self.delete.assert_called_once_with(userid='middle', project_path='/project-A')
        self.ui.refleshCB.assert_called_once_with()

    def test_bad_rows_never_delete_another_item(self):
        for row in (None, 0, -1, 4, True, False, 1.5, '1', [1]):
            self.row = row
            self.callback(self.ui)
        self.delete.assert_not_called()
        self.ui.refleshCB.assert_not_called()

    def test_query_or_cached_id_failure_cannot_delete(self):
        self.table.side_effect = RuntimeError('query failed')
        self.callback(self.ui)
        self.table.side_effect = lambda *args, **kwargs: self.row
        self.ui.m_list[1] = 'bad\nid'
        self.callback(self.ui)
        self.delete.assert_not_called()
        self.ui.refleshCB.assert_not_called()
        self.assertTrue(self.warnings)

    def test_failed_save_does_not_refresh_as_success(self):
        self.delete.return_value = False
        self.callback(self.ui)
        self.delete.assert_called_once_with(userid='middle', project_path='/project-A')
        self.ui.refleshCB.assert_not_called()


class MailingBatchCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.namespace = {}
        cls.callback = staticmethod(_callback(
            'python/DPyTools/integrations/smtp/mail.py', 'DMailingListManager',
            'addUserFromuserIds', cls.namespace))

    def setUp(self):
        self.save = Mock(return_value=True)
        self.warnings = []
        self.namespace.update(dasset=SimpleNamespace(addUsersMailingList=self.save),
                              pm=SimpleNamespace(warning=self.warnings.append))
        self.ui = SimpleNamespace(m_list=['cached_old_id'], refleshCB=Mock(), _project_path='/project-A')

    def test_addition_uses_one_batch_call_and_propagates_commit_status(self):
        self.assertTrue(self.callback(self.ui, ['artist_b', 'artist_a']))
        self.save.assert_called_once_with(userids=['artist_b', 'artist_a'], project_path='/project-A')
        self.ui.refleshCB.assert_called_once_with()

    def test_failed_batch_does_not_refresh_or_report_success(self):
        self.save.return_value = False
        self.assertFalse(self.callback(self.ui, ['artist_b', 'artist_a']))
        self.ui.refleshCB.assert_not_called()

    def test_saved_batch_with_refresh_failure_still_reports_commit(self):
        self.ui.refleshCB.side_effect = RuntimeError('refresh failed')
        self.assertTrue(self.callback(self.ui, ['artist_b']))
        self.assertTrue(any('was saved' in value for value in self.warnings))


if __name__ == '__main__':
    unittest.main()
