# -*- coding: utf-8 -*-
"""Host-free admission of asset export layout and prospective metadata."""

import os

from DPyTools.core import paths, strings
from DPyTools.core.numeric import finite_number
from DPyTools.pipeline.assets import metadata, versioning


def build(asset_root, export_path, asset_name, extension, timerange, creator,
		comment, date, thumbnail=False):
	"""Reject the complete layout/record before directory or scene mutations."""
	asset_name = paths.validate_portable_component(asset_name, 'asset name')
	if extension not in metadata.ALLOWED_EXTENSIONS or not isinstance(thumbnail, bool):
		raise ValueError('Invalid asset extension or thumbnail option')
	if not isinstance(timerange, (list, tuple)) or len(timerange) != 2:
		raise ValueError('Export time range must contain start and end')
	start, end = tuple(finite_number(value, 'Export frame') for value in timerange)
	if start > end:
		raise ValueError('Export start frame must not exceed end frame')
	for value in (asset_root, export_path):
		if (not isinstance(value, strings.STRING_TYPES) or not value or not os.path.isabs(value)
				or any(ord(character) < 32 or ord(character) == 127 for character in value)):
			raise ValueError('Asset/export directories must be absolute paths without controls')
	asset_root, export_path = os.path.abspath(asset_root), os.path.abspath(export_path)
	if not os.path.isdir(asset_root) or os.path.islink(asset_root):
		raise IOError('Asset root must be an existing non-symlink directory')
	if os.path.normcase(os.path.dirname(export_path)) != os.path.normcase(asset_root):
		raise ValueError('Export version must be a direct child of the asset root')
	version = paths.validate_portable_component(os.path.basename(export_path), 'version')
	if versioning.parse_version(version) is None or not paths.is_within(export_path, asset_root):
		raise ValueError('Export version is invalid or escapes its asset root')
	if os.path.lexists(export_path):
		raise IOError('Export version directory is already occupied')
	filename = paths.validate_portable_component(asset_name + '_' + version + extension, 'export filename')
	scenes = os.path.join(export_path, 'scenes')
	output = os.path.join(scenes if extension in ('.ma', '.mb') else export_path, filename)
	record = metadata.validate_version_record({
		'version': version, 'filepath': output, 'filename': filename,
		'extension': extension, 'creator': creator, 'comment': comment, 'date': date,
		'thumbnail': os.path.join(export_path, 'thumbnail.png') if thumbnail else ''},
		asset_root=asset_root)
	return {'asset_root': asset_root, 'export_path': export_path, 'scenes': scenes,
		'sourceimages': os.path.join(export_path, 'sourceimages'), 'record': record,
		'timerange': (start, end), 'metadata_path': os.path.join(asset_root, '.asset')}
