# -*- coding: utf-8 -*-
"""Complete one-mesh edge/face rivet plans and explicitly owned outputs."""

import sys

import pymel.core as pm

from DPyTools.core import geometry, strings
from DPyTools.core.numeric import finite_number
from DPyTools.features.rigging import follicles
from DPyTools.scene import groups, nodes, ownership, parameters, selection


MAX_RIVETS = 10000


def _create(kind):
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		plan = selection.mesh_component_plan(pm.ls(selection=True, flatten=True) or [],
			kind=kind, limit=MAX_RIVETS)
		if not plan:
			raise ValueError('Select mesh {0} components for rivets'.format(kind))
		node_type, index_attribute = (('DPolyEdgeToMat', 'EdgeNumber') if kind == 'edge'
			else ('DPolyFaceToMat', 'FaceNumber'))
		result = []
		for mesh, index in plan:
			frame = owned.create(node_type)
			pm.setAttr(frame.attr(index_attribute), index)
			pm.connectAttr(mesh.attr('worldMesh')[0], frame.attr('inMesh'))
			transform = owned.create('transform')
			owned.create('locator', parent=transform)
			pm.connectAttr(frame.attr('outTranslate'), transform.attr('translate'))
			pm.connectAttr(frame.attr('outRotate'), transform.attr('rotate'))
			result.append((frame, transform))
		completed = True
		return tuple(result)
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Rivets were not created: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)


def createEdgeRivet():
	return _create('edge')


def createFaceRivet():
	return _create('face')


def _vertex_plan(target_mesh, positions, uvs, rotation_link):
	mesh = selection.resolve_shape(target_mesh)
	if (not isinstance(rotation_link, bool) or not isinstance(positions, (list, tuple))
			or not isinstance(uvs, (list, tuple)) or not 1 <= len(positions) <= MAX_RIVETS
			or len(positions) != len(uvs)):
		raise ValueError('Vertex rivets require bounded matching positions/UVs and a Boolean rotation option')
	points = tuple(geometry.position3(point) for point in positions)
	coordinates = []
	for uv in uvs:
		if not isinstance(uv, (list, tuple)) or len(uv) != 2:
			raise ValueError('Every vertex rivet requires one UV pair')
		coordinates.append(tuple(finite_number(value, 'Rivet UV') for value in uv))
	width = len(str(len(points)))
	rows, names = [], ['rivet_fol_grp', 'rivet_jnt_grp']
	for index, (point, uv) in enumerate(zip(points, coordinates), 1):
		padding = strings.pad_number(index, width)
		fol_name, offset_name, joint_name = tuple(parameters.control_name(prefix + padding)
			for prefix in ('rivet_fol_', 'rivet_ofset_', 'rivet_jnt_'))
		rows.append((point, uv, fol_name, offset_name, joint_name))
		names.extend((fol_name, fol_name + 'Shape', offset_name, joint_name))
	if any(pm.objExists(parameters.control_name(name)) for name in names):
		raise ValueError('A requested vertex-rivet name already exists')
	return mesh, tuple(rows)


def createVertexRivets(target_mesh, positions, uvs, rotation_link=False):
	"""Build only an admitted vertex batch; clean confirmed graph nodes on failure."""
	selected = pm.ls(selection=True) or []
	owned = ownership.SceneNodes()
	completed = failed = False
	count = 0
	try:
		mesh, plan = _vertex_plan(target_mesh, positions, uvs, rotation_link)
		follicle_group = owned.adopt(groups.createEmpty('rivet_fol_grp'))
		joint_group = owned.adopt(groups.createEmpty('rivet_jnt_grp'))
		created_follicles, created_joints = [], []
		for point, uv, fol_name, offset_name, joint_name in plan:
			follicle = follicles.create_follicle(mesh, uv, owner=owned, name=fol_name, parent=follicle_group)
			if follicle is None or (isinstance(follicle, bool) and not follicle):
				raise RuntimeError('Follicle creation was not confirmed')
			follicle_parent = follicle.getParent()
			offset = owned.adopt(groups.createEmpty(offset_name, position=point))
			parameters.set_values(offset, {'inheritsTransform': False})
			joint = owned.adopt(nodes.createNodeWithParam('joint', joint_name))
			pm.xform(joint, worldSpace=True, translation=point)
			pm.parent(joint, offset)
			pm.parent(offset, joint_group)
			pm.connectAttr(follicle_parent.attr('translate'), offset.attr('translate'))
			if rotation_link:
				pm.connectAttr(follicle_parent.attr('rotate'), offset.attr('rotate'))
			created_follicles.append(follicle)
			created_joints.append(joint)
			count += 1
		completed = True
		return follicle_group, joint_group, tuple(created_follicles), tuple(created_joints)
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Rivet creation stopped after {0} completed rivets; known-node cleanup is attempted: {1}'.format(count, error))
		return False
	finally:
		try:
			if not completed:
				exception_type = sys.exc_info()[0]
				owned.cleanup(active_error=failed or exception_type is not None)
		finally:
			selection.restore_selection(selected)
