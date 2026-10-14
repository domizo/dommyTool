# -*- coding: utf-8 -*-
u"""Validated JSON reads and atomic JSON writes."""

import json
import math
import os
import re
import sys

from DPyTools.core import bounded_text, file_publish
from DPyTools.core.atomic_file import StagedFile, validate_destination


_MISSING = object()
MAX_FILE_SIZE = 16 * 1024 * 1024
MAX_NESTING_DEPTH = 64
_SURROGATE_PAIR = re.compile(u'[\ud800-\udbff][\udc00-\udfff]')

try:
	INTEGER_TYPES = (int, long)
except NameError:
	INTEGER_TYPES = (int,)

try:
	_UNICODE_CHARACTER = unichr
except NameError:
	_UNICODE_CHARACTER = chr


def _validate_type(data, expected_type, filename):
	if expected_type is not None and not isinstance(data, expected_type):
		raise TypeError(
			'Expected {0} in {1}, found {2}'.format(
				expected_type, filename, type(data)))
	return data


def _validate_max_file_size(max_file_size):
	if isinstance(max_file_size, bool) or not isinstance(max_file_size, INTEGER_TYPES):
		raise TypeError('max_file_size must be an integer')
	if max_file_size <= 0:
		raise ValueError('max_file_size must be greater than zero')
	return max_file_size


def _validate_input_nesting(payload):
	u"""Bound containers before decoding; quoted/escaped delimiters are data."""
	containers = []
	quoted = False
	escaped = False
	for character in payload:
		if quoted:
			if escaped:
				escaped = False
			elif character == '\\':
				escaped = True
			elif character == '"':
				quoted = False
		elif character == '"':
			quoted = True
		elif character in ('[', '{'):
			if len(containers) >= MAX_NESTING_DEPTH:
				raise ValueError('JSON nesting exceeds the depth limit')
			containers.append(character)
		elif character in (']', '}'):
			opening = '[' if character == ']' else '{'
			if not containers or containers.pop() != opening:
				raise ValueError('JSON contains mismatched container delimiters')
	if containers or quoted:
		raise ValueError('JSON contains an incomplete container or string')


def _unique_object(pairs):
	data = {}
	for key, value in pairs:
		if key in data:
			raise ValueError('Duplicate JSON object key')
		data[key] = value
	return data


def _reject_constant(unused_value):
	raise ValueError('Non-finite JSON number is not supported')


def _finite_float(value):
	number = float(value)
	if math.isnan(number) or math.isinf(number):
		_reject_constant(value)
	return number


def _text_value(value, remaining):
	if len(value) + 2 > remaining:
		raise ValueError('Serialized JSON exceeds the byte size limit')
	if isinstance(value, bounded_text.TEXT_TYPE):
		return value
	return value.decode('utf-8')


def _string_size(value, remaining, ensure_ascii):
	u"""Admit one escaped string before the standard encoder allocates it."""
	value = _text_value(value, remaining)
	size = 2
	index = 0
	while index < len(value):
		character = value[index]
		code = ord(character)
		if character in ('"', '\\', '\b', '\f', '\n', '\r', '\t'):
			size += 2
		elif code < 32:
			size += 6
		elif ensure_ascii:
			size += 1 if code < 127 else (6 if code <= 0xffff else 12)
		elif code <= 0x7f:
			size += 1
		elif code <= 0x7ff:
			size += 2
		elif 0xd800 <= code <= 0xdbff and index + 1 < len(value) and (
				0xdc00 <= ord(value[index + 1]) <= 0xdfff):
			# A narrow native Python 2 Unicode build represents this as a pair.
			size += 4
			index += 1
		elif 0xd800 <= code <= 0xdfff:
			raise ValueError('JSON text cannot be represented as UTF-8')
		else:
			size += 3 if code <= 0xffff else 4
		if size > remaining:
			raise ValueError('Serialized JSON exceeds the byte size limit')
		index += 1
	return size


def _integer_text(value, remaining):
	# A lower decimal-digit bound avoids expanding an already oversized long.
	minimum = max(1, (value.bit_length() - 1) * 301 // 1000 + 1)
	if minimum + (1 if value < 0 else 0) > remaining:
		raise ValueError('Serialized JSON integer exceeds the byte size limit')
	return str(value)


def _combine_surrogates(match):
	pair = match.group(0)
	code = 0x10000 + ((ord(pair[0]) - 0xd800) << 10) + ord(pair[1]) - 0xdc00
	return _UNICODE_CHARACTER(code)


def _object_children(data, max_file_size):
	u"""Keep standard key coercion, rejecting collisions in its JSON spelling."""
	seen = set()
	for key in data:
		if isinstance(key, bounded_text.STRING_TYPES):
			name = _text_value(key, max_file_size)
		elif isinstance(key, bool):
			name = 'true' if key else 'false'
		elif key is None:
			name = 'null'
		elif isinstance(key, INTEGER_TYPES):
			name = _integer_text(key, max_file_size)
		elif isinstance(key, float):
			_finite_float(key)
			name = float.__repr__(key)
		else:
			raise TypeError('JSON object key has an unsupported type')
		if sys.maxunicode > 0xffff:
			# Match JSON decoding of escaped UTF-16 pairs on wide Unicode builds.
			name = _SURROGATE_PAIR.sub(_combine_surrogates, name)
		if name in seen:
			raise ValueError('Duplicate JSON object key after key conversion')
		seen.add(name)
		yield name
		yield data[key]


def _validate_output(data, max_file_size, indent, ensure_ascii):
	u"""Iteratively admit depth, cycles, scalars and minimum output bytes.

	Only active ancestry/iterators are retained, not a second object graph.
	Actual indentation/separator bytes are counted during streaming.
	"""
	if indent is not None and not isinstance(indent, INTEGER_TYPES):
		raise TypeError('JSON indent must be an integer or None')
	minimum_bytes = 1  # Reserve the final LF before admitting any value.
	active = set()
	stack = [(iter((data,)), None)]
	while stack:
		children, identity = stack[-1]
		try:
			value = next(children)
		except StopIteration:
			stack.pop()
			if identity is not None:
				active.remove(identity)
			continue
		remaining = max_file_size - minimum_bytes
		if isinstance(value, (dict, list, tuple)):
			identity = id(value)
			if identity in active:
				raise ValueError('Circular JSON container reference')
			depth = len(active) + 1
			if depth > MAX_NESTING_DEPTH:
				raise ValueError('JSON nesting exceeds the depth limit')
			if value and indent is not None and indent > 0 and (
					indent * depth + 1 > max_file_size):
				raise ValueError('JSON indentation exceeds the byte size limit')
			minimum_bytes += 2 + max(0, len(value) - 1)
			if isinstance(value, dict):
				minimum_bytes += len(value)  # At least one colon per member.
				iterator = _object_children(value, max_file_size)
			else:
				iterator = iter(value)
			active.add(identity)
			stack.append((iterator, identity))
		elif isinstance(value, bounded_text.STRING_TYPES):
			minimum_bytes += _string_size(value, remaining, ensure_ascii)
		elif value is None:
			minimum_bytes += 4
		elif isinstance(value, bool):
			minimum_bytes += 4 if value else 5
		elif isinstance(value, INTEGER_TYPES):
			minimum_bytes += len(_integer_text(value, remaining))
		elif isinstance(value, float):
			_finite_float(value)
			minimum_bytes += len(float.__repr__(value))
		else:
			raise TypeError('JSON value has an unsupported type')
		if minimum_bytes > max_file_size:
			raise ValueError('Serialized JSON exceeds the byte size limit')


def _output_chunks(encoder, data):
	try:
		for chunk in encoder.iterencode(data):
			yield chunk
	except RuntimeError:
		raise ValueError('JSON encoder could not process the admitted nesting')
	yield '\n'


def load(filename, default=_MISSING, expected_type=None,
		max_file_size=MAX_FILE_SIZE):
	u"""Read bounded, unambiguous, finite JSON and validate its top-level type."""
	max_file_size = _validate_max_file_size(max_file_size)
	if not os.path.isfile(filename):
		if default is _MISSING:
			raise IOError('JSON file does not exist: {0}'.format(filename))
		return _validate_type(default, expected_type, filename)

	if os.path.getsize(filename) > max_file_size:
		raise ValueError('JSON file is too large: {0}'.format(filename))
	with open(filename, 'rb') as stream:
		payload = stream.read(max_file_size + 1)
	if len(payload) > max_file_size:
		raise ValueError('JSON file is too large: {0}'.format(filename))
	payload = payload.decode('utf-8')
	_validate_input_nesting(payload)
	try:
		data = json.loads(
			payload, object_pairs_hook=_unique_object,
			parse_constant=_reject_constant, parse_float=_finite_float)
	except RuntimeError:
		# Python 2 reports decoder recursion exhaustion as RuntimeError.
		# Keep malformed input on the existing UI ValueError path.
		raise ValueError('JSON decoder could not process the admitted nesting')
	return _validate_type(data, expected_type, filename)


def _unicode_path(value):
	return file_publish.unicode_path(value)


def _move_file_windows(source, destination, replace_existing):
	return file_publish.move_file_windows(source, destination, replace_existing)


def replace_file(source, destination):
	"""Compatibility entry point; maintained ownership uses file_publish."""
	return file_publish.replace_file(source, destination)


def publish_file_no_replace(source, destination):
	u"""Publish a sealed sibling file without overwriting an existing path.

	On POSIX the source link remains owned by the caller for later cleanup.
	Unsupported hard-link filesystems fail rather than use an overwrite race.
	"""
	return file_publish.publish_file_no_replace(source, destination)


def dump(filename, data, indent=4, ensure_ascii=True,
		max_file_size=MAX_FILE_SIZE, replace_existing=True):
	u"""Stream admitted JSON; optionally publish only to an absent destination."""
	if not isinstance(replace_existing, bool):
		raise TypeError('JSON replacement policy must be Boolean')
	max_file_size = _validate_max_file_size(max_file_size)
	validate_destination(filename)
	_validate_output(data, max_file_size, indent, ensure_ascii)
	if indent is not None and (indent < 0 or not isinstance(data, (dict, list, tuple)) or not data):
		# Scalar/empty output has no indentation; avoid unused huge repeats.
		# Negative indentation likewise retains newline-only formatting.
		indent = 0
	encoder = json.JSONEncoder(indent=indent, ensure_ascii=ensure_ascii, allow_nan=False)
	budget = bounded_text.TextBudget(max_file_size, max_file_size, ValueError)

	directory = os.path.dirname(os.path.abspath(filename))
	if not os.path.isdir(directory):
		raise IOError('JSON directory does not exist: {0}'.format(directory))

	with StagedFile(filename, mode='wb') as staged:
		for chunk in _output_chunks(encoder, data):
			payload = budget.admit(chunk)
			written = staged.stream.write(payload)
			if written is not None and written != len(payload):
				raise IOError('JSON stream did not write the complete chunk')
		staged.seal()
		staged.preserve_destination_mode(filename)
		if replace_existing:
			staged.publish(filename)
		else:
			staged.publish(filename, replace_existing=False)
	return filename
