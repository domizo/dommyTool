# -*- coding: utf-8 -*-
"""Validated, Maya-independent persistence for ``.elip`` edge lists."""

import collections
import os
import re

from DPyTools.core import bounded_text
from DPyTools.core import components
from DPyTools.core.atomic_text import AtomicTextWriter


MAX_FILE_SIZE = 1024 * 1024
MAX_LINE_LENGTH = MAX_FILE_SIZE
MAX_EDGE_COUNT = components.MAX_COMPONENT_COUNT
# Maya mesh edge IDs use the nonnegative signed-int domain.
MAX_EDGE_INDEX = components.MAX_COMPONENT_INDEX
LipEdgeData = collections.namedtuple('LipEdgeData', 'top bottom')
EDGE_INTEGER_RE = re.compile(r'^[+-]?[0-9]+$')

try:
	INTEGER_TYPES = (int, long)
	STRING_TYPES = (basestring,)
except NameError:
	INTEGER_TYPES = (int,)
	STRING_TYPES = (str,)


class LipEdgeFormatError(ValueError):
	"""Raised when a lip-edge file is incomplete or malformed."""


def _validate_edge_values(values, label):
	if isinstance(values, STRING_TYPES):
		raise LipEdgeFormatError('{0} edges must be an integer sequence'.format(label))
	try:
		iterator = iter(values)
	except TypeError:
		raise LipEdgeFormatError('{0} edges must be an integer sequence'.format(label))
	edges = []
	seen = set()
	for value in iterator:
		if len(edges) >= MAX_EDGE_COUNT:
			raise LipEdgeFormatError('{0} edge list exceeds the size limit'.format(label))
		if isinstance(value, bool) or not isinstance(value, INTEGER_TYPES + STRING_TYPES):
			raise LipEdgeFormatError('{0} edges must be integers'.format(label))
		if isinstance(value, STRING_TYPES):
			value = value.strip()
			if not EDGE_INTEGER_RE.match(value):
				raise LipEdgeFormatError('{0} edges must be integers'.format(label))
			# Reject oversized decimal IDs before expensive big-integer conversion.
			digits = value.lstrip('+-').lstrip('0') or '0'
			if len(digits) > 10:
				raise LipEdgeFormatError('{0} edge index exceeds the size limit'.format(label))
			value = ('-' if value.startswith('-') else '') + digits
		try:
			edge = int(value)
		except (TypeError, ValueError, OverflowError):
			raise LipEdgeFormatError('{0} edges must be integers'.format(label))
		try:
			edge = components.component_index(edge)
		except (TypeError, ValueError) as error:
			raise LipEdgeFormatError('{0}: {1}'.format(label, error))
		if edge in seen:
			raise LipEdgeFormatError('{0} edge list contains duplicates'.format(label))
		seen.add(edge)
		edges.append(edge)
	if not edges:
		raise LipEdgeFormatError('{0} edge list must not be empty'.format(label))
	return tuple(edges)


def create(top, bottom):
	"""Validate edge sequences and return immutable lip-edge data."""
	return LipEdgeData(
		_validate_edge_values(top, 'Top lip'),
		_validate_edge_values(bottom, 'Bottom lip'))


def parse_lines(lines):
	"""Parse exactly one ``TLIP`` and one ``BLIP`` record."""
	records = {}
	for line_number, line in enumerate(bounded_text.checked_lines(
			lines, MAX_FILE_SIZE, MAX_LINE_LENGTH, LipEdgeFormatError), 1):
		stripped = line.strip()
		if not stripped or stripped.startswith('#'):
			continue
		parts = stripped.split()
		label = parts[0]
		if label not in ('TLIP', 'BLIP'):
			raise LipEdgeFormatError(
				'Line {0}: unknown lip-edge record'.format(line_number))
		if label in records:
			raise LipEdgeFormatError(
				'Line {0}: duplicate {1} record'.format(line_number, label))
		records[label] = parts[1:]
	if set(records) != set(('TLIP', 'BLIP')):
		raise LipEdgeFormatError('Lip-edge file requires TLIP and BLIP records')
	return create(records['TLIP'], records['BLIP'])


def load(filename):
	"""Load a bounded lip-edge file while guaranteeing handle closure."""
	if os.path.getsize(filename) > MAX_FILE_SIZE:
		raise LipEdgeFormatError('Lip-edge file exceeds the size limit')
	with open(filename, 'rb') as stream:
		return parse_lines(bounded_text.read_stream_lines(
			stream, MAX_FILE_SIZE, MAX_LINE_LENGTH, LipEdgeFormatError))


def serialize(data):
	"""Return the canonical two-line representation of lip-edge data."""
	validated = create(data.top, data.bottom)
	lines = (
		'TLIP {0}\n'.format(' '.join(str(edge) for edge in validated.top)),
		'BLIP {0}\n'.format(' '.join(str(edge) for edge in validated.bottom)),
	)
	return ''.join(bounded_text.checked_lines(
		lines, MAX_FILE_SIZE, MAX_LINE_LENGTH, LipEdgeFormatError))


def save(filename, top, bottom):
	"""Validate and atomically replace a lip-edge file."""
	data = create(top, bottom)
	payload = bounded_text.encoded_bytes(serialize(data), LipEdgeFormatError)
	with AtomicTextWriter(filename, mode='wb') as writer:
		writer.write(payload)
	return data
