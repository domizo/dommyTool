# -*- coding: utf-8 -*-
"""Compatibility imports for the historical material-operations path."""

from DPyTools.pipeline.lookdev import materials as _implementation

getMaterialFromObject = _implementation.getMaterialFromObject
getMaterialFromPerFace = _implementation.getMaterialFromPerFace
getAllFileTypeNode = _implementation.getAllFileTypeNode
isUnResolveFiles = _implementation.isUnResolveFiles
getUnResolveFiles = _implementation.getUnResolveFiles
getListFileDirs = _implementation.getListFileDirs
getListFileDirsStatus = _implementation.getListFileDirsStatus
getAllFileNode = _implementation.getAllFileNode
getAllFileNodeFilePaths = _implementation.getAllFileNodeFilePaths
copyFileNodeImageToFolder = _implementation.copyFileNodeImageToFolder
replaceFilePath = _implementation.replaceFilePath

__all__ = (
    'getMaterialFromObject', 'getMaterialFromPerFace', 'getAllFileTypeNode',
    'isUnResolveFiles', 'getUnResolveFiles', 'getListFileDirs',
    'getListFileDirsStatus', 'getAllFileNode', 'getAllFileNodeFilePaths',
    'copyFileNodeImageToFolder', 'replaceFilePath',
)
