#!/usr/bin/env python3
"""Unit tests for host-free face-rig data models."""

import ast
import functools
import sys
import tempfile
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.features.rigging.face import models
from DPyTools.core import json_store


@functools.lru_cache(maxsize=1)
def _face_persistence_code():
    filename = REPO_ROOT / 'python/DPyTools/rigging/DFaceRigOp.py'
    converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
    converted = str(converter.refactor_string(filename.read_text(encoding='utf-8') + '\n', str(filename)))
    ui_class = next(node for node in ast.parse(converted).body
                    if isinstance(node, ast.ClassDef) and node.name == 'DFaceRiggingTool')
    methods = [node for node in ui_class.body
               if isinstance(node, ast.FunctionDef)
               and node.name in ('readExprFileJson', 'saveExprData')]
    module = ast.fix_missing_locations(ast.Module(body=methods, type_ignores=[]))
    return compile(module, str(filename), 'exec')


class FaceModelTests(unittest.TestCase):
    def test_expression_round_trip(self):
        controller = models.ControllerInfo("mouth_ctrl", "slider")
        controller.add_attribute_value("translateX", 2.5)
        expression = models.FaceExpression("smile")
        expression.add_controller_info(controller)

        restored = models.FaceExpression()
        self.assertTrue(restored.set_json_data(expression.to_dict()))
        self.assertEqual(restored.to_dict(), expression.to_dict())

    def test_invalid_data_does_not_partially_replace_expression(self):
        expression = models.FaceExpression("original")
        invalid = {
            "name": "invalid",
            "cntlr_list": [{"cntlr_name": "control"}],
        }
        self.assertFalse(expression.set_json_data(invalid))
        self.assertEqual(expression.m_name, "original")
        self.assertEqual(expression.m_controller_list, [])

    def test_historical_names_remain_available(self):
        self.assertIs(models.DValue, models.Value)
        self.assertIs(models.DControllerInfo, models.ControllerInfo)
        self.assertIs(models.DFaceExpr, models.FaceExpression)

    def test_collection_is_validated_before_results_are_returned(self):
        valid = {
            "expr_list": [{
                "name": "smile",
                "cntlr_list": [{
                    "cntlr_name": "mouth_ctrl",
                    "cntlr_type": "slider",
                    "attr_list": {"translateX": 1.0},
                }],
            }],
        }
        expressions = models.expressions_from_collection(valid)
        self.assertEqual([item.m_name for item in expressions], ["smile"])

        invalid = {
            "expr_list": valid["expr_list"] + [{"name": "broken"}],
        }
        with self.assertRaises(ValueError):
            models.expressions_from_collection(invalid)

    def test_non_finite_and_non_numeric_attributes_are_rejected(self):
        template = {
            "cntlr_name": "mouth_ctrl",
            "cntlr_type": "slider",
            "attr_list": {"translateX": float("nan")},
        }
        self.assertFalse(models.ControllerInfo().set_json_data(template))
        template["attr_list"] = {"translateX": "one"}
        self.assertFalse(models.ControllerInfo().set_json_data(template))
        template["attr_list"] = {"translateX": 10 ** 10000}
        self.assertFalse(models.ControllerInfo().set_json_data(template))

    def test_unicode_names_are_preserved(self):
        controller = models.ControllerInfo("口コントローラ", "スライダー")
        expression = models.FaceExpression("笑顔")
        expression.add_controller_info(controller)

        collection = models.collection_from_expressions([expression])
        restored = models.expressions_from_collection(collection)
        self.assertEqual(restored[0].m_name, "笑顔")
        self.assertEqual(
            restored[0].m_controller_list[0].m_cntlr_name,
            "口コントローラ")

    def test_current_models_are_revalidated_before_save(self):
        controller = models.ControllerInfo("mouth_ctrl", "slider")
        controller.add_attribute_value("translateX", float("nan"))
        expression = models.FaceExpression("smile")
        expression.add_controller_info(controller)

        with self.assertRaises(ValueError):
            models.collection_from_expressions([expression])

        with self.assertRaises(ValueError):
            models.collection_from_expressions([object()])

    def test_duplicate_attributes_are_rejected_without_mutating_models(self):
        for second_value in (1.0, 2.0):
            with self.subTest(second_value=second_value):
                controller = models.ControllerInfo('mouth_ctrl', 'slider')
                controller.add_attribute_value('translateX', 1.0)
                controller.addAttrValue('translateX', second_value)
                expression = models.FaceExpression('smile')
                expression.add_controller_info(controller)
                with self.assertRaisesRegex(ValueError, 'Duplicate'):
                    controller.to_dict()
                with self.assertRaises(ValueError):
                    models.collection_from_expressions([expression])
                self.assertEqual(controller.getAttrCount(), 2)
                self.assertEqual([item.m_value for item in controller.m_attr_list], [1.0, second_value])

    def test_direct_historical_attribute_mutation_cannot_hide_duplicates(self):
        controller = models.ControllerInfo('mouth_ctrl', 'slider')
        controller.m_attr_list = [models.Value('translateX', 1), models.Value('translateX', 2)]
        with self.assertRaises(ValueError):
            controller.genJsonData()

    def test_distinct_unicode_attributes_remain_round_trip_compatible(self):
        controller = models.ControllerInfo('口', 'slider')
        controller.add_attribute_value('笑顔', 1)
        controller.add_attribute_value('口開き', 2)
        expression = models.FaceExpression('smile')
        expression.add_controller_info(controller)
        restored = models.expressions_from_collection(
            models.collection_from_expressions([expression]))[0]
        self.assertEqual(restored.m_controller_list[0].to_dict(), controller.to_dict())

    def test_actual_load_body_keeps_ui_state_on_invalid_json(self):
        warnings_seen = []
        namespace = {'pm': SimpleNamespace(warning=warnings_seen.append),
                     'json_store': json_store, 'face_models': models}
        exec(_face_persistence_code(), namespace)
        original = [models.FaceExpression('original')]
        ui = SimpleNamespace(m_face_expression_list=original, m_mouth_expression_list=original,
                             clearAllFaceData=Mock(), clearAllMouthData=Mock(),
                             updateFaceListUI=Mock(), updateMouthListUI=Mock())
        payloads = ('{"expr_list":[],"expr_list":[]}',
                    '{"expr_list":' + '[' * 2000 + '0' + ']' * 2000 + '}')
        with tempfile.TemporaryDirectory(prefix='dommytool-face-json-') as directory:
            filename = Path(directory) / 'expression.json'
            for payload in payloads:
                filename.write_text(payload, encoding='utf-8')
                for parts_type in (0, 1):
                    with self.subTest(parts_type=parts_type):
                        self.assertFalse(namespace['readExprFileJson'](ui, str(filename), parts_type))
                        self.assertIs(ui.m_face_expression_list, original)
                        self.assertIs(ui.m_mouth_expression_list, original)
        self.assertEqual(len(warnings_seen), 4)
        for method in (ui.clearAllFaceData, ui.clearAllMouthData,
                       ui.updateFaceListUI, ui.updateMouthListUI):
            method.assert_not_called()

    def test_actual_save_body_rejects_duplicates_before_dialog_or_write(self):
        controller = models.ControllerInfo('mouth_ctrl', 'slider')
        controller.add_attribute_value('translateX', 1)
        controller.add_attribute_value('translateX', 2)
        expression = models.FaceExpression('smile')
        expression.add_controller_info(controller)
        warnings_seen = []
        dialog, dump = Mock(), Mock()
        namespace = {'pm': SimpleNamespace(warning=warnings_seen.append, fileDialog2=dialog),
                     'json_store': SimpleNamespace(dump=dump), 'face_models': models}
        exec(_face_persistence_code(), namespace)
        ui = SimpleNamespace(m_face_expression_list=[expression], m_mouth_expression_list=[expression])
        for parts_type in (0, 1):
            self.assertFalse(namespace['saveExprData'](ui, parts_type))
        self.assertEqual(len(warnings_seen), 2)
        dialog.assert_not_called()
        dump.assert_not_called()


if __name__ == "__main__":
    unittest.main()
