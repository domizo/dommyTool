#!/usr/bin/env python3
"""Host-free regression checks for archived human-rig joint specifications."""

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / 'python'))

from DPyTools.features.rigging.human import specs


class HumanRigSpecTests(unittest.TestCase):
    def test_all_joint_lists_are_mirrored(self):
        self.assertTrue(specs.validate_mirrored_joint_lists())

    def test_right_thumb_ends_with_right_tip(self):
        self.assertEqual(specs.RIGHT_THUMB[-1], 'thumb_d_R')

    def test_mismatch_is_rejected(self):
        with self.assertRaises(ValueError):
            specs.validate_mirrored_joint_lists(((('jointL',), ('wrongR',)),))

    def test_joint_positions_are_mirrored_across_yz(self):
        left = (
            ('shoulderL', (3, 10, -2)),
            ('wrist_L', (8.5, 9, 1)),
        )
        right = (
            ('shoulderR', (-3, 10, -2)),
            ('wrist_R', (-8.5, 9, 1)),
        )
        self.assertEqual(specs.mirrored_position_items(left), right)
        self.assertTrue(specs.validate_mirrored_position_items(left, right))

    def test_position_mismatch_is_rejected(self):
        with self.assertRaises(ValueError):
            specs.validate_mirrored_position_items(
                (('jointL', (1, 2, 3)),),
                (('jointR', (1, 2, 3)),))


if __name__ == '__main__':
    unittest.main()
