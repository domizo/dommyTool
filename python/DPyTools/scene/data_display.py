# -*- coding: utf-8 -*-
"""Target-local custom-data display connections; never disconnect a fanout."""

import sys

import pymel.core as pm

from DPyTools.scene import ownership, parameters
from DPyTools.core import strings


DISPLAY_PORTS = (
	('DPointDisplay', 'inDP'), ('DCurveDisplay', 'inDC'),
	('DCMarker', 'inDC'), ('DMeshDisplay', 'inDM'), ('DMMarker', 'inDM'),
)


def _source(node, plug):
	parameters.attribute_names([plug])
	node = pm.PyNode(node)
	if not node.hasAttr(plug) or not pm.attributeQuery(plug, node=node, readable=True, connectable=True):
		raise ValueError('Display source must be a readable, connectable attribute')
	return node.attr(plug)


def check_source(node='', plug='outDP'):
	"""Compatibility check only: no existing outgoing connection is changed."""
	try:
		return _source(node, plug)
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Display source was not admitted: {0}'.format(error))
		return False


def _declared_type(port):
	# getAttr(type=True) can return None for an uninitialized typed payload.
	types = pm.addAttr(port, q=True, dataType=True)
	if isinstance(types, strings.STRING_TYPES):
		types = [types]
	if (not isinstance(types, (list, tuple)) or len(types) != 1
			or not isinstance(types[0], strings.STRING_TYPES) or not types[0]):
		raise ValueError('Display requires one declared typed-data schema')
	return types[0]


def declared_type(port):
	"""Shared declaration query; an empty payload does not erase its schema."""
	return _declared_type(port)


def source_port(node, plug):
	"""Public readable/connectable source admission for custom-data consumers."""
	return _source(node, plug)


def _connect(source, display):
	port_name = dict(DISPLAY_PORTS).get(display.nodeType())
	if port_name is None or display.isReferenced():
		raise ValueError('Target must be a supported, non-referenced display shape')
	if display.isInstanced():
		raise ValueError('Display switching refuses instanced shapes')
	if not pm.attributeQuery(port_name, node=display, writable=True, connectable=True):
		raise ValueError('Display input is not writable/connectable')
	target = display.attr(port_name)
	if source.node() == display:
		raise ValueError('A display must not drive itself')
	if _declared_type(source) != _declared_type(target):
		raise ValueError('Source and display input data types do not match')
	if target.inputs(plugs=True) == [source]:
		return display
	if pm.getAttr(target, lock=True):
		raise ValueError('Display input is locked')
	# Selecting this target authorizes changing its input, not the source fanout.
	pm.connectAttr(source, target, force=True)
	return display


def switch_display(target_node='', target_display='', plug='outDP'):
	try:
		return _connect(_source(target_node, plug), pm.PyNode(target_display))
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Display was not switched: {0}. Native target edits may remain.'.format(error))
		return False


def create_display(node_type, target_node='', plug='outDP'):
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		if node_type not in dict(DISPLAY_PORTS):
			raise ValueError('Unsupported display node type')
		source = _source(target_node, plug)
		parent = owned.create('transform')
		display = owned.create(node_type, parent=parent)
		_connect(source, display)
		completed = True
		return display
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Display was not created: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)


def dp_dsiplay_connect_check(dpdisp='', plug='outDP'):
	return check_source(dpdisp, plug)


def createDPointDisplay(targetNode='', plug='outDP'):
	return create_display('DPointDisplay', targetNode, plug)


def switch_dpoint_display(target_node='', target_display='', plug='outDP'):
	return switch_display(target_node, target_display, plug)


def dc_dsiplay_connect_check(dcdisp='', plug='outDC'):
	return check_source(dcdisp, plug)


def createDCurveDisplay(targetNode='', plug='outDC'):
	return create_display('DCurveDisplay', targetNode, plug)


def switch_dcurve_display(target_node='', target_display='', plug='outDC'):
	return switch_display(target_node, target_display, plug)


def createDCMarker(targetNode='', plug='outDC'):
	return create_display('DCMarker', targetNode, plug)


def dm_dsiplay_connect_check(dmdisp='', plug='outDM'):
	return check_source(dmdisp, plug)


def createDMeshDisplay(targetNode='', plug='outDM'):
	return create_display('DMeshDisplay', targetNode, plug)


def switch_dmesh_display(target_node='', target_display='', plug='outDM'):
	return switch_display(target_node, target_display, plug)


def createDMMarker(targetNode='', plug='outDM'):
	return create_display('DMMarker', targetNode, plug)
