# -*- coding: utf-8 -*-
"""Maya UI for mailing-list management and notification compatibility."""
import pymel.core as pm
import maya.cmds as cmds

from DPyTools.pipeline.assets import operations as dasset

from DPyTools.pipeline.assets import users as dusers
from DPyTools.pipeline.assets import user_data
from DPyTools.pipeline.assets import paths as asset_paths
from DPyTools.integrations.smtp.client import send_notification
from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection

'''
to run:
	>>> import DPyTools.integrations.smtp.mail as dmail


'''



class DMailingListManager(object):
	u"""メールを送る人の管理ツール"""


	m_uiname = "DMailingListManager"

	#current project
	m_project = ''

	#mailingList_dic
	m_list = None  # Cached user IDs; initialized per instance.
	m_user_list = None




	def __init__(self):
		self.m_uiname = "DMailingListManager"
		self.m_list = []
		self.m_user_list = dusers.getUserList()
		self._table_rows = ()
		self._project_path = ''


	def openWin(self, parent=None, project_path=None):
		u"""ウィンドウを表示"""
		project = dasset.getAssetCurrentProjectPath() if project_path is None else project_path
		project = asset_paths.existing_project_path(project or '')
		if not project:
			pm.warning('Select an existing project before opening its mailing list.')
			return False
		self._project_path = project


		#new window
		ui_windows.delete_window(self.m_uiname)


		self.mailWin = pm.window(self.m_uiname, widthHeight=(320, 640), parent=parent)

		############################################################
		####################create form and column####################
		self.form = pm.formLayout(numberOfDivisions=100)

		self.table = cmds.scriptTable('Dmail_list_table', rows=0, columns=1, selectionMode=1, selectionBehavior=1,
									label=[(1,"Name")],
									editable=0, sortEnabled=0,
									columnWidth=[(1,150)],
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
			attachPosition=[(self.table, 'right', 0, 50)]
			)

		#show window
		self.mailWin.show()

		#prepareDefault
		self.prepareCB()
		return True




	def prepareCB(self) :
		u"""ウィンドウを開いた際のデータの初期読み込み"""
		self.refleshCB()


	def clearTableCB(self):
		u"""テーブルのクリア"""
		self.m_list = []
		self._table_rows = ()
		cmds.scriptTable(self.table, e=1, rows=0)


	def loadMailListData(self):
		u"""Admit fresh mailing IDs and names before replacing either cached model."""
		ids = user_data.validate_mailing_list(dasset.getMaillingList(project_path=self._project_path))
		users = user_data.validate_user_collection(dusers.getUserList())
		rows = user_data.mailing_table_rows(ids, users)
		self.m_list, self.m_user_list, self._table_rows = ids, users, rows


	def addUserFromuserIds(self, userids=None):
		u"""Return confirmed batch-save status, independent of a stale UI cache."""
		if not dasset.addUsersMailingList(userids=userids, project_path=self._project_path):
			return False
		try:
			self.refleshCB()
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Mailing list was saved, but its view could not be refreshed: {0}'
				.format(exc))
		return True


	def addUserCB(self) :
		u"""ユーザーの追加のためのユーザーリストを表示"""

		# print ('Add user CB')
		addWin = dusers.DUserSelectWin()
		addWin.openWin(parent=self.mailWin)
		addWin.w_cntlr = self

	def delUserCB(self) :
		u"""ユーザーの削除"""

		if len(self.m_list)==0 :
			return

		try:
			selection = cmds.scriptTable(self.table, q=1, selectedRow=1)
			rows = table_selection.selected_rows(selection, len(self.m_list))
			if not rows:
				return
			uid = user_data.validate_user_id(self.m_list[rows[0] - 1])
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Mailing-list selection could not be read: {0}'.format(exc))
			return
		if dasset.deleteUserMailningList(userid=uid, project_path=self._project_path):
			self.refleshCB()



	def refleshCB(self):
		u"""メーリングリストを再読み込み"""
		self.loadMailListData()
		cmds.scriptTable(self.table, e=1, rows=len(self._table_rows))
		cmds.scriptTable(self.table, e=1, clearTable=1)



	def getCellCB(self, row, column):
		u"""Display one retained cell; never interpret invalid rows as negative indices."""
		return table_selection.display_cell(self._table_rows, row, column)





#################################################Functions



def send_gmail_notif(title='title', message='', address_list=None):
	"""Compatibility wrapper for the historical function name."""
	send_notification(
		title=title, message=message, address_list=address_list)
