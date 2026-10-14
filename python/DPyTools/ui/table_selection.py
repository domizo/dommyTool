# -*- coding: utf-8 -*-
u"""Host-independent normalization of Maya scriptTable selection values."""

import numbers


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
MAX_TEXT_ITEMS = 10000


def selected_rows(value, row_count, multiple=False):
	u"""Return validated one-based rows; None/zero mean no single selection."""
	if (isinstance(row_count, bool) or not isinstance(row_count, numbers.Integral)
			or row_count < 0):
		raise ValueError('Table row count must be a non-negative integer')
	if value is None:
		return ()
	if multiple:
		if not isinstance(value, (list, tuple)):
			raise TypeError('Selected rows must be a list or tuple')
		rows = value
	else:
		if not isinstance(value, bool) and isinstance(value, numbers.Integral) and value == 0:
			return ()
		rows = (value,)
	if len(rows) > row_count:
		raise ValueError('Selected row count exceeds the table size')
	seen = set()
	for row in rows:
		if (isinstance(row, bool) or not isinstance(row, numbers.Integral)
				or not 1 <= row <= row_count):
			raise ValueError('Selected row must be a valid one-based integer')
		if row in seen:
			raise ValueError('Selected rows must not contain duplicates')
		seen.add(row)
	return tuple(rows)


def cell_text(value):
	u"""Accept plain text or a one-item query sequence without truncating IDs."""
	if isinstance(value, (list, tuple)) and len(value) == 1:
		value = value[0]
	if not isinstance(value, STRING_TYPES):
		raise TypeError('Selected cell must contain one text value')
	return value


def selected_text(value):
	"""Normalize one text-scroll choice; None/empty sequences mean no choice."""
	if value is None or (isinstance(value, (list, tuple)) and not value):
		return ''
	return cell_text(value)


def text_items(value):
	"""Snapshot a bounded, unique list model before changing its native view."""
	if value is None:
		return ()
	if not isinstance(value, (list, tuple)) or len(value) > MAX_TEXT_ITEMS:
		raise ValueError('Text list must be a bounded sequence')
	seen = set()
	for item in value:
		if not isinstance(item, STRING_TYPES) or not item or '\x00' in item:
			raise ValueError('Text list items must be nonempty text without NUL')
		if item in seen:
			raise ValueError('Text list contains duplicate items')
		seen.add(item)
	return tuple(value)


def list_selection(value, items):
	"""Keep a previous choice only if it remains in the admitted visible model."""
	items = text_items(items)
	selected = selected_text(value)
	return items, selected if selected in items else ''


def display_cell(rows, row, column):
	"""Return snapshot text only for admitted one-based coordinates; else blank."""
	if not isinstance(rows, (list, tuple)) or len(rows) > MAX_TEXT_ITEMS:
		return ''
	try:
		row_indices = selected_rows(row, len(rows))
		if not row_indices:
			return ''
		values = rows[row_indices[0] - 1]
		if not isinstance(values, (list, tuple)):
			return ''
		column_indices = selected_rows(column, len(values))
		return cell_text(values[column_indices[0] - 1]) if column_indices else ''
	except (TypeError, ValueError):
		return ''
