# -*- coding: utf-8 -*-
u"""Host-independent planning and execution of collision-safe texture copies."""

import os
import shutil

from DPyTools.core import paths as core_paths
from DPyTools.core.atomic_file import StagedFile, remove_owned_file


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)


class TextureCopyError(ValueError):
	pass


class _CopyOwnership(object):
	def __init__(self, destination, identity):
		self.destination = destination
		self.identity = identity
		self.published = False


def _source_path(value):
	if not isinstance(value, STRING_TYPES) or not value:
		raise TypeError('texture source must be a non-empty string')
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise TextureCopyError('texture source contains a control character')
	path = os.path.abspath(os.path.expanduser(value))
	if not os.path.isfile(path):
		raise IOError('texture source does not exist: {0}'.format(value))
	return path


def build_copy_plan(source_paths, destination_directory):
	u"""Return ``(copies, mapping)`` after rejecting basename collisions."""
	if not isinstance(source_paths, (list, tuple)):
		raise TypeError('source_paths must be a list or tuple')
	if not isinstance(destination_directory, STRING_TYPES):
		raise TypeError('destination_directory must be a string')
	if not destination_directory:
		raise TextureCopyError('texture destination must not be empty')
	if any(ord(character) < 32 or ord(character) == 127 for character in destination_directory):
		raise TextureCopyError('texture destination contains a control character')
	destination_directory = os.path.abspath(
		os.path.expanduser(destination_directory))
	if not os.path.isdir(destination_directory):
		raise IOError(
			'texture destination does not exist: {0}'.format(destination_directory))
	if os.path.islink(destination_directory):
		raise TextureCopyError('texture destination must not be a symbolic link')

	copies = []
	mapping = {}
	destination_sources = {}
	for original_source in source_paths:
		source = _source_path(original_source)
		try:
			filename = core_paths.validate_portable_component(os.path.basename(source), 'texture filename')
		except (TypeError, ValueError) as error:
			raise TextureCopyError(str(error))
		destination = os.path.join(destination_directory, filename)
		if not core_paths.is_within(destination, destination_directory):
			raise TextureCopyError('texture destination escapes its directory')
		key = filename.lower()
		previous = destination_sources.get(key)
		if previous is not None:
			previous_source, previous_destination = previous
			if core_paths.resolve(previous_source) != core_paths.resolve(source):
				raise TextureCopyError(
					'texture filename collision: {0}'.format(filename))
			# A differently spelled reference to the same file reuses the first
			# portable destination name rather than planning a missing alias.
			destination = previous_destination
		else:
			if os.path.lexists(destination):
				raise TextureCopyError(
					'texture destination already exists: {0}'.format(destination))
			destination_sources[key] = (source, destination)
			copies.append((source, destination))
		mapping[original_source] = destination
	return copies, mapping


def execute_copy_plan(copies):
	u"""Stage complete copies, publish without overwrite, roll back owned files."""
	attempts = []
	try:
		for source, destination in copies:
			if not os.path.isfile(source):
				raise IOError('texture source disappeared: {0}'.format(source))
			if os.path.lexists(destination):
				raise TextureCopyError(
					'texture destination appeared during copy: {0}'.format(destination))
			with StagedFile(destination, mode='wb') as staged:
				ownership = _CopyOwnership(destination, staged.identity)
				# Reserve the ledger entry before the externally visible publish.
				attempts.append(ownership)
				with open(source, 'rb') as input_stream:
					shutil.copyfileobj(input_stream, staged.stream)
				staged.seal()
				shutil.copystat(source, staged.temporary_name)
				staged.publish(destination, replace_existing=False)
				ownership.published = True
		return [ownership.destination for ownership in attempts]
	except BaseException:
		for ownership in reversed(attempts):
			if ownership.published:
				try:
					remove_owned_file(ownership.destination, ownership.identity)
				except OSError:
					# Try every owned file while preserving the original failure.
					pass
		raise
