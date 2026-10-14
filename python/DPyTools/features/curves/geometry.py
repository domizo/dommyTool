# -*- coding: utf-8 -*-
u"""Maya-independent admission and arithmetic for open NURBS curve helpers."""

import numbers

from DPyTools.core.numeric import finite_number
from DPyTools.core import geometry as core_geometry

# Keep the archived import as the same function, not a second validator.
position3 = core_geometry.position3


SUPPORTED_DEGREES = (1, 2, 3, 5, 7)
CLOSEST_POINT_TOLERANCE = 1e-8


def open_uniform_knots(point_count, degree):
	u"""Build Maya's clamped open knot vector (CV count + degree - 1 values)."""
	if isinstance(degree, bool) or not isinstance(degree, numbers.Integral):
		raise TypeError('Curve degree must be an integer')
	if degree not in SUPPORTED_DEGREES:
		raise ValueError('Curve degree must be one of {0}'.format(SUPPORTED_DEGREES))
	if isinstance(point_count, bool) or not isinstance(point_count, numbers.Integral):
		raise TypeError('Curve point count must be an integer')
	if point_count < degree + 1:
		raise ValueError('Curve needs at least degree + 1 points')
	end = point_count - degree
	return tuple([0] * (degree - 1) + list(range(end + 1)) + [end] * (degree - 1))


def domain_parameter(minimum, maximum, normalized):
	u"""Map a finite [0, 1] parameter to a finite, non-degenerate knot domain."""
	minimum = finite_number(minimum, 'Curve domain minimum')
	maximum = finite_number(maximum, 'Curve domain maximum')
	normalized = finite_number(normalized, 'Normalized curve parameter')
	if minimum >= maximum:
		raise ValueError('Curve domain must be increasing and non-degenerate')
	if not 0.0 <= normalized <= 1.0:
		raise ValueError('Normalized curve parameter must be within [0, 1]')
	return core_geometry.interpolate_scalar(minimum, maximum, normalized)
