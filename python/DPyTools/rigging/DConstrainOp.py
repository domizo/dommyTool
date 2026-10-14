# -*- coding: utf-8 -*-
"""Historical edge/face rivet entry points."""

from DPyTools.features.rigging import rivets

__all__ = ['createEdgeRivet', 'createFaceRivet']


def createEdgeRivet():
	return rivets.createEdgeRivet()


def createFaceRivet():
	return rivets.createFaceRivet()
