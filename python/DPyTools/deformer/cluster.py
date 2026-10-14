# -*- coding: utf-8 -*-
"""Cluster creation helpers."""

import pymel.core as pm
import sys

from DPyTools.core import components, geometry
from DPyTools.scene import ownership, parameters, selection


MAX_CLUSTER_CVS = 1000


def _curve_plan(value):
	curve = selection.resolve_shape(value, ('nurbsCurve',))
	parameters.editable_node(curve)
	if pm.getAttr(curve.attr('create'), lock=True):
		raise ValueError('Curve construction input is locked')
	# Periodic mathematical CV counts include duplicates that cannot be edited.
	count = components.component_index(curve.numCVs(editableOnly=True))
	if not 1 <= count <= MAX_CLUSTER_CVS:
		raise ValueError('Curve must contain 1..{0} editable CVs'.format(MAX_CLUSTER_CVS))
	plan = tuple(curve.cv[index] for index in range(count))
	for cv in plan:
		geometry.position3(pm.xform(cv, query=True, worldSpace=True, translation=True))
	return plan


def _adopt_cluster(owned, result, previous):
	nodes = owned.adopt_returned(result, previous, ('cluster', 'transform'))
	deformer = next(node for node in nodes if node.nodeType() == 'cluster')
	handle = next(node for node in nodes if node.nodeType() == 'transform')
	shapes = handle.getShapes(noIntermediate=True) or []
	if len(shapes) != 1 or shapes[0].nodeType() != 'clusterHandle' or shapes[0].getParent() != handle:
		raise ValueError('Cluster handle must own one confirmed clusterHandle shape')
	owned.adopt_returned(shapes, previous, ('clusterHandle',))
	return deformer, handle


def createClusterFromCurve(crv=''):
	"""Plan every editable CV first; return handles or explicit failure status.

	Only confirmed cluster/handle/handle-shape identities are cleanup-owned.
	Opaque native sets/history/automatic orig shapes are not swept into deletion.
	"""
	selected = pm.ls(selection=True) or []
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	handles = []
	try:
		plan = _curve_plan(crv)
		for cv in plan:
			previous = set(pm.ls(type=('cluster', 'transform', 'clusterHandle')) or [])
			result = pm.cluster(cv, relative=True, envelope=1)
			deformer, handle = _adopt_cluster(owned, result, previous)
			handles.append(handle)
		completed = True
		return handles
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning(('Curve clusters stopped after {0} confirmed results: {1}. '
			'Known-node cleanup is attempted; unconfirmed native history edits may remain.').format(len(handles), error))
		return False
	finally:
		try:
			if not completed:
				exception_type = sys.exc_info()[0]
				owned.cleanup(active_error=failed or exception_type is not None)
		finally:
			selection.restore_selection(selected)
