# -*- coding: utf-8 -*-
"""Host-independent admission for bounded unitless key batches."""

from DPyTools.core.numeric import finite_number


MAX_KEY_PAIRS = 10000


def key_pairs(times, values):
	"""Snapshot, validate and sort pairs without losing their correspondence."""
	if (not isinstance(times, (list, tuple)) or not isinstance(values, (list, tuple))
			or not 1 <= len(times) <= MAX_KEY_PAIRS or len(times) != len(values)):
		raise ValueError('Keys require equally sized nonempty bounded sequences')
	pairs = tuple((finite_number(time, 'Key input'), finite_number(value, 'Key value'))
		for time, value in zip(times, values))
	if len(set(time for time, value in pairs)) != len(pairs):
		raise ValueError('Key inputs must be unique')
	return tuple(sorted(pairs))
