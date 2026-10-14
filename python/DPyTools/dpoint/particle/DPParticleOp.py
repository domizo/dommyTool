# -*- coding: utf-8 -*-
"""Compatibility export for the checked particle/emitter builder."""

from DPyTools.features import particles as _implementation

createDPEmitter = _implementation.createDPEmitter

__all__ = ('createDPEmitter',)
