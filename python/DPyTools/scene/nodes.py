# -*- coding: utf-8 -*-
import pymel.core as pm
import maya.cmds as cmds
import sys

import DPyTools.scene.attributes as dattr
from DPyTools.core import strings as string_utils
from DPyTools.scene import ownership, parameters
'''
to run:
	>>> import DPyTools.scene.nodes as nodeAct


'''


def isExistNode(name=''):
	u"""指定された名前のノードが存在する場合真を返す"""

	return pm.objExists(name)

def deleteNode(name=''):
	u"""指定された名前のノードを削除します"""

	pm.delete(name)

def isExistNodeDelete(name=''):
	u"""指定された名前のノードが存在する場合、削除します"""

	if isExistNode(name):
		deleteNode(name)




_unknown_delete_active = False


def _admit_unknown_node(node):
	"""Recheck the retained identity/type without resolving its old name again."""
	if not pm.objExists(node) or node.nodeType() != 'unknown':
		raise ValueError('A confirmed Unknown node disappeared or changed type')
	return parameters.editable_node(node)


def _unknown_node_plan(values):
	"""Admit the complete bounded, unique, editable Unknown-node batch."""
	values = [] if values is None else values
	nodes = tuple(parameters.resolve_nodes(values, allow_empty=True, require_same_type=False))
	for node in nodes:
		_admit_unknown_node(node)
	return nodes


def deleteUnknown():
	"""Confirm retained Unknown nodes; return returned-delete count or False.

		Cancellation/empty input returns zero. Native partial edits are not undone.
	"""
	global _unknown_delete_active
	if _unknown_delete_active:
		return False
	_unknown_delete_active = True
	completed = 0
	try:
		nodes = _unknown_node_plan(cmds.ls(type='unknown'))
		if not nodes:
			return 0
		result = cmds.confirmDialog(title='Unknown nodes found: {0}'.format(len(nodes)),
			message='Delete the confirmed Unknown nodes from the current scene?',
			button=['Yes', 'No'], defaultButton='No', cancelButton='No', dismissString='No')
		if result != 'Yes':
			return 0
		# Finish post-dialog admission before deleting even the first valid node.
		for node in nodes:
			_admit_unknown_node(node)
		for node in nodes:
			_admit_unknown_node(node)
			pm.delete(node)
			completed += 1
		return completed
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Unknown node deletion stopped after {0} completed delete calls; native edits may remain: {1}'
			.format(completed, error))
		return False
	finally:
		_unknown_delete_active = False




def createNodeWithParam(node_type=None, name='node1', param=None):
	"""Create a dependency node and apply the supplied attribute values."""
	if param is None:
		param = {}
	if not node_type:
		raise ValueError('node_type must be specified')

	parameters.value_items(param)
	name = parameters.control_name(name)
	if pm.objExists(name):
		raise ValueError('Requested node name already exists')
	owned = ownership.SceneNodes()
	completed = False
	try:
		# Shape factories must supply an explicit parent, not an implicit transform.
		if 'shape' in (pm.nodeType(node_type, isTypeName=True, inherited=True) or []):
			raise ValueError('This dependency-node factory does not create shapes')
		newNode = owned.create(node_type, name=name)
		if newNode.name().lstrip(':') != name.lstrip(':'):
			raise ValueError('Native node naming did not preserve the requested name')
		dattr.setMultiAttributes(newNode, param)
		completed = True
		return newNode
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)


def cretateNodeWithParam(type=None, name='node1', param=None):
	"""Compatibility wrapper for the historical misspelled function name."""
	return createNodeWithParam(node_type=type, name=name, param=param)


def getObjectFromList(name='', objList=None):
	if objList is None:
		objList = []

	for o in objList:
		if isinstance(o, basestring):
			if string_utils.contains(o, name):
				return o
		else:
			if string_utils.contains(o.name(), name):
				return o

	return None


def showPyNodeInfo(obj):
	"""与えられたPyNodeの情報を表示します。"""

	lines = ['Info for %s' % obj.name(),'Attributes:']

	for a in obj.listAttr():
		lines.append('   ' + a.name())
	lines.append('MEL type: %s' % obj.type())
	lines.append('MRO:')
	lines.extend(['   '+t.__name__ for t in type(obj).__mro__])
	result = '\n'.join(lines)
	print result




