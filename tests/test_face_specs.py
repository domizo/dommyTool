#!/usr/bin/env python3
"""Tests for Maya-independent, immutable face-rig naming specifications."""

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.features.rigging.face import specs


class FaceRigSpecsTests(unittest.TestCase):
    def test_specification_is_valid_and_immutable_by_convention(self):
        self.assertTrue(specs.validate_specs())
        self.assertIsInstance(specs.EYE_LOOK_ALL, tuple)
        self.assertIsInstance(specs.FACE_BLEND_SHAPE_GROUPS, tuple)
        self.assertIsInstance(specs.FACE_BLEND_SHAPES_BY_GROUP, tuple)
        for group_name, shape_names in specs.FACE_BLEND_SHAPES_BY_GROUP:
            self.assertIsInstance(group_name, str)
            self.assertIsInstance(shape_names, tuple)

    def test_eye_look_order_preserves_left_right_pairs(self):
        expected = tuple(
            name
            for pair in zip(specs.EYE_LOOK_LEFT, specs.EYE_LOOK_RIGHT)
            for name in pair
        )
        self.assertEqual(specs.EYE_LOOK_ALL, expected)

    def test_group_mapping_matches_flat_specification(self):
        self.assertEqual(
            specs.FACE_BLEND_SHAPE_LISTS,
            tuple(names for unused_group, names in specs.FACE_BLEND_SHAPES_BY_GROUP),
        )
        mapped_groups = tuple(
            group for group, unused_names in specs.FACE_BLEND_SHAPES_BY_GROUP
        )
        self.assertNotIn(specs.EYE_LOOK_GROUP, mapped_groups)
        self.assertEqual(
            set(mapped_groups), set(specs.FACE_BLEND_SHAPE_GROUPS[1:])
        )


if __name__ == "__main__":
    unittest.main()
