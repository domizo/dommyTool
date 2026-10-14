#!/usr/bin/env python3
"""Host-stubbed tests for shared Maya window lifecycle handling."""

import importlib
import sys
import types
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))


class WindowHelpersTests(unittest.TestCase):
    def setUp(self):
        self.existing = {"tool"}
        self.deleted = []
        core = types.ModuleType("pymel.core")

        def window(name, exists=False, **unused):
            return name in self.existing

        def delete_ui(name):
            self.deleted.append(name)
            self.existing.remove(name)

        core.window = window
        core.deleteUI = delete_ui
        pymel = types.ModuleType("pymel")
        pymel.core = core
        self.previous = {
            name: sys.modules.get(name)
            for name in ("pymel", "pymel.core", "DPyTools.ui.windows")
        }
        sys.modules["pymel"] = pymel
        sys.modules["pymel.core"] = core
        sys.modules.pop("DPyTools.ui.windows", None)
        self.windows = importlib.import_module("DPyTools.ui.windows")

    def tearDown(self):
        for name, module in self.previous.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module

    def test_existing_window_is_deleted_once(self):
        self.assertTrue(self.windows.delete_window("tool"))
        self.assertFalse(self.windows.delete_window("tool"))
        self.assertEqual(self.deleted, ["tool"])

    def test_empty_window_name_is_rejected(self):
        with self.assertRaises(ValueError):
            self.windows.delete_window("")


if __name__ == "__main__":
    unittest.main()
