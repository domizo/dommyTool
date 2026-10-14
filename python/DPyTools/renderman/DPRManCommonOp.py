# -*- coding: utf-8 -*-
"""Compatibility export for the RenderMan integration namespace."""

from DPyTools.integrations.renderman import common as _implementation

CreateSimplePxrVDM = _implementation.CreateSimplePxrVDM

__all__ = ('CreateSimplePxrVDM',)
