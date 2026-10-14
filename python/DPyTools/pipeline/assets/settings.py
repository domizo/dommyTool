# -*- coding: utf-8 -*-
u"""Maya-independent schema for the per-user ``.dasset`` preferences."""

import ntpath
import os

from DPyTools.pipeline.assets import paths as asset_paths


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
SCHEMA_VERSION = 1
SCHEMA_VERSION_KEY = 'schema_version'
ROOT_PATH_KEY = 'rootpath'
USERNAME_KEY = 'username'
ASSET_TYPE_KEY = 'asset_type'
CURRENT_PROJECT_KEY = 'current_project'
NOTIFICATION_KEY = 'notif_mail_opt_in'
ALLOWED_KEYS = frozenset((
	SCHEMA_VERSION_KEY, ROOT_PATH_KEY, USERNAME_KEY, ASSET_TYPE_KEY,
	CURRENT_PROJECT_KEY, NOTIFICATION_KEY))
MAX_ASSET_TYPES = 100


def _text(value, field_name, allow_empty=False, max_length=1024):
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if not allow_empty and not value:
		raise ValueError('{0} must not be empty'.format(field_name))
	if len(value) > max_length:
		raise ValueError('{0} is too long'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise ValueError('{0} contains a control character'.format(field_name))
	return value


def _root_path(value):
	value = _text(value, 'root path')
	if not os.path.isabs(value) and not ntpath.isabs(value):
		raise ValueError('root path must be absolute')
	return value


def _asset_types(value):
	if not isinstance(value, list):
		raise TypeError('asset_type must be a list')
	if len(value) > MAX_ASSET_TYPES:
		raise ValueError('asset_type contains too many entries')
	validated = []
	seen = set()
	for item in value:
		item = asset_paths.validate_component(item, field_name='asset type')
		if item in seen:
			raise ValueError('asset_type contains a duplicate: {0}'.format(item))
		seen.add(item)
		validated.append(item)
	return validated


def validate_settings(data):
	u"""Return a normalized copy of a complete or partial settings mapping."""
	if not isinstance(data, dict):
		raise TypeError('asset settings must be a mapping')
	unknown = set(data).difference(ALLOWED_KEYS)
	if unknown:
		raise ValueError(
			'asset settings contain unknown keys: {0}'.format(', '.join(sorted(unknown))))

	version = data.get(SCHEMA_VERSION_KEY, SCHEMA_VERSION)
	if isinstance(version, bool) or version != SCHEMA_VERSION:
		raise ValueError('unsupported asset settings schema version')
	validated = {SCHEMA_VERSION_KEY: SCHEMA_VERSION}
	if ROOT_PATH_KEY in data:
		validated[ROOT_PATH_KEY] = _root_path(data[ROOT_PATH_KEY])
	if USERNAME_KEY in data:
		validated[USERNAME_KEY] = _text(
			data[USERNAME_KEY], 'username', max_length=256)
	if ASSET_TYPE_KEY in data:
		validated[ASSET_TYPE_KEY] = _asset_types(data[ASSET_TYPE_KEY])
	if CURRENT_PROJECT_KEY in data:
		current = _text(
			data[CURRENT_PROJECT_KEY], 'current project', allow_empty=True,
			max_length=asset_paths.MAX_COMPONENT_LENGTH)
		if current:
			current = asset_paths.validate_component(
				current, field_name='current project')
		validated[CURRENT_PROJECT_KEY] = current
	if NOTIFICATION_KEY in data:
		value = data[NOTIFICATION_KEY]
		if not isinstance(value, bool):
			raise TypeError('notification preference must be a boolean')
		validated[NOTIFICATION_KEY] = value
	return validated


def default_settings(root_path, username):
	u"""Build the complete safe default used when preferences are unavailable."""
	return validate_settings({
		ROOT_PATH_KEY: root_path,
		USERNAME_KEY: username,
		CURRENT_PROJECT_KEY: '',
		NOTIFICATION_KEY: False,
	})
