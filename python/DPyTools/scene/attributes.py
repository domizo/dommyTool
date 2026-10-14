# -*- coding: utf-8 -*-
u"""Maya attribute operations and compatibility-safe numeric default handling."""

import pymel.core as pm

from DPyTools.core import strings as string_utils
from DPyTools.scene import attribute_values, parameters

'''
to run:
	>>> import DPyTools.scene.attributes as DPAttr


'''


def hideAttributes(obj=None, attrList=None):
	if attrList is None:
		attrList = []
	for a in attrList:
		obj.setAttr(a, k=False)


def createAttributeFromDic(node=None, attributes=None):
	"""Preflight scalar numeric/string definitions; report native partial edits."""
	if attributes is None:
		attributes = []
	if not isinstance(attributes, (list, tuple)) or len(attributes) > parameters.MAX_CONNECTIONS:
		raise ValueError('Attribute definitions must be a bounded sequence')
	node = parameters.editable_node(node)
	plan, names = [], set()
	for specification in attributes:
		kwargs = _attribute_spec(specification)
		for name in (kwargs['longName'], kwargs.get('shortName', kwargs['longName'])):
			if name in names or node.hasAttr(name):
				raise ValueError('Attribute definition collides with an existing or planned name')
			# The default short and long name represent one attribute.
			if name != kwargs['longName']:
				names.add(name)
		names.add(kwargs['longName'])
		plan.append(kwargs)
	completed = 0
	try:
		for kwargs in plan:
			pm.addAttr(node, **kwargs)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Attribute creation stopped after {0} additions; native edits may remain: {1}'.format(completed, error))
	return completed


def _attribute_spec(specification):
	aliases = {'ln': 'longName', 'sn': 'shortName', 'at': 'attributeType',
		'dt': 'dataType', 'dv': 'defaultValue', 'min': 'minValue', 'max': 'maxValue',
		'hnv': 'hasMinValue', 'hxv': 'hasMaxValue', 'h': 'hidden', 'k': 'keyable', 'en': 'enumName'}
	allowed = set(aliases.values())
	if not isinstance(specification, dict):
		raise TypeError('Each attribute definition must be a dictionary')
	result = {}
	for key, value in specification.items():
		key = aliases.get(key, key)
		if key not in allowed or key in result:
			raise ValueError('Unsupported or duplicated attribute definition flag')
		result[key] = value
	for key in ('longName', 'shortName'):
		if key in result:
			name = parameters.attribute_path(result[key])
			if '.' in name or '[' in name:
				raise ValueError('New attributes require simple names')
	if 'longName' not in result:
		raise ValueError('Attribute definition requires a long name')
	for key in ('hasMinValue', 'hasMaxValue', 'hidden', 'keyable'):
		if key in result and not isinstance(result[key], bool):
			raise TypeError('Attribute definition flags must be Boolean')
	kind = result.get('attributeType')
	if 'dataType' in result:
		if kind is not None or result['dataType'] != 'string' or any(key in result for key in ('defaultValue', 'minValue', 'maxValue', 'hasMinValue', 'hasMaxValue', 'enumName')):
			raise ValueError('Only a plain string data definition is supported')
		return result
	if kind not in attribute_values.SCALAR_TYPES:
		raise ValueError('Only scalar numeric or string attribute definitions are supported')
	for key in ('defaultValue', 'minValue', 'maxValue'):
		if key in result:
			value = result[key]
			if isinstance(value, bool) and kind != 'bool':
				value = int(value)  # Historical numeric separator defaults use False.
			result[key] = attribute_values.numeric_value(value, kind)
	minimum, maximum = result.get('minValue'), result.get('maxValue')
	if minimum is not None and maximum is not None and minimum > maximum:
		raise ValueError('Attribute minimum exceeds maximum')
	default = result.get('defaultValue', 0)
	if ((result.get('hasMinValue', minimum is not None) and minimum is not None and default < minimum)
			or (result.get('hasMaxValue', maximum is not None) and maximum is not None and default > maximum)):
		raise ValueError('Attribute default is outside its declared range')
	if kind == 'enum':
		fields = result.get('enumName')
		if not isinstance(fields, string_utils.STRING_TYPES) or not fields or len(fields) > 4096:
			raise ValueError('Enum definition requires bounded field names')
		labels, values, next_value = set(), set(), 0
		for field in fields.split(':'):
			parts = field.split('=')
			if len(parts) > 2 or not parts[0] or parts[0] in labels:
				raise ValueError('Enum fields are empty or duplicated')
			if len(parts) == 2:
				next_value = int(parts[1])
			next_value = attribute_values.numeric_value(next_value, 'enum')
			if next_value in values:
				raise ValueError('Enum field values are duplicated')
			labels.add(parts[0])
			values.add(next_value)
			next_value += 1
		if default not in values:
			raise ValueError('Enum default is not declared')
	elif 'enumName' in result:
		raise ValueError('Enum fields require an enum attribute')
	return result


def connectAttrToList(baseAttr='', attributes=None):
	if attributes is None:
		attributes = []

	if not isinstance(attributes, (list, tuple)) or len(attributes) > parameters.MAX_CONNECTIONS:
		raise ValueError('Connection destinations must be a bounded sequence')
	source = pm.PyNode(baseAttr)
	if not source.isReadable() or not source.isConnectable() or source.isArray():
		raise ValueError('Connection source must be a readable explicit plug')
	targets, seen = [], set()
	source_kind = pm.getAttr(source, type=True)
	for value in attributes:
		target = pm.PyNode(value)
		parameters.editable_node(target.node())
		if target == source or target in seen:
			raise ValueError('Connection destinations contain a self-link or duplicate')
		seen.add(target)
		parameters.writable_port(target, allowed_input=source)
		if not target.isConnectable() or attribute_values.value_width(pm.getAttr(target, type=True)) != attribute_values.value_width(source_kind):
			raise ValueError('Connection requires compatible numeric destinations')
		if target.inputs(plugs=True) != [source]:
			targets.append(target)
	if any(parent in seen for target in seen for parent in target.getAllParents(arrays=True) or []):
		raise ValueError('Do not connect both an attribute and its parent')
	completed = 0
	try:
		for target in targets:
			pm.connectAttr(source, target)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Attribute connection stopped after {0} connections; native edits may remain: {1}'.format(completed, error))
	return completed


def showAttrInChanelBox(node=None, attributes=None):
	if attributes is None:
		attributes = []
	for attr in attributes:
		node.setAttr(attr, e=True, channelBox=True)


def setMultiAttributes(node=None, param=None):
	return parameters.set_values(node, param)


def getMatchNameAttribute(node=None, matchStr='', kwargs=None):
	if kwargs is None:
		kwargs = {}

	attributes = pm.listAttr(node, **kwargs) or []

	for attr in attributes:
		if string_utils.contains(attr, matchStr):
			return attr

	return None


def getMatchNameAttribue(node=None, matchStr='', kwargs=None):
	"""Compatibility wrapper for the historical misspelled function name."""
	return getMatchNameAttribute(node=node, matchStr=matchStr, kwargs=kwargs)


def getAttrDefaultValue(node=None, attr=None):
	u"""Retain the historical raw default-query return shape."""
	return pm.attributeQuery(attr, n=node, ld=True)


def _numeric_default(node, attr):
	attribute_name = u'{0}.{1}'.format(node, attr)
	attribute_type = pm.getAttr(attribute_name, type=True)
	default = attribute_values.numeric_value(getAttrDefaultValue(node, attr), attribute_type)
	return attribute_name, attribute_type, default


def setAttrDefaultValue(node=None, attr=None):
	u"""Reset a supported numeric scalar/vector without losing components."""
	attribute_name, attribute_type, default = _numeric_default(node, attr)
	if attribute_values.value_width(attribute_type) == 1:
		pm.setAttr(attribute_name, default)
	else:
		pm.setAttr(attribute_name, *default, type=attribute_type)


def isStaticAttribute(node=None, attr=None):
	u"""Compare current numeric value with its default, retaining legacy meaning."""
	attribute_name, attribute_type, default = _numeric_default(node, attr)
	current = attribute_values.numeric_value(pm.getAttr(attribute_name), attribute_type)
	return default == current
