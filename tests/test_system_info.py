#!/usr/bin/env python3
"""Actual read-only information reports with typed/unavailable native queries."""

import numbers
import unittest
from contextlib import redirect_stdout
from io import StringIO
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_scene_node_ownership import definitions
from DPyTools.core import strings


SOURCE = 'python/DPyTools/features/system_info.py'


class SystemInfoTests(unittest.TestCase):
    def setUp(self):
        self.tablet = False
        self.hardware = {'cpuType': ['CPU'], 'brdType': ['Board'],
                         'graphicsType': ['Graphics'], 'numProcessors': [8], 'megaHertz': [3000]}
        self.pm = NS(memory=Mock(return_value=['memory summary']),
                     hardware=Mock(side_effect=lambda **kw: self.hardware[next(iter(kw))]),
                     about=Mock(side_effect=self.about),
                     pluginInfo=Mock(return_value=None),
                     workspace=Mock(return_value='project_root/担当者'),
                     fileInfo=NS(items=Mock(return_value=[('owner', 'artist')])),
                     openGLExtension=Mock(side_effect=lambda **kw:
                        'GL_A\tGL_B\nGL_C' if 'extension' in kw else 'GL info'),
                     clearCache=Mock(), loadPlugin=Mock(), warning=Mock(), delete=Mock())
        self.scope = definitions(SOURCE, {'pm': self.pm, 'strings': strings, 'numbers': numbers,
            'platform': NS(python_version=lambda: 'test-python'), 'sys': NS(path=['package_path'])})

    def about(self, **kwargs):
        flag = next(iter(kwargs))
        return self.tablet if flag == 'tablet' else 'available'

    def report(self, function):
        output = StringIO()
        with redirect_stdout(output):
            result = self.scope[function]()
        self.assertIsNone(result)
        return output.getvalue()

    def test_normal_report_keeps_every_section_and_numeric_hardware(self):
        text = self.report('displayInfo')
        for section in ('Memory', 'Hardware', 'Maya Info', 'Python Info', 'Python PATH',
                        'Qt Info', 'System File', 'Date Info', 'OS Info', 'Language', 'Tablet'):
            self.assertIn('------------{0}------------'.format(section), text)
        self.assertIn('processor Number:8', text)
        self.assertIn('MHz:3000', text)
        self.pm.clearCache.assert_not_called()
        self.pm.loadPlugin.assert_not_called()
        self.pm.delete.assert_not_called()

    def test_one_unavailable_hardware_query_does_not_hide_later_information(self):
        def hardware(**kwargs):
            if kwargs.get('brdType'):
                raise RuntimeError('board query unavailable')
            return ['hardware']
        self.pm.hardware.side_effect = hardware
        text = self.report('displayInfo')
        self.assertIn('Board:<unavailable: board query unavailable>', text)
        self.assertIn('Maya Info', text)
        self.assertIn('Python Version:test-python', text)
        self.assertIn('OS Info', text)

    def test_memory_failure_and_none_or_empty_hardware_are_reported_without_aborting(self):
        self.pm.memory.side_effect = RuntimeError('memory unavailable')
        self.hardware.update(cpuType=None, brdType=[], graphicsType=(), numProcessors=[0], megaHertz=False)
        text = self.report('displayInfo')
        self.assertIn('memory unavailable', text)
        self.assertIn('Cpu:<unavailable>', text)
        self.assertIn('Board:<unavailable>', text)
        self.assertIn('processor Number:0', text)
        self.assertIn('MHz:False', text)
        self.assertIn('Tablet', text)

    def test_failed_about_fields_do_not_prevent_later_queries(self):
        def about(**kwargs):
            if kwargs.get('product') or kwargs.get('irix'):
                raise TypeError('unsupported flag')
            return self.about(**kwargs)
        self.pm.about.side_effect = about
        text = self.report('displayInfo')
        self.assertIn('product:<unavailable: unsupported flag>', text)
        self.assertIn('irix:<unavailable: unsupported flag>', text)
        self.assertIn('uiLocaleLanguage:available', text)
        self.assertIn('Tablet', text)

    def test_unicode_bytes_and_multi_value_queries_are_display_only_text(self):
        self.hardware['cpuType'] = ['処理装置']
        self.hardware['brdType'] = [b'board\xff']
        self.hardware['graphicsType'] = ['GPU_A', 'GPU_B']
        text = self.report('displayInfo')
        self.assertIn('Cpu:処理装置', text)
        self.assertIn('Board:board\ufffd', text)
        self.assertIn('Graphic Card:GPU_A, GPU_B', text)

    def test_malformed_value_formatting_is_unavailable_not_a_report_abort(self):
        class BrokenValue:
            def __str__(self):
                raise ValueError('formatting failed')
        self.hardware['cpuType'] = [BrokenValue()]
        text = self.report('displayInfo')
        self.assertIn('Cpu:<unavailable: formatting failed>', text)
        self.assertIn('OS Info', text)

    def test_empty_memory_and_path_queries_have_explicit_none_rows(self):
        self.pm.memory.return_value = None
        self.scope['sys'].path = []
        text = self.report('displayInfo')
        self.assertIn('Summary:<none>', text)
        self.assertIn('Path:<none>', text)

    def test_bounded_sequence_failure_does_not_hide_later_sections(self):
        self.scope['sys'].path = ['entry'] * 10001
        text = self.report('displayInfo')
        self.assertIn('Path:<unavailable:', text)
        self.assertIn('Qt Info', text)
        self.assertIn('Tablet', text)

    def test_tablet_mode_is_only_queried_when_presence_is_confirmed(self):
        self.report('displayInfo')
        self.assertFalse(any(call.kwargs.get('tabletMode') for call in self.pm.about.call_args_list))
        self.tablet = True
        self.pm.about.reset_mock()
        text = self.report('displayInfo')
        self.assertIn('Tablet is found.', text)
        self.assertTrue(any(call.kwargs.get('tabletMode') for call in self.pm.about.call_args_list))

    def test_unavailable_or_malformed_tablet_presence_does_not_claim_absence(self):
        for result in (None, [], 'not a Boolean'):
            self.tablet = result
            text = self.report('displayInfo')
            self.assertIn('tablet:<unavailable:', text)
            self.assertNotIn('Tablet is not found.', text)

    def test_query_interruption_is_not_hidden_as_missing_information(self):
        error = KeyboardInterrupt('hardware interrupted')
        self.pm.hardware.side_effect = error
        with redirect_stdout(StringIO()), self.assertRaises(KeyboardInterrupt) as caught:
            self.scope['displayInfo']()
        self.assertIs(caught.exception, error)
        self.pm.about.assert_not_called()

    def test_missing_native_information_commands_are_reported_without_aborting(self):
        del self.pm.hardware
        del self.pm.pluginInfo
        del self.pm.openGLExtension
        text = self.report('displayInfo')
        self.assertIn('Cpu:<unavailable:', text)
        self.assertIn('OS Info', text)
        self.assertIn('<unavailable:', self.report('pluginInfo'))
        text = self.report('displayOpenGLInfo')
        self.assertIn('Vendor:<unavailable:', text)
        self.assertIn('Extension:<unavailable:', text)

    def test_system_exit_is_not_hidden(self):
        self.pm.memory.side_effect = SystemExit(3)
        with redirect_stdout(StringIO()), self.assertRaises(SystemExit):
            self.scope['displayInfo']()

    def test_none_or_empty_plugin_inventory_is_a_readonly_noop(self):
        for values in (None, [], ()):
            self.pm.pluginInfo.return_value = values
            text = self.report('pluginInfo')
            self.assertIn('No plug-ins loaded.', text)
        self.assertTrue(all(call.kwargs == {'query': True, 'listPlugins': True}
                            for call in self.pm.pluginInfo.call_args_list))

    def test_malformed_or_over_limit_plugin_inventory_never_queries_name_prefixes(self):
        for values in ('plugin', [None], [''], ['plugin'] * 10001):
            self.pm.pluginInfo.reset_mock()
            self.pm.pluginInfo.return_value = values
            self.assertIn('<unavailable:', self.report('pluginInfo'))
            self.pm.pluginInfo.assert_called_once_with(query=True, listPlugins=True)

    def test_late_plugin_field_failure_keeps_other_fields_and_plugins(self):
        def plugin(*args, **kwargs):
            if kwargs.get('listPlugins'):
                return ['firstPlugin', '後続Plugin']
            if args[0] == 'firstPlugin' and kwargs.get('version'):
                raise RuntimeError('stale version query')
            return 'field info'
        self.pm.pluginInfo.side_effect = plugin
        text = self.report('pluginInfo')
        self.assertIn('firstPlugin', text)
        self.assertIn('stale version query', text)
        self.assertIn('後続Plugin', text)
        self.assertEqual(self.pm.pluginInfo.call_count, 9)

    def test_current_file_unicode_path_and_metadata_are_displayed_without_encoding_concat(self):
        text = self.report('currentFileInfo')
        self.assertIn('Project root dir:project_root/担当者', text)
        self.assertIn('owner, artist', text)

    def test_failed_workspace_query_still_displays_file_metadata(self):
        self.pm.workspace.side_effect = RuntimeError('workspace unavailable')
        text = self.report('currentFileInfo')
        self.assertIn('workspace unavailable', text)
        self.assertIn('owner, artist', text)

    def test_opengl_field_failure_still_displays_other_fields_and_extensions(self):
        def query(**kwargs):
            if kwargs.get('renderer'):
                raise RuntimeError('renderer unavailable')
            return 'GL_A\tGL_B\nGL_C' if 'extension' in kwargs else 'GL info'
        self.pm.openGLExtension.side_effect = query
        text = self.report('displayOpenGLInfo')
        self.assertIn('renderer unavailable', text)
        self.assertIn('Vendor:GL info', text)
        for name in ('GL_A', 'GL_B', 'GL_C'):
            self.assertIn('Extension:' + name, text)

    def test_empty_or_malformed_extensions_do_not_raise(self):
        for value in (None, '', 3):
            self.pm.openGLExtension.side_effect = lambda **kwargs: value if 'extension' in kwargs else 'GL info'
            self.assertIn('Extension:<', self.report('displayOpenGLInfo'))

    def test_existing_plugin_load_and_explicit_cache_clear_flags_are_preserved(self):
        self.pm.pluginInfo.return_value = True
        with redirect_stdout(StringIO()):
            self.assertIs(self.scope['loadMayaPlugin']('plugin'), True)
            self.scope['clearNodeCache']()
        self.pm.loadPlugin.assert_called_once_with('plugin')
        self.pm.pluginInfo.assert_called_once_with('plugin', query=True, loaded=True)
        self.pm.clearCache.assert_called_once_with(all=True)

    def test_historical_module_is_an_explicit_identity_preserving_facade(self):
        facade = definitions('python/DPyTools/system/DSystemInfo.py',
                             {'_implementation': NS(**self.scope)})
        expected = ('isLoadedPlugin', 'loadMayaPlugin', 'echoWarning', 'displayInfo',
                    'pluginInfo', 'currentFileInfo', 'displayOpenGLInfo', 'clearNodeCache')
        self.assertEqual(facade['__all__'], expected)
        for name in expected:
            self.assertIs(facade[name], self.scope[name])


if __name__ == '__main__':
    unittest.main()
