# -*- coding: utf-8 -*-
u"""Maya-independent validation for user and mailing-list persistence."""

import numbers

from DPyTools.core.mail_address import plain_address


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
USER_ID_KEY = 'id'
USER_NAME_KEY = 'name'
USER_MAIL_KEY = 'mail'
REQUIRED_USER_FIELDS = frozenset((USER_ID_KEY, USER_NAME_KEY, USER_MAIL_KEY))
MAX_USERS = 10000
MAX_TEXT_LENGTH = 512


def _text(value, field_name, max_length=MAX_TEXT_LENGTH):
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if not value or value != value.strip():
		raise ValueError('{0} must be non-empty without surrounding whitespace'.format(
			field_name))
	if len(value) > max_length:
		raise ValueError('{0} is too long'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise ValueError('{0} contains a control character'.format(field_name))
	return value


def validate_user_id(value):
	return _text(value, 'user id', max_length=128)


def validate_user_record(record, expected_id=None):
	u"""Return a validated copy of one user record."""
	if not isinstance(record, dict):
		raise TypeError('user record must be a mapping')
	missing = REQUIRED_USER_FIELDS.difference(record)
	if missing:
		raise ValueError('user record is missing: {0}'.format(', '.join(sorted(missing))))
	validated = dict(record)
	user_id = validate_user_id(validated[USER_ID_KEY])
	if expected_id is not None and user_id != expected_id:
		raise ValueError('user record id does not match its mapping key')
	_text(validated[USER_NAME_KEY], 'user name')
	validated[USER_MAIL_KEY] = plain_address(validated[USER_MAIL_KEY])
	return validated


def validate_user_collection(users, max_users=MAX_USERS):
	u"""Validate a bounded user-id mapping."""
	if not isinstance(users, dict):
		raise TypeError('user collection must be a mapping')
	if len(users) > max_users:
		raise ValueError('user collection contains too many records')
	validated = {}
	for user_id, record in users.items():
		validate_user_id(user_id)
		validated[user_id] = validate_user_record(record, expected_id=user_id)
	return validated


def validate_mailing_list(user_ids, known_users=None, max_entries=MAX_USERS):
	u"""Validate a bounded, duplicate-free list of user IDs."""
	if not isinstance(user_ids, list):
		raise TypeError('mailing list must be a list')
	if len(user_ids) > max_entries:
		raise ValueError('mailing list contains too many entries')
	validated = []
	seen = set()
	for user_id in user_ids:
		user_id = validate_user_id(user_id)
		if user_id in seen:
			raise ValueError('mailing list contains a duplicate user id: {0}'.format(
				user_id))
		if known_users is not None and user_id not in known_users:
			raise ValueError('mailing list contains an unknown user id: {0}'.format(
				user_id))
		seen.add(user_id)
		validated.append(user_id)
	return validated


def merge_mailing_list(existing, requested, known_users, max_entries=MAX_USERS):
	u"""Admit a complete ordered addition plan without mutating either input."""
	if (isinstance(max_entries, bool) or not isinstance(max_entries, numbers.Integral)
			or max_entries < 0):
		raise ValueError('Mailing-list limit must be a non-negative integer')
	merged = validate_mailing_list(existing, max_entries=max_entries)
	if not isinstance(requested, (list, tuple)):
		raise TypeError('Requested user IDs must be a list or tuple')
	if len(requested) > max_entries:
		raise ValueError('Too many requested user IDs')
	if not isinstance(known_users, dict):
		raise TypeError('Known users must be an ID mapping')
	seen = set(merged)
	for user_id in requested:
		user_id = validate_user_id(user_id)
		if user_id not in known_users:
			raise ValueError('Requested user ID is unknown: {0}'.format(user_id))
		if user_id not in seen:
			if len(merged) >= max_entries:
				raise ValueError('Mailing list contains too many entries')
			merged.append(user_id)
			seen.add(user_id)
	return merged


def mailing_addresses(user_ids, users):
	"""Snapshot admitted recipients in mailing order, omitting stale IDs/duplicates."""
	user_ids = validate_mailing_list(user_ids)
	users = validate_user_collection(users)
	addresses = []
	seen = set()
	for user_id in user_ids:
		if user_id in users:
			address = users[user_id][USER_MAIL_KEY]
			if address not in seen:
				addresses.append(address)
				seen.add(address)
	return addresses


def user_table_rows(users):
	"""Snapshot ID-sorted display values once, independently of later mutations."""
	users = validate_user_collection(users)
	return tuple((user_id, users[user_id][USER_NAME_KEY], users[user_id][USER_MAIL_KEY])
		for user_id in sorted(users))


def mailing_table_rows(user_ids, users):
	"""Keep mailing order and stale-ID rows; display names from one fresh registry."""
	user_ids = validate_mailing_list(user_ids)
	users = validate_user_collection(users)
	return tuple((users[user_id][USER_NAME_KEY] if user_id in users else '',)
		for user_id in user_ids)
