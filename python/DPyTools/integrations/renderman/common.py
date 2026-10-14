# -*- coding: utf-8 -*-
"""Small RenderMan shading-network helpers."""

import pymel.core as pm


def CreateSimplePxrVDM(ptexMode=True):
	displacement = pm.shadingNode('PxrDisplace', asShader=True)
	transform = pm.shadingNode('PxrDispTransform', asTexture=True)
	texture_type = 'PxrPtexture' if ptexMode else 'PxrTexture'
	texture = pm.shadingNode(texture_type, asTexture=True)

	pm.connectAttr(
		transform + '.resultXYZ', displacement + '.dispVector', force=True)
	pm.connectAttr(texture + '.resultRGB', transform + '.dispVector', force=True)
	return displacement, transform, texture
