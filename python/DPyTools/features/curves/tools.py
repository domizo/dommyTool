# -*- coding: utf-8 -*-
u"""Checked Maya adapters for the historical curve helper API."""

import pymel.core as pm
import maya.OpenMaya as om

from DPyTools.features.curves import geometry


def getCvCount(crv=None):

	if crv is not None:
		degs = pm.getAttr( (crv+'.degree') )
		spans = pm.getAttr( (crv+'.spans') )
		return degs+spans


	return 0



def createLinearCurve(name='curve1', points=None):
	u"""Create a finite linear curve; empty input retains its historical no-op."""
	if points is None or len(points) == 0:
		return None
	knots = geometry.open_uniform_knots(len(points), 1)
	positions = [geometry.position3(point) for point in points]
	return pm.curve(n=name, d=1, p=positions, k=knots)


def createCurveAlongObject(name='curve1', objects=None, degree=1):
	u"""Build an open curve through checked world-space object positions."""
	if objects is None or len(objects) == 0:
		return None
	knots = geometry.open_uniform_knots(len(objects), degree)
	points = [geometry.position3(pm.xform(node, q=True, ws=True, t=True))
		for node in objects]
	return pm.curve(n=name, d=degree, p=points, k=knots, ws=True)


def _curve_shape(curve):
	u"""Resolve a transform/string/PyNode to its NURBS shape before API access."""
	if curve is None:
		raise ValueError('Input must be a NURBS curve')
	node = pm.PyNode(curve)
	if node.type() == 'transform':
		node = node.getShape()
	if node is None or node.type() != 'nurbsCurve':
		raise ValueError('Input must be a NURBS curve')
	return node


def getCurveParamPosition(curve=None, param=0.0):
	u"""Query position using a normalized [0, 1] parameter, not arc length."""
	try:
		curve = _curve_shape(curve)
	except (RuntimeError, TypeError, ValueError) as exc:
		pm.warning(str(exc))
		return False
	parameter = geometry.domain_parameter(
		curve.getAttr('minValue'), curve.getAttr('maxValue'), param)
	return pm.pointOnCurve(curve, pr=parameter, p=True)


def closestPointOnCurve(location, curveObject):
	u"""Return the raw curve parameter nearest to a finite world-space point."""
	position = geometry.position3(location)
	curve = _curve_shape(curveObject)
	selection = om.MSelectionList()
	selection.add(curve.longName())
	dag_path = om.MDagPath()
	selection.getDagPath(0, dag_path)
	curve_fn = om.MFnNurbsCurve(dag_path)
	point = om.MPoint(*position)
	# Keep the pointer owner alive for the complete native call and readback.
	parameter_owner = om.MScriptUtil()
	# This overload treats the parameter as output, not a search guess.
	# An unchanged sentinel must not be mistaken for a successful parameter 0.
	parameter_owner.createFromDouble(float('nan'))
	pointer = parameter_owner.asDoublePtr()
	curve_fn.closestPoint(point, pointer, geometry.CLOSEST_POINT_TOLERANCE,
		om.MSpace.kWorld)
	return geometry.finite_number(om.MScriptUtil.getDouble(pointer),
		'Closest-point curve parameter')
