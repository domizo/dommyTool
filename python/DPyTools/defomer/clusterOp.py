# -*- coding: utf-8 -*-
"""Compatibility export for the historical misspelled package name."""

from DPyTools.deformer import cluster as _implementation

createClusterFromCurve = _implementation.createClusterFromCurve

__all__ = ('createClusterFromCurve',)
