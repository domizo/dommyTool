# -*- coding: utf-8 -*-
"""Construct the archived eyelash generator with a real live input curve."""

import sys

import pymel.core as pm

from DPyTools.core.numeric import finite_number
from DPyTools.features.curves import edges
from DPyTools.scene import ownership, selection


def createEyelash():
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		selected = pm.ls(orderedSelection=True, flatten=True) or []
		curve = None
		edge_plan = None
		if len(selected) == 1 and not isinstance(selected[0], pm.general.Component):
			curve = selection.resolve_shape(selected[0], ('nurbsCurve',))
			if finite_number(pm.arclen(curve), 'Input curve length') <= 0:
				raise ValueError('Input curve must have positive finite length')
		else:
			components = selection.ordered_edge_plan(selection=selected, limit=edges.MAX_EDGE_CURVE_EDGES)
			edge_plan = edges.plan_edge_curve(components)
		generator = owned.create('DHEyelashGen')
		if edge_plan is None:
			input_curve = curve.attr('worldSpace')[0]
		else:
			mesh, indices, directions = edge_plan
			input_curve = edges.edge_curve_output(owned, mesh, indices, directions)
			pm.setAttr(generator.attr('EdgeList'), ' '.join(str(index) for index in indices), type='string')
			pm.connectAttr(mesh.attr('worldMesh')[0], generator.attr('inMesh'))
		pm.connectAttr(input_curve, generator.attr('inCurve'))
		group = owned.create('curveVarGroup')
		pm.connectAttr(generator.attr('outCurves'), group.attr('create'))
		# Maya creates output transform/shape pairs; no dummy curve is required.
		pm.getAttr(group.attr('local'), silent=True)
		completed = True
		return generator, group
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Eyelash was not created: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)
