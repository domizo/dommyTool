# -*- coding: utf-8 -*-
"""Maya scene rename operations and historical string aliases."""

import pymel.core as pm

from DPyTools.core import strings as string_utils
from DPyTools.core.geometry import position3


# Historical mutable names remain available to archived scripts.
side_list_withC = list(string_utils.SIDES_WITH_CENTER)
side_list = list(string_utils.SIDES)
side_rep_dic = dict(string_utils.REVERSE_SIDES)

# Historical pure helper names retain their original keyword signatures.
def isContain(base='', target=''):
	return string_utils.contains(base, target)


def isContainList(base='', target=None):
	return string_utils.contains_any(base, target)


def isContainLR(base='', isCenter=0):
	return string_utils.contains_side(base, include_center=bool(isCenter))


def replaceText(src='', dstDic=None):
	return string_utils.replace_text(src, dstDic)


def replaceTextToWS(src='', dst=None):
	return string_utils.remove_text(src, dst)


def filterTextInItems(items=None, filterText=''):
	return string_utils.filter_items(items, filterText)


def subStringForSide(src=''):
	return string_utils.strip_side(src)


def getCurrentSide(src=None):
	return string_utils.current_side(src)


def getRevSide(side=''):
	return string_utils.reverse_side(side)


def numberPadding(count=1, size=3):
	return string_utils.pad_number(count, size)


def filterTextInList(list=None, filterText=''):
	"""Compatibility wrapper retaining the historical keyword ``list``."""
	return string_utils.filter_items(items=list, filter_text=filterText)


def rename_reverse_side(source=None, side=''):
	"""Rename a Maya object with the opposite historical side suffix."""
	reversed_side = string_utils.reverse_side(side)
	if reversed_side:
		node = pm.PyNode(source)
		target = string_utils.set_side(string_utils.leaf_name(node.name()), reversed_side)
		return pm.rename(node, ':' + target.lstrip(':'))
	return None


def rename_by_position(source=None, destination=None, include_center=None):
	"""Rename a Maya object from the sign of its world-space X position."""
	node = pm.PyNode(source)
	source_name = string_utils.leaf_name(node.name())
	base_name = string_utils.leaf_name(destination) if destination else source_name
	if ':' not in base_name and ':' in source_name:
		base_name = source_name.rsplit(':', 1)[0] + ':' + base_name
	# Admit ambiguous names before querying or mutating the scene.
	string_utils.current_side(base_name)
	position = position3(pm.xform(node, query=True, worldSpace=True, translation=True))
	side = None
	if position[0] == 0:
		if include_center:
			side = '_c'
	elif position[0] > 0.0:
		side = '_l'
	else:
		side = '_r'
	if side is not None:
		target = string_utils.set_side(base_name, side)
		return pm.rename(node, ':' + target.lstrip(':'))
	return None


def renameRevSide(src=None, side=''):
	"""Compatibility wrapper for ``rename_reverse_side``."""
	rename_reverse_side(source=src, side=side)


def renameLR(src=None, dst=None, isCenter=None):
	"""Compatibility wrapper for ``rename_by_position``."""
	rename_by_position(
		source=src, destination=dst, include_center=isCenter)


__all__ = (
	'side_list_withC', 'side_list', 'side_rep_dic',
	'isContain', 'isContainList', 'isContainLR', 'replaceText',
	'replaceTextToWS', 'filterTextInItems', 'filterTextInList',
	'subStringForSide', 'getCurrentSide', 'getRevSide',
	'renameRevSide', 'renameLR', 'numberPadding',
	'rename_reverse_side', 'rename_by_position',
)
