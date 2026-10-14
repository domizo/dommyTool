# -*- coding: utf-8 -*-
u"""Low-level sibling-file publication, independent of format and ownership."""

import ctypes
import os
import sys


def unicode_path(value):
	text_type = type(u'')
	if isinstance(value, text_type):
		return value
	return value.decode(sys.getfilesystemencoding() or 'utf-8')


def move_file_windows(source, destination, replace_existing):
	move_file = ctypes.windll.kernel32.MoveFileExW
	move_file.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint]
	move_file.restype = ctypes.c_int
	flags = 0x8 | (0x1 if replace_existing else 0)
	if not move_file(unicode_path(source), unicode_path(destination), flags):
		raise ctypes.WinError()


def replace_file(source, destination):
	replace = getattr(os, 'replace', None)
	if replace is not None:
		replace(source, destination)
		return
	if os.name == 'nt':
		move_file_windows(source, destination, True)
		return
	os.rename(source, destination)


def publish_file_no_replace(source, destination):
	u"""Publish without overwrite; POSIX source cleanup remains caller-owned."""
	if os.name == 'nt':
		move_file_windows(source, destination, False)
	else:
		link = getattr(os, 'link', None)
		if link is None:
			raise OSError('Hard-link publication is unavailable on this platform')
		link(source, destination)
