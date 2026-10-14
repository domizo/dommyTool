# -*- coding: utf-8 -*-
"""Owned bottle-rig graph construction before the final native constraint."""

import sys

import pymel.core as pm

from DPyTools.core import geometry
from DPyTools.features import measurement
from DPyTools.scene import groups, ownership, parameters, selection, transforms


def _named_node(owned, kind, name, parent=None):
	kwargs = {'name': name}
	if parent is not None:
		kwargs['parent'] = parent
	node = owned.create(kind, **kwargs)
	if node.name().lstrip(':') != name.lstrip(':'):
		raise ValueError('Native rig naming did not preserve the requested name')
	return node


def createBottleSpinRig(isCenterRig=True):
	"""Return the root handle or False; unknown native side effects may remain."""
	selected = pm.ls(selection=True) or []
	owned = ownership.SceneNodes()
	completed = failed = False
	try:
		if not isinstance(isCenterRig, bool) or len(selected) != 1:
			raise ValueError('Bottle rig requires a Boolean option and one selected transform')
		target = transforms.transform_node(selected[0])
		if target.nodeType() != 'transform':
			raise ValueError('Bottle geometry must be a transform, not a joint or component')
		parameters.value_plan(target, dict((name, pm.getAttr(target.attr(name)))
			for name in ('translate', 'rotate')))
		bbox = measurement.getBBoxInfo(target)
		object_size_x = abs(bbox[3]) if abs(bbox[3]) > 0 else 2.0
		plan = [('root_aux', None, (0, 0, 0), False)]
		parent_name = 'root_aux'
		if isCenterRig:
			position = geometry.position3((0, bbox[7] * .5, 0))
			plan.extend((('trans_c_aux', parent_name, position, True),
				('rot_c_aux', 'trans_c_aux', position, False)))
			parent_name = 'rot_c_aux'
		plan.extend((('spin_aux', parent_name, (0, 0, 0), True),
			('tilt_aux', 'spin_aux', geometry.position3((object_size_x, 0, 0)), True),
			('neg_aux', 'tilt_aux', (0, 0, 0), False),
			('geo_root', 'neg_aux', (0, 0, 0), False)))
		names = dict((name, parameters.control_name(name)) for name in
			[name for name, parent, position, handle in plan] + ['tilt_pivot', 'tilt_pivotShape',
			'bottle_rig_decomposeMatrix', 'bottle_rig_multiplyDivide', 'bottle_rig_parentConstraint'])
		if any(pm.objExists(name) for name in names.values()):
			raise ValueError('A requested bottle-rig name already exists')
		nodes = {}
		for name, parent, position, handle in plan:
			node = owned.adopt(groups.createEmpty(names[name], nodes.get(parent), position))
			nodes[name] = node
			if handle:
				parameters.set_values(node, {'displayHandle': True})
		pivot = _named_node(owned, 'transform', names['tilt_pivot'], nodes['root_aux'])
		_named_node(owned, 'locator', names['tilt_pivotShape'], pivot)
		parameters.set_values(pivot, {'visibility': False})
		dcmat = _named_node(owned, 'decomposeMatrix', names['bottle_rig_decomposeMatrix'])
		multdiv = _named_node(owned, 'multiplyDivide', names['bottle_rig_multiplyDivide'])
		parameters.set_values(multdiv, {'input2Y': -1})
		for source, destination in ((pivot.attr('matrix'), dcmat.attr('inputMatrix')),
				(dcmat.attr('outputTranslate'), nodes['tilt_aux'].attr('rotatePivot')),
				(nodes['spin_aux'].attr('rotateY'), multdiv.attr('input1Y')),
				(multdiv.attr('outputY'), nodes['neg_aux'].attr('rotateY'))):
			pm.connectAttr(source, destination)
		# Do not reparent borrowed geometry. Mutate it only after the graph is complete.
		previous = set(pm.ls(type='parentConstraint') or [])
		result = pm.parentConstraint(nodes['geo_root'], target, name=names['bottle_rig_parentConstraint'])
		constraint = owned.adopt_returned(result, previous, ('parentConstraint',))[0]
		if constraint.name().lstrip(':') != names['bottle_rig_parentConstraint'].lstrip(':'):
			raise ValueError('Native constraint naming did not preserve the requested name')
		completed = True
		return nodes['root_aux']
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning(
			'Bottle rig was not completed; unconfirmed native constraint edits may remain: {0}'
			.format(error))
		return False
	finally:
		try:
			if not completed:
				exception_type = sys.exc_info()[0]
				owned.cleanup(active_error=failed or exception_type is not None)
		finally:
			selection.restore_selection(selected)
