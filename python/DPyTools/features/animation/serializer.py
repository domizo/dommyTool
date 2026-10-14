# -*- coding: utf-8 -*-
"""Maya-independent construction and serialization for DAnim records."""

import math

from DPyTools.core import bounded_text
from DPyTools.features.animation import parser
from DPyTools.features.animation.format import VALID_INFINITY_NAMES


try:
	STRING_TYPES = (basestring,)
except NameError:
	STRING_TYPES = (str,)


class AnimationSerializationError(ValueError):
	"""Raised when Maya returns incomplete or unsupported animation data."""


def _token(value, field_name):
	if not isinstance(value, STRING_TYPES) or not value:
		raise AnimationSerializationError(
			'{0} must be a non-empty token'.format(field_name))
	if len(value) > parser.MAX_LINE_LENGTH:
		raise AnimationSerializationError('{0} exceeds the line-size limit'.format(field_name))
	if value.split() != [value]:
		raise AnimationSerializationError(
			'{0} must be a non-empty token'.format(field_name))
	return value


def _finite_number(value, field_name):
	try:
		number = float(value)
	except (TypeError, ValueError, OverflowError):
		raise AnimationSerializationError(
			'{0} must be numeric'.format(field_name))
	if math.isnan(number) or math.isinf(number):
		raise AnimationSerializationError(
			'{0} must be finite'.format(field_name))
	return number


def _flag(value, field_name):
	if value not in (False, True, 0, 1):
		raise AnimationSerializationError(
			'{0} must be boolean'.format(field_name))
	return bool(value)


def _sequence(value, field_name, max_length=None):
	if value is None:
		raise AnimationSerializationError(
			'{0} data is unavailable'.format(field_name))
	max_length = parser.MAX_KEYS if max_length is None else max_length
	try:
		length = len(value)
	except TypeError:
		length = None
	if length is not None and length > max_length:
		raise AnimationSerializationError('{0} exceeds the key-count limit'.format(field_name))
	if isinstance(value, (list, tuple)):
		return tuple(value)
	try:
		iterator = iter(value)
	except TypeError:
		raise AnimationSerializationError(
			'{0} must be a sequence'.format(field_name))
	items = []
	for item in iterator:
		if len(items) >= max_length:
			raise AnimationSerializationError('{0} exceeds the key-count limit'.format(field_name))
		items.append(item)
	return tuple(items)


def _optional_sequence(value, field_name, expected_length):
	if value is None:
		return (None,) * expected_length
	sequence = _sequence(value, field_name)
	if len(sequence) != expected_length:
		raise AnimationSerializationError(
			'{0} count does not match key count'.format(field_name))
	return sequence


def _format_number(value, field_name):
	return repr(_finite_number(value, field_name))


def _validate_tangent(value, field_name):
	_token(value, field_name)
	if value not in parser.VALID_TANGENT_NAMES:
		raise AnimationSerializationError(
			'{0} is not supported by Maya 2019'.format(field_name))
	return value


def build_animation_record(attribute, node, has_parent, weighted,
		pre_infinity, post_infinity, times, values, in_tangents,
		out_tangents, tangent_locks, weight_locks, breakdown_times=None,
		in_angles=None, in_weights=None, out_angles=None, out_weights=None,
		breakdown_flags=None):
	"""Validate parallel Maya query results and return one typed record."""
	attribute = _token(attribute, 'attribute')
	node = _token(node, 'node')
	has_parent = _flag(has_parent, 'parent flag')
	weighted = _flag(weighted, 'weighted flag')
	if pre_infinity not in VALID_INFINITY_NAMES:
		raise AnimationSerializationError('Unsupported pre-infinity mode')
	if post_infinity not in VALID_INFINITY_NAMES:
		raise AnimationSerializationError('Unsupported post-infinity mode')

	parallel_values = (
		('times', times), ('values', values),
		('in tangents', in_tangents), ('out tangents', out_tangents),
		('tangent locks', tangent_locks), ('weight locks', weight_locks),
	)
	sequences = dict(
		(name, _sequence(value, name)) for name, value in parallel_values)
	key_count = len(sequences['times'])
	if not key_count:
		raise AnimationSerializationError(
			'Animation record must contain at least one key')
	for name, sequence in sequences.items():
		if len(sequence) != key_count:
			raise AnimationSerializationError(
				'{0} count does not match key count'.format(name))
	sequences['times'] = tuple(
		_finite_number(value, 'key time')
		for value in sequences['times'])
	sequences.update({
		'in angles': _optional_sequence(in_angles, 'in angles', key_count),
		'in weights': _optional_sequence(in_weights, 'in weights', key_count),
		'out angles': _optional_sequence(out_angles, 'out angles', key_count),
		'out weights': _optional_sequence(out_weights, 'out weights', key_count),
	})

	if breakdown_flags is None:
		breakdown_values = frozenset(
			_finite_number(value, 'breakdown time')
			for value in _sequence(
				breakdown_times or (), 'breakdown times'))
		breakdowns = tuple(
			key_time in breakdown_values
			for key_time in sequences['times'])
	else:
		breakdowns = _sequence(breakdown_flags, 'breakdown flags')
		if len(breakdowns) != key_count:
			raise AnimationSerializationError(
				'breakdown flags count does not match key count')
		breakdowns = tuple(
			_flag(value, 'breakdown flag') for value in breakdowns)
	keys = []
	for index in range(key_count):
		in_tangent = _validate_tangent(
			sequences['in tangents'][index], 'in tangent')
		out_tangent = _validate_tangent(
			sequences['out tangents'][index], 'out tangent')
		key_time = sequences['times'][index]
		in_angle = in_weight = out_angle = out_weight = None
		if in_tangent == 'fixed':
			in_angle = _finite_number(
				sequences['in angles'][index], 'fixed in angle')
			in_weight = _finite_number(
				sequences['in weights'][index], 'fixed in weight')
		if out_tangent == 'fixed':
			out_angle = _finite_number(
				sequences['out angles'][index], 'fixed out angle')
			out_weight = _finite_number(
				sequences['out weights'][index], 'fixed out weight')
		keys.append(parser.AnimationKey(
			key_time,
			_finite_number(sequences['values'][index], 'key value'),
			in_tangent,
			out_tangent,
			_flag(sequences['tangent locks'][index], 'tangent lock'),
			_flag(sequences['weight locks'][index], 'weight lock'),
			breakdowns[index],
			in_angle, in_weight, out_angle, out_weight))

	return parser.AnimationRecord(
		attribute, node, has_parent, weighted,
		pre_infinity, post_infinity, tuple(keys))


def build_static_record(attribute, node, has_parent, value):
	"""Validate one unanimated scalar and return a typed static record."""
	attribute = _token(attribute, 'attribute')
	node = _token(node, 'node')
	has_parent = _flag(has_parent, 'parent flag')
	if isinstance(value, bool):
		clean_value = value
	else:
		clean_value = _finite_number(value, 'static value')
	return parser.StaticRecord(attribute, node, has_parent, clean_value)


def _validated_animation_record(record, max_keys=None):
	if not isinstance(record, parser.AnimationRecord):
		raise AnimationSerializationError('Expected an AnimationRecord')
	keys = _sequence(record.keys, 'keys', max_length=max_keys)
	if any(not isinstance(key, parser.AnimationKey) for key in keys):
		raise AnimationSerializationError('Expected AnimationKey entries')
	# Reconstructing validates records not created through the public builder.
	return build_animation_record(
		record.attribute, record.node, record.has_parent, record.weighted,
		record.pre_infinity, record.post_infinity,
		(key.time for key in keys),
		(key.value for key in keys),
		(key.in_tangent for key in keys),
		(key.out_tangent for key in keys),
		(key.tangent_locked for key in keys),
		(key.weight_locked for key in keys),
		in_angles=(key.in_angle for key in keys),
		in_weights=(key.in_weight for key in keys),
		out_angles=(key.out_angle for key in keys),
		out_weights=(key.out_weight for key in keys),
		breakdown_flags=(key.breakdown for key in keys))


def _animation_lines(validated):
	for line in (
		'anim {0} {0} {1} {2} 0 0;'.format(
			validated.attribute, validated.node, int(validated.has_parent)),
		'animData {',
		'  weighted {0};'.format(int(validated.weighted)),
		'  preInfinity {0};'.format(validated.pre_infinity),
		'  postInfinity {0};'.format(validated.post_infinity),
		'  keys {',
	):
		yield line + '\n'
	for key in validated.keys:
		values = [
			_format_number(key.time, 'key time'),
			_format_number(key.value, 'key value'),
			key.in_tangent,
			key.out_tangent,
			str(int(key.tangent_locked)),
			str(int(key.weight_locked)),
			str(int(key.breakdown)),
		]
		if key.in_tangent == 'fixed':
			values.extend((
				_format_number(key.in_angle, 'fixed in angle'),
				_format_number(key.in_weight, 'fixed in weight')))
		if key.out_tangent == 'fixed':
			values.extend((
				_format_number(key.out_angle, 'fixed out angle'),
				_format_number(key.out_weight, 'fixed out weight')))
		yield '    {0};\n'.format(' '.join(values))
	yield '  }\n'
	yield '}\n'


def _serialized_lines(lines):
	return ''.join(bounded_text.checked_lines(
		lines, parser.MAX_FILE_SIZE, parser.MAX_LINE_LENGTH,
		AnimationSerializationError))


def serialize_animation_record(record):
	"""Serialize a validated record admitted by the reader's own limits."""
	return _serialized_lines(_animation_lines(_validated_animation_record(record)))


def serialize_static_record(record):
	"""Serialize one validated static record in canonical DAnim form."""
	if not isinstance(record, parser.StaticRecord):
		raise AnimationSerializationError('Expected a StaticRecord')
	validated = build_static_record(
		record.attribute, record.node, record.has_parent, record.value)
	if isinstance(validated.value, bool):
		value = str(validated.value)
	else:
		value = _format_number(validated.value, 'static value')
	return _serialized_lines(('static {0} {0} {1} {2} {3}\n'.format(
		validated.attribute, validated.node,
		int(validated.has_parent), value),))


def serialize_record(record):
	"""Serialize one typed DAnim record."""
	if isinstance(record, parser.AnimationRecord):
		return serialize_animation_record(record)
	if isinstance(record, parser.StaticRecord):
		return serialize_static_record(record)
	raise AnimationSerializationError('Unsupported DAnim record type')


class AnimationWriter(object):
	"""Write canonical binary DAnim through one cumulative, terminal budget.

	The stream is borrowed; callers own staging/commit/abort. A failed writer
	must not be reused or finalized after any validation or write exception.
	"""

	def __init__(self, stream):
		self._stream = stream
		self._text = bounded_text.TextBudget(
			parser.MAX_FILE_SIZE, parser.MAX_LINE_LENGTH, AnimationSerializationError)
		self.record_count = 0
		self.key_count = 0
		self._failed = False
		self._finished = False

	def _require_active(self):
		if self._failed:
			raise AnimationSerializationError('Animation writer has already failed')
		if self._finished:
			raise AnimationSerializationError('Animation writer has already finished')

	def _write_line(self, line):
		data = self._text.admit(line)
		written = self._stream.write(data)
		if written is not None and written != len(data):
			raise IOError('Animation output reported an incomplete write')

	def write_header(self, header):
		"""Write complete comment/blank lines inside the same output byte budget."""
		self._require_active()
		try:
			if not isinstance(header, STRING_TYPES) or (header and not header.endswith('\n')):
				raise AnimationSerializationError('Header requires complete text lines')
			if len(header) > self._text.max_bytes - self._text.total_bytes:
				raise AnimationSerializationError('Header exceeds the byte size limit')
			position = 0
			while position < len(header):
				# Match the binary reader's LF boundary without allocating all lines.
				end = header.find('\n', position, position + self._text.max_line_bytes + 1)
				if end < 0:
					raise AnimationSerializationError('Header line exceeds the size limit')
				line = header[position:end + 1]
				if line.strip() and not line.lstrip().startswith('#'):
					raise AnimationSerializationError('Header must contain only comments or blank lines')
				self._write_line(line)
				position = end + 1
		except BaseException:
			self._failed = True
			raise

	def write_record(self, record):
		self._require_active()
		try:
			if self.record_count >= parser.MAX_RECORDS:
				raise AnimationSerializationError('Animation output exceeds the record-count limit')
			if isinstance(record, parser.AnimationRecord):
				validated = _validated_animation_record(
					record, max_keys=parser.MAX_KEYS - self.key_count)
				key_count = len(validated.keys)
				lines = _animation_lines(validated)
			elif isinstance(record, parser.StaticRecord):
				key_count = 0
				lines = (serialize_static_record(record),)
			else:
				raise AnimationSerializationError('Unsupported DAnim record type')
			for line in lines:
				self._write_line(line)
			self.record_count += 1
			self.key_count += key_count
		except BaseException:
			self._failed = True
			raise

	def finish(self):
		self._require_active()
		if not self.record_count:
			self._failed = True
			raise AnimationSerializationError('No writable animation or static attributes were found')
		if self._text.line_bytes:
			self._failed = True
			raise AnimationSerializationError('Animation output ends with an incomplete line')
		self._finished = True
		return self.record_count
