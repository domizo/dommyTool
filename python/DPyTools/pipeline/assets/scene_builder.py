# -*- coding: utf-8 -*-
u"""Scene assembly UI backed by asset version metadata."""

import pymel.core as pm
import maya.cmds as cmds
import maya.OpenMaya as om



from DPyTools.pipeline.assets import operations as dasset

from DPyTools.features import system_info as dsi
import DPyTools.system.DPathOp as dpath


from DPyTools.features.animation import io as danimIO
from DPyTools.pipeline.assets import repository as asset_repository
from DPyTools.pipeline.assets import metadata as asset_metadata
from DPyTools.pipeline.assets import browser
from DPyTools.core import diagnostics
from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection


_LOGGER = diagnostics.get_logger(__name__)




'''
to run:
	>>> import DPyTools.pipeline.assets.scene_builder as DAScene

'''




class DSceneBuilder(object):

	m_uiname = "DSceneBuilder"

	# File data belongs to each window instance.
	m_file_data = None


	def __init__(self):
		self.m_uiname = "DSceneBuilder"
		self.m_file_data = []



	def openWin(self):
		u"""ウィンドウを表示"""

		#template
		template = pm.uiTemplate('dSceneBuilderTemplate', force=True)
		template.define(pm.columnLayout, rowSpacing=10, adjustableColumn=True)


		#new window
		ui_windows.delete_window(self.m_uiname)


		win = pm.window(self.m_uiname, widthHeight=(790, 640))

		#set template
		pm.setUITemplate( 'dSceneBuilderTemplate', pushTemplate=True )



		############################################################
		####################create form and column####################
		self.form = pm.formLayout(numberOfDivisions=100)


		self.mainRowLayout = pm.rowLayout(numberOfColumns=3,
										rowAttach=[(1,'both',0),(2,'both',0),(3,'both',0)])


		#pm.textScrollList( allowMultiSelection=False, sc=lambda *args: self.changeProjectSelectCB())
		self.projTextScroll = dasset.DProjectTextScroll(parent=self.mainRowLayout, isActionButton=None)
		self.projTextScroll.m_parent_win = self.m_uiname
		self.projTextScroll.delegate_changeProj_fnc = self.changeProjectCB

		#pm.textScrollList( allowMultiSelection=False, sc=lambda *args: self.changeProjectSelectCB())
		self.assetTypeScroll = dasset.DAssetTypeTextScroll(parent=self.mainRowLayout, isActionButton=None)
		self.assetTypeScroll.m_parent_win = self.m_uiname
		self.assetTypeScroll.project_path_getter = self.projTextScroll.getProjectPath
		self.assetTypeScroll.delegate_changeSelect_fnc = self.changeAssetTypeCB


		#pm.textScrollList( allowMultiSelection=False, sc=lambda *args: self.changeProjectSelectCB())
		self.assetScroll = dasset.DAssetTextScroll(parent=self.mainRowLayout, isActionButton=None)
		self.assetScroll.m_parent_win = self.m_uiname
		self.assetScroll.project_path_getter = self.projTextScroll.getProjectPath
		self.assetScroll.delegate_changeSelect_fnc = self.changeAssetCB
		pm.setParent( '..' )#rowLayout


		self.underRowLayout = pm.rowLayout(numberOfColumns=3, adjustableColumn=1,
									columnWidth3=(330, 256, 256),
									rowAttach=[(1,'both',0),(2,'both',0),(3,'both',0)])

		self.fileTable = cmds.scriptTable('DSfile_list_table', rows=0, columns=6, selectionMode=1, selectionBehavior=1,
									label=[(1,"Version"), (2,"FileName"), (3,"Creator"), (4,"Ext"), (5,"Date"), (6,"Comment")],
									editable=0, sortEnabled=0,
									columnWidth=[(1,60),(2,200),(3,80),(4,60),(5,200),(6,200)],
					getCellCmd=self.getCellCB,
					selectionChangedCmd=self.changeSelectionCB
					)

		imageSize = 256
		pm.columnLayout()
		self.sceneThumbView = pm.image(width=imageSize, height=imageSize, bgc=[.2,.2,.2])
		self.commentText = pm.text(label='', wordWrap=1, height=60, align='left', recomputeSize=0, bgc=[.2,.2,.2])

		pm.setParent( '..' )#columnLayout


		pm.columnLayout()

		importTypeList = ['Import', 'Reference']
		self.importTypeMenu = pm.optionMenu( label='Type', width=imageSize)
		for stype in importTypeList :
			pm.menuItem( label=stype )



		self.namespaceField = pm.textFieldGrp( label='namespace', text='', width=imageSize )

		self.isIgnoreVerCheck = pm.checkBoxGrp(label="ignore Version", value1=0, width=imageSize )

		#reference
		self.isDeferRefCheck = pm.checkBoxGrp(label="Defer Reference", value1=0, width=imageSize )
		self.isLockRefCheck = pm.checkBoxGrp(label="Lock Reference", value1=1, width=imageSize )

		#for danim
		self.d_paths = pm.checkBox(al="left", v=1, label="Load Explicit Paths")
		self.d_unKeyed = pm.checkBox(v=1, label="Load Un-Keyed")
		self.targetNamespaceField = pm.textFieldGrp( label='target namespace', placeholderText='for danim', text='', width=imageSize )

		#run button
		pm.button(label='Import',bgc=[0.5,.0,0.0], c=lambda *args: self.runImportCB(), width=imageSize, height=44)
		pm.setParent( '..' )#columnLayout


		pm.setParent( '..' )#underRowLayout
		pm.setParent( '..' )#form


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.mainRowLayout, 'top', 5), (self.mainRowLayout, 'left', 5), (self.mainRowLayout, 'right', 5),
						(self.underRowLayout, 'left', 5), (self.underRowLayout, 'right', 5), (self.underRowLayout, 'bottom', 5)],
			attachPosition=[(self.mainRowLayout, 'bottom', 0, 40)],
			attachControl=[(self.underRowLayout, 'top', 5, self.mainRowLayout)]
			)


		#fin template
		pm.setUITemplate( popTemplate=True )

		#show window
		win.show()


		#initial load project list
		self.prepareCB()



	def prepareCB(self):

		self.projTextScroll.refleshCB()

		#clear file table
		self.clearFileTableCB()


	@browser.action
	def changeProjectCB(self):
		return self.assetTypeScroll.refleshCB(preserve_selection=False)

	@browser.action
	def changeAssetTypeCB(self):
		category = self.assetTypeScroll.getAssetType()
		if not isinstance(category, table_selection.STRING_TYPES):
			return False
		self.assetScroll.m_asset_type = category
		return self.assetScroll.refleshCB()

	def clearFileTableCB(self):
		_LOGGER.debug('Clearing Scene Builder version table')
		self.m_file_data = []
		#pm.scriptTable(self.fileTable, e=1, selectedRows=[])
		pm.scriptTable(self.fileTable, e=1, rows=0)


		pm.image(self.sceneThumbView, e=1, image='')
		pm.text(self.commentText, e=1, label='')


	@browser.action
	def changeAssetCB(self):
		_LOGGER.debug('Scene Builder asset changed')

		#clear file table
		self.clearFileTableCB()
		if not self.assetScroll.m_asset_type or not self.assetScroll.m_asset:
			return True

		#version_list = dasset.getAssetVersionList(assetType=self.assetScroll.m_asset_type, assetName=self.assetScroll.m_asset)
		#print ('version_list:{f}'.format(f=version_list))
		assetPath = dasset.getAssetPath(assetType=self.assetScroll.m_asset_type, assetName=self.assetScroll.m_asset,
			project_path=self.projTextScroll.getProjectPath())
		_LOGGER.debug('Selected asset path: %s', assetPath)
		if not assetPath:
			pm.warning('Invalid asset path.')
			return
		jsonFile = dpath.append(assetPath, '.asset')



		if dpath.isFile(jsonFile):

			#load exist json file
			#data = []
			try:
				self.m_file_data = asset_repository.load_version_records(
					jsonFile, asset_root=assetPath)
			except (IOError, OSError, TypeError, ValueError) as exc:
				pm.warning('Invalid asset metadata: {0}'.format(exc))
				self.m_file_data = []
			self.m_file_data.reverse()

			# print ('file data:{f}'.format(f=self.m_file_data))


			versionCount = len(self.m_file_data)
			_LOGGER.debug('Asset version count: %s', versionCount)


			pm.scriptTable(self.fileTable, e=1, rows=versionCount)
			pm.scriptTable(self.fileTable, e=1, clearTable=1)

			if versionCount > 0 :
				cmds.scriptTable(self.fileTable, e=1, selectedRow=1)


	def _selected_version(self):
		u"""Return one cached record only after shared row admission."""
		try:
			selection = pm.scriptTable(self.fileTable, q=1, selectedRow=1)
			rows = table_selection.selected_rows(selection, len(self.m_file_data))
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Version selection could not be read: {0}'.format(exc))
			return None
		return self.m_file_data[rows[0] - 1] if rows else None

	@browser.action
	def changeSelectionCB(self):
		ver_data = self._selected_version()
		if ver_data is None:
			return
		assetPath = dasset.getAssetPath(
			assetType=self.assetScroll.m_asset_type,
			assetName=self.assetScroll.m_asset,
			project_path=self.projTextScroll.getProjectPath())
		if not assetPath:
			pm.warning('Asset metadata is no longer safe to display: invalid asset path')
			return
		try:
			ver_data = asset_metadata.validate_version_record(ver_data, asset_root=assetPath)
		except (IOError, OSError, TypeError, ValueError) as exc:
			pm.warning('Asset metadata is no longer safe to display: {0}'.format(exc))
			return
		thumbnail = ver_data['thumbnail']
		pm.image(self.sceneThumbView, e=1,
			image=thumbnail if thumbnail and dpath.isFile(thumbnail) else '')
		pm.text(self.commentText, e=1, label=ver_data['comment'])
		pm.textFieldGrp(self.namespaceField, e=1, text=self.assetScroll.m_asset)

	def getCellCB(self, row, column):
		u"""Return a cell only for valid cached row/column coordinates."""
		fields = ('version', 'filename', 'creator', 'extension', 'date', 'comment')
		try:
			rows = table_selection.selected_rows(row, len(self.m_file_data))
			columns = table_selection.selected_rows(column, len(fields))
		except (TypeError, ValueError):
			return ''
		if not rows or not columns:
			return ''
		return self.m_file_data[rows[0] - 1].get(fields[columns[0] - 1], '')

	def refreshCellCB(self, row, column, value):
		return 1

	def runImportCB(self):
		u"""Revalidate selection, metadata and controls before file dispatch."""
		ver_data = self._selected_version()
		if ver_data is None:
			pm.warning('No valid selected file')
			return False
		try:
			assetPath = dasset.getAssetPath(
				assetType=self.assetScroll.m_asset_type,
				assetName=self.assetScroll.m_asset,
				project_path=self.projTextScroll.getProjectPath())
			if not assetPath:
				raise ValueError('Invalid asset path')
			ver_data = asset_metadata.validate_import_record(ver_data, asset_root=assetPath)
			file_path = ver_data['filepath']
			import_type = pm.optionMenu(self.importTypeMenu, q=1, value=1)
			if import_type not in ('Import', 'Reference'):
				raise ValueError('Unsupported scene operation mode')
			namespace = table_selection.cell_text(
				pm.textFieldGrp(self.namespaceField, q=1, text=1))
			ignore_version = pm.checkBoxGrp(self.isIgnoreVerCheck, q=1, value1=1)
			defer_reference = pm.checkBoxGrp(self.isDeferRefCheck, q=1, value1=1)
			lock_reference = pm.checkBoxGrp(self.isLockRefCheck, q=1, value1=1)
			extension = ver_data['extension']

			if extension in ('.ma', '.mb'):
				io = om.MFileIO()
				if import_type == 'Import':
					io.importFile(file_path, None, 0, namespace or None, ignore_version)
				else:
					# Reference takes a string; an empty field is not the name "None".
					io.reference(file_path, defer_reference, lock_reference, namespace)
			elif extension == '.abc':
				if not dsi.isLoadedPlugin(pluginName='AbcImport'):
					if not dsi.loadMayaPlugin(pluginName='AbcImport'):
						return False
				cmds.AbcImport(file_path, mode='import')
			elif extension == '.anim':
				if not dsi.isLoadedPlugin(pluginName='animImportExport'):
					if not dsi.loadMayaPlugin(pluginName='animImportExport'):
						return False
				if not pm.ls(sl=1):
					raise ValueError('Select at least one animation target')
				cmds.file(file_path, i=1, type='animImport', gr=1,
					ignoreVersion=ignore_version, ra=1, rdn=1, mergeNamespacesOnClash=0,
					namespace=namespace,
					options=';targetTime=4;copies=1;option=replaceCompletely;pictures=0;connect=0;',
					pr=1, importTimeRange='combine')
			elif extension == '.danim':
				target_namespace = table_selection.cell_text(
					pm.textFieldGrp(self.targetNamespaceField, q=1, text=1))
				paths = pm.checkBox(self.d_paths, q=1, v=1)
				unkeyed = pm.checkBox(self.d_unKeyed, q=1, v=1)
				return bool(danimIO.d_animRead(filename=file_path, paths=paths,
					unkeyed=unkeyed, namespace=target_namespace))
			else:
				raise ValueError('Unsupported asset file type')
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Scene file operation could not complete: {0}'.format(exc))
			return False
		return True
