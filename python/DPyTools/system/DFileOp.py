# -*- coding: utf-8 -*-
u"""Host-independent legacy file-information facade."""

import os


__all__ = ('isExist', 'isFile', 'isDir', 'isLink', 'getTime', 'size')



def isExist(filepath=''):
	return os.path.exists(filepath)

def isFile(filepath=''):
	return os.path.isfile(filepath)

def isDir(filepath=''):
	return os.path.isdir(filepath)

def isLink(filepath=''):
	return os.path.islink(filepath)

def getTime(filepath=''):
	u"""Return the historical access-time value for compatibility."""
	return os.path.getatime(filepath)

def size(path=''):
	suffix = 'B'
	num = os.path.getsize(path)
	for unit in ('','K','M','G','T','P','E','Z'):
		if abs(num) < 1024.0:
			return "%3.1f%s%s" % (num, unit, suffix)
		num /= 1024.0
	return "%.1f%s%s" % (num, 'Y', suffix)
