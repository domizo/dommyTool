# -*- coding: utf-8 -*-
"""Compatibility import for checked world-space UV queries."""

from DPyTools.features.meshes import uv as _implementation

getUVfromPoint = _implementation.getUVfromPoint

__all__ = ('getUVfromPoint',)
