# -*- coding: utf-8 -*-
"""Compatibility imports for the historical transform-actions path."""

from DPyTools.scene import transforms as _implementation

isRootTrans = _implementation.isRootTrans
hasShape = _implementation.hasShape
getRootTransformStr = _implementation.getRootTransformStr
setTransformToObjectZero = _implementation.setTransformToObjectZero
setRotateAimAxis = _implementation.setRotateAimAxis
getPositionBetweenObjects = _implementation.getPositionBetweenObjects
getDistanceBetweenObjects = _implementation.getDistanceBetweenObjects

__all__ = (
    'isRootTrans', 'hasShape', 'getRootTransformStr',
    'setTransformToObjectZero', 'setRotateAimAxis',
    'getPositionBetweenObjects', 'getDistanceBetweenObjects',
)
