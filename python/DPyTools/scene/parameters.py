# -*- coding: utf-8 -*-
"""Complete admission for numeric/string parameter sharing and UI node models."""

import sys
import numbers
import re

import pymel.core as pm

from DPyTools.core import strings
from DPyTools.scene import attribute_values, ownership


MAX_CONNECTIONS = 10000
SUPPORTED_TYPES = attribute_values.SCALAR_TYPES + attribute_values.COMPOUND_TYPES + ('string',)


def resolve_nodes(values, allow_empty=False, require_same_type=True):
	if not isinstance(values, (list, tuple)) or len(values) > MAX_CONNECTIONS:
		raise ValueError('Node list must be a bounded sequence')
	nodes = [pm.PyNode(value) for value in values]
	if not nodes and not allow_empty:
		raise ValueError('At least one node is required')
	if len(set(nodes)) != len(nodes):
		raise ValueError('Node list contains duplicate identities')
	if require_same_type and nodes and any(node.nodeType() != nodes[0].nodeType() for node in nodes):
		raise ValueError('All parameter nodes must have the same type')
	return nodes


def merge_nodes(existing, requested):
	current = resolve_nodes(existing, allow_empty=True)
	added = resolve_nodes(requested, allow_empty=True)
	if current and added and current[0].nodeType() != added[0].nodeType():
		raise ValueError('Added nodes must match the existing node type')
	return resolve_nodes(current + [node for node in added if node not in current], allow_empty=True)


def node_label(node):
	return node.longName() if isinstance(node, pm.nt.DagNode) else node.name()


def attribute_names(attributes):
	if not isinstance(attributes, (list, tuple)) or not attributes:
		raise ValueError('Select at least one attribute')
	if len(attributes) > MAX_CONNECTIONS or len(set(attributes)) != len(attributes):
		raise ValueError('Attribute selection is too large or duplicated')
	for name in attributes:
		if (not isinstance(name, strings.STRING_TYPES) or not name or len(name) > 255
				or any(character in name for character in '|.*?[]')
				or any(character.isspace() or ord(character) < 32 for character in name)):
			raise ValueError('Attribute selection must contain simple attribute names')
	return tuple(attributes)


def _ports(node, attributes, writable=False):
	if writable:
		node = editable_node(node)
	result = {}
	for name in attributes:
		if not node.hasAttr(name):
			raise ValueError('Node does not contain attribute: {0}'.format(name))
		parents = pm.attributeQuery(name, node=node, listParent=True) or []
		if any(parent in attributes for parent in parents):
			raise ValueError('Do not select both a compound and its child attributes')
		plug = node.attr(name)
		kind = pm.getAttr(plug, type=True)
		if kind not in SUPPORTED_TYPES:
			raise ValueError('Unsupported shared attribute type: {0}'.format(kind))
		if not pm.attributeQuery(name, node=node, readable=True, connectable=True):
			raise ValueError('Attribute must be readable and connectable')
		if writable and not pm.attributeQuery(name, node=node, writable=True):
			raise ValueError('Destination attribute is not writable')
		value = pm.getAttr(plug)
		value = _admit_value(plug, kind, value)
		children = pm.attributeQuery(name, node=node, listChildren=True) or []
		plugs = [plug] + [node.attr(child) for child in children + parents]
		if writable and any(pm.getAttr(child, lock=True) for child in plugs):
			raise ValueError('Destination attribute or child is locked')
		result[name] = (plug, kind, value, plugs)
	return result


def common_attributes(nodes, require_same_type=True):
	nodes = resolve_nodes(nodes, allow_empty=True, require_same_type=require_same_type)
	if not nodes:
		return []
	attributes = pm.listAttr(nodes[0], connectable=True, write=True) or []
	if len(attributes) * len(nodes) > MAX_CONNECTIONS:
		raise ValueError('Attribute view exceeds the parameter-inspection limit')
	return sorted(name for name in attributes if all(node.hasAttr(name) for node in nodes)
		and pm.getAttr(nodes[0].attr(name), type=True) in SUPPORTED_TYPES)


def control_name(name):
	if (not isinstance(name, strings.STRING_TYPES) or not name or len(name) > 240
			or name != name.strip() or any(character in name for character in '|.*?[]')
			or any(character.isspace() or ord(character) < 32 for character in name)):
		raise ValueError('Share group name must be a simple namespace-qualified name')
	if any(not part for part in name.lstrip(':').split(':')):
		raise ValueError('Share group name has an empty namespace or leaf')
	return ':' + name.lstrip(':')


def _set_value(port, kind, value):
	if kind == 'string':
		pm.setAttr(port, value, type='string')
	elif attribute_values.value_width(kind) == 1:
		pm.setAttr(port, value)
	else:
		pm.setAttr(port, *value, type=kind)


def attribute_path(name):
	"""Admit a bounded explicit plug path, including sparse array elements."""
	if (not isinstance(name, strings.STRING_TYPES) or not name or len(name) > 255
			or not re.match(r'^[A-Za-z_][A-Za-z0-9_]*(\[[0-9]+\])?(\.[A-Za-z_][A-Za-z0-9_]*(\[[0-9]+\])?)*\Z', name)):
		raise ValueError('Attribute path must contain explicit names and nonnegative indices')
	if any(int(index) > 2147483647 for index in re.findall(r'\[([0-9]+)\]', name)):
		raise ValueError('Attribute index exceeds the native integer limit')
	return name


def value_items(values):
	"""Snapshot the complete mapping before node allocation or mutation."""
	if values is None:
		values = {}
	if not isinstance(values, dict) or len(values) > MAX_CONNECTIONS:
		raise ValueError('Attribute values must be a bounded dictionary')
	names = [attribute_path(name) for name in values]
	return tuple((name, values[name]) for name in sorted(names))


def editable_node(value):
	node = pm.PyNode(value)
	if node.isReferenced() or any(pm.lockNode(node, query=True, lock=True) or []):
		raise ValueError('Attribute edits refuse referenced or locked nodes')
	if isinstance(node, pm.nt.DagNode) and node.isInstanced():
		raise ValueError('Attribute edits require a non-instanced DAG node')
	return node


def writable_port(plug, allowed_input=None):
	"""Check the plug and its actual parents/children, not reconstructed names."""
	if plug.isArray():
		raise ValueError('Select an explicit array element, not a whole multi attribute')
	name = plug.lastPlugAttr(longName=True)
	if not pm.attributeQuery(name, node=plug.node(), writable=True):
		raise ValueError('Destination attribute is not writable')
	related = [plug] + (plug.getAllParents(arrays=True) or [])
	if plug.isCompound():
		related.extend(plug.getChildren() or [])
	if len(related) > MAX_CONNECTIONS:
		raise ValueError('Attribute hierarchy exceeds the inspection limit')
	for port in related:
		inputs = port.inputs(plugs=True) or []
		if (pm.getAttr(port, lock=True) or (inputs and not
				(allowed_input is not None and port == plug and inputs == [allowed_input]))):
			raise ValueError('Destination or its parent/child is locked or connected')
	return tuple(related)


def _value_ports(plug, kind):
	ports = [plug] if kind == 'string' or attribute_values.value_width(kind) == 1 else (plug.getChildren() or [])
	if len(ports) != (1 if kind == 'string' else attribute_values.value_width(kind)):
		raise ValueError('Numeric compound schema has an unexpected width')
	return ports


def _compatible_schema(source, target, kind, shared=False):
	"""Preserve enum meaning; live drivers must fit destination declared ranges."""
	if kind == 'string':
		return
	for first, second in zip(_value_ports(source, kind), _value_ports(target, kind)):
		first_kind, second_kind = pm.getAttr(first, type=True), pm.getAttr(second, type=True)
		if first_kind != second_kind:
			raise ValueError('Attribute component types do not match')
		if first_kind == 'enum' and dict(first.getEnums()) != dict(second.getEnums()):
			raise ValueError('Enum field names and numeric values do not match')
		if shared:
			first_min, first_max = _driver_bounds(first, first_kind)
			second_min, second_max = _driver_bounds(second, second_kind)
			if first_min < second_min or first_max > second_max:
				raise ValueError('Driver declared range is not contained by the destination range')


def _driver_bounds(port, kind):
	minimum, maximum = attribute_values.numeric_limits(kind)
	if kind == 'enum':
		fields = list(port.getEnums().values())
		if not fields:
			raise ValueError('Enum schema must contain fields')
		minimum, maximum = min(fields), max(fields)
	declared_min, declared_max = _declared_bounds(port, kind)
	if declared_min is not None:
		minimum = max(minimum, declared_min)
	if declared_max is not None:
		maximum = min(maximum, declared_max)
	if minimum > maximum:
		raise ValueError('Attribute schema has an empty declared range')
	return minimum, maximum


def _declared_bounds(port, kind):
	minimum, maximum = port.getMin(), port.getMax()
	minimum = attribute_values.numeric_value(minimum, kind) if minimum is not None else None
	maximum = attribute_values.numeric_value(maximum, kind) if maximum is not None else None
	if minimum is not None and maximum is not None and minimum > maximum:
		raise ValueError('Attribute minimum exceeds maximum')
	return minimum, maximum


def _admit_value(plug, kind, value):
	if kind == 'string':
		if not isinstance(value, strings.STRING_TYPES):
			raise TypeError('String attribute requires a string value')
		return value
	value = attribute_values.numeric_value(value, kind)
	ports = _value_ports(plug, kind)
	values = value if attribute_values.value_width(kind) > 1 else [value]
	for port, component in zip(ports, values):
		component_kind = pm.getAttr(port, type=True)
		minimum, maximum = _declared_bounds(port, component_kind)
		if (minimum is not None and component < minimum) or (maximum is not None and component > maximum):
			raise ValueError('Attribute value is outside its declared range')
		if component_kind == 'enum' and component not in port.getEnums().values():
			raise ValueError('Enum value is not declared by this attribute')
	return value


def admit_value(plug, kind, value):
	"""Reuse typed/range/enum admission without performing a value write."""
	return _admit_value(plug, kind, value)


def value_plan(node, values):
	"""Resolve every value and destination before the first write."""
	items = value_items(values)
	node = editable_node(node)
	plan, selected = [], set()
	for name, value in items:
		plug = node.attr(name)
		if plug in selected:
			raise ValueError('Attribute mapping contains aliased duplicate destinations')
		selected.add(plug)
		related = writable_port(plug)
		kind = pm.getAttr(plug, type=True)
		if kind not in SUPPORTED_TYPES:
			raise ValueError('Unsupported attribute value type: {0}'.format(kind))
		plan.append((plug, kind, _admit_value(plug, kind, value), related))
	if any(parent in selected for plug, kind, value, related in plan
			for parent in plug.getAllParents(arrays=True) or []):
		raise ValueError('Do not write both an attribute and its parent')
	return tuple((plug, kind, value) for plug, kind, value, related in plan)


def write_values(plan):
	"""Dispatch a previously admitted snapshot; do not invent native rollback."""
	completed = 0
	try:
		for plug, kind, value in plan:
			_set_value(plug, kind, value)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Attribute setting stopped after {0} writes; native edits may remain: {1}'.format(completed, error))
	return completed


def set_values(node, values):
	return write_values(value_plan(node, values))


def set_values_many(nodes, values):
	"""Admit the complete target batch before dispatching any value writes."""
	items = value_items(values)
	nodes = resolve_nodes(nodes, allow_empty=True, require_same_type=False)
	if len(nodes) * len(items) > MAX_CONNECTIONS:
		raise ValueError('Attribute batch plan exceeds the limit')
	plan = []
	for node in nodes:
		plan.extend(value_plan(node, dict(items)))
	return write_values(plan)


def copy_parameters(source, targets, attributes=None):
	"""Copy one admitted numeric/string snapshot; never disconnect animation."""
	source = pm.PyNode(source)
	targets = resolve_nodes(targets, require_same_type=False)
	if source in targets:
		raise ValueError('Copy targets must not contain the source')
	if any(node.isReferenced() for node in targets):
		raise ValueError('Parameter copying refuses referenced destinations')
	if attributes is None:
		attributes = common_attributes([source] + targets, require_same_type=False)
		# A compound owns its children; do not write the same fields twice.
		attributes = [name for name in attributes if not any(parent in attributes for parent in
			(pm.attributeQuery(name, node=source, listParent=True) or []))]
	attributes = attribute_names(attributes)
	if len(attributes) * len(targets) > MAX_CONNECTIONS:
		raise ValueError('Parameter copy plan exceeds the limit')
	source_ports = _ports(source, attributes)
	plans = []
	for node in targets:
		ports = _ports(node, attributes, writable=True)
		for name in attributes:
			if ports[name][1] != source_ports[name][1]:
				raise ValueError('Copy attribute types do not match')
			if any(port.inputs(plugs=True) for port in ports[name][3]):
				raise ValueError('Copy destination has an existing input connection')
			_compatible_schema(source_ports[name][0], ports[name][0], ports[name][1])
			value = _admit_value(ports[name][0], ports[name][1], source_ports[name][2])
			plans.append((ports[name][0], ports[name][1], value))
	completed = 0
	try:
		for port, kind, value in plans:
			_set_value(port, kind, value)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError('Parameter copy stopped after {0} writes; native edits may remain: {1}'.format(completed, error))
	return completed


def share_parameters(values, attributes, mode=1, name='', allow_overwrite=False):
	"""Preflight every target; native dispatch failures report remaining edits."""
	if isinstance(mode, bool) or not isinstance(mode, numbers.Integral) or mode not in (1, 2, 3):
		raise ValueError('Unknown parameter-sharing mode')
	if not isinstance(allow_overwrite, bool):
		raise TypeError('Connection overwrite permission must be Boolean')
	nodes = resolve_nodes(values)
	attributes = attribute_names(attributes)
	if len(nodes) * len(attributes) > MAX_CONNECTIONS:
		raise ValueError('Parameter connection plan exceeds the limit')
	if any(node.isReferenced() for node in (nodes[1:] if mode == 3 else nodes)):
		raise ValueError('Parameter sharing refuses referenced destination nodes')
	target_ports = [_ports(node, attributes, writable=not (mode == 3 and index == 0))
		for index, node in enumerate(nodes)]
	group = None
	controller = None
	if mode in (1, 2):
		name = control_name(name)
		controller_name = name + 'Cntlr'
		if mode == 1:
			if pm.objExists(name) or pm.objExists(controller_name):
				raise ValueError('Share group or controller name already exists')
		else:
			group = pm.PyNode(name)
			controller = pm.PyNode(controller_name)
			if group.nodeType() != 'objectSet' or controller.nodeType() != nodes[0].nodeType():
				raise ValueError('Existing share group/controller type does not match')
			if group.isReferenced():
				raise ValueError('Existing share group is referenced')
	elif mode == 3:
		controller = nodes[0]
	source_ports = _ports(controller, attributes) if controller is not None else target_ports[0]
	for node, ports in zip(nodes, target_ports):
		if node == controller:
			continue
		for attribute in attributes:
			if ports[attribute][1] != source_ports[attribute][1]:
				raise ValueError('Shared attribute types do not match')
			_compatible_schema(source_ports[attribute][0], ports[attribute][0], ports[attribute][1], shared=True)
			_admit_value(ports[attribute][0], ports[attribute][1], source_ports[attribute][2])
			for plug in ports[attribute][3]:
				inputs = plug.inputs(plugs=True) or []
				if inputs and not allow_overwrite:
					if controller is None or plug != ports[attribute][0] or inputs != [source_ports[attribute][0]]:
						raise ValueError('Existing input connection requires explicit overwrite permission')
	owned = ownership.SceneNodes()
	dispatched = False
	completed = 0
	try:
		if mode == 1:
			if isinstance(nodes[0], pm.nt.Shape):
				parent = owned.create('transform', name=controller_name + 'Transform')
				controller = owned.create(nodes[0].nodeType(), name=controller_name, parent=parent)
			else:
				controller = owned.create(nodes[0].nodeType(), name=controller_name)
			if controller.name().lstrip(':') != controller_name.lstrip(':'):
				raise ValueError('Native controller naming did not preserve the requested name')
			new_ports = _ports(controller, attributes, writable=True)
			initial_values = []
			for attribute in attributes:
				if new_ports[attribute][1] != source_ports[attribute][1]:
					raise ValueError('New controller does not contain a compatible attribute')
				_compatible_schema(new_ports[attribute][0], source_ports[attribute][0], new_ports[attribute][1], shared=True)
				value = _admit_value(new_ports[attribute][0], new_ports[attribute][1], source_ports[attribute][2])
				initial_values.append((new_ports[attribute][0], new_ports[attribute][1], value))
			for port, kind, value in initial_values:
				_set_value(port, kind, value)
			source_ports = new_ports
			group = owned.create('objectSet', name=name)
			if group.name().lstrip(':') != name.lstrip(':'):
				raise ValueError('Native share-group naming did not preserve the requested name')
		for node, ports in zip(nodes, target_ports):
			if node == controller:
				continue
			for attribute in attributes:
				source = source_ports[attribute][0]
				target = ports[attribute][0]
				if target.inputs(plugs=True) == [source]:
					continue
				# Once dispatched, unknown native side effects may reference this driver.
				dispatched = True
				pm.connectAttr(source, target, force=allow_overwrite)
				completed += 1
		if group is not None:
			for node in [controller] + nodes:
				if not pm.sets(group, isMember=node):
					pm.sets(group, addElement=node)
		return controller
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError(('Parameter sharing stopped after {0} completed connections: {1}. '
			'Native edits and created controllers may remain; no automatic undo was attempted.').format(completed, error))
	finally:
		exception_type = sys.exc_info()[0]
		if exception_type is not None and not dispatched:
			owned.cleanup(active_error=True)
