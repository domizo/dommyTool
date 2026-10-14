#!/usr/bin/env python3
"""Unit tests for host-free animation file conversion."""

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.features.animation import format as animation_format


class AnimationFormatTests(unittest.TestCase):
    def test_infinity_modes(self):
        self.assertEqual(animation_format.infinity_name(1), "linear")
        self.assertEqual(animation_format.infinity_name("4"), "cycleRelative")
        self.assertEqual(animation_format.infinity_name(999), "constant")

    def test_static_values(self):
        self.assertIs(animation_format.parse_static_value("True;"), True)
        self.assertIs(animation_format.parse_static_value("False"), False)
        self.assertEqual(animation_format.parse_static_value(" -2.5; "), -2.5)

    def test_non_finite_values_are_rejected(self):
        for value in ("nan", "inf", "-inf"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    animation_format.parse_static_value(value)

    def test_dag_name_mapping_is_segment_aware(self):
        self.assertEqual(
            animation_format.remap_node_name(
                '|root|control', has_parent=True, paths=True,
                namespace='character'),
            '|character:root|character:control')
        self.assertEqual(
            animation_format.remap_node_name(
                '|root|control', has_parent=True, paths=True,
                search='control', replace='hand', prefix='copy_'),
            '|copy_root|copy_hand')

    def test_top_only_prefix_preserves_child_short_names(self):
        self.assertEqual(
            animation_format.remap_node_name(
                '|root|control', has_parent=True, paths=True,
                prefix='copy_', prefix_top_only=True),
            '|copy_root|control')
        self.assertEqual(
            animation_format.remap_node_name(
                'control', has_parent=True, paths=False,
                prefix='copy_', prefix_top_only=True),
            'control')
        self.assertEqual(
            animation_format.remap_node_name(
                'root', has_parent=False, paths=False,
                prefix='copy_', prefix_top_only=True),
            'copy_root')

    def test_channel_scope_uses_shell_patterns(self):
        self.assertTrue(animation_format.channel_matches(
            '|root|hand.translateX', '*hand.translate?'))
        self.assertFalse(animation_format.channel_matches(
            '|root|hand.visibility', '*hand.translate?'))
        self.assertTrue(animation_format.channel_matches('anything', ''))


if __name__ == "__main__":
    unittest.main()
