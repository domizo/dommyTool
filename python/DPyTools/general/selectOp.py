# -*- coding: utf-8 -*-
"""Compatibility imports for the historical selection path."""

from DPyTools.scene import selection as _implementation

isSelectItem = _implementation.isSelectItem
nodeFilter = _implementation.nodeFilter
getSelectionWithFilterType = _implementation.getSelectionWithFilterType
getJointlist = _implementation.getJointlist
getGeometryList = _implementation.getGeometryList
getGeometryeList = _implementation.getGeometryeList
getMeshList = _implementation.getMeshList
getNurbsSurfaceList = _implementation.getNurbsSurfaceList
getNurbsCurveList = _implementation.getNurbsCurveList
getObjectInHierarchy = _implementation.getObjectInHierarchy
getNodeFromHierarchy = _implementation.getNodeFromHierarchy
getSelectedEdgeIdsList = _implementation.getSelectedEdgeIdsList

__all__ = (
    'isSelectItem', 'nodeFilter', 'getSelectionWithFilterType', 'getJointlist',
    'getGeometryList', 'getGeometryeList', 'getMeshList',
    'getNurbsSurfaceList', 'getNurbsCurveList', 'getObjectInHierarchy',
    'getNodeFromHierarchy', 'getSelectedEdgeIdsList',
)
