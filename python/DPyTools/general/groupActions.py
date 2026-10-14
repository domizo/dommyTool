# -*- coding: utf-8 -*-
"""Compatibility exports for the historical group helpers."""

from DPyTools.scene import groups as _implementation

createEmpty = _implementation.createEmpty
createEmptyWithAlign = _implementation.createEmptyWithAlign

__all__ = ('createEmpty', 'createEmptyWithAlign')
