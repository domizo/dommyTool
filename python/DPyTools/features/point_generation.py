# -*- coding: utf-8 -*-
"""Admitted world-space point snapshots and live transform-driven points."""

import sys

import pymel.core as pm

from DPyTools.core import geometry
from DPyTools.scene import attribute_values, ownership


MAX_INPUT_POINTS = 10000


def _input_plan(connect):
	selected = pm.ls(selection=True, flatten=True) or []
	if not selected or len(selected) > MAX_INPUT_POINTS:
		raise ValueError('Select 1..10000 point inputs')
	items = [pm.PyNode(value) for value in selected]
	if len(set(items)) != len(items):
		raise ValueError('Point inputs contain duplicate identities')
	plan = []
	for item in items:
		component = isinstance(item, pm.general.Component)
		node = item.node() if component else item
		if not isinstance(node, pm.nt.DagNode) or node.isInstanced():
			raise ValueError('Point input requires a non-instanced DAG node/component')
		position = attribute_values.numeric_value(
			geometry.position3(pm.xform(item, q=True, ws=True, t=True)), 'float3')
		matrix = None
		if connect:
			if component or not isinstance(node, pm.nt.Transform):
				raise ValueError('Live point inputs require transforms/joints, not components/shapes')
			matrix = node.attr('worldMatrix')[0]
			if pm.getAttr(matrix, type=True) != 'matrix':
				raise ValueError('Live input does not provide a world matrix')
		plan.append((position, matrix))
	return plan


def create_from_selection(connect=False):
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		if not isinstance(connect, bool):
			raise TypeError('Live point option must be Boolean')
		plan = _input_plan(connect)
		generator = owned.create('DPAddPoint')
		for index, (position, matrix) in enumerate(plan):
			target = generator.attr('Point')[index]
			if pm.getAttr(target, type=True) != 'float3':
				raise ValueError('DPAddPoint.Point must match the C++ float3 contract')
			if matrix is None:
				pm.setAttr(target, *position, type='float3')
			else:
				decompose = owned.create('decomposeMatrix')
				pm.connectAttr(matrix, decompose.attr('inputMatrix'))
				pm.connectAttr(decompose.attr('outputTranslate'), target)
		completed = True
		return generator
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('DPoint was not created: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)
