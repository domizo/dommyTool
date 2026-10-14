# -*- coding: utf-8 -*-
"""Checked animation adapters; native graph edits are not blanket rollback."""

import numbers
import sys

import pymel.core as pm
import maya.api.OpenMaya as om
import maya.api.OpenMayaAnim as oma

from DPyTools.core import strings
from DPyTools.core.numeric import finite_number
from DPyTools.features.animation import key_plan
from DPyTools.scene import attribute_values, parameters, selection


def _attribute(value):
	port = pm.PyNode(value)
	if not isinstance(port, pm.general.Attribute) or port.isArray():
		raise ValueError('Animation requires an explicit attribute or array element')
	return port


def _scalar_port(value):
	port = _attribute(value)
	kind = pm.getAttr(port, type=True)
	if kind not in attribute_values.SCALAR_TYPES or port.isCompound():
		raise ValueError('Driven keys require scalar numeric attributes')
	if not pm.attributeQuery(port.lastPlugAttr(longName=True), node=port.node(),
			readable=True, connectable=True):
		raise ValueError('Driven-key attributes must be readable and connectable')
	return port, kind


def _key_editable(port):
	parameters.editable_node(port.node())
	related = [port] + (port.getAllParents(arrays=True) or [])
	if port.isCompound():
		related.extend(port.getChildren() or [])
	if len(related) > parameters.MAX_CONNECTIONS:
		raise ValueError('Animation attribute hierarchy exceeds the limit')
	if (not pm.attributeQuery(port.lastPlugAttr(longName=True), node=port.node(), writable=True)
			or any(pm.getAttr(item, lock=True) for item in related)):
		raise ValueError('Animation destination or its parent/child is not editable')
	# Existing animation inputs are expected; do not disconnect them or force writes.
	return port


def createDrivenKeys(driver='', target='', times=None, values=None):
	"""Set explicit key coordinates, never temporary driver/target values."""
	pairs = key_plan.key_pairs(times, values)
	driver, driver_kind = _scalar_port(driver)
	target, target_kind = _scalar_port(target)
	if driver == target:
		raise ValueError('A driven attribute cannot drive itself')
	_key_editable(target)
	pairs = tuple((parameters.admit_value(driver, driver_kind, time),
		parameters.admit_value(target, target_kind, value)) for time, value in pairs)
	selected = pm.ls(selection=True) or []
	completed = 0
	try:
		for time, value in pairs:
			result = pm.setDrivenKeyframe(target, currentDriver=driver,
				driverValue=time, value=value, inTangentType='linear', outTangentType='linear',
				insertBlend=False, hierarchy='none', shape=False)
			if isinstance(result, bool) or not isinstance(result, numbers.Integral) or result != 1:
				raise RuntimeError('Native driven-key command did not confirm one key')
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError(
			'Driven-key creation stopped after {0} confirmed keys; native edits may remain: {1}'
			.format(completed, error))
	finally:
		selection.restore_selection(selected)
	return completed


def _key_count(port):
	count = pm.keyframe(port, query=True, keyframeCount=True, animation='objects')
	if count is None:
		return 0
	if isinstance(count, bool) or not isinstance(count, numbers.Integral) or count < 0:
		raise ValueError('Native key count must be a nonnegative integer')
	return count


def _animatable_ports(nodes):
	ports, seen = [], set()
	for node in nodes:
		owners = {node}
		if isinstance(node, pm.nt.DagNode) and node.nodeType() in ('transform', 'joint'):
			shapes = pm.listRelatives(node, shapes=True) or []
			if len(shapes) > parameters.MAX_CONNECTIONS:
				raise ValueError('Animation shape inspection exceeds the limit')
			for value in shapes:
				shape = pm.PyNode(value)
				if (not isinstance(shape, pm.nt.DagNode) or shape.getParent() != node
						or shape.nodeType() in ('transform', 'joint')):
					raise ValueError('Native shape query returned an unrelated identity')
				owners.add(shape)
		attributes = pm.listAnimatable(node) or []
		if len(attributes) + len(ports) > parameters.MAX_CONNECTIONS:
			raise ValueError('Animation attribute inspection exceeds the limit')
		for value in attributes:
			port = _attribute(value)
			if port.node() not in owners:
				raise ValueError('Native animatable attribute belongs to an unrelated node')
			if port not in seen:
				seen.add(port)
				ports.append(port)
	return tuple(ports)


def _animation_curves(ports, editable=False):
	curves, seen = [], set()
	for port in ports:
		if not _key_count(port):
			continue
		if editable:
			_key_editable(port)
		values = pm.keyframe(port, query=True, name=True, animation='objects') or []
		if not values or len(values) + len(curves) > parameters.MAX_CONNECTIONS:
			raise ValueError('Native animation curve query is empty or exceeds the limit')
		for value in values:
			curve = pm.PyNode(value)
			if not curve.nodeType().startswith('animCurve'):
				raise ValueError('Native animation query returned a non-curve node')
			if editable:
				parameters.editable_node(curve)
			if curve not in seen:
				seen.add(curve)
				curves.append(curve)
	return tuple(curves)


def isAnimationAttribute(attr=''):
	"""Return a Boolean without depending on active Graph Editor keys."""
	if isinstance(attr, strings.STRING_TYPES) and not attr:
		return False
	return bool(_key_count(_attribute(attr)))


def isAnimationExist(objs=None):
	nodes = parameters.resolve_nodes([] if objs is None else objs,
		allow_empty=True, require_same_type=False)
	return any(_key_count(port) for port in _animatable_ports(nodes))


def _delete_curves(curves):
	completed = 0
	try:
		for curve in curves:
			result = pm.cutKey(curve, clear=True, animation='objects', option='keys',
				hierarchy='none', shape=False)
			if isinstance(result, bool) or not isinstance(result, numbers.Integral) or result != 1:
				raise RuntimeError('Native key deletion did not confirm one curve')
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError(
			'Animation deletion stopped after {0} confirmed curves; native edits may remain: {1}'
			.format(completed, error))
	return completed


def deleteAnimationAttribute(node='', attr=''):
	resolved = parameters.resolve_nodes([node], require_same_type=False)[0]
	if isinstance(attr, strings.STRING_TYPES) and '.' not in attr:
		port = resolved.attr(parameters.attribute_path(attr))
	else:
		port = _attribute(attr)
	if port.node() != resolved:
		raise ValueError('Animation deletion attribute does not belong to the requested node')
	return _delete_curves(_animation_curves((_attribute(port),), editable=True))


def deleteAnimationKeys(objs=None):
	nodes = parameters.resolve_nodes([] if objs is None else objs,
		allow_empty=True, require_same_type=False)
	for node in nodes:
		parameters.editable_node(node)
	return _delete_curves(_animation_curves(_animatable_ports(nodes), editable=True))


def echoAnimationData(node=None, hierarchy=None):
	"""Print and return a fully validated snapshot, respecting explicit input."""
	hierarchy = False if hierarchy is None else hierarchy
	if not isinstance(hierarchy, bool):
		raise TypeError('Animation inspection hierarchy option must be Boolean')
	nodes = parameters.resolve_nodes((pm.ls(selection=True) or []) if node is None else [node],
		allow_empty=True, require_same_type=False)
	if hierarchy:
		expanded = list(nodes)
		for root in nodes:
			for child in pm.listRelatives(root, allDescendents=True) or []:
				if child not in expanded:
					expanded.append(child)
				if len(expanded) > parameters.MAX_CONNECTIONS:
					raise ValueError('Animation hierarchy inspection exceeds the limit')
		nodes = parameters.resolve_nodes(expanded, allow_empty=True, require_same_type=False)
	records, total = [], 0
	for curve in _animation_curves(_animatable_ports(nodes)):
		count = _key_count(curve)
		total += count
		if total > key_plan.MAX_KEY_PAIRS:
			raise ValueError('Animation report exceeds the key limit')
		if not count:
			continue
		options = {'query': True, 'animation': 'objects', 'index': (0, count - 1)}
		input_options = dict(options)
		input_options['floatChange' if curve.nodeType().startswith('animCurveU') else 'timeChange'] = True
		times = pm.keyframe(curve, **input_options) or []
		values = pm.keyframe(curve, valueChange=True, **options) or []
		if len(times) != count or len(values) != count:
			raise ValueError('Native animation query arrays do not match the key count')
		pairs = tuple((finite_number(time, 'Animation input'),
			finite_number(value, 'Animation value')) for time, value in zip(times, values))
		records.append((curve.name(), pairs))
	for name, pairs in records:
		print('Found {0} keys on {1}'.format(len(pairs), name))
		for index, pair in enumerate(pairs):
			print(index, pair[0], pair[1])
	return tuple(records)


def createAnimCurveUU(name='animCurve', times=(), values=()):
	"""Create one named, unattached unitless curve; delete its known API object on failure."""
	pairs = key_plan.key_pairs(times, values)
	name = parameters.control_name(name)
	if pm.objExists(name):
		raise ValueError('Requested animation curve name already exists')
	curve, created, completed = oma.MFnAnimCurve(), None, False
	try:
		created = curve.create(oma.MFnAnimCurve.kAnimCurveUU)
		if created is None or created.isNull():
			created = None
			raise RuntimeError('Native animation creation did not return an object')
		curve.setName(name)
		actual_name = curve.name()
		if actual_name.lstrip(':') != name.lstrip(':'):
			raise ValueError('Native animation naming did not preserve the requested name')
		for time, value in pairs:
			curve.addKey(time, value,
				oma.MFnAnimCurve.kTangentLinear, oma.MFnAnimCurve.kTangentLinear)
		actual_name = curve.name()
		if actual_name.lstrip(':') != name.lstrip(':'):
			raise ValueError('Native animation name changed during key creation')
		completed = True
		return actual_name
	finally:
		if not completed and created is not None:
			exception_type = sys.exc_info()[0]
			active_error = exception_type is not None
			try:
				modifier = om.MDGModifier()
				modifier.deleteNode(created)
				modifier.doIt()
			except (RuntimeError, TypeError, ValueError) as error:
				if not active_error:
					raise
				try:
					pm.warning('Animation curve cleanup also failed: {0}'.format(error))
				except (RuntimeError, TypeError, ValueError):
					pass
