# -*- coding: utf-8 -*-
"""Maya-independent, containment-aware filesystem helpers."""

import os
import ntpath

from DPyTools.core import strings


MAX_COMPONENT_LENGTH = 255
PORTABLE_RESERVED_NAMES = ('con', 'prn', 'aux', 'nul') + tuple(
	'com{0}'.format(index) for index in range(1, 10)) + tuple(
	'lpt{0}'.format(index) for index in range(1, 10))


def validate_component(component, field_name='path component'):
	"""Admit one filesystem leaf without path traversal or control characters."""
	if not isinstance(component, strings.STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if not component:
		raise ValueError('{0} must not be empty'.format(field_name))
	if component != component.strip():
		raise ValueError('{0} must not start or end with whitespace'.format(field_name))
	if component in ('.', '..'):
		raise ValueError('{0} must be a single directory name'.format(field_name))
	if len(component) > MAX_COMPONENT_LENGTH:
		raise ValueError('{0} is too long'.format(field_name))
	if os.path.isabs(component) or ntpath.splitdrive(component)[0]:
		raise ValueError('{0} must not be an absolute path'.format(field_name))
	if '/' in component or '\\' in component:
		raise ValueError('{0} must not contain path separators'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in component):
		raise ValueError('{0} must not contain control characters'.format(field_name))
	return component


def validate_portable_component(component, field_name='path component'):
	"""Admit new output names; retain validate_component for legacy read paths."""
	component = validate_component(component, field_name)
	if (component.endswith('.') or component.split('.')[0].lower() in PORTABLE_RESERVED_NAMES
			or any(character in component for character in ':*?<>|"')):
		raise ValueError('{0} is not a portable filesystem leaf'.format(field_name))
	return component


def resolve(path):
	if not path:
		raise ValueError('path must not be empty')
	return os.path.realpath(os.path.abspath(os.path.expanduser(path)))


def home():
	return os.path.expanduser('~')


def ensure_directory(dirname, parent=None):
	if not dirname:
		raise ValueError('dirname must not be empty')
	target = os.path.join(parent, dirname) if parent else dirname
	target = os.path.abspath(os.path.expanduser(target))
	if os.path.isdir(target):
		return target
	if os.path.exists(target):
		raise OSError('Path exists and is not a directory: {0}'.format(target))
	os.makedirs(target)
	return target


def is_within(path, root):
	"""Compare resolved, platform-normalized names on a separator boundary."""
	resolved_path = os.path.normcase(resolve(path))
	resolved_root = os.path.normcase(resolve(root))
	root_prefix = resolved_root if resolved_root.endswith(os.sep) else resolved_root + os.sep
	return resolved_path == resolved_root or resolved_path.startswith(root_prefix)


def delete_file(path, root=None):
	if os.path.islink(path):
		raise OSError('Refusing to delete a symbolic link: {0}'.format(path))
	target = resolve(path)
	if root is not None and not is_within(target, root):
		raise ValueError('File is outside the allowed root: {0}'.format(path))
	if not os.path.isfile(target):
		raise OSError('File does not exist: {0}'.format(path))
	os.remove(target)


def delete_empty_directory(path, root=None):
	if os.path.islink(path):
		raise OSError('Refusing to delete a symbolic link: {0}'.format(path))
	target = resolve(path)
	if root is not None and not is_within(target, root):
		raise ValueError('Directory is outside the allowed root: {0}'.format(path))
	if not os.path.isdir(target):
		raise OSError('Directory does not exist: {0}'.format(path))
	os.rmdir(target)
