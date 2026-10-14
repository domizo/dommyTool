# -*- coding: utf-8 -*-
"""BBox measurement with explicit generated-hierarchy ownership."""

import sys

import pymel.core as pm

from DPyTools.core.numeric import finite_number
from DPyTools.scene import ownership


bboxMeasureName = 'bboxMeasure_grp'
TOOL_NAME = 'DPyTools.BBoxMeasure.v1'


def getBBoxInfo(target=None):
	"""Return the historical object-space bounds, sizes and largest axis."""
	if target is None:
		selection = pm.ls(sl=True) or []
		if not selection:
			pm.warning('Please select an object.')
			return []
		target = selection[0]
	bounds = pm.xform(target, q=True, bb=True)
	if len(bounds) != 6:
		raise ValueError('Bounding box must contain six coordinates')
	bounds = [finite_number(value, 'Bounding-box coordinate') for value in bounds]
	if any(bounds[index] > bounds[index + 3] for index in range(3)):
		raise ValueError('Bounding-box minimum exceeds maximum')
	sizes = [finite_number(bounds[index + 3] - bounds[index], 'Bounding-box size')
		for index in range(3)]
	axis = max(range(3), key=lambda index: sizes[index])
	return bounds + sizes + ['XYZ'[axis], sizes[axis]]


def printBBox():
	data = getBBoxInfo()
	if data:
		for index, axis in enumerate('XYZ'):
			print('min{0}:{1} max{0}:{2}'.format(axis, data[index], data[index + 3]))
			print('size{0}:{1}'.format(axis, data[index + 6]))
		print('max axis:{0}'.format(data[9]))
		print('max size:{0}'.format(data[10]))


def _existing_measure(protected=None):
	name = ':' + bboxMeasureName
	if not pm.objExists(name):
		return None
	return ownership.require_owned_hierarchy(pm.PyNode(name), TOOL_NAME, protected)


def deleteBBoxMeasure():
	"""Delete only a fully marked measurement hierarchy, never a name match."""
	try:
		root = _existing_measure()
		if root is None:
			return False
		pm.delete(root)
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('BBox measurement was not deleted: {0}'.format(error))
		return False
	return True


def createBBoxMeasure():
	"""Build a known-node candidate before replacing a checked old result."""
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		selection = pm.ls(sl=True) or []
		if len(selection) != 1:
			raise ValueError('Please select one transform for measurement')
		target = pm.PyNode(selection[0])
		if target.nodeType() != 'transform' or target.isInstanced():
			raise ValueError('Measurement requires a non-instanced transform')
		getBBoxInfo(target)
		old_root = _existing_measure(protected=target)
		root = owned.create('transform', name=':bboxMeasure_pending#')
		corners = (
			(('MinX', 'MinY', 'MaxZ'), ('MaxX', 'MinY', 'MaxZ')),
			(('MaxX', 'MinY', 'MaxZ'), ('MaxX', 'MaxY', 'MaxZ')),
			(('MaxX', 'MinY', 'MinZ'), ('MaxX', 'MinY', 'MaxZ')),
		)
		for axis, pair in zip('XYZ', corners):
			locators = []
			for label, coordinates in zip(('min', 'max'), pair):
				transform = owned.create('transform', name=label + '_' + axis.lower() + '_loc', parent=root)
				shape = owned.create('locator', parent=transform)
				for component, coordinate in zip('XYZ', coordinates):
					pm.connectAttr(target.attr('boundingBox' + coordinate), transform.attr('translate' + component))
				locators.append(shape)
			dimension_parent = owned.create('transform', name='dim' + axis, parent=root)
			dimension = owned.create('distanceDimShape', parent=dimension_parent)
			pm.connectAttr(locators[0].worldPosition[0], dimension.startPoint)
			pm.connectAttr(locators[1].worldPosition[0], dimension.endPoint)
		ownership.mark_hierarchy(root, owned.nodes, TOOL_NAME)
		if old_root is not None:
			ownership.require_owned_hierarchy(old_root, TOOL_NAME, protected=target)
			pm.delete(old_root)
		if pm.objExists(':' + bboxMeasureName):
			raise ValueError('Measurement name became occupied during creation')
		pm.rename(root, ':' + bboxMeasureName)
		if root.name().lstrip(':') != bboxMeasureName:
			raise ValueError('Native measurement naming did not preserve the requested name')
		completed = True
		return root
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('BBox measurement was not completed: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)


def openMatchScaleTool():
	"""Retained archive entry point; the scale-fitting prototype was incomplete."""
	pm.warning('Match Scale Tool is an archived, unimplemented idea; no dialog was opened.')
	return False


class matchScaleTool(object):
	"""Compatibility name for the disabled prototype, not a functional Qt dialog."""
	def __init__(self, title='title', parent=None):
		openMatchScaleTool()

	@classmethod
	def show_dialog(cls):
		return openMatchScaleTool()
