#!/usr/bin/env python3
"""Explicit owning project contexts, mailing snapshots and shared-setting changes."""

import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_asset_browser import BrowserFixture, methods
from types import MethodType
from DPyTools.pipeline.assets import creation, paths as asset_paths, repository, user_data, versioning


class ExplicitContextTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommy-project-context-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.a = self.root / 'root-A' / 'same'
        self.b = self.root / 'root-B' / 'same'
        for project in (self.a, self.b):
            (project / 'works' / 'assets' / 'PROPS' / 'prop' / 'v001').mkdir(parents=True)
        repository.save_mailing_list(str(self.a / '.proj_ml'), ['artist_a'])
        repository.save_mailing_list(str(self.b / '.proj_ml'), ['artist_b'])
        self.users = {name: {'id': name, 'name': name, 'mail': name + '@example.invalid'}
                      for name in ('artist_a', 'artist_b')}
        self.prefs = {'root': str(self.b.parent), 'current': 'same'}
        self.read_prefs = Mock(side_effect=lambda **kw: dict(self.prefs))
        self.scope = functions('python/DPyTools/pipeline/assets/operations.py', {
            'asset_paths': asset_paths, 'asset_repository': repository, 'user_data': user_data,
            'creation': creation, 'versioning': versioning, 'root_path_key': 'root',
            'current_project_key': 'current', 'mailingList_file': '.proj_ml',
            'pm': NS(warning=Mock()), '_LOGGER': NS(debug=lambda *a: None),
            'dusers': NS(getUserList=Mock(side_effect=lambda: dict(self.users))),
            'dpath': NS(isDir=os.path.isdir, isFile=os.path.isfile,
                        append=lambda *a, **kw: os.path.join(*a) if a else os.path.join(kw['path'], kw['other']))})
        self.scope['getAssetUserDataDic'] = self.read_prefs
    def test_explicit_path_reads_do_not_consult_another_shared_root_with_the_same_project_name(self):
        self.read_prefs.side_effect = RuntimeError('global preferences must not be read')
        self.assertEqual(self.scope['getAssetFolderPath'](project_path=str(self.a)), str(self.a / 'works' / 'assets'))
        self.assertEqual(self.scope['getAssetList']('PROPS', project_path=str(self.a)), ['prop'])
        self.assertEqual(self.scope['getExportPath']('PROPS', 'prop', project_path=str(self.a)),
                         str(self.a / 'works' / 'assets' / 'PROPS' / 'prop' / 'v002'))
        self.assertEqual(self.scope['generateAssetFileName']('PROPS', 'prop', project_path=str(self.a)), 'prop_v002.ma')
        self.read_prefs.assert_not_called()
    def test_mail_addresses_are_from_the_explicit_project_not_shared_preferences(self):
        self.assertEqual(self.scope['getMailAddressFromML'](project_path=str(self.a)), ['artist_a@example.invalid'])
        self.read_prefs.assert_not_called()
        self.assertEqual(self.scope['getMailAddressFromML'](), ['artist_b@example.invalid'])
    def test_explicit_mailing_deletion_and_addition_leave_the_other_project_unchanged(self):
        other = (self.b / '.proj_ml').read_bytes()
        self.assertTrue(self.scope['deleteUserMailningList']('artist_a', project_path=str(self.a)))
        self.assertTrue(self.scope['addUsersMailingList'](['artist_b'], project_path=str(self.a)))
        self.assertEqual(repository.load_list(str(self.a / '.proj_ml')), ['artist_b'])
        self.assertEqual((self.b / '.proj_ml').read_bytes(), other)
        self.read_prefs.assert_not_called()
    def test_empty_or_invalid_explicit_context_never_falls_back_to_shared_preferences(self):
        other = (self.b / '.proj_ml').read_bytes()
        self.assertEqual(self.scope['getMaillingList'](project_path=''), [])
        self.assertEqual(self.scope['getAssetList']('PROPS', project_path=''), [])
        self.assertFalse(self.scope['addUsersMailingList'](['artist_a'], project_path=''))
        for path in ('relative', 'bad\x00path', True):
            with self.assertRaises((TypeError, ValueError)):
                self.scope['getMaillingListFile'](project_path=path)
        self.read_prefs.assert_not_called()
        self.assertEqual((self.b / '.proj_ml').read_bytes(), other)
    def test_escaping_mailing_symlink_is_rejected_before_read_or_save(self):
        (self.a / '.proj_ml').unlink()
        (self.a / '.proj_ml').symlink_to(self.b / '.proj_ml')
        other = (self.b / '.proj_ml').read_bytes()
        with self.assertRaises(ValueError): self.scope['getMailAddressFromML'](project_path=str(self.a))
        self.assertFalse(self.scope['addUsersMailingList'](['artist_a'], project_path=str(self.a)))
        self.assertEqual((self.b / '.proj_ml').read_bytes(), other)
    def test_recipient_snapshot_reuses_user_schema_and_preserves_first_address_order(self):
        users = dict(self.users)
        users['duplicate'] = {'id': 'duplicate', 'name': 'Duplicate', 'mail': self.users['artist_a']['mail']}
        self.assertEqual(user_data.mailing_addresses(['artist_b', 'artist_a', 'duplicate', 'stale'], users),
                         ['artist_b@example.invalid', 'artist_a@example.invalid'])
        for ids in (['artist_a', 'artist_a'], [True], ['x'] * 10001):
            with self.assertRaises((TypeError, ValueError)): user_data.mailing_addresses(ids, users)
        users['artist_a'] = dict(users['artist_a'], mail='bad\nmail')
        with self.assertRaises(ValueError): user_data.mailing_addresses(['artist_a'], users)
    def test_mail_window_keeps_explicit_or_legacy_open_context_after_global_root_changes(self):
        pm = NS(window=Mock(return_value=NS(show=Mock())), formLayout=Mock(return_value='form'),
                columnLayout=Mock(return_value='column'), button=Mock(), setParent=Mock(), warning=Mock())
        window_api = NS(delete_window=Mock())
        api = NS(getAssetCurrentProjectPath=self.scope['getAssetCurrentProjectPath'],
                 getMaillingList=self.scope['getMaillingList'])
        scope = methods('python/DPyTools/integrations/smtp/mail.py', 'DMailingListManager', {
            'pm': pm, 'cmds': NS(scriptTable=Mock(return_value='table')), 'ui_windows': window_api,
            'asset_paths': asset_paths, 'dasset': api, 'user_data': user_data,
            'dusers': NS(getUserList=Mock(side_effect=lambda: dict(self.users)))},
            {'openWin', 'loadMailListData'})
        for explicit, expected in ((str(self.a), 'artist_a'), (None, 'artist_b')):
            self.prefs['root'] = str(self.b.parent)
            owner = NS(m_uiname='mail-window', prepareCB=Mock(), getCellCB=Mock(), m_list=[], _project_path='')
            owner.openWin = MethodType(scope['openWin'], owner)
            owner.loadMailListData = MethodType(scope['loadMailListData'], owner)
            self.assertTrue(owner.openWin(project_path=explicit))
            self.prefs['root'] = str(self.a.parent)
            owner.loadMailListData()
            self.assertEqual(owner.m_list, [expected])
            self.assertEqual(owner._table_rows, user_data.mailing_table_rows([expected], self.users))
    def test_empty_mail_open_preserves_existing_context_and_does_not_remove_its_window(self):
        window_api = NS(delete_window=Mock())
        scope = methods('python/DPyTools/integrations/smtp/mail.py', 'DMailingListManager', {
            'pm': NS(warning=Mock()), 'asset_paths': asset_paths, 'ui_windows': window_api}, {'openWin'})
        owner = NS(_project_path=str(self.a))
        self.assertFalse(scope['openWin'](owner, project_path=''))
        self.assertEqual(owner._project_path, str(self.a))
        window_api.delete_window.assert_not_called()


class OwningViewContextTests(BrowserFixture):
    def test_manager_keeps_its_root_after_another_window_changes_shared_settings(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.gui.models['categories']['selected'] = ['PROPS']
        self.gui.models['assets']['selected'] = ['prop']
        self.api.getAssetProjectRootPath.return_value = '/other-root'
        self.prefs['current'] = 'B'
        self.assertEqual(owner.getProjectPath(), '/root/A')
        self.assertTrue(owner.refleshAssetListCB())
        self.api.getAssetList.assert_called_once_with(assetType='PROPS', project_path='/root/A')
        self.api.getAssetProjectRootPath.assert_not_called()
    def test_explicit_root_reload_clears_same_project_label_from_another_root(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.api.getAssetProjectRootPath.return_value = '/other-root'
        self.assertTrue(owner.refleshProjectCB())
        self.assertEqual(owner._project_root, '/other-root')
        self.assertEqual(owner.getProjectPath(), '')
        self.assertEqual(self.prefs['current'], '')
        self.assertEqual(self.gui.models['assets']['items'], [])
    def test_partial_list_replacement_invalidates_root_instead_of_reusing_an_old_context(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        native = self.gui.pm.textScrollList.side_effect
        def dispatch(control, **kw):
            if 'append' in kw: raise RuntimeError('partial replacement')
            return native(control, **kw)
        self.gui.pm.textScrollList.side_effect = dispatch
        self.assertFalse(owner.refleshProjectCB())
        self.assertIsNone(owner._project_root)
        self.assertEqual(owner.getProjectPath(), '')
    def test_project_widget_refuses_ui_choice_that_was_not_committed_to_its_context(self):
        owner = self.widget('DProjectTextScroll', 'projects')
        owner.m_current_project = 'A'
        self.gui.models['projects']['selected'] = ['B']
        self.assertEqual(owner.getProjectPath(), '')
        self.gui.models['projects']['selected'] = ['A']
        self.assertEqual(owner.getProjectPath(), '/root/A')
    def test_category_and_asset_widgets_read_the_owning_provider_not_a_global_project(self):
        category = self.widget('DAssetTypeTextScroll', 'categories')
        category.project_path_getter = lambda: '/root/A'
        self.prefs['current'] = 'B'
        self.assertTrue(category.refleshCB())
        self.api.getAssetTypeListInProject.assert_called_once_with(project_path='/root/A')
        asset = self.widget('DAssetTextScroll', 'assets')
        asset.project_path_getter = lambda: '/root/A'
        asset._loaded_asset_context = ('/root/A', 'PROPS')
        self.gui.models['assets']['selected'] = ['same']
        self.assertTrue(asset.refleshCB())
        self.assertEqual(asset.m_asset, 'same')
        asset.project_path_getter = lambda: '/root/B'
        self.assertTrue(asset.refleshCB())
        self.assertEqual(asset.m_asset, '')
        self.assertEqual(self.api.getAssetList.call_args.kwargs['project_path'], '/root/B')
    def test_invalid_provider_is_rejected_before_view_mutation(self):
        owner = self.widget('DAssetTypeTextScroll', 'categories')
        for provider in (False, lambda: False):
            owner.project_path_getter = provider
            self.assertFalse(owner.refleshCB())
        self.gui.pm.textScrollList.assert_called()
        self.assertFalse(any(call.kwargs.get('edit') for call in self.gui.pm.textScrollList.call_args_list))


if __name__ == '__main__': unittest.main()
