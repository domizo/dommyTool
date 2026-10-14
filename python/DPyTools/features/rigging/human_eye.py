# -*- coding: utf-8 -*-
"""Owned historical eye graph with confirmed native polygon-history returns."""

import sys

import pymel.core as pm

from DPyTools.scene import ownership


def _history(owned, command, kind, shape, **kwargs):
	previous = set(pm.ls(type=kind) or [])
	result = command(shape, **kwargs)
	return owned.adopt_returned(result, previous, (kind,))[0]


def createHumanEye():
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		generator = owned.create('DHEyeGen')
		group = owned.create('transform', name='eye')
		lens_parent = owned.create('transform', name='lenz', parent=group)
		iris_parent = owned.create('transform', name='iris', parent=group)
		lens = owned.create('mesh', name='lenzShape', parent=lens_parent)
		iris = owned.create('mesh', name='irisShape', parent=iris_parent)
		pm.connectAttr(generator.attr('outLenzMesh'), lens.attr('inMesh'))
		pm.connectAttr(generator.attr('outIrisMesh'), iris.attr('inMesh'))
		history = (
			_history(owned, pm.polyMergeVertex, 'polyMergeVert', lens, d=0.001, am=True, ch=True),
			_history(owned, pm.polySoftEdge, 'polySoftEdge', lens, a=180, ch=True),
			_history(owned, pm.polySoftEdge, 'polySoftEdge', iris, a=180, ch=True))
		pm.sets('initialShadingGroup', edit=True, forceElement=lens)
		pm.sets('initialShadingGroup', edit=True, forceElement=iris)
		completed = True
		return group, generator, (lens, iris), history
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Human eye was not created; unconfirmed native side effects may remain: {0}'.format(error))
		return False
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=failed or exception_type is not None)
