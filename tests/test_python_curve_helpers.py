#!/usr/bin/env python3
"""Curve arithmetic plus actual host adapters with controlled PyMEL/API stubs."""

import ast
import math
import sys
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.features.curves import geometry


class CurveGeometryTests(unittest.TestCase):
    def test_knot_count_order_and_endpoint_multiplicity(self):
        for degree in geometry.SUPPORTED_DEGREES:
            for count in range(degree + 1, 26):
                with self.subTest(degree=degree, count=count):
                    knots = geometry.open_uniform_knots(count, degree)
                    self.assertEqual(len(knots), count + degree - 1)
                    self.assertEqual(knots, tuple(sorted(knots)))
                    self.assertEqual(knots[:degree], (0,) * degree)
                    self.assertEqual(knots[-degree:], (count - degree,) * degree)

    def test_historical_linear_and_cubic_knots_are_preserved(self):
        for count in range(4, 51):
            self.assertEqual(geometry.open_uniform_knots(count, 1), tuple(range(count)))
            self.assertEqual(geometry.open_uniform_knots(count, 3),
                             tuple([0, 0] + list(range(count - 2)) + [count - 3] * 2))

    def test_invalid_degree_or_insufficient_points_are_rejected(self):
        for degree in (None, True, 1.0, '3', 0, 4, 6, 8, -1):
            with self.subTest(degree=degree), self.assertRaises((TypeError, ValueError)):
                geometry.open_uniform_knots(10, degree)
        for count in (None, True, 3.0, '3', -1, 0, 1, 2, 3):
            with self.subTest(count=count), self.assertRaises((TypeError, ValueError)):
                geometry.open_uniform_knots(count, 3)

    def test_normalized_parameters_include_nonzero_domain_start(self):
        self.assertEqual([geometry.domain_parameter(5, 10, parameter)
                          for parameter in (0, 0.5, 1)], [5, 7.5, 10])
        self.assertEqual(geometry.domain_parameter(-1e308, 1e308, 0.5), 0.0)

    def test_invalid_parameter_or_domain_is_rejected(self):
        for parameter in (-0.1, 1.1, True, None, '0.5', float('nan'), float('inf'), 10 ** 400):
            with self.subTest(parameter=parameter), self.assertRaises((TypeError, ValueError)):
                geometry.domain_parameter(0, 10, parameter)
        for minimum, maximum in ((0, 0), (10, 5), (float('nan'), 10),
                                 (0, float('inf')), (False, 10)):
            with self.assertRaises((TypeError, ValueError)):
                geometry.domain_parameter(minimum, maximum, 0.5)

    def test_position_admits_indexed_vector_without_mutating_input(self):
        point = [1, 2, 3]
        self.assertEqual(geometry.position3(point), (1.0, 2.0, 3.0))
        class Vector:
            def __len__(self):
                return 3
            def __getitem__(self, index):
                return point[index]
        vector = Vector()
        self.assertEqual(geometry.position3(vector), (1.0, 2.0, 3.0))
        self.assertEqual(point, [1, 2, 3])

    def test_bad_positions_fail(self):
        for value in (None, '123', [1, 2], [1, 2, 3, 4], [True, 0, 0],
                      [float('nan'), 0, 0], [10 ** 400, 0, 0], {'x': 1, 'y': 2, 'z': 3}):
            with self.subTest(value=value), self.assertRaises((TypeError, ValueError)):
                geometry.position3(value)


class Shape:
    def type(self):
        return 'nurbsCurve'
    def getAttr(self, name):
        return {'minValue': 5.0, 'maxValue': 10.0}[name]
    def longName(self):
        return '|root|curve|curveShape'


class Transform:
    def __init__(self, shape):
        self.shape = shape
    def type(self):
        return 'transform'
    def getShape(self):
        return self.shape


class Point:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x, self.y, self.z = x, y, z


class ScriptUtil:
    def createFromDouble(self, value):
        self.initial = value
    def asDoublePtr(self):
        return [self.initial]
    @staticmethod
    def setDouble(pointer, value):
        pointer[0] = value
    @staticmethod
    def getDouble(pointer):
        return pointer[0]


class CurveAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        filename = ROOT / 'python/DPyTools/features/curves/tools.py'
        converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
        tree = ast.parse(str(converter.refactor_string(
            filename.read_text(encoding='utf-8') + '\n', str(filename))))
        body = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
        module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
        cls.namespace = {'geometry': geometry}
        exec(compile(module, str(filename), 'exec'), cls.namespace)

    def setUp(self):
        self.shape = Shape()
        self.transform = Transform(self.shape)
        self.created = Mock(return_value='created_curve')
        self.position = Mock(return_value=[5.0, 0.0, 0.0])
        self.xform = Mock(return_value=[1.0, 2.0, 3.0])
        self.warnings = []
        def resolve(value):
            if value == 'curve':
                return self.transform
            if value is None:
                raise ValueError('Missing node')
            return value
        self.pm = SimpleNamespace(PyNode=resolve, curve=self.created,
                                  pointOnCurve=self.position, xform=self.xform,
                                  warning=self.warnings.append)
        self.namespace['pm'] = self.pm
        self.selected_paths = []
        self.closest_calls = []
        outer = self
        class Selection:
            def add(self, path):
                if not isinstance(path, str):
                    raise TypeError('Selection needs a full shape path')
                outer.selected_paths.append(path)
            def getDagPath(self, index, path):
                pass
            def getDependNode(self, index, node):
                pass
        class CurveFunction:
            def __init__(self, path):
                pass
            def closestPoint(self, point, pointer, tolerance, space):
                outer.closest_calls.append((point.x, tolerance, space))
                # Local x=0..10 segment, translated +100 in world coordinates.
                local_x = point.x - 100.0 if space == 'world' else point.x
                pointer[0] = min(10.0, max(0.0, local_x))
                return Point(pointer[0])
        self.namespace['om'] = SimpleNamespace(
            MSelectionList=Selection, MDagPath=object, MObject=object,
            MFnNurbsCurve=CurveFunction, MPoint=Point, MScriptUtil=ScriptUtil,
            MSpace=SimpleNamespace(kObject='object', kWorld='world'))

    def test_domain_mapping_is_used_by_actual_transform_and_shape_adapter(self):
        for curve in (self.transform, self.shape, 'curve'):
            self.assertEqual(self.namespace['getCurveParamPosition'](curve, 0.5), [5.0, 0.0, 0.0])
            self.position.assert_called_with(self.shape, pr=7.5, p=True)

    def test_narrow_domain_adapter_does_not_query_out_of_domain(self):
        minimum, maximum = 3.0, math.nextafter(3.0, math.inf)
        self.shape.getAttr = lambda name: {'minValue': minimum, 'maxValue': maximum}[name]
        self.namespace['getCurveParamPosition'](self.shape, 0.01)
        parameter = self.position.call_args.kwargs['pr']
        self.assertLessEqual(minimum, parameter)
        self.assertLessEqual(parameter, maximum)

    def test_bad_normalized_parameters_do_not_query_a_position(self):
        for parameter in (-1, 2, float('nan'), True):
            with self.assertRaises((TypeError, ValueError)):
                self.namespace['getCurveParamPosition'](self.shape, parameter)
        self.position.assert_not_called()

    def test_invalid_curve_keeps_legacy_warning_false_result(self):
        for curve in (None, Transform(None), SimpleNamespace(type=lambda: 'mesh')):
            self.assertFalse(self.namespace['getCurveParamPosition'](curve, 0.5))
        self.position.assert_not_called()
        self.assertTrue(self.warnings)

    def test_empty_creation_remains_a_noop(self):
        self.assertIsNone(self.namespace['createLinearCurve']())
        self.assertIsNone(self.namespace['createCurveAlongObject']())
        self.created.assert_not_called()

    def test_linear_and_cubic_creation_keep_historical_knots(self):
        points = [(0, 0, 0), (1, 2, 3)]
        self.namespace['createLinearCurve'](points=points)
        self.assertEqual(self.created.call_args.kwargs['k'], (0, 1))
        self.assertEqual(points, [(0, 0, 0), (1, 2, 3)])
        self.namespace['createCurveAlongObject'](objects=['a', 'b', 'c', 'd'], degree=3)
        self.assertEqual(self.created.call_args.kwargs['k'], (0, 0, 0, 1, 1, 1))

    def test_requested_quadratic_degree_is_not_silently_cubic(self):
        self.namespace['createCurveAlongObject'](objects=['a', 'b', 'c'], degree=2)
        self.assertEqual(self.created.call_args.kwargs['d'], 2)
        self.assertEqual(self.created.call_args.kwargs['k'], (0, 0, 1, 1))

    def test_higher_supported_degrees_are_forwarded_with_matching_knots(self):
        for degree in (5, 7):
            self.namespace['createCurveAlongObject'](objects=['node'] * (degree + 1), degree=degree)
            self.assertEqual(self.created.call_args.kwargs['d'], degree)
            self.assertEqual(len(self.created.call_args.kwargs['k']), 2 * degree)

    def test_insufficient_points_fail_before_queries_or_creation(self):
        with self.assertRaises(ValueError):
            self.namespace['createLinearCurve'](points=[(0, 0, 0)])
        for objects in (['a'], ['a', 'b'], ['a', 'b', 'c']):
            with self.assertRaises(ValueError):
                self.namespace['createCurveAlongObject'](objects=objects, degree=3)
        self.created.assert_not_called()
        self.xform.assert_not_called()

    def test_invalid_degrees_fail_before_scene_queries(self):
        for degree in (True, '3', 3.0, 4, 0):
            with self.assertRaises((TypeError, ValueError)):
                self.namespace['createCurveAlongObject'](objects=['a'] * 10, degree=degree)
        self.created.assert_not_called()
        self.xform.assert_not_called()

    def test_bad_world_query_positions_fail_before_creation(self):
        self.xform.side_effect = [[1, 2, 3], [float('nan'), 0, 0]]
        with self.assertRaises(ValueError):
            self.namespace['createCurveAlongObject'](objects=['a', 'b'])
        self.created.assert_not_called()

    def test_world_query_failure_cannot_create_partial_curve(self):
        self.xform.side_effect = RuntimeError('World query failed')
        with self.assertRaises(RuntimeError):
            self.namespace['createCurveAlongObject'](objects=['a', 'b'])
        self.created.assert_not_called()

    def test_closest_point_uses_world_space_shape_path_and_positive_tolerance(self):
        parameter = self.namespace['closestPointOnCurve']([105, 0, 0], self.transform)
        self.assertEqual(parameter, 5.0)
        self.assertEqual(self.selected_paths, [self.shape.longName()])
        self.assertEqual(self.closest_calls, [(105.0, geometry.CLOSEST_POINT_TOLERANCE, 'world')])

    def test_bad_closest_point_input_fails_before_api_query(self):
        for position in (None, [1, 2], [float('nan'), 0, 0], [True, 0, 0]):
            with self.assertRaises((TypeError, ValueError)):
                self.namespace['closestPointOnCurve'](position, self.shape)
        self.assertEqual(self.selected_paths, [])
        self.assertEqual(self.closest_calls, [])

    def test_nonfinite_native_parameter_is_rejected(self):
        def closest(point, pointer, tolerance, space):
            pointer[0] = float('nan')
        self.namespace['om'].MFnNurbsCurve = lambda path: SimpleNamespace(closestPoint=closest)
        with self.assertRaises(ValueError):
            self.namespace['closestPointOnCurve']([105, 0, 0], self.shape)

    def test_native_call_without_parameter_writeback_is_rejected(self):
        self.namespace['om'].MFnNurbsCurve = lambda path: SimpleNamespace(
            closestPoint=lambda *args: Point())
        with self.assertRaises(ValueError):
            self.namespace['closestPointOnCurve']([105, 0, 0], self.shape)

    def test_legacy_curve_facade_preserves_exact_function_identity(self):
        filename = ROOT / 'python/DPyTools/curve/curveTool.py'
        body = [node for node in ast.parse(filename.read_text(encoding='utf-8')).body
                if not isinstance(node, ast.ImportFrom)]
        namespace = {'_implementation': SimpleNamespace(**self.namespace)}
        exec(compile(ast.fix_missing_locations(ast.Module(body=body, type_ignores=[])),
                     str(filename), 'exec'), namespace)
        for name in namespace['__all__']:
            self.assertIs(namespace[name], self.namespace[name])


if __name__ == '__main__':
    unittest.main()
