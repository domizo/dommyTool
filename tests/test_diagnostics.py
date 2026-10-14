#!/usr/bin/env python3
"""Tests for opt-in DPyTools package diagnostics."""

import io
import importlib
import logging
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.core import diagnostics


class DiagnosticsTests(unittest.TestCase):
    def tearDown(self):
        diagnostics.configure_debug(False)

    def test_debug_output_is_disabled_by_default(self):
        diagnostics.configure_debug(False)
        self.assertFalse(diagnostics.is_debug_enabled())

    def test_debug_output_can_be_enabled_explicitly(self):
        stream = io.StringIO()
        self.assertTrue(diagnostics.configure_debug(True, stream=stream))
        logger = diagnostics.get_logger("pipeline.assets")
        logger.debug("asset %s", "ready")
        self.assertIn(
            "DPyTools.pipeline.assets DEBUG: asset ready", stream.getvalue()
        )

    def test_existing_package_module_name_is_not_duplicated(self):
        logger = diagnostics.get_logger("DPyTools.utility.renamer")
        self.assertEqual(logger.name, "DPyTools.utility.renamer")

    def test_reload_does_not_accumulate_handlers(self):
        package_logger = logging.getLogger(diagnostics.LOGGER_NAME)
        importlib.reload(diagnostics)
        null_handlers = [
            handler
            for handler in package_logger.handlers
            if getattr(handler, "_dpytools_null_handler", False)
        ]
        debug_handlers = [
            handler
            for handler in package_logger.handlers
            if getattr(handler, "_dpytools_debug_handler", False)
        ]
        self.assertEqual(len(null_handlers), 1)
        self.assertEqual(debug_handlers, [])
        self.assertTrue(package_logger.propagate)


if __name__ == "__main__":
    unittest.main()
