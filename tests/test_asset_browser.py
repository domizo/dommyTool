#!/usr/bin/env python3
"""Actual browser helpers, list callbacks and project-specific path adapters."""

import ast
import contextlib
import copy
import functools
import os
import tempfile
import unittest
from pathlib import Path
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _tree
from DPyTools.core import strings, paths as core_paths
from DPyTools.pipeline.assets import paths as asset_paths, metadata, versioning, creation
from DPyTools.ui import table_selection


def methods(relative, class_name, namespace, names):
    tree = _tree(relative)
    cls = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == class_name)
    body = [node for node in cls.body if isinstance(node, ast.FunctionDef) and node.name in names]
    module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
    exec(compile(module, 'production-browser-methods', 'exec'), namespace)
    return namespace


class ListUI:
    def __init__(self):
        self.models = {name: {'items': [], 'selected': None} for name in ('projects', 'categories', 'assets')}
        self.fields = {'project-filter': '', 'asset-filter': '', 'filename': 'old.ma', 'root': ''}
        self.callbacks = {}
        self.pm = NS(textScrollList=Mock(side_effect=self.scroll), textField=Mock(side_effect=self.field),
                     text=Mock(side_effect=self.field), warning=Mock(), optionMenu=Mock(return_value='.ma'),
                     fileDialog2=Mock(return_value=None), promptDialog=Mock(return_value='Cancel'),
                     ls=Mock(return_value=None))
        self.browser_scope = functions('python/DPyTools/pipeline/assets/browser.py', {
            'pm': self.pm, 'functools': functools, 'contextmanager': contextlib.contextmanager,
            'strings': strings, 'paths': asset_paths, 'table_selection': table_selection})
        self.browser = NS(**self.browser_scope)
    def scroll(self, control, **kw):
        model = self.models[control]
        if kw.get('query') or kw.get('q'): return model['selected']
        if kw.get('removeAll'): model.update(items=[], selected=None)
        if 'append' in kw: model['items'].extend(kw['append'])
        if 'selectItem' in kw:
            if kw['selectItem'] not in model['items']: raise RuntimeError('stale selection')
            model['selected'] = [kw['selectItem']]
        if control in self.callbacks: self.callbacks[control]()
    def field(self, control, **kw):
        if kw.get('query') or kw.get('q'): return self.fields[control]
        self.fields[control] = kw.get('label', kw.get('text', ''))


class TextModelTests(unittest.TestCase):
    def test_empty_and_single_unicode_selections_are_normalized(self):
        for value in (None, [], (), ''): self.assertEqual(table_selection.selected_text(value), '')
        for value in ('人物', ['人物'], ('人物',)): self.assertEqual(table_selection.selected_text(value), '人物')
    def test_malformed_multiple_or_nontext_selections_fail(self):
        for value in ([1], ['a', 'b'], True, {'id': 'a'}):
            with self.assertRaises(TypeError): table_selection.selected_text(value)
    def test_text_models_reject_duplicates_nontext_and_overlimit(self):
        for value in ('a', ['a', 'a'], [True], [''], ['bad\x00text'], ['a'] * 10001):
            with self.assertRaises(ValueError): table_selection.text_items(value)
        self.assertEqual(table_selection.text_items(None), ())
    def test_only_visible_selections_survive_refresh(self):
        self.assertEqual(table_selection.list_selection(['old'], ['new']), (('new',), ''))
        self.assertEqual(table_selection.list_selection(['old'], ['old', 'new']), (('old', 'new'), 'old'))


class BrowserHelperTests(unittest.TestCase):
    def setUp(self): self.gui = ListUI()
    def test_filter_and_component_admission_precede_view_mutation(self):
        self.assertEqual(self.gui.browser.list_plan('A', ['A', 'B'], 'B'), (('B',), ''))
        for value in ('../escape', 'bad\x00name'):
            with self.assertRaises(ValueError): self.gui.browser.list_plan('', [value])
        self.gui.pm.textScrollList.assert_not_called()
    def test_guard_resets_after_update_failure_and_interruption(self):
        owner = NS()
        for error in (RuntimeError('failed'), KeyboardInterrupt()):
            with self.assertRaises(type(error)):
                with self.gui.browser.updating(owner): raise error
            self.assertFalse(owner._updating_browser)
    def test_action_preserves_interruptions_and_reports_expected_failures(self):
        @self.gui.browser.action
        def fail(owner, error): raise error
        self.assertFalse(fail(NS(), ValueError('bad input')))
        with self.assertRaises(KeyboardInterrupt): fail(NS(), KeyboardInterrupt())
        self.gui.pm.warning.assert_called_once()
    def test_bad_model_does_not_clear_an_existing_view(self):
        self.gui.models['projects']['items'] = ['old']
        with self.assertRaises(ValueError): self.gui.browser.replace_items(NS(), 'projects', ['bad/name'])
        self.assertEqual(self.gui.models['projects']['items'], ['old'])


class BrowserFixture(unittest.TestCase):
    def setUp(self):
        self.gui = ListUI()
        self.prefs = {'current': 'old'}
        self.projects = ['A', 'B']
        self.categories = {'A': ['PROPS', 'CH'], 'B': ['PROPS']}
        self.assets = {'PROPS': ['same', 'prop'], 'CH': ['same', 'character']}
        self.save = Mock(side_effect=self.persist)
        self.delegate = Mock(return_value=True)
        self.api = NS(current_project_key='current', root_path_key='root', setAssetUserDataDic=self.save,
                      getAssetProjectRootPath=Mock(return_value='/root'),
                      getAssetTypeProjectList=Mock(side_effect=lambda **kw: self.projects),
                      getAssetTypeListInProject=Mock(side_effect=lambda project_name=None, project_path=None: self.categories.get(
                          os.path.basename(project_path) if project_path else project_name or self.prefs['current'], [])),
                      getAssetList=Mock(side_effect=lambda assetType, **kw: self.assets.get(assetType, [])),
                      generateAssetFileName=Mock(return_value='prop_v001.ma'), createNewAssetFolder=Mock(return_value=True),
                      DMakeAssetClass=Mock(return_value=NS(openWin=Mock())))
    def persist(self, userDic):
        self.prefs.update(userDic)
        return True
    def bind(self, relative, class_name, owner, names, namespace):
        scope = methods(relative, class_name, dict(namespace, browser=self.gui.browser, pm=self.gui.pm), names)
        for name in names: setattr(owner, name, MethodType(scope[name], owner))
        return owner
    def manager(self):
        owner = NS(projTextScroll='projects', classTextScroll='categories', assetTextScroll='assets',
                   projectFilterText='project-filter', assetFilterText='asset-filter', exportNameText='filename',
                   fileTypeMenu='type', rootText='root', m_uiname='manager', mainWin='window',
                   _loaded_asset_context=('/root', 'A', 'PROPS'), _project_root='/root')
        names = {'changeProjectSelectCB', 'refleshProjectCB', 'refleshAssetTypeListCB', 'refleshAssetListCB',
                 'refleshExportFileNameTextCB', 'clearExportFiletextFieldCB', 'getCategory', 'makeAssetCB',
                 'opemMailingManagerCB', 'setRootNodeCB', 'changeRootCB', 'getProjectPath'}
        return self.bind('python/DPyTools/pipeline/assets/manager.py', 'DAssetManager', owner, names, {
            'dasset': self.api, 'dmail': NS(DMailingListManager=Mock()), 'asset_paths': asset_paths,
            'asset_metadata': metadata, 'table_selection': table_selection,
            'core_paths': core_paths, 'creation': creation,
            'parameters': NS(node_label=lambda node: node.name())})
    def widget(self, class_name, control):
        owner = NS(m_scroll_text=control, m_filter='project-filter' if control == 'projects' else 'asset-filter',
                   m_root_path='/root', m_current_project='old', m_asset_type='PROPS',
                   m_asset='old-asset', _loaded_asset_type='PROPS',
                   _loaded_asset_context=(None, 'PROPS'), project_path_getter=None,
                   delegate_changeProj_fnc=self.delegate, delegate_changeSelect_fnc=self.delegate)
        names = {'changeSelectCB', 'refleshCB'}
        if class_name == 'DProjectTextScroll': names.update({'isSelectProject', 'changeRootCB', 'getProjectPath'})
        elif class_name == 'DAssetTypeTextScroll': names.add('getAssetType')
        else: names.add('clearListCB')
        return self.bind('python/DPyTools/pipeline/assets/operations.py', class_name, owner, names, {
            'setAssetUserDataDic': self.save, 'current_project_key': 'current', 'root_path_key': 'root',
            'getAssetProjectRootPath': lambda: '/root', 'getAssetTypeProjectList': self.api.getAssetTypeProjectList,
            'getAssetTypeListInProject': self.api.getAssetTypeListInProject, 'getAssetList': self.api.getAssetList,
            'asset_paths': asset_paths, 'table_selection': table_selection})


class ManagerBrowserTests(BrowserFixture):
    def test_empty_project_selection_clears_settings_categories_assets_and_filename(self):
        owner = self.manager()
        self.gui.models['categories']['items'] = ['stale']
        self.gui.models['assets']['items'] = ['stale']
        self.assertTrue(owner.changeProjectSelectCB())
        self.assertEqual(self.prefs['current'], '')
        self.assertEqual(self.gui.models['categories']['items'], [])
        self.assertEqual(self.gui.fields['filename'], '')
    def test_refresh_filters_removed_selection_without_selecting_stale_text(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.gui.fields['project-filter'] = 'B'
        self.assertTrue(owner.refleshProjectCB())
        self.assertEqual(self.gui.models['projects']['items'], ['B'])
        self.assertEqual(self.prefs['current'], '')
    def test_visible_project_is_preserved_and_categories_are_loaded_once_after_update(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.gui.callbacks['projects'] = owner.changeProjectSelectCB
        self.assertTrue(owner.refleshProjectCB())
        self.assertEqual(self.prefs['current'], 'A')
        self.assertEqual(self.save.call_count, 1)
        self.assertEqual(self.gui.models['categories']['items'], ['PROPS', 'CH'])
    def test_malformed_selection_model_or_fetch_failure_does_not_mutate_view(self):
        owner = self.manager()
        before = copy.deepcopy(self.gui.models)
        for value in (['A', 'B'], [True], ['../escape']):
            self.gui.models['projects']['selected'] = value
            self.assertFalse(owner.changeProjectSelectCB())
        self.save.assert_not_called()
        self.gui.models = before
        self.api.getAssetTypeProjectList.side_effect = OSError('list failed')
        self.assertFalse(owner.refleshProjectCB())
        self.assertEqual(self.gui.models, before)
    def test_settings_failure_does_not_clear_dependent_lists(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        before = copy.deepcopy(self.gui.models)
        self.save.side_effect = None
        self.save.return_value = False
        self.assertFalse(owner.changeProjectSelectCB())
        self.assertEqual(self.gui.models, before)
    def test_same_label_in_a_new_category_is_not_retained_as_the_old_asset(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.gui.models['categories']['selected'] = ['CH']
        self.gui.models['assets']['selected'] = ['same']
        self.assertTrue(owner.refleshAssetListCB())
        self.assertIsNone(self.gui.models['assets']['selected'])
        self.assertEqual(self.gui.fields['filename'], '')
        self.api.getAssetList.assert_called_once_with(assetType='CH', project_path='/root/A')
    def test_same_context_retains_visible_asset_and_uses_explicit_project_for_filename(self):
        owner = self.manager()
        self.gui.models['projects']['selected'] = ['A']
        self.gui.models['categories']['selected'] = ['PROPS']
        self.gui.models['assets']['selected'] = ['prop']
        self.assertTrue(owner.refleshAssetListCB())
        self.assertEqual(self.gui.models['assets']['selected'], ['prop'])
        self.api.generateAssetFileName.assert_called_once_with(assetType='PROPS', assetName='prop', fileType='.ma', project_path='/root/A')
    def test_no_selection_menu_creation_and_root_actions_do_not_dispatch(self):
        owner = self.manager()
        self.assertFalse(owner.makeAssetCB())
        self.assertFalse(owner.opemMailingManagerCB())
        self.assertFalse(owner.setRootNodeCB())
        self.assertEqual(owner.getCategory(), '')
        self.gui.pm.promptDialog.assert_not_called()
        self.save.assert_not_called()
    def test_native_update_failure_resets_the_reentry_guard(self):
        owner = self.manager()
        native = self.gui.pm.textScrollList.side_effect
        def dispatch(control, **kw):
            if 'append' in kw: raise RuntimeError('append failed')
            return native(control, **kw)
        self.gui.pm.textScrollList.side_effect = dispatch
        self.assertFalse(owner.refleshProjectCB())
        self.assertFalse(owner._updating_browser)


class ScrollWidgetTests(BrowserFixture):
    def test_empty_project_choice_clears_cached_and_persisted_context_and_notifies(self):
        owner = self.widget('DProjectTextScroll', 'projects')
        self.assertTrue(owner.changeSelectCB())
        self.assertEqual((owner.m_current_project, self.prefs['current']), ('', ''))
        self.assertFalse(owner.isSelectProject())
        self.delegate.assert_called_once()
    def test_filtered_project_refresh_retains_only_visible_selection(self):
        owner = self.widget('DProjectTextScroll', 'projects')
        self.gui.models['projects']['selected'] = ['A']
        self.gui.fields['project-filter'] = 'B'
        self.assertTrue(owner.refleshCB())
        self.assertEqual(owner.m_current_project, '')
        self.assertEqual(owner.m_root_path, '/root')
    def test_project_update_does_not_persist_transient_empty_native_selection(self):
        owner = self.widget('DProjectTextScroll', 'projects')
        self.gui.models['projects']['selected'] = ['A']
        self.gui.callbacks['projects'] = owner.changeSelectCB
        self.assertTrue(owner.refleshCB())
        self.assertEqual(self.save.call_count, 1)
        self.delegate.assert_called_once()
    def test_category_none_and_project_change_clear_selection(self):
        owner = self.widget('DAssetTypeTextScroll', 'categories')
        self.assertEqual(owner.getAssetType(), '')
        self.prefs['current'] = 'A'
        self.gui.models['categories']['selected'] = ['PROPS']
        self.assertTrue(owner.refleshCB(preserve_selection=False))
        self.assertEqual(owner.getAssetType(), '')
    def test_asset_none_and_context_changes_clear_cached_names(self):
        owner = self.widget('DAssetTextScroll', 'assets')
        self.assertTrue(owner.changeSelectCB())
        self.assertEqual(owner.m_asset, '')
        self.gui.models['assets']['selected'] = ['same']
        owner.m_asset_type = 'CH'
        self.assertTrue(owner.refleshCB())
        self.assertEqual(owner.m_asset, '')
        self.assertTrue(owner.clearListCB())
        self.assertEqual((owner.m_asset_type, owner.m_asset), ('', ''))
    def test_bad_selection_or_failed_persistence_keeps_previous_cached_context(self):
        owner = self.widget('DProjectTextScroll', 'projects')
        self.gui.models['projects']['selected'] = ['A', 'B']
        self.assertFalse(owner.changeSelectCB())
        self.assertEqual(owner.m_current_project, 'old')
        self.gui.models['projects']['selected'] = ['A']
        self.save.side_effect = None
        self.save.return_value = False
        self.assertFalse(owner.changeSelectCB())
        self.assertEqual(owner.m_current_project, 'old')


class SceneCascadeTests(BrowserFixture):
    def test_empty_project_cascade_clears_asset_and_version_models(self):
        category = self.widget('DAssetTypeTextScroll', 'categories')
        asset = self.widget('DAssetTextScroll', 'assets')
        owner = NS(assetTypeScroll=category, assetScroll=asset, m_file_data=['old'],
                   fileTable='table', sceneThumbView='image', commentText='comment')
        self.gui.pm.scriptTable = Mock()
        self.gui.pm.image = Mock()
        names = {'changeProjectCB', 'changeAssetTypeCB', 'changeAssetCB', 'clearFileTableCB'}
        owner = self.bind('python/DPyTools/pipeline/assets/scene_builder.py', 'DSceneBuilder', owner, names, {
            'table_selection': table_selection, '_LOGGER': NS(debug=lambda *a: None)})
        category.delegate_changeSelect_fnc = owner.changeAssetTypeCB
        asset.delegate_changeSelect_fnc = owner.changeAssetCB
        self.assertTrue(owner.changeProjectCB())
        self.assertEqual((asset.m_asset_type, asset.m_asset), ('', ''))
        self.assertEqual(owner.m_file_data, [])
    def test_failed_category_query_does_not_overwrite_a_valid_asset_context(self):
        asset = self.widget('DAssetTextScroll', 'assets')
        owner = NS(assetScroll=asset, assetTypeScroll=NS(getAssetType=Mock(return_value=False)))
        owner = self.bind('python/DPyTools/pipeline/assets/scene_builder.py', 'DSceneBuilder', owner,
                          {'changeAssetTypeCB'}, {'table_selection': table_selection})
        self.assertFalse(owner.changeAssetTypeCB())
        self.assertEqual(asset.m_asset_type, 'PROPS')


class ProjectPathAdapterTests(unittest.TestCase):
    def test_explicit_project_reads_do_not_modify_or_use_a_different_cached_project(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for project in ('A', 'B'):
                (root / project / '3d' / 'assets' / 'PROPS' / 'prop' / 'v001').mkdir(parents=True)
            prefs = {'root': str(root), 'current': 'B'}
            scope = functions('python/DPyTools/pipeline/assets/operations.py', {
                'getAssetUserDataDic': lambda **kw: dict(prefs), 'root_path_key': 'root', 'current_project_key': 'current',
                'asset_paths': asset_paths, 'versioning': versioning, 'pm': NS(warning=Mock()),
                'dpath': NS(isDir=os.path.isdir, append=lambda *a, **kw: os.path.join(*a) if a else os.path.join(kw['path'], kw['other'])),
                '_LOGGER': NS(debug=lambda *a: None)})
            # Retain the read-only injected preference source, not the legacy file reader body.
            scope['getAssetUserDataDic'] = lambda **kw: dict(prefs)
            self.assertEqual(scope['getAssetCurrentProjectPath'](), str(root / 'B'))
            self.assertEqual(scope['getAssetCurrentProjectPath']('A'), str(root / 'A'))
            self.assertEqual(scope['getAssetList']('PROPS', project_name='A'), ['prop'])
            self.assertEqual(scope['getExportPath']('PROPS', 'prop', project_name='A'), str(root / 'A' / '3d' / 'assets' / 'PROPS' / 'prop' / 'v002'))
            self.assertEqual(prefs['current'], 'B')


if __name__ == '__main__': unittest.main()
