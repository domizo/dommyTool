# -*- coding: utf-8 -*-
u"""Compatibility exports for historical string and rename helpers."""

from DPyTools.scene import naming as _implementation


__all__ = (
	'side_list_withC', 'side_list', 'side_rep_dic',
	'isContain', 'isContainList', 'isContainLR', 'replaceText',
	'replaceTextToWS', 'filterTextInItems', 'filterTextInList',
	'subStringForSide', 'getCurrentSide', 'getRevSide',
	'renameRevSide', 'renameLR', 'numberPadding',
	'rename_reverse_side', 'rename_by_position',
)

for _name in __all__:
	globals()[_name] = getattr(_implementation, _name)
del _name
