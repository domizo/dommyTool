# -*- coding: utf-8 -*-
"""Compatibility exports for :mod:`DPyTools.plugins.dpt_locator`."""

from DPyTools.plugins import dpt_locator as _implementation

nodeTypeName = _implementation.nodeTypeName
nodeTypeId = _implementation.nodeTypeId
myNode = _implementation.myNode
nodeCreator = _implementation.nodeCreator
nodeInitializer = _implementation.nodeInitializer
initializePlugin = _implementation.initializePlugin
uninitializePlugin = _implementation.uninitializePlugin

__all__ = (
	'nodeTypeName', 'nodeTypeId', 'myNode', 'nodeCreator', 'nodeInitializer',
	'initializePlugin', 'uninitializePlugin',
)
