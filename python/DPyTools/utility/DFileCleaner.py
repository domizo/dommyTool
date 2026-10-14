# -*- coding: utf-8 -*-
"""Preview and remove files with one extension below a selected folder."""

import pymel.core as pm

from DPyTools.core import file_cleanup
from DPyTools.ui import windows as ui_windows


def _validate_root(path):
	return file_cleanup.validate_root(path)


def _validate_extension(extension):
	return file_cleanup.validate_extension(extension)


def search_folder(path='', extension='.pyc'):
	u"""指定フォルダ以下にある指定拡張子の通常ファイルを収集します。"""
	return file_cleanup.search_files(path, extension=extension)


def searchFolder(path='', type='.pyc', inv=None):
	u"""Compatibility wrapper for the historical function signature."""
	if inv:
		raise ValueError('Exclusive deletion is disabled for safety.')
	return search_folder(path=path, extension=type)


class DFileCleaner(object):
	u"""選択フォルダ内の指定拡張子ファイルを確認後に削除するツール。"""

	m_uiname = 'DFileCleaner'

	def __init__(self):
		self.m_uiname = 'DFileCleaner'

	def openWin(self):
		ui_windows.delete_window(self.m_uiname)

		win = pm.window(self.m_uiname, widthHeight=(320, 260))
		self.form = pm.formLayout(numberOfDivisions=100)
		self.col = pm.columnLayout(rowSpacing=10, adjustableColumn=True)

		pm.rowLayout(numberOfColumns=2, adjustableColumn=1)
		self.projectPathText = pm.textField(placeholderText='Enter target folder')
		pm.button(label='...', c=lambda *args: self.setPathFromDialogCB())
		pm.setParent('..')

		self.extensionText = pm.textField(text='.pyc')
		pm.text(label='Only files with this exact extension will be removed.', align='left')
		pm.button(
			label='Preview and Clean', bgc=[0.5, 0.0, 0.0], height=44,
			c=lambda *args: self.runCleanCB())

		pm.formLayout(
			self.form, edit=True,
			attachForm=[
				(self.col, 'top', 20), (self.col, 'left', 20),
				(self.col, 'right', 20), (self.col, 'bottom', 20)])
		win.show()

	def setPathFromDialogCB(self):
		dirs = pm.fileDialog2(
			caption='Select Folder', okCaption='Set', fileMode=2, dialogStyle=2)
		if dirs:
			pm.textField(self.projectPathText, e=1, text=dirs[0])

	def runCleanCB(self):
		path = pm.textField(self.projectPathText, q=1, text=1)
		extension = pm.textField(self.extensionText, q=1, text=1)

		try:
			root = _validate_root(path)
			plan = file_cleanup.build_deletion_plan(root, extension)
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
			pm.warning(str(error))
			return False

		if not plan.entries:
			pm.warning('No target files found for extension: {0}'.format(extension))
			return False

		for file_path, signature in plan.entries:
			print file_path

		result = pm.confirmDialog(
			title='Confirm file deletion',
			message=(
				'Delete {0} file(s) ending in {1} below:\n{2}'
				.format(len(plan.entries), plan.extension, plan.root)),
			button=['Delete', 'Cancel'],
			defaultButton='Cancel', cancelButton='Cancel', dismissString='Cancel')
		if result != 'Delete':
			return False

		try:
			deleted = file_cleanup.execute_deletion_plan(plan)
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
			pm.warning(str(error))
			return False
		except KeyboardInterrupt:
			try:
				pm.warning('Deletion interrupted; some confirmed files may already have been removed.')
			except (RuntimeError, TypeError, ValueError):
				pass
			raise

		print 'Deleted {0} file(s) below {1}'.format(deleted, root)
		return True
