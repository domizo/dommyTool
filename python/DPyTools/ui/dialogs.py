# -*- coding: utf-8 -*-
u"""Small, reusable Qt dialog helpers for Maya 2019."""

from PySide2 import QtCore
from PySide2 import QtWidgets
from shiboken2 import wrapInstance

import maya.OpenMayaUI as omui


def maya_main_window():
	u"""Return Maya's main window, or ``None`` during Maya shutdown."""
	main_window_pointer = omui.MQtUtil.mainWindow()
	if not main_window_pointer:
		return None
	return wrapInstance(long(main_window_pointer), QtWidgets.QWidget)


class DPySdBaseDialog(QtWidgets.QDialog):
	u"""Base class that maintains one live dialog instance per subclass."""

	dlg_instance = None

	@classmethod
	def show_dialog(cls):
		u"""Create or focus the subclass' current dialog instance."""
		if cls.dlg_instance is None:
			cls.dlg_instance = cls()

		if cls.dlg_instance.isHidden():
			cls.dlg_instance.show()
		else:
			cls.dlg_instance.raise_()
			cls.dlg_instance.activateWindow()
		return cls.dlg_instance

	def __init__(self, title='title', parent=None):
		if parent is None:
			parent = maya_main_window()
		super(DPySdBaseDialog, self).__init__(parent)

		self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
		self.setWindowTitle(title)
		self.setMinimumWidth(200)
		self.setWindowFlags(
			QtCore.Qt.Window |
			QtCore.Qt.WindowCloseButtonHint |
			QtCore.Qt.WindowStaysOnTopHint)
		self.create_control()

	def create_control(self):
		u"""Hook for subclasses that need to populate the dialog."""
		return None

	def closeEvent(self, event):
		if type(self).dlg_instance is self:
			type(self).dlg_instance = None
		super(DPySdBaseDialog, self).closeEvent(event)
