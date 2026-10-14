# -*- coding: utf-8 -*-
"""Historical joint operations with protected axis-correction cleanup."""

import sys

import pymel.core as pm
import numbers

import DPyTools.general.stringActions as DPTStr
from DPyTools.core import geometry
from DPyTools.core import strings
from DPyTools.scene import ownership


MAX_SPLIT_JOINTS = 10000


'''
to run:
	>>> import DPyTools.rigging.jointActions as DPTJoint
	>>> DPTJoint.correctJointAxis()

'''

def getJointChilds(joint=None):
	return pm.listRelatives(joint, ad=True, children=True, type='joint') or []


def showLocalAxis(rootJoint='', visible=True):
	for jnt in getJointChilds(rootJoint):
		jnt.setAttr('displayLocalAxis',visible)


def duplicateSameJointSet(topJoint='', parent='', nameDic=None):
	if nameDic is None:
		nameDic = {}

	newTopJoint = pm.duplicate(topJoint, n=DPTStr.replaceText(topJoint,nameDic))
	pm.parent(newTopJoint,parent)
	childs = getJointChilds(newTopJoint)

	newJoints=[newTopJoint[0]]
	newJoints.extend(childs)

	for c in childs:
		newname = DPTStr.replaceText(c,nameDic)
		pm.rename(c, newname)

	return newJoints

def _cleanup_joint_axis(joint, children, constraint, active_error):
	"""Attempt every known cleanup; never replace an active operation failure."""
	errors = []
	if constraint is not None:
		try:
			pm.delete(constraint)
		except (RuntimeError, TypeError, ValueError) as error:
			errors.append(error)
	for child in children:
		try:
			pm.parent(child, joint)
		except (RuntimeError, TypeError, ValueError) as error:
			errors.append(error)
	if errors:
		if not active_error:
			raise errors[0]
		try:
			pm.warning('Joint axis cleanup also failed: {0}'.format(errors[0]))
		except (RuntimeError, TypeError, ValueError):
			# Diagnostic failure must not hide the original command failure.
			pass


def correctJointAxis(joint=None, aimAxis=(1, 0, 0), upAxis=(0, 1, 0),
		upDir=(0, 1, 0)):
	"""Orient toward a child (or parent) while restoring detached children.

	The supplied aim/up frame defines the orientation. A single-joint call has
	no previous-up state, so it must not invent a continuity-based 180-degree
	flip. Completed orientation edits are not automatically rolled back.
	"""
	if joint is None:
		return
	aim_axis, up_axis = geometry.independent_directions(aimAxis, upAxis)
	up_direction = geometry.direction3(upDir)
	joint = pm.PyNode(joint)
	if joint.nodeType() != 'joint' or joint.isInstanced():
		raise ValueError('Axis correction requires a non-instanced joint')
	children = [pm.PyNode(child) for child in
		(pm.listRelatives(joint, children=True, type=('transform', 'joint')) or [])]
	if any(child.isInstanced() for child in children):
		raise ValueError('Axis correction requires non-instanced children')
	parents = pm.listRelatives(joint, parent=True) or []
	parent = pm.PyNode(parents[0]) if parents else None
	aim_target = next((child for child in children
		if child.nodeType() == 'joint'), None)
	attempted_children = []
	constraint = None
	try:
		for child in children:
			# Register before dispatch: a command may change state then fail.
			attempted_children.append(child)
			pm.parent(child, world=True)
		if aim_target is not None:
			constraint = pm.aimConstraint(aim_target, joint, aim=aim_axis,
				upVector=up_axis, worldUpVector=up_direction,
				worldUpType='vector', weight=1.0)
		elif parent is not None:
			constraint = pm.orientConstraint(parent, joint, weight=1.0)
		else:
			return
		if not constraint:
			raise RuntimeError('Joint orientation constraint creation was not confirmed')
		pm.delete(constraint)
		constraint = None
		pm.joint(joint, e=True, zso=True)
		pm.makeIdentity(joint, apply=True)
	finally:
		exception_type = sys.exc_info()[0]
		_cleanup_joint_axis(joint, attempted_children, constraint,
			exception_type is not None)


def _axis_targets(values):
	if not isinstance(values, (list, tuple)) or len(values) > MAX_SPLIT_JOINTS:
		raise ValueError('Joint input must be a bounded sequence')
	nodes = [pm.PyNode(value) for value in values]
	if len(set(nodes)) != len(nodes):
		raise ValueError('Joint input contains duplicate identities')
	for node in nodes:
		if node.nodeType() != 'joint' or node.isInstanced():
			raise ValueError('Joint input must contain non-instanced joints')
		children = pm.listRelatives(node, children=True, type=('transform', 'joint')) or []
		if any(child.isInstanced() for child in children):
			raise ValueError('Joint input contains instanced children')
	return nodes


def crossCorrectJointAxis(joints=None, aimAxis=(1, 0, 0), upAxis=(0, 1, 0), reverse=False):
	if joints is None or len(joints) != 3:
		return
	geometry.independent_directions(aimAxis, upAxis)
	points = [geometry.position3(pm.xform(pm.PyNode(node), q=True, ws=True, t=True))
		for node in joints]
	targets = _axis_targets(list(joints[:2]))
	directions = [geometry.plane_direction(points[0], points[1], points[2], reverse),
		geometry.plane_direction(points[1], points[2], points[0], reverse)]
	for node, direction in zip(targets, directions):
		correctJointAxis(node, aimAxis, upAxis, direction)


def crossCorrectJointAxisForFingur(joints=None, poleObjName='', aimAxis=(1, 0, 0),
		upAxis=(0, 1, 0), reverse=False):
	if joints is None or len(joints) < 2 or not poleObjName:
		return
	geometry.independent_directions(aimAxis, upAxis)
	nodes = _axis_targets(joints)
	pole = geometry.position3(pm.xform(pm.PyNode(poleObjName), q=True, ws=True, t=True))
	points = [geometry.position3(pm.xform(node, q=True, ws=True, t=True)) for node in nodes]
	# Keep the archived final-tip-to-first reference; only admission is changed.
	directions = [geometry.plane_direction(point, points[(index + 1) % len(points)], pole, reverse)
		for index, point in enumerate(points)]
	for node, direction in zip(nodes, directions):
		correctJointAxis(node, aimAxis, upAxis, direction)


#behavior orient mirror YZPlane
def reverseBehaviorOrient(rootJoint='', isChild=True):

	rootJointTrans = pm.ls(rootJoint)[0]

	rootOrient = rootJointTrans.getAttr( "jointOrient")
	#print 'rootOrient:'+str(rootOrient[0])+' '+str(rootOrient[1])+' '+str(rootOrient[2])

	rootJointTrans.setAttr( "jointOrientX", -rootOrient[0])
	rootJointTrans.setAttr( "jointOrientY", -rootOrient[1])
	rootJointTrans.setAttr( "jointOrientZ", rootOrient[2]+180)

	#print 'new rootOrient:'+str(rootJointTrans.getAttr( "jointOrient")[0])

	if isChild:

		childs = pm.listRelatives( rootJoint, ad=True, children=True, type=('joint') )

		for child in childs:
			translateX = child.getAttr( "translateX" )
			child.setAttr( "translateX", -translateX )

			childOrient = child.getAttr( "jointOrient" )

			#if childOrient[0]<0:
			#	child.setAttr( "jointOrientX",childOrient[0]+180 )
			child.setAttr( "jointOrientX", -childOrient[0])
			child.setAttr( "jointOrientY", -childOrient[1])


def reverseBehaviorOrientArm(rootJoint='', isChild=True):

	rootJointTrans = pm.ls(rootJoint)[0]

	rootOrient = rootJointTrans.getAttr( "jointOrient")

	rootJointTrans.setAttr( "jointOrientX", -(rootOrient[0]+180))
	rootJointTrans.setAttr( "jointOrientY", -rootOrient[1])
	rootJointTrans.setAttr( "jointOrientZ", rootOrient[2]-180)

	if isChild:

		childs = pm.listRelatives( rootJoint, ad=True, children=True, type=('joint') )

		for child in childs:
			translateX = child.getAttr( "translateX" )
			child.setAttr( "translateX", -translateX )
			translateZ = child.getAttr( "translateZ" )
			child.setAttr( "translateZ", -translateZ )

			childOrient = child.getAttr( "jointOrient" )

			child.setAttr( "jointOrientX", -childOrient[0])
			#child.setAttr( "jointOrientY", -childOrient[1])
			child.setAttr( "jointOrientZ", -childOrient[2])


def makeSplitJoint(srcJoint=None, targetJoint=None, numJoint=3, nameDic=None, withEndJoint=False):
	if (isinstance(numJoint, bool) or not isinstance(numJoint, numbers.Integral)
			or not 1 <= numJoint <= MAX_SPLIT_JOINTS):
		raise ValueError('Split count must be an integer within 1..10000')
	if not isinstance(withEndJoint, bool):
		raise TypeError('withEndJoint must be Boolean')
	if numJoint + int(withEndJoint) > MAX_SPLIT_JOINTS:
		raise ValueError('Total generated joint count exceeds the limit')
	nameDic = {} if nameDic is None else nameDic
	if not isinstance(nameDic, dict) or any(not isinstance(key, strings.STRING_TYPES) or not key
			or not isinstance(value, strings.STRING_TYPES) for key, value in nameDic.items()):
		raise ValueError('Joint name replacements must contain nonempty string keys/string values')
	source, target = _axis_targets([srcJoint, targetJoint])
	start = geometry.position3(pm.xform(source, q=True, ws=True, t=True))
	end = geometry.position3(pm.xform(target, q=True, ws=True, t=True))
	positions = [geometry.interpolate_position(start, end, float(index) / numJoint)
		for index in range(numJoint)]
	seeds = [source] * numJoint
	if withEndJoint:
		positions.append(end)
		seeds.append(target)
	names = [strings.replace_text(strings.leaf_name(node.name()), nameDic) for node in seeds]
	if any(not name or any(character in name for character in '|.*?[]')
			or any(character.isspace() or ord(character) < 32 for character in name) for name in names):
		raise ValueError('Replacement produced an invalid joint name')
	owned = ownership.SceneNodes()
	completed = False
	try:
		result = []
		for seed, name, position in zip(seeds, names, positions):
			created = pm.duplicate(seed, n=':' + name.lstrip(':'), parentOnly=True) or []
			for node in created:
				owned.adopt(node)
			if len(created) != 1:
				raise ValueError('Joint duplication did not return one root')
			node = owned.nodes[-1]
			pm.parent(node, world=True)
			pm.xform(node, ws=True, t=position)
			result.append(node)
		completed = True
		return result
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)


def parentJointFromlist(joints=None):
	if not joints:
		return
	nodes = _axis_targets(joints)
	planned = dict((child, parent) for parent, child in zip(nodes, nodes[1:]))
	parents = {}
	for node in nodes:
		seen = set()
		current = node
		while current is not None:
			if current in seen:
				raise ValueError('Joint parenting plan would create a cycle')
			seen.add(current)
			if current in planned:
				current = planned[current]
			else:
				if current not in parents:
					items = pm.listRelatives(current, parent=True) or []
					parents[current] = items[0] if items else None
				current = parents[current]
	completed = 0
	try:
		for parent, child in zip(nodes, nodes[1:]):
			pm.parent(child, parent)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Joint parenting stopped after {0} links; completed edits remain: {1}'.format(completed, error))




