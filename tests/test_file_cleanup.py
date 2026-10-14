#!/usr/bin/env python3
"""Unit tests for the host-independent file-cleaner discovery boundary."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.core import file_cleanup


class FileCleanupTests(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="dommytool-clean-")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_only_exact_extension_is_returned_in_sorted_order(self):
        nested = os.path.join(self.root, "nested")
        os.mkdir(nested)
        targets = [os.path.join(nested, "b.pyc"), os.path.join(self.root, "a.pyc")]
        for filename in targets:
            Path(filename).write_bytes(b"cache")
        Path(os.path.join(self.root, "keep.pyc.bak")).write_bytes(b"keep")
        Path(os.path.join(self.root, "keep.PYC")).write_bytes(b"keep")
        expected = sorted(os.path.realpath(filename) for filename in targets)
        self.assertEqual(file_cleanup.search_files(self.root, ".pyc"), expected)

    def test_invalid_extensions_and_dangerous_roots_are_rejected(self):
        for extension in ("pyc", ".", "../.pyc", ".pyc\n"):
            with self.subTest(extension=extension):
                with self.assertRaises((TypeError, ValueError)):
                    file_cleanup.validate_extension(extension)
        with self.assertRaises(ValueError):
            file_cleanup.validate_root(os.path.expanduser("~"))

    def test_symbolic_link_files_and_directories_are_skipped(self):
        target = os.path.join(self.root, "target.pyc")
        linked_file = os.path.join(self.root, "linked.pyc")
        external = tempfile.mkdtemp(prefix="dommytool-clean-external-")
        linked_dir = os.path.join(self.root, "linked-directory")
        Path(target).write_bytes(b"target")
        Path(os.path.join(external, "external.pyc")).write_bytes(b"external")
        try:
            os.symlink(target, linked_file)
            os.symlink(external, linked_dir)
        except (AttributeError, NotImplementedError, OSError):
            shutil.rmtree(external, ignore_errors=True)
            self.skipTest("Symbolic links are not available")
        try:
            self.assertEqual(
                file_cleanup.search_files(self.root, ".pyc"),
                [os.path.realpath(target)],
            )
        finally:
            shutil.rmtree(external, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
