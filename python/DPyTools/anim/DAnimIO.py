# -*- coding: utf-8 -*-
"""Compatibility imports for the historical DAnimIO path."""

from DPyTools.features.animation import io as _implementation

getAttributeFromChBoxVis = _implementation.getAttributeFromChBoxVis
d_animWrite = _implementation.d_animWrite
dk_filter_nodes = _implementation.dk_filter_nodes
d_animRead = _implementation.d_animRead
dkAnim = _implementation.dkAnim

__all__ = (
    'getAttributeFromChBoxVis', 'd_animWrite', 'dk_filter_nodes',
    'd_animRead', 'dkAnim',
)
