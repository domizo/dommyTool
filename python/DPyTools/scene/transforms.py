import pymel.core as pm
import pymel.util as pmu
import sys

from DPyTools.scene import parameters
from DPyTools.core import geometry


'''
to run:
	>>> import DPyTools.scene.transforms as DPTr


'''

def isRootTrans(object=None):
	"""Return whether *object* is a transform directly under world."""
	if object is None:
		return False
	return not bool(pm.listRelatives(object, parent=True))

def hasShape(object=None):
	"""Return whether *object* owns at least one shape node."""
	if object is None:
		return False
	return bool(pm.listRelatives(object, shapes=True))



def getRootTransformStr(node=''):
	#This method use pyNode
	#return pyNode


	if node:
		pNode = pm.general.PyNode(node)
		#return pNode.longName().split("|")[1]
		return pNode.root()

	selection = pm.ls(sl=True) or []
	if not selection:
		return None
	return selection[0].root()







def transform_node(value):
	"""Resolve an editable non-instanced transform/joint identity."""
	node = parameters.editable_node(value)
	if node.nodeType() not in ('transform', 'joint'):
		raise ValueError('Operation requires a transform or joint')
	return node


def _restore_parent(source, parent, active_error):
	try:
		if parent is None:
			pm.parent(source, world=True)
		else:
			pm.parent(source, parent)
	except (RuntimeError, TypeError, ValueError) as error:
		if not active_error:
			raise
		try:
			pm.warning('Transform parent restoration also failed: {0}'.format(error))
		except (RuntimeError, TypeError, ValueError):
			pass


def setTransformToObjectZero(srcObj=None, targetObj=None, isTrans=True, isRot=True, isScale=True, isUnParent=True):
	"""Align through a temporary parent; restore hierarchy after any failure.

	Success retains historical semantics: unparent to world, or keep target as
	parent when requested. Completed channel edits are not rolled back.
	"""
	if any(not isinstance(flag, bool) for flag in (isTrans, isRot, isScale, isUnParent)):
		raise TypeError('Transform alignment options must be Boolean')
	source, target = transform_node(srcObj), transform_node(targetObj)
	if source == target or target in (pm.listRelatives(source, allDescendents=True) or []):
		raise ValueError('Transform alignment would create a hierarchy cycle')
	parents = pm.listRelatives(source, parent=True) or []
	if len(parents) > 1:
		raise ValueError('Transform alignment requires one unambiguous parent')
	parent = parents[0] if parents else None
	values = {}
	for enabled, name, value in ((isTrans, 'translate', (0, 0, 0)),
			(isRot, 'rotate', (0, 0, 0)), (isScale, 'scale', (1, 1, 1))):
		if enabled:
			values[name] = value
	plan = parameters.value_plan(source, values)
	completed = False
	try:
		pm.parent(source, target)
		parameters.write_values(plan)
		if isUnParent:
			pm.parent(source, world=True)
		completed = True
		return source
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			_restore_parent(source, parent, active_error=exception_type is not None)


def setRotateAimAxis(object=None, srcVec=(1, 0, 0), aimVec=(0, 0, 1)):
	"""Set the historical Y-only foot aim, with a signed planar angle."""
	angle = geometry.signed_y_angle_degrees(srcVec, aimVec)
	node = transform_node(object)
	parameters.set_values(node, {'rotate': (0, angle, 0)})
	return node


def _world_position(value):
	node = pm.PyNode(value)
	if not isinstance(node, pm.nt.DagNode) or node.isInstanced():
		raise ValueError('World-position queries require a non-instanced DAG node')
	return geometry.position3(pm.xform(node, query=True, worldSpace=True, translation=True))


def getPositionBetweenObjects(source=None, target=None, param=0.5):
	"""Return a VectorN at a normalized [0, 1] world-position fraction."""
	return pmu.VectorN(geometry.interpolate_position(_world_position(source), _world_position(target), param))


def getDistanceBetweenObjects(source=None, target=None):
	return geometry.distance3(_world_position(source), _world_position(target))
