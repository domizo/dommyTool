# -*- coding: utf-8 -*-
"""Checked construction of the historical curve and blend-shape node graphs."""

import sys

import pymel.core as pm

from DPyTools.core import strings
from DPyTools.scene import ownership
from DPyTools.scene.selection import resolve_shape


def _selection(shape_type, count):
	selected = pm.ls(selection=True) or []
	if len(selected) != count:
		raise ValueError('Select exactly {0} {1} input(s)'.format(count, shape_type))
	shapes = [resolve_shape(node, (shape_type,)) for node in selected]
	if len(set(shapes)) != len(shapes):
		raise ValueError('Input geometry identities must be distinct')
	return shapes


def _graph(node_type, inputs, outputs, parameters=()):
	"""Create only owned outputs; all inputs are borrowed and never deleted."""
	for kind, name, output_attribute, input_attribute in outputs:
		if name is not None and pm.objExists(name):
			raise ValueError('Generated output name already exists: {0}'.format(name))
	owned = ownership.SceneNodes()
	completed = False
	try:
		generator = owned.create(node_type)
		for name, value in parameters:
			pm.setAttr(generator.attr(name), value)
		for source, attribute in inputs:
			pm.connectAttr(source, generator.attr(attribute))
		result = []
		for kind, name, output_attribute, input_attribute in outputs:
			kwargs = {} if name is None else {'name': name}
			transform = owned.create('transform', **kwargs)
			if name is not None and strings.leaf_name(transform.name()).lstrip(':') != name.lstrip(':'):
				raise ValueError('Native output naming did not preserve the requested name')
			shape = owned.create(kind, parent=transform)
			pm.connectAttr(generator.attr(output_attribute), shape.attr(input_attribute))
			if kind == 'mesh':
				pm.sets('initialShadingGroup', e=True, forceElement=shape)
			result.append(transform)
		completed = True
		return generator, tuple(result)
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)


def createSpiralCurve():
	try:
		curve = _selection('nurbsCurve', 1)[0]
		return _graph('DSpiralCurve', ((curve.worldSpace[0], 'inputCurve'),),
			(('nurbsCurve', None, 'outputCurve', 'create'),))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Spiral curve was not created: {0}'.format(error))
		return False


def createPolyWire():
	try:
		curve = _selection('nurbsCurve', 1)[0]
		return _graph('DPolyWire', ((curve.worldSpace[0], 'inCurve'),),
			(('mesh', None, 'outMesh', 'inMesh'),))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Poly wire was not created: {0}'.format(error))
		return False


def _blend_inputs():
	base, target = _selection('mesh', 2)
	name = strings.leaf_name(target.getParent().name())
	strings.current_side(name)
	return ((base.worldMesh[0], 'inBaseMesh'), (target.worldMesh[0], 'inTargetMesh')), name


def createLRShape():
	try:
		inputs, name = _blend_inputs()
		outputs = tuple(('mesh', ':' + strings.set_side(name, side).lstrip(':'), attribute, 'inMesh')
			for side, attribute in (('_l', 'outLeftMesh'), ('_r', 'outRightMesh')))
		parameters = [('Distance', 2)]
		for index, value in ((0, 0.0), (1, 1.0)):
			prefix = 'BlendRamp[{0}].BlendRamp_'.format(index)
			parameters.extend(((prefix + 'FloatValue', value), (prefix + 'Position', value),
				(prefix + 'Interp', 1)))
		return _graph('DBSMirrorMesh', inputs, outputs, parameters)
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Left/right shapes were not created: {0}'.format(error))
		return False


def createBSFlipShape():
	try:
		inputs, name = _blend_inputs()
		side = strings.reverse_side(strings.current_side(name))
		name = strings.set_side(name, side) if side else name + '_flip'
		return _graph('DBSFlipMesh', inputs,
			(('mesh', ':' + name.lstrip(':'), 'outMesh', 'inMesh'),), (('Tolerance', 0.1),))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Flipped shape was not created: {0}'.format(error))
		return False


def createUDShape():
	try:
		inputs, name = _blend_inputs()
		outputs = tuple(('mesh', ':' + (name + suffix).lstrip(':'), attribute, 'inMesh')
			for suffix, attribute in (('_u', 'outUpMesh'), ('_d', 'outDownMesh')))
		return _graph('DBSUpDownMesh', inputs, outputs, (('Invert', 0),))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Up/down shapes were not created: {0}'.format(error))
		return False
