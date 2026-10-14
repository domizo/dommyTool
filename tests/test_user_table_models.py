#!/usr/bin/env python3
"""Immutable display snapshots and actual user/mailing callbacks without Maya."""

import copy
import unittest
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock

from test_asset_browser import methods
from DPyTools.pipeline.assets import user_data
from DPyTools.ui import table_selection


def registry():
    return {'b': {'id': 'b', 'name': '担当者 B', 'mail': 'b@example.com'},
            'a': {'id': 'a', 'name': 'Artist A', 'mail': 'a@example.com'}}


class UserTableModelTests(unittest.TestCase):
    def test_user_rows_are_sorted_immutable_text_snapshots(self):
        users = registry()
        rows = user_data.user_table_rows(users)
        self.assertEqual(rows, (('a', 'Artist A', 'a@example.com'), ('b', '担当者 B', 'b@example.com')))
        users['a']['name'] = 'Changed'
        users.clear()
        self.assertEqual(rows[0][1], 'Artist A')

    def test_mail_rows_preserve_order_stale_ids_and_later_input_independence(self):
        users, ids = registry(), ['b', 'stale', 'a']
        rows = user_data.mailing_table_rows(ids, users)
        self.assertEqual(rows, (('担当者 B',), ('',), ('Artist A',)))
        users['b']['name'] = 'Changed'
        ids.clear()
        self.assertEqual(rows[0][0], '担当者 B')

    def test_complete_models_are_validated_before_returning_rows(self):
        users = registry()
        users['b']['mail'] = 'bad\x00@example.com'
        with self.assertRaises(ValueError):
            user_data.user_table_rows(users)
        with self.assertRaises(ValueError):
            user_data.mailing_table_rows(['a', 'b'], users)
        with self.assertRaises(ValueError):
            user_data.mailing_table_rows(['a', 'a'], registry())

    def test_display_cells_reject_invalid_rows_and_columns_without_negative_indexing(self):
        rows = user_data.user_table_rows(registry())
        for value in (None, 0, -1, 3, True, False, 1.0, '1', [], 10 ** 400):
            self.assertEqual(table_selection.display_cell(rows, value, 1), '')
        for value in (None, 0, -1, 4, True, False, 1.0, '1', []):
            self.assertEqual(table_selection.display_cell(rows, 1, value), '')
        self.assertEqual(table_selection.display_cell(rows, 2, 2), '担当者 B')

    def test_invalid_or_empty_model_cells_are_blank(self):
        for rows in (None, 'rows', [], [None], [[None]], [()], [('x',)] * 10001):
            self.assertEqual(table_selection.display_cell(rows, 1, 1), '')

    def test_user_model_accepts_exact_bound_and_refuses_overflow(self):
        users = {'u{0:05d}'.format(index): {'id': 'u{0:05d}'.format(index), 'name': 'Artist',
                                         'mail': 'a@example.com'}
                 for index in range(user_data.MAX_USERS)}
        self.assertEqual(len(user_data.user_table_rows(users)), user_data.MAX_USERS)
        users['overflow'] = {'id': 'overflow', 'name': 'Artist', 'mail': 'a@example.com'}
        with self.assertRaises(ValueError):
            user_data.user_table_rows(users)


class UserTableAdapterTests(unittest.TestCase):
    def fixture(self, class_name):
        data = registry()
        get_users = Mock(return_value=data)
        cmds = NS(scriptTable=Mock())
        scope = methods('python/DPyTools/pipeline/assets/users.py', class_name,
                        {'user_data': user_data, 'table_selection': table_selection,
                         'getUserList': get_users, 'cmds': cmds},
                        ['loadUserData', 'refleshCB', 'getCellCB', 'clearTableCB'])
        owner = NS(table='table', m_list={}, _table_rows=())
        for name, function in scope.items():
            if name in ('loadUserData', 'refleshCB', 'getCellCB', 'clearTableCB'):
                setattr(owner, name, MethodType(function, owner))
        return owner, data, get_users, cmds

    def test_both_user_views_show_sorted_cells_without_native_queries_per_cell(self):
        for name in ('DUserSelectWin', 'DUserManager'):
            owner, data, get_users, cmds = self.fixture(name)
            owner.refleshCB()
            get_users.assert_called_once_with()
            cmds.scriptTable.reset_mock()
            self.assertEqual(owner.getCellCB(1, 1), 'a')
            self.assertEqual(owner.getCellCB(2, 2), '担当者 B')
            for row in (0, -1, 3, True, None):
                self.assertEqual(owner.getCellCB(row, 1), '')
            self.assertEqual(owner.getCellCB(1, 4), '')
            data.clear()
            self.assertEqual(owner.getCellCB(2, 2), '担当者 B')
            cmds.scriptTable.assert_not_called()

    def test_bad_user_snapshot_does_not_clear_old_model_or_native_rows(self):
        for name in ('DUserSelectWin', 'DUserManager'):
            owner, data, get_users, cmds = self.fixture(name)
            owner.refleshCB()
            previous = owner._table_rows
            get_users.return_value = {'invalid': {'id': 'different'}}
            cmds.scriptTable.reset_mock()
            with self.assertRaises(ValueError):
                owner.refleshCB()
            self.assertIs(owner._table_rows, previous)
            cmds.scriptTable.assert_not_called()

    def test_refresh_to_empty_explicitly_clears_native_rows_and_snapshot(self):
        for name in ('DUserSelectWin', 'DUserManager'):
            owner, data, get_users, cmds = self.fixture(name)
            owner.refleshCB()
            get_users.return_value = {}
            owner.refleshCB()
            self.assertEqual(owner._table_rows, ())
            self.assertEqual(cmds.scriptTable.call_args_list[-2].kwargs['rows'], 0)
            self.assertEqual(owner.getCellCB(1, 1), '')

    def test_explicit_clear_also_clears_display_snapshot(self):
        for name in ('DUserSelectWin', 'DUserManager'):
            owner, data, get_users, cmds = self.fixture(name)
            owner.refleshCB()
            owner.clearTableCB()
            self.assertEqual(owner.m_list, {})
            self.assertEqual(owner._table_rows, ())


class MailingTableAdapterTests(unittest.TestCase):
    def setUp(self):
        self.data = registry()
        self.ids = ['a']
        self.get_users = Mock(side_effect=lambda: copy.deepcopy(self.data))
        self.get_ids = Mock(side_effect=lambda **kwargs: list(self.ids))
        self.cmds = NS(scriptTable=Mock())
        scope = methods('python/DPyTools/integrations/smtp/mail.py', 'DMailingListManager',
                        {'user_data': user_data, 'table_selection': table_selection, 'cmds': self.cmds,
                         'dusers': NS(getUserList=self.get_users),
                         'dasset': NS(getMaillingList=self.get_ids)},
                        ['loadMailListData', 'refleshCB', 'getCellCB', 'clearTableCB'])
        self.owner = NS(table='mail', _project_path='/retained-project', m_list=[], m_user_list={}, _table_rows=())
        for name in ('loadMailListData', 'refleshCB', 'getCellCB', 'clearTableCB'):
            setattr(self.owner, name, MethodType(scope[name], self.owner))

    def test_refresh_reads_both_models_once_and_uses_retained_project(self):
        self.owner.refleshCB()
        self.get_ids.assert_called_once_with(project_path='/retained-project')
        self.get_users.assert_called_once_with()
        self.assertEqual(self.owner.getCellCB(1, 1), 'Artist A')

    def test_added_or_renamed_users_get_fresh_names_after_refresh(self):
        self.owner.refleshCB()
        self.data['new'] = {'id': 'new', 'name': 'New Artist', 'mail': 'new@example.com'}
        self.data['a']['name'] = 'Renamed Artist'
        self.ids.append('new')
        self.owner.refleshCB()
        self.assertEqual(self.owner.getCellCB(1, 1), 'Renamed Artist')
        self.assertEqual(self.owner.getCellCB(2, 1), 'New Artist')

    def test_stale_user_rows_remain_in_order_without_mutating_mailing_ids(self):
        self.ids = ['stale', 'b', 'a']
        self.owner.refleshCB()
        self.assertEqual(self.owner.m_list, self.ids)
        self.assertEqual([self.owner.getCellCB(row, 1) for row in (1, 2, 3)], ['', '担当者 B', 'Artist A'])

    def test_bad_later_registry_record_keeps_previous_snapshot_and_view(self):
        self.owner.refleshCB()
        previous = (self.owner.m_list, self.owner.m_user_list, self.owner._table_rows)
        self.data['b']['mail'] = 'bad\x00@example.com'
        self.cmds.scriptTable.reset_mock()
        with self.assertRaises(ValueError):
            self.owner.refleshCB()
        self.assertEqual((self.owner.m_list, self.owner.m_user_list, self.owner._table_rows), previous)
        self.cmds.scriptTable.assert_not_called()

    def test_invalid_cell_coordinates_never_query_native_table(self):
        self.owner.refleshCB()
        self.cmds.scriptTable.reset_mock()
        for row in (None, 0, -1, 2, True, 1.5, '1'):
            self.assertEqual(self.owner.getCellCB(row, 1), '')
        for column in (None, 0, -1, 2, True, 1.5, '1'):
            self.assertEqual(self.owner.getCellCB(1, column), '')
        self.cmds.scriptTable.assert_not_called()

    def test_source_changes_do_not_mutate_retained_cells_without_refresh(self):
        self.owner.refleshCB()
        self.data.clear()
        self.ids.clear()
        self.owner.m_user_list.clear()
        self.owner.m_list.clear()
        self.assertEqual(self.owner.getCellCB(1, 1), 'Artist A')

    def test_empty_refresh_and_explicit_clear_remove_old_rows(self):
        self.owner.refleshCB()
        self.ids.clear()
        self.owner.refleshCB()
        self.assertEqual(self.owner._table_rows, ())
        self.assertEqual(self.cmds.scriptTable.call_args_list[-2].kwargs['rows'], 0)
        self.owner.clearTableCB()
        self.assertEqual(self.owner.m_list, [])

    def test_interruption_during_read_keeps_previous_model(self):
        self.owner.refleshCB()
        previous = self.owner._table_rows
        self.get_users.side_effect = KeyboardInterrupt()
        self.cmds.scriptTable.reset_mock()
        with self.assertRaises(KeyboardInterrupt):
            self.owner.refleshCB()
        self.assertIs(self.owner._table_rows, previous)
        self.cmds.scriptTable.assert_not_called()


if __name__ == '__main__':
    unittest.main()
