# -*- coding: utf-8 -*-
"""Historical animation API backed by one checked implementation."""

from DPyTools.features.animation import operations as _implementation

isAnimationAttribute = _implementation.isAnimationAttribute
isAnimationExist = _implementation.isAnimationExist
deleteAnimationAttribute = _implementation.deleteAnimationAttribute
deleteAnimationKeys = _implementation.deleteAnimationKeys
echoAnimationData = _implementation.echoAnimationData
createAnimCurveUU = _implementation.createAnimCurveUU

__all__ = ['isAnimationAttribute', 'isAnimationExist', 'deleteAnimationAttribute', 'deleteAnimationKeys', 'echoAnimationData', 'createAnimCurveUU']


