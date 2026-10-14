# -*- coding: utf-8 -*-
u"""Maya-independent validation for complete node rename preflight plans."""

from collections import Counter, namedtuple

from DPyTools.core.strings import leaf_name, remove_text, replace_text
from DPyTools.ui.table_selection import selected_rows


TEXT_TYPE = type(u'')
STRING_TYPES = (str,) if str is TEXT_TYPE else (str, TEXT_TYPE)
MAX_RENAME_ITEMS = 10000
MAX_NODE_NAME_LENGTH = 255
Preview = namedtuple('Preview', 'sources targets selected')


def _text(value, field_name, allow_hierarchy=False):
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be a string'.format(field_name))
	if not value:
		raise ValueError('{0} must not be empty'.format(field_name))
	if len(value) > MAX_NODE_NAME_LENGTH and not allow_hierarchy:
		raise ValueError('{0} is too long'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise ValueError('{0} contains a control character'.format(field_name))
	if not allow_hierarchy:
		if value != value.strip():
			raise ValueError('{0} must not start or end with whitespace'.format(field_name))
		if '|' in value:
			raise ValueError('{0} must be a short Maya node name'.format(field_name))
	return value


def short_name(source_name):
	u"""Return the final DAG component of a validated source name."""
	source_name = _text(source_name, 'source name', allow_hierarchy=True)
	return leaf_name(source_name)


def build_rename_plan(source_names, target_names, max_items=MAX_RENAME_ITEMS):
	u"""Validate two complete name lists and return immutable rename pairs."""
	if not isinstance(source_names, (list, tuple)):
		raise TypeError('source names must be a list or tuple')
	if not isinstance(target_names, (list, tuple)):
		raise TypeError('target names must be a list or tuple')
	if len(source_names) != len(target_names):
		raise ValueError('source and target name counts must match')
	if not source_names:
		raise ValueError('rename plan must not be empty')
	if len(source_names) > max_items:
		raise ValueError('rename plan contains too many items')

	plan = []
	seen_sources = set()
	seen_targets = set()
	for index, (source_name, target_name) in enumerate(
			zip(source_names, target_names)):
		source_name = _text(
			source_name, 'source name at index {0}'.format(index),
			allow_hierarchy=True)
		target_name = _text(
			target_name, 'target name at index {0}'.format(index))
		if source_name in seen_sources:
			raise ValueError('duplicate rename source: {0}'.format(source_name))
		if target_name in seen_targets:
			raise ValueError('duplicate rename target: {0}'.format(target_name))
		seen_sources.add(source_name)
		seen_targets.add(target_name)
		plan.append((source_name, target_name))
	return tuple(plan)


def _preview_names(values, field_name, allow_hierarchy=False):
	values = () if values is None else values
	if not isinstance(values, (list, tuple)) or len(values) > MAX_RENAME_ITEMS:
		raise ValueError('{0} must be a bounded name sequence'.format(field_name))
	return tuple(_text(value, field_name, allow_hierarchy) for value in values)


def preview(sources, targets, selected=None):
	"""Snapshot paired text lists; duplicate targets may be repaired in preview."""
	sources = _preview_names(sources, 'preview source', allow_hierarchy=True)
	targets = _preview_names(targets, 'preview target')
	if len(sources) != len(targets):
		raise ValueError('Preview source and target counts must match')
	rows = selected_rows(selected, len(targets), multiple=True)
	return Preview(sources, targets, rows)


def _validated_preview(model):
	if not isinstance(model, Preview):
		raise TypeError('Rename preview must be an admitted snapshot')
	return preview(model.sources, model.targets, model.selected)


def _fragment(value, field_name, allow_empty=False):
	if not isinstance(value, STRING_TYPES):
		raise TypeError('{0} must be text'.format(field_name))
	if (not allow_empty and not value) or len(value) > MAX_NODE_NAME_LENGTH:
		raise ValueError('{0} must be bounded nonempty text'.format(field_name))
	if any(ord(character) < 32 or ord(character) == 127 for character in value):
		raise ValueError('{0} contains a control character'.format(field_name))
	return value


def _scope(model, all_if_none=False):
	if not isinstance(all_if_none, bool):
		raise TypeError('Preview all-rows option must be Boolean')
	if all_if_none and not model.selected:
		return tuple(range(1, len(model.targets) + 1))
	return model.selected


def _mapped_preview(model, transform, all_if_none=False):
	targets = list(model.targets)
	for row in _scope(model, all_if_none):
		targets[row - 1] = transform(targets[row - 1])
	return preview(model.sources, targets, model.selected)


def prefix_preview(model, prefix):
	model = _validated_preview(model)
	prefix = _fragment(prefix, 'prefix')
	if '_' not in prefix:
		prefix += '_'
	return _mapped_preview(model, lambda value: prefix + value, all_if_none=True)


def replace_preview(model, source, replacement):
	model = _validated_preview(model)
	source = _fragment(source, 'replacement source')
	replacement = _fragment(replacement, 'replacement text', allow_empty=True)
	return _mapped_preview(model, lambda value: replace_text(value, {source: replacement}), all_if_none=True)


def numbered_preview(model, basename):
	"""Allocate forward-only numbers against remaining and already edited rows."""
	model = _validated_preview(model)
	basename = _fragment(basename, 'numbered name')
	targets = list(model.targets)
	occupied = Counter(targets)
	number = 1
	for row in model.selected:
		old = targets[row - 1]
		occupied[old] -= 1
		if not occupied[old]:
			del occupied[old]
		candidate = _text('{0}{1}'.format(basename, number), 'numbered target')
		while candidate in occupied:
			number += 1
			candidate = _text('{0}{1}'.format(basename, number), 'numbered target')
		targets[row - 1] = candidate
		occupied[candidate] += 1
		number += 1
	return preview(model.sources, targets, model.selected)


def tag_preview(model, tag, switches=None):
	model = _validated_preview(model)
	tag = _fragment(tag, 'tag')
	switches = () if switches is None else switches
	if not isinstance(switches, (list, tuple)) or len(switches) > MAX_RENAME_ITEMS:
		raise ValueError('Tag switches must be a bounded sequence')
	switches = tuple(_fragment(value, 'tag switch') for value in switches)
	def toggle(value):
		for old in switches:
			if old in value:
				value = remove_text(value, (old,))
				break
		return remove_text(value, (tag,)) if tag in value else value + tag
	return _mapped_preview(model, toggle)


def custom_tag_preview(model, tag):
	tag = _fragment(tag, 'custom tag')
	tag = remove_text(tag, ('_',))
	if not tag:
		raise ValueError('Custom tag must contain more than underscores')
	return tag_preview(model, '_' + tag)


def manual_preview(model, target):
	model = _validated_preview(model)
	target = _text(target, 'manual target')
	targets = list(model.targets)
	if model.selected:
		targets[model.selected[0] - 1] = target
	return preview(model.sources, targets, model.selected)


def delete_preview_rows(model):
	model = _validated_preview(model)
	removed = set(model.selected)
	sources = tuple(value for row, value in enumerate(model.sources, 1) if row not in removed)
	targets = tuple(value for row, value in enumerate(model.targets, 1) if row not in removed)
	return preview(sources, targets)
