# -*- coding: utf-8 -*-
"""Known-node cleanup and accidental-deletion guards, not scene authentication."""

import uuid

import pymel.core as pm


OWNER_ATTRIBUTE = 'dommyToolOwnerId'
TOOL_ATTRIBUTE = 'dommyToolOwnerTool'


class SceneNodes(object):
	"""Retain only handles returned by this operation's creation calls."""
	def __init__(self):
		self.nodes = []

	def adopt(self, node):
		node = pm.PyNode(node)
		if node in self.nodes:
			raise ValueError('Created node was returned more than once')
		self.nodes.append(node)
		return node

	def create(self, node_type, **kwargs):
		kwargs['skipSelect'] = True
		return self.adopt(pm.createNode(node_type, **kwargs))

	def adopt_returned(self, result, previous, expected_types):
		"""Adopt only confirmed new native results, never a scene-name difference."""
		values = result if isinstance(result, (list, tuple)) else [result]
		if len(values) != len(expected_types) or any(value is None for value in values):
			raise ValueError('Native creation returned an unexpected result count')
		nodes = [pm.PyNode(value) for value in values]
		if (len(set(nodes)) != len(nodes) or any(node in previous or node in self.nodes for node in nodes)
				or sorted(node.nodeType() for node in nodes) != sorted(expected_types)):
			raise ValueError('Native creation did not return distinct new nodes of the expected types')
		if any(node.isReferenced() or (isinstance(node, pm.nt.DagNode) and node.isInstanced()) for node in nodes):
			raise ValueError('Native creation returned referenced or instanced nodes')
		return tuple(self.adopt(node) for node in nodes)

	def cleanup(self, active_error=False):
		"""Attempt all known deletions, refusing borrowed descendants/instances."""
		errors = []
		known = set(self.nodes)
		for node in reversed(self.nodes):
			try:
				if not pm.objExists(node):
					continue
				if node.isReferenced():
					raise ValueError('Refusing cleanup of a referenced node')
				if isinstance(node, pm.nt.DagNode):
					if node.isInstanced():
						raise ValueError('Refusing cleanup of an instanced node')
					children = pm.listRelatives(node, allDescendents=True) or []
					if any(child not in known for child in children):
						raise ValueError('Refusing cleanup of a borrowed descendant')
				pm.delete(node)
			except (RuntimeError, TypeError, ValueError) as error:
				errors.append(error)
		if errors:
			if not active_error:
				raise errors[0]
			try:
				pm.warning('Known-node cleanup also failed: {0}'.format(errors[0]))
			except (RuntimeError, TypeError, ValueError):
				pass


def mark_hierarchy(root, nodes, tool_name):
	"""Mark completed generated nodes; markers do not authenticate provenance."""
	token = str(uuid.uuid4())
	for node in nodes:
		node.addAttr(OWNER_ATTRIBUTE, dataType='string')
		node.attr(OWNER_ATTRIBUTE).set(token)
		node.attr(OWNER_ATTRIBUTE).lock()
	root.addAttr(TOOL_ATTRIBUTE, dataType='string')
	root.attr(TOOL_ATTRIBUTE).set(tool_name)
	root.attr(TOOL_ATTRIBUTE).lock()


def require_owned_hierarchy(root, tool_name, protected=None):
	"""Refuse unmarked, foreign, referenced or instanced deletion candidates."""
	root = pm.PyNode(root)
	if root.nodeType() != 'transform' or not root.hasAttr(TOOL_ATTRIBUTE):
		raise ValueError('Existing group is not a managed tool result')
	if root.attr(TOOL_ATTRIBUTE).get() != tool_name or not root.hasAttr(OWNER_ATTRIBUTE):
		raise ValueError('Existing group belongs to another operation')
	token = root.attr(OWNER_ATTRIBUTE).get()
	try:
		valid_token = bool(token) and str(uuid.UUID(token)) == token
	except (AttributeError, TypeError, ValueError):
		valid_token = False
	if not valid_token:
		raise ValueError('Existing group has invalid ownership metadata')
	nodes = [root] + (pm.listRelatives(root, allDescendents=True) or [])
	if protected is not None and protected in nodes:
		raise ValueError('The measurement target must not be deleted')
	for node in nodes:
		if node.isReferenced() or node.isInstanced():
			raise ValueError('Managed deletion refuses references and instances')
		if not node.hasAttr(OWNER_ATTRIBUTE) or node.attr(OWNER_ATTRIBUTE).get() != token:
			raise ValueError('Existing group contains an unowned descendant')
	return root
