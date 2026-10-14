# -*- coding: utf-8 -*-
u"""Asset project operations and legacy Maya UI components."""

import pymel.core as pm
import maya.mel as mel


import DPyTools.system.DWorkspace as dws
import DPyTools.system.DPathOp as dpath
from DPyTools.core import diagnostics
from DPyTools.pipeline.assets import paths as asset_paths
from DPyTools.pipeline.assets import naming as asset_naming
from DPyTools.pipeline.assets import repository as asset_repository
from DPyTools.pipeline.assets import user_data
from DPyTools.pipeline.assets import versioning
from DPyTools.pipeline.assets import settings as asset_settings
from DPyTools.pipeline.assets import users as dusers
from DPyTools.pipeline.assets import browser
from DPyTools.pipeline.assets import creation
from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection
from DPyTools.scene import transforms as scene_transforms

import re
import getpass


_LOGGER = diagnostics.get_logger(__name__)



'''
to run:
>>> import DPyTools.pipeline.assets.operations as dasset

'''

#default geometry type list
mayaGeoDic = {'mesh':'_msh', 'nurbsCurve':'_crv', 'nurbsSurface':'_srf'}
#mayaGeoList = ['mesh', 'nurbsCurve', 'nurbsSurface']
#mayaConstList = ['parentConstraint', 'pointConstraint', 'orientConstraint', 'scaleConstraint', 'aimConstraint', 'poleVectorConstraint']
mayaConstDic = {'parentConstraint':'_prCons',
				'pointConstraint':'_posCons',
				'orientConstraint':'_ortCons',
				'scaleConstraint':'_sclCons',
				'aimConstraint':'_aimCons',
				'poleVectorConstraint':'_pvecCons'
				}
typeTagList = ['_grp','_ctrl','_geo','_msh','_crv','_srf','_set','_light','_mtl','_cache','_null']
sideTagList = ['_ul','_u','_ur','_l','_c','_r','_dl','_d','_dr']
sideTagCheckList = ['_ul','_ur','_dl','_dr','_d','_u','_l','_c','_r']#for rename

transTagList = ['_master', '_trans', '_rot', '_scale', '_aux', '_dummy']

#default asset type list
classList = ['CH', 'BIND', 'RIG', 'ANIM', 'ENV', 'BG', 'PROPS', 'FX', 'LIGHT', 'CAM', 'CACHE' ]
assetClassDic = {'CH':'Charactor', 'BIND':'Skin Bind', 'RIG':'Rigged', 'ANIM':'Animation Curve', 'ENV':'Environment', 'BG':'Background',
				'PROPS':'Prop', 'FX':'Effect', 'LIGHT':'Light', 'CAM':'Camera', 'CACHE':'Cache' }
#default state list
stateList = ['Proc','Mod','Anim','Fix', 'Check', 'Fin']



# Local project roots must not be embedded in source control.
rootProjectPath = asset_paths.default_project_root()

#vfx folder name
vfxFolder = 'works'

#mailing_list path
mailingList_file = '.proj_ml'

#asset user data file name
user_asset_file = '.dasset'
#asset project asset info file name
proj_asset_file = '.proj_dasset'


root_path_key = asset_settings.ROOT_PATH_KEY
username_key = asset_settings.USERNAME_KEY
asset_type_key = asset_settings.ASSET_TYPE_KEY
current_project_key = asset_settings.CURRENT_PROJECT_KEY
# A new key prevents legacy default-true preferences from silently opting in.
notif_send_email_key = asset_settings.NOTIFICATION_KEY




def deleteUnUsedNodes():
	u"""使用されていないシェーダーノードの削除"""

	mel.eval('MLdeleteUnused;')



def createProjectFolderStructure(path='', projectName=''):
	u"""Create the complete admitted layout before reporting success."""
	try:
		plan = creation.project_plan(path, projectName, content_name=vfxFolder)
		creation.execute(plan)
	except (IOError, OSError, TypeError, ValueError) as error:
		pm.warning('Project creation did not complete: {0}'.format(error))
		return False
	pm.confirmDialog(title='New Project', message='Project is created!\n{0}'.format(plan.targets[0]),
		button=['OK'], defaultButton='OK', cancelButton='OK', dismissString='OK')
	return True


def getAssetUserFileName():
	u"""ユーザーのアセットの情報ファイルの名前を取得"""
	return dpath.append(path=dpath.home(), other=user_asset_file)


def getAssetProjectFileName(path=None):
	u"""利用するアセットのベースとなるプロジェクトのディレクトリネームの取得"""
	if path :
		return dpath.append(path=path, other=proj_asset_file)
	else :
		return dpath.append(path=getAssetProjectRootPath(), other=proj_asset_file)



def createDefaultAssetUserData():
	u"""Create defaults only when no previous settings path exists."""
	return asset_repository.create_settings(
		getAssetUserFileName(), _defaultAssetUserData())


def _defaultAssetUserData():
	return asset_settings.default_settings(rootProjectPath, getUserName())




def isExistUserAssetFile():
	u"""ユーザーのアセット情報ファイルのが存在するか確認"""
	return dpath.isFile(getAssetUserFileName())


def readUserAssetData():
	u"""ユーザーのアセット情報ファイルを読んで、ディクショナリーを返す"""
	defaults = _defaultAssetUserData()
	try:
		return asset_repository.load_settings(getAssetUserFileName(), defaults)
	except (IOError, OSError, TypeError, ValueError) as exc:
		pm.warning('Invalid asset settings; using safe defaults: {0}'.format(exc))
		return defaults


def updateUserAssetData(dic=None):
	u"""ユーザーのアセット情報ファイルを更新"""
	if dic is None:
		dic = {}
	return asset_repository.update_settings(
		getAssetUserFileName(), _defaultAssetUserData(), dic, replace=True)




def getAssetUserDataDic(key_list=None):
	u"""ユーザーのアセット情報ファイルのディクショナリーを返す"""
	if key_list is None:
		key_list = []

	if isExistUserAssetFile() :
		data = readUserAssetData()

		dataDic = {}
		for key in data.keys() :
			if key in key_list :
				dataDic[key] = data[key]

		return dataDic

	#return default
	return {}


def getNotifState() :

	data = getAssetUserDataDic(key_list=[notif_send_email_key])

	if notif_send_email_key in data.keys() :
		value = data[notif_send_email_key]
		return isinstance(value, bool) and value
	else :
		return False

def setAssetUserDataDic(userDic=None):
	u"""ユーザーのアセット情報ファイルをディクショナリーを利用してアップデート。"""
	if userDic is None:
		userDic = {}

	try:
		asset_repository.update_settings(
			getAssetUserFileName(), _defaultAssetUserData(), userDic)
	except (IOError, OSError, TypeError, ValueError) as exc:
		pm.warning('Asset settings could not be updated: {0}'.format(exc))
		return False
	return True


def getAssetProjectRootPath():
	u"""アセットを読み込むベースとなるプロジェクトが格納されているルートパスを設定"""

	dataDic = getAssetUserDataDic(key_list=[root_path_key])

	if root_path_key in dataDic.keys() :
		return dataDic[root_path_key]

	#return default path
	return rootProjectPath


def getAssetCurrentProjectPath(project_name=None):
	u"""現在セットされているプロジェクトのパスを返す"""

	dataDic = getAssetUserDataDic(key_list=[root_path_key, current_project_key])

	if root_path_key in dataDic.keys() and (project_name is not None or current_project_key in dataDic.keys()):
		rootPath = dataDic[root_path_key]
		currentProj = dataDic[current_project_key] if project_name is None else project_name

		projectPath = asset_paths.project_path(rootPath, currentProj)
		if projectPath:
			return projectPath

	return None


def getAssetTypeList():
	u"""現在セットされているプロジェクトのアセットのタイプリストを返す"""

	dataDic = getAssetUserDataDic(key_list=[asset_type_key])

	if asset_type_key in dataDic.keys() :
		return dataDic[asset_type_key]

	#return default path
	return classList


def getAssetFolderPath(project_name=None, project_path=None):
	u"""現在セットされているプロジェクトのアセットのパスを返す"""


	project = (getAssetCurrentProjectPath(project_name) if project_path is None
		else asset_paths.existing_project_path(project_path))
	return asset_paths.asset_root(project)


def getMaillingListFile(project_path=None):
	u"""現在セットされているプロジェクトのメーリングリストのファイルパスを返します"""

	#get project path
	projPath = (getAssetCurrentProjectPath() if project_path is None
		else asset_paths.existing_project_path(project_path))

	if projPath and dpath.isDir(projPath) :
		mailListPath = asset_paths.child_path(projPath, mailingList_file, field_name='mailing list filename')

		return mailListPath

	return None


def getMaillingList(project_path=None):
	u"""現在セットされているプロジェクトのメーリングリストを返します"""

	#get project path
	jsonFile = getMaillingListFile(project_path=project_path)

	m_list = []
	#check data file
	if jsonFile and dpath.isFile(jsonFile):
		try:
			m_list = user_data.validate_mailing_list(
				asset_repository.load_list(jsonFile))
		except (IOError, OSError, TypeError, ValueError) as exc:
			pm.warning('Invalid project mailing list: {0}'.format(exc))

	return m_list


def addUserMailingList(useid='', project_path=None):
	u"""Compatibility API for one ID; use the same complete batch commit path."""
	return addUsersMailingList(userids=[useid], project_path=project_path)


def addUsersMailingList(userids=None, project_path=None):
	u"""Validate one snapshot and commit all requested IDs in one atomic save."""
	if userids is None:
		userids = []
	try:
		jsonFile = getMaillingListFile(project_path=project_path)
		if not jsonFile:
			raise ValueError('No project mailing-list destination is configured')
		data = asset_repository.load_list(jsonFile) if dpath.isFile(jsonFile) else []
		merged = user_data.merge_mailing_list(data, userids, dusers.getUserList())
		if merged != data:
			asset_repository.save_mailing_list(jsonFile, merged)
		return True
	except (IOError, OSError, TypeError, ValueError) as exc:
		pm.warning('Mailing list could not be updated: {0}'.format(exc))
		return False




def deleteUserMailningList(userid='', project_path=None):
	u"""Delete one validated mailing ID and report whether saving succeeded."""
	try:
		userid = user_data.validate_user_id(userid)
		jsonFile = getMaillingListFile(project_path=project_path)
		if not jsonFile or not dpath.isFile(path=jsonFile):
			return False
		data = user_data.validate_mailing_list(asset_repository.load_list(jsonFile))
		if userid not in data:
			return False
		data.remove(userid)
		asset_repository.save_mailing_list(jsonFile, data)
		return True
	except (IOError, OSError, TypeError, ValueError) as exc:
		pm.warning('Mailing list could not be updated: {0}'.format(exc))
		return False


def getMailAddressFromML(project_path=None):
	u"""現在セットされているプロジェクトのメーリングリストのアドレスを返します"""

	mailingList = getMaillingList(project_path=project_path)
	userlist = dusers.getUserList()
	return user_data.mailing_addresses(mailingList, userlist)

def getShotFolderPath():
	u"""現在セットされているプロジェクトのカットのパスを返す"""
	u"""カットフォルダーは現状では基本worksの直下に置くことにしています"""

	return asset_paths.shot_root(getAssetCurrentProjectPath())



def getAssetTypeProjectList(project_root=None):
	u"""現在のプロジェクトのリストを返す"""

	projPath = getAssetProjectRootPath() if project_root is None else project_root

	if dpath.isDir(projPath) :
		return asset_paths.list_child_directories(projPath)

	return []



def getAssetTypeListInProject(project_name=None, project_path=None):
	u"""現在のプロジェクトのアセットのタイプリストを返す"""

	projPath = getAssetFolderPath(project_name, project_path=project_path)
	#print ('projPath:{path}'.format(path=projPath))

	if projPath and dpath.isDir(projPath) :
		return asset_paths.list_child_directories(projPath)

	return []


def createNewAssetFolder(assetType='', assetName='', asset_root=None):
	u"""Create in one retained asset root, preserving legacy caller defaults."""
	try:
		root = getAssetFolderPath() if asset_root is None else asset_root
		creation.execute(creation.asset_plan(root, assetType, assetName))
	except (IOError, OSError, TypeError, ValueError) as error:
		pm.warning('Asset creation did not complete: {0}'.format(error))
		return False
	return True

def getAssetList(assetType='', project_name=None, project_path=None):
	u"""アセットのリストの取得"""

	assetTypePath = getAssetFolderPath(project_name, project_path=project_path)

	if assetTypePath and dpath.isDir(assetTypePath) :
		try:
			path = asset_paths.child_path(assetTypePath, assetType, field_name='asset type')
		except (TypeError, ValueError):
			return []

		if path and dpath.isDir(path) :
			return asset_paths.list_child_directories(path)

	return []

def getAssetPath(assetType='', assetName='', project_name=None, project_path=None):
	u"""アセットのパスを取得"""


	assetTypePath = getAssetFolderPath(project_name, project_path=project_path)

	if assetTypePath and dpath.isDir(assetTypePath) :
		try:
			assetPath = asset_paths.child_path(assetTypePath, assetType, field_name='asset type')
			return asset_paths.child_path(assetPath, assetName, field_name='asset name')
		except (TypeError, ValueError):
			return None


def getAssetVersionList(assetType='', assetName='', project_name=None, project_path=None):
	u"""アセットのバージョンリストを取得"""

	assetPath = getAssetPath(assetType=assetType, assetName=assetName, project_name=project_name, project_path=project_path)

	if assetPath and dpath.isDir(assetPath) :
		return asset_paths.list_child_directories(assetPath)

	return []


def generateAssetFileName(assetType='', assetName='', fileType='.ma', project_name=None, project_path=None):
	u"""アセットのバージョンリストを取得"""
	assetPath = getAssetPath(assetType=assetType, assetName=assetName, project_name=project_name, project_path=project_path)


	if assetPath and dpath.isDir(assetPath) :
		ver_list = getAssetVersionList(assetType=assetType, assetName=assetName, project_name=project_name, project_path=project_path)
		try:
			version = versioning.next_version(ver_list)
		except ValueError as exc:
			pm.warning(str(exc))
			return ''
		#print ('version:{f}'.format(f=version))

		versionPath = dpath.append( assetPath , version )
		#print ('versionPath:{f}'.format(f=versionPath))

		#get file type
		#print ('fileType:{f}'.format(f=fileType))

		finFileName = assetName+'_'+version+fileType
		#print ('finFileName:{f}'.format(f=finFileName))

		#generate final path
		return finFileName

	return ''

def getExportPath(assetType='', assetName='', project_name=None, project_path=None):
	u"""アセットのエクスポートするパスの取得"""
	assetPath = getAssetPath(assetType=assetType, assetName=assetName, project_name=project_name, project_path=project_path)


	if assetPath and dpath.isDir(assetPath) :
		try:
			version = versioning.next_version(
				getAssetVersionList(assetType=assetType, assetName=assetName, project_name=project_name, project_path=project_path))
		except ValueError as exc:
			pm.warning(str(exc))
			return ''
		_LOGGER.debug('Reserved asset version: %s', version)

		versionPath = dpath.append( assetPath , version )
		return versionPath

	return ''


class DMakeProject(object):

	m_uiname = "DMakeProject"

	#delegate func
	func_finalAct = None


	def __init__(self):
		self.m_uiname = "DMakeProject"
		self.refWin = None
		self.func_finalAct = None
		self._creation_root = None

	@browser.action
	def openWin(self, refWin=None, parentWin=None, project_root=None):
		root = getAssetProjectRootPath() if project_root is None else project_root
		try:
			self._creation_root = creation.snapshot_root(root)
		except (IOError, OSError, TypeError, ValueError):
			if project_root is not None:
				raise
			# An unavailable default must still allow the user to choose a root.
			self._creation_root = None
			pm.warning('Choose an existing project root before creating a project.')

		#template
		template = pm.uiTemplate('makeProjectTemplate', force=True)
		template.define(pm.columnLayout, rowSpacing=10, adjustableColumn=True)

		#new window
		ui_windows.delete_window(self.m_uiname)

		win = None
		if parentWin:
			win = pm.window(self.m_uiname, widthHeight=(790, 320), parent=parentWin)
		else :
			win = pm.window(self.m_uiname, widthHeight=(790, 320))

		#set template
		pm.setUITemplate( 'makeProjectTemplate', pushTemplate=True )


		self.form = pm.formLayout(numberOfDivisions=100)
		self.mainColumn = pm.columnLayout()



		pm.rowLayout(numberOfColumns=2, adjustableColumn=1)
		self.projRootPathText = pm.text(label=rootProjectPath, bgc=[0.5,.0,.0], width=120, height=32)
		pm.button(label='Change',width=60, c=lambda *args: self.changeRootProjectPathCB(), height=32)
		pm.setParent( '..' )#rowLayout

		pm.text(label='')

		pm.text(label='Project Name', height=32)
		self.projNameText = pm.textField(placeholderText='Enter Project Name', height=32)


		pm.button(label='Create Project', c=lambda *args: self.createProjectCB(), height=32)



		#edit layout
		pm.formLayout( self.form, edit=True,
			attachPosition=[(self.mainColumn, 'top', 0, 10),(self.mainColumn, 'left', 0, 20),(self.mainColumn, 'right', 0, 80),(self.mainColumn, 'bottom', 0, 10)]
			)


		#fin template
		pm.setUITemplate( popTemplate=True )

		#show window
		win.show()

		#assign refresh window
		self.refWin = refWin

		#check asset user property
		label = self._creation_root.path if self._creation_root is not None else 'Choose an existing project root'
		pm.text(self.projRootPathText, e=1, label=label)
		return True



	@browser.action
	def changeRootProjectPathCB(self):
		path = table_selection.selected_text(pm.fileDialog2(
			caption='Set Root Project Path', okCaption='Set', fileMode=2, dialogStyle=2))
		if not path:
			return False
		root = creation.snapshot_root(path)
		if not setAssetUserDataDic(userDic={root_path_key: root.path, current_project_key: ''}):
			return False
		self._creation_root = root
		pm.text(self.projRootPathText, edit=True, label=root.path)
		return True


	@browser.action
	def createProjectCB(self):
		if self._creation_root is None:
			pm.warning('Choose an existing project root before creating a project.')
			return False
		name = table_selection.cell_text(pm.textField(self.projNameText, query=True, text=True))
		if not createProjectFolderStructure(path=self._creation_root, projectName=name):
			return False
		if not browser.notify(self.func_finalAct):
			pm.warning('Project was created, but its view could not be refreshed.')
			return False
		refresh = getattr(self.refWin, 'refleshProjectCB', None)
		if not browser.notify(refresh):
			pm.warning('Project was created, but its parent view could not be refreshed.')
			return False
		pm.deleteUI(self.m_uiname)
		return True






class DMakeAssetClass(object):

	m_uiname = "DMakeAssetClass"

	#delegate func
	func_finalAct = None

	def __init__(self):
		self.m_uiname = "DMakeAssetClass"
		self.refWin = None
		self.func_finalAct = None
		self._creation_root = None

	@browser.action
	def openWin(self, refWin=None, parentWin=None, asset_root=None):
		root = getAssetFolderPath() if asset_root is None else asset_root
		self._creation_root = creation.snapshot_root(root)

		#template
		template = pm.uiTemplate('makeAssetClassTemplate', force=True)
		template.define(pm.columnLayout, rowSpacing=10, adjustableColumn=True)

		#new window
		ui_windows.delete_window(self.m_uiname)

		win = None
		if parentWin:
			win = pm.window(self.m_uiname, widthHeight=(320, 240), parent=parentWin)
		else :
			win = pm.window(self.m_uiname, widthHeight=(320, 240))


		#set template
		pm.setUITemplate( 'makeAssetClassTemplate', pushTemplate=True )


		self.form = pm.formLayout(numberOfDivisions=100)
		self.mainColumn = pm.columnLayout()


		#######make menu method
		pm.text(label='Asset root: {0}'.format(self._creation_root.path), wordWrap=True)
		def makeAssetMenu(label='', checkList=None):
			if checkList is None:
				checkList = []
			pm.menuItem(label=label)

		#######make menu
		self.assetTypeMenu = pm.optionMenu( label='Asset Type', height=32)
		for m in classList :
			makeAssetMenu(label=m)


		pm.button(label='Create Asset Type', c=lambda *args: self.createAssetCB(), height=32)


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachPosition=[(self.mainColumn, 'top', 0, 10),(self.mainColumn, 'left', 0, 20),(self.mainColumn, 'right', 0, 80),(self.mainColumn, 'bottom', 0, 10)]
			)


		#fin template
		pm.setUITemplate( popTemplate=True )

		#show window
		win.show()

		#assign refresh window
		self.refWin = refWin
		return True



	@browser.action
	def createAssetCB(self):
		category = table_selection.cell_text(pm.optionMenu(self.assetTypeMenu, query=True, value=True))
		creation.execute(creation.category_plan(self._creation_root, category))
		if not browser.notify(self.func_finalAct):
			pm.warning('Category was created, but its view could not be refreshed.')
			return False
		refresh = getattr(self.refWin, 'refleshAssetTypeListCB', None)
		if refresh is not None:
			if not callable(refresh):
				raise TypeError('Category refresh delegate must be callable')
			result = refresh(asset_type=category)
			if isinstance(result, bool) and not result:
				pm.warning('Category was created, but its parent view could not be refreshed.')
				return False
		pm.deleteUI(self.m_uiname)
		return True







class DProjectTextScroll(object):
	u"""指定されたルートパス内にあるプロジェクトを表示するテキストスクロールUI"""

	#data member
	m_root_path = '/'
	m_current_project = ''

	#ui member
	m_parent_win = None
	m_form = None
	#m_column = None
	m_scroll_text = None

	#delegate func
	delegate_changeProj_fnc = None
	delegate_makeProj_fnc = None




	def __init__(self, parent=None, isActionButton=None ):
		self.m_root_path = getAssetProjectRootPath()
		self.m_current_project = ''
		self.m_form = pm.formLayout(numberOfDivisions=100, parent=parent)
		self.m_title = pm.text(label='Project')
		#self.m_column = pm.columnLayout(pm.columnLayout, rowSpacing=10, adjustableColumn=True, parent=parent)

		self.m_filter = pm.textField(placeholderText='Project Filter', searchField=1, textChangedCommand=lambda *args: self.refleshCB() )
		self.m_scroll_text = pm.textScrollList( allowMultiSelection=False, sc=lambda *args: self.changeSelectCB())

		self.under_controls = pm.rowLayout(numberOfColumns=4, adjustableColumn=3)
		pm.button(label='Root', c=lambda *args: self.changeRootCB())
		pm.button(label='Reload', c=lambda *args: self.refleshCB())
		pm.text(label='')
		pm.button(label='Make Project', c=lambda *args: self.makeProjectCB())
		# pm.button(label='Config')
		pm.setParent( '..' )#rowLayout

		#edit layout
		pm.formLayout( self.m_form, edit=True,
			attachForm=[(self.m_title, 'top', 5), (self.m_title, 'left', 5), (self.m_title, 'right', 5),
					(self.m_filter, 'left', 5), (self.m_filter, 'right', 5),
					(self.m_scroll_text, 'left', 5), (self.m_scroll_text, 'right', 5),
					(self.under_controls, 'left', 5), (self.under_controls, 'right', 5), (self.under_controls, 'bottom', 5)
					],
			attachControl=[(self.m_filter, 'top', 5, self.m_title),
							(self.m_scroll_text, 'top', 5, self.m_filter),
							(self.m_scroll_text, 'bottom', 5, self.under_controls)
							]
			)


		pm.setParent( parent )#rowLayout




	def getProjectPath(self):
		selected = browser.selected_name(self.m_scroll_text)
		if selected != self.m_current_project:
			return ''
		return asset_paths.project_path(self.m_root_path, selected)


	@browser.action
	def changeRootCB(self):
		path = table_selection.selected_text(pm.fileDialog2(
			caption='Set Root Project Path', okCaption='Set', fileMode=2, dialogStyle=2))
		if not path:
			return False
		if not setAssetUserDataDic(userDic={root_path_key: path, current_project_key: ''}):
			return False
		self.m_root_path, self.m_current_project = path, ''
		return self.refleshCB(preserve_selection=False)


	@browser.action
	def changeSelectCB(self):
		project = browser.selected_name(self.m_scroll_text)
		if not setAssetUserDataDic(userDic={root_path_key: self.m_root_path, current_project_key: project}):
			return False
		self.m_current_project = project
		return browser.notify(self.delegate_changeProj_fnc)

	@browser.action
	def refleshCB(self, preserve_selection=True):
		if not isinstance(preserve_selection, bool):
			raise TypeError('Selection-preservation option must be Boolean')
		previous = browser.selected_name(self.m_scroll_text) if preserve_selection else ''
		root = getAssetProjectRootPath()
		if root != self.m_root_path:
			previous = ''
		items, selected = browser.list_plan(previous, getAssetTypeProjectList(project_root=root),
			pm.textField(self.m_filter, query=True, text=True))
		if not setAssetUserDataDic(userDic={root_path_key: root, current_project_key: selected}):
			return False
		browser.replace_items(self, self.m_scroll_text, items, selected)
		self.m_root_path, self.m_current_project = root, selected
		return browser.notify(self.delegate_changeProj_fnc)

	@browser.action
	def makeProjectCB(self):
		u"""新しいプロジェクトの作成"""
		_LOGGER.debug('Opening project creation UI')
		makeProWin = DMakeProject()
		if not makeProWin.openWin(refWin=None, parentWin=self.m_parent_win, project_root=self.m_root_path):
			return False
		#DMakeProject().openWin(refWin=self, parentWin=self.m_uiname)

		makeProWin.func_finalAct = self.refleshCB
		return True

	@browser.action
	def isSelectProject(self):
		return bool(browser.selected_name(self.m_scroll_text))


class DAssetTypeTextScroll(object):
	u"""プロジェクト内の存在するアセットのカテゴリーフォルダを表示するテキストスクロールUI"""

	#data member
	m_current_project = ''

	#ui member
	m_parent_win = None
	m_form = None
	#m_column = None
	m_scroll_text = None

	#delegate func
	delegate_changeSelect_fnc = None
	delegate_makeAsset_fnc = None


	def __init__(self, parent=None, isActionButton=None ):
		self.project_path_getter = None
		self.m_form = pm.formLayout(numberOfDivisions=100, parent=parent)
		self.m_title = pm.text(label='Category')

		self.m_scroll_text = pm.textScrollList( allowMultiSelection=False, sc=lambda *args: self.changeSelectCB())

		self.under_controls = pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		pm.button(label='New Category', c=lambda *args: self.makeAssetTypeCB())
		pm.text(label='')
		pm.setParent( '..' )#rowLayout

		#edit layout
		pm.formLayout( self.m_form, edit=True,
			attachForm=[(self.m_title, 'top', 5), (self.m_title, 'left', 5), (self.m_title, 'right', 5),
					(self.m_scroll_text, 'left', 5), (self.m_scroll_text, 'right', 5),
					(self.under_controls, 'left', 5), (self.under_controls, 'right', 5), (self.under_controls, 'bottom', 5)
					],
			attachControl=[(self.m_scroll_text, 'top', 5, self.m_title),
							(self.m_scroll_text, 'bottom', 5, self.under_controls)
							]
			)


		pm.setParent( parent )#rowLayout


	@browser.action
	def getAssetType(self):
		return browser.selected_name(self.m_scroll_text)


	@browser.action
	def makeAssetTypeCB(self):
		u"""アセットの項目を追加します"""
		# print ('makeAssetTypeCB')
		makeAssetTypeWin = DMakeAssetClass()
		root = creation.snapshot_root(getAssetFolderPath(project_path=browser.project_path(self)))
		if not makeAssetTypeWin.openWin(refWin=None, parentWin=self.m_parent_win, asset_root=root):
			return False
		makeAssetTypeWin.func_finalAct = self.refleshCB
		return True


	@browser.action
	def changeSelectCB(self):
		browser.selected_name(self.m_scroll_text)
		return browser.notify(self.delegate_changeSelect_fnc)



	@browser.action
	def refleshCB(self, preserve_selection=True):
		if not isinstance(preserve_selection, bool):
			raise TypeError('Selection-preservation option must be Boolean')
		previous = browser.selected_name(self.m_scroll_text) if preserve_selection else ''
		items, selected = browser.list_plan(previous,
			getAssetTypeListInProject(project_path=browser.project_path(self)))
		browser.replace_items(self, self.m_scroll_text, items, selected)
		return browser.notify(self.delegate_changeSelect_fnc)



class DAssetTextScroll(object):
	u"""プロジェクト内の存在するアセットのリストを表示するテキストスクロールUI"""

	#data member
	m_current_project = ''
	m_asset=''

	#ui member
	m_parent_win = None
	m_form = None
	#m_column = None
	m_scroll_text = None

	#delegate func
	delegate_changeSelect_fnc = None
	delegate_makeAsset_fnc = None


	def __init__(self, parent=None, isActionButton=None ):
		self.m_asset_type, self.m_asset, self._loaded_asset_type = '', '', ''
		self.project_path_getter = None
		self._loaded_asset_context = None
		self.m_form = pm.formLayout(numberOfDivisions=100, parent=parent)
		self.m_title = pm.text(label='Asset')

		self.m_filter = pm.textField(placeholderText='Asset Filter', searchField=1, textChangedCommand=lambda *args: self.refleshCB() )
		self.m_scroll_text = pm.textScrollList( allowMultiSelection=False,
												sc=lambda *args: self.changeSelectCB())

		self.under_controls = pm.rowLayout(numberOfColumns=2, adjustableColumn=2)
		pm.button(label='New Asset', c=lambda *args: self.makeAssetCB())
		pm.text(label='')
		pm.setParent( '..' )#rowLayout

		#edit layout
		pm.formLayout( self.m_form, edit=True,
			attachForm=[(self.m_title, 'top', 5), (self.m_title, 'left', 5), (self.m_title, 'right', 5),
					(self.m_filter, 'left', 5), (self.m_filter, 'right', 5),
					(self.m_scroll_text, 'left', 5), (self.m_scroll_text, 'right', 5),
					(self.under_controls, 'left', 5), (self.under_controls, 'right', 5), (self.under_controls, 'bottom', 5)
					],
			attachControl=[(self.m_filter, 'top', 5, self.m_title),
							(self.m_scroll_text, 'top', 5, self.m_filter),
							(self.m_scroll_text, 'bottom', 5, self.under_controls)
							]
			)


		pm.setParent( parent )#rowLayout


	def getAssetName(self):
		_LOGGER.debug('Reading asset name')

		return self.m_asset





	@browser.action
	def makeAssetCB(self):
		category = self.m_asset_type
		if not category:
			return False
		asset_paths.validate_component(category, 'asset category')
		root = creation.snapshot_root(getAssetFolderPath(project_path=browser.project_path(self)))
		result = pm.promptDialog(title='New Asset', message='Enter Name:\n{0}'.format(root.path), button=['Create', 'Cancel'],
			defaultButton='Create', cancelButton='Cancel', dismissString='Cancel')
		if result != 'Create':
			return False
		name = asset_paths.validate_component(pm.promptDialog(query=True, text=True), 'asset name')
		if not createNewAssetFolder(assetType=category, assetName=name, asset_root=root):
			return False
		return self.refleshCB()


	@browser.action
	def changeSelectCB(self):
		self.m_asset = browser.selected_name(self.m_scroll_text)
		return browser.notify(self.delegate_changeSelect_fnc)



	@browser.action
	def refleshCB(self):
		previous = browser.selected_name(self.m_scroll_text)
		project = browser.project_path(self)
		context = (project, self.m_asset_type)
		if getattr(self, '_loaded_asset_context', None) != context:
			previous = ''
		items = getAssetList(assetType=self.m_asset_type, project_path=project) if self.m_asset_type else []
		items, selected = browser.list_plan(previous, items, pm.textField(self.m_filter, query=True, text=True))
		browser.replace_items(self, self.m_scroll_text, items, selected)
		self.m_asset, self._loaded_asset_type = selected, self.m_asset_type
		self._loaded_asset_context = context
		return browser.notify(self.delegate_changeSelect_fnc)

	@browser.action
	def clearListCB(self):
		browser.replace_items(self, self.m_scroll_text, ())
		self.m_asset_type, self.m_asset, self._loaded_asset_type = '', '', ''
		self._loaded_asset_context = None
		return browser.notify(self.delegate_changeSelect_fnc)






def getSimplePorjectCode(codeNum=2):

	if codeNum < 1 :
		codeNum = 1

	ppath = dws.getProjectPath()
	pfolder = dpath.lastComponent(ppath)
	return pfolder[0:codeNum].upper()



def getUserName():

	return getpass.getuser()



def getObjectTypeCode(nodetype=None):
	"""Compatibility wrapper for the Maya-independent naming helper."""
	return asset_naming.object_type_code(nodetype)







def getAssetTypeNodeFromHierarchy(obj = None, typeStr=''):

	if obj is None:
		return []

	outputList = []

	if len(typeStr)>0 :
		#get node list specify node only
		childList = pm.listRelatives(obj, children=True, allDescendents=True) or []


		for c in childList :
			if c.nodeType() == typeStr:
				outputList.append(c)

		if obj.nodeType() == typeStr:
			outputList.insert(0, obj)

	else :
		#get bnode list all children
		outputList = list(pm.listRelatives(obj, children=True, allDescendents=True) or [])
		outputList.insert(0, obj)

	return outputList

def isHaveShape(obj = None) :
	"""Compatibility entry point for the shared actual-shape query."""
	return scene_transforms.hasShape(obj)



def getAssetTransformNoShapeFromHierarchy(obj = None):
	u"""Get transform node　選択したオブジェクト下にあるトランスフォームノードを取得します。"""

	if obj is None :
		return []

	outputList = list(pm.listRelatives(obj, children=1, allDescendents=1, type='transform') or [])
	outputList.insert(0, obj)

	return outputList


def getAssetTransformFromHierarchy(obj = None):
	#get transform node dont have shape node

	if obj is None :
		return []

	outputList = []

	childList = pm.listRelatives(obj, children=True, allDescendents=True) or []


	for node in childList :
		if node.nodeType() == 'transform' :
			#print ('numChildren:{node}'.format(node=node.numChildren()))
			if node.numChildren() == 0 :
				outputList.append(node)
				continue
			else :
				if not isHaveShape(node):
					outputList.append(node)


	if obj.nodeType() == 'transform' :
		if not isHaveShape(obj):
			outputList.insert(0, obj)

	return outputList



def getAssetGeometryFromHierarchy(obj=None, geotypes=('mesh', 'nurbsCurve', 'nurbsSurface')):
	"""Include matching descendants and the selected transform itself."""
	return getAssetShapeParentFromHierarchy(obj, shapetypes=geotypes)


def checkShapeType(obj=None, shapetypes=None):
	"""Check direct shape children, not arbitrary non-transform children."""
	if obj is None:
		return False
	shapetypes = () if shapetypes is None else shapetypes
	return any(shape.nodeType() in shapetypes
		for shape in (pm.listRelatives(obj, shapes=True) or []))



def getAssetShapeParentFromHierarchy(obj=None, shapetypes=None):
	if shapetypes is None:
		shapetypes = []

	if obj is None :
		return []

	outNodeList = []

	#check geometry shape contained in children

	childList = pm.listRelatives(obj, children=True, allDescendents=True) or []

	for node in childList :

		if node.nodeType() == 'transform' :

			if checkShapeType(node, shapetypes):
				outNodeList.append(node)

	if obj.nodeType() == 'transform' and checkShapeType(obj, shapetypes):
		outNodeList.append(obj)

	return outNodeList




def assetAutoRenameGen(obj=None, ignore=''):

	if obj is None:
		return None

	_LOGGER.debug('Generate automatic name for %s', obj.name())


	isNumberLast = False



	#get node type
	nodeType = obj.nodeType()
	_LOGGER.debug('Automatic rename node type: %s', nodeType)



	#check root
	parent_op = getattr(obj, "getParent", None)

	if callable(parent_op) and obj.getParent() is None and not nodeType == 'objectSet':
		return 'root'

	#check parent method is exist
	parentCount_op = getattr(obj, "parentCount", None)
	if callable(parentCount_op) :
		_LOGGER.debug('Node %s parent count: %s', obj.name(), obj.parentCount())

	#init outputname name
	baseName = obj.name()

	#check hierarchy bar
	if '|' in baseName:
		temList = baseName.split('|')
		if len(temList) :
			baseName = temList[-1]

	#check ignore
	if len(ignore) :
		baseName = baseName.replace(ignore, '')

	splitList = baseName.split('_')
	_LOGGER.debug('Automatic rename name segments: %s', splitList)


	outputName =  obj.name()

	#check hierarchy bar
	if '|' in outputName:
		temList = outputName.split('|')
		if len(temList) :
			outputName = temList[-1]

	#check ignore
	if len(ignore) :
		outputName = outputName.replace(ignore, '')


	if len(splitList)>1 :
		outputName = splitList[0]

	#check number exist
	geoIndex = -1
	if isNumberLast :
		match = re.search(r"([a-zA-Z]+)([0-9]+)$", outputName)

		if match:
			#print(match.group(0))
			_LOGGER.debug(
				'Automatic rename numeric suffix: %s / %s',
				match.group(1), match.group(2))
			outputName = match.group(1)
			geoIndex = match.group(2)



	shape_op = getattr(obj, "getShape", None)

	if nodeType == 'objectSet' :
		#object set dont have shape
		outputName = outputName + '_set'
	elif nodeType in mayaConstDic :
		#outputName = outputName+'_cons'
		outputName = outputName+mayaConstDic[nodeType]
	elif nodeType == 'joint' :
		outputName = outputName+'_jnt'
	elif nodeType in pm.listNodeTypes("shader") and nodeType != 'displacementShader' :
		outputName = outputName+'_mtl'
	elif callable(shape_op) :
		shape = obj.getShape()
		if shape is None :
			_LOGGER.debug('Automatic rename node has no shape')
		else :

			shapeType = shape.nodeType()
			_LOGGER.debug('Automatic rename shape %s has type %s', shape, shapeType)

			if shapeType == 'camera' :
				outputName = outputName+'_cam'
			elif shapeType in pm.listNodeTypes("light") :
				outputName = outputName+'_light'
			elif shapeType in mayaGeoDic :
				outputName = outputName+'_geo'




	if isNumberLast and geoIndex > -1 :
		outputName = outputName+'_'+str(geoIndex)



	return outputName
