# -*- coding: utf-8 -*-
"""Compatibility imports for the historical mesh-tool path."""

from DPyTools.features.meshes import tools as _implementation

getSelectedMeshIds = _implementation.getSelectedMeshIds
getClosestPointFromPoint = _implementation.getClosestPointFromPoint

__all__ = ('getSelectedMeshIds', 'getClosestPointFromPoint')
