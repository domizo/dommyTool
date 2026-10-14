# -*- coding: utf-8 -*-
"""Maya-independent naming conventions for archived asset nodes."""


OBJECT_TYPE_CODES = {
	'camera': '_cam',
	'joint': '_jnt',
	'mesh': '_geo',
	'nurbsCurve': '_geo',
	'nurbsSurface': '_geo',
	'instance': '_inst',
	'parentConstraint': '_cons',
	'pointConstraint': '_cons',
	'orientConstraint': '_cons',
	'scaleConstraint': '_cons',
	'aimConstraint': '_cons',
	'poleVectorConstraint': '_cons',
	'objectSet': '_set',
	'controller': '_cntlr',
	'group': '_grp',
	'locator': '_loc',
	'material': '_mtl',
	'shader': '_mtl',
	'light': '_light',
	'ncache': '_ncache',
	'abcCache': '_abccache',
	'fluidCache': '_fluidcache',
	'vdbCache': '_vdbcache',
}


def object_type_code(node_type=None):
	"""Return the historical name suffix for a Maya object type."""
	return OBJECT_TYPE_CODES.get(node_type, '')
