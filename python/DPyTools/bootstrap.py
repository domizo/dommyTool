# -*- coding: utf-8 -*-
"""Maya menu/bootstrap integration with lazy feature imports."""

import importlib

import maya.cmds as cmds
import maya.mel as mel
from maya.app.general import nodeEditorMenus


DPToolsMenuName = 'DPToolsMenu'
PLUGIN_NAMES = ('dommyTool', 'DTools')


def _display_warning(message):
	cmds.warning(message)


def _resolve(module_name, attribute_path):
	value = importlib.import_module(module_name)
	for attribute in attribute_path.split('.'):
		value = getattr(value, attribute)
	return value


def _invoke(module_name, attribute_path, args=None, kwargs=None):
	args = () if args is None else args
	kwargs = {} if kwargs is None else kwargs
	try:
		return _resolve(module_name, attribute_path)(*args, **kwargs)
	except Exception as error:
		cmds.error(
			'DPyTools command failed ({0}.{1}): {2}'
			.format(module_name, attribute_path, error))


def _invoke_method(module_name, class_name, method_name):
	try:
		instance = _resolve(module_name, class_name)()
		return getattr(instance, method_name)()
	except Exception as error:
		cmds.error(
			'DPyTools command failed ({0}.{1}.{2}): {3}'
			.format(module_name, class_name, method_name, error))


def _callback(module_name, attribute_path, args=None, kwargs=None):
	def callback(*unused):
		return _invoke(module_name, attribute_path, args=args, kwargs=kwargs)
	return callback


def _method_callback(module_name, class_name, method_name):
	def callback(*unused):
		return _invoke_method(module_name, class_name, method_name)
	return callback


def _item(label, module_name, attribute_path, annotation=None, args=None, kwargs=None):
	options = {
		'label': label,
		'command': _callback(module_name, attribute_path, args=args, kwargs=kwargs),
	}
	if annotation:
		options['annotation'] = annotation
	return cmds.menuItem(**options)


def _method_item(label, module_name, class_name, method_name, annotation=None):
	options = {
		'label': label,
		'command': _method_callback(module_name, class_name, method_name),
	}
	if annotation:
		options['annotation'] = annotation
	return cmds.menuItem(**options)


def _submenu(label):
	return cmds.menuItem(label=label, subMenu=True, tearOff=True)


def _end_submenu():
	cmds.setParent('..', menu=True)


def _divider(label=None):
	options = {'divider': True}
	if label:
		options['dividerLabel'] = label
	cmds.menuItem(**options)


def createDPyToolsMenu():
	"""Create the DPyTools menu once and return its Maya UI name."""
	if cmds.menu(DPToolsMenuName, query=True, exists=True):
		return DPToolsMenuName

	main_window = mel.eval('$temp1 = $gMainWindow')
	menu = cmds.menu(
		DPToolsMenuName, parent=main_window, label='DPTools', tearOff=True)
	cmds.setParent(menu, menu=True)

	_submenu('Create')
	_divider('Curve')
	_item('Spiral Curve', 'DPyTools.curve.createCurveOp', 'createSpiralCurve')
	_divider('Mesh')
	_item('Poly Wire', 'DPyTools.mesh.meshGen', 'createPolyWire')
	_divider('Human')
	_item('Human Eye', 'DPyTools.create.HumanGenOp', 'createHumanEye')
	_item('Create Eye Lash', 'DPyTools.create.HumanGenOp', 'createEyelash')
	_end_submenu()

	_submenu('Utility')
	_item('Print BBox Information', 'DPyTools.general.measureOp', 'printBBox')
	_item('Create BBox Measure', 'DPyTools.general.measureOp', 'createBBoxMeasure')
	_item('Delete BBox Measure', 'DPyTools.general.measureOp', 'deleteBBoxMeasure')
	_end_submenu()

	_submenu('Asset')
	_divider('Project Management')
	_method_item('Make Project...', 'DPyTools.pipeline.assets.operations', 'DMakeProject', 'openWin')
	_method_item('Open Renamer', 'DPyTools.utility.DAssetRenamer', 'DAssetRenamer', 'openWin')
	_method_item('Open Asset Manager', 'DPyTools.pipeline.assets.manager', 'DAssetManager', 'openWin')
	_method_item('Open Scene Builder', 'DPyTools.pipeline.assets.scene_builder', 'DSceneBuilder', 'openWin')
	_divider('User Management')
	_method_item('User Management', 'DPyTools.pipeline.assets.users', 'DUserManager', 'openWin')
	_end_submenu()

	_submenu('Node Operations')
	_item('Open Node Info View', 'DPyTools.node.DNodeOp', 'nodeInfoView')
	_divider()
	_item('Share Parameters...', 'DPyTools.operation.DShareParamOp', 'openWin')
	_item('Copy Parameters...', 'DPyTools.operation.DCopyParamOp', 'openWin')
	_end_submenu()

	_submenu('Rig Controller')
	controller_module = 'DPyTools.create.rig_controller'
	controller_shapes = (
		('Square', 0, {}), ('Circle', 1, {}), ('Cube', 2, {}),
		('Sphere', 3, {}), ('Diamond', 3, {'division': 4}),
		('Pyramid', 4, {}), ('Arrow', 5, {}), ('Slide Arrow', 6, {}),
		('Cross Arrow', 7, {}), ('Pin', 8, {'arrowRatio': 0.2}),
	)
	for label, shape_type, extra_attributes in controller_shapes:
		attributes = {'shapeType': shape_type}
		attributes.update(extra_attributes)
		_item(
			label, controller_module, 'createRigCtrl',
			kwargs={'attributes': attributes, 'side': 1})
	_divider()
	_item('Custom', controller_module, 'openWin')
	_end_submenu()

	_submenu('Bottle Rig Tools')
	_item(
		'Create Tilt Spin Rig', 'DPyTools.rigging.bottle.bottleRigTool',
		'createBottleSpinRig')
	_end_submenu()

	_submenu('Human Rig Tool')
	_divider('Rivet Tools')
	_item('Polygon Edge Rivet', 'DPyTools.rigging.DConstrainOp', 'createEdgeRivet')
	_item('Polygon Face Rivet', 'DPyTools.rigging.DConstrainOp', 'createFaceRivet')
	_divider('BlendShape Tools')
	_item('BlendShape Mirror', 'DPyTools.rigging.DBlendShapeOp', 'createLRShape')
	_item('BlendShape UD From Color', 'DPyTools.rigging.DBlendShapeOp', 'createUDShape')
	_item('BlendShape Flip', 'DPyTools.rigging.DBlendShapeOp', 'createBSFlipShape')
	_divider('Rig Tools')
	_method_item('Eye Rig Tool', 'DPyTools.rigging.DEyeRigOp', 'EyeRiggingTool', 'openWin')
	_method_item('Face Rig Tool', 'DPyTools.rigging.DFaceRigOp', 'DFaceRiggingTool', 'openWin')
	_item('Human Rig Tool', 'DPyTools.rigging.humanRigTool', 'openWin')
	_divider()
	_method_item(
		'DFace Joint Tool', 'DPyTools.rigging.DFaceJointTool',
		'DFaceJointTool', 'openWin')
	_end_submenu()

	_submenu('DPoint')
	_item('Create DPoint from Selection', 'DPyTools.dpoint.generate.DPointAddOp', 'openWin')
	_divider()
	_item(
		'DPEmitter old', 'DPyTools.dpoint.particle.DPParticleOp',
		'createDPEmitter', kwargs={'nParticleMode': False})
	_item(
		'DPEmitter', 'DPyTools.dpoint.particle.DPParticleOp',
		'createDPEmitter', kwargs={'nParticleMode': True})
	_end_submenu()

	_submenu('DCurve')
	_item('Convert To Maya Curves', 'DPyTools.dcurve.convert.DCConvertOp', 'createDCConvertToCurve')
	_end_submenu()

	_submenu('DMesh')
	_item('Convert To Maya Mesh', 'DPyTools.dmesh.convert.DMConvertOp', 'createDMToMesh')
	_end_submenu()

	_submenu('RenderMan')
	_item(
		'Create VDM Ptex Template', 'DPyTools.integrations.renderman.common',
		'CreateSimplePxrVDM', kwargs={'ptexMode': True})
	_item(
		'Create VDM UV Template', 'DPyTools.integrations.renderman.common',
		'CreateSimplePxrVDM', kwargs={'ptexMode': False})
	_end_submenu()

	_submenu('Import / Export')
	_item('Export to Alembic', 'DPyTools.pipeline.io.alembic', 'ddata_export')
	_end_submenu()

	_submenu('Windows')
	_item('Outliner', 'DPyTools.ui.DOutlinerUI', 'openWin')
	_end_submenu()

	_submenu('System')
	_item('Current Opened File Info', 'DPyTools.features.system_info', 'currentFileInfo')
	_divider()
	_item('System Info', 'DPyTools.features.system_info', 'displayInfo')
	_item('Loaded Plugin Info', 'DPyTools.features.system_info', 'pluginInfo')
	_item('OpenGL Info', 'DPyTools.features.system_info', 'displayOpenGLInfo')
	_divider()
	_method_item(
		'Clear Files in Target Folder', 'DPyTools.utility.DFileCleaner',
		'DFileCleaner', 'openWin')
	_end_submenu()

	return menu


def _load_plugin():
	for plugin_name in PLUGIN_NAMES:
		try:
			if cmds.pluginInfo(plugin_name, query=True, loaded=True):
				return plugin_name
		except RuntimeError:
			pass

	for plugin_name in PLUGIN_NAMES:
		try:
			cmds.loadPlugin(plugin_name, quiet=True)
			return plugin_name
		except RuntimeError:
			continue

	_display_warning(
		'DPyTools menu loaded, but the dommyTool plug-in binary was not found.')
	return None


def _is_marking_menu_callback(callback):
	return (
		getattr(callback, '__module__', None) == 'DPyTools.ui.customMarkingMenus'
		and getattr(callback, '__name__', None) == 'createDToolNodeEdMarkingMenus')


def _register_node_editor_callback():
	callback = _resolve(
		'DPyTools.ui.customMarkingMenus', 'createDToolNodeEdMarkingMenus')
	callbacks = nodeEditorMenus.customInclusiveNodeItemMenuCallbacks
	callbacks[:] = [item for item in callbacks if not _is_marking_menu_callback(item)]
	callbacks.append(callback)


def _unregister_node_editor_callback():
	callbacks = nodeEditorMenus.customInclusiveNodeItemMenuCallbacks
	callbacks[:] = [item for item in callbacks if not _is_marking_menu_callback(item)]


def menuImport():
	_load_plugin()
	menu = createDPyToolsMenu()
	_display_warning('DPTools loaded.')
	return menu


def main():
	"""Load the menu and callbacks once in interactive Maya sessions."""
	if cmds.about(batch=True):
		return False
	menuImport()
	_register_node_editor_callback()
	return True


def unload():
	"""Remove Python UI and callbacks without unloading the C++ plug-in."""
	_unregister_node_editor_callback()
	if cmds.menu(DPToolsMenuName, query=True, exists=True):
		cmds.deleteUI(DPToolsMenuName, menu=True)
	return True
