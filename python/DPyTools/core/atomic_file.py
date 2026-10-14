# -*- coding: utf-8 -*-
u"""Owned sibling-file staging for trusted, stable output directories."""

import errno
import os
import stat
import tempfile

from DPyTools.core import file_publish


def validate_destination(filename, error_type=IOError):
	u"""Admit absent/regular output leaves in a trusted stable namespace."""
	try:
		info = os.lstat(filename)
	except OSError as error:
		if error.errno == errno.ENOENT:
			return filename
		raise
	if not stat.S_ISREG(info.st_mode):
		raise error_type('Output destination must be absent or a regular non-symlink file: {0}'.format(filename))
	return filename


def _file_identity(info):
	return info.st_dev, info.st_ino


def file_identity(info):
	"""Share device/inode identity hints; these are not authentication."""
	return _file_identity(info)


def _matches_owned_file(filename, identity):
	try:
		info = os.lstat(filename)
	except OSError as error:
		if error.errno == errno.ENOENT:
			return False
		raise
	return stat.S_ISREG(info.st_mode) and (
		identity is None or _file_identity(info) == identity)


def remove_owned_file(filename, identity, make_writable=True):
	u"""Remove only a matching regular file, never a substituted symlink.

	Identity is not authentication. Windows Python 2 identifiers and hostile
	namespace changes require native validation; directories must be trusted.
	"""
	if not isinstance(make_writable, bool):
		raise TypeError('Writable cleanup option must be Boolean')
	if not _matches_owned_file(filename, identity):
		return False
	if os.name == 'nt' and make_writable:
		# copystat may have retained a source's read-only attribute.
		os.chmod(filename, stat.S_IWRITE)
	os.unlink(filename)
	return True


class StagedFile(object):
	u"""Own an open descriptor/stream and its temporary path until publication."""

	def __init__(self, filename, mode='w'):
		self.stream = None
		self.temporary_name = None
		self.identity = None
		self._descriptor = None
		if mode not in ('w', 'wb'):
			raise ValueError('Staged file mode must be w or wb')
		filename = os.path.abspath(filename)
		validate_destination(filename)
		directory = os.path.dirname(filename)
		if not os.path.isdir(directory):
			raise IOError('Output directory does not exist: {0}'.format(directory))
		try:
			# The final leaf may already fill NAME_MAX; do not extend it for staging.
			self._descriptor, self.temporary_name = tempfile.mkstemp(
				prefix='.dommy-',
				suffix='.tmp', dir=directory)
			self.identity = _file_identity(os.fstat(self._descriptor))
			self.stream = os.fdopen(self._descriptor, mode)
			self._descriptor = None
		except BaseException:
			self.discard_after_failure()
			raise

	def __enter__(self):
		return self

	def __exit__(self, exception_type, exception_value, traceback):
		if exception_type is not None:
			self.discard_after_failure()
		else:
			self.discard()
		return False

	def seal(self):
		if self.stream is None:
			raise RuntimeError('Staged file is already closed')
		self.stream.flush()
		os.fsync(self.stream.fileno())
		self.stream.close()
		self.stream = None

	def publish(self, destination, replace_existing=True):
		if self.stream is not None or self._descriptor is not None:
			raise RuntimeError('Staged file must be sealed before publication')
		self.verify_owned()
		validate_destination(destination)
		if replace_existing:
			file_publish.replace_file(self.temporary_name, destination)
			self.temporary_name = None
		else:
			file_publish.publish_file_no_replace(self.temporary_name, destination)

	def verify_owned(self):
		"""Refuse a missing/substituted staging path in a trusted namespace."""
		if self.temporary_name is None or not _matches_owned_file(
				self.temporary_name, self.identity):
			raise IOError('Staged file is missing or has been replaced')

	def preserve_destination_mode(self, destination):
		"""Keep an existing regular destination's mode in a trusted namespace."""
		self.verify_owned()
		if os.path.isfile(destination) and not os.path.islink(destination):
			mode = stat.S_IMODE(os.stat(destination).st_mode)
			os.chmod(self.temporary_name, mode)

	def discard(self):
		first_error = None
		if self.stream is not None:
			try:
				self.stream.close()
				self.stream = None
			except Exception as error:
				first_error = error
		if self._descriptor is not None:
			descriptor = self._descriptor
			# A failed raw close may already have released/reused the number.
			# Never retry that ambiguous descriptor from later cleanup/destruction.
			self._descriptor = None
			try:
				os.close(descriptor)
			except OSError as error:
				if first_error is None:
					first_error = error
		if self.temporary_name is not None:
			try:
				remove_owned_file(self.temporary_name, self.identity)
				self.temporary_name = None
			except OSError as error:
				if first_error is None:
					first_error = error
		if first_error is not None:
			raise first_error

	def discard_after_failure(self):
		# A cleanup error must not replace an active copy/write/init exception.
		# Failed streams/paths may be retried; ambiguous raw close is one-shot.
		try:
			self.discard()
		except Exception:
			pass

	def __del__(self):
		self.discard_after_failure()
