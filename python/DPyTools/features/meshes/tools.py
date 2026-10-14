# -*- coding: utf-8 -*-
"""Checked world-space queries for the historical mesh helper API."""
import maya.OpenMaya as om

from DPyTools.core.geometry import position3
from DPyTools.core.components import component_index
from DPyTools.features.meshes import api
from DPyTools.scene.selection import resolve_shape


def getSelectedMeshIds(target_mesh=None):
	u"""Return selected vertex IDs/world positions from one admitted mesh.

	Transforms, other components and mixed meshes are rejected rather than
	interpreted as a whole-mesh vertex selection or silently truncated.
	"""
	selection = om.MSelectionList()
	om.MGlobal.getActiveSelectionList(selection)
	if selection.length() == 0:
		return [], []
	expected_path = (resolve_shape(target_mesh).longName()
		if target_mesh is not None else None)
	components = []
	for index in range(selection.length()):
		path = om.MDagPath()
		component = om.MObject()
		selection.getDagPath(index, path, component)
		if component.isNull() or not component.hasFn(om.MFn.kMeshVertComponent):
			raise ValueError('Select mesh vertices only')
		shape_path = resolve_shape(path.fullPathName()).longName()
		if expected_path is None:
			expected_path = shape_path
		if shape_path != expected_path:
			raise ValueError('Selected vertices must belong to the target mesh')
		components.append((path, component))
	ids = []
	positions = []
	seen = set()
	for path, component in components:
		iterator = om.MItMeshVertex(path, component)
		while not iterator.isDone():
			vertex_id = component_index(iterator.index())
			if vertex_id in seen:
				raise ValueError('Selected vertex IDs must be unique')
			seen.add(vertex_id)
			point = iterator.position(om.MSpace.kWorld)
			positions.append(position3((point.x, point.y, point.z)))
			ids.append(vertex_id)
			iterator.next()
	return ids, positions


def getClosestPointFromPoint(m_mesh='', pt_list=None):
	u"""Return finite world-space closest points for finite world-space inputs."""
	positions = [position3(point) for point in (pt_list or [])]
	if not positions:
		return []
	mesh_fn = api.getMeshFn(m_object=m_mesh)
	result = []
	for position in positions:
		closest = om.MPoint(float('nan'), float('nan'), float('nan'))
		mesh_fn.getClosestPoint(om.MPoint(*position), closest, om.MSpace.kWorld)
		result.append(list(position3((closest.x, closest.y, closest.z))))
	return result
