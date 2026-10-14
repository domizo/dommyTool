#!/usr/bin/env python3
"""Bounded floating interpolation shared by XYZ and Curve domain helpers."""

import math
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.core import geometry
from DPyTools.features.curves import geometry as curves


class InterpolationTests(unittest.TestCase):
    def test_constant_coordinates_remain_exact_at_ordinary_large_and_tiny_scales(self):
        for value in (0.1, 3.0, 1e12, 1e308, sys.float_info.max,
                      math.nextafter(0.0, 1.0)):
            for fraction in (0, 0.01, 0.07, 0.18, 0.5, 0.93, 1):
                with self.subTest(value=value, fraction=fraction):
                    result = geometry.interpolate_position(
                        (value, 0, value), (value, 1, value), fraction)
                    self.assertEqual(result[0], value)
                    self.assertEqual(result[2], value)

    def test_narrow_curve_domain_never_escapes_its_endpoints(self):
        for minimum in (3.0, 0.1, 1e12, 1e308, -3.0, -1e308):
            maximum = math.nextafter(minimum, math.inf)
            for fraction in (0, 0.01, 0.07, 0.18, 0.5, 0.93, 1):
                result = curves.domain_parameter(minimum, maximum, fraction)
                self.assertLessEqual(minimum, result)
                self.assertLessEqual(result, maximum)

    def test_scalar_endpoints_and_equal_values_are_preserved(self):
        for first, second in ((-1e308, 1e308), (3, -7), (-0.0, 0.0)):
            self.assertEqual(geometry.interpolate_scalar(first, second, 0), first)
            self.assertEqual(geometry.interpolate_scalar(first, second, 1), second)
        tiny = math.nextafter(0.0, 1.0)
        self.assertEqual(geometry.interpolate_scalar(tiny, tiny, 0.5), tiny)
        self.assertEqual(math.copysign(1, geometry.interpolate_scalar(-0.0, 0.0, 0)), -1)
        self.assertEqual(math.copysign(1, geometry.interpolate_scalar(-0.0, 0.0, 1)), 1)

    def test_wide_increasing_and_decreasing_interpolation_is_finite_bounded_and_monotonic(self):
        for first, second in ((-sys.float_info.max, sys.float_info.max),
                              (sys.float_info.max, -sys.float_info.max),
                              (1e308, 1.1e308), (-1e308, -1.1e308),
                              (5e-324, 1e-323), (0.1, 0.2)):
            results = [geometry.interpolate_scalar(first, second, index / 100.0)
                       for index in range(101)]
            self.assertTrue(all(math.isfinite(value) for value in results))
            self.assertTrue(all(min(first, second) <= value <= max(first, second)
                                for value in results))
            self.assertEqual(results, sorted(results, reverse=first > second))
        self.assertEqual(geometry.interpolate_scalar(-1e308, 1e308, 0.5), 0)

    def test_invalid_scalars_and_fractions_are_rejected_even_at_endpoints(self):
        invalid = (True, '3', None, math.nan, math.inf, 10 ** 400)
        for value in invalid:
            for args in ((value, 3, 0), (3, value, 1), (3, 4, value)):
                with self.subTest(args=args), self.assertRaises((TypeError, ValueError)):
                    geometry.interpolate_scalar(*args)
        for fraction in (-0.01, 1.01):
            with self.assertRaises(ValueError):
                geometry.interpolate_scalar(3, 3, fraction)

    def test_xyz_and_curve_use_the_same_scalar_implementation(self):
        with patch.object(geometry, 'interpolate_scalar', return_value=3) as scalar:
            self.assertEqual(geometry.interpolate_position((1, 2, 3), (4, 5, 6), 0.5), (3, 3, 3))
            self.assertEqual(scalar.call_count, 3)
            scalar.reset_mock()
            self.assertEqual(curves.domain_parameter(1, 4, 0.5), 3)
            scalar.assert_called_once_with(1.0, 4.0, 0.5)


if __name__ == '__main__':
    unittest.main()
