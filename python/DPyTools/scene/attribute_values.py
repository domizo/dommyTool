# -*- coding: utf-8 -*-
u"""Host-independent shape/type admission for Maya numeric attribute values."""

import numbers
import sys

from DPyTools.core.numeric import finite_number


SCALAR_TYPES = ('bool', 'byte', 'char', 'short', 'long', 'enum', 'float',
	'double', 'doubleLinear', 'doubleAngle', 'time')
COMPOUND_TYPES = ('short2', 'short3', 'long2', 'long3', 'float2', 'float3', 'double2', 'double3')
FLOAT_MAX = 3.4028234663852886e38


def value_width(attribute_type):
	if attribute_type in SCALAR_TYPES:
		return 1
	if attribute_type in COMPOUND_TYPES:
		return int(attribute_type[-1])
	raise ValueError('Unsupported numeric attribute type: {0}'.format(attribute_type))


def numeric_limits(attribute_type):
	"""Intrinsic representable range shared by values and live schema checks."""
	value_width(attribute_type)
	if attribute_type == 'bool':
		return 0, 1
	for kind, minimum, maximum in (('byte', 0, 255), ('char', -128, 127),
			('short', -32768, 32767), ('long', -2147483648, 2147483647),
			('enum', -2147483648, 2147483647)):
		if attribute_type == kind or attribute_type in (kind + '2', kind + '3'):
			return minimum, maximum
	if attribute_type.startswith('float'):
		return -FLOAT_MAX, FLOAT_MAX
	return -sys.float_info.max, sys.float_info.max


def _component(value, attribute_type):
	if attribute_type == 'bool':
		if isinstance(value, bool):
			return value
		number = finite_number(value, 'Boolean attribute value')
		if number not in (0.0, 1.0):
			raise ValueError('Boolean attribute value must be zero or one')
		return bool(number)
	integer_kind = None
	for kind in ('byte', 'char', 'short', 'long', 'enum'):
		if attribute_type == kind or attribute_type in (kind + '2', kind + '3'):
			integer_kind = kind
			break
	if integer_kind is not None:
		if isinstance(value, bool):
			raise TypeError('Integer attribute value must not be a boolean')
		number = value if isinstance(value, numbers.Integral) else finite_number(value, 'Integer attribute value')
		minimum, maximum = numeric_limits(integer_kind)
		if not minimum <= number <= maximum or int(number) != number:
			raise ValueError('Integer attribute value is fractional or out of range')
		return int(number)
	number = finite_number(value, 'Numeric attribute value')
	if attribute_type.startswith('float') and abs(number) > FLOAT_MAX:
		raise ValueError('Float attribute value is out of range')
	return number


def numeric_value(value, attribute_type):
	u"""Normalize scalar/default lists and flat/wrapped/indexed numeric vectors."""
	width = value_width(attribute_type)
	if width == 1:
		if isinstance(value, (list, tuple)):
			if len(value) != 1:
				raise ValueError('Scalar attribute query must contain one value')
			value = value[0]
		return _component(value, attribute_type)
	if isinstance(value, (list, tuple)) and len(value) == 1:
		value = value[0]
	try:
		if len(value) != width:
			raise ValueError('Numeric compound query has the wrong component count')
		components = tuple(value[index] for index in range(width))
	except (TypeError, KeyError, IndexError):
		raise TypeError('Numeric compound query must be an indexed sequence')
	return tuple(_component(component, attribute_type) for component in components)
