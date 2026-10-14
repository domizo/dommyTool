# -*- coding: utf-8 -*-
"""Checked DAG/mesh access for the legacy OpenMaya API."""

import maya.OpenMaya as om
import pymel.core as pm

from DPyTools.scene.selection import resolve_shape


def getDagPathfromName(m_object=''):
	"""Resolve a retained node to a full DAG path, not a dependency object."""
	node = pm.PyNode(m_object)
	selection = om.MSelectionList()
	selection.add(node.longName())
	path = om.MDagPath()
	selection.getDagPath(0, path)
	return path


def getMeshFn(m_object=''):
	"""Accept a mesh transform/shape and reject ambiguous instance geometry."""
	return om.MFnMesh(getDagPathfromName(resolve_shape(m_object)))
