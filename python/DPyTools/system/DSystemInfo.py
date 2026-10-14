# -*- coding: utf-8 -*-
"""Compatibility exports for the historical Maya diagnostic-report path."""

from DPyTools.features import system_info as _implementation

isLoadedPlugin = _implementation.isLoadedPlugin
loadMayaPlugin = _implementation.loadMayaPlugin
echoWarning = _implementation.echoWarning
displayInfo = _implementation.displayInfo
pluginInfo = _implementation.pluginInfo
currentFileInfo = _implementation.currentFileInfo
displayOpenGLInfo = _implementation.displayOpenGLInfo
clearNodeCache = _implementation.clearNodeCache

__all__ = (
	'isLoadedPlugin', 'loadMayaPlugin', 'echoWarning', 'displayInfo',
	'pluginInfo', 'currentFileInfo', 'displayOpenGLInfo', 'clearNodeCache',
)
