# -*- coding: utf-8 -*-
"""Compatibility imports for the historical Alembic-export path."""

from DPyTools.pipeline.io import alembic as _implementation

ddata_export = _implementation.ddata_export

__all__ = ('ddata_export',)
