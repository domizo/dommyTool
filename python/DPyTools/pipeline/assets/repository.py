# -*- coding: utf-8 -*-
u"""Persistence boundary shared by the legacy asset-management UIs."""

import errno
import os

from DPyTools.core import json_store
from DPyTools.pipeline.assets import metadata, settings, user_data


def load_mapping(filename, default=None):
	if default is None:
		default = {}
	return json_store.load(filename, default=default, expected_type=dict)


def load_list(filename, default=None):
	if default is None:
		default = []
	return json_store.load(filename, default=default, expected_type=list)


def save(filename, data):
	return json_store.dump(filename, data)


def _settings_snapshot(filename, defaults):
	"""Use defaults only for a missing path; present unreadable data must fail."""
	defaults = settings.validate_settings(defaults)
	try:
		os.lstat(filename)
	except OSError as error:
		if error.errno != errno.ENOENT:
			raise
		return defaults, False
	# No JSON default: a path removed/replaced after lstat is not new settings.
	loaded = settings.validate_settings(json_store.load(filename, expected_type=dict))
	defaults.update(loaded)
	return settings.validate_settings(defaults), True


def load_settings(filename, defaults):
	"""Read strictly; the Maya display adapter owns any safe-default fallback."""
	return _settings_snapshot(filename, defaults)[0]


def update_settings(filename, defaults, update, replace=False):
	"""Refuse unreadable/unsupported originals before merging or replacing data.

	New-file publication is exclusive. Existing-file updates still require a
	trusted stable namespace and are not concurrent read/modify/write locking.
	"""
	if not isinstance(replace, bool):
		raise TypeError('Settings replacement policy must be Boolean')
	update = settings.validate_settings(update)
	current, existed = _settings_snapshot(filename, defaults)
	if replace:
		current = update
	else:
		current.update(update)
	return json_store.dump(filename, settings.validate_settings(current), replace_existing=existed)


def create_settings(filename, defaults):
	"""Initialize only an absent settings path; never reset existing data."""
	return json_store.dump(filename, settings.validate_settings(defaults), replace_existing=False)


def save_user_collection(filename, users):
	u"""Validate the complete post-mutation registry before atomic persistence."""
	return save(filename, user_data.validate_user_collection(users))


def save_mailing_list(filename, user_ids):
	u"""Preserve legacy IDs/order while enforcing the reader's complete schema."""
	return save(filename, user_data.validate_mailing_list(user_ids))


def load_version_records(filename, asset_root=None, require_files=False):
	data = json_store.load(filename, default=[])
	return metadata.decode_version_document(
		data, asset_root=asset_root, require_files=require_files)


def save_version_records(filename, records, asset_root=None, require_files=False):
	document = metadata.encode_version_document(
		records, asset_root=asset_root, require_files=require_files)
	return save(filename, document)
