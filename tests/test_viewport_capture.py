#!/usr/bin/env python3
"""Actual viewport-state/capture adapters and thumbnail callback branches."""

import ast
import contextlib
import copy
import numbers
import sys
import tempfile
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock
from pathlib import Path

from test_python_rig_safety import functions
from test_python_table_selection import _tree
from DPyTools.core import strings
from DPyTools.scene import attribute_values
from DPyTools.core import image_output


class EditorGraph:
    def __init__(self):
        self.state = {'a': {'selectionHiliteDisplay': True, 'headsUpDisplay': False},
                      'b': {'selectionHiliteDisplay': False, 'headsUpDisplay': True}}
        self.initial = copy.deepcopy(self.state)
        self.panels = ['panel_a', 'other', 'panel_b']
        self.pm = NS(getPanel=self.panel, modelPanel=lambda panel, **kw: panel[-1],
                     modelEditor=Mock(side_effect=self.editor), warning=Mock())
        self.scope = functions('python/DPyTools/ui/viewport_state.py', {
            'pm': self.pm, 'sys': sys, 'contextmanager': contextlib.contextmanager,
            'strings': strings, 'attribute_values': attribute_values,
            'MAX_VISIBLE_PANELS': 128,
            'OVERLAY_FLAGS': ('selectionHiliteDisplay', 'headsUpDisplay')})
    def panel(self, **kwargs):
        return self.panels if kwargs.get('visiblePanels') else ('modelPanel' if kwargs['typeOf'].startswith('panel_') else 'outlinerPanel')
    def editor(self, editor, **kwargs):
        fields = {key: value for key, value in kwargs.items() if key not in ('query', 'edit')}
        if kwargs.get('query'): return self.state[editor][next(iter(fields))]
        self.state[editor].update(fields)
    def hidden(self): return self.scope['hidden_overlays']()


class ViewportStateTests(unittest.TestCase):
    def setUp(self): self.graph = EditorGraph()
    def test_all_editors_restore_exact_prior_values_on_success(self):
        with self.graph.hidden():
            self.assertTrue(all(not value for state in self.graph.state.values() for value in state.values()))
        self.assertEqual(self.graph.state, self.graph.initial)
    def test_capture_failure_and_interruption_restore_every_editor(self):
        for error in (RuntimeError('capture failed'), KeyboardInterrupt()):
            with self.subTest(error=error), self.assertRaises(type(error)):
                with self.graph.hidden(): raise error
            self.assertEqual(self.graph.state, self.graph.initial)
    def test_late_query_failure_precedes_every_edit(self):
        native = self.graph.pm.modelEditor.side_effect
        def dispatch(editor, **kwargs):
            if editor == 'b' and kwargs.get('query'): raise RuntimeError('query failed')
            return native(editor, **kwargs)
        self.graph.pm.modelEditor.side_effect = dispatch
        with self.assertRaises(RuntimeError):
            with self.graph.hidden(): self.fail('must not enter')
        self.assertFalse(any(call.kwargs.get('edit') for call in self.graph.pm.modelEditor.call_args_list))
    def test_change_then_throw_restores_that_editor_and_previous_edits(self):
        native = self.graph.pm.modelEditor.side_effect
        def dispatch(editor, **kwargs):
            result = native(editor, **kwargs)
            if editor == 'b' and kwargs.get('edit') and len(kwargs) == 3: raise RuntimeError('changed then failed')
            return result
        self.graph.pm.modelEditor.side_effect = dispatch
        with self.assertRaisesRegex(RuntimeError, 'changed then failed'):
            with self.graph.hidden(): self.fail('must not enter')
        self.assertEqual(self.graph.state, self.graph.initial)
    def test_restore_failure_attempts_other_flags_and_editors_without_masking_primary(self):
        native = self.graph.pm.modelEditor.side_effect
        def dispatch(editor, **kwargs):
            if editor == 'b' and kwargs.get('edit') and len(kwargs) == 2 and 'selectionHiliteDisplay' in kwargs: raise RuntimeError('restore failed')
            return native(editor, **kwargs)
        self.graph.pm.modelEditor.side_effect = dispatch
        error = ValueError('primary')
        with self.assertRaises(ValueError) as caught:
            with self.graph.hidden(): raise error
        self.assertIs(caught.exception, error)
        self.assertEqual(self.graph.state['a'], self.graph.initial['a'])
        self.assertEqual(self.graph.state['b']['headsUpDisplay'], True)
        self.graph.pm.warning.assert_called_once()
    def test_restore_failure_after_success_is_not_reported_as_success(self):
        native = self.graph.pm.modelEditor.side_effect
        def dispatch(editor, **kwargs):
            if kwargs.get('edit') and len(kwargs) == 2: raise RuntimeError('restore failed')
            return native(editor, **kwargs)
        self.graph.pm.modelEditor.side_effect = dispatch
        with self.assertRaisesRegex(RuntimeError, 'restoration was incomplete'):
            with self.graph.hidden(): pass
    def test_empty_or_duplicate_visible_panels_do_not_invent_edits(self):
        self.graph.panels = None
        with self.graph.hidden(): pass
        self.graph.pm.modelEditor.assert_not_called()
        self.graph.panels = ['panel_a', 'panel_a']
        with self.graph.hidden(): pass
        edits = [call for call in self.graph.pm.modelEditor.call_args_list if call.kwargs.get('edit')]
        self.assertEqual(len(edits), 3)
    def test_invalid_panel_list_editor_or_flag_precedes_edits(self):
        for panels in ('panel_a', ['panel_a'] * 129):
            self.graph.panels = panels
            with self.assertRaises(ValueError):
                with self.graph.hidden(): pass
        self.graph.panels = ['panel_a']
        self.graph.state['a']['headsUpDisplay'] = 'false'
        with self.assertRaises(TypeError):
            with self.graph.hidden(): pass
        self.assertFalse(any(call.kwargs.get('edit') for call in self.graph.pm.modelEditor.call_args_list))
    def test_warning_failure_does_not_mask_capture_failure(self):
        self.graph.pm.warning.side_effect = RuntimeError('warning failed')
        native = self.graph.pm.modelEditor.side_effect
        def dispatch(editor, **kwargs):
            if kwargs.get('edit') and len(kwargs) == 2: raise RuntimeError('restore failed')
            return native(editor, **kwargs)
        self.graph.pm.modelEditor.side_effect = dispatch
        with self.assertRaisesRegex(ValueError, 'primary'):
            with self.graph.hidden(): raise ValueError('primary')


def thumbnail_branch(namespace):
    tree = _tree('python/DPyTools/pipeline/assets/manager.py')
    initial = next(node for node in ast.walk(tree) if isinstance(node, ast.Assign)
                   and any(isinstance(target, ast.Name) and target.id == 'thumbnail_path' for target in node.targets)
                   and isinstance(node.value, ast.Constant) and node.value.value == '')
    branch = next(node for node in ast.walk(tree) if isinstance(node, ast.If)
                  and isinstance(node.test, ast.Name) and node.test.id == 'isSaveThumbnail')
    function = ast.FunctionDef(name='run', args=ast.arguments(posonlyargs=[], args=[], kwonlyargs=[], kw_defaults=[], defaults=[]),
                              body=[initial, branch, ast.Return(value=ast.Name(id='thumbnail_path', ctx=ast.Load()))], decorator_list=[])
    module = ast.fix_missing_locations(ast.Module(body=[function], type_ignores=[]))
    exec(compile(module, 'production-thumbnail-branch', 'exec'), namespace)
    return namespace['run']


class ThumbnailCallbackTests(unittest.TestCase):
    def setUp(self):
        self.capture = Mock(return_value='saved-thumbnail.png')
        self.warning = Mock()
        self.scope = {'pm': NS(warning=self.warning), 'dview': NS(captureThumbnail=self.capture),
                      'isSaveThumbnail': True, 'exportPath': 'export-directory'}
        self.run = thumbnail_branch(self.scope)
    def test_capture_success_propagates_the_confirmed_thumbnail_path(self):
        self.assertEqual(self.run(), 'saved-thumbnail.png')
        self.capture.assert_called_once_with(filepath='export-directory', filename='thumbnail', ext='png')
    def test_failed_or_unconfirmed_capture_stops_the_export_branch(self):
        for error in (IOError('save failed'), RuntimeError('viewport failed')):
            self.capture.side_effect = error
            self.assertIs(self.run(), False)
        self.capture.side_effect, self.capture.return_value = None, None
        self.assertIs(self.run(), False)
        self.assertEqual(self.warning.call_count, 3)
    def test_disabled_thumbnail_is_empty_without_capture(self):
        self.scope['isSaveThumbnail'] = False
        self.assertEqual(self.run(), '')
        self.capture.assert_not_called()
    def test_interrupt_propagates_instead_of_becoming_export_success(self):
        self.capture.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.run()
        self.warning.assert_not_called()


class CaptureAdapterTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'thumbnail.png'
        self.width, self.height, self.address = 2, 3, 1
        self.null, self.saved, self.renderer = False, True, 'legacy'
        self.created_images, self.reads, self.saves = [], [], []
        test = self
        class Image:
            kFloat, kByte = 'float', 'byte'
            def __init__(self): test.created_images.append(self)
            def create(self, *args): self.created = args
            def convertPixelFormat(self, kind): self.converted = kind
            def pixels(self): return NS(__long__=lambda: test.address)
        class QtImage:
            Format_ARGB32 = 1
            def __init__(self, data, width, height, kind): test.assertEqual(len(data), width * height * 4)
            def isNull(self): return test.null
            def mirrored(self, **kwargs):
                test.assertEqual(kwargs, {'horizontal': False, 'vertical': True})
                return self
            def scaled(self, width, height, *args):
                test.assertEqual((width, height), (320, 320))
                return self
            def save(self, filename, fmt):
                test.saves.append((filename, fmt))
                Path(filename).write_bytes(b'encoded-image' if test.saved else b'partial')
                return test.saved
        self.view = NS(portWidth=lambda: self.width, portHeight=lambda: self.height,
                       getRendererName=lambda: self.renderer, kViewport2Renderer='viewport2', readColorBuffer=Mock())
        self.ctype = NS(cast=lambda *args: None, POINTER=lambda kind: None, c_char=object(), string_at=self.read)
        self.context = Mock(return_value=contextlib.nullcontext())
        self.scope = functions('python/DPyTools/features/view_capture.py', {
            'api': NS(MImage=Image), 'ctypes': self.ctype, 'QImage': QtImage, 'numbers': numbers,
            'Qt': NS(KeepAspectRatio=1, SmoothTransformation=1), 'image_output': image_output,
            'viewport_state': NS(hidden_overlays=self.context), 'os': __import__('os'), 'sys': sys})
        self.camera = Mock(return_value=(None, self.view))
        self.scope['getViewForThumbnail'] = self.camera
    def read(self, pointer, count):
        self.reads.append(count)
        return b'x' * count
    def run_capture(self, **kwargs): return self.scope['captureActiveView'](self.directory.name, 'thumbnail', **kwargs)
    def test_success_returns_only_published_output_and_uses_explicit_codec(self):
        self.assertEqual(self.run_capture(), str(self.path))
        self.assertEqual(self.path.read_bytes(), b'encoded-image')
        self.assertEqual(self.reads, [24])
        self.assertEqual(self.saves[0][1], 'PNG')
        self.assertNotEqual(self.saves[0][0], str(self.path))
    def test_false_save_preserves_an_existing_image(self):
        self.path.write_bytes(b'old')
        self.saved = False
        with self.assertRaises(IOError): self.run_capture()
        self.assertEqual(self.path.read_bytes(), b'old')
        self.assertEqual(list(self.path.parent.iterdir()), [self.path])
    def test_invalid_size_precedes_camera_or_native_image_reads(self):
        for size in (True, 10.5, -1, 4097):
            with self.assertRaises((TypeError, ValueError)): self.run_capture(size=size)
        self.camera.assert_not_called()
        self.assertEqual(self.created_images, [])
        self.assertEqual(self.reads, [])
    def test_invalid_viewport_budget_precedes_image_allocation(self):
        self.width, self.height = 8192, 8192
        with self.assertRaises(ValueError): self.run_capture()
        self.assertEqual(self.created_images, [])
        self.view.readColorBuffer.assert_not_called()
    def test_null_pointer_null_image_or_short_byte_read_cannot_save(self):
        self.address = 0
        with self.assertRaises(RuntimeError): self.run_capture()
        self.assertEqual(self.reads, [])
        self.address, self.null = 1, True
        with self.assertRaises(RuntimeError): self.run_capture()
        self.null = False
        self.ctype.string_at = lambda *args: b'short'
        with self.assertRaises(RuntimeError): self.run_capture()
        self.assertEqual(self.saves, [])
    def test_viewport2_retains_the_historical_float_to_byte_conversion(self):
        self.renderer = 'viewport2'
        self.run_capture()
        self.assertEqual(self.created_images[0].created, (2, 3, 4, 'float'))
        self.assertEqual(self.created_images[0].converted, 'byte')
    def test_thumbnail_invalid_plan_precedes_overlay_changes(self):
        with self.assertRaises(ValueError): self.scope['captureThumbnail'](self.directory.name, '../escape')
        self.context.assert_not_called()
        self.assertEqual(self.scope['captureThumbnail'](self.directory.name), str(self.path))
        self.context.assert_called_once()


if __name__ == '__main__': unittest.main()
