# -*- coding: utf-8 -*-
u"""Immutable display/connection snapshots for the historical Node Info View."""

from collections import namedtuple

from DPyTools.ui import table_selection


AttributeRow = namedtuple('AttributeRow', 'data cells source destinations')


def snapshot(data):
	u"""Freeze all cell/connection text before replacing a native table."""
	if not isinstance(data, (list, tuple)) or len(data) > table_selection.MAX_TEXT_ITEMS:
		raise ValueError('Attribute table exceeds the supported row limit')
	rows = []
	for item in data:
		source = table_selection.cell_text(item.m_src_node) if item.m_src_node is not None else ''
		destinations = item.m_dest_nodes
		if destinations is None:
			destinations = ()
		if not isinstance(destinations, (list, tuple)) or len(destinations) > table_selection.MAX_TEXT_ITEMS:
			raise ValueError('Attribute destinations must be a bounded sequence')
		destinations = tuple(table_selection.cell_text(value) for value in destinations)
		values = (item.m_name, item.m_type, item.m_size,
			'' if item.m_array_size == 0 else item.m_array_size,
			source, ':'.join(destinations), item.m_val)
		# Retain native Python 2 byte/unicode text without guessing its encoding.
		cells = tuple(value if isinstance(value, table_selection.STRING_TYPES)
			else table_selection.TEXT_TYPE(value) for value in values)
		rows.append(AttributeRow(item, cells, source, destinations))
	return tuple(rows)


def filter_rows(rows, text='', connected_only=False):
	text = table_selection.cell_text(text).lower()
	if not isinstance(connected_only, bool):
		raise TypeError('Connection filter must be Boolean')
	if not isinstance(rows, (list, tuple)) or len(rows) > table_selection.MAX_TEXT_ITEMS:
		raise ValueError('Attribute rows must be a bounded snapshot')
	if any(not isinstance(row, AttributeRow) for row in rows):
		raise TypeError('Attribute rows must be prepared snapshots')
	return tuple(row for row in rows if text in row.cells[0].lower()
		and (not connected_only or bool(row.source or row.destinations)))


def selected_cell(value, row_count):
	"""Admit exactly one one-based cell; no selection remains a no-op."""
	if value is None or (isinstance(value, (list, tuple)) and not value):
		return ()
	if not isinstance(value, (list, tuple)) or len(value) != 2:
		raise ValueError('Select exactly one attribute-table cell')
	rows = table_selection.selected_rows(value[0], row_count)
	columns = table_selection.selected_rows(value[1], 7)
	return (rows[0], columns[0]) if rows and columns else ()


def connection_targets(row, column):
	"""Prepare explicit node names, never wildcard scope or a valid prefix."""
	if column == 5:
		plugs = (row.source,) if row.source else ()
	elif column == 6:
		plugs = row.destinations
	else:
		return ()
	targets, seen = [], set()
	for plug in plugs:
		node, separator, attribute = plug.partition('.')
		if (not separator or not node or not attribute
				or any(character in node for character in '*?[]')
				or any(character.isspace() or ord(character) < 32 or ord(character) == 127 for character in plug)):
			raise ValueError('Connection must name an explicit node and attribute')
		if node not in seen:
			targets.append(node)
			seen.add(node)
	return tuple(targets)
