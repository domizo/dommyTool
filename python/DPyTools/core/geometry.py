# -*- coding: utf-8 -*-
u"""Shared finite XYZ admission and scale-safe direction arithmetic."""

import math

from DPyTools.core.numeric import finite_number


def position3(value):
	u"""Return a finite XYZ tuple, including indexed Maya vector-like inputs."""
	try:
		if len(value) != 3:
			raise ValueError('Position must contain exactly three coordinates')
		coordinates = (value[0], value[1], value[2])
	except (TypeError, KeyError, IndexError):
		raise TypeError('Position must be an indexed XYZ sequence')
	return tuple(finite_number(coordinate, 'Position coordinate')
		for coordinate in coordinates)


def direction3(value):
	u"""Normalize a finite nonzero direction without squaring its full scale."""
	coordinates = position3(value)
	scale = max(abs(coordinate) for coordinate in coordinates)
	if scale == 0.0:
		raise ValueError('Direction must not be zero')
	scaled = tuple(coordinate / scale for coordinate in coordinates)
	length = math.sqrt(sum(coordinate * coordinate for coordinate in scaled))
	return tuple(coordinate / length for coordinate in scaled)


def independent_directions(first, second):
	u"""Return normalized nonparallel directions for an aim/up frame."""
	first = direction3(first)
	second = direction3(second)
	cross = (first[1] * second[2] - first[2] * second[1],
		first[2] * second[0] - first[0] * second[2],
		first[0] * second[1] - first[1] * second[0])
	if max(abs(coordinate) for coordinate in cross) <= 1e-12:
		raise ValueError('Aim and up directions must not be parallel')
	return first, second


def interpolate_scalar(first, second, fraction):
	"""Interpolate finite endpoints, retaining equal values and their closed range."""
	first = finite_number(first, 'Interpolation first endpoint')
	second = finite_number(second, 'Interpolation second endpoint')
	fraction = finite_number(fraction, 'Interpolation fraction')
	if not 0.0 <= fraction <= 1.0:
		raise ValueError('Interpolation fraction must be within [0, 1]')
	if fraction == 0.0:
		return first
	if fraction == 1.0:
		return second
	if first == second:
		return first
	difference = second - first
	if math.isinf(difference):
		# Opposite wide endpoints cannot use their unrepresentable difference.
		result = (1.0 - fraction) * first + fraction * second
	elif fraction <= 0.5:
		result = first + fraction * difference
	else:
		result = second - (1.0 - fraction) * difference
	# Roundoff must not turn an admitted fraction into extrapolation.
	minimum, maximum = min(first, second), max(first, second)
	if result < minimum:
		return minimum
	if result > maximum:
		return maximum
	return finite_number(result, 'Interpolated scalar')


def interpolate_position(first, second, fraction):
	"""Interpolate finite XYZ without overflowing a wide endpoint difference."""
	first, second = position3(first), position3(second)
	fraction = finite_number(fraction, 'Position fraction')
	if not 0.0 <= fraction <= 1.0:
		raise ValueError('Position fraction must be within [0, 1]')
	return position3(tuple(interpolate_scalar(a, b, fraction)
		for a, b in zip(first, second)))


def distance3(first, second):
	"""Finite Euclidean distance without squaring full-scale differences."""
	first, second = position3(first), position3(second)
	difference = tuple(a - b for a, b in zip(first, second))
	return finite_number(math.hypot(math.hypot(difference[0], difference[1]), difference[2]),
		'Position distance')


def signed_y_angle_degrees(first, second):
	"""Signed Y rotation between nonzero directions in the XZ plane."""
	first, second = position3(first), position3(second)
	if first[1] != 0.0 or second[1] != 0.0:
		raise ValueError('Y-axis aim rotation requires directions in the XZ plane')
	first, second = direction3(first), direction3(second)
	cross_y = first[2] * second[0] - first[0] * second[2]
	dot = first[0] * second[0] + first[2] * second[2]
	return math.degrees(math.atan2(cross_y, dot))


def _plane_displacement(origin, point):
	"""Keep finite local differences; rescale only an overflowing displacement."""
	difference = tuple(value - start for start, value in zip(origin, point))
	if not any(math.isinf(value) for value in difference):
		return difference
	scale = max(abs(value) for position in (origin, point) for value in position)
	# Finite components already hold the accurate subtraction. Do not subtract
	# their rounded scaled endpoints again just because another component overflowed.
	return tuple(value / scale - start / scale if math.isinf(delta) else delta / scale
		for start, value, delta in zip(origin, point, difference))


def plane_direction(origin, target, pole, reverse=False):
	"""Return a scale-safe nondegenerate plane normal for joint-axis planning."""
	origin, target, pole = position3(origin), position3(target), position3(pole)
	if not any(origin + target + pole):
		raise ValueError('Joint plane is degenerate')
	first, second = independent_directions(
		_plane_displacement(origin, pole), _plane_displacement(origin, target))
	normal = (first[1] * second[2] - first[2] * second[1],
		first[2] * second[0] - first[0] * second[2],
		first[0] * second[1] - first[1] * second[0])
	if reverse:
		normal = tuple(-coordinate for coordinate in normal)
	return direction3(normal)
