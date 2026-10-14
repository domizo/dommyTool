# -*- coding: utf-8 -*-
"""Admitted ordered mesh-edge curves using explicitly owned native history."""

import pymel.core as pm

from DPyTools.core import components, geometry


MAX_EDGE_CURVE_EDGES = 1000


def plan_edge_curve(component_plan):
	if not isinstance(component_plan, (list, tuple)) or not 1 <= len(component_plan) <= MAX_EDGE_CURVE_EDGES:
		raise ValueError('Edge curve requires 1..1000 admitted edges')
	if not isinstance(component_plan[0], (list, tuple)) or len(component_plan[0]) != 2:
		raise ValueError('Edge curve plan must contain mesh/index pairs')
	mesh = component_plan[0][0]
	indices, pairs = [], []
	for row in component_plan:
		if not isinstance(row, (list, tuple)) or len(row) != 2:
			raise ValueError('Edge curve plan must contain mesh/index pairs')
		owner, index = row
		if owner != mesh:
			raise ValueError('Edge curve inputs must belong to one mesh')
		index = components.component_index(index)
		edge = mesh.e[index]
		vertices = list(edge.connectedVertices())
		if len(vertices) != 2:
			raise ValueError('Mesh edge must contain exactly two vertices')
		positions = [geometry.position3(pm.xform(vertex, q=True, ws=True, t=True)) for vertex in vertices]
		geometry.direction3(tuple(end - start for start, end in zip(*positions)))
		pairs.append(tuple(vertex.index() for vertex in vertices))
		indices.append(index)
	if len(set(indices)) != len(indices):
		raise ValueError('Edge curve input repeats an edge')
	return mesh, tuple(indices), components.ordered_edge_directions(pairs)


def edge_curve_output(owned, mesh, indices, directions):
	"""Caller owns cleanup; no shared-name hierarchy or borrowed node is edited."""
	if (not isinstance(indices, (list, tuple)) or not 1 <= len(indices) <= MAX_EDGE_CURVE_EDGES
			or not isinstance(directions, (list, tuple)) or len(directions) != len(indices)
			or any(not isinstance(value, bool) for value in directions)):
		raise ValueError('Edge curve construction requires a complete admitted direction plan')
	indices = tuple(components.component_index(index) for index in indices)
	if len(set(indices)) != len(indices):
		raise ValueError('Edge curve construction repeats an edge')
	output = None
	for index, (edge_id, reverse) in enumerate(zip(indices, directions)):
		edge = owned.create('curveFromMeshEdge')
		pm.setAttr(edge.attr('edgeIndex')[0], edge_id)
		pm.connectAttr(mesh.attr('worldMesh')[0], edge.attr('inputMesh'))
		if output is None:
			output = edge.attr('outputCurve')
		else:
			attach = owned.create('attachCurve')
			pm.setAttr(attach.attr('method'), 0)
			pm.setAttr(attach.attr('keepMultipleKnots'), True)
			pm.setAttr(attach.attr('reverse1'), directions[0] if index == 1 else False)
			pm.setAttr(attach.attr('reverse2'), reverse)
			pm.connectAttr(output, attach.attr('inputCurve1'))
			pm.connectAttr(edge.attr('outputCurve'), attach.attr('inputCurve2'))
			output = attach.attr('outputCurve')
	return output
