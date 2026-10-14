#!/usr/bin/env python3
"""Numeric attribute value shapes and actual legacy default/reset adapters."""

import ast
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.core import numeric
from DPyTools.features.curves import geometry
from DPyTools.scene import attribute_values


class AttributeValueTests(unittest.TestCase):
    def test_intrinsic_limits_match_scalar_and_compound_admission(self):
        self.assertEqual(attribute_values.numeric_limits('bool'), (0, 1))
        self.assertEqual(attribute_values.numeric_limits('byte'), (0, 255))
        self.assertEqual(attribute_values.numeric_limits('short3'), (-32768, 32767))
        self.assertEqual(attribute_values.numeric_limits('float3'), (-attribute_values.FLOAT_MAX, attribute_values.FLOAT_MAX))
        self.assertEqual(attribute_values.numeric_limits('double'), (-sys.float_info.max, sys.float_info.max))
        with self.assertRaises(ValueError): attribute_values.numeric_limits('matrix')
    def test_scalars_keep_boolean_integer_and_finite_float_meaning(self):
        for value in (True, [1.0], (1,)):
            self.assertEqual(attribute_values.numeric_value(value, 'bool'), True)
        self.assertEqual(attribute_values.numeric_value([3.0], 'enum'), 3)
        self.assertEqual(attribute_values.numeric_value(1.25, 'doubleLinear'), 1.25)
        self.assertIs(geometry.finite_number, numeric.finite_number)

    def test_numeric_vectors_normalize_flat_and_wrapped_queries(self):
        for attribute_type in attribute_values.COMPOUND_TYPES:
            width = attribute_values.value_width(attribute_type)
            point = list(range(1, width + 1))
            for value in (point, tuple(point), [tuple(point)]):
                self.assertEqual(attribute_values.numeric_value(value, attribute_type), tuple(point))

    def test_indexed_vector_like_values_preserve_all_components(self):
        class Vector:
            def __len__(self):
                return 3
            def __getitem__(self, index):
                return [1.0, 2.0, 3.0][index]
        self.assertEqual(attribute_values.numeric_value(Vector(), 'double3'), (1.0, 2.0, 3.0))

    def test_integer_and_float_boundaries_are_admitted_without_truncation(self):
        self.assertEqual(attribute_values.numeric_value([-32768, 32767], 'short2'), (-32768, 32767))
        self.assertEqual(attribute_values.numeric_value([0, 255], 'double2'), (0.0, 255.0))
        for value, attribute_type in ((1.5, 'long'), (True, 'long'), (256, 'byte'),
                                     (32768, 'short'), (2147483648, 'long'),
                                     (attribute_values.FLOAT_MAX * 2, 'float')):
            with self.assertRaises((TypeError, ValueError)):
                attribute_values.numeric_value(value, attribute_type)

    def test_malformed_nonfinite_or_unsupported_values_fail(self):
        for value, attribute_type in (([], 'double'), ([1, 2], 'double3'),
                                     ([1, 2, 3, 4], 'double3'), ([True, 0, 0], 'double3'),
                                     ([float('nan'), 0, 0], 'double3'), ('123', 'double3'),
                                     (float('inf'), 'double'), (10 ** 400, 'double'),
                                     (2, 'bool'), ([1, 2, 3], 'compound'), (None, 'matrix')):
            with self.subTest(value=value, attribute_type=attribute_type), \
                    self.assertRaises((TypeError, ValueError)):
                attribute_values.numeric_value(value, attribute_type)


class AttributeAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        filename = ROOT / 'python/DPyTools/scene/attributes.py'
        tree = ast.parse(filename.read_text(encoding='utf-8'))
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
        module = ast.fix_missing_locations(ast.Module(body=functions, type_ignores=[]))
        cls.namespace = {'attribute_values': attribute_values}
        exec(compile(module, str(filename), 'exec'), cls.namespace)

    def setUp(self):
        self.attribute_type = 'double3'
        self.default = [1.0, 2.0, 3.0]
        self.current = [(1.0, 2.0, 3.0)]
        self.setter = Mock()
        self.pm = SimpleNamespace(attributeQuery=lambda *args, **kw: self.default,
                                  getAttr=lambda *args, **kw: self.attribute_type if kw.get('type') else self.current,
                                  setAttr=self.setter)
        self.namespace['pm'] = self.pm

    def test_reset_submits_all_components_with_explicit_type(self):
        self.namespace['setAttrDefaultValue']('node', 'compound')
        self.setter.assert_called_once_with('node.compound', 1.0, 2.0, 3.0, type='double3')

    def test_equal_compound_values_are_recognized_and_changes_are_not(self):
        self.assertTrue(self.namespace['isStaticAttribute']('node', 'compound'))
        self.current = [(1.0, 2.0, 4.0)]
        self.assertFalse(self.namespace['isStaticAttribute']('node', 'compound'))

    def test_scalar_reset_and_raw_default_query_remain_compatible(self):
        self.attribute_type = 'double'
        self.default = [3.5]
        self.current = 3.5
        self.assertIs(self.namespace['getAttrDefaultValue']('node', 'scalar'), self.default)
        self.namespace['setAttrDefaultValue']('node', 'scalar')
        self.setter.assert_called_once_with('node.scalar', 3.5)
        self.assertTrue(self.namespace['isStaticAttribute']('node', 'scalar'))

    def test_bad_defaults_or_unsupported_types_do_not_write(self):
        for attribute_type, default in (('matrix', [0] * 16), ('double3', [1, 2]),
                                        ('double3', [1, 2, float('nan')]), ('long', [1.5])):
            self.attribute_type, self.default = attribute_type, default
            with self.assertRaises((TypeError, ValueError)):
                self.namespace['setAttrDefaultValue']('node', 'value')
        self.setter.assert_not_called()

    def test_query_errors_and_interrupts_are_not_hidden(self):
        for error in (RuntimeError('query failed'), KeyboardInterrupt(), MemoryError('simulated')):
            self.pm.attributeQuery = Mock(side_effect=error)
            with self.assertRaises(type(error)):
                self.namespace['setAttrDefaultValue']('node', 'value')
        self.setter.assert_not_called()

    def test_legacy_facade_keeps_exact_public_function_identity(self):
        filename = ROOT / 'python/DPyTools/general/attributeActions.py'
        body = [node for node in ast.parse(filename.read_text(encoding='utf-8')).body
                if not isinstance(node, ast.ImportFrom)]
        context = {'_implementation': SimpleNamespace(**self.namespace)}
        exec(compile(ast.fix_missing_locations(ast.Module(body=body, type_ignores=[])),
                     str(filename), 'exec'), context)
        for name in context['__all__']:
            self.assertIs(context[name], self.namespace[name])


if __name__ == '__main__':
    unittest.main()
