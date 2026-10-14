#!/usr/bin/env python3
"""Tests for Maya-independent node rename preflight validation."""

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.assets import rename_plan


class RenamePlanTests(unittest.TestCase):
    def test_valid_plan_is_immutable_and_preserves_order(self):
        plan = rename_plan.build_rename_plan(
            ["|asset|old_a", "|asset|old_b"], ["new_a", "new_b"]
        )
        self.assertEqual(
            plan,
            (("|asset|old_a", "new_a"), ("|asset|old_b", "new_b")),
        )
        self.assertEqual(rename_plan.short_name("|asset|old_a"), "old_a")

    def test_count_mismatch_and_empty_plan_are_rejected(self):
        with self.assertRaises(ValueError):
            rename_plan.build_rename_plan(["a"], [])
        with self.assertRaises(ValueError):
            rename_plan.build_rename_plan([], [])

    def test_duplicate_sources_and_targets_are_rejected(self):
        with self.assertRaises(ValueError):
            rename_plan.build_rename_plan(["a", "a"], ["b", "c"])
        with self.assertRaises(ValueError):
            rename_plan.build_rename_plan(["a", "b"], ["c", "c"])

    def test_invalid_target_names_are_rejected(self):
        for target in ("", " name", "name ", "parent|name", "bad\nname"):
            with self.subTest(target=target):
                with self.assertRaises(ValueError):
                    rename_plan.build_rename_plan(["source"], [target])

    def test_namespaces_and_no_op_targets_are_supported(self):
        plan = rename_plan.build_rename_plan(
            ["|asset|character:mesh"], ["character:mesh"]
        )
        self.assertEqual(plan[0][1], rename_plan.short_name(plan[0][0]))


if __name__ == "__main__":
    unittest.main()
