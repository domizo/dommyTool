# -*- coding: utf-8 -*-
u"""Maya-independent construction of safely quoted Alembic job strings."""

from DPyTools.core.numeric import finite_number


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
ALLOWED_FLAGS = frozenset((
	'-renderableOnly', '-writeVisibility', '-wholeFrameGeo', '-uvWrite',
	'-worldSpace', '-noNormals', '-verbose'))
ALLOWED_DATA_FORMATS = frozenset(('ogawa', 'hdf'))


def quote_argument(value):
	u"""Quote one value for the Alembic plug-in's job-string parser."""
	if not isinstance(value, STRING_TYPES):
		value = TEXT_TYPE(value)
	if not value:
		raise ValueError('Alembic job argument must not be empty.')
	if any(character in value for character in ('\x00', '\r', '\n', '"')):
		raise ValueError('Unsupported character in Alembic job argument.')
	return '"{0}"'.format(value.replace('\\', '/'))


def _number(value, field_name):
	return finite_number(value, field_name)


def build_export_job(start_frame, end_frame, step, filepath, roots=None,
		selection=False, flags=None, data_format=None):
	u"""Build one validated job string accepted by Maya's Alembic exporters."""
	start_frame = _number(start_frame, 'start frame')
	end_frame = _number(end_frame, 'end frame')
	step = _number(step, 'frame step')
	if start_frame > end_frame:
		raise ValueError('Start frame must not be greater than end frame.')
	if step <= 0:
		raise ValueError('Frame step must be greater than zero.')
	if not isinstance(selection, bool):
		raise TypeError('selection must be Boolean')

	if roots is None:
		roots = []
	if not isinstance(roots, (list, tuple)):
		raise TypeError('roots must be a list or tuple')
	if roots and selection:
		raise ValueError('roots and selection are mutually exclusive')

	if flags is None:
		flags = []
	if not isinstance(flags, (list, tuple)):
		raise TypeError('flags must be a list or tuple')
	if any(not isinstance(flag, STRING_TYPES) for flag in flags):
		raise TypeError('Alembic flags must be strings')
	unknown_flags = set(flags).difference(ALLOWED_FLAGS)
	if unknown_flags:
		raise ValueError(
			'Unsupported Alembic flag: {0}'.format(', '.join(sorted(unknown_flags))))

	arguments = [
		'-frameRange', str(start_frame), str(end_frame), '-step', str(step)]
	for root in roots:
		arguments.extend(('-root', quote_argument(root)))
	if selection:
		arguments.append('-selection')
	for flag in flags:
		if flag not in arguments:
			arguments.append(flag)
	if data_format is not None:
		if not isinstance(data_format, STRING_TYPES):
			raise TypeError('Alembic data format must be a string')
		if data_format not in ALLOWED_DATA_FORMATS:
			raise ValueError('Unsupported Alembic data format: {0}'.format(data_format))
		arguments.extend(('-dataFormat', data_format))
	arguments.extend(('-file', quote_argument(filepath)))
	return ' '.join(arguments)
