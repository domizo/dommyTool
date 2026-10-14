# -*- coding: utf-8 -*-
"""Compatibility import for checked follicle creation."""

from DPyTools.features.rigging import follicles as _implementation

create_follicle = _implementation.create_follicle

__all__ = ('create_follicle',)
