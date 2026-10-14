# -*- coding: utf-8 -*-
"""Finite world-space point-to-UV queries with owned API output storage."""

import maya.OpenMaya as om

from DPyTools.core.geometry import position3
from DPyTools.core.numeric import finite_number
from DPyTools.features.meshes import api


def getUVfromPoint(meshObj=None, pt_list=None):
	"""Return UVs for finite world points; reject unwritten/non-finite output."""
	positions = [position3(point) for point in (pt_list or [])]
	if not positions:
		return []
	mesh_fn = api.getMeshFn(meshObj)
	result = []
	for position in positions:
		owner = om.MScriptUtil()
		owner.createFromList([float('nan'), float('nan')], 2)
		pointer = owner.asFloat2Ptr()
		mesh_fn.getUVAtPoint(om.MPoint(*position), pointer, om.MSpace.kWorld)
		result.append([finite_number(
			om.MScriptUtil.getFloat2ArrayItem(pointer, 0, coordinate), 'Mesh UV')
			for coordinate in (0, 1)])
	return result
