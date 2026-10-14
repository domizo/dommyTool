# -*- coding: utf-8 -*-
u"""User directory data and its legacy Maya management UI."""

import pymel.core as pm
import maya.cmds as cmds

import DPyTools.system.DPathOp as dpath
from DPyTools.pipeline.assets import repository as asset_repository
from DPyTools.pipeline.assets import user_data
from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection


import os

'''
to run:
>>> import DPyTools.pipeline.assets.users as dusers

'''



# Local and network paths are configured per environment, not committed.
user_list_path = os.environ.get(
	'DOMMYTOOL_USER_LIST', os.path.join(os.path.expanduser('~'), 'user_list.db'))


#user data keys
userIdKey = 'id'
userNameKey = 'name'
userMailKey = 'mail'


def getUserList():
	u"""ユーザーのリストの参照"""
	if dpath.isFile(path=user_list_path) :
		try:
			return user_data.validate_user_collection(
				asset_repository.load_mapping(user_list_path))
		except (IOError, OSError, TypeError, ValueError) as exc:
			pm.warning('Invalid user database: {0}'.format(exc))
	return {}


def addUserData(user_dic=None):
	u"""ユーザーデータの追加"""
	if user_dic is None:
		raise ValueError('user_dic must not be empty')

	validated_user = user_data.validate_user_record(user_dic)
	if not dpath.isFile( user_list_path ):
		user_list = {validated_user[userIdKey] : validated_user}
		asset_repository.save_user_collection(user_list_path, user_list)
		return True

	else :

		#load exist json file
		data = user_data.validate_user_collection(
			asset_repository.load_mapping(user_list_path))

		if not (validated_user[userIdKey] in data.keys()) :
			data[validated_user[userIdKey]] = validated_user

			#save exist json file
			asset_repository.save_user_collection(user_list_path, data)
			return True
	return False


def addUser(name='', userid='', mail_addr=''):
	u"""ユーザーの追加"""

	if len(name)==0 or len(userid)==0 or len(mail_addr)==0 :
		return
	else :

		userData = {userNameKey: name,
					userIdKey: userid,
					userMailKey: mail_addr
					}

		try:
			return addUserData(user_dic=userData)
		except (IOError, OSError, TypeError, ValueError) as exc:
			pm.warning('User could not be added: {0}'.format(exc))
			return False



def _delete_user(user_id=None, name=None):
	u"""Resolve one identity from one validated snapshot before persistence."""
	try:
		if user_id is not None:
			user_id = user_data.validate_user_id(user_id)
		elif not isinstance(name, user_data.STRING_TYPES) or not name:
			return False
		if not dpath.isFile(path=user_list_path):
			return False
		data = user_data.validate_user_collection(
			asset_repository.load_mapping(user_list_path))
		if user_id is None:
			for key in data:
				if data[key][userNameKey] == name:
					if user_id is not None:
						raise ValueError('User name is ambiguous; delete by user ID instead')
					user_id = key
		if user_id not in data:
			return False
		del data[user_id]
		asset_repository.save_user_collection(user_list_path, data)
		return True
	except (IOError, OSError, TypeError, ValueError) as exc:
		pm.warning('User could not be deleted: {0}'.format(exc))
		return False


def deleteUserFromId(userid=''):
	u"""Delete only the exact persistent user ID, independent of display names."""
	return _delete_user(user_id=userid)


def deleteUserFromName(name=''):
	u"""Compatibility API: delete a unique exact name, refusing ambiguity."""
	return _delete_user(name=name)


def _selected_user_ids(table, known_ids, multiple=False):
	u"""Validate all displayed IDs before allowing a user-table mutation."""
	if multiple:
		selection = cmds.scriptTable(table, q=1, selectedRows=1)
	else:
		selection = cmds.scriptTable(table, q=1, selectedRow=1)
	rows = table_selection.selected_rows(selection, len(known_ids), multiple=multiple)
	user_ids = []
	seen = set()
	for row in rows:
		cell = cmds.scriptTable(table, cellIndex=[row, 1], q=1, cellValue=1)
		user_id = user_data.validate_user_id(table_selection.cell_text(cell))
		if user_id not in known_ids:
			raise ValueError('The selected user is stale; refresh the user list')
		if user_id in seen:
			raise ValueError('Selected user IDs must not contain duplicates')
		seen.add(user_id)
		user_ids.append(user_id)
	return user_ids





#################################################################################
##########################User manage class######################################


class DAddUserWin(object):
	u"""ユーザー追加ウィンドウ"""

	m_uiname = "DAddUserWin"


	#delegate
	func_Added_User = None
	func_Closed_Win = None


	def __init__(self):
		self.m_uiname = "DAddUserWin"

		self.func_Added_User = None
		self.func_Closed_Win = None


	def openWin(self, parent=None):
		u"""ウィンドウを表示"""

		#new window
		ui_windows.delete_window(self.m_uiname)

		self.mainWin = pm.window(self.m_uiname, widthHeight=(300, 200), parent=parent )

		pm.columnLayout(adjustableColumn=1,rowSpacing=5)

		self.userIdText = pm.textField(placeholderText='User ID')
		self.userNameText = pm.textField(placeholderText='User Name')
		self.mailText = pm.textField(placeholderText='Mail')

		pm.rowLayout(numberOfColumns=2)
		pm.button(label='Add', c=lambda *args: self.addCB(), width=80)
		pm.button(label='Cancel', c=lambda *args: self.cancelCB(), width=80)
		pm.setParent( '..' )#rowLayout

		pm.setParent( '..' )#column

		self.mainWin.show()



	def addCB(self):
		u"""ユーザーの追加"""

		idText = pm.textField(self.userIdText, q=1, tx=1)
		nameText = pm.textField(self.userNameText, q=1, tx=1)
		mailText = pm.textField(self.mailText, q=1, tx=1)

		# print ('id:{i}, name:{n}, mail:{m}'.format(i=idText, n=nameText, m=mailText))

		if len(idText)==0 or len(nameText)==0 or len(mailText)==0 :
			pm.warning('Please enter user infomations! userid, name, mail.')
			return

		if not addUser(name=nameText, userid=idText, mail_addr=mailText):
			return

		if self.func_Added_User :
			self.func_Added_User()

		pm.deleteUI(self.mainWin)

	def cancelCB(self):
		u"""ウィンドウを閉じる"""

		pm.deleteUI(self.mainWin)




class DUserSelectWin(object):
	u"""ユーザー選択ツール"""

	m_uiname = "DUserSelectWin"

	# data list
	m_list = None


	#delegate
	func_Added_User = None
	func_Canceled = None
	w_cntlr = None

	def __init__(self):
		self.m_uiname = "DUserSelectWin"
		self.m_list = getUserList()
		self._table_rows = user_data.user_table_rows(self.m_list)
		self.func_Added_User = None
		self.func_Canceled = None
		self.w_cntlr = None


	def openWin(self, parent=None):
		u"""ウィンドウを表示"""

		#new window
		ui_windows.delete_window(self.m_uiname)


		self.userWin = pm.window(self.m_uiname, widthHeight=(600, 640), parent=parent )

		self.form = pm.formLayout(numberOfDivisions=100)


		self.table = cmds.scriptTable('Duser_list_table', rows=0, columns=3, selectionMode=3, selectionBehavior=1,
									label=[(1,"UserID"),(2,"Name"),(3,"Mail")],
									editable=0,
									#
									columnWidth=[(1,80),(2,150),(3,150)],sortEnabled=1,
									getCellCmd=self.getCellCB
									)


		self.AddBtn = pm.button(label='Add', c=lambda *args: self.addUserCB())
		self.CancelBtn = pm.button(label='Cancel', c=lambda *args: self.cancelCB())




		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.table, 'top', 5), (self.table, 'left', 5),(self.table, 'right', 5),
						(self.AddBtn, 'left', 5), (self.AddBtn, 'bottom', 5),
						(self.CancelBtn, 'right', 5), (self.CancelBtn, 'bottom', 5)],
			attachControl=[(self.table, 'bottom', 5, self.AddBtn),
							(self.CancelBtn, 'left', 5, self.AddBtn)],
			attachPosition=[(self.AddBtn, 'right', 0, 50)]
			)

		#show window
		self.userWin.show()

		#prepareDefault
		self.prepareCB()

	def prepareCB(self) :
		u"""ユーザーリスト、GUIの初期化"""
		self.refleshCB()

	def clearTableCB(self):
		u"""テーブルのクリア"""
		self.m_list = {}
		self._table_rows = ()
		cmds.scriptTable(self.table, e=1, rows=0)

	def loadUserData(self):
		u"""User listの読み込み"""
		data = getUserList()
		rows = user_data.user_table_rows(data)
		self.m_list, self._table_rows = data, rows

	def addUserCB(self) :
		u"""ユーザーの追加"""
		try:
			data = _selected_user_ids(self.table, self.m_list, multiple=True)
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('User selection could not be read: {0}'.format(exc))
			return False
		if not data:
			return False
		add_op = getattr(self.w_cntlr, 'addUserFromuserIds', None)
		if not callable(add_op):
			pm.warning('No user-selection receiver is available')
			return False
		try:
			saved = add_op(userids=data)
			if not isinstance(saved, bool) or not saved:
				pm.warning('User-selection receiver did not confirm a successful save')
				return False
		except (IOError, OSError, RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Selected users could not be added: {0}'.format(exc))
			return False
		pm.deleteUI(self.userWin)
		return True

	def cancelCB(self) :
		u"""ユーザーの追加のキャンセル"""
		# print ('cancel')
		pm.deleteUI(self.userWin)


	def refleshCB(self):
		u"""GUIを更新"""
		self.loadUserData()
		cmds.scriptTable(self.table, e=1, rows=len(self._table_rows))
		cmds.scriptTable(self.table, e=1, clearTable=1)

	def getCurrentKey(self, row=0):
		u"""指定されたナンバーのキーを取得"""
		return sorted(self.m_list.keys())[row]


	def getCellCB(self, row, column):
		u"""Display one admitted snapshot cell without sorting or native queries."""
		return table_selection.display_cell(self._table_rows, row, column)


class DUserManager(object):
	u"""ユーザー管理ツール"""

	m_uiname = "DUserManager"

	# data list
	m_list = None


	def __init__(self):
		self.m_uiname = "DUserManager"
		self.m_list = {}
		self._table_rows = ()

	def openWin(self):
		u"""ウィンドウを表示"""


		#new window
		ui_windows.delete_window(self.m_uiname)


		self.userWin = pm.window(self.m_uiname, widthHeight=(600, 640))

		self.form = pm.formLayout(numberOfDivisions=100)


		self.table = cmds.scriptTable('Duser_list_table', rows=0, columns=3, selectionMode=1, selectionBehavior=1,
									label=[(1,"UserID"),(2,"Name"),(3,"Mail")],
									editable=0,
									#
									columnWidth=[(1,80),(2,150),(3,150)],sortEnabled=1,
									getCellCmd=self.getCellCB
									)


		self.column = pm.columnLayout(adjustableColumn=1,rowSpacing=5)

		pm.button(label='Add', c=lambda *args: self.addUserCB())
		pm.button(label='Delete', c=lambda *args: self.delUserCB())

		pm.setParent( '..' )#column


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.table, 'top', 5), (self.table, 'left', 5),(self.table, 'bottom', 5),
						(self.column, 'top', 5), (self.column, 'right', 5),(self.column, 'bottom', 5)],
			attachControl=[(self.column, 'left', 5, self.table)],
			attachPosition=[(self.table, 'right', 0, 80)]
			)

		#show window
		self.userWin.show()

		#prepareDefault
		self.prepareCB()


	def prepareCB(self) :
		u"""ユーザーリスト、GUIの初期化"""


		self.refleshCB()



	def clearTableCB(self):
		u"""テーブルのクリア"""
		# print ('clearTableCB')
		self.m_list = {}
		self._table_rows = ()
		cmds.scriptTable(self.table, e=1, rows=0)


	def loadUserData(self):
		u"""User listの読み込み"""
		data = getUserList()
		rows = user_data.user_table_rows(data)
		self.m_list, self._table_rows = data, rows

	def addUserCB(self) :
		u"""ユーザーの追加"""
		# print ('Add User')

		addWin = DAddUserWin()
		addWin.openWin(parent=self.userWin)
		#set delegate
		addWin.func_Added_User = self.refleshCB

	def getCurrentKey(self, row=0):
		u"""指定されたナンバーのキーを取得"""
		return sorted(self.m_list.keys())[row]

	def delUserCB(self) :
		u"""ユーザーの削除"""
		# print ('Delete User')

		if len(self.m_list)==0 :
			return

		try:
			user_ids = _selected_user_ids(self.table, self.m_list)
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('User selection could not be read: {0}'.format(exc))
			return
		if user_ids and deleteUserFromId(userid=user_ids[0]):
			self.refleshCB()



	def refleshCB(self):
		u"""GUIを更新"""
		self.loadUserData()
		cmds.scriptTable(self.table, e=1, rows=len(self._table_rows))
		cmds.scriptTable(self.table, e=1, clearTable=1)


	def getCellCB(self, row, column):
		u"""Display one admitted snapshot cell without sorting or native queries."""
		return table_selection.display_cell(self._table_rows, row, column)
