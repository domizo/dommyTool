# -*- coding: utf-8 -*-
"""Checked viewport image acquisition with legacy Maya 2019 pixel conversion."""

import ctypes
import numbers
import os
import sys

import pymel.core as pm
import maya.OpenMaya as api
import maya.OpenMayaUI as apiUI
from PySide2.QtCore import Qt
from PySide2.QtGui import QImage

from DPyTools.core import image_output
from DPyTools.ui import viewport_state


def getActiveCam():
	view = apiUI.M3dView.active3dView()
	dagpath = api.MDagPath()
	view.getCamera(dagpath)
	return dagpath, view


def getActiveViewName():
	return getActiveCam()[0].fullPathName()


def snapShot():
	"""Historical refresh-to-JPEG API; its native file output is not staged here."""
	directory = os.path.join(pm.workspace(query=True, fullName=True), 'images')
	pm.sysFile(directory, makeDir=True)
	destination = os.path.join(directory, 'screenSnapshot.jpg')
	hwr = pm.PyNode('hardwareRenderingGlobals')
	original = hwr.multiSampleEnable.get()
	try:
		hwr.multiSampleEnable.set(1)
		pm.refresh(cv=True, fe='jpg', fn=destination)
	finally:
		exception_type = sys.exc_info()[0]
		try:
			hwr.multiSampleEnable.set(original)
		except (RuntimeError, TypeError, ValueError) as error:
			if exception_type is None:
				raise
			try:
				pm.warning('Snapshot setting restoration also failed: {0}'.format(error))
			except (RuntimeError, TypeError, ValueError):
				pass
	return destination


def getViewForThumbnail():
	"""Keep the historical shader-ball/perspective camera selection policy."""
	actCam, actView = getActiveCam()
	if 'shaderBallCamera' in actCam.fullPathName():
		for index in range(apiUI.M3dView.numberOf3dViews()):
			view = apiUI.M3dView()
			apiUI.M3dView.get3dView(index, view)
			dagpath = api.MDagPath()
			view.getCamera(dagpath)
			if 'persp' in dagpath.fullPathName():
				actCam, actView = dagpath, view
	return actCam, actView


def captureActiveView(filepath='', filename='', ext='png', size=None):
	destination, image_format, size = image_output.output_plan(filepath, filename, ext, size)
	actCam, actView = getViewForThumbnail()
	width, height = actView.portWidth(), actView.portHeight()
	byte_count = image_output.capture_byte_count(width, height)
	img = api.MImage()
	if actView.getRendererName() == actView.kViewport2Renderer:
		img.create(width, height, 4, api.MImage.kFloat)
		actView.readColorBuffer(img)
		img.convertPixelFormat(api.MImage.kByte)
	else:
		actView.readColorBuffer(img)
	address = img.pixels().__long__()
	if isinstance(address, bool) or not isinstance(address, numbers.Integral) or address <= 0:
		raise RuntimeError('Native viewport image did not return a non-null pixel address')
	# Bounds/null checks do not certify native buffer extent or channel layout.
	ptr = ctypes.cast(address, ctypes.POINTER(ctypes.c_char))
	pixels = ctypes.string_at(ptr, byte_count)
	if len(pixels) != byte_count:
		raise RuntimeError('Native viewport byte read returned an unexpected length')
	qimg = QImage(pixels, width, height, QImage.Format_ARGB32)
	if qimg.isNull():
		raise RuntimeError('Viewport image conversion returned a null image')
	qimg = qimg.mirrored(horizontal=False, vertical=True)
	qimg = qimg.scaled(size, size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
	if qimg.isNull():
		raise RuntimeError('Viewport image scaling returned a null image')
	return image_output.publish_image(destination, image_format, qimg.save)


def captureThumbnail(filepath='', filename='thumbnail', ext='png', size=None):
	# Validate before even hiding editor overlays.
	image_output.output_plan(filepath, filename, ext, size)
	with viewport_state.hidden_overlays():
		return captureActiveView(filepath, filename, ext, size)
