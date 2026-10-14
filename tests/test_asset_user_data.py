#!/usr/bin/env python3
"""Unit tests for host-independent user and mailing-list schemas."""

import ast
import functools
import numbers
import os
import sys
import tempfile
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import repository, user_data
from DPyTools.ui import table_selection
from test_python_table_selection import _callback


@functools.lru_cache(maxsize=2)
def _persistence_code(module_name):
    filename = REPO_ROOT / ('python/DPyTools/pipeline/assets/' + module_name + '.py')
    converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
    tree = ast.parse(str(converter.refactor_string(
        filename.read_text(encoding='utf-8') + '\n', str(filename))))
    names = {'_delete_user', '_selected_user_ids', 'deleteUserFromId', 'deleteUserFromName',
             'getUserList', 'addUserData', 'addUser', 'getMaillingList',
             'addUserMailingList', 'addUsersMailingList', 'deleteUserMailningList'}
    functions = [node for node in tree.body
                 if isinstance(node, ast.FunctionDef) and node.name in names]
    if module_name == 'users':
        ui_class = next(node for node in tree.body
                        if isinstance(node, ast.ClassDef) and node.name == 'DUserManager')
        functions.extend(node for node in ui_class.body
                         if isinstance(node, ast.FunctionDef) and node.name == 'delUserCB')
    module = ast.fix_missing_locations(ast.Module(body=functions, type_ignores=[]))
    return compile(module, str(filename), 'exec')


def _user(identifier, name='Artist'):
    return {'id': identifier, 'name': name, 'mail': 'artist@example.com'}


class AssetUserDataTests(unittest.TestCase):
    def setUp(self):
        self.user = {"id": "artist01", "name": "Artist One", "mail": "a@example.com"}

    def test_user_collection_requires_matching_ids(self):
        validated = user_data.validate_user_collection({"artist01": self.user})
        self.assertEqual(validated["artist01"], self.user)
        with self.assertRaises(ValueError):
            user_data.validate_user_collection({"other": self.user})

    def test_invalid_mail_and_control_characters_are_rejected(self):
        with self.assertRaises(ValueError):
            user_data.validate_user_record(dict(self.user, mail="not-an-address"))
        with self.assertRaises(ValueError):
            user_data.validate_user_record(dict(self.user, name="Artist\nInjected"))

    def test_mailing_list_must_be_unique_and_known_when_requested(self):
        known = {"artist01": self.user}
        self.assertEqual(
            user_data.validate_mailing_list(["artist01"], known_users=known),
            ["artist01"],
        )
        with self.assertRaises(ValueError):
            user_data.validate_mailing_list(["artist01", "artist01"])
        with self.assertRaises(ValueError):
            user_data.validate_mailing_list(["unknown"], known_users=known)


class AssetUserPersistenceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommytool-user-persistence-')
        self.addCleanup(self.directory.cleanup)
        self.filename = str(Path(self.directory.name) / 'users.json')
        self.mailing_filename = str(Path(self.directory.name) / 'mailing.json')
        self.warnings = []
        self.data = {'artist_a': _user('artist_a', 'Same Name'),
                     'artist_b': _user('artist_b', 'Same Name')}
        repository.save(self.filename, self.data)
        self.row, self.cell = 2, ['artist_b']
        self.table_queries = []

        def table(unused_table, **kwargs):
            self.table_queries.append(kwargs)
            if kwargs.get('selectedRow'):
                return self.row
            if kwargs.get('cellValue'):
                return self.cell
            raise AssertionError('Unexpected table query')

        self.namespace = {
            'numbers': numbers, 'user_data': user_data, 'table_selection': table_selection,
            'user_list_path': self.filename, 'userIdKey': 'id',
            'userNameKey': 'name', 'userMailKey': 'mail',
            'asset_repository': repository,
            'dpath': SimpleNamespace(isFile=lambda path: os.path.isfile(path)),
            'pm': SimpleNamespace(warning=self.warnings.append),
            'cmds': SimpleNamespace(scriptTable=table),
        }
        exec(_persistence_code('users'), self.namespace)
        self.ui = SimpleNamespace(
            m_list=self.data, table='fake_table', refleshCB=Mock(),
            getCurrentKey=lambda row: sorted(self.data)[row])

    def test_actual_callback_deletes_selected_id_not_same_name_peer(self):
        self.namespace['delUserCB'](self.ui)
        self.assertEqual(set(repository.load_mapping(self.filename)), {'artist_a'})
        self.ui.refleshCB.assert_called_once_with()
        self.assertEqual(self.data, {'artist_a': _user('artist_a', 'Same Name'),
                                     'artist_b': _user('artist_b', 'Same Name')})

    def test_callback_uses_displayed_id_even_when_row_order_differs(self):
        self.row = 1
        self.namespace['delUserCB'](self.ui)
        self.assertEqual(set(repository.load_mapping(self.filename)), {'artist_a'})
        self.assertIn({'q': 1, 'cellIndex': [1, 1], 'cellValue': 1}, self.table_queries)
        self.assertEqual(list(self.table_queries[-1]), ['cellIndex', 'q', 'cellValue'])

    def test_ambiguous_legacy_name_api_preserves_file(self):
        original = Path(self.filename).read_bytes()
        self.assertFalse(self.namespace['deleteUserFromName']('Same Name'))
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertTrue(self.warnings)

    def test_unique_legacy_name_api_still_deletes_one_user(self):
        self.data['artist_b']['name'] = 'Other Name'
        repository.save(self.filename, self.data)
        self.assertTrue(self.namespace['deleteUserFromName']('Other Name'))
        self.assertEqual(set(repository.load_mapping(self.filename)), {'artist_a'})

    def test_id_api_rejects_missing_and_invalid_ids_without_writing(self):
        original = Path(self.filename).read_bytes()
        for identifier in ('unknown', '', None, True, [], 'artist_a\n'):
            with self.subTest(identifier=identifier):
                self.assertFalse(self.namespace['deleteUserFromId'](identifier))
                self.assertEqual(Path(self.filename).read_bytes(), original)

    def test_bad_rows_and_stale_or_malformed_cells_cannot_delete(self):
        original = Path(self.filename).read_bytes()
        for row in (None, 0, -1, 3, True, 1.5, '1'):
            with self.subTest(row=row):
                self.row = row
                self.namespace['delUserCB'](self.ui)
                self.assertEqual(Path(self.filename).read_bytes(), original)
        self.row = 1
        for cell in (None, [], ['artist_a', 'artist_b'], '', 'unknown', ['unknown'], [True]):
            with self.subTest(cell=cell):
                self.cell = cell
                self.namespace['delUserCB'](self.ui)
                self.assertEqual(Path(self.filename).read_bytes(), original)
        self.ui.refleshCB.assert_not_called()

    def test_cell_query_accepts_plain_text_or_one_item_sequence(self):
        for cell in ('artist_b', ['artist_b'], ('artist_b',)):
            with self.subTest(cell=cell):
                repository.save(self.filename, self.data)
                self.cell = cell
                self.namespace['delUserCB'](self.ui)
                self.assertEqual(set(repository.load_mapping(self.filename)), {'artist_a'})

    def test_id_deletion_and_unique_name_api_preserve_unicode_identity(self):
        data = {'担当者_a': _user('担当者_a', '担当者 A'),
                '担当者_b': _user('担当者_b', '担当者 B')}
        repository.save(self.filename, data)
        self.assertTrue(self.namespace['deleteUserFromId']('担当者_b'))
        self.assertEqual(repository.load_mapping(self.filename), {'担当者_a': data['担当者_a']})
        self.assertTrue(self.namespace['deleteUserFromName']('担当者 A'))
        self.assertEqual(repository.load_mapping(self.filename), {})

    def test_query_and_save_errors_keep_file_and_do_not_refresh(self):
        original = Path(self.filename).read_bytes()
        with patch.object(self.namespace['cmds'], 'scriptTable', side_effect=RuntimeError('query failed')):
            self.namespace['delUserCB'](self.ui)
        with patch.object(repository, 'save', side_effect=OSError('save failed')):
            self.namespace['delUserCB'](self.ui)
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.ui.refleshCB.assert_not_called()

    def test_legacy_facade_exports_id_api_as_same_function(self):
        filename = REPO_ROOT / 'python/DPyTools/utility/DUserManager.py'
        body = [node for node in ast.parse(filename.read_text(encoding='utf-8')).body
                if not isinstance(node, ast.ImportFrom)]
        namespace = dict(self.namespace, DAddUserWin=object(), DUserManager=object(),
                         DUserSelectWin=object())
        result = {'users': SimpleNamespace(**namespace)}
        exec(compile(ast.fix_missing_locations(ast.Module(body=body, type_ignores=[])),
                     str(filename), 'exec'), result)
        self.assertIn('deleteUserFromId', result['__all__'])
        self.assertIs(result['deleteUserFromId'], self.namespace['deleteUserFromId'])
        self.assertIs(result['deleteUserFromName'], self.namespace['deleteUserFromName'])

    def _mailing_namespace(self):
        namespace = dict(self.namespace,
                         getMaillingListFile=lambda **kw: self.mailing_filename,
                         dusers=SimpleNamespace(getUserList=lambda: self.data))
        exec(_persistence_code('operations'), namespace)
        return namespace

    def test_actual_user_add_at_limit_keeps_readable_previous_file(self):
        data = {'u{0:05d}'.format(index): _user('u{0:05d}'.format(index))
                for index in range(user_data.MAX_USERS)}
        repository.save(self.filename, data)
        original = Path(self.filename).read_bytes()
        with self.assertRaises(ValueError):
            self.namespace['addUserData'](_user('new_user'))
        self.assertFalse(self.namespace['addUser']('Artist', 'new_user', 'artist@example.com'))
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertEqual(len(self.namespace['getUserList']()), user_data.MAX_USERS)
        self.assertEqual(os.listdir(self.directory.name), ['users.json'])

    def test_actual_mailing_add_at_limit_keeps_readable_previous_file(self):
        data = ['old{0:05d}'.format(index) for index in range(user_data.MAX_USERS)]
        repository.save(self.mailing_filename, data)
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        namespace['addUserMailingList']('artist_b')
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)
        self.assertEqual(namespace['getMaillingList'](), data)
        self.assertTrue(self.warnings)

    def test_user_creation_addition_and_duplicate_noop_remain_compatible(self):
        os.unlink(self.filename)
        self.assertTrue(self.namespace['addUserData'](_user('artist_a')))
        self.assertTrue(self.namespace['addUserData'](_user('artist_b')))
        original = Path(self.filename).read_bytes()
        self.assertFalse(self.namespace['addUserData'](_user('artist_b', 'Other Name')))
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertEqual(set(self.namespace['getUserList']()), {'artist_a', 'artist_b'})

    def test_mailing_creation_append_duplicate_and_removal_keep_order(self):
        namespace = self._mailing_namespace()
        namespace['addUserMailingList']('artist_b')
        namespace['addUserMailingList']('artist_a')
        original = Path(self.mailing_filename).read_bytes()
        namespace['addUserMailingList']('artist_b')
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)
        self.assertEqual(namespace['getMaillingList'](), ['artist_b', 'artist_a'])
        namespace['deleteUserMailningList']('artist_b')
        self.assertEqual(namespace['getMaillingList'](), ['artist_a'])

    def test_schema_writers_reject_invalid_collections_before_generic_save(self):
        oversized_users = {'u{0:05d}'.format(index): _user('u{0:05d}'.format(index))
                           for index in range(user_data.MAX_USERS + 1)}
        oversized_mail = ['u{0:05d}'.format(index) for index in range(user_data.MAX_USERS + 1)]
        with patch.object(repository, 'save') as save:
            for data in (oversized_users, {'other': _user('artist_a')}):
                with self.assertRaises(ValueError):
                    repository.save_user_collection(self.filename, data)
            for data in (oversized_mail, ['artist_a', 'artist_a'], ['bad\nid']):
                with self.assertRaises(ValueError):
                    repository.save_mailing_list(self.mailing_filename, data)
        save.assert_not_called()

    def test_production_user_and_mailing_mutations_use_schema_writers(self):
        with patch.object(repository, 'save_user_collection', wraps=repository.save_user_collection) as users:
            self.namespace['addUserData'](_user('artist_c'))
            self.namespace['deleteUserFromId']('artist_c')
        self.assertEqual(users.call_count, 2)
        namespace = self._mailing_namespace()
        with patch.object(repository, 'save_mailing_list', wraps=repository.save_mailing_list) as mailing:
            namespace['addUserMailingList']('artist_a')
            namespace['addUserMailingList']('artist_b')
            namespace['deleteUserMailningList']('artist_a')
        self.assertEqual(mailing.call_count, 3)

    def test_mailing_save_failure_retains_file_and_reports_warning(self):
        repository.save(self.mailing_filename, ['artist_a'])
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        with patch.object(repository, 'save', side_effect=OSError('save failed')):
            namespace['addUserMailingList']('artist_b')
            self.assertFalse(namespace['deleteUserMailningList']('artist_a'))
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)
        self.assertEqual(len(self.warnings), 2)

    def test_mailing_delete_status_is_true_only_after_successful_save(self):
        namespace = self._mailing_namespace()
        self.assertFalse(namespace['deleteUserMailningList']('artist_a'))
        repository.save(self.mailing_filename, ['artist_a', 'stale_user'])
        original = Path(self.mailing_filename).read_bytes()
        for identifier in ('unknown', None, True, '', 'bad\nid'):
            self.assertFalse(namespace['deleteUserMailningList'](identifier))
            self.assertEqual(Path(self.mailing_filename).read_bytes(), original)
        self.assertTrue(namespace['deleteUserMailningList']('stale_user'))
        self.assertEqual(repository.load_list(self.mailing_filename), ['artist_a'])

    def test_batch_add_uses_one_registry_snapshot_and_one_complete_save(self):
        namespace = self._mailing_namespace()
        with patch.object(namespace['dusers'], 'getUserList', return_value=self.data) as users, \
                patch.object(repository, 'save_mailing_list', wraps=repository.save_mailing_list) as save:
            self.assertTrue(namespace['addUsersMailingList'](['artist_b', 'artist_a']))
        users.assert_called_once_with()
        save.assert_called_once_with(self.mailing_filename, ['artist_b', 'artist_a'])
        self.assertEqual(namespace['getMaillingList'](), ['artist_b', 'artist_a'])

    def test_batch_add_refuses_bad_later_id_without_saving_prefix(self):
        repository.save_mailing_list(self.mailing_filename, ['artist_a'])
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        with patch.object(repository, 'save_mailing_list') as save:
            for values in (['artist_b', 'unknown'], ['artist_b', True], 'artist_b',
                           ['artist_b', 'bad\nid']):
                self.assertFalse(namespace['addUsersMailingList'](values))
        save.assert_not_called()
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)

    def test_batch_save_failure_keeps_previous_complete_file(self):
        repository.save_mailing_list(self.mailing_filename, ['artist_a'])
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        with patch.object(repository, 'save', side_effect=OSError('save failed')):
            self.assertFalse(namespace['addUsersMailingList'](['artist_b', 'artist_a']))
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)

    def test_batch_add_preserves_stale_ids_order_and_idempotent_duplicates(self):
        repository.save_mailing_list(self.mailing_filename, ['stale', 'artist_a'])
        namespace = self._mailing_namespace()
        self.assertTrue(namespace['addUsersMailingList'](('artist_b', 'artist_b', 'artist_a')))
        self.assertEqual(namespace['getMaillingList'](), ['stale', 'artist_a', 'artist_b'])
        with patch.object(repository, 'save_mailing_list') as save:
            self.assertTrue(namespace['addUsersMailingList'](['artist_b', 'artist_a']))
            self.assertTrue(namespace['addUsersMailingList']([]))
        save.assert_not_called()

    def test_batch_add_rechecks_fresh_file_not_cached_membership(self):
        namespace = self._mailing_namespace()
        self.assertTrue(namespace['addUserMailingList']('artist_b'))
        repository.save_mailing_list(self.mailing_filename, ['artist_a'])
        self.assertTrue(namespace['addUsersMailingList'](['artist_b']))
        self.assertEqual(namespace['getMaillingList'](), ['artist_a', 'artist_b'])

    def test_invalid_existing_file_and_missing_destination_fail_closed(self):
        Path(self.mailing_filename).write_text('{"invalid": "mapping"}', encoding='utf-8')
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        self.assertFalse(namespace['addUsersMailingList'](['artist_b']))
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)
        namespace['getMaillingListFile'] = lambda **kw: None
        self.assertFalse(namespace['addUsersMailingList'](['artist_b']))

    def test_merge_plan_is_bounded_and_does_not_modify_inputs(self):
        existing, requested = ['stale', 'artist_a'], ['artist_b', 'artist_b']
        self.assertEqual(user_data.merge_mailing_list(existing, requested, self.data),
                         ['stale', 'artist_a', 'artist_b'])
        self.assertEqual(existing, ['stale', 'artist_a'])
        self.assertEqual(requested, ['artist_b', 'artist_b'])
        with self.assertRaises(ValueError):
            user_data.merge_mailing_list(existing, ['artist_b'], self.data, max_entries=2)
        with self.assertRaises(ValueError):
            user_data.merge_mailing_list([], ['artist_a'] * 3, self.data, max_entries=2)
        for limit in (None, float('nan'), True, -1, 1.5, '1'):
            with self.assertRaises(ValueError):
                user_data.merge_mailing_list([], [], self.data, max_entries=limit)

    def test_batch_ui_failure_preserves_file_and_keeps_selector_open(self):
        repository.save_mailing_list(self.mailing_filename, ['old_id'])
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        mail_context = {'dasset': SimpleNamespace(addUsersMailingList=namespace['addUsersMailingList']),
                        'pm': SimpleNamespace(warning=self.warnings.append)}
        receiver = _callback('python/DPyTools/integrations/smtp/mail.py',
                             'DMailingListManager', 'addUserFromuserIds', mail_context)
        mail_ui = SimpleNamespace(m_list=['cached_old'], refleshCB=Mock(), _project_path='/project-A')
        closed = Mock()
        user_context = dict(self.namespace, pm=SimpleNamespace(warning=self.warnings.append, deleteUI=closed))
        user_context['cmds'] = SimpleNamespace(scriptTable=lambda unused, **kw:
            [1, 2] if kw.get('selectedRows') else
            {1: 'artist_b', 2: 'artist_a'}[kw['cellIndex'][0]])
        selector_callback = _callback('python/DPyTools/pipeline/assets/users.py',
                                     'DUserSelectWin', 'addUserCB', user_context,
                                     helpers=('_selected_user_ids',))
        selector = SimpleNamespace(table='table', userWin='window', m_list=self.data,
            w_cntlr=SimpleNamespace(addUserFromuserIds=lambda userids: receiver(mail_ui, userids)))
        with patch.object(repository, 'save', side_effect=OSError('save failed')):
            self.assertFalse(selector_callback(selector))
        closed.assert_not_called()
        mail_ui.refleshCB.assert_not_called()
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)

    def test_batch_crossing_collection_bound_cannot_publish_valid_prefix(self):
        data = ['old{0:05d}'.format(index) for index in range(user_data.MAX_USERS - 1)]
        repository.save_mailing_list(self.mailing_filename, data)
        original = Path(self.mailing_filename).read_bytes()
        namespace = self._mailing_namespace()
        self.assertFalse(namespace['addUsersMailingList'](['artist_a', 'artist_b']))
        self.assertEqual(Path(self.mailing_filename).read_bytes(), original)

    def test_merge_plan_rejects_ambiguous_known_user_container(self):
        for known in (None, 'artist_a', ['artist_a'], True):
            with self.assertRaises(TypeError):
                user_data.merge_mailing_list([], ['artist_a'], known)


if __name__ == "__main__":
    unittest.main()
