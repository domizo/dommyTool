# -*- coding: utf-8 -*-
u"""Maya-independent discovery for the explicitly confirmed file cleaner."""

import os
import stat
from collections import namedtuple

from DPyTools.core import atomic_file, paths


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
MAX_CANDIDATES = 10000
MAX_SCANNED_ENTRIES = 100000
DeletionPlan = namedtuple('DeletionPlan', 'root extension root_identity entries')


class FileCleanupError(IOError):
	"""Report confirmed removals without claiming deleted files were restored."""
	def __init__(self, message, deleted=0):
		super(FileCleanupError, self).__init__(message)
		self.deleted = deleted


def validate_root(path):
	if not isinstance(path, STRING_TYPES) or not path:
		raise ValueError('Target folder must not be empty.')
	if os.path.islink(path):
		raise ValueError('Symbolic-link folders are not supported.')

	root = os.path.realpath(os.path.abspath(os.path.expanduser(path)))
	if not os.path.isdir(root):
		raise ValueError('Target folder does not exist: {0}'.format(path))

	drive_root = os.path.abspath(os.path.join(root, os.pardir)) == root
	if drive_root or root == os.path.realpath(os.path.expanduser('~')):
		raise ValueError('Refusing to clean a filesystem root or the home folder.')
	return root


def validate_extension(extension):
	if not isinstance(extension, STRING_TYPES):
		raise TypeError('Extension must be a string.')
	if not extension or not extension.startswith('.'):
		raise ValueError('Extension must start with a dot.')
	if (extension in ('.', '..') or '/' in extension or '\\' in extension
			or any(ord(character) < 32 or ord(character) == 127
				for character in extension)):
		raise ValueError('Invalid extension: {0}'.format(extension))
	return extension


def search_files(path, extension='.pyc'):
	u"""Return sorted regular files with one exact extension below *path*."""
	root = validate_root(path)
	extension = validate_extension(extension)
	results = []
	scanned = 0
	for current_root, directories, filenames in os.walk(root, followlinks=False):
		scanned += len(directories) + len(filenames)
		if scanned > MAX_SCANNED_ENTRIES:
			raise ValueError('File scan exceeds the supported entry budget')
		directories[:] = [
			name for name in directories
			if not os.path.islink(os.path.join(current_root, name))]
		for filename in filenames:
			file_path = os.path.join(current_root, filename)
			if os.path.islink(file_path) or not os.path.isfile(file_path):
				continue
			if os.path.splitext(filename)[1] == extension:
				results.append(file_path)
				if len(results) > MAX_CANDIDATES:
					raise ValueError('File deletion candidates exceed the supported limit')
	return sorted(results)


def _identity(info):
	identity = atomic_file.file_identity(info)
	if identity[1] <= 0:
		raise ValueError('Filesystem does not provide a usable inode identity; deletion is disabled')
	return identity


def _signature(filename):
	info = os.lstat(filename)
	if not stat.S_ISREG(info.st_mode):
		raise ValueError('Deletion target is no longer a regular non-symlink file')
	return _identity(info) + (info.st_mode, info.st_size, info.st_mtime, info.st_ctime)


def _root_identity(root):
	info = os.lstat(root)
	if not stat.S_ISDIR(info.st_mode):
		raise ValueError('Deletion root is no longer a non-symlink directory')
	return _identity(info)


def build_deletion_plan(path, extension='.pyc'):
	"""Snapshot the exact candidates shown to the user in a trusted namespace."""
	root = validate_root(path)
	extension = validate_extension(extension)
	identity = _root_identity(root)
	entries = tuple((filename, _signature(filename)) for filename in search_files(root, extension))
	if _root_identity(root) != identity:
		raise ValueError('Deletion root changed during discovery')
	return DeletionPlan(root, extension, identity, entries)


def _validate_plan_root(plan):
	root = validate_root(plan.root)
	if root != plan.root or _root_identity(root) != plan.root_identity:
		raise ValueError('Deletion root changed after preview')
	return root


def _validate_entry(plan, filename, signature):
	if (not isinstance(filename, STRING_TYPES) or not os.path.isabs(filename)
			or os.path.abspath(filename) != filename
			or not paths.is_within(filename, plan.root)
			or os.path.splitext(filename)[1] != plan.extension):
		raise ValueError('Deletion candidate escapes the confirmed scope')
	parent = os.path.dirname(filename)
	while parent != plan.root:
		if os.path.islink(parent):
			raise ValueError('Deletion candidate acquired a symlinked ancestor')
		previous = parent
		parent = os.path.dirname(parent)
		if parent == previous:
			raise ValueError('Deletion candidate is not a descendant of its root')
	if _signature(filename) != signature:
		raise ValueError('Deletion candidate changed after preview')


def execute_deletion_plan(plan):
	"""Refuse stale previews; recheck each unlink without forcing file permissions.

	Identifiers/timestamps are accidental-deletion guards in trusted stable
	directories, not atomic protection against hostile namespace races.
	"""
	deleted = 0
	try:
		if (not isinstance(plan, DeletionPlan) or not isinstance(plan.entries, tuple)
				or len(plan.entries) > MAX_CANDIDATES):
			raise ValueError('Deletion requires a bounded confirmed plan')
		validate_extension(plan.extension)
		_validate_plan_root(plan)
		seen = set()
		# Admit the complete preview again before removing any valid prefix.
		for filename, signature in plan.entries:
			key = os.path.normcase(filename)
			if key in seen:
				raise ValueError('Deletion plan contains duplicate paths')
			seen.add(key)
			_validate_entry(plan, filename, signature)
		for filename, signature in plan.entries:
			_validate_plan_root(plan)
			_validate_entry(plan, filename, signature)
			if not atomic_file.remove_owned_file(filename, signature[:2], make_writable=False):
				raise ValueError('Deletion candidate identity changed before unlink')
			deleted += 1
	except (OSError, TypeError, ValueError) as error:
		raise FileCleanupError('Deletion stopped after {0} confirmed removals: {1}'.format(deleted, error), deleted)
	return deleted
