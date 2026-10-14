#!/usr/bin/env python3
"""Actual Scene Builder callbacks with disposable metadata and host stubs."""

import ast
import contextlib
import functools
import os
import sys
import tempfile
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
from DPyTools.pipeline.assets import metadata
from DPyTools.ui import table_selection
from DPyTools.core import strings
from DPyTools.pipeline.assets import paths as asset_paths
from test_python_rig_safety import functions


class SceneBuilderCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        filename = ROOT / 'python/DPyTools/pipeline/assets/scene_builder.py'
        converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
        tree = ast.parse(str(converter.refactor_string(
            filename.read_text(encoding='utf-8') + '\n', str(filename))))
        ui = next(node for node in tree.body
                  if isinstance(node, ast.ClassDef) and node.name == 'DSceneBuilder')
        names = {'_selected_version', 'runImportCB', 'getCellCB', 'changeSelectionCB'}
        body = [node for node in ui.body
                if isinstance(node, ast.FunctionDef) and node.name in names]
        module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
        cls.namespace = {'asset_metadata': metadata, 'table_selection': table_selection,
                         '_LOGGER': SimpleNamespace(debug=lambda *args: None)}
        cls.browser_scope = functions('python/DPyTools/pipeline/assets/browser.py', {
            'functools': functools, 'contextmanager': contextlib.contextmanager,
            'table_selection': table_selection, 'paths': asset_paths, 'strings': strings})
        cls.namespace['browser'] = SimpleNamespace(**cls.browser_scope)
        exec(compile(module, str(filename), 'exec'), cls.namespace)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommytool-scene-builder-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.records = [self.record(index) for index in range(1, 4)]
        self.row = 2
        self.mode = 'Import'
        self.namespace_value = ''
        self.warnings = []
        self.io = SimpleNamespace(importFile=Mock(), reference=Mock())
        self.pm = SimpleNamespace(
            scriptTable=Mock(side_effect=lambda *args, **kwargs: self.row),
            textScrollList=lambda *args, **kwargs: ['project'],
            optionMenu=lambda *args, **kwargs: self.mode,
            textFieldGrp=Mock(side_effect=lambda *args, **kwargs: self.namespace_value),
            checkBoxGrp=lambda *args, **kwargs: False,
            checkBox=lambda *args, **kwargs: True,
            image=Mock(), text=Mock(), ls=lambda **kwargs: ['node'],
            warning=self.warnings.append)
        self.commands = SimpleNamespace(AbcImport=Mock(), file=Mock())
        self.plugins = SimpleNamespace(isLoadedPlugin=Mock(return_value=False),
                                       loadMayaPlugin=Mock(return_value=True))
        self.animation = SimpleNamespace(d_animRead=Mock(return_value=True))
        self.asset = SimpleNamespace(getAssetPath=Mock(return_value=str(self.root)))
        self.namespace.update(pm=self.pm, om=SimpleNamespace(MFileIO=lambda: self.io),
                              cmds=self.commands, dsi=self.plugins, danimIO=self.animation,
                              dasset=self.asset, dpath=SimpleNamespace(isFile=os.path.isfile))
        self.browser_scope['pm'] = self.pm
        self.ui = SimpleNamespace(m_file_data=self.records, fileTable='table',
                                  projTextScroll=SimpleNamespace(m_scroll_text='project', getProjectPath=lambda: str(self.root)),
                                  assetScroll=SimpleNamespace(m_asset_type='PROPS', m_asset='asset'),
                                  importTypeMenu='mode', namespaceField='namespace',
                                  isIgnoreVerCheck='ignore', isDeferRefCheck='defer', isLockRefCheck='lock',
                                  targetNamespaceField='target', d_paths='paths', d_unKeyed='unkeyed',
                                  sceneThumbView='thumbnail', commentText='comment')
        if '_selected_version' in self.namespace:
            self.ui._selected_version = lambda: self.namespace['_selected_version'](self.ui)

    def record(self, index, extension='.ma'):
        version = 'v{0:03d}'.format(index)
        directory = self.root / version
        directory.mkdir(exist_ok=True)
        filename = 'asset_' + version + extension
        filepath = directory / filename
        filepath.write_text('diagnostic fixture only\n', encoding='utf-8')
        return {'version': version, 'filepath': str(filepath), 'filename': filename,
                'comment': 'comment ' + version, 'creator': 'artist',
                'extension': extension, 'thumbnail': '', 'date': '2019/01/01'}

    def run_import(self):
        return self.namespace['runImportCB'](self.ui)

    def assert_no_file_operation(self):
        self.io.importFile.assert_not_called()
        self.io.reference.assert_not_called()
        self.commands.AbcImport.assert_not_called()
        self.commands.file.assert_not_called()
        self.animation.d_animRead.assert_not_called()

    def test_invalid_rows_cannot_dispatch_any_file_operation(self):
        for row in (None, 0, -1, 4, True, False, 1.5, '1', [1]):
            with self.subTest(row=row):
                self.row = row
                self.assertFalse(self.run_import())
        self.assert_no_file_operation()

    def test_valid_import_uses_selected_version_and_empty_legacy_namespace(self):
        self.assertTrue(self.run_import())
        self.io.importFile.assert_called_once_with(self.records[1]['filepath'], None, 0, None, False)

    def test_empty_reference_namespace_is_not_literal_none(self):
        self.mode = 'Reference'
        self.assertTrue(self.run_import())
        self.io.reference.assert_called_once_with(self.records[1]['filepath'], False, False, '')

    def test_unicode_namespace_is_preserved_without_byte_coercion(self):
        self.mode = 'Reference'
        self.namespace_value = '担当者:asset'
        self.assertTrue(self.run_import())
        self.assertEqual(self.io.reference.call_args.args[-1], '担当者:asset')

    def test_row_or_control_query_failure_is_reported_without_dispatch(self):
        self.pm.scriptTable.side_effect = RuntimeError('row query failed')
        self.assertFalse(self.run_import())
        self.pm.scriptTable.side_effect = lambda *args, **kwargs: self.row
        self.pm.textFieldGrp.side_effect = RuntimeError('namespace query failed')
        self.assertFalse(self.run_import())
        self.assert_no_file_operation()
        self.assertTrue(self.warnings)

    def test_metadata_or_missing_asset_file_is_rechecked(self):
        self.ui.m_file_data[1] = dict(self.records[1], filepath=str(self.root.parent / 'outside.ma'))
        self.assertFalse(self.run_import())
        self.ui.m_file_data[1] = self.records[1] = self.record(2)
        Path(self.records[1]['filepath']).unlink()
        self.assertFalse(self.run_import())
        self.assert_no_file_operation()

    def test_alembic_loads_only_import_plugin(self):
        self.ui.m_file_data[1] = self.record(2, '.abc')
        self.assertTrue(self.run_import())
        self.plugins.isLoadedPlugin.assert_called_once_with(pluginName='AbcImport')
        self.plugins.loadMayaPlugin.assert_called_once_with(pluginName='AbcImport')
        self.commands.AbcImport.assert_called_once_with(self.ui.m_file_data[1]['filepath'], mode='import')

    def test_already_loaded_plugin_is_not_reloaded(self):
        self.ui.m_file_data[1] = self.record(2, '.abc')
        self.plugins.isLoadedPlugin.return_value = True
        self.assertTrue(self.run_import())
        self.plugins.loadMayaPlugin.assert_not_called()

    def test_plugin_load_or_native_import_failure_is_not_success(self):
        self.ui.m_file_data[1] = self.record(2, '.abc')
        self.plugins.loadMayaPlugin.return_value = False
        self.assertFalse(self.run_import())
        self.commands.AbcImport.assert_not_called()
        self.plugins.loadMayaPlugin.return_value = True
        self.commands.AbcImport.side_effect = RuntimeError('native import failed')
        self.assertFalse(self.run_import())
        self.assertTrue(self.warnings)

    def test_animation_routes_preserve_options_and_propagate_danim_status(self):
        self.ui.m_file_data[1] = self.record(2, '.anim')
        self.namespace_value = '担当者'
        self.assertTrue(self.run_import())
        self.assertEqual(self.commands.file.call_args.kwargs['namespace'], '担当者')
        self.plugins.loadMayaPlugin.assert_called_with(pluginName='animImportExport')
        self.ui.m_file_data[1] = self.record(2, '.danim')
        self.assertTrue(self.run_import())
        self.animation.d_animRead.assert_called_with(
            filename=self.ui.m_file_data[1]['filepath'], paths=True, unkeyed=True, namespace='担当者')
        self.animation.d_animRead.return_value = False
        self.assertFalse(self.run_import())

    def test_bad_namespace_or_mode_cannot_dispatch(self):
        self.namespace_value = None
        self.assertFalse(self.run_import())
        self.namespace_value = ''
        self.mode = 'Unknown'
        self.assertFalse(self.run_import())
        self.assert_no_file_operation()

    def test_invalid_display_rows_do_not_update_ui_from_another_record(self):
        for row in (None, 0, -1, 4, True, 1.5):
            self.row = row
            self.namespace['changeSelectionCB'](self.ui)
        self.pm.image.assert_not_called()
        self.pm.text.assert_not_called()

    def test_cell_provider_rejects_invalid_rows_and_columns(self):
        for row, column in ((0, 1), (-1, 1), (4, 1), (True, 1), (1, 0), (1, 7), (1, True)):
            self.assertEqual(self.namespace['getCellCB'](self.ui, row, column), '')
        self.assertEqual(self.namespace['getCellCB'](self.ui, 2, 1), 'v002')

    def test_interrupt_and_memory_error_are_not_hidden(self):
        for failure in (KeyboardInterrupt(), MemoryError('simulated allocation failure')):
            self.io.importFile.side_effect = failure
            with self.assertRaises(type(failure)):
                self.run_import()


if __name__ == '__main__':
    unittest.main()
