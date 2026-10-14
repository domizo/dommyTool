# -*- coding: utf-8 -*-
"""Temporary model-editor changes with complete snapshots and restoration."""

import sys
from contextlib import contextmanager

import pymel.core as pm

from DPyTools.core import strings
from DPyTools.scene import attribute_values


MAX_VISIBLE_PANELS = 128
OVERLAY_FLAGS = ('selectionHiliteDisplay', 'headsUpDisplay')


def _visible_state():
	panels = pm.getPanel(visiblePanels=True) or []
	if not isinstance(panels, (list, tuple)) or len(panels) > MAX_VISIBLE_PANELS:
		raise ValueError('Visible panel list exceeds the supported limit')
	states, seen = [], set()
	for panel in panels:
		if pm.getPanel(typeOf=panel) != 'modelPanel':
			continue
		editor = pm.modelPanel(panel, query=True, modelEditor=True)
		if not isinstance(editor, strings.STRING_TYPES) or not editor:
			raise ValueError('Model panel did not return an editor name')
		if editor in seen:
			continue
		values = dict((flag, attribute_values.numeric_value(
			pm.modelEditor(editor, query=True, **{flag: True}), 'bool')) for flag in OVERLAY_FLAGS)
		states.append((editor, values))
		seen.add(editor)
	return tuple(states)


def _restore(states, active_error):
	errors = []
	for editor, values in reversed(states):
		# One failed flag must not prevent the other flag or other editors.
		for flag in OVERLAY_FLAGS:
			try:
				pm.modelEditor(editor, edit=True, **{flag: values[flag]})
			except (RuntimeError, TypeError, ValueError) as error:
				errors.append(error)
	if errors:
		message = 'Viewport setting restoration was incomplete: {0}'.format(errors[0])
		if not active_error:
			raise RuntimeError(message)
		try:
			pm.warning(message)
		except (RuntimeError, TypeError, ValueError):
			pass


@contextmanager
def hidden_overlays():
	"""Restore exact prior flags after success, failure or interruption."""
	states = _visible_state()
	attempted = []
	try:
		for editor, values in states:
			# Record before dispatch: the command may change state then throw.
			attempted.append((editor, values))
			pm.modelEditor(editor, edit=True, selectionHiliteDisplay=False, headsUpDisplay=False)
		yield
	finally:
		exception_type = sys.exc_info()[0]
		_restore(attempted, active_error=exception_type is not None)
