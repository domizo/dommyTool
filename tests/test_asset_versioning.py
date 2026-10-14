#!/usr/bin/env python3
"""Unit tests for collision-resistant asset version naming."""

import sys
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import versioning


class AssetVersioningTests(unittest.TestCase):
    def test_next_version_uses_greatest_valid_value(self):
        names = ["v001", "thumbnail.png", "notes", "v010", "vnotanumber"]
        self.assertEqual(versioning.next_version(names), "v011")

    def test_gaps_and_non_version_entries_do_not_cause_collisions(self):
        self.assertEqual(versioning.next_version(["v001", "v003"]), "v004")
        self.assertEqual(versioning.next_version([]), "v001")

    def test_parser_rejects_path_like_and_out_of_range_values(self):
        self.assertIsNone(versioning.parse_version("../v001"))
        self.assertIsNone(versioning.parse_version("v1000000"))
        self.assertIsNone(versioning.parse_version(None))

    def test_parser_requires_the_complete_string(self):
        for name in ('v001\n', 'v001\r\n', 'v001\t', ' v001', 'v001 ',
                     'v001\x00', 'v\n001', 'v001/extra'):
            with self.subTest(name=name):
                self.assertIsNone(versioning.parse_version(name))

    def test_valid_legacy_widths_zero_and_maximum_remain_accepted(self):
        cases = {'v000': 0, 'v1': 1, 'v001': 1, 'v999999': 999999,
                 'v' + '0' * 30 + '1': 1}
        for name, expected in cases.items():
            self.assertEqual(versioning.parse_version(name), expected)
        self.assertIsNone(versioning.parse_version('v' + '0' * 31 + '1'))

    def test_generated_names_always_fit_the_shared_parser(self):
        for width in (1, 3, 31):
            name = versioning.next_version(['v001', 'v010'], width=width)
            self.assertEqual(versioning.parse_version(name), 11)
        for width in (0, -1, True, 1.5, '3', 32):
            with self.subTest(width=width):
                with self.assertRaises(ValueError):
                    versioning.next_version([], width=width)
        with patch.object(versioning, 'str', side_effect=AssertionError('Formatting must not start'), create=True):
            with self.assertRaises(ValueError):
                versioning.next_version([], width=10 ** 18)

    def test_invalid_trailing_newline_does_not_consume_version_limit(self):
        self.assertEqual(versioning.next_version(['v001', 'v999999\n']), 'v002')
        with self.assertRaises(ValueError):
            versioning.next_version(['v999999'])


if __name__ == "__main__":
    unittest.main()
