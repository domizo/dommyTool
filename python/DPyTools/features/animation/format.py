# -*- coding: utf-8 -*-
u"""Maya-independent value conversion for the archived animation format."""

import fnmatch
import math


INFINITY_NAMES = {
	0: 'constant',
	1: 'linear',
	2: 'constant',
	3: 'cycle',
	4: 'cycleRelative',
	5: 'oscillate',
}

VALID_INFINITY_NAMES = frozenset(INFINITY_NAMES.values())


def infinity_name(value):
	u"""Convert Maya's numeric infinity mode to its file-format name."""
	return INFINITY_NAMES.get(int(value), 'constant')


def parse_static_value(value):
	u"""Parse a finite scalar or boolean from a static animation record."""
	normalised = value.strip().rstrip(';')
	if normalised == 'True':
		return True
	if normalised == 'False':
		return False

	number = float(normalised)
	if math.isnan(number) or math.isinf(number):
		raise ValueError('Non-finite static animation value is not supported.')
	return number


def remap_node_name(node, has_parent=False, paths=False, namespace='',
		search='', replace='', prefix='', prefix_top_only=False):
	u"""Apply DAnim node-name options without importing Maya.

	DAG path segments are handled independently so namespaces and prefixes do
	not corrupt the ``|`` separators in an explicit Maya path.
	"""
	if not node:
		return node

	leading_separator = node.startswith('|')
	segments = [segment for segment in node.split('|') if segment]
	if not segments:
		return node

	if namespace:
		namespace_prefix = namespace.rstrip(':') + ':'
		segments = [namespace_prefix + segment for segment in segments]

	if search:
		segments = [segment.replace(search, replace) for segment in segments]

	if prefix:
		if prefix_top_only:
			if not has_parent or (paths and len(segments) > 1):
				segments[0] = prefix + segments[0]
		else:
			segments = [prefix + segment for segment in segments]

	if not paths:
		return segments[-1]
	return ('|' if leading_separator else '') + '|'.join(segments)


def channel_matches(channel_name, pattern):
	"""Return whether a mapped ``node.attribute`` matches a scope pattern."""
	return fnmatch.fnmatchcase(channel_name, pattern or '*')
