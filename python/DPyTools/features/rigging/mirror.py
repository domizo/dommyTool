# -*- coding: utf-8 -*-
"""Preflighted joint mirroring without substring-based native renaming."""

import pymel.core as pm

from DPyTools.core import strings
from DPyTools.core.geometry import position3


def _joint_tree(root):
	"""Retain joint identities and direct-child order in a breadth-first tree."""
	nodes = [pm.PyNode(root)]
	seen = set(nodes)
	child_counts = []
	index = 0
	while index < len(nodes):
		node = nodes[index]
		if node.nodeType() != 'joint' or node.isInstanced():
			raise ValueError('Mirroring requires a non-instanced joint hierarchy')
		children = [pm.PyNode(child) for child in
			(pm.listRelatives(node, children=True, type='joint') or [])]
		child_counts.append(len(children))
		for child in children:
			if child in seen:
				raise ValueError('Joint hierarchy contains duplicate identities')
			seen.add(child)
			nodes.append(child)
		index += 1
	return nodes, tuple(child_counts)


def _mirrored_tree(result, expected_counts):
	"""Validate the returned complete tree instead of trusting return order."""
	created = [pm.PyNode(node) for node in (result or [])]
	created_set = set(created)
	if len(created_set) != len(created) or len(created) != len(expected_counts):
		raise ValueError('Mirrored joint result has an unexpected count')
	roots = [node for node in created if not any(parent in created_set for parent in
		(pm.listRelatives(node, parent=True) or []))]
	if len(roots) != 1:
		raise ValueError('Mirrored joint result must contain one complete root')
	nodes, child_counts = _joint_tree(roots[0])
	if set(nodes) != created_set or child_counts != expected_counts:
		raise ValueError('Mirrored joint topology does not match the source tree')
	return nodes


def mirror_joints(joints, mirror_behavior=False, rename_mirrored=True,
		rename_sources=True):
	"""Mirror admitted roots; completed native edits are not auto-rolled back."""
	plans = []
	seen_sources = set()
	seen_targets = set()
	for value in joints:
		root = pm.PyNode(value)
		nodes, child_counts = _joint_tree(root)
		if seen_sources.intersection(nodes):
			raise ValueError('Selected joint roots overlap or repeat')
		seen_sources.update(nodes)
		x = position3(pm.xform(root, q=True, ws=True, t=True))[0]
		if x == 0.0:
			continue
		root_name = strings.leaf_name(root.name())
		root_side = strings.current_side(root_name)
		targets = []
		if rename_mirrored:
			fallback_side = '_r' if x > 0 else '_l'
			for node in nodes:
				name = strings.leaf_name(node.name())
				side = strings.reverse_side(strings.current_side(name)) or fallback_side
				target = ':' + strings.set_side(name, side).lstrip(':')
				if target in seen_targets or pm.objExists(target):
					raise ValueError('Mirrored joint name already exists: {0}'.format(target))
				seen_targets.add(target)
				targets.append(target)
		source_target = None
		if rename_sources and root_side is None:
			source_target = ':' + strings.set_side(
				root_name, '_l' if x > 0 else '_r').lstrip(':')
			if source_target in seen_targets or pm.objExists(source_target):
				raise ValueError('Source joint name already exists: {0}'.format(source_target))
			seen_targets.add(source_target)
		plans.append((root, child_counts, targets, source_target))
	completed = 0
	try:
		for root, child_counts, targets, source_target in plans:
			result = pm.mirrorJoint(root, mirrorYZ=True, mirrorBehavior=bool(mirror_behavior))
			mirrored = _mirrored_tree(result, child_counts)
			for node, target in zip(mirrored, targets):
				# Recheck after creation/previous edits rather than accepting auto-numbering.
				if pm.objExists(target):
					raise ValueError('Mirrored target changed during creation: {0}'.format(target))
				pm.rename(node, target)
			if source_target is not None:
				if pm.objExists(source_target):
					raise ValueError('Source target changed during creation: {0}'.format(source_target))
				pm.rename(root, source_target)
			completed += 1
	except (RuntimeError, TypeError, ValueError) as error:
		raise RuntimeError(('Mirroring stopped after {0} completed roots: {1}. '
			'Created joints and completed edits may remain in the scene.').format(
			completed, error))
	return completed
