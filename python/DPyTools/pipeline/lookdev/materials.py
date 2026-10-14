# -*- coding: utf-8 -*-
import sys
from contextlib import contextmanager

import pymel.core as pm



import DPyTools.system.DPathOp as dpath
from DPyTools.pipeline.lookdev import texture_plan
from DPyTools.core import strings
from DPyTools.scene import parameters






'''
to run:
	>>> import DPyTools.pipeline.lookdev.materials as dmat


'''






def _selected_objects(obj=None):
	if obj is None:
		return pm.ls(selection=True, objectsOnly=True) or []
	if isinstance(obj, (list, tuple)):
		return list(obj)
	return [obj]


def _surface_materials(shading_engines):
	materials = []
	for shading_engine in shading_engines:
		connections = pm.listConnections(
			shading_engine.surfaceShader, source=True, destination=False) or []
		for material in connections:
			if material not in materials:
				materials.append(material)
	return materials


def getMaterialFromObject(obj=None):
	u"""選択物または指定ノードに接続されたSurface Materialを返します。"""
	shading_engines = []
	for node in _selected_objects(obj):
		shapes = node.getShapes(noIntermediate=True) if node.type() == 'transform' else [node]
		for shape in shapes:
			for shading_engine in (
					pm.listConnections(shape, type='shadingEngine') or []):
				if shading_engine not in shading_engines:
					shading_engines.append(shading_engine)
	return _surface_materials(shading_engines)


def getMaterialFromPerFace(obj=None):
	u"""指定メッシュのFace Componentに割り当てられたMaterialを返します。"""
	objects = _selected_objects(obj)
	object_names = set(str(node) for node in objects)
	shading_engines = []
	for node in objects:
		shapes = node.getShapes(noIntermediate=True) if node.type() == 'transform' else [node]
		object_names.update(str(shape) for shape in shapes)
		for shape in shapes:
			for shading_engine in (
					pm.listConnections(shape, type='shadingEngine') or []):
				members = pm.sets(shading_engine, query=True) or []
				if any(
						'.f[' in str(member)
						and str(member).split('.', 1)[0] in object_names
						for member in members):
					if shading_engine not in shading_engines:
						shading_engines.append(shading_engine)
	return _surface_materials(shading_engines)


def getAllFileTypeNode():
	u"""ファイルとしてのアトリビュートが登録されているノードのリストを返します"""
	return _file_editor_list(q=1, listRegisteredTypes=1)


def _file_editor_list(**kwargs):
	values = pm.filePathEditor(**kwargs)
	values = [] if values is None else values
	if not isinstance(values, (list, tuple)) or len(values) > parameters.MAX_CONNECTIONS:
		raise ValueError('File-path query must return a bounded list')
	return list(values)


def isUnResolveFiles():
	u"""シーン内に存在する未解決のファイルリストを返します"""
	return bool(getUnResolveFiles())


def getUnResolveFiles():
	u"""シーン内に存在する未解決のファイルリストを返します"""
	pm.filePathEditor(refresh=1)
	return _file_editor_list(q=1, listFiles="", unresolved=1)


def getListFileDirs():
	u"""シーン内に存在するファイルディレクトリパスのリストを返します"""
	return _file_editor_list(q=1, listDirectories='')


def getListFileDirsStatus(dir=''):
	u"""シーン内に存在するファイルディレクトリパスの状態を含めたリストを返します"""
	return _file_editor_list(query=1, listDirectories=dir, status=1)



def getAllFileNode():
	return pm.ls(et="file") or []



def getAllFileNodeFilePaths():
	return [value for port, value in _texture_inputs()]


def copyFileNodeImageToFolder(copyFolder='', verbose=True):
	source_paths = getAllFileNodeFilePaths()
	copies, mapping = texture_plan.build_copy_plan(source_paths, copyFolder)
	if verbose:
		for source, destination in copies:
			print ('Copy texture: {0} -> {1}'.format(source, destination))
	texture_plan.execute_copy_plan(copies)
	return mapping






def _texture_path(value, allow_empty=False):
	if (not isinstance(value, strings.STRING_TYPES) or (not value and not allow_empty)
			or '\x00' in value):
		raise ValueError('Texture path must be text without NUL characters')
	return value


def _texture_inputs(editable=False):
	result = []
	for node in parameters.resolve_nodes(getAllFileNode(), allow_empty=True):
		port = node.attr('fileTextureName')
		value = _texture_path(pm.getAttr(port), allow_empty=True)
		if not value:
			continue
		if pm.getAttr(port, type=True) != 'string':
			raise ValueError('Texture filename attribute must have string type')
		if editable:
			parameters.value_plan(node, {'fileTextureName': value})
		result.append((port, value))
	return tuple(result)


def validate_texture_edits():
	"""Admit current texture destinations before creating export directories."""
	_texture_inputs(editable=True)


def _file_path_plan(replacePath, path_mapping):
	if path_mapping is not None and (not isinstance(path_mapping, dict)
			or len(path_mapping) > parameters.MAX_CONNECTIONS):
		raise ValueError('Texture path mapping must be a bounded dictionary')
	updates = []
	for port, currentFilePath in _texture_inputs(editable=True):
		if path_mapping is None:
			newNodePath = dpath.append(
				replacePath, dpath.lastComponent(path=currentFilePath))
		else:
			if currentFilePath not in path_mapping:
				raise ValueError(
					'No copied texture path for: {0}'.format(currentFilePath))
			newNodePath = path_mapping[currentFilePath]
		newNodePath = _texture_path(newNodePath)
		admitted = parameters.value_plan(port.node(), {'fileTextureName': newNodePath})
		if admitted[0][1] != 'string':
			raise ValueError('Texture filename attribute must have string type')
		updates.append((port, currentFilePath, newNodePath))
	return tuple(updates)


def _apply_file_paths(plan, attempted, verbose):
	for port, original, replacement in plan:
		if verbose:
			print ('New Node Path:{0}'.format(replacement))
		# A native dispatch may change the value and then throw.
		attempted.append((port, original))
		pm.setAttr(port, replacement, type='string')


def _restore_file_paths(attempted, active_error):
	errors = []
	for port, original in reversed(attempted):
		try:
			parameters.editable_node(port.node())
			parameters.writable_port(port)
			if pm.getAttr(port, type=True) != 'string':
				raise ValueError('Texture attribute type changed during export')
			pm.setAttr(port, original, type='string')
		except (RuntimeError, TypeError, ValueError) as error:
			errors.append(error)
	if errors:
		message = 'Texture path restoration was incomplete: {0}'.format(errors[0])
		if not active_error:
			raise RuntimeError(message)
		try:
			pm.warning(message)
		except (RuntimeError, TypeError, ValueError):
			pass


def replaceFilePath(replacePath='', verbose=True, path_mapping=None):
	"""Historical persistent replacement; failed dispatch restores attempted ports."""
	plan = _file_path_plan(replacePath, path_mapping)
	attempted, completed = [], False
	try:
		_apply_file_paths(plan, attempted, verbose)
		result = dict((str(port), original) for port, original in attempted)
		completed = True
		return result
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			_restore_file_paths(attempted, active_error=exception_type is not None)


@contextmanager
def temporary_file_paths(replacePath='', verbose=True, path_mapping=None):
	"""Restore retained texture ports after success, failure or interruption."""
	plan = _file_path_plan(replacePath, path_mapping)
	attempted = []
	try:
		_apply_file_paths(plan, attempted, verbose)
		yield
	finally:
		exception_type = sys.exc_info()[0]
		_restore_file_paths(attempted, active_error=exception_type is not None)
