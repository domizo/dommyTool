# -*- coding: utf-8 -*-
u"""Legacy Maya-facing path facade.

New host-independent code should use :mod:`DPyTools.core.paths`. Functions that
change process state, touch files, or overwrite copies remain only for archived
script compatibility and are intentionally named in ``LEGACY_MUTATING_APIS``.
"""

import os
import ntpath
import shutil

import pymel.core as pm

import DPyTools.system.DFileOp as dfile
from DPyTools.core import diagnostics
import DPyTools.core.paths as core_paths


LEGACY_MUTATING_APIS = (
	'chdir', 'mkEmptyFile', 'checkDirectory', 'copyFile',
)

__all__ = (
	'LEGACY_MUTATING_APIS', 'home', 'chdir', 'pwd', 'mkEmptyFile', 'mkdir',
	'checkDirectory', 'openCreateDirectoryDialog', 'isFile', 'isDir',
	'checkFileName', 'checkDirName', 'getDirCount', 'isWithin', 'delFile',
	'delDir', 'append', 'ls', 'lastComponent', 'extension', 'extention',
	'copyFile',
)

_LOGGER = diagnostics.get_logger(__name__)


def home():
	u"""ユーザーのホームディレクトリを返す"""
	return core_paths.home()

def chdir(path=''):
	u"""Change global process state; retained for historical scripts only."""
	os.chdir(path)

def pwd():
	u"""カレントディレクトリを返す"""
	return os.getcwd()

def mkEmptyFile(path=''):
	u"""Touch a file; retained for historical scripts only."""
	with open(path, 'a'):
		os.utime(path, None)

def mkdir(dirname='', path=''):
	u"""ディレクトリの作成"""
	return core_paths.ensure_directory(dirname, parent=path or None)


def checkDirectory(dirname='', isCreate=True):
	u"""ディレクトリの存在を確認。無ければisCreateがTrueの場合作成"""
	if dfile.isExist(dirname):
		return True

	_LOGGER.debug('Directory does not exist: %s', dirname)
	if not isCreate:
		return False
	mkdir(dirname)
	return dfile.isExist(dirname)



def openCreateDirectoryDialog(dirpath='', messase='', title=''):
	u"""ディレクトリ作成のダイアログの表示"""
	stat = pm.confirmDialog( title=title,
									message=messase,
									button=['Create','Cancel'],
									defaultButton='Create',
									cancelButton='Cancel',
									dismissString='Cancel' )

	if stat == 'Cancel':
		return False
	try:
		# A version directory is a reservation: never accept or reuse an
		# existing path if another process created it after the dialog opened.
		os.mkdir(dirpath)
	except OSError as exc:
		pm.warning('Directory could not be created exclusively: {0}'.format(exc))
		return False
	return dfile.isExist(dirpath)


def isFile(path=''):
	u"""ファイルであるかの確認"""
	return os.path.isfile(path)

def isDir(path=''):
	u"""ディレクトリであるかの確認"""
	return os.path.isdir(path)


def checkFileName(name=''):
	u"""ファイルの名前が有効かの確認"""
	return bool(name)


def checkDirName(name=''):
	u"""ディレクトリの名前が有効かの確認"""
	return bool(name) and isDir(name)


def getDirCount(path='', no_hidden=None):
	u"""指定したディレクトリの数を返す"""
	return len(ls(path=path, no_hidden=no_hidden))


def _resolvedPath(path):
	return core_paths.resolve(path)


def isWithin(path='', root=''):
	"""Return True when *path* resolves to *root* or one of its children."""
	return core_paths.is_within(path, root)


def delFile(path='', root=None):
	u"""指定したファイルの削除"""
	return core_paths.delete_file(path, root=root)

def delDir(path='', root=None):
	u"""指定したディレクトリの削除"""
	return core_paths.delete_empty_directory(path, root=root)

def append(path='', other=''):
	u"""指定したパスにディレクトリ、ファイルのパスの追加"""
	return os.path.join(path, other).replace('/', os.sep)


def ls(path='.', no_hidden=None):
	u"""指定したパスのファイルリストを返す。no_hiddenがTrueで隠されたファイルは返さない"""
	items = os.listdir(path)
	if no_hidden:
		return [item for item in items if not item.startswith('.')]
	return items

def lastComponent(path=''):
	u"""指定したパスの最後の項目を返す"""
	return ntpath.basename(path)

def extension(path=''):
	u"""指定したパスの拡張子を返す"""
	return os.path.splitext(path)[1]


def extention(path=''):
	u"""Compatibility wrapper for the historical misspelled function name."""
	return extension(path=path)

def copyFile(source='', target=''):
	u"""Copy and overwrite like the historical API; prefer a planned copy."""
	return shutil.copy2(source, target)
