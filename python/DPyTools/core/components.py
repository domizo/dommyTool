# -*- coding: utf-8 -*-
"""Shared native component-index limits without Maya imports."""

import numbers


MAX_COMPONENT_INDEX = 0x7fffffff
MAX_COMPONENT_COUNT = 100000


def component_index(value):
	"""Admit one non-Boolean integer in Maya's nonnegative signed-int domain."""
	if isinstance(value, bool) or not isinstance(value, numbers.Integral):
		raise TypeError('Component index must be an integer')
	if value < 0 or value > MAX_COMPONENT_INDEX:
		raise ValueError('Component index is outside the supported range')
	return int(value)


def ordered_edge_directions(pairs):
	"""Return reversal flags for one ordered path (optionally closing once)."""
	if not isinstance(pairs, (list, tuple)) or not 1 <= len(pairs) <= MAX_COMPONENT_COUNT:
		raise ValueError('Edge chain must be a bounded, nonempty sequence')
	edges = []
	seen_edges = set()
	for pair in pairs:
		if not isinstance(pair, (list, tuple)) or len(pair) != 2:
			raise ValueError('Each edge must have two vertex IDs')
		first, second = [component_index(value) for value in pair]
		key = tuple(sorted((first, second)))
		if first == second or key in seen_edges:
			raise ValueError('Edge chain contains a collapsed or duplicate edge')
		seen_edges.add(key)
		edges.append((first, second))
	reverse = len(edges) > 1 and edges[0][1] not in edges[1]
	start, end = edges[0][::-1] if reverse else edges[0]
	result = [reverse]
	visited = set((start, end))
	for index, (first, second) in enumerate(edges[1:], 1):
		if first == end:
			next_vertex, reversed_edge = second, False
		elif second == end:
			next_vertex, reversed_edge = first, True
		else:
			raise ValueError('Edges must be selected as one continuous ordered chain')
		if next_vertex in visited and not (index == len(edges) - 1 and next_vertex == start):
			raise ValueError('Edge chain branches or closes before its final edge')
		visited.add(next_vertex)
		end = next_vertex
		result.append(reversed_edge)
	return tuple(result)
