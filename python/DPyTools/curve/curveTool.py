# -*- coding: utf-8 -*-
"""Compatibility imports for the historical curve-tool path."""

from DPyTools.features.curves import tools as _implementation

getCvCount = _implementation.getCvCount
createLinearCurve = _implementation.createLinearCurve
createCurveAlongObject = _implementation.createCurveAlongObject
getCurveParamPosition = _implementation.getCurveParamPosition
closestPointOnCurve = _implementation.closestPointOnCurve

__all__ = (
    'getCvCount', 'createLinearCurve', 'createCurveAlongObject',
    'getCurveParamPosition', 'closestPointOnCurve',
)
