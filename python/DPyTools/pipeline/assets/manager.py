# -*- coding: utf-8 -*-
u"""Asset publishing and version-management UI."""

import pymel.core as pm
import maya.cmds as cmds
import maya.OpenMaya as om



import DPyTools.system.DPathOp as dpath
from DPyTools.features import system_info as dsi

from DPyTools.pipeline.assets import operations as dasset

from DPyTools.pipeline.lookdev import materials as dmat
from DPyTools.core import diagnostics
import DPyTools.general.nameActions as dname
from DPyTools.scene import selection as selOp

from DPyTools.features import view_capture as dview

from DPyTools.features.animation import operations as aniOp
from DPyTools.features.animation import io as danimIO

from DPyTools.integrations.smtp import client as smtp_client
from DPyTools.integrations.smtp import mail as dmail
from DPyTools.pipeline.assets import repository as asset_repository
from DPyTools.pipeline.assets import metadata as asset_metadata
from DPyTools.pipeline.assets import export_plan
from DPyTools.pipeline.assets import browser
from DPyTools.pipeline.assets import creation
from DPyTools.pipeline.assets import paths as asset_paths
from DPyTools.scene import attribute_values, parameters
from DPyTools.core import components, paths as core_paths
from DPyTools.pipeline.io import alembic_job
from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection

import datetime
import os


_LOGGER = diagnostics.get_logger(__name__)

'''
to run:
	>>> import DPyTools.pipeline.assets.manager as DAManager

'''

exportFileTypeList = ['.ma', '.mb', '.abc', '.anim', '.danim']
exportFileTypeDic = {'.ma':'mayaAscii', '.mb':'mayaBinary', '.abc':'Alembic', '.anim':'animExport', '.danim':'DAnimExport' }






class DAssetManager(object):
	u"""アセット管理ツール"""


	m_uiname = "DAssetManager"
	_projMenu = None

	def __init__(self):
		self.m_uiname = "DAssetManager"
		self._loaded_asset_context = ('', '')
		self._project_root = None


	def openWin(self):
		u"""ウィンドウを表示"""

		#template
		template = pm.uiTemplate('assetManagerTemplate', force=True)
		template.define(pm.columnLayout, rowSpacing=10, adjustableColumn=True)


		#new window
		ui_windows.delete_window(self.m_uiname)


		self.mainWin = pm.window(self.m_uiname, widthHeight=(790, 640), menuBar=True)

		#set template
		pm.setUITemplate( 'assetManagerTemplate', pushTemplate=True )

		############################################################
		####################create menu####################

		pm.menu( label='Project' )
		pm.menuItem( label='Set Root Path', c=lambda *args: self.changeRootCB() )
		pm.menuItem( label='Make Project', c=lambda *args: self.makeProjectCB() )
		pm.menuItem( label='Reload Project', c=lambda *args: self.refleshProjectCB() )
		pm.menu( label='Category' )
		pm.menuItem( label='Create New Category', c=lambda *args: self.makeAssetTypeCB() )
		pm.menu( label='Asset' )
		pm.menuItem( label='Create New Asset', c=lambda *args: self.makeAssetCB() )
		pm.menu( label='Mail List' )
		pm.menuItem( label='Open Mailing List Manager', c=lambda *args: self.opemMailingManagerCB() )


		############################################################
		####################create form and column####################
		self.form = pm.formLayout(numberOfDivisions=100)

		self.firstColumn = pm.columnLayout()

		pm.text(label='Project')


		self.projectFilterText = pm.textField(placeholderText='Project Filter', searchField=1, textChangedCommand=lambda *args: self.refleshProjectCB() )

		self.projTextScroll = pm.textScrollList( allowMultiSelection=False, width=120, height=480, sc=lambda *args: self.changeProjectSelectCB())


		pm.rowLayout(numberOfColumns=4, adjustableColumn=3)
		pm.button(label='Root', c=lambda *args: self.changeRootCB())
		pm.button(label='Reload', c=lambda *args: self.refleshProjectCB())

		pm.text(label='')
		pm.button(label='Make Project', c=lambda *args: self.makeProjectCB())
		# pm.button(label='Config')

		pm.setParent( '..' )#rowLayout


		'''
		pm.text(label='Project Path', width=200, align='left')

		pm.rowLayout(numberOfColumns=3)
		self.projectPathText = pm.textField(placeholderText='project path', width=240)
		pm.button(label='...', c=lambda *args: self.setProjectPathFromDialogCB())
		pm.button(label='Get', c=lambda *args: self.setProjectPathCB())
		pm.setParent( '..' )#rowLayout
		'''



		'''
		pm.rowLayout(numberOfColumns=3)
		pm.text(label='Project ID',width=80, align='left')
		self.projectIDText = pm.textField(placeholderText='project ID')
		pm.button(label='Gen',width=40, c=lambda *args: self.setProjectIDCB())
		pm.setParent( '..' )#rowLayout
		'''




		pm.setParent( '..' )#firstColumn


		####################create second column####################
		self.secondColumn = pm.columnLayout()

		pm.text(label='Category')
		self.classTextScroll = pm.textScrollList( allowMultiSelection=False, width=120, sc=lambda *args: self.changeSelectAssetTypeCB())

		pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		pm.button(label='New Category', c=lambda *args: self.makeAssetTypeCB())
		pm.text(label='')
		pm.setParent( '..' )#rowLayout

		pm.text(label='Asset')
		self.assetFilterText = pm.textField(placeholderText='Asset Filter', searchField=1, textChangedCommand=lambda *args: self.refleshAssetListCB())
		self.assetTextScroll = pm.textScrollList( allowMultiSelection=False, width=120, sc=lambda *args: self.changeSelectAssetCB())

		pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		pm.button(label='New Asset', c=lambda *args: self.makeAssetCB())
		pm.text(label='')


		pm.setParent( '..' )#rowLayout


		# self.stateTypeMenu = pm.optionMenu( label='State')

		# for stype in dasset.stateList :
		# 	pm.menuItem( label=stype )

		# pm.text(label='State')
		# self.stateTextScroll = pm.textScrollList( allowMultiSelection=False, append=dasset.stateList, width=120)


		pm.setParent( '..' )#secondColumn




		####################create second column####################
		self.thirdColumn = pm.columnLayout()

		pm.text(label=' Export Option')



		#top node
		pm.rowLayout(numberOfColumns=3, adjustableColumn=2)
		pm.text(label='Top Node',width=80, align='right')
		self.rootText = pm.textField(text='root')
		pm.button(label='Set', c=lambda *args: self.setRootNodeCB())
		pm.setParent( '..' )#rowLayout


		#fileName or assetName
		# pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		# pm.text(label='Asset Name', width=80, align='left')
		# self.assetNameText = pm.textField(placeholderText='Enter Asset Name', width=200)
		# pm.setParent( '..' )#rowLayout


		#comment
		pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		pm.text(label='comment', width=80, align='right')
		self.commentField = pm.textField(placeholderText='EnterComment', width=200)
		pm.setParent( '..' )#rowLayout

		#creator
		pm.rowLayout(numberOfColumns=3, adjustableColumn=2)
		pm.text(label='creator', width=80, align='right')
		self.CreatorText = pm.textField(placeholderText='creator name')
		pm.button(label='Set', c=lambda *args: self.setUserNameCB())
		pm.setParent( '..' )#rowLayout


		#time range
		pm.rowLayout(numberOfColumns=6, adjustableColumn=5)
		pm.text(label='start', width=80, align='right')
		self.startTime = pm.intField(width=60)
		pm.text(label='end', width=40, align='right')
		self.endTime = pm.intField(width=60)

		pm.text(label='', width=10)
		pm.button(label='Get Time Range', c=lambda *args: self.setTimerangeFromTimeSliderCB())

		pm.setParent( '..' )#rowLayout


		self.fileTypeMenu = pm.optionMenu( label='file type')

		for ftype in exportFileTypeList :
			pm.menuItem( label=ftype )


		##########file type option
		pm.frameLayout( label="Animation Option", bgc=[0.0,.7,.7], collapsable=True, collapse=True, borderVisible=1)
		#alembic option
		pm.frameLayout( label="Alembic Option", collapsable=True, collapse=False, borderVisible=1)
		self.isNoNormalCheck = pm.checkBoxGrp(label="No Normal", value1=0 )
		self.isRenderableOnlyCheck = pm.checkBoxGrp(label="Renderable Only", value1=1 )
		self.isUVWriteCheck = pm.checkBoxGrp(label="UV Write", value1=1 )
		self.isWorldSpaceCheck = pm.checkBoxGrp(label="World Space", value1=0 )
		self.isWholeFrameGeoCheck = pm.checkBoxGrp(label="Whole FrameGeo", value1=0 )
		self.isWriteVisibilityCheck = pm.checkBoxGrp(label="Write Visibility", value1=0 )
		pm.setParent( '..' )#frameLayout

		#animation option
		pm.frameLayout( label="Animation Option", collapsable=True, collapse=False, borderVisible=1)
		self.animHierarchyRadio = pm.radioButtonGrp(select=2, numberOfRadioButtons=2, label="Hierarchy", labelArray2=("Selected", "Below"))
		pm.setParent( '..' )#frameLayout

		pm.setParent( '..' )#frameLayout


		#export file texture
		pm.rowLayout(numberOfColumns=3)
		pm.text(label='Export File Node',width=100, align='left')
		self.checkExportFile = pm.checkBox(label="", value=0 )
		pm.setParent( '..' )#rowLayout


		#save thumbnail
		pm.rowLayout(numberOfColumns=3)
		pm.text(label='Save Thumbnail',width=100, align='left')
		self.checkThumbnail = pm.checkBox(label="", value=1 )
		pm.setParent( '..' )#rowLayout


		#send update
		pm.rowLayout(numberOfColumns=3)
		pm.text(label='Send Notification',width=100, align='left')
		self.sendMailTask = pm.checkBox(label="", value=False )
		pm.setParent( '..' )#rowLayout

		pm.button(
			label='Report Duplicate Names', bgc=[0.0, 0.7, 0.7],
			c=lambda *args: self.checkProblemCB())

		self.exportNameText = pm.text(label='', height=60)
		pm.button(label='Export',bgc=[0.5,.0,0.0], c=lambda *args: self.runExportCB(), height=44)

		pm.setParent( '..' )#thirdColumn

		pm.setParent( '..' )#form


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.firstColumn, 'top', 5), (self.firstColumn, 'left', 5), (self.firstColumn, 'bottom', 5),
					(self.secondColumn, 'top', 5), (self.secondColumn, 'bottom', 5),
					(self.thirdColumn, 'top', 5), (self.thirdColumn, 'right', 5), (self.thirdColumn, 'bottom', 5)],
			attachControl=[(self.secondColumn, 'left', 5, self.firstColumn),
							(self.thirdColumn, 'left', 5, self.secondColumn)],
			attachPosition=[(self.firstColumn, 'right', 0, 25),(self.secondColumn, 'right', 0, 50)]
			)


		#fin template
		pm.setUITemplate( popTemplate=True )

		#show window
		self.mainWin.show()

		#init state
		self.prepareDefaultCB()



	@browser.action
	def opemMailingManagerCB(self):
		project = browser.selected_name(self.projTextScroll)
		if not project:
			pm.warning('Please select a project.')
			return False
		return dmail.DMailingListManager().openWin(parent=self.mainWin,
			project_path=self.getProjectPath(project_name=project))


	def getProjectPath(self, project_name=None):
		"""Resolve the view's retained root/choice, never another window's setting."""
		project = browser.selected_name(self.projTextScroll) if project_name is None else project_name
		return asset_paths.project_path(self._project_root, project)




	@browser.action
	def changeRootCB(self):
		path = table_selection.selected_text(pm.fileDialog2(
			caption='Set Root Project Path', okCaption='Set', fileMode=2, dialogStyle=2))
		if not path:
			return False
		if not dasset.setAssetUserDataDic(userDic={dasset.root_path_key: path, dasset.current_project_key: ''}):
			return False
		return self.refleshProjectCB(preserve_selection=False)


	@browser.action
	def changeProjectSelectCB(self):
		project = browser.selected_name(self.projTextScroll)
		items = dasset.getAssetTypeListInProject(project_path=self.getProjectPath(project_name=project)) if project else []
		items, selected = browser.list_plan('', items)
		if not dasset.setAssetUserDataDic(userDic={dasset.root_path_key: self._project_root,
				dasset.current_project_key: project}):
			return False
		browser.replace_items(self, self.classTextScroll, items, selected)
		browser.replace_items(self, self.assetTextScroll, ())
		self._loaded_asset_context = (self._project_root, project, '')
		self.clearExportFiletextFieldCB()
		return True


	@browser.action
	def changeSelectAssetTypeCB(self):
		return self.refleshAssetListCB()


	@browser.action
	def changeSelectAssetCB(self):
		return self.refleshExportFileNameTextCB()


	@browser.action
	def refleshAssetTypeListCB(self, asset_type=None):
		project = browser.selected_name(self.projTextScroll)
		previous = browser.selected_name(self.classTextScroll) if asset_type is None else asset_type
		items = dasset.getAssetTypeListInProject(project_path=self.getProjectPath(project_name=project)) if project else []
		items, selected = browser.list_plan(previous, items)
		if not dasset.setAssetUserDataDic(userDic={dasset.root_path_key: self._project_root,
				dasset.current_project_key: project}):
			return False
		browser.replace_items(self, self.classTextScroll, items, selected)
		return self.refleshAssetListCB()


	@browser.action
	def refleshAssetListCB(self):
		project = browser.selected_name(self.projTextScroll)
		category = browser.selected_name(self.classTextScroll)
		previous = browser.selected_name(self.assetTextScroll)
		context = (self._project_root, project, category)
		if getattr(self, '_loaded_asset_context', None) != context:
			previous = ''
		items = dasset.getAssetList(assetType=category,
			project_path=self.getProjectPath(project_name=project)) if project and category else []
		items, selected = browser.list_plan(previous, items, pm.textField(self.assetFilterText, query=True, text=True))
		browser.replace_items(self, self.assetTextScroll, items, selected)
		self._loaded_asset_context = context
		return self.refleshExportFileNameTextCB()




	@browser.action
	def makeProjectCB(self):
		_LOGGER.debug('Opening project creation UI')
		return dasset.DMakeProject().openWin(refWin=self, parentWin=self.m_uiname,
			project_root=self._project_root)


	@browser.action
	def makeAssetTypeCB(self):
		project = browser.selected_name(self.projTextScroll)
		if not project:
			pm.warning('Please select a project.')
			return False
		root = creation.snapshot_root(dasset.getAssetFolderPath(project_path=self.getProjectPath(project_name=project)))
		return dasset.DMakeAssetClass().openWin(refWin=self, parentWin=self.m_uiname, asset_root=root)


	@browser.action
	def makeAssetCB(self):
		project = browser.selected_name(self.projTextScroll)
		category = browser.selected_name(self.classTextScroll)
		if not project or not category:
			pm.warning('Please select a project and category.')
			return False
		root = creation.snapshot_root(dasset.getAssetFolderPath(project_path=self.getProjectPath(project_name=project)))
		result = pm.promptDialog(title='New Asset', message='Enter Name:\n{0}'.format(root.path), button=['Create', 'Cancel'],
			defaultButton='Create', cancelButton='Cancel', dismissString='Cancel')
		if result != 'Create':
			return False
		name = asset_paths.validate_component(pm.promptDialog(query=True, text=True), 'asset name')
		if not dasset.createNewAssetFolder(assetType=category, assetName=name, asset_root=root):
			pm.warning('Failed to create the asset folder.')
			return False
		return self.refleshAssetListCB()




	@browser.action
	def refleshProjectCB(self, preserve_selection=True):
		if not isinstance(preserve_selection, bool):
			raise TypeError('Selection-preservation option must be Boolean')
		previous = browser.selected_name(self.projTextScroll) if preserve_selection else ''
		root = core_paths.resolve(dasset.getAssetProjectRootPath())
		if root != self._project_root:
			previous = ''
		items, selected = browser.list_plan(previous, dasset.getAssetTypeProjectList(project_root=root),
			pm.textField(self.projectFilterText, query=True, text=True))
		try:
			browser.replace_items(self, self.projTextScroll, items, selected)
		except BaseException:
			# A partially replaced list must not dispatch using its former root.
			self._project_root = None
			raise
		self._project_root = root
		return self.changeProjectSelectCB()





	def clearExportFiletextFieldCB(self):
		u"""filterのテキストフィールドをクリア"""
		pm.text(self.exportNameText, e=1, label='')


	@browser.action
	def refleshExportFileNameTextCB(self):
		category = browser.selected_name(self.classTextScroll)
		asset = browser.selected_name(self.assetTextScroll)
		project = browser.selected_name(self.projTextScroll)
		filename = ''
		if project and category and asset:
			extension = pm.optionMenu(self.fileTypeMenu, query=True, value=True)
			if extension not in asset_metadata.ALLOWED_EXTENSIONS:
				raise ValueError('Unsupported asset file type')
			filename = dasset.generateAssetFileName(assetType=category, assetName=asset, fileType=extension,
				project_path=self.getProjectPath(project_name=project))
		pm.text(self.exportNameText, edit=True, label=filename)
		return True

	'''
	def setProjectPathFromDialogCB(self):
		dirs = pm.fileDialog2(caption='ProjectPath', okCaption='Set', fileMode=2, dialogStyle=2)

		if dirs is None or len(dirs)==0 :
			return

		if len(dirs[0])>0 :
			pm.textField(self.projectPathText, e=1, text=dirs[0])
	'''

	'''
	def setProjectPathCB(self):
		projPath = dws.getProjectPath()
		pm.textField(self.projectPathText, e=1, text=projPath)
	'''

	'''
	def setProjectIDCB(self):
		proId = dasset.getSimplePorjectCode(codeNum=2)
		pm.textField(self.projectIDText, e=1, text=proId)
	'''


	@browser.action
	def setRootNodeCB(self):
		selected = pm.ls(selection=True)
		selected = [] if selected is None else selected
		if not isinstance(selected, (list, tuple)) or len(selected) != 1:
			pm.warning('Select exactly one export root.')
			return False
		pm.textField(self.rootText, edit=True, text=parameters.node_label(selected[0]))
		return True


	@browser.action
	def getCategory(self):
		return browser.selected_name(self.classTextScroll)


	def getState(self):
		u"""現在選択されているステートタイプを取得"""
		#stateStrList = pm.textScrollList( self.stateTextScroll, q=1, selectItem=1)
		stateType = pm.optionMenu(self.stateTypeMenu, q=1, value=1)
		return stateType


	def getTimeRangeCB(self):
		u"""現在のタイムレンジを取得"""
		start = pm.intField(self.startTime, q=1, value=1)
		end = pm.intField(self.endTime, q=1, value=1)
		return [start,end]


	def setTimerangeFromTimeSliderCB(self):
		u"""現在のタイムレンジをセット"""
		pm.intField(self.startTime, e=1, value=pm.playbackOptions(q=1,min=1))
		pm.intField(self.endTime, e=1, value=pm.playbackOptions(q=1,max=1))


	def setUserNameCB(self):
		u"""ユーザーの名前をセット"""
		username = dasset.getUserName()
		pm.textField(self.CreatorText, e=1, text=username)



	# def setFileNameCB(self):
	# 	fineName = dws.getCurrentFileName()
	# 	pm.textField(self.assetNameText, e=1, text= fineName.split('.')[0])



	def prepareDefaultCB(self):
		u"""ウィンドウを開いた際に最初に実行されるメソッド"""


		#Reflesh project, category, asset
		self.refleshProjectCB()
		self.refleshAssetTypeListCB()
		self.refleshAssetListCB()

		#project path
		# self.setProjectPathCB()

		#projectID
		# self.setProjectIDCB()

		#file name
		# self.setFileNameCB()

		#username
		self.setUserNameCB()

		#get timerange
		self.setTimerangeFromTimeSliderCB()

		#select Category
		# num = pm.textScrollList( self.classTextScroll, q=1,numberOfItems=1)
		# print num
		# if num>0 :
		# 	pm.textScrollList( self.classTextScroll, e=1, selectIndexedItem=1)

		#select State
		#pm.textScrollList(self.stateTextScroll, e=1, selectIndexedItem=1)

		#set send notif state
		stateNotif = dasset.getNotifState()
		# print ('State notif:{notif}'.format(notif=stateNotif))
		pm.checkBox(self.sendMailTask, e=1, value=stateNotif )



	def checkProblemCB(self):
		u"""Report duplicate short node names without modifying the scene."""
		duplicate_names = sorted(set(dname.checkUniqueName(isRename=None)))
		if duplicate_names:
			print ('Duplicate short names ({0}):'.format(len(duplicate_names)))
			for node_name in duplicate_names:
				print ('  {0}'.format(node_name))
			pm.warning(
				'Found {0} duplicate short node name(s).'.format(
					len(duplicate_names)))
		else:
			print ('No duplicate short node names found.')
		return duplicate_names



	def updateExportOptionState(self):
		u"""エクスポート時のオプションの状態を保存"""


		#save send mail state
		isSendMailTask = pm.checkBox(self.sendMailTask, q=1, value=1 )
		dasset.setAssetUserDataDic(userDic={dasset.notif_send_email_key:isSendMailTask})


	def _prepare_export(self):
		"""Read and admit every export input before filesystem/selection mutation."""
		top_text = table_selection.cell_text(pm.textField(self.rootText, q=True, text=True))
		if not top_text or any(ord(character) < 32 for character in top_text):
			raise ValueError('Enter one valid export root node')
		top_node = pm.PyNode(top_text)
		if not hasattr(top_node, 'nodeType'):
			raise ValueError('Export root must identify a node, not an attribute')
		top_name = parameters.node_label(top_node)
		asset_type = asset_paths.validate_component(table_selection.cell_text(
			pm.textScrollList(self.classTextScroll, q=True, selectItem=True)), 'asset type')
		asset_name = asset_paths.validate_component(table_selection.cell_text(
			pm.textScrollList(self.assetTextScroll, q=True, selectItem=True)), 'asset name')
		project_name = asset_paths.validate_component(table_selection.cell_text(
			pm.textScrollList(self.projTextScroll, q=True, selectItem=True)), 'project name')
		project_path = self.getProjectPath(project_name=project_name)
		extension = pm.optionMenu(self.fileTypeMenu, q=True, value=True)
		textures = attribute_values.numeric_value(pm.checkBox(self.checkExportFile, q=True, value=True), 'bool')
		thumbnail = attribute_values.numeric_value(pm.checkBox(self.checkThumbnail, q=True, value=True), 'bool')
		notify = attribute_values.numeric_value(pm.checkBox(self.sendMailTask, q=True, value=True), 'bool')
		asset_root = dasset.getAssetPath(assetType=asset_type, assetName=asset_name, project_path=project_path)
		export_path = dasset.getExportPath(assetType=asset_type, assetName=asset_name, project_path=project_path)
		plan = export_plan.build(asset_root, export_path, asset_name, extension,
			self.getTimeRangeCB(), pm.textField(self.CreatorText, q=True, text=True),
			pm.textField(self.commentField, q=True, text=True),
			datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S'), thumbnail)
		plan.update(top_node=top_node, top_name=top_name, thumbnail=thumbnail,
			textures=textures and extension in ('.ma', '.mb'), notify=notify,
			asset_type=asset_type, asset_name=asset_name, project_name=project_name,
			project_path=project_path, recipients=(), notification_error='')
		if notify:
			try:
				plan['recipients'] = table_selection.text_items(
					dasset.getMailAddressFromML(project_path=project_path))
			except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
				# Optional notification failure must not abort an otherwise valid export.
				plan['notification_error'] = str(error)
		if plan['textures']:
			if dmat.isUnResolveFiles():
				raise ValueError('Resolve missing texture files before export')
			dmat.validate_texture_edits()
		if extension in ('.anim', '.danim'):
			hierarchy = components.component_index(pm.radioButtonGrp(self.animHierarchyRadio, q=True, select=True))
			if hierarchy not in (1, 2):
				raise ValueError('Unsupported animation hierarchy mode')
			plan['hierarchy'] = hierarchy
			if extension == '.anim':
				objects = [top_node] if hierarchy == 1 else selOp.getObjectInHierarchy(top_node)
				if not aniOp.isAnimationExist(objects):
					raise ValueError('The export root does not contain animation')
		if extension == '.abc':
			flags = []
			for control, flag in ((self.isRenderableOnlyCheck, '-renderableOnly'),
					(self.isWriteVisibilityCheck, '-writeVisibility'),
					(self.isWholeFrameGeoCheck, '-wholeFrameGeo'), (self.isUVWriteCheck, '-uvWrite'),
					(self.isWorldSpaceCheck, '-worldSpace'), (self.isNoNormalCheck, '-noNormals')):
				if attribute_values.numeric_value(pm.checkBoxGrp(control, q=True, value1=True), 'bool'):
					flags.append(flag)
			plan['abc_job'] = alembic_job.build_export_job(plan['timerange'][0], plan['timerange'][1],
				1.0, plan['record']['filepath'], roots=[top_name], flags=flags)
		records = asset_repository.load_version_records(plan['metadata_path'], asset_root=asset_root)
		asset_metadata.validate_version_records(records + [plan['record']], asset_root=asset_root)
		return plan

	def _dispatch_export(self, plan):
		"""Native serializers keep their historical format/options; no Undo is implied."""
		extension, output = plan['record']['extension'], plan['record']['filepath']
		if os.name == 'nt':
			output = output.replace('\\', '/')
		pm.select(plan['top_node'], replace=True)
		if extension in ('.ma', '.mb'):
			om.MFileIO().exportSelected(output, exportFileTypeDic[extension])
		elif extension == '.abc':
			cmds.AbcExport(verbose=0, j=plan['abc_job'])
		elif extension == '.anim':
			hierarchy = 'none' if plan['hierarchy'] == 1 else 'below'
			# Preserve legacy animExport range/options until Maya behavior is verified.
			options = ('precision=8;intValue=17;nodeNames=1;verboseUnits=0;whichRange=1;range=0:10;'
				'options=keys;hierarchy={0};controlPoints=0;shapes=1;helpPictures=0;useChannelBox=0;'
				'copyKeyCmd=-animation objects -option keys -hierarchy {0} -controlPoints 0 -shape 1 ').format(hierarchy)
			cmds.file(output, force=1, options=options, typ=exportFileTypeDic[extension],
				preserveReferences=1, exportSelected=1)
		elif extension == '.danim':
			if not danimIO.d_animWrite(filename=output, hi=0 if plan['hierarchy'] == 1 else 1):
				raise RuntimeError('DAnim writer did not confirm success')

	def _notify_export(self, plan):
		if not plan['notify']:
			return
		if plan['notification_error']:
			pm.warning('Asset export succeeded, but notification preparation failed: {0}'.format(
				plan['notification_error']))
			return
		project, recipients = plan['project_name'], plan['recipients']
		if not recipients:
			return
		record = plan['record']
		title = '{0} | {1} | {2}'.format(project, plan['asset_type'], plan['asset_name']).upper()
		message = 'version:{0}\ncreator:{1}\ncomment:{2}\n\n\n'.format(
			record['version'], record['creator'], record['comment'])
		try:
			smtp_client.send_notification(title=title, message=message, address_list=recipients)
		except smtp_client.MailError as error:
			pm.warning('Asset export succeeded, but the notification was not sent: {0}'.format(error))

	def runExportCB(self):
		"""Export with complete admission and independent texture/selection restoration."""
		selected = None
		restored = True
		try:
			plan = self._prepare_export()
			current = pm.ls(selection=True) or []
			if not isinstance(current, (list, tuple)) or len(current) > parameters.MAX_CONNECTIONS:
				raise ValueError('Previous selection exceeds the supported snapshot limit')
			selected = tuple(current)
			extension = plan['record']['extension']
			plugin = {'abc': 'AbcExport', 'anim': 'animImportExport'}.get(extension[1:])
			if plugin and not dsi.isLoadedPlugin(pluginName=plugin):
				if not dsi.loadMayaPlugin(pluginName=plugin):
					return False
			exportPath, isSaveThumbnail = plan['export_path'], plan['thumbnail']
			if not dpath.openCreateDirectoryDialog(dirpath=exportPath,
					messase='Do you create new version?\n{0}'.format(exportPath), title='New Version Asset'):
				return False
			thumbnail_path = ''
			if isSaveThumbnail:
				try:
					thumbnail_path = dview.captureThumbnail(filepath=exportPath, filename='thumbnail', ext='png')
					if not thumbnail_path:
						raise RuntimeError('Thumbnail capture did not confirm a saved file')
				except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
					pm.warning('Asset export stopped before scene/texture export: {0}'.format(error))
					return False
			if isSaveThumbnail and core_paths.resolve(thumbnail_path) != core_paths.resolve(plan['record']['thumbnail']):
				raise ValueError('Thumbnail capture returned an unexpected destination')
			plan['record']['thumbnail'] = thumbnail_path
			dpath.mkdir(plan['scenes'])
			dpath.mkdir(plan['sourceimages'])
			if plan['textures']:
				mapping = dmat.copyFileNodeImageToFolder(copyFolder=plan['sourceimages'])
				with dmat.temporary_file_paths(path_mapping=mapping, verbose=False):
					self._dispatch_export(plan)
			else:
				self._dispatch_export(plan)
			# Commit metadata only after every temporary texture path restored.
			record = asset_metadata.validate_version_record(plan['record'],
				asset_root=plan['asset_root'], require_file=True)
			records = asset_repository.load_version_records(plan['metadata_path'], asset_root=plan['asset_root'])
			asset_repository.save_version_records(plan['metadata_path'], records + [record], asset_root=plan['asset_root'])
			self.updateExportOptionState()
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
			pm.warning('Asset export did not complete every step; files/metadata may already exist: {0}'.format(error))
			return False
		finally:
			if selected is not None:
				restored = selOp.restore_selection(selected)
		if not restored:
			return False
		try:
			self._notify_export(plan)
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
			pm.warning('Asset export succeeded, but notification preparation failed: {0}'.format(error))
		return True
