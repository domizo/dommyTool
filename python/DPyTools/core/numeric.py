# -*- coding: utf-8 -*-
u"""Shared host-independent finite real-number admission."""

import math
import numbers


def finite_number(value, field_name):
	u"""Admit a finite real value, excluding booleans and numeric strings."""
	if isinstance(value, bool) or not isinstance(value, numbers.Real):
		raise TypeError('{0} must be a real number'.format(field_name))
	try:
		result = float(value)
	except (OverflowError, ValueError):
		raise ValueError('{0} must be a finite double'.format(field_name))
	if math.isnan(result) or math.isinf(result):
		raise ValueError('{0} must be finite'.format(field_name))
	return result
