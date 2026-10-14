# -*- coding: utf-8 -*-
u"""Host-independent parsing and allocation of legacy ``v001`` names."""

import re


VERSION_PATTERN = re.compile(r'\Av([0-9]+)\Z')
MAX_VERSION = 999999
MAX_VERSION_NAME_LENGTH = 32


def parse_version(name):
	u"""Return an integer for a valid version name, otherwise ``None``."""
	if not isinstance(name, str):
		try:
			if not isinstance(name, type(u'')):
				return None
		except TypeError:
			return None
	if len(name) > MAX_VERSION_NAME_LENGTH:
		return None
	match = VERSION_PATTERN.match(name)
	if match is None:
		return None
	value = int(match.group(1))
	if value > MAX_VERSION:
		return None
	return value


def next_version(names, width=3):
	u"""Return the next version after the greatest valid name in *names*."""
	if (not isinstance(width, int) or isinstance(width, bool)
			or not 1 <= width < MAX_VERSION_NAME_LENGTH):
		raise ValueError('version width must be an integer from 1 to {0}'.format(
			MAX_VERSION_NAME_LENGTH - 1))
	values = [value for value in (parse_version(name) for name in names)
		if value is not None]
	next_value = (max(values) if values else 0) + 1
	if next_value > MAX_VERSION:
		raise ValueError('asset version limit has been reached')
	return 'v{0}'.format(str(next_value).zfill(width))
