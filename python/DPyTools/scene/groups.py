# -*- coding: utf-8 -*-
"""Owned empty-transform factories behind the historical group helpers."""

import sys

import pymel.core as pm

from DPyTools.core import geometry
from DPyTools.scene import ownership, parameters, transforms


def _name(name):
	if name == '':
		return {}
	name = parameters.control_name(name)
	if pm.objExists(name):
		raise ValueError('Requested group name already exists')
	return {'name': name}


def createEmpty(name='', parent=None, position=(0, 0, 0)):
	position = geometry.position3(position)
	kwargs = _name(name)
	if isinstance(parent, (list, tuple)):
		if len(parent) > 1:
			raise ValueError('Group creation requires at most one parent')
		parent = parent[0] if parent else None
	if parent is not None:
		parent = transforms.transform_node(parent)
	owned = ownership.SceneNodes()
	completed = False
	try:
		result = owned.create('transform', **kwargs)
		if kwargs and result.name().lstrip(':') != kwargs['name'].lstrip(':'):
			raise ValueError('Native group naming did not preserve the requested name')
		pm.xform(result, worldSpace=True, translation=position)
		if parent is not None:
			pm.parent(result, parent)
		completed = True
		return result
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)


def createEmptyWithAlign(name='', alignObj=None, isTrans=True, isRot=True, isScale=True, isUnParent=True):
	target = transforms.transform_node(alignObj)
	if any(not isinstance(flag, bool) for flag in (isTrans, isRot, isScale, isUnParent)):
		raise TypeError('Group alignment options must be Boolean')
	kwargs = _name(name)
	owned = ownership.SceneNodes()
	completed = False
	try:
		result = owned.create('transform', **kwargs)
		if kwargs and result.name().lstrip(':') != kwargs['name'].lstrip(':'):
			raise ValueError('Native group naming did not preserve the requested name')
		transforms.setTransformToObjectZero(result, target, isTrans, isRot, isScale, isUnParent)
		completed = True
		return result
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)
