# -*- coding: utf-8 -*-
"""Historical viewport API backed by checked capture and publication."""

from DPyTools.features import view_capture as _implementation

getActiveCam = _implementation.getActiveCam
getActiveViewName = _implementation.getActiveViewName
snapShot = _implementation.snapShot
getViewForThumbnail = _implementation.getViewForThumbnail
captureActiveView = _implementation.captureActiveView
captureThumbnail = _implementation.captureThumbnail

__all__ = ('getActiveCam', 'getActiveViewName', 'snapShot', 'getViewForThumbnail', 'captureActiveView', 'captureThumbnail')
