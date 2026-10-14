#!/usr/bin/env python3
"""Unit tests for Maya-independent filesystem helpers."""

import os
import ntpath
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.core import paths


class PathTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.mkdtemp(prefix="dommytool-paths-")

    def tearDown(self):
        shutil.rmtree(self.temporary_directory, ignore_errors=True)

    def test_directory_creation_uses_parent(self):
        created = paths.ensure_directory("nested", parent=self.temporary_directory)
        self.assertEqual(created, os.path.join(self.temporary_directory, "nested"))
        self.assertTrue(os.path.isdir(created))

    def test_portable_new_names_reject_reserved_leaves_and_invalid_characters(self):
        names = ('CON', 'con.txt', 'PRN', 'aux', 'nul.png', 'com1', 'LPT9.txt', 'name.',
                 'bad:name', 'a*b', 'a?b', 'a<b', 'a>b', 'a|b', 'a"b')
        for name in names:
            with self.subTest(name=name), self.assertRaises(ValueError):
                paths.validate_portable_component(name)

    def test_portable_admission_keeps_unicode_and_existing_component_bounds(self):
        for name in ('人物', 'hero mesh', 'asset-v001', 'com10', 'conifer', 'x' * 255):
            self.assertEqual(paths.validate_portable_component(name), name)
        for name in (None, True, [], '', '..', '../escape', 'a/b', 'a\\b', ' bad', 'bad\x00name', 'x' * 256):
            with self.assertRaises((TypeError, ValueError)):
                paths.validate_portable_component(name)

    def test_legacy_read_component_admission_is_not_tightened(self):
        for name in ('CON', 'name.', 'bad:name', 'a?b'):
            self.assertEqual(paths.validate_component(name), name)

    def test_delete_file_enforces_root(self):
        allowed = os.path.join(self.temporary_directory, "allowed")
        outside = os.path.join(self.temporary_directory, "outside")
        os.makedirs(allowed)
        os.makedirs(outside)
        outside_file = os.path.join(outside, "test.txt")
        Path(outside_file).write_text("test", encoding="utf-8")

        with self.assertRaises(ValueError):
            paths.delete_file(outside_file, root=allowed)
        self.assertTrue(os.path.isfile(outside_file))

    def test_delete_file_rejects_symlink(self):
        target = os.path.join(self.temporary_directory, "target.txt")
        link = os.path.join(self.temporary_directory, "link.txt")
        Path(target).write_text("test", encoding="utf-8")
        try:
            os.symlink(target, link)
        except (AttributeError, NotImplementedError, OSError):
            self.skipTest("Symbolic links are not available")

        with self.assertRaises(OSError):
            paths.delete_file(link, root=self.temporary_directory)
        self.assertTrue(os.path.isfile(target))

    def test_filesystem_root_contains_itself_and_ordinary_children(self):
        root = os.path.abspath(os.sep)
        self.assertTrue(paths.is_within(root, root))
        self.assertTrue(paths.is_within(self.temporary_directory, root))

    def test_separator_boundary_and_parent_traversal_do_not_admit_siblings(self):
        root = os.path.join(self.temporary_directory, 'allowed')
        child = os.path.join(root, 'nested', 'file.txt')
        self.assertTrue(paths.is_within(child, root + os.sep))
        self.assertFalse(paths.is_within(root + '_sibling/file.txt', root))
        self.assertFalse(paths.is_within(os.path.join(root, '..', 'outside'), root))

    def test_containment_resolves_escaping_symlink(self):
        allowed = Path(self.temporary_directory) / 'allowed'
        outside = Path(self.temporary_directory) / 'outside'
        allowed.mkdir()
        outside.mkdir()
        link = allowed / 'link'
        try:
            link.symlink_to(outside, target_is_directory=True)
        except (AttributeError, NotImplementedError, OSError):
            self.skipTest('Symbolic links are unavailable')
        self.assertFalse(paths.is_within(str(link / 'file.txt'), str(allowed)))

    def test_windows_lexical_drive_and_case_boundaries(self):
        with patch.object(paths, 'os', SimpleNamespace(path=ntpath, sep='\\')):
            self.assertTrue(paths.is_within('C:/projects/asset.ma', 'C:\\'))
            self.assertTrue(paths.is_within('C:/PROJECTS/asset.ma', 'c:/projects'))
            self.assertFalse(paths.is_within('D:/projects/asset.ma', 'C:\\'))
            self.assertFalse(paths.is_within('C:/projects_other/asset.ma', 'C:/projects'))
            self.assertFalse(paths.is_within('C:/projects/../outside.ma', 'C:/projects'))

    def test_windows_lexical_unc_share_boundaries(self):
        with patch.object(paths, 'os', SimpleNamespace(path=ntpath, sep='\\')):
            self.assertTrue(paths.is_within('//server/share/asset.ma', '//server/share/'))
            self.assertTrue(paths.is_within('//SERVER/SHARE/asset.ma', '//server/share'))
            self.assertFalse(paths.is_within('//server/share_other/asset.ma', '//server/share'))


if __name__ == "__main__":
    unittest.main()
