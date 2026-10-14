# -*- coding: utf-8 -*-
import pymel.core as pm
from DPyTools.core import components


def restore_selection(selected):
	"""Best-effort restoration of retained identities, without masking errors."""
	try:
		existing = [item for item in selected if pm.objExists(item)]
		if existing:
			pm.select(existing, replace=True)
		else:
			pm.select(clear=True)
		return True
	except (RuntimeError, TypeError, ValueError) as error:
		try:
			pm.warning('Could not fully restore the previous selection: {0}'.format(error))
		except (RuntimeError, TypeError, ValueError):
			pass
		return False



'''
to run:
	>>> import DPyTools.scene.selection as selOp
	>>>

'''


def isSelectItem():
	u"""選択していたらboolとオブジェクトのリストを返します"""
	objects = pm.ls(sl=True) or []
	return bool(objects), objects



def nodeFilter(items=None, nodetype='') :
	u"""items内のオブジェクトを指定したNodeタイプのみにフィルターして返します。"""
	if items is None:
		items = []

	tmp_list = []

	for item in items :
		if item.type() == nodetype:
			tmp_list.append(item)

	return tmp_list



def getSelectionWithFilterType(nodetype='', selected=1):
	u"""選択されてるオブジェクトの指定したNodeタイプでフィルターしたリストを返します。"""

	if len(nodetype) :
		return pm.ls(sl=selected, type=nodetype)
	else :
		return pm.ls(sl=selected)


def getJointlist(selected=1):
	u"""選択されてるジョイントのリストを返します。"""
	return getSelectionWithFilterType(nodetype='joint', selected=selected)


def getGeometryList(node_type=''):
	u"""選択されてるオブジェクト内の指定したジオメトリのタイプのリストを返します。"""
	return pm.listRelatives(shapes=True, type=node_type) or []


def getGeometryeList(type=''):
	u"""Compatibility wrapper for the historical misspelled function name."""
	return getGeometryList(node_type=type)


def getMeshList():
	u"""選択されてるメッシュシェイプのリストを返します。"""
	return getGeometryList(node_type='mesh')


def resolve_shape(value, shape_types=('mesh',)):
	u"""Resolve exactly one non-intermediate, non-instanced supported shape."""
	node = pm.PyNode(value)
	if node.nodeType() == 'transform':
		shapes = [shape for shape in (node.getShapes(noIntermediate=True) or [])
			if shape.nodeType() in shape_types]
		if len(shapes) != 1:
			raise ValueError('Input must own exactly one supported geometry shape')
		node = shapes[0]
	if node.nodeType() not in shape_types or node.getAttr('intermediateObject'):
		raise ValueError('Input must be a supported non-intermediate geometry shape')
	if node.isInstanced():
		raise ValueError('Instanced geometry requires an explicit instance-aware tool')
	return node


def getNurbsSurfaceList():
	u"""選択されてるナーブスサーフェイスシェイプのリストを返します。"""
	return getGeometryList(node_type='nurbsSurface')


def getNurbsCurveList():
	u"""選択されてるナーブスカーブシェイプのリストを返します。"""
	return getGeometryList(node_type='nurbsCurve')


def getObjectInHierarchy(obj = None):
	u"""Get transform node　選択したオブジェクト下にあるトランスフォームノードを取得します。"""

	if obj is None :
		return []

	outputList = pm.listRelatives(
		obj, children=True, allDescendents=True, type='transform') or []
	outputList.insert(0, obj)

	return outputList


def getNodeFromHierarchy(obj=None, typeStr=''):
	u"""Get transform node　選択したオブジェクト下にある特定のノードを取得します。"""

	if obj is None:
		return []

	outputList = []

	if len(typeStr)>0 :

		childList = pm.listRelatives(obj, children=True, allDescendents=True) or []

		for c in childList :
			if c.nodeType() == typeStr:
				outputList.append(c)

		if obj.nodeType() == typeStr:
			outputList.insert(0, obj)

	return outputList


def mesh_component_plan(selection, kind='edge', target_mesh=None, limit=None):
	"""Admit the complete flattened edge/face selection on one mesh."""
	if limit is None:
		limit = components.MAX_COMPONENT_COUNT
	limit = components.component_index(limit)
	if not 1 <= limit <= components.MAX_COMPONENT_COUNT or kind not in ('edge', 'face'):
		raise ValueError('Invalid mesh component kind/count limit')
	if not isinstance(selection, (list, tuple)) or len(selection) > limit:
		raise ValueError('Mesh component input exceeds the supported limit')
	if not selection:
		return []
	expected = pm.general.MeshEdge if kind == 'edge' else pm.general.MeshFace
	owner = resolve_shape(target_mesh) if target_mesh is not None else None
	count = None
	result = []
	seen = set()
	for item in selection:
		if not isinstance(item, expected):
			raise ValueError('Select mesh {0} components only'.format(kind))
		mesh = resolve_shape(item.node())
		if owner is None:
			owner = mesh
		if mesh != owner:
			raise ValueError('Selected components must belong to one target mesh')
		if count is None:
			count = mesh.numEdges() if kind == 'edge' else mesh.numFaces()
		index = components.component_index(item.index())
		if index >= count or index in seen:
			raise ValueError('Selected component index is stale or duplicated')
		seen.add(index)
		result.append((mesh, index))
	return result


def ordered_edge_plan(target_mesh=None, limit=None, selection=None):
	u"""Read tracked edge order from one mesh, without changing preferences.

	Enable Track Selection Order before selecting edges. Enabling it during
	this query cannot reconstruct the user's earlier selection history.
	"""
	if selection is None:
		selection = pm.ls(orderedSelection=True, flatten=True) or []
	if not selection:
		return []
	if not pm.selectPref(q=True, trackSelectionOrder=True):
		raise ValueError('Enable Track Selection Order, then reselect the mesh edges.')
	return mesh_component_plan(selection, target_mesh=target_mesh, limit=limit)


def getSelectedEdgeIdsList(target_mesh=None):
	return [index for mesh, index in ordered_edge_plan(target_mesh)]
