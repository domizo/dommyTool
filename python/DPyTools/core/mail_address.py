# -*- coding: utf-8 -*-
"""Plain bounded mailbox admission shared by registry and SMTP boundaries."""

from email.utils import parseaddr

from DPyTools.core.strings import STRING_TYPES


MAX_ADDRESS_LENGTH = 512


def plain_address(value, field_name='mail address', strip=False):
	"""Preserve the archived plain-address contract, rejecting all controls first."""
	if not isinstance(strip, bool):
		raise TypeError('Address whitespace policy must be Boolean')
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if len(value) > MAX_ADDRESS_LENGTH:
		raise ValueError('{0} is too long'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise ValueError('{0} contains a control character'.format(field_name))
	if strip:
		value = value.strip()
	elif value != value.strip():
		raise ValueError('{0} contains surrounding whitespace'.format(field_name))
	if not value:
		raise ValueError('{0} must not be empty'.format(field_name))
	parsed = parseaddr(value)[1]
	if parsed != value or '@' not in parsed:
		raise ValueError('{0} must be a plain email address'.format(field_name))
	return value
