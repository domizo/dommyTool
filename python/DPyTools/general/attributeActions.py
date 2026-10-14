# -*- coding: utf-8 -*-
"""Compatibility imports for the historical attribute-actions path."""

from DPyTools.scene import attributes as _implementation

hideAttributes = _implementation.hideAttributes
createAttributeFromDic = _implementation.createAttributeFromDic
connectAttrToList = _implementation.connectAttrToList
showAttrInChanelBox = _implementation.showAttrInChanelBox
setMultiAttributes = _implementation.setMultiAttributes
getMatchNameAttribute = _implementation.getMatchNameAttribute
getMatchNameAttribue = _implementation.getMatchNameAttribue
getAttrDefaultValue = _implementation.getAttrDefaultValue
setAttrDefaultValue = _implementation.setAttrDefaultValue
isStaticAttribute = _implementation.isStaticAttribute

__all__ = (
    'hideAttributes', 'createAttributeFromDic', 'connectAttrToList',
    'showAttrInChanelBox', 'setMultiAttributes', 'getMatchNameAttribute',
    'getMatchNameAttribue', 'getAttrDefaultValue', 'setAttrDefaultValue',
    'isStaticAttribute',
)
