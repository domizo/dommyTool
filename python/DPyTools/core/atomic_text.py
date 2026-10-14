# -*- coding: utf-8 -*-
"""Atomic text output for legacy writers that stream their content."""

import os

from DPyTools.core.atomic_file import StagedFile


class AtomicTextWriter(object):
	"""Replace after commit; default text mode, optional binary codec output."""

	def __init__(self, filename, mode='w'):
		self._staged = None
		self.filename = os.path.abspath(filename)
		self._staged = StagedFile(self.filename, mode=mode)

	@property
	def stream(self):
		return None if self._staged is None else self._staged.stream

	@stream.setter
	def stream(self, value):
		if self._staged is None:
			raise RuntimeError('Atomic text writer is already closed')
		self._staged.stream = value

	@property
	def temporary_name(self):
		return None if self._staged is None else self._staged.temporary_name

	def write(self, value):
		return self.stream.write(value)

	def __enter__(self):
		return self

	def __exit__(self, exception_type, exception_value, traceback):
		if exception_type is not None:
			self._abort_after_failure()
			return False
		try:
			self.commit()
		except BaseException:
			self._abort_after_failure()
			raise
		return False

	def commit(self):
		if self.stream is None:
			raise RuntimeError('Atomic text writer is already closed')
		self._staged.seal()
		self._staged.preserve_destination_mode(self.filename)
		self._staged.publish(self.filename)
		self._staged = None
		return self.filename

	def abort(self):
		if self._staged is not None:
			self._staged.discard()
			self._staged = None

	def _abort_after_failure(self):
		try:
			self.abort()
		except Exception:
			pass

	def __del__(self):
		self._abort_after_failure()


def write_text(filename, value):
	"""Atomically replace ``filename`` with a complete text value."""
	with AtomicTextWriter(filename) as writer:
		writer.write(value)
	return os.path.abspath(filename)
