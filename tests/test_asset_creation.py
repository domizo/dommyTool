#!/usr/bin/env python3
"""Real disposable directory plans and converted creation callback failures."""

import contextlib
import functools
import os
import tempfile
import unittest
from pathlib import Path
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock, patch

from test_python_rig_safety import functions
from test_asset_browser import methods
from DPyTools.core import strings
from DPyTools.pipeline.assets import creation, paths as asset_paths
from DPyTools.ui import table_selection


class DirectoryPlanTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommy-creation-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
    def test_complete_project_layout_is_created_in_parent_order(self):
        plan = creation.project_plan(str(self.root), 'project')
        self.assertEqual(creation.execute(plan), plan.targets)
        self.assertEqual(len(plan.targets), 5)
        self.assertTrue(all(Path(path).is_dir() for path in plan.targets))
    def test_existing_project_category_and_asset_are_never_reused(self):
        (self.root / 'project').mkdir()
        (self.root / 'PROPS').mkdir()
        (self.root / 'PROPS' / 'prop').mkdir()
        for make in (lambda: creation.project_plan(str(self.root), 'project'),
                     lambda: creation.category_plan(str(self.root), 'PROPS'),
                     lambda: creation.asset_plan(str(self.root), 'PROPS', 'prop')):
            with self.assertRaises(ValueError): make()
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), ['PROPS', 'project'])
    def test_asset_creation_preserves_a_borrowed_category_and_can_create_a_missing_one(self):
        category = self.root / 'PROPS'
        category.mkdir()
        marker = category / 'borrowed.txt'
        marker.write_text('keep', encoding='utf-8')
        plan = creation.asset_plan(str(self.root), 'PROPS', 'prop')
        self.assertEqual(len(plan.targets), 1)
        creation.execute(plan)
        self.assertEqual(marker.read_text(encoding='utf-8'), 'keep')
        plan = creation.asset_plan(str(self.root), 'CH', 'hero')
        self.assertEqual(len(plan.targets), 2)
        creation.execute(plan)
    def test_malformed_names_and_roots_fail_before_creation(self):
        for name in ('', '..', '../outside', 'a/b', 'bad\x00name', True, ['name']):
            with self.assertRaises((TypeError, ValueError)):
                creation.project_plan(str(self.root), name)
        for root in (None, '', 'relative', 'bad\x00path'):
            with self.assertRaises((TypeError, ValueError)):
                creation.snapshot_root(root)
        self.assertEqual(list(self.root.iterdir()), [])
    def test_new_project_category_asset_and_content_names_require_portable_leaves(self):
        for name in ('CON', 'name.', 'bad:name', 'a?b'):
            for make in (lambda: creation.project_plan(str(self.root), name),
                         lambda: creation.project_plan(str(self.root), 'project', content_name=name),
                         lambda: creation.category_plan(str(self.root), name),
                         lambda: creation.asset_plan(str(self.root), 'PROPS', name),
                         lambda: creation.asset_plan(str(self.root), name, 'asset')):
                with self.assertRaises(ValueError): make()
        self.assertEqual(list(self.root.iterdir()), [])
    def test_forged_nonportable_target_is_rejected_before_any_directory_creation(self):
        root = creation.snapshot_root(str(self.root))
        plan = creation.CreationPlan(root, (str(self.root / 'good'), str(self.root / 'bad:name')))
        with self.assertRaises(creation.CreationError) as caught: creation.execute(plan)
        self.assertEqual(caught.exception.created, ())
        self.assertEqual(list(self.root.iterdir()), [])
    def test_borrowed_posix_legacy_category_is_not_renamed_or_rejected_for_reading(self):
        if os.name == 'nt': self.skipTest('Legacy colon leaf is a POSIX fixture, not native Windows validation')
        category = self.root / 'legacy:category'
        category.mkdir()
        plan = creation.asset_plan(str(self.root), category.name, 'new-asset')
        self.assertEqual(len(plan.targets), 1)
        creation.execute(plan)
        self.assertTrue((category / 'new-asset').is_dir())
    def test_entire_forged_plan_is_admitted_before_creating_a_prefix(self):
        root = creation.snapshot_root(str(self.root))
        good = str(self.root / 'good')
        bad_plans = (creation.CreationPlan(root, (good, str(self.root.parent / 'outside'))),
                     creation.CreationPlan(root, (good, good)),
                     creation.CreationPlan(root, (str(self.root / 'a' / 'b' / 'c' / 'd'),)),
                     creation.CreationPlan(root, tuple(str(self.root / str(i)) for i in range(9))),
                     creation.CreationPlan(root, (str(self.root / 'missing' / 'child'),)),
                     creation.CreationPlan(root, ()))
        for plan in bad_plans:
            with self.assertRaises(creation.CreationError) as caught:
                creation.execute(plan)
            self.assertEqual(caught.exception.created, ())
        self.assertEqual(list(self.root.iterdir()), [])
    def test_root_replacement_after_snapshot_stops_without_creating(self):
        root = self.root / 'context'
        root.mkdir()
        plan = creation.category_plan(creation.snapshot_root(str(root)), 'PROPS')
        root.rename(self.root / 'old-context')
        root.mkdir()
        with self.assertRaises(creation.CreationError) as caught: creation.execute(plan)
        self.assertEqual(caught.exception.created, ())
        self.assertEqual(list(root.iterdir()), [])
    def test_symlinked_borrowed_parent_and_non_directory_context_are_rejected(self):
        real = self.root / 'real'
        real.mkdir()
        (self.root / 'PROPS').symlink_to(real, target_is_directory=True)
        with self.assertRaises(ValueError): creation.asset_plan(str(self.root), 'PROPS', 'prop')
        file = self.root / 'file'
        file.write_text('keep', encoding='utf-8')
        with self.assertRaises(ValueError): creation.snapshot_root(str(file))
        self.assertEqual(list(real.iterdir()), [])
    def test_late_mkdir_failure_reports_confirmed_prefix_without_deleting_it(self):
        plan = creation.project_plan(str(self.root), 'project')
        native = creation.os.mkdir
        def mkdir(path):
            if path == plan.targets[2]: raise OSError('injected failure')
            return native(path)
        with patch.object(creation.os, 'mkdir', side_effect=mkdir):
            with self.assertRaises(creation.CreationError) as caught: creation.execute(plan)
        self.assertEqual(caught.exception.created, plan.targets[:2])
        self.assertTrue(all(Path(path).is_dir() for path in plan.targets[:2]))
        self.assertFalse(Path(plan.targets[2]).exists())
    def test_target_appearing_during_execution_is_not_accepted_as_owned(self):
        plan = creation.project_plan(str(self.root), 'project')
        native = creation.os.mkdir
        def mkdir(path):
            native(path)
            if path == plan.targets[0]: native(plan.targets[1])
        with patch.object(creation.os, 'mkdir', side_effect=mkdir):
            with self.assertRaises(creation.CreationError) as caught: creation.execute(plan)
        self.assertEqual(caught.exception.created, plan.targets[:1])
        self.assertTrue(Path(plan.targets[1]).is_dir())
        self.assertFalse(Path(plan.targets[2]).exists())
    def test_interruption_propagates_and_does_not_delete_confirmed_directories(self):
        plan = creation.project_plan(str(self.root), 'project')
        native = creation.os.mkdir
        def mkdir(path):
            if path == plan.targets[1]: raise KeyboardInterrupt()
            native(path)
        with patch.object(creation.os, 'mkdir', side_effect=mkdir):
            with self.assertRaises(KeyboardInterrupt): creation.execute(plan)
        self.assertTrue(Path(plan.targets[0]).is_dir())


class CreationUIFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommy-creation-callback-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.a, self.b = self.root / 'A', self.root / 'B'
        self.a.mkdir()
        self.b.mkdir()
        self.pm = NS(warning=Mock(), textField=Mock(return_value='project'), optionMenu=Mock(return_value='PROPS'),
                     fileDialog2=Mock(return_value=None), text=Mock(return_value='root-label'),
                     deleteUI=Mock(), confirmDialog=Mock(), promptDialog=Mock(return_value='Cancel'))
        for name in ('formLayout', 'columnLayout', 'rowLayout', 'button', 'setParent', 'setUITemplate', 'menuItem'):
            setattr(self.pm, name, Mock(return_value=name))
        self.pm.uiTemplate = Mock(return_value=NS(define=Mock()))
        self.pm.window = Mock(return_value=NS(show=Mock()))
        browser_scope = functions('python/DPyTools/pipeline/assets/browser.py', {
            'pm': self.pm, 'functools': functools, 'contextmanager': contextlib.contextmanager,
            'strings': strings, 'paths': asset_paths, 'table_selection': table_selection})
        self.browser = NS(**browser_scope)
        self.scope = functions('python/DPyTools/pipeline/assets/operations.py', {
            'pm': self.pm, 'creation': creation, 'browser': self.browser, 'asset_paths': asset_paths,
            'table_selection': table_selection, 'vfxFolder': 'works', 'root_path_key': 'root',
            'current_project_key': 'current', 'rootProjectPath': str(self.a), 'classList': ['PROPS', 'CH'],
            '_LOGGER': NS(debug=lambda *a: None),
            'ui_windows': NS(delete_window=Mock())})
        self.scope['getAssetFolderPath'] = Mock(return_value=str(self.a))
        self.scope['getAssetProjectRootPath'] = Mock(return_value=str(self.a))
        self.scope['setAssetUserDataDic'] = Mock(return_value=True)
    def owner(self, class_name, names):
        owner = NS(m_uiname='dialog', refWin=None, func_finalAct=None, _creation_root=creation.snapshot_root(str(self.a)),
                   projRootPathText='root-label', projNameText='name', assetTypeMenu='category')
        scope = methods('python/DPyTools/pipeline/assets/operations.py', class_name, dict(self.scope), names)
        for name in names: setattr(owner, name, MethodType(scope[name], owner))
        return owner


class CreationCallbackTests(CreationUIFixture):
    def test_success_dialog_is_only_shown_after_the_complete_project_layout(self):
        def confirm(**kw):
            self.assertTrue(all((self.a / 'project' / 'works' / name).is_dir()
                                for name in ('assets', 'scenes', 'sourceimages')))
        self.pm.confirmDialog.side_effect = confirm
        self.assertTrue(self.scope['createProjectFolderStructure'](str(self.a), 'project'))
        self.pm.confirmDialog.assert_called_once()
    def test_partial_project_failure_never_shows_success_or_closes_the_dialog(self):
        owner = self.owner('DMakeProject', {'createProjectCB'})
        native = creation.os.mkdir
        def mkdir(path):
            if path.endswith(os.path.join('works', 'assets')): raise OSError('denied')
            native(path)
        with patch.object(creation.os, 'mkdir', side_effect=mkdir): self.assertFalse(owner.createProjectCB())
        self.pm.confirmDialog.assert_not_called()
        self.pm.deleteUI.assert_not_called()
        self.assertTrue(any('2 confirmed directories' in str(call) for call in self.pm.warning.call_args_list))
    def test_root_save_failure_does_not_change_display_or_retained_context(self):
        owner = self.owner('DMakeProject', {'changeRootProjectPathCB'})
        before = owner._creation_root
        self.pm.fileDialog2.return_value = [str(self.b)]
        self.scope['setAssetUserDataDic'].return_value = False
        self.assertFalse(owner.changeRootProjectPathCB())
        self.assertEqual(owner._creation_root, before)
        self.pm.text.assert_not_called()
    def test_root_change_commits_before_view_update_and_clears_old_project(self):
        owner = self.owner('DMakeProject', {'changeRootProjectPathCB'})
        self.pm.fileDialog2.return_value = [str(self.b)]
        self.assertTrue(owner.changeRootProjectPathCB())
        self.scope['setAssetUserDataDic'].assert_called_once_with(userDic={'root': str(self.b), 'current': ''})
        self.assertEqual(owner._creation_root.path, str(self.b))
        self.pm.text.assert_called_once_with('root-label', edit=True, label=str(self.b))
    def test_cancel_or_malformed_root_choice_has_no_save_or_display_effect(self):
        owner = self.owner('DMakeProject', {'changeRootProjectPathCB'})
        for value in (None, [], ['a', 'b'], [True], ['bad\x00path']):
            self.pm.fileDialog2.return_value = value
            self.assertFalse(owner.changeRootProjectPathCB())
        self.scope['setAssetUserDataDic'].assert_not_called()
        self.pm.text.assert_not_called()
    def test_project_dialog_uses_root_retained_when_it_opened(self):
        owner = self.owner('DMakeProject', {'openWin', 'createProjectCB'})
        self.assertTrue(owner.openWin())
        self.scope['getAssetProjectRootPath'].return_value = str(self.b)
        self.assertTrue(owner.createProjectCB())
        self.assertTrue((self.a / 'project' / 'works' / 'assets').is_dir())
        self.assertFalse((self.b / 'project').exists())
    def test_category_dialog_uses_asset_root_retained_when_it_opened(self):
        owner = self.owner('DMakeAssetClass', {'openWin', 'createAssetCB'})
        self.assertTrue(owner.openWin())
        self.scope['getAssetFolderPath'].return_value = str(self.b)
        self.assertTrue(owner.createAssetCB())
        self.scope['getAssetFolderPath'].assert_called_once_with()
        self.assertTrue((self.a / 'PROPS').is_dir())
        self.assertFalse((self.b / 'PROPS').exists())
    def test_invalid_open_context_does_not_delete_or_create_native_windows(self):
        for class_name, keyword in (('DMakeProject', 'project_root'), ('DMakeAssetClass', 'asset_root')):
            owner = self.owner(class_name, {'openWin'})
            self.assertFalse(owner.openWin(**{keyword: str(self.root / 'missing')}))
        self.scope['ui_windows'].delete_window.assert_not_called()
        self.pm.window.assert_not_called()
    def test_unavailable_legacy_default_allows_root_selection_but_not_creation(self):
        owner = self.owner('DMakeProject', {'openWin', 'createProjectCB'})
        self.scope['getAssetProjectRootPath'].return_value = str(self.root / 'missing-default')
        self.assertTrue(owner.openWin())
        self.assertIsNone(owner._creation_root)
        self.assertFalse(owner.createProjectCB())
        self.pm.confirmDialog.assert_not_called()
        self.pm.deleteUI.assert_not_called()
    def test_existing_category_does_not_close_or_refresh_as_success(self):
        owner = self.owner('DMakeAssetClass', {'createAssetCB'})
        (self.a / 'PROPS').mkdir()
        owner.func_finalAct = Mock()
        self.assertFalse(owner.createAssetCB())
        owner.func_finalAct.assert_not_called()
        self.pm.deleteUI.assert_not_called()
    def test_failed_refresh_after_creation_reports_partial_completion(self):
        owner = self.owner('DMakeAssetClass', {'createAssetCB'})
        owner.refWin = NS(refleshAssetTypeListCB=Mock(return_value=False))
        self.assertFalse(owner.createAssetCB())
        self.assertTrue((self.a / 'PROPS').is_dir())
        self.pm.deleteUI.assert_not_called()
        self.pm.warning.assert_called()
    def test_explicit_asset_root_never_reads_a_new_global_destination(self):
        root = creation.snapshot_root(str(self.a))
        self.scope['getAssetFolderPath'].return_value = str(self.b)
        self.assertTrue(self.scope['createNewAssetFolder']('PROPS', 'prop', asset_root=root))
        self.scope['getAssetFolderPath'].assert_not_called()
        self.assertTrue((self.a / 'PROPS' / 'prop').is_dir())
        self.assertFalse((self.b / 'PROPS').exists())
    def test_replaced_dialog_root_and_interruption_do_not_claim_success(self):
        owner = self.owner('DMakeAssetClass', {'createAssetCB'})
        self.a.rename(self.root / 'old-A')
        self.a.mkdir()
        self.assertFalse(owner.createAssetCB())
        self.pm.deleteUI.assert_not_called()
        owner._creation_root = creation.snapshot_root(str(self.a))
        with patch.object(creation.os, 'mkdir', side_effect=KeyboardInterrupt()):
            with self.assertRaises(KeyboardInterrupt): owner.createAssetCB()


class AssetPromptTests(CreationUIFixture):
    def bind(self, class_name, names, namespace):
        owner = NS(projTextScroll='project', classTextScroll='category', m_asset_type='PROPS',
                   project_path_getter=lambda: str(self.a), m_parent_win='parent',
                   getProjectPath=lambda **kw: str(self.a), refleshAssetListCB=Mock(return_value=True),
                   refleshCB=Mock(return_value=True))
        scope = methods('python/DPyTools/pipeline/assets/' + ('manager.py' if class_name == 'DAssetManager' else 'operations.py'),
                        class_name, dict(self.scope, **namespace), names)
        for name in names: setattr(owner, name, MethodType(scope[name], owner))
        return owner
    def manager(self):
        api = NS(getAssetFolderPath=Mock(return_value=str(self.a)),
                 createNewAssetFolder=self.scope['createNewAssetFolder'])
        owner = self.bind('DAssetManager', {'makeAssetCB'}, {'dasset': api})
        self.pm.textScrollList = Mock(side_effect=lambda control, **kw: ['project'] if control == 'project' else ['PROPS'])
        return owner, api
    def test_manager_prompt_changes_cannot_retarget_a_retained_asset_root(self):
        owner, api = self.manager()
        def prompt(**kw):
            api.getAssetFolderPath.return_value = str(self.b)
            self.scope['getAssetFolderPath'].return_value = str(self.b)
            return 'prop' if kw.get('query') else 'Create'
        self.pm.promptDialog.side_effect = prompt
        self.assertTrue(owner.makeAssetCB())
        self.assertTrue((self.a / 'PROPS' / 'prop').is_dir())
        self.assertFalse((self.b / 'PROPS').exists())
        api.getAssetFolderPath.assert_called_once_with(project_path=str(self.a))
        self.scope['getAssetFolderPath'].assert_not_called()
    def test_common_widget_prompt_keeps_root_and_category_before_native_dispatch(self):
        owner = self.bind('DAssetTextScroll', {'makeAssetCB'}, {})
        def prompt(**kw):
            self.scope['getAssetFolderPath'].return_value = str(self.b)
            owner.m_asset_type = 'CH'
            owner.project_path_getter = lambda: str(self.b)
            return 'prop' if kw.get('query') else 'Create'
        self.pm.promptDialog.side_effect = prompt
        self.assertTrue(owner.makeAssetCB())
        self.assertTrue((self.a / 'PROPS' / 'prop').is_dir())
        self.assertFalse((self.b / 'CH').exists())
    def test_cancelled_prompt_does_not_create_or_refresh(self):
        owner, api = self.manager()
        self.assertFalse(owner.makeAssetCB())
        self.assertEqual(list(self.a.iterdir()), [])
        owner.refleshAssetListCB.assert_not_called()
    def test_invalid_or_occupied_asset_does_not_refresh_as_success(self):
        owner, api = self.manager()
        self.pm.promptDialog.side_effect = lambda **kw: '../outside' if kw.get('query') else 'Create'
        self.assertFalse(owner.makeAssetCB())
        self.assertEqual(list(self.a.iterdir()), [])
        self.pm.promptDialog.side_effect = lambda **kw: 'prop' if kw.get('query') else 'Create'
        (self.a / 'PROPS' / 'prop').mkdir(parents=True)
        self.assertFalse(owner.makeAssetCB())
        owner.refleshAssetListCB.assert_not_called()
    def test_root_replaced_during_prompt_stops_creation(self):
        owner, api = self.manager()
        def prompt(**kw):
            if not kw.get('query'):
                self.a.rename(self.root / 'old-A')
                self.a.mkdir()
                return 'Create'
            return 'prop'
        self.pm.promptDialog.side_effect = prompt
        self.assertFalse(owner.makeAssetCB())
        self.assertEqual(list(self.a.iterdir()), [])
        owner.refleshAssetListCB.assert_not_called()


if __name__ == '__main__': unittest.main()
