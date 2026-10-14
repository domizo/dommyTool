# -*- coding: utf-8 -*-
"""Exclusive, bounded directory creation in retained trusted asset contexts."""

import os
import stat
from collections import namedtuple

from DPyTools.core import atomic_file, paths, strings


MAX_TARGETS = 8
MAX_DEPTH = 3
RootSnapshot = namedtuple('RootSnapshot', 'path identity')
CreationPlan = namedtuple('CreationPlan', 'root targets')


class CreationError(IOError):
	"""Report confirmed new directories without deleting potentially borrowed data."""
	def __init__(self, message, created=()):
		super(CreationError, self).__init__(message)
		self.created = tuple(created)


def _directory_identity(path):
	info = os.lstat(path)
	if not stat.S_ISDIR(info.st_mode):
		raise ValueError('Creation context must be a regular non-symlink directory')
	return atomic_file.file_identity(info)


def _validate_root(root):
	if (not isinstance(root, RootSnapshot) or not isinstance(root.path, strings.STRING_TYPES)
			or not os.path.isabs(root.path) or paths.resolve(root.path) != root.path
			or _directory_identity(root.path) != root.identity):
		raise ValueError('Creation context changed after it was retained')
	return root


def snapshot_root(path):
	"""Retain an absolute directory and identity hint before opening a dialog."""
	if isinstance(path, RootSnapshot):
		return _validate_root(path)
	if (not isinstance(path, strings.STRING_TYPES) or not path or not os.path.isabs(path)
			or any(ord(character) < 32 or ord(character) == 127 for character in path)):
		raise ValueError('Creation root must be absolute text without control characters')
	path = paths.resolve(path)
	return RootSnapshot(path, _directory_identity(path))


def _validate_target(root, target, planned_parents=()):
	if (not isinstance(target, strings.STRING_TYPES) or not os.path.isabs(target)
			or os.path.abspath(target) != target or not paths.is_within(target, root.path)
			or target == root.path):
		raise ValueError('Creation target escapes its retained root')
	components = os.path.relpath(target, root.path).split(os.sep)
	if not 1 <= len(components) <= MAX_DEPTH:
		raise ValueError('Creation target exceeds the supported layout depth')
	for component in components:
		paths.validate_component(component)
	# Borrowed legacy ancestors remain readable; every newly created leaf is portable.
	paths.validate_portable_component(components[-1])
	if os.path.lexists(target):
		raise ValueError('Creation target already exists: {0}'.format(target))
	parent = os.path.dirname(target)
	while parent != root.path:
		if parent not in planned_parents:
			_directory_identity(parent)
		parent = os.path.dirname(parent)


def _validate_plan(plan):
	if (not isinstance(plan, CreationPlan) or not isinstance(plan.targets, tuple)
			or not 1 <= len(plan.targets) <= MAX_TARGETS):
		raise ValueError('Directory creation requires a bounded nonempty plan')
	_validate_root(plan.root)
	seen = set()
	for target in plan.targets:
		_validate_target(plan.root, target, seen)
		key = os.path.normcase(target)
		if key in set(os.path.normcase(path) for path in seen):
			raise ValueError('Creation plan contains duplicate targets')
		seen.add(target)
	return plan


def _plan(root, targets):
	return _validate_plan(CreationPlan(root, tuple(targets)))


def project_plan(root, project_name, content_name='works'):
	root = snapshot_root(root)
	project_name = paths.validate_portable_component(project_name, 'project name')
	content_name = paths.validate_portable_component(content_name, 'content directory')
	project = os.path.join(root.path, project_name)
	content = os.path.join(project, content_name)
	return _plan(root, (project, content) + tuple(
		os.path.join(content, name) for name in ('assets', 'scenes', 'sourceimages')))


def category_plan(root, category):
	root = snapshot_root(root)
	category = paths.validate_portable_component(category, 'asset category')
	return _plan(root, (os.path.join(root.path, category),))


def asset_plan(root, category, asset_name):
	root = snapshot_root(root)
	category = paths.validate_component(category, 'asset category')
	asset_name = paths.validate_portable_component(asset_name, 'asset name')
	category_path = os.path.join(root.path, category)
	targets = () if os.path.lexists(category_path) else (category_path,)
	return _plan(root, targets + (os.path.join(category_path, asset_name),))


def execute(plan):
	"""Create absent targets exclusively; report partial failure without rollback.

	Identity hints and path checks require a stable trusted namespace. They do
	not authenticate ancestry or isolate hostile check/mkdir races. Native inode
	availability and Windows/Python 2 semantics remain host validation gates.
	"""
	created = []
	try:
		_validate_plan(plan)
		for target in plan.targets:
			_validate_root(plan.root)
			_validate_target(plan.root, target)
			os.mkdir(target)
			_directory_identity(target)
			created.append(target)
	except (IOError, OSError, TypeError, ValueError) as error:
		raise CreationError('Creation stopped after {0} confirmed directories; partial output may remain: {1}'.format(
			len(created), error), created)
	return tuple(created)
