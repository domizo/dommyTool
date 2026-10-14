#!/usr/bin/env python3
"""Tests for Maya-independent asset naming rules."""

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import naming


class AssetNamingTests(unittest.TestCase):
    def test_supported_types_map_to_historical_codes(self):
        expected = {
            "camera": "_cam",
            "joint": "_jnt",
            "mesh": "_geo",
            "nurbsCurve": "_geo",
            "parentConstraint": "_cons",
            "objectSet": "_set",
            "shader": "_mtl",
            "vdbCache": "_vdbcache",
        }
        self.assertEqual(
            {key: naming.object_type_code(key) for key in expected},
            expected)

    def test_missing_and_unknown_types_have_no_code(self):
        self.assertEqual(naming.object_type_code(), "")
        self.assertEqual(naming.object_type_code("unknown"), "")
        self.assertEqual(naming.object_type_code(42), "")


if __name__ == "__main__":
    unittest.main()
