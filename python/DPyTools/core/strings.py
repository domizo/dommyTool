# -*- coding: utf-8 -*-
"""Small Maya-independent string and naming helpers."""

import re


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)


SIDES_WITH_CENTER = ('_l', '_r', '_ul', '_ur', '_dl', '_dr', '_c')
SIDES = ('_l', '_r', '_ul', '_ur', '_dl', '_dr')
REVERSE_SIDES = {
	'_l': '_r',
	'_r': '_l',
	'_ul': '_ur',
	'_ur': '_ul',
	'_dl': '_dr',
	'_dr': '_dl',
}
SIDE_TOKEN = re.compile(r'_(?:ul|ur|dl|dr|l|r|c)(?=_|$)')


def leaf_name(source):
	"""Return the final DAG segment, keeping its complete namespace prefix."""
	if not isinstance(source, STRING_TYPES):
		raise TypeError('Node name must be a string')
	return source.rsplit('|', 1)[-1]


def _side_match(source):
	leaf = leaf_name(source)
	base = leaf.rsplit(':', 1)[-1]
	matches = SIDE_TOKEN.finditer(base)
	first = next(matches, None)
	if next(matches, None) is not None:
		raise ValueError('Node name contains multiple ambiguous side tokens')
	return base, first


def contains(base='', target=''):
	"""Return whether ``target`` occurs in ``base``."""
	return target in base


def contains_any(base='', targets=None):
	"""Return whether any target occurs in ``base``."""
	return any(target in base for target in (targets or ()))


def contains_side(base='', include_center=False):
	"""Match one bounded side token on the node, never a parent/namespace."""
	side = current_side(base)
	return side is not None and (include_center or side != '_c')


def replace_text(source='', replacements=None):
	"""Apply every replacement mapping to ``source`` in iteration order."""
	if not source:
		return source
	for old_text, new_text in (replacements or {}).items():
		source = source.replace(old_text, new_text)
	return source


def remove_text(source='', targets=None):
	"""Remove every target substring from ``source``."""
	return replace_text(
		source, dict((target, '') for target in (targets or ())))


def filter_items(items=None, filter_text=''):
	"""Return a new list containing case-insensitive substring matches."""
	items = [] if items is None else items
	if not filter_text:
		return list(items)
	normalized = filter_text.lower()
	return [item for item in items if normalized in item.lower()]


def strip_side(source=''):
	"""Remove one bounded node-side token, preserving parent/namespace text."""
	base, match = _side_match(source)
	if match is None:
		return source
	prefix = source[:len(source) - len(base)]
	return prefix + base[:match.start()] + base[match.end():]


def current_side(source=None):
	"""Return the unique bounded side token, or None; reject ambiguity."""
	if source is None:
		return None
	unused_base, match = _side_match(source)
	return match.group(0) if match is not None else None


def set_side(source, side):
	"""Replace the node-side token in place, or append it if absent."""
	if side not in SIDES_WITH_CENTER:
		raise ValueError('Unsupported side token')
	base, match = _side_match(source)
	if not base:
		raise ValueError('Node name must not be empty')
	if match is None:
		return source + side
	prefix = source[:len(source) - len(base)]
	return prefix + base[:match.start()] + side + base[match.end():]


def reverse_side(side=''):
	"""Return the matching opposite side token, or an empty string."""
	return REVERSE_SIDES.get(side, '')


def pad_number(count=1, size=3):
	"""Return a zero-padded decimal representation."""
	return str(count).zfill(size)
