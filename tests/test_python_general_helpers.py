#!/usr/bin/env python3
"""Host-free behavior tests for small DPyTools general helpers."""

import importlib
import sys
import types
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))


class GeneralHelperTests(unittest.TestCase):
    def setUp(self):
        pymel = types.ModuleType("pymel")
        pymel.__path__ = []
        core = types.ModuleType("pymel.core")
        util = types.ModuleType("pymel.util")

        self.parents = {}
        self.shapes = {}
        self.selection = []

        def list_relatives(node, **kwargs):
            if kwargs.get("parent"):
                return self.parents.get(node, [])
            if kwargs.get("shapes"):
                return self.shapes.get(node, [])
            return []

        core.listRelatives = list_relatives
        core.ls = lambda **kwargs: list(self.selection)
        core.general = types.SimpleNamespace(PyNode=lambda node: node)
        pymel.core = core
        pymel.util = util

        self.fake_modules = {
            "pymel": pymel,
            "pymel.core": core,
            "pymel.util": util,
        }
        self.previous_modules = {
            name: sys.modules.get(name) for name in self.fake_modules
        }
        sys.modules.update(self.fake_modules)

        for name in (
                "DPyTools.scene.naming",
                "DPyTools.general.stringActions",
                "DPyTools.scene.transforms",
                "DPyTools.general.transformActions"):
            sys.modules.pop(name, None)
        self.core_strings = importlib.import_module("DPyTools.core.strings")
        self.scene_naming = importlib.import_module("DPyTools.scene.naming")
        self.strings = importlib.import_module("DPyTools.general.stringActions")
        self.transforms = importlib.import_module("DPyTools.scene.transforms")
        self.transform_compat = importlib.import_module(
            "DPyTools.general.transformActions")

    def tearDown(self):
        for name in (
                "DPyTools.scene.naming",
                "DPyTools.general.stringActions",
                "DPyTools.scene.transforms",
                "DPyTools.general.transformActions"):
            sys.modules.pop(name, None)
        for name, previous in self.previous_modules.items():
            if previous is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = previous

    def test_string_filters_return_booleans_and_new_lists(self):
        self.assertTrue(self.core_strings.contains("FaceRig", "Rig"))
        self.assertFalse(
            self.core_strings.contains_any("FaceRig", ["mesh", "curve"]))

        source = ["FaceRig", "MeshTool"]
        self.assertEqual(
            self.core_strings.filter_items(source, "rig"), ["FaceRig"])
        result = self.strings.filterTextInList(list=source, filterText="")
        self.assertEqual(result, source)
        self.assertIsNot(result, source)
        self.assertTrue(self.strings.isContain(base="FaceRig", target="Rig"))
        self.assertEqual(
            self.scene_naming.replaceText(
                src="arm_l_jnt", dstDic={"_l_": "_r_"}),
            "arm_r_jnt")

    def test_transform_queries_handle_world_and_missing_shapes(self):
        self.parents["root"] = []
        self.parents["child"] = ["root"]
        self.shapes["mesh"] = ["meshShape"]

        self.assertTrue(self.transforms.isRootTrans("root"))
        self.assertFalse(self.transforms.isRootTrans("child"))
        self.assertFalse(self.transforms.isRootTrans(None))
        self.assertTrue(self.transforms.hasShape("mesh"))
        self.assertFalse(self.transforms.hasShape("root"))
        self.assertIsNone(self.transforms.getRootTransformStr())

        self.assertIs(
            self.transform_compat.isRootTrans, self.transforms.isRootTrans)


if __name__ == "__main__":
    unittest.main()
