#!/usr/bin/env python3
"""Translation-stable plane arithmetic with independent overflow fallback."""

import math
import sys
import unittest
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.core import geometry


def adjacent_plane(exponent=30):
    scale = 3.0 * 2 ** exponent
    x = math.nextafter(scale / 2, math.inf)
    next_x = math.nextafter(x, math.inf)
    return (x, 0.0, scale), (next_x, 0.0, scale), (x, 1.0, scale)


class PlaneDirectionTests(unittest.TestCase):
    def assert_vector(self, actual, expected, tolerance=1e-14):
        self.assertEqual(len(actual), 3)
        self.assertTrue(all(math.isfinite(value) for value in actual))
        for value, reference in zip(actual, expected):
            self.assertAlmostEqual(value, reference, delta=tolerance)

    def test_representable_adjacent_points_keep_the_plane_after_translation(self):
        for exponent in (0, 20, 30, 40):
            with self.subTest(exponent=exponent):
                origin, target, pole = adjacent_plane(exponent)
                delta = target[0] - origin[0]
                self.assertGreater(delta, 0.0)
                local = geometry.plane_direction((0, 0, 0), (delta, 0, 0), (0, 1, 0))
                self.assertEqual(geometry.plane_direction(origin, target, pole), local)
                self.assertEqual(local, (0, 0, -1))

    def test_ordinary_oblique_plane_is_translation_invariant(self):
        points = ((0, 0, 0), (4, 2, 2), (3, -6, -2))
        expected = geometry.plane_direction(*points)
        for offset in ((1e12, -1e12, 1e12), (-2 ** 40, 2 ** 40, -2 ** 40)):
            translated = tuple(tuple(value + shift for value, shift in zip(point, offset)) for point in points)
            self.assert_vector(geometry.plane_direction(*translated), expected)

    def test_adjacent_point_case_retains_orientation_on_all_three_axes(self):
        points = adjacent_plane()
        for shift in range(3):
            permuted = tuple(point[shift:] + point[:shift] for point in points)
            normal = (0, 0, -1)
            self.assertEqual(geometry.plane_direction(*permuted), normal[shift:] + normal[:shift])

    def test_original_wide_plane_stays_finite(self):
        points = ((-1e308, 0, 0), (1e308, 0, 0), (0, 1e308, 0))
        self.assertEqual(geometry.plane_direction(*points), (0, 0, -1))

    def test_finite_tiny_up_displacement_survives_overflowing_target_difference(self):
        for tiny in (1.0, 1e-308, 5e-324):
            points = ((1e308, 0, 0), (-1e308, 0, 0), (1e308, tiny, 0))
            self.assertEqual(geometry.plane_direction(*points), (0, 0, 1))

    def test_finite_tiny_target_displacement_survives_overflowing_up_difference(self):
        for tiny in (1.0, 1e-308, 5e-324):
            points = ((1e308, 0, 0), (1e308, tiny, 0), (-1e308, 0, 0))
            self.assertEqual(geometry.plane_direction(*points), (0, 0, -1))

    def test_both_displacements_can_overflow_independently(self):
        points = ((-1e308, -1e308, 0), (1e308, -1e308, 0), (-1e308, 1e308, 0))
        self.assertEqual(geometry.plane_direction(*points), (0, 0, -1))

    def test_finite_transverse_component_is_preserved_when_another_component_overflows(self):
        scale = 3.0 * 2 ** 1022
        y = math.nextafter(scale / 2, math.inf)
        next_y = math.nextafter(y, math.inf)
        points = ((scale, y, 0), (-scale, next_y, 0), (scale, y, scale))
        normal = geometry.plane_direction(*points)
        reference = float(Fraction(next_y - y) / (2 * Fraction(scale)))
        self.assertGreater(reference, 0.0)
        self.assertAlmostEqual(normal[0], -reference, delta=reference * 1e-14)
        self.assertEqual(normal[1:], (-1, 0))

    def test_subnormal_nonzero_displacements_remain_valid(self):
        points = ((5e-324, 0, 5e-324), (1e-323, 0, 5e-324), (5e-324, 5e-324, 5e-324))
        self.assertEqual(geometry.plane_direction(*points), (0, 0, -1))

    def test_reverse_is_exact_sign_reversal_for_translated_and_wide_planes(self):
        for points in (adjacent_plane(), ((-1e308, 0, 0), (1e308, 0, 0), (0, 1e308, 0))):
            normal = geometry.plane_direction(*points)
            self.assertEqual(geometry.plane_direction(*points, reverse=True), tuple(-value for value in normal))

    def test_swap_reverses_and_cyclic_permutation_preserves_orientation(self):
        points = ((-1, 2, 3), (3, 4, 5), (2, -4, 1))
        normal = geometry.plane_direction(*points)
        self.assert_vector(geometry.plane_direction(points[0], points[2], points[1]), tuple(-value for value in normal))
        self.assert_vector(geometry.plane_direction(points[1], points[2], points[0]), normal)

    def test_collapsed_and_collinear_planes_are_still_rejected(self):
        cases = (((0, 0, 0), (0, 0, 0), (0, 0, 0)),
                 ((1, 2, 3), (1, 2, 3), (4, 5, 6)),
                 ((0, 0, 0), (1, 0, 0), (2, 0, 0)),
                 ((0, 0, 0), (-1, 0, 0), (2, 0, 0)),
                 ((0, 0, 0), (1e308, 0, 0), (-1e308, 0, 0)))
        for points in cases:
            with self.subTest(points=points), self.assertRaises(ValueError):
                geometry.plane_direction(*points)

    def test_existing_near_parallel_tolerance_is_not_weakened(self):
        for height in (1e-14, 1e-12):
            with self.assertRaises(ValueError):
                geometry.plane_direction((0, 0, 0), (1, 0, 0), (1, height, 0))
        self.assertEqual(geometry.plane_direction((0, 0, 0), (1, 0, 0), (1, 1e-10, 0)), (0, 0, -1))

    def test_invalid_coordinates_are_rejected_for_every_input_point(self):
        for invalid in ((True, 0, 0), (math.nan, 0, 0), (math.inf, 0, 0),
                        (10 ** 400, 0, 0), (1, 2), None, '123'):
            for index in range(3):
                points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
                points[index] = invalid
                with self.subTest(index=index, invalid=invalid), self.assertRaises((TypeError, ValueError)):
                    geometry.plane_direction(*points)

    def test_oblique_normal_is_unit_and_orthogonal_to_both_displacements(self):
        points = ((2, 3, 4), (6, 5, 6), (5, -3, 2))
        normal = geometry.plane_direction(*points)
        self.assertAlmostEqual(sum(value * value for value in normal), 1.0, delta=1e-14)
        for point in points[1:]:
            vector = tuple(value - origin for value, origin in zip(point, points[0]))
            self.assertAlmostEqual(sum(a * b for a, b in zip(normal, vector)), 0.0, delta=1e-13)

    def test_positive_uniform_scale_preserves_ordinary_plane_orientation(self):
        points = ((-1, 2, 3), (3, 4, 5), (2, -4, 1))
        normal = geometry.plane_direction(*points)
        for scale in (1e-200, 1e-100, 1.0, 1e100, 1e200):
            scaled = tuple(tuple(value * scale for value in point) for point in points)
            self.assert_vector(geometry.plane_direction(*scaled), normal)


if __name__ == '__main__':
    unittest.main()
