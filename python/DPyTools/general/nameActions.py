# -*- coding: utf-8 -*-
from collections import Counter as _Counter

import pymel.core as pm

'''
to run:
	>>> import DPyTools.general.nameActions as DPName


'''



def objectNameAppendPrefix(objects=None, prefix=''):
	u"""objects内のオブジェクトにprefixの文字列を追加します。"""
	if objects is None:
		objects = []
	for obj in objects:
		pm.rename(obj, prefix + str(obj))


def checkUniqueName(isRename=None):
	u"""ユニークな名前でないノードを抽出します。 isRenameがTrueの時、リネームを実行"""

	objs = pm.ls(shapes=0)
	short_names = [str(obj).rpartition('|')[2] for obj in objs]
	if not isRename:
		# Preserve the historical repeated names/order without quadratic scans.
		counts = _Counter(short_names)
		return [name for name in short_names if counts[name] > 1]

	# Keep the legacy mutating path until its Maya behavior can be verified.
	not_unique_list = []
	for i in range(len(short_names)):
		startIndex = 0
		copyCount = short_names.count(short_names[i])
		if copyCount > 1:
			dup = short_names[i]
			not_unique_list.append(dup)
			#print dup

			#if needs, rename object
			if isRename :
				for j in range(copyCount):
					index = short_names.index(dup, startIndex)
					short_names[index] = pm.rename(
						objs[index], dup + '_' + str(j))
					startIndex = index+1

	return not_unique_list
