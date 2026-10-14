# -*- coding: utf-8 -*-
import pymel.core as pm


from DPyTools.core import geometry
from DPyTools.features.meshes import tools as mesh_tools
from DPyTools.scene import parameters


'''
to run:
	>>> import DPyTools.joint.JointOp as jntOp

'''


def createSymmetryJoint(jnt_list=None):
	u"""Compatibility entry point for an unimplemented symmetry-joint prototype."""
	pm.warning('createSymmetryJoint is an archived, unimplemented idea; no joints were created.')
	return False




def _editable_fit_joint(value):
	"""Reuse node/port admission without treating world XYZ as local values."""
	node = parameters.editable_node(value)
	if node.nodeType() != 'joint':
		raise ValueError('Joint fitting requires joint nodes')
	parameters.writable_port(node.attr('translate'))
	return node


def _joint_fit_plan(target_mesh, joint_list):
	values = [] if joint_list is None else joint_list
	nodes = parameters.resolve_nodes(values, allow_empty=True)
	if not nodes:
		return ()
	nodes = [_editable_fit_joint(node) for node in nodes]
	positions = [geometry.position3(pm.xform(node, q=True, ws=True, t=True))
		for node in nodes]
	closest = mesh_tools.getClosestPointFromPoint(m_mesh=target_mesh, pt_list=positions)
	if not isinstance(closest, (list, tuple)) or len(closest) != len(nodes):
		raise ValueError('Closest-point results must match the complete joint batch')
	targets = [geometry.position3(point) for point in closest]
	return tuple(zip(nodes, targets))


def fitJointToMesh(target_mesh='', joint_list=None):
	u"""Fit a fully admitted world-space batch; return completed native write count.

	Expected failures report the returned-write prefix. A command that changes
	state then throws may also leave edits; no automatic Undo is attempted.
	"""
	completed = 0
	try:
		plan = _joint_fit_plan(target_mesh, joint_list)
		for node, position in plan:
			_editable_fit_joint(node)
			pm.xform(node, ws=True, t=position)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Joint fitting stopped after {0} completed writes; native edits may remain: {1}'.format(completed, error))
	return completed







