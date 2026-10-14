# -*- coding: utf-8 -*-
"""Strict, Maya-independent parsing for the archived DAnim text format."""

import collections
import math
import os
import re

from DPyTools.core import bounded_text
from DPyTools.features.animation.format import parse_static_value
from DPyTools.features.animation.format import VALID_INFINITY_NAMES


MAX_LINE_LENGTH = 1024 * 1024
MAX_FILE_SIZE = 256 * 1024 * 1024
MAX_RECORDS = 100000
MAX_KEYS = 2000000
TANGENT_NAME_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_]*$')
VALID_TANGENT_NAMES = frozenset((
	'auto', 'clamped', 'fast', 'fixed', 'flat', 'linear', 'plateau',
	'slow', 'spline', 'step', 'stepnext',
))

AnimationKey = collections.namedtuple(
	'AnimationKey',
	('time value in_tangent out_tangent tangent_locked weight_locked '
	 'breakdown in_angle in_weight out_angle out_weight'))
AnimationRecord = collections.namedtuple(
	'AnimationRecord',
	'attribute node has_parent weighted pre_infinity post_infinity keys')
StaticRecord = collections.namedtuple(
	'StaticRecord', 'attribute node has_parent value')


class AnimationDocument(object):
	"""A fully validated collection of animation and static records."""

	def __init__(self, records):
		self.records = tuple(records)
		self.animation_count = sum(
			isinstance(record, AnimationRecord) for record in self.records)
		self.static_count = sum(
			isinstance(record, StaticRecord) for record in self.records)
		self.key_count = sum(
			len(record.keys) for record in self.records
			if isinstance(record, AnimationRecord))

	def counts(self):
		return {
			'animation_count': self.animation_count,
			'static_count': self.static_count,
			'key_count': self.key_count,
		}


class AnimationFormatError(ValueError):
	"""Raised when a DAnim stream is incomplete or malformed."""


class _LineCursor(object):
	def __init__(self, lines):
		self._lines = iter(bounded_text.checked_lines(
			lines, MAX_FILE_SIZE, MAX_LINE_LENGTH, AnimationFormatError))
		self.line_number = 0

	def next_content(self, allow_eof=False):
		while True:
			try:
				line = next(self._lines)
			except StopIteration:
				if allow_eof:
					return None
				raise AnimationFormatError(
					'Unexpected end of file after line {0}'.format(self.line_number))
			self.line_number += 1
			stripped = line.strip()
			if stripped and not stripped.startswith('#'):
				return stripped

	def error(self, message):
		raise AnimationFormatError(
			'Line {0}: {1}'.format(self.line_number, message))


def _finite_float(value, cursor, field_name):
	try:
		number = float(value)
	except (TypeError, ValueError):
		cursor.error('{0} must be numeric'.format(field_name))
	if math.isnan(number) or math.isinf(number):
		cursor.error('{0} must be finite'.format(field_name))
	return number


def _zero_or_one(value, cursor, field_name):
	if value not in ('0', '1'):
		cursor.error('{0} must be 0 or 1'.format(field_name))
	return int(value)


def _parse_static(header, cursor):
	tokens = header.split()
	if len(tokens) != 6:
		cursor.error('static record must contain six fields')
	has_parent = _zero_or_one(tokens[4], cursor, 'static parent flag')
	try:
		value = parse_static_value(tokens[5])
	except (TypeError, ValueError):
		cursor.error('static value must be a finite scalar or boolean')
	return StaticRecord(tokens[2], tokens[3], bool(has_parent), value)


def _expect_property(cursor, property_name, allowed_values=None):
	line = cursor.next_content()
	if not line.endswith(';'):
		cursor.error('{0} property must end with a semicolon'.format(property_name))
	tokens = line.rstrip(';').split()
	if len(tokens) != 2 or tokens[0] != property_name:
		cursor.error('expected {0} property'.format(property_name))
	if allowed_values is not None and tokens[1] not in allowed_values:
		cursor.error('invalid {0} value'.format(property_name))
	return tokens[1]


def _parse_key(line, cursor):
	if not line.endswith(';'):
		cursor.error('animation key must end with a semicolon')
	tokens = line.rstrip(';').split()
	if len(tokens) < 7:
		cursor.error('animation key has too few fields')
	in_tangent = tokens[2]
	out_tangent = tokens[3]
	if not TANGENT_NAME_RE.match(in_tangent) or not TANGENT_NAME_RE.match(out_tangent):
		cursor.error('animation key has an invalid tangent name')
	if in_tangent not in VALID_TANGENT_NAMES or out_tangent not in VALID_TANGENT_NAMES:
		cursor.error('animation key has an unsupported Maya 2019 tangent type')
	expected_fields = 7
	if in_tangent == 'fixed':
		expected_fields += 2
	if out_tangent == 'fixed':
		expected_fields += 2
	if len(tokens) != expected_fields:
		cursor.error(
			'animation key has {0} fields; expected {1}'.format(
				len(tokens), expected_fields))

	time = _finite_float(tokens[0], cursor, 'key time')
	value = _finite_float(tokens[1], cursor, 'key value')
	tangent_locked = _zero_or_one(tokens[4], cursor, 'tangent lock')
	weight_locked = _zero_or_one(tokens[5], cursor, 'weight lock')
	breakdown = _zero_or_one(tokens[6], cursor, 'breakdown flag')

	index = 7
	in_angle = None
	in_weight = None
	out_angle = None
	out_weight = None
	if in_tangent == 'fixed':
		in_angle = _finite_float(tokens[index], cursor, 'fixed in angle')
		in_weight = _finite_float(tokens[index + 1], cursor, 'fixed in weight')
		index += 2
	if out_tangent == 'fixed':
		out_angle = _finite_float(tokens[index], cursor, 'fixed out angle')
		out_weight = _finite_float(tokens[index + 1], cursor, 'fixed out weight')

	return AnimationKey(
		time, value, in_tangent, out_tangent, tangent_locked,
		weight_locked, breakdown, in_angle, in_weight, out_angle, out_weight)


def _parse_animation(header, cursor, key_budget):
	tokens = header.split()
	if len(tokens) != 7 or not tokens[-1].endswith(';'):
		cursor.error('anim record must contain seven fields and end with a semicolon')
	has_parent = _zero_or_one(tokens[4], cursor, 'animation parent flag')
	if cursor.next_content() != 'animData {':
		cursor.error('expected animData block')
	weighted_value = _expect_property(
		cursor, 'weighted', ('0', '1', 'True', 'False'))
	pre_infinity = _expect_property(
		cursor, 'preInfinity', VALID_INFINITY_NAMES)
	post_infinity = _expect_property(
		cursor, 'postInfinity', VALID_INFINITY_NAMES)
	if cursor.next_content() != 'keys {':
		cursor.error('expected keys block')

	keys = []
	line = cursor.next_content()
	while line != '}':
		if len(keys) >= key_budget:
			cursor.error('animation file exceeds the key-count limit')
		keys.append(_parse_key(line, cursor))
		line = cursor.next_content()
	if not keys:
		cursor.error('animation record must contain at least one key')
	if cursor.next_content() != '}':
		cursor.error('expected animData closing brace')
	return AnimationRecord(
		tokens[2], tokens[3], bool(has_parent),
		weighted_value in ('1', 'True'), pre_infinity, post_infinity,
		tuple(keys))


def parse_animation_lines(lines):
	"""Parse an iterable of DAnim lines into a validated document."""
	cursor = _LineCursor(lines)
	records = []
	total_keys = 0
	line = cursor.next_content(allow_eof=True)
	while line is not None:
		if len(records) >= MAX_RECORDS:
			cursor.error('animation file exceeds the record-count limit')
		if line.startswith('anim '):
			record = _parse_animation(line, cursor, MAX_KEYS - total_keys)
			total_keys += len(record.keys)
		elif line.startswith('static '):
			record = _parse_static(line, cursor)
		else:
			cursor.error('expected anim or static record')
		records.append(record)
		line = cursor.next_content(allow_eof=True)
	return AnimationDocument(records)


def validate_animation_lines(lines):
	"""Validate an iterable of DAnim lines and return record counts."""
	return parse_animation_lines(lines).counts()


def parse_animation_file(filename):
	"""Parse a bounded DAnim file while guaranteeing handle closure."""
	if os.path.getsize(filename) > MAX_FILE_SIZE:
		raise AnimationFormatError(
			'Animation file exceeds the {0} byte size limit'.format(MAX_FILE_SIZE))
	with open(filename, 'rb') as stream:
		return parse_animation_lines(bounded_text.read_stream_lines(
			stream, MAX_FILE_SIZE, MAX_LINE_LENGTH, AnimationFormatError))


def validate_animation_file(filename):
	"""Validate a DAnim file while guaranteeing that its handle is closed."""
	return parse_animation_file(filename).counts()
