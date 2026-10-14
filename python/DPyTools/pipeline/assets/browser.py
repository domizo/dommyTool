# -*- coding: utf-8 -*-
"""Shared admission and guarded updates for historical asset browser widgets."""

import functools
from contextlib import contextmanager

import pymel.core as pm

from DPyTools.core import strings
from DPyTools.pipeline.assets import paths
from DPyTools.ui import table_selection


def action(function):
	"""Report expected UI failures; suppress same-owner update re-entry only."""
	@functools.wraps(function)
	def wrapped(self, *args, **kwargs):
		if getattr(self, '_updating_browser', False):
			return False
		try:
			return function(self, *args, **kwargs)
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
			pm.warning('Asset browser update was incomplete; settings/UI may already have changed: {0}'.format(error))
			return False
	return wrapped


def selected_name(control):
	value = table_selection.selected_text(pm.textScrollList(control, query=True, selectItem=True))
	return paths.validate_component(value, 'browser selection') if value else ''


def project_path(owner):
	"""Read an owning view's context; None preserves standalone legacy defaults."""
	getter = getattr(owner, 'project_path_getter', None)
	if getter is None:
		return None
	if not callable(getter):
		raise TypeError('Project context provider must be callable')
	return table_selection.cell_text(getter())


def list_plan(selected, items, filter_text=''):
	items = table_selection.text_items(items)
	for item in items:
		paths.validate_component(item, 'browser item')
	selected = table_selection.selected_text(selected)
	if selected:
		paths.validate_component(selected, 'browser selection')
	filter_text = table_selection.cell_text(filter_text)
	return table_selection.list_selection(selected, strings.filter_items(items, filter_text))


@contextmanager
def updating(owner):
	previous = getattr(owner, '_updating_browser', False)
	owner._updating_browser = True
	try:
		yield
	finally:
		owner._updating_browser = previous


def replace_items(owner, control, items, selected=''):
	items, selected = list_plan(selected, items)
	with updating(owner):
		pm.textScrollList(control, edit=True, removeAll=True)
		if items:
			pm.textScrollList(control, edit=True, append=list(items))
		if selected:
			pm.textScrollList(control, edit=True, selectItem=selected)
	return selected


def notify(callback):
	if callback is None:
		return True
	if not callable(callback):
		raise TypeError('Browser delegate must be callable')
	result = callback()
	return not (isinstance(result, bool) and not result)
