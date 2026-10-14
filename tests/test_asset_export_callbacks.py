#!/usr/bin/env python3
"""Actual complete asset export callbacks with owned disposable files and stubs."""

import ast
import datetime
import os
import tempfile
import unittest
from pathlib import Path
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Node
from test_texture_paths import TextureGraph
from test_python_rig_safety import functions
from test_python_table_selection import _tree
from DPyTools.core import components, paths
from DPyTools.pipeline.assets import export_plan, metadata, repository, paths as asset_paths
from DPyTools.pipeline.io import alembic_job
from DPyTools.scene import attribute_values
from DPyTools.ui import table_selection


def export_methods(namespace):
    tree = _tree('python/DPyTools/pipeline/assets/manager.py')
    cls = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == 'DAssetManager')
    names = {'_prepare_export', '_dispatch_export', '_notify_export', 'runExportCB'}
    body = [node for node in cls.body if isinstance(node, ast.FunctionDef) and node.name in names]
    module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
    exec(compile(module, 'production-asset-export', 'exec'), namespace)
    return namespace


class AssetExportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.version = self.root / 'v001'
        self.texture = TextureGraph()
        self.selected = ['borrowed-selection']
        self.controls = {'root': 'root', 'creator': 'artist', 'comment': 'archive', 'type': '.ma',
                         'textures': True, 'thumbnail': False, 'notify': False, 'category': ['PROPS'],
                         'asset': ['prop'], 'project': ['project'], 'hierarchy': 2}
        self.timerange = [1, 24]
        self.native_error, self.write_native = None, True
        self.top = Node('root', 'transform')
        self.events = []
        self.pm = NS(textField=Mock(side_effect=self.query), optionMenu=Mock(side_effect=self.query),
                     checkBox=Mock(side_effect=self.query), textScrollList=Mock(side_effect=self.query),
                     radioButtonGrp=Mock(side_effect=self.query), checkBoxGrp=Mock(return_value=False),
                     PyNode=Mock(return_value=self.top), nt=self.texture.pm.nt,
                     listRelatives=Mock(return_value=[]),
                     ls=Mock(side_effect=lambda **kw: list(self.selected)), objExists=lambda node: True,
                     select=Mock(side_effect=self.select), warning=Mock())
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {'pm': self.pm, 'components': components}))
        self.material = NS(temporary_file_paths=self.texture.scope['temporary_file_paths'],
                           validate_texture_edits=self.texture.scope['validate_texture_edits'],
                           isUnResolveFiles=Mock(return_value=False), copyFileNodeImageToFolder=Mock(side_effect=self.copy))
        self.io = NS(exportSelected=Mock(side_effect=self.native))
        self.commands = NS(AbcExport=Mock(side_effect=self.abc), file=Mock(side_effect=self.native))
        self.animation = NS(d_animWrite=Mock(side_effect=lambda filename, **kw: self.native(filename) or True))
        self.plugins = NS(isLoadedPlugin=Mock(return_value=True), loadMayaPlugin=Mock(return_value=True))
        self.capture = Mock(side_effect=self.thumbnail)
        self.asset = NS(getAssetPath=Mock(return_value=str(self.root)), getExportPath=Mock(return_value=str(self.version)),
                        getMailAddressFromML=Mock(return_value=['recipient@example.invalid']))
        self.repo = NS(load_version_records=Mock(side_effect=repository.load_version_records),
                       save_version_records=Mock(side_effect=repository.save_version_records))
        self.path_api = NS(openCreateDirectoryDialog=Mock(side_effect=self.reserve), mkdir=paths.ensure_directory)
        self.mail = NS(MailError=RuntimeError, send_notification=Mock())
        scope = export_methods({'pm': self.pm, 'om': NS(MFileIO=lambda: self.io), 'cmds': self.commands,
                                'danimIO': self.animation, 'dsi': self.plugins, 'dview': NS(captureThumbnail=self.capture),
                                'dmat': self.material, 'dasset': self.asset, 'asset_repository': self.repo,
                                'asset_metadata': metadata, 'export_plan': export_plan, 'asset_paths': asset_paths,
                                'parameters': self.texture.graph.parameters, 'attribute_values': attribute_values,
                                'components': components, 'core_paths': paths, 'table_selection': table_selection,
                                'datetime': datetime, 'os': os, 'selOp': self.selection,
                                'aniOp': NS(isAnimationExist=Mock(return_value=True)), 'dpath': self.path_api,
                                'alembic_job': alembic_job, 'smtp_client': self.mail,
                                'exportFileTypeDic': {'.ma': 'mayaAscii', '.mb': 'mayaBinary', '.anim': 'animExport'}})
        self.ui = NS(rootText='root', CreatorText='creator', commentField='comment', fileTypeMenu='type',
                     checkExportFile='textures', checkThumbnail='thumbnail', sendMailTask='notify',
                     classTextScroll='category', assetTextScroll='asset', projTextScroll='project',
                     animHierarchyRadio='hierarchy', isRenderableOnlyCheck='renderable', isWriteVisibilityCheck='visibility',
                     isWholeFrameGeoCheck='whole', isUVWriteCheck='uv', isWorldSpaceCheck='world', isNoNormalCheck='normal',
                     getTimeRangeCB=lambda: self.timerange, updateExportOptionState=Mock(),
                     getProjectPath=Mock(return_value=str(self.root)))
        for name in ('_prepare_export', '_dispatch_export', '_notify_export', 'runExportCB'):
            setattr(self.ui, name, MethodType(scope[name], self.ui))
        self.scope = scope
    def query(self, control, **kwargs):
        return self.controls[control]
    def select(self, value=None, **kwargs):
        self.selected = [] if kwargs.get('clear') else (list(value) if isinstance(value, (list, tuple)) else [value])
    def reserve(self, dirpath, **kwargs):
        os.mkdir(dirpath)
        return True
    def copy(self, copyFolder):
        result = {}
        for index, source in enumerate(('old1.png', 'old2.png'), 1):
            destination = Path(copyFolder) / ('texture{0}.png'.format(index))
            destination.write_bytes(b'texture-fixture')
            result[source] = str(destination)
        self.events.append('copy')
        return result
    def thumbnail(self, filepath, filename, ext):
        destination = Path(filepath) / (filename + '.' + ext)
        destination.write_bytes(b'image-fixture')
        return str(destination)
    def native(self, filename, *args, **kwargs):
        self.events.append('native')
        self.assertEqual(self.selected, [self.top])
        if self.controls['textures'] and self.controls['type'] in ('.ma', '.mb'):
            self.assertTrue(all(value.startswith(str(self.version / 'sourceimages')) for value in self.texture.values()))
        if self.write_native: Path(filename).write_bytes(b'native-output-fixture')
        if self.native_error is not None: raise self.native_error
    def abc(self, **kwargs):
        filepath = self.version / 'prop_v001.abc'
        self.native(str(filepath))
    def run_export(self):
        return self.ui.runExportCB()
    def assert_restored(self):
        self.assertEqual(self.selected, ['borrowed-selection'])
        self.assertEqual(self.texture.values(), ('old1.png', 'old2.png'))
    def assert_no_mutation(self):
        self.pm.select.assert_not_called()
        self.path_api.openCreateDirectoryDialog.assert_not_called()
        self.material.copyFileNodeImageToFolder.assert_not_called()
        self.capture.assert_not_called()
        self.assertFalse(self.version.exists())
    def test_success_commits_metadata_only_after_temporary_paths_restored(self):
        native_save = self.repo.save_version_records.side_effect
        def save(*args, **kwargs):
            self.assertEqual(self.texture.values(), ('old1.png', 'old2.png'))
            self.events.append('metadata')
            return native_save(*args, **kwargs)
        self.repo.save_version_records.side_effect = save
        self.assertTrue(self.run_export())
        self.assert_restored()
        self.assertEqual(self.events, ['copy', 'native', 'metadata'])
        records = repository.load_version_records(str(self.root / '.asset'), asset_root=str(self.root), require_files=True)
        self.assertEqual(records[0]['filename'], 'prop_v001.ma')
    def test_native_failure_or_interruption_restores_paths_selection_and_never_saves_metadata(self):
        for error in (RuntimeError('export failed'), KeyboardInterrupt()):
            self.setUp()
            self.native_error = error
            if isinstance(error, KeyboardInterrupt):
                with self.assertRaises(KeyboardInterrupt): self.run_export()
            else: self.assertFalse(self.run_export())
            self.assert_restored()
            self.repo.save_version_records.assert_not_called()
            self.mail.send_notification.assert_not_called()
    def test_failed_path_restore_cannot_publish_metadata_as_success(self):
        native = self.texture.pm.setAttr.side_effect
        self.texture.pm.setAttr.side_effect = lambda port, value, **kw: (_ for _ in ()).throw(RuntimeError('restore failed')) if value == 'old2.png' else native(port, value, **kw)
        self.assertFalse(self.run_export())
        self.assertEqual(self.selected, ['borrowed-selection'])
        self.assertEqual(self.texture.values()[0], 'old1.png')
        self.repo.save_version_records.assert_not_called()
    def test_change_then_throw_during_repath_restores_before_any_native_export(self):
        native = self.texture.pm.setAttr.side_effect
        def write(port, value, **kw):
            native(port, value, **kw)
            if value.endswith('texture2.png'): raise RuntimeError('changed then failed')
        self.texture.pm.setAttr.side_effect = write
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.io.exportSelected.assert_not_called()
    def test_copy_failure_restores_selection_without_mutating_texture_paths(self):
        self.material.copyFileNodeImageToFolder.side_effect = IOError('copy failed')
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.texture.pm.setAttr.assert_not_called()
    def test_thumbnail_failure_or_wrong_destination_stops_before_texture_scene_export(self):
        for result in (IOError('save failed'), None, str(self.root / 'outside.png')):
            self.setUp()
            self.controls['thumbnail'] = True
            self.capture.side_effect = result if isinstance(result, BaseException) else None
            self.capture.return_value = result if not isinstance(result, BaseException) else None
            self.assertFalse(self.run_export())
            self.assert_restored()
            self.material.copyFileNodeImageToFolder.assert_not_called()
            self.io.exportSelected.assert_not_called()
    def test_cancelled_reservation_and_failed_plugin_load_do_not_touch_texture_paths(self):
        self.path_api.openCreateDirectoryDialog.side_effect = None
        self.path_api.openCreateDirectoryDialog.return_value = False
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.texture.pm.setAttr.assert_not_called()
        self.controls['type'] = '.abc'
        self.plugins.isLoadedPlugin.return_value = False
        self.plugins.loadMayaPlugin.return_value = False
        self.path_api.openCreateDirectoryDialog.reset_mock()
        self.assertFalse(self.run_export())
        self.path_api.openCreateDirectoryDialog.assert_not_called()
        self.assert_restored()
    def test_empty_or_invalid_root_category_asset_and_type_precede_mutation(self):
        for field, value in (('root', ''), ('root', 'bad\x00node'), ('category', None), ('asset', []),
                             ('asset', ['a', 'b']), ('asset', ['../escape']), ('type', '.exe')):
            self.setUp()
            self.controls[field] = value
            with self.subTest(field=field, value=value): self.assertFalse(self.run_export())
            self.assert_no_mutation()
    def test_time_record_options_and_invalid_paths_precede_mutation(self):
        for value in ([2, 1], [True, 2], [1, float('nan')]):
            self.setUp()
            self.timerange = value
            self.assertFalse(self.run_export())
            self.assert_no_mutation()
        for field, value in (('creator', None), ('comment', 'bad\x00value'), ('thumbnail', 'false')):
            self.setUp()
            self.controls[field] = value
            self.assertFalse(self.run_export())
            self.assert_no_mutation()
        self.setUp()
        self.asset.getExportPath.return_value = str(self.root.parent / 'v001')
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
    def test_unresolved_textures_and_invalid_existing_metadata_precede_mutation(self):
        self.material.isUnResolveFiles.return_value = True
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
        self.material.isUnResolveFiles.return_value = False
        (self.root / '.asset').write_text('{bad-json', encoding='utf-8')
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
    def test_late_abc_option_failure_precedes_directory_or_scene_mutation(self):
        self.controls['type'] = '.abc'
        self.pm.checkBoxGrp.side_effect = RuntimeError('option query failed')
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
    def test_missing_native_output_or_metadata_failure_restores_and_reports_false(self):
        self.write_native = False
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.repo.save_version_records.assert_not_called()
        self.setUp()
        self.repo.save_version_records.side_effect = OSError('metadata failed')
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.mail.send_notification.assert_not_called()
    def test_late_selection_failure_is_restored_even_after_change_then_throw(self):
        native = self.pm.select.side_effect
        def select(value=None, **kw):
            native(value, **kw)
            if value is self.top: raise RuntimeError('select changed then failed')
        self.pm.select.side_effect = select
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.io.exportSelected.assert_not_called()
    def test_selection_restore_failure_is_not_a_success_or_a_masked_primary(self):
        native = self.pm.select.side_effect
        self.pm.select.side_effect = lambda value=None, **kw: (_ for _ in ()).throw(RuntimeError('restore failed')) if value == ['borrowed-selection'] else native(value, **kw)
        self.assertFalse(self.run_export())
        self.assertEqual(self.texture.values(), ('old1.png', 'old2.png'))
        self.setUp()
        self.native_error = KeyboardInterrupt()
        self.selection.restore_selection = Mock(return_value=False)
        with self.assertRaises(KeyboardInterrupt): self.run_export()
    def test_all_native_formats_keep_dispatch_options_and_restore_selection(self):
        for extension in ('.mb', '.abc', '.anim', '.danim'):
            self.setUp()
            self.controls['type'] = extension
            self.assertTrue(self.run_export())
            self.assert_restored()
            if extension == '.anim':
                self.assertIn('range=0:10;', self.commands.file.call_args.kwargs['options'])
            if extension not in ('.ma', '.mb'): self.material.copyFileNodeImageToFolder.assert_not_called()
    def test_invalid_hierarchy_or_failed_danim_confirmation_is_not_success(self):
        for extension in ('.anim', '.danim'):
            self.controls.update(type=extension, hierarchy=3)
            self.assertFalse(self.run_export())
            self.assert_no_mutation()
        self.controls.update(type='.danim', hierarchy=2)
        self.animation.d_animWrite.side_effect = None
        self.animation.d_animWrite.return_value = False
        self.assertFalse(self.run_export())
        self.assert_restored()
        self.repo.save_version_records.assert_not_called()
    def test_notifications_run_after_restoration_and_cannot_erase_successful_export(self):
        self.controls['notify'] = True
        self.mail.send_notification.side_effect = lambda **kw: self.assert_restored()
        self.assertTrue(self.run_export())
        self.mail.send_notification.assert_called_once()
        self.setUp()
        self.controls['notify'] = True
        self.mail.send_notification.side_effect = RuntimeError('mail failed')
        self.assertTrue(self.run_export())
        self.assert_restored()
        self.pm.warning.assert_called()
    def test_notification_retains_project_when_ui_selection_disappears_after_commit(self):
        self.controls['notify'] = True
        self.ui.updateExportOptionState.side_effect = lambda: self.controls.update(project=None)
        self.assertTrue(self.run_export())
        self.mail.send_notification.assert_called_once()
        self.assertTrue(self.mail.send_notification.call_args.kwargs['title'].startswith('PROJECT |'))
    def test_empty_snapshot_recipients_skip_delivery(self):
        self.controls['notify'] = True
        self.asset.getMailAddressFromML.return_value = []
        self.assertTrue(self.run_export())
        self.mail.send_notification.assert_not_called()
    def test_project_and_recipient_changes_after_admission_do_not_retarget_notification(self):
        self.controls['notify'] = True
        recipients = ['original@example.invalid']
        self.asset.getMailAddressFromML.return_value = recipients
        def change():
            self.controls['project'] = ['other_project']
            recipients[:] = ['other_project@example.invalid']
        self.ui.updateExportOptionState.side_effect = change
        self.assertTrue(self.run_export())
        self.asset.getMailAddressFromML.assert_called_once_with(project_path=str(self.root))
        self.ui.getProjectPath.assert_called_once_with(project_name='project')
        sent = self.mail.send_notification.call_args.kwargs
        self.assertTrue(sent['title'].startswith('PROJECT |'))
        self.assertEqual(sent['address_list'], ('original@example.invalid',))
        self.asset.getAssetPath.assert_called_once_with(assetType='PROPS', assetName='prop', project_path=str(self.root))
    def test_notification_snapshot_failure_skips_delivery_without_losing_valid_export(self):
        for value in (RuntimeError('query failed'), 'not-a-list', [True], ['a', 'a']):
            self.setUp()
            self.controls['notify'] = True
            self.asset.getMailAddressFromML.side_effect = value if isinstance(value, BaseException) else None
            self.asset.getMailAddressFromML.return_value = value
            self.assertTrue(self.run_export())
            self.assert_restored()
            self.mail.send_notification.assert_not_called()
            self.assertTrue(any('notification preparation failed' in str(call) for call in self.pm.warning.call_args_list))
    def test_invalid_selection_snapshot_or_query_failure_precedes_mutation(self):
        for value in ('not-a-list', ['node'] * 10001):
            self.pm.ls.side_effect = None
            self.pm.ls.return_value = value
            self.assertFalse(self.run_export())
            self.assert_no_mutation()
        self.pm.ls.side_effect = RuntimeError('selection query failed')
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
    def test_prospective_metadata_collision_is_rejected_before_reservation(self):
        record = export_plan.build(str(self.root), str(self.version), 'prop', '.ma', [1, 24],
                                   'artist', 'archive', '2019/01/01')['record']
        repository.save_version_records(str(self.root / '.asset'), [record], asset_root=str(self.root))
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
    def test_confirmed_thumbnail_is_recorded_only_after_complete_export(self):
        self.controls['thumbnail'] = True
        self.assertTrue(self.run_export())
        self.assert_restored()
        records = repository.load_version_records(str(self.root / '.asset'), asset_root=str(self.root))
        self.assertEqual(records[0]['thumbnail'], str(self.version / 'thumbnail.png'))
    def test_late_locked_texture_is_rejected_before_copy_or_reservation(self):
        self.texture.nodes[1].attr('fileTextureName').locked = True
        self.assertFalse(self.run_export())
        self.assert_no_mutation()
        self.texture.pm.setAttr.assert_not_called()


if __name__ == '__main__': unittest.main()
