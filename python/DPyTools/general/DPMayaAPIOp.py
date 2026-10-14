# -*- coding: utf-8 -*-
"""Compatibility names for the checked OpenMaya DAG/mesh boundary."""

from DPyTools.features.meshes import api as _implementation

getDagPathfromName = _implementation.getDagPathfromName
getMeshFn = _implementation.getMeshFn

__all__ = ('getDagPathfromName', 'getMeshFn')
