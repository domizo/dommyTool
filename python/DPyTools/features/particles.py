# -*- coding: utf-8 -*-
"""Checked DPEmitter creation; native shared solvers are never adopted."""

import sys

import pymel.core as pm

from DPyTools.scene import ownership, parameters, selection


def createDPEmitter(nParticleMode=False):
	if not isinstance(nParticleMode, bool):
		raise TypeError('Particle mode must be Boolean')
	selected = pm.ls(selection=True) or []
	kind = 'nParticle' if nParticleMode else 'particle'
	previous = set(pm.ls(type=('transform', kind)) or [])
	owned = ownership.SceneNodes()
	completed = False
	failed = False
	try:
		emitter = owned.create('DPEmitter')
		parameters.set_values(emitter, {'rate': 1})
		command = pm.nParticle if nParticleMode else pm.particle
		# Preserve the historical native empty-system creation call.
		result = command()
		nodes = owned.adopt_returned(result, previous, ('transform', kind))
		transform = next(node for node in nodes if node.nodeType() == 'transform')
		shape = next(node for node in nodes if node.nodeType() == kind)
		if shape.getParent() != transform:
			raise ValueError('Particle result does not contain its confirmed owning transform')
		pm.connectDynamic(shape, em=emitter)
		pm.addDynamic(emitter, shape)
		completed = True
		return emitter, transform, shape
	except (RuntimeError, TypeError, ValueError) as error:
		failed = True
		pm.warning('Emitter was not created; unconfirmed native side effects may remain: {0}'.format(error))
		return False
	finally:
		try:
			if not completed:
				exception_type = sys.exc_info()[0]
				owned.cleanup(active_error=failed or exception_type is not None)
		finally:
			selection.restore_selection(selected)
