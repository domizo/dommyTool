# -*- coding: utf-8 -*-
"""Complete selection/value admission for Outliner display-color edits."""

import pymel.core as pm

from DPyTools.core import geometry
from DPyTools.scene import parameters


def _targets(nodes):
	if nodes is None:
		nodes = pm.ls(selection=True) or []
	return parameters.resolve_nodes(nodes, allow_empty=True, require_same_type=False)


def set_color_enabled(nodes=None, enabled=True):
	if not isinstance(enabled, bool):
		raise TypeError('Outliner color enablement must be Boolean')
	return parameters.set_values_many(_targets(nodes), {'useOutlinerColor': enabled})


def set_color(nodes=None, color=(1, 1, 1)):
	color = geometry.position3(color)
	if any(not 0.0 <= component <= 1.0 for component in color):
		raise ValueError('Outliner color components must be within [0, 1]')
	return parameters.set_values_many(_targets(nodes), {'outlinerColor': color})
