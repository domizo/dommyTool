# -*- coding: utf-8 -*-
"""Compatibility imports for the historical misspelled module name."""

from DPyTools.create import rig_controller as _implementation

createRigCtrlBase = _implementation.createRigCtrlBase
createRigCtrl = _implementation.createRigCtrl
openWin = _implementation.openWin

__all__ = ('createRigCtrlBase', 'createRigCtrl', 'openWin')
