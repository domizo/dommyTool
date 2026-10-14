# -*- coding: utf-8 -*-
u"""Small shared lifecycle helpers for Maya windows."""

import pymel.core as pm


def delete_window(name):
	u"""Delete an existing Maya window once and report whether it existed."""
	if not name:
		raise ValueError('window name must not be empty')
	if not pm.window(name, exists=True):
		return False
	try:
		pm.deleteUI(name)
	except RuntimeError:
		# A host callback may already have removed the window. Only suppress that
		# race; surface an actual deletion failure while the window still exists.
		if pm.window(name, exists=True):
			raise
		return False
	return True
