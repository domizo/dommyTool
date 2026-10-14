# -*- coding: utf-8 -*-
u"""Byte-budgeted text lines shared by legacy host-independent parsers."""


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)

try:
	INTEGER_TYPES = (int, long)
except NameError:
	INTEGER_TYPES = (int,)


def _validate_limits(max_bytes, max_line_bytes, error_type):
	if any(isinstance(value, bool) or not isinstance(value, INTEGER_TYPES) or value <= 0
			for value in (max_bytes, max_line_bytes)):
		raise error_type('Byte budgets must be positive integers')


def _byte_length(line, error_type):
	if not isinstance(line, STRING_TYPES):
		raise error_type('Text lines must be strings')
	return len(encoded_bytes(line, error_type))


def encoded_bytes(value, error_type):
	u"""Encode Unicode as UTF-8 without re-encoding legacy Python 2 byte strings."""
	try:
		if isinstance(value, TEXT_TYPE):
			return value.encode('utf-8')
		if isinstance(value, bytes):
			return value
	except UnicodeError:
		raise error_type('Text line cannot be represented as UTF-8')
	raise error_type('Text output must be a string or bytes')


class TextBudget(object):
	u"""Admit encoded output chunks transactionally, including split physical lines."""

	def __init__(self, max_bytes, max_line_bytes, error_type):
		_validate_limits(max_bytes, max_line_bytes, error_type)
		self.max_bytes = max_bytes
		self.max_line_bytes = max_line_bytes
		self.error_type = error_type
		self.total_bytes = 0
		self.line_bytes = 0

	def admit(self, value):
		if not isinstance(value, STRING_TYPES + (bytes,)):
			raise self.error_type('Text output must be a string or bytes')
		if len(value) > self.max_bytes - self.total_bytes:
			raise self.error_type('Text output exceeds the byte size limit')
		newline = u'\n' if isinstance(value, TEXT_TYPE) else b'\n'
		first_newline = value.find(newline)
		if (first_newline in (-1, len(value) - 1)
				and self.line_bytes + len(value) > self.max_line_bytes):
			raise self.error_type('Text output line exceeds the size limit')
		data = encoded_bytes(value, self.error_type)
		if len(data) > self.max_bytes - self.total_bytes:
			raise self.error_type('Text output exceeds the byte size limit')
		line_bytes = self.line_bytes
		position = 0
		while position < len(data):
			end = data.find(b'\n', position)
			length = (len(data) if end < 0 else end + 1) - position
			line_bytes += length
			if line_bytes > self.max_line_bytes:
				raise self.error_type('Text output line exceeds the size limit')
			if end < 0:
				break
			line_bytes = 0
			position = end + 1
		self.total_bytes += len(data)
		self.line_bytes = line_bytes
		return data


def checked_lines(lines, max_bytes, max_line_bytes, error_type):
	u"""Count all supplied lines, including comments, before parser allocation."""
	_validate_limits(max_bytes, max_line_bytes, error_type)
	total_bytes = 0
	for line_number, line in enumerate(lines, 1):
		if isinstance(line, STRING_TYPES) and len(line) > max_line_bytes:
			raise error_type('Line {0} exceeds the size limit'.format(line_number))
		length = _byte_length(line, error_type)
		if length > max_line_bytes:
			raise error_type('Line {0} exceeds the size limit'.format(line_number))
		total_bytes += length
		if total_bytes > max_bytes:
			raise error_type('Text input exceeds the byte size limit')
		yield line


def read_stream_lines(stream, max_bytes, max_line_bytes, error_type):
	u"""Read bounded binary lines; the caller retains ownership of the stream.

	Native Python 2 retains byte strings, matching the archived file readers.
	Python 3 test hosts decode UTF-8; this does not migrate the Maya baseline.
	"""
	_validate_limits(max_bytes, max_line_bytes, error_type)
	total_bytes = 0
	line_number = 0
	while True:
		# Probe at most one byte beyond either budget, including at exact EOF.
		line = stream.readline(min(max_line_bytes + 1, max_bytes - total_bytes + 1))
		if not line:
			return
		line_number += 1
		if len(line) > max_line_bytes:
			raise error_type('Line {0} exceeds the size limit'.format(line_number))
		total_bytes += len(line)
		if total_bytes > max_bytes:
			raise error_type('Text input exceeds the byte size limit')
		if not isinstance(line, str):
			try:
				line = line.decode('utf-8')
			except UnicodeError:
				raise error_type('Text input is not valid UTF-8')
		yield line
