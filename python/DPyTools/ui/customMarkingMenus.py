import pymel.core as pm
from maya import cmds
import importlib

'''
to run:
	>>> import DPyTools.ui.customMarkingMenus as customMarkingMenus


'''


def _command(module_name, function_name, **kwargs):
	def callback(*unused):
		module = importlib.import_module(module_name)
		return getattr(module, function_name)(**kwargs)
	return callback


def createDToolNodeEdMarkingMenus(parent, node):
	"""
	Check for plusMinusAverage node and create necessary menu items
	"""
	try:
		loaded = pm.pluginInfo('dommyTool', query=True, loaded=True)
	except RuntimeError:
		loaded = False
	attrList = pm.listAttr(node) or []
	node_type = cmds.nodeType(node)
	menu_created = False

	dtoolNodeList = []
	if loaded:
		dtoolNodeList = pm.pluginInfo("dommyTool", q=True, dependNode=True )

		if node_type in dtoolNodeList:
			if 'outDP' in attrList:
				menu_created = True
				cmds.menuItem(subMenu=True, label="DPointDisplay")

				dp_list = pm.ls(type='DPointDisplay')
				if len(dp_list):
					for dpd in dp_list:
						#cmds.menuItem(label=str(dpd),sourceType='python', c=lambda *args:DPointDisplayOp.switch_dpoint_display(target_node=node, target_display=str(dpd)))
						cmds.menuItem(
							label=str(dpd),
							c=_command(
								'DPyTools.dpoint.display.DPointDisplayOp',
								'switch_dpoint_display', target_node=node,
								target_display=str(dpd)))
				cmds.menuItem(
					label="Create New DPointDisplay",
					c=_command(
						'DPyTools.dpoint.display.DPointDisplayOp',
						'createDPointDisplay', targetNode=node))
				cmds.setParent('..', menu=True)
			elif 'outDC' in attrList:
				menu_created = True
				cmds.menuItem(subMenu=True, label="DCurveDisplay")

				cmds.menuItem(
					label="Create New DCurveDisplay",
					c=_command(
						'DPyTools.dcurve.display.DCurveDisplayOp',
						'createDCurveDisplay', targetNode=node))

				dc_list = pm.ls(type='DCurveDisplay')
				if len(dc_list):
					for dcd in dc_list:
						cmds.menuItem(
							label=str(dcd),
							c=_command(
								'DPyTools.dcurve.display.DCurveDisplayOp',
								'switch_dcurve_display', target_node=node,
								target_display=str(dcd)))

				cmds.setParent('..', menu=True)

				cmds.menuItem(subMenu=True, label="DCMarker")
				cmds.menuItem(
					label="Create New DCMarker",
					c=_command(
						'DPyTools.dcurve.display.DCurveDisplayOp',
						'createDCMarker', targetNode=node))

				dc_list = pm.ls(type='DCMarker')
				if len(dc_list):
					for dcd in dc_list:
						cmds.menuItem(
							label=str(dcd),
							c=_command(
								'DPyTools.dcurve.display.DCurveDisplayOp',
								'switch_dcurve_display', target_node=node,
								target_display=str(dcd)))

				cmds.setParent('..', menu=True)

			elif 'outDM' in attrList:
				menu_created = True
				cmds.menuItem(subMenu=True, label="DMeshDisplay")
				cmds.menuItem(
					label="Create New DMeshDisplay",
					c=_command(
						'DPyTools.dmesh.display.DMeshDisplayOp',
						'createDMeshDisplay', targetNode=node))

				dm_list = pm.ls(type='DMeshDisplay')
				if len(dm_list):
					for dmd in dm_list:
						cmds.menuItem(
							label=str(dmd),
							c=_command(
								'DPyTools.dmesh.display.DMeshDisplayOp',
								'switch_dmesh_display', target_node=node,
								target_display=str(dmd)))

				cmds.setParent('..', menu=True)

				cmds.menuItem(subMenu=True, label="DMMarker")
				cmds.menuItem(
					label="Create New DMMarker",
					c=_command(
						'DPyTools.dmesh.display.DMeshDisplayOp',
						'createDMMarker', targetNode=node))

				dm_list = pm.ls(type='DMMarker')
				if len(dm_list):
					for dmd in dm_list:
						cmds.menuItem(
							label=str(dmd),
							c=_command(
								'DPyTools.dmesh.display.DMeshDisplayOp',
								'switch_dmesh_display', target_node=node,
								target_display=str(dmd)))

				cmds.setParent('..', menu=True)

	return menu_created
