# -*- coding: utf-8 -*-
"""Follicle creation with one local-geometry/world-matrix contract."""

import sys

import pymel.core as pm

from DPyTools.core.numeric import finite_number
from DPyTools.core.strings import leaf_name
from DPyTools.scene.selection import resolve_shape
from DPyTools.scene import groups, ownership, parameters


def create_follicle(targetObj, uv=None, owner=None, name=None, parent=None):
	"""Create a world-driven follicle; an optional owner retains explicit DAG nodes.

	The historical no-owner creation/naming path is preserved. A supplied owner
	is responsible for cleanup of its complete graph after any factory failure.
	"""
	uv = (0.0, 0.0) if uv is None else uv
	if len(uv) != 2:
		raise ValueError('uv must contain exactly U and V')
	coordinates = tuple(finite_number(value, 'Follicle UV') for value in uv)
	try:
		shape = resolve_shape(targetObj, ('nurbsSurface', 'mesh'))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning(str(error))
		return False
	explicit_parent = None
	if owner is None:
		if name is not None or parent is not None:
			raise ValueError('Explicit follicle naming/parenting requires a graph owner')
		legacy_name = '_'.join((leaf_name(shape.name()), 'follicle', '0#'))
		follicle = pm.createNode('follicle', name=legacy_name)
	else:
		if not isinstance(owner, ownership.SceneNodes):
			raise TypeError('Follicle graph owner must be a SceneNodes instance')
		name = parameters.control_name(name)
		shape_name = parameters.control_name(name + 'Shape')
		if pm.objExists(name) or pm.objExists(shape_name):
			raise ValueError('Requested follicle transform/shape name already exists')
		explicit_parent = owner.adopt(groups.createEmpty(name, parent))
		follicle = owner.create('follicle', name=shape_name, parent=explicit_parent)
		if follicle.name().lstrip(':') != shape_name.lstrip(':'):
			raise ValueError('Native follicle naming did not preserve the requested name')
	parent = None
	completed = False
	try:
		parent = follicle.getParent()
		if owner is not None and parent != explicit_parent:
			raise ValueError('Native follicle did not retain its explicit owning transform')
		if shape.nodeType() == 'mesh':
			shape.outMesh.connect(follicle.inputMesh)
		else:
			shape.local.connect(follicle.inputSurface)
		shape.worldMatrix[0].connect(follicle.inputWorldMatrix)
		# Outputs are world-space; grouping must not transform them again.
		parent.inheritsTransform.set(False)
		follicle.outRotate.connect(parent.rotate)
		follicle.outTranslate.connect(parent.translate)
		follicle.parameterU.set(coordinates[0])
		follicle.parameterV.set(coordinates[1])
		completed = True
		return follicle
	finally:
		if not completed and owner is None:
			exception_type = sys.exc_info()[0]
			active_error = exception_type is not None
			try:
				pm.delete(parent if parent is not None else follicle)
			except (RuntimeError, TypeError, ValueError) as error:
				if not active_error:
					raise
				try:
					pm.warning('Incomplete follicle cleanup failed: {0}'.format(error))
				except (RuntimeError, TypeError, ValueError):
					pass
