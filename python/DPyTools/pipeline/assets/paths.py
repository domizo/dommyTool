# -*- coding: utf-8 -*-
u"""Maya-independent project layout discovery for legacy asset tools."""

import os

from DPyTools.core import paths as core_paths


CONTENT_DIRECTORY_NAMES = ('3d', 'vfx', 'works')
TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
MAX_COMPONENT_LENGTH = core_paths.MAX_COMPONENT_LENGTH
validate_component = core_paths.validate_component


def child_path(root, component, field_name='path component'):
	u"""Return a single validated child contained by *root*, including symlinks."""
	if not isinstance(root, STRING_TYPES):
		raise TypeError('root must be a string')
	if not root:
		raise ValueError('root must not be empty')
	component = validate_component(component, field_name=field_name)
	candidate = os.path.join(root, component)
	if not core_paths.is_within(candidate, root):
		raise ValueError('{0} resolves outside the allowed root'.format(field_name))
	if core_paths.resolve(candidate) == core_paths.resolve(root):
		raise ValueError('{0} must identify a child path'.format(field_name))
	return candidate


def default_project_root():
	u"""Return the configured project root without embedding workstation paths."""
	configured = os.environ.get(
		'DOMMY_PROJECT_ROOT',
		os.path.join(os.path.expanduser('~'), 'maya', 'projects'))
	return os.path.abspath(os.path.expanduser(configured))


def list_child_directories(root, include_hidden=False):
	u"""List safe immediate child directories without following root escapes."""
	if not isinstance(root, STRING_TYPES) or not root or not os.path.isdir(root):
		return []
	try:
		names = os.listdir(root)
	except OSError:
		return []
	result = []
	for name in names:
		if not include_hidden and name.startswith('.'):
			continue
		try:
			candidate = child_path(root, name)
		except (TypeError, ValueError, OSError):
			continue
		if os.path.isdir(candidate):
			result.append(name)
	return sorted(result)


def project_path(project_root, project_name):
	u"""Resolve a selected project while containing it below its configured root."""
	try:
		return child_path(project_root, project_name, field_name='project name')
	except (TypeError, ValueError):
		return ''


def existing_project_path(path):
	"""Admit an explicit project context without consulting shared preferences."""
	if not isinstance(path, STRING_TYPES):
		raise TypeError('project path must be text')
	if not path:
		return ''
	if (not os.path.isabs(path)
			or any(ord(character) < 32 or ord(character) == 127 for character in path)):
		raise ValueError('project path must be absolute without control characters')
	path = core_paths.resolve(path)
	return path if os.path.isdir(path) else ''


def content_root(project_path):
	u"""Find the first supported content directory below a Maya project."""
	if not project_path or not os.path.isdir(project_path):
		return ''
	for directory_name in CONTENT_DIRECTORY_NAMES:
		candidate = os.path.join(project_path, directory_name)
		if (os.path.isdir(candidate)
				and core_paths.is_within(candidate, project_path)):
			return candidate
	return project_path


def asset_root(project_path):
	u"""Return the existing asset directory for a Maya project."""
	root = content_root(project_path)
	if not root:
		return ''
	candidate = os.path.join(root, 'assets')
	if os.path.isdir(candidate) and core_paths.is_within(candidate, root):
		return candidate
	return ''


def shot_root(project_path):
	u"""Return the existing content directory used by the legacy shot tools."""
	return content_root(project_path)
