# -*- coding: utf-8 -*-
"""Live custom-data conversion through Maya's variable geometry groups."""

import sys

import pymel.core as pm

from DPyTools.scene import data_display, ownership


def _source(value, plug, expected_type):
	if value is None:
		selected = pm.ls(selection=True) or []
		if len(selected) != 1:
			raise ValueError('Select exactly one custom-data output or display')
		value = selected[0]
	node = pm.PyNode(value)
	if isinstance(node, pm.general.Component):
		raise ValueError('Conversion input must be a node or an output plug, not a component')
	if isinstance(node, pm.general.Attribute):
		port = node
	else:
		if node.nodeType() in ('transform', 'joint'):
			shapes = node.getShapes(noIntermediate=True) or []
			if len(shapes) != 1:
				raise ValueError('Conversion input must own exactly one non-intermediate shape')
			node = shapes[0]
		if isinstance(node, pm.nt.DagNode) and node.isInstanced():
			raise ValueError('Conversion requires a non-instanced input')
		if node.hasAttr(plug):
			port = data_display.source_port(node, plug)
		else:
			input_name = dict(data_display.DISPLAY_PORTS).get(node.nodeType())
			if input_name is None:
				raise ValueError('Input node does not expose the requested custom-data output')
			inputs = node.attr(input_name).inputs(plugs=True) or []
			if len(inputs) != 1:
				raise ValueError('Display must have exactly one connected data source')
			port = inputs[0]
	owner = port.node()
	if isinstance(owner, pm.nt.DagNode) and owner.isInstanced():
		raise ValueError('Conversion requires a non-instanced source identity')
	if port.isArray() or not port.isReadable() or not port.isConnectable():
		raise ValueError('Conversion requires a readable explicit custom-data plug')
	if data_display.declared_type(port) != expected_type:
		raise ValueError('Input custom-data schema does not match the converter')
	return port


def _convert(source, plug, converter_type, group_type, input_name, output_name, schema):
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		port = _source(source, plug, schema)
		converter = owned.create(converter_type)
		input_port = converter.attr(input_name)
		if data_display.declared_type(input_port) != schema:
			raise ValueError('Loaded converter input schema does not match the archive')
		group = owned.create(group_type)
		if data_display.declared_type(converter.attr(output_name)) != data_display.declared_type(group.attr('create')):
			raise ValueError('Converter output and variable group schemas do not match')
		pm.connectAttr(port, input_port)
		pm.connectAttr(converter.attr(output_name), group.attr('create'))
		# The native group manages output pairs; no placeholder local[0] graph.
		pm.getAttr(group.attr('local'), silent=True)
		completed = True
		return converter, group
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Custom-data conversion was not created: {0}. Unconfirmed native side effects may remain.'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)


def createDCConvertToCurve(source=None, plug='outDC'):
	return _convert(source, plug, 'DCToCurves', 'curveVarGroup', 'inDC', 'outCurves', 'DCurveData')


def createDMToMesh(source=None, plug='outDM'):
	return _convert(source, plug, 'DMToMesh', 'meshVarGroup', 'inDM', 'outMesh', 'DMeshData')
