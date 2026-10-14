#!/usr/bin/env python3
"""Bounded side tokens, DAG/namespace preservation and actual rename adapters."""

import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_rig_safety import functions
from DPyTools.core import strings
from DPyTools.core.geometry import position3


class SideTokenTests(unittest.TestCase):
    def test_all_historical_tokens_at_end_or_middle_are_preserved(self):
        for side in strings.SIDES_WITH_CENTER:
            for name in ('arm' + side, 'arm' + side + '_jnt'):
                self.assertEqual(strings.current_side(name), side)
                self.assertEqual(strings.strip_side(name), name.replace(side, ''))

    def test_ordinary_words_are_not_side_tokens(self):
        for name in ('upper_lip', 'jaw_left', 'cheek_right', 'mesh_root', 'ctrl_center'):
            self.assertIsNone(strings.current_side(name))
            self.assertFalse(strings.contains_side(name, include_center=True))
            self.assertEqual(strings.strip_side(name), name)

    def test_parent_and_namespace_tokens_are_never_used_or_removed(self):
        name = '|parent_l|char_l:sub_r:hand_dr_jnt'
        self.assertEqual(strings.current_side(name), '_dr')
        self.assertEqual(strings.strip_side(name), '|parent_l|char_l:sub_r:hand_jnt')
        self.assertEqual(strings.set_side(name, '_dl'), '|parent_l|char_l:sub_r:hand_dl_jnt')
        self.assertIsNone(strings.current_side('|parent_l|char_r:hand'))

    def test_center_is_excluded_unless_requested(self):
        self.assertFalse(strings.contains_side('jaw_c'))
        self.assertTrue(strings.contains_side('jaw_c', include_center=True))
        self.assertEqual(strings.reverse_side('_c'), '')

    def test_ambiguous_names_are_rejected_by_every_side_operation(self):
        for name in ('arm_l_r', 'arm_l_l', 'face_c_ul'):
            for function in (strings.current_side, strings.strip_side, strings.contains_side):
                with self.assertRaises(ValueError):
                    function(name)
            with self.assertRaises(ValueError):
                strings.set_side(name, '_r')

    def test_setting_side_is_idempotent_and_replaces_in_place(self):
        self.assertEqual(strings.set_side('arm_l_jnt', '_r'), 'arm_r_jnt')
        self.assertEqual(strings.set_side('arm_l', '_l'), 'arm_l')
        self.assertEqual(strings.set_side('upper_lip', '_r'), 'upper_lip_r')

    def test_unicode_and_empty_query_contracts(self):
        self.assertEqual(strings.current_side('|親_l|担当者_r:手_l'), '_l')
        self.assertIsNone(strings.current_side(None))
        self.assertIsNone(strings.current_side(''))
        self.assertEqual(strings.strip_side(''), '')
        for name in (None, 1, ['arm_l']):
            with self.assertRaises(TypeError):
                strings.strip_side(name)
        with self.assertRaises(ValueError):
            strings.set_side('', '_l')


class SideRenameTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = functions('python/DPyTools/scene/naming.py',
                              {'string_utils': strings, 'position3': position3})

    def setUp(self):
        self.node = SimpleNamespace(name=lambda: '|parent_r|char_l:arm_l_jnt')
        self.pm = SimpleNamespace(PyNode=Mock(return_value=self.node),
                                  rename=Mock(return_value='renamed'),
                                  xform=Mock(return_value=[1, 0, 0]))
        self.scope['pm'] = self.pm

    def test_reverse_renames_only_leaf_side_using_absolute_namespace(self):
        self.assertEqual(self.scope['rename_reverse_side']('node', '_l'), 'renamed')
        self.pm.rename.assert_called_once_with(self.node, ':char_l:arm_r_jnt')

    def test_position_rename_preserves_namespace_and_never_duplicates_side(self):
        self.scope['rename_by_position']('node')
        self.pm.rename.assert_called_once_with(self.node, ':char_l:arm_l_jnt')
        self.pm.rename.reset_mock()
        self.pm.xform.return_value = [-1, 0, 0]
        self.scope['rename_by_position']('node', destination='hand')
        self.pm.rename.assert_called_once_with(self.node, ':char_l:hand_r')

    def test_center_and_unsupported_reverse_keep_noop_contract(self):
        self.pm.xform.return_value = [0, 0, 0]
        self.assertIsNone(self.scope['rename_by_position']('node'))
        self.assertIsNone(self.scope['rename_reverse_side'](None, 'invalid'))
        self.pm.rename.assert_not_called()

    def test_ambiguous_destination_fails_before_scene_query_or_rename(self):
        with self.assertRaises(ValueError):
            self.scope['rename_by_position']('node', destination='hand_l_r')
        self.pm.xform.assert_not_called()
        self.pm.rename.assert_not_called()

    def test_nonfinite_position_fails_before_rename(self):
        self.pm.xform.return_value = [float('nan'), 0, 0]
        with self.assertRaises(ValueError):
            self.scope['rename_by_position']('node')
        self.pm.rename.assert_not_called()

    def test_historical_wrappers_still_delegate_and_return_none(self):
        self.assertIsNone(self.scope['renameRevSide']('node', '_l'))
        self.assertIsNone(self.scope['renameLR']('node'))
        self.assertEqual(self.pm.rename.call_count, 2)


if __name__ == '__main__':
    unittest.main()
