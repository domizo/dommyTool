#!/usr/bin/env python3
"""Actual Human Rig export cancellation/admission and shared selection restore."""

import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from DPyTools.ui import table_selection


class HumanExportTests(unittest.TestCase):
    def setUp(self):
        self.selected = ['borrowed']
        self.pm = NS(objExists=Mock(return_value=True), fileDialog2=Mock(return_value=['rig.ma']),
                     PyNode=Mock(return_value='root-handle'), ls=Mock(side_effect=lambda **kw: list(self.selected)),
                     select=Mock(side_effect=self.select), exportSelected=Mock(), warning=Mock())
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {'pm': self.pm}))
        self.scope = functions('python/DPyTools/rigging/humanRigTool.py', {
            'pm': self.pm, 'CharaGroupTop': 'RigGrp', 'scene_selection': self.selection,
            'table_selection': table_selection})
    def select(self, value=None, **kw):
        self.selected = [] if kw.get('clear') else (list(value) if isinstance(value, (list, tuple)) else [value])
    def run_export(self): return self.scope['exportRigSet']()
    def test_cancel_or_empty_choice_does_not_touch_selection(self):
        for files in (None, [], (), ''):
            self.pm.fileDialog2.return_value = files
            self.assertFalse(self.run_export())
        self.pm.select.assert_not_called()
        self.pm.exportSelected.assert_not_called()
        self.assertEqual(self.selected, ['borrowed'])
    def test_malformed_or_control_character_choice_precedes_scene_edits(self):
        for files in (['a.ma', 'b.ma'], True, ['bad\x00path'], [None]):
            self.pm.fileDialog2.return_value = files
            self.assertFalse(self.run_export())
        self.pm.select.assert_not_called()
        self.pm.exportSelected.assert_not_called()
    def test_success_keeps_native_overwrite_option_and_restores_selection(self):
        self.assertTrue(self.run_export())
        self.pm.exportSelected.assert_called_once_with('rig.ma', force=True)
        self.assertEqual(self.selected, ['borrowed'])
    def test_native_failure_or_interruption_restores_retained_selection(self):
        for error in (RuntimeError('export failed'), KeyboardInterrupt()):
            self.pm.exportSelected.side_effect = error
            if isinstance(error, KeyboardInterrupt):
                with self.assertRaises(KeyboardInterrupt): self.run_export()
            else: self.assertFalse(self.run_export())
            self.assertEqual(self.selected, ['borrowed'])
    def test_selection_change_then_throw_still_restores(self):
        native = self.pm.select.side_effect
        def select(value=None, **kw):
            native(value, **kw)
            if value == 'root-handle': raise RuntimeError('selection failed')
        self.pm.select.side_effect = select
        self.assertFalse(self.run_export())
        self.assertEqual(self.selected, ['borrowed'])
        self.pm.exportSelected.assert_not_called()
    def test_missing_root_or_bad_query_does_not_export(self):
        self.pm.objExists.return_value = False
        self.assertFalse(self.run_export())
        self.pm.fileDialog2.assert_not_called()
        self.pm.objExists.return_value = True
        self.pm.ls.side_effect = RuntimeError('query failed')
        self.assertFalse(self.run_export())
        self.pm.select.assert_not_called()
    def test_failed_restoration_is_not_a_success_or_masked_interruption(self):
        self.selection.restore_selection = Mock(return_value=False)
        self.assertFalse(self.run_export())
        self.pm.exportSelected.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.run_export()


if __name__ == '__main__': unittest.main()
