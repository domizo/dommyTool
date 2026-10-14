# -*- coding: utf-8 -*-
"""Legacy viewport locator MPxNode plug-in."""

import sys

import maya.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx
import maya.OpenMayaRender as OpenMayaRender


nodeTypeName = 'DPTLocator'
nodeTypeId = OpenMaya.MTypeId(0x88010)

glRenderer = OpenMayaRender.MHardwareRenderer.theRenderer()
glFT = glRenderer.glFunctionTable()


class myNode(OpenMayaMPx.MPxLocatorNode):
	def __init__(self):
		OpenMayaMPx.MPxLocatorNode.__init__(self)

	def draw(self, view, path, style, status):
		view.beginGL()
		try:
			glFT.glBegin(OpenMayaRender.MGL_LINES)
			glFT.glVertex3f(0.0, -0.5, 0.0)
			glFT.glVertex3f(0.0, 0.5, 0.0)
			glFT.glEnd()
		finally:
			view.endGL()


def nodeCreator():
	return OpenMayaMPx.asMPxPtr(myNode())


def nodeInitializer():
	return OpenMaya.MStatus.kSuccess


def initializePlugin(obj):
	plugin = OpenMayaMPx.MFnPlugin(obj)
	try:
		plugin.registerNode(
			nodeTypeName, nodeTypeId, nodeCreator, nodeInitializer,
			OpenMayaMPx.MPxNode.kLocatorNode)
	except RuntimeError:
		sys.stderr.write('Failed to register node: %s\n' % nodeTypeName)
		raise


def uninitializePlugin(obj):
	plugin = OpenMayaMPx.MFnPlugin(obj)
	try:
		plugin.deregisterNode(nodeTypeId)
	except RuntimeError:
		sys.stderr.write('Failed to deregister node: %s\n' % nodeTypeName)
		raise
