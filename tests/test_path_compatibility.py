#!/usr/bin/env python3
"""Host-stubbed tests for the legacy DPathOp compatibility boundary."""

import importlib
import os
import shutil
import sys
import tempfile
import types
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))


class PathCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        core = types.ModuleType("pymel.core")
        pymel = types.ModuleType("pymel")
        pymel.core = core
        cls.previous = {
            name: sys.modules.get(name)
            for name in ("pymel", "pymel.core", "DPyTools.system.DPathOp")
        }
        sys.modules["pymel"] = pymel
        sys.modules["pymel.core"] = core
        sys.modules.pop("DPyTools.system.DPathOp", None)
        cls.dpath = importlib.import_module("DPyTools.system.DPathOp")

    @classmethod
    def tearDownClass(cls):
        for name, module in cls.previous.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="dommytool-path-compat-")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_non_mutating_helpers_keep_historical_results(self):
        filename = self.dpath.append(self.root, "asset.ma")
        self.assertEqual(self.dpath.lastComponent(filename), "asset.ma")
        self.assertEqual(self.dpath.extension(filename), ".ma")
        self.assertEqual(self.dpath.extention(filename), ".ma")
        self.assertTrue(self.dpath.isWithin(filename, self.root))
        self.assertTrue(self.dpath.isWithin(filename, os.path.abspath(os.sep)))

    def test_file_deletion_delegates_to_containment_aware_core(self):
        filename = os.path.join(self.root, "temporary.txt")
        Path(filename).write_text("temporary")
        self.dpath.delFile(filename, root=self.root)
        self.assertFalse(os.path.exists(filename))

    def test_mutating_legacy_api_is_explicit(self):
        self.assertEqual(
            self.dpath.LEGACY_MUTATING_APIS,
            ("chdir", "mkEmptyFile", "checkDirectory", "copyFile"),
        )


if __name__ == "__main__":
    unittest.main()
