#!/usr/bin/env python3
"""Unit tests for the host-independent per-user asset settings schema."""

import os
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import settings


class AssetSettingsTests(unittest.TestCase):
    def test_defaults_are_complete_and_versioned(self):
        root = os.path.abspath(tempfile.gettempdir())
        data = settings.default_settings(root, "artist")
        self.assertEqual(data[settings.SCHEMA_VERSION_KEY], 1)
        self.assertEqual(data[settings.ROOT_PATH_KEY], root)
        self.assertEqual(data[settings.CURRENT_PROJECT_KEY], "")
        self.assertFalse(data[settings.NOTIFICATION_KEY])

    def test_legacy_partial_mapping_is_normalized(self):
        data = settings.validate_settings({settings.CURRENT_PROJECT_KEY: "Project A"})
        self.assertEqual(data[settings.SCHEMA_VERSION_KEY], 1)
        self.assertEqual(data[settings.CURRENT_PROJECT_KEY], "Project A")

    def test_unknown_keys_and_invalid_types_are_rejected(self):
        with self.assertRaises(ValueError):
            settings.validate_settings({"unexpected": True})
        with self.assertRaises(TypeError):
            settings.validate_settings({settings.NOTIFICATION_KEY: "yes"})
        with self.assertRaises(ValueError):
            settings.validate_settings({settings.ROOT_PATH_KEY: "relative/path"})

    def test_project_and_asset_type_names_are_safe_and_unique(self):
        valid = settings.validate_settings({
            settings.CURRENT_PROJECT_KEY: "Project A",
            settings.ASSET_TYPE_KEY: ["CH", "PROPS"],
        })
        self.assertEqual(valid[settings.ASSET_TYPE_KEY], ["CH", "PROPS"])
        with self.assertRaises(ValueError):
            settings.validate_settings({settings.CURRENT_PROJECT_KEY: "../outside"})
        with self.assertRaises(ValueError):
            settings.validate_settings({settings.ASSET_TYPE_KEY: ["CH", "CH"]})


if __name__ == "__main__":
    unittest.main()
