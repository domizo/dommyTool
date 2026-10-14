# -*- coding: utf-8 -*-
u"""Validation and portable serialization for ``.asset`` version records."""

import ntpath
import os

from DPyTools.core import paths as core_paths
from DPyTools.pipeline.assets import versioning


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
ALLOWED_EXTENSIONS = frozenset(('.ma', '.mb', '.abc', '.anim', '.danim'))
SCHEMA_VERSION = 1
SCHEMA_VERSION_KEY = 'schema_version'
VERSIONS_KEY = 'versions'
DOCUMENT_FIELDS = frozenset((SCHEMA_VERSION_KEY, VERSIONS_KEY))
REQUIRED_FIELDS = frozenset((
	'version', 'filepath', 'filename', 'comment', 'creator', 'extension',
	'thumbnail', 'date'))
VERSION_PATTERN = versioning.VERSION_PATTERN
MAX_RECORDS = 10000


def _require_text(value, field_name, allow_empty=True):
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if not allow_empty and not value:
		raise ValueError('{0} must not be empty'.format(field_name))
	if any(ord(character) == 0 for character in value):
		raise ValueError('{0} must not contain NUL characters'.format(field_name))
	return value


def _validate_contained_path(value, field_name, asset_root, require_file=False):
	_require_text(value, field_name, allow_empty=False)
	if not os.path.isabs(value):
		if asset_root is None:
			raise ValueError(
				'{0} must be absolute when asset_root is omitted'.format(field_name))
		if ntpath.isabs(value):
			raise ValueError(
				'{0} uses an absolute path for another platform'.format(field_name))
		portable_value = value.replace('\\', '/')
		components = portable_value.split('/')
		if any(component in ('', '.', '..') for component in components):
			raise ValueError('{0} is not a safe relative path'.format(field_name))
		value = os.path.join(asset_root, *components)
	value = os.path.abspath(os.path.expanduser(value))
	if asset_root is not None and not core_paths.is_within(value, asset_root):
		raise ValueError('{0} is outside the asset root'.format(field_name))
	if require_file and not os.path.isfile(value):
		raise IOError('{0} does not exist: {1}'.format(field_name, value))
	return value


def validate_version_record(record, asset_root=None, require_file=False):
	u"""Return a validated copy of one asset-version record."""
	if not isinstance(record, dict):
		raise TypeError('asset version record must be a mapping')
	missing = REQUIRED_FIELDS.difference(record)
	if missing:
		raise ValueError(
			'asset version record is missing: {0}'.format(', '.join(sorted(missing))))
	unknown = set(record).difference(REQUIRED_FIELDS)
	if unknown:
		raise ValueError(
			'asset version record contains unknown fields: {0}'.format(
				', '.join(sorted(unknown))))

	validated = dict(record)
	version = _require_text(validated['version'], 'version', allow_empty=False)
	if versioning.parse_version(version) is None:
		raise ValueError('version must use the form v001 within the supported range')

	extension = _require_text(validated['extension'], 'extension', allow_empty=False)
	if extension not in ALLOWED_EXTENSIONS:
		raise ValueError('unsupported asset extension: {0}'.format(extension))

	filename = _require_text(validated['filename'], 'filename', allow_empty=False)
	if filename != os.path.basename(filename) or filename != ntpath.basename(filename):
		raise ValueError('filename must not contain a directory path')
	if os.path.splitext(filename)[1] != extension:
		raise ValueError('filename extension does not match extension')

	filepath = _validate_contained_path(
		validated['filepath'], 'filepath', asset_root, require_file=require_file)
	validated['filepath'] = filepath
	if os.path.basename(filepath) != filename:
		raise ValueError('filepath basename does not match filename')
	if os.path.splitext(filepath)[1] != extension:
		raise ValueError('filepath extension does not match extension')

	thumbnail = _require_text(validated['thumbnail'], 'thumbnail')
	if thumbnail:
		thumbnail = _validate_contained_path(
			thumbnail, 'thumbnail', asset_root)
		if os.path.splitext(thumbnail)[1].lower() != '.png':
			raise ValueError('thumbnail must be a PNG path')
		validated['thumbnail'] = thumbnail

	for field_name in ('comment', 'creator', 'date'):
		_require_text(validated[field_name], field_name)
	return validated


def decode_version_document(data, asset_root=None, require_files=False):
	u"""Decode either a versioned document or the legacy top-level list."""
	if isinstance(data, list):
		records = data
	elif isinstance(data, dict):
		unknown_fields = set(data).difference(DOCUMENT_FIELDS)
		missing_fields = DOCUMENT_FIELDS.difference(data)
		if missing_fields:
			raise ValueError(
				'asset metadata is missing: {0}'.format(
					', '.join(sorted(missing_fields))))
		if unknown_fields:
			raise ValueError(
				'asset metadata contains unknown fields: {0}'.format(
					', '.join(sorted(unknown_fields))))
		schema_version = data[SCHEMA_VERSION_KEY]
		if isinstance(schema_version, bool) or schema_version != SCHEMA_VERSION:
			raise ValueError(
				'unsupported asset metadata schema: {0}'.format(schema_version))
		records = data[VERSIONS_KEY]
	else:
		raise TypeError('asset metadata must be a mapping or legacy list')

	return validate_version_records(
		records, asset_root=asset_root, require_files=require_files)


def _portable_relative_path(value, field_name, asset_root):
	absolute_path = _validate_contained_path(
		value, field_name, asset_root, require_file=False)
	relative_path = os.path.relpath(absolute_path, os.path.abspath(asset_root))
	portable_path = relative_path.replace(os.sep, '/')
	components = portable_path.split('/')
	if any(component in ('', '.', '..') for component in components):
		raise ValueError('{0} cannot be stored safely'.format(field_name))
	return portable_path


def encode_version_document(records, asset_root, require_files=False):
	u"""Create the portable, versioned representation written to ``.asset``."""
	if not asset_root or not os.path.isabs(asset_root):
		raise ValueError('asset_root must be an absolute path')
	validated = validate_version_records(
		records, asset_root=asset_root, require_files=require_files)
	portable_records = []
	for record in validated:
		portable_record = dict(record)
		portable_record['filepath'] = _portable_relative_path(
			record['filepath'], 'filepath', asset_root)
		if record['thumbnail']:
			portable_record['thumbnail'] = _portable_relative_path(
				record['thumbnail'], 'thumbnail', asset_root)
		portable_records.append(portable_record)
	return {
		SCHEMA_VERSION_KEY: SCHEMA_VERSION,
		VERSIONS_KEY: portable_records,
	}


def validate_import_record(record, asset_root):
	u"""Validate one cached record immediately before a Maya file operation."""
	return validate_version_record(
		record, asset_root=asset_root, require_file=True)


def validate_version_records(records, asset_root=None, require_files=False,
		max_records=MAX_RECORDS):
	u"""Validate a bounded list of records and reject duplicate versions."""
	if not isinstance(records, list):
		raise TypeError('asset version metadata must be a list')
	if len(records) > max_records:
		raise ValueError('asset version metadata contains too many records')
	validated = []
	versions = set()
	filepaths = set()
	for record in records:
		item = validate_version_record(
			record, asset_root=asset_root, require_file=require_files)
		if item['version'] in versions:
			raise ValueError('duplicate asset version: {0}'.format(item['version']))
		filepath_key = os.path.normcase(core_paths.resolve(item['filepath']))
		if filepath_key in filepaths:
			raise ValueError('duplicate asset filepath: {0}'.format(item['filepath']))
		versions.add(item['version'])
		filepaths.add(filepath_key)
		validated.append(item)
	return validated
