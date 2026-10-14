#!/usr/bin/env python3
"""Unit tests for Maya-independent asset project discovery."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import paths


class AssetPathTests(unittest.TestCase):
    def setUp(self):
        self.project = tempfile.mkdtemp(prefix="dommytool-project-")

    def tearDown(self):
        shutil.rmtree(self.project, ignore_errors=True)

    def test_direct_asset_directory(self):
        expected = os.path.join(self.project, "assets")
        os.mkdir(expected)
        self.assertEqual(paths.asset_root(self.project), expected)
        self.assertEqual(paths.shot_root(self.project), self.project)

    def test_content_directory_precedence_matches_legacy_behavior(self):
        works = os.path.join(self.project, "works")
        three_d = os.path.join(self.project, "3d")
        os.mkdir(works)
        os.mkdir(three_d)
        expected = os.path.join(three_d, "assets")
        os.mkdir(expected)

        self.assertEqual(paths.content_root(self.project), three_d)
        self.assertEqual(paths.asset_root(self.project), expected)

    def test_files_named_like_content_directories_are_ignored(self):
        Path(os.path.join(self.project, "3d")).write_text("not a directory")
        works = os.path.join(self.project, "works")
        os.mkdir(works)
        self.assertEqual(paths.content_root(self.project), works)

    def test_missing_project_returns_empty_paths(self):
        missing = os.path.join(self.project, "missing")
        self.assertEqual(paths.asset_root(missing), "")
        self.assertEqual(paths.shot_root(missing), "")

    def test_selected_project_is_contained_by_configured_root(self):
        selected = paths.project_path(self.project, "project_a")
        self.assertEqual(selected, os.path.join(self.project, "project_a"))
        self.assertEqual(paths.project_path(self.project, "../outside"), "")
        self.assertEqual(paths.project_path(self.project, "/outside"), "")
        self.assertEqual(paths.project_path(self.project, "nested/project"), "")
        self.assertEqual(paths.project_path(self.project, "C:\\outside"), "")
        self.assertEqual(paths.project_path(self.project, "project\nname"), "")
        self.assertEqual(paths.project_path(self.project, 42), "")

    def test_child_path_accepts_a_single_human_readable_name(self):
        self.assertEqual(
            paths.child_path(self.project, "Project Alpha"),
            os.path.join(self.project, "Project Alpha"),
        )

    def test_child_path_rejects_ambiguous_components(self):
        invalid = ("", ".", "..", " name", "name ", "a/b", "a\\b", "C:name")
        for component in invalid:
            with self.subTest(component=component):
                with self.assertRaises((TypeError, ValueError)):
                    paths.child_path(self.project, component)

    def test_directory_listing_excludes_files_hidden_items_and_escapes(self):
        os.mkdir(os.path.join(self.project, "Project B"))
        os.mkdir(os.path.join(self.project, "Project A"))
        os.mkdir(os.path.join(self.project, ".hidden"))
        Path(os.path.join(self.project, "README.txt")).write_text("not a project")
        outside = tempfile.mkdtemp(prefix="dommytool-list-outside-")
        link = os.path.join(self.project, "linked")
        try:
            os.symlink(outside, link)
        except (AttributeError, NotImplementedError, OSError):
            shutil.rmtree(outside, ignore_errors=True)
            self.skipTest("Symbolic links are not available")
        try:
            self.assertEqual(
                paths.list_child_directories(self.project),
                ["Project A", "Project B"],
            )
        finally:
            shutil.rmtree(outside, ignore_errors=True)

    def test_selected_project_rejects_symlink_escape(self):
        outside = tempfile.mkdtemp(prefix="dommytool-project-outside-")
        link = os.path.join(self.project, "linked_project")
        try:
            os.symlink(outside, link)
        except (AttributeError, NotImplementedError, OSError):
            shutil.rmtree(outside, ignore_errors=True)
            self.skipTest("Symbolic links are not available")
        try:
            self.assertEqual(paths.project_path(self.project, "linked_project"), "")
        finally:
            shutil.rmtree(outside, ignore_errors=True)

    def test_content_and_asset_roots_reject_symlink_escapes(self):
        outside = tempfile.mkdtemp(prefix="dommytool-content-outside-")
        link = os.path.join(self.project, "works")
        os.mkdir(os.path.join(outside, "assets"))
        try:
            os.symlink(outside, link)
        except (AttributeError, NotImplementedError, OSError):
            shutil.rmtree(outside, ignore_errors=True)
            self.skipTest("Symbolic links are not available")
        try:
            self.assertEqual(paths.content_root(self.project), self.project)
            self.assertEqual(paths.asset_root(self.project), "")
        finally:
            shutil.rmtree(outside, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
