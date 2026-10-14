import pymel.core as pm

from DPyTools.ui import windows as ui_windows
from DPyTools.ui import table_selection
from DPyTools.scene import parameters

'''
to run:
	>>> import DPyTools.operation.DShareParamOp as DShareParamOp
	>>> DShareParamOp.openWin()

'''


#open  window
class openWin(object):

	nodeList = None


	def __init__(self):
		self.nodeList = []
		uiname = "DShareParameterOperationToolWin"

		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname,widthHeight=(480, 640))
		self.form = pm.formLayout(numberOfDivisions=100)

		self.reloadSelObjBtn = pm.button(label='Reload Node',c = lambda *args: self.reloadSelObjCB() )
		self.addBtn = pm.button(label='+',c = lambda *args: self.addSelObjCB())
		self.minusBtn = pm.button(label='-',c = lambda *args: self.minusSelObjCB())
		self.runShareBtn = pm.button(label='Run Share Attribute',c = lambda *args: self.runShareCB() )

		self.modeOptMenu =pm.optionMenu( label='Operation',changeCommand=lambda *args: self.runChangedCtrlNameCB() )
		pm.menuItem( label='Create New' )
		pm.menuItem( label='Add Exsist' )
		pm.menuItem( label='Share first node in List' )

		self.objectListView =pm.textScrollList(allowMultiSelection=True)
		self.AttrListView =pm.textScrollList(allowMultiSelection=True)


		self.cntlrNameTextField = pm.textField(placeholderText='Enter Name')
		pm.textField( self.cntlrNameTextField, edit=True, enterCommand= lambda *args: self.runChangedCtrlNameCB(), textChangedCommand= lambda *args: self.runChangedCtrlNameCB())
		self.cntlrWarningField = pm.text( label='test', align='left',visible=False )
		self.overwriteCheck = pm.checkBox(label='Allow replacing existing input connections', value=False)
		self.attrFilterTextField = pm.textField(placeholderText='Enter Attribute Name')
		pm.textField( self.attrFilterTextField, edit=True, enterCommand= lambda *args: self.runFilterAttrCB(), textChangedCommand= lambda *args: self.runFilterAttrCB())

		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.reloadSelObjBtn, 'top', 5), (self.reloadSelObjBtn, 'left', 5),
        		(self.addBtn, 'top', 5),
        		(self.minusBtn, 'top', 5),
        		(self.attrFilterTextField, 'top', 5), (self.attrFilterTextField, 'right', 5),
        		(self.objectListView, 'left', 5),
        		(self.AttrListView, 'right', 5),
        		(self.runShareBtn, 'left', 5),(self.runShareBtn, 'bottom', 5),(self.runShareBtn, 'right', 5),
        		(self.modeOptMenu, 'left', 5),
        		(self.cntlrNameTextField, 'left', 5),
        		(self.cntlrWarningField, 'right', 5),
				(self.overwriteCheck, 'left', 5), (self.overwriteCheck, 'right', 5)],
    		attachControl=[(self.objectListView, 'top', 5, self.reloadSelObjBtn),
        		(self.objectListView, 'bottom', 5, self.modeOptMenu),
        		(self.addBtn, 'left', 5, self.reloadSelObjBtn),
        		(self.minusBtn, 'left', 5, self.addBtn),
        		(self.AttrListView, 'top', 5, self.reloadSelObjBtn),
        		(self.AttrListView, 'bottom', 5, self.modeOptMenu),
        		(self.AttrListView, 'left', 0, self.objectListView),
        		(self.modeOptMenu, 'bottom', 5, self.cntlrNameTextField),
        		(self.overwriteCheck, 'bottom', 5, self.runShareBtn),
        		(self.cntlrNameTextField, 'bottom', 5, self.overwriteCheck),
        		(self.cntlrWarningField, 'left', 5, self.cntlrNameTextField),
        		(self.cntlrWarningField, 'top', 5, self.modeOptMenu),
        		(self.cntlrWarningField, 'bottom', 5, self.runShareBtn)],
    		attachPosition=[(self.reloadSelObjBtn, 'bottom', 5, 5),
        		(self.addBtn, 'bottom', 5, 5),
        		(self.minusBtn, 'bottom', 5, 5),
        		(self.attrFilterTextField, 'bottom', 5, 5),
        		(self.objectListView, 'right', 0, 50),
        		(self.attrFilterTextField, 'left', 0, 50),
        		(self.cntlrNameTextField, 'right', 0, 50)],
    		attachNone=(self.runShareBtn, 'top') )

		pm.showWindow( self.window )


	def clearListCB(self):
		pm.textScrollList(self.objectListView, edit=True, removeAll=True)
		pm.textScrollList(self.AttrListView, edit=True, removeAll=True)

	def _set_node_list(self, nodes):
		labels = [parameters.node_label(node) for node in nodes]
		pm.textScrollList(self.objectListView, edit=True, removeAll=True)
		for label in labels:
			pm.textScrollList(self.objectListView, edit=True, append=label)
		self.nodeList = nodes
		self.refreshAttrListCB()

	def refreshAttrListCB(self):
		try:
			attributes = parameters.common_attributes(self.nodeList)
			filter_text = pm.textField(self.attrFilterTextField, query=True, text=True)
			pm.textScrollList(self.AttrListView, edit=True, removeAll=True)
			for attribute in attributes:
				if not filter_text or filter_text in attribute:
					pm.textScrollList(self.AttrListView, edit=True, append=attribute)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Parameter view could not be refreshed: {0}'.format(error))
			return False
		return True

	def reloadSelObjCB(self):
		try:
			nodes = parameters.resolve_nodes(pm.ls(selection=True) or [])
			self._set_node_list(nodes)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node list was not reloaded: {0}'.format(error))
			return False
		return True

	def addSelObjCB(self):
		try:
			selection = pm.ls(selection=True) or []
			if not selection:
				raise ValueError('Please select parameter nodes to add')
			nodes = parameters.merge_nodes(self.nodeList, selection)
			self._set_node_list(nodes)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node list was not changed: {0}'.format(error))
			return False
		return True

	def minusSelObjCB(self):
		try:
			indices = pm.textScrollList(self.objectListView, q=True, selectIndexedItem=True) or []
			rows = table_selection.selected_rows(indices, len(self.nodeList), multiple=True)
			removed = set(row - 1 for row in rows)
			self._set_node_list([node for index, node in enumerate(self.nodeList) if index not in removed])
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node list was not changed: {0}'.format(error))
			return False
		return True

	def runChangedCtrlNameCB(self):
		mode = pm.optionMenu(self.modeOptMenu, q=True, select=True)
		text = pm.textField(self.cntlrNameTextField, q=True, text=True)
		message = ''
		if mode in (1, 2):
			try:
				name = parameters.control_name(text)
				if mode == 1 and (pm.objExists(name) or pm.objExists(name + 'Cntlr')):
					message = 'Share group or controller name already exists.'
				elif mode == 2 and not pm.objExists(name):
					message = 'Share group does not exist.'
			except (TypeError, ValueError) as error:
				message = str(error)
		pm.text(self.cntlrWarningField, e=True, label=message, visible=bool(message))

	def runShareCB(self):
		try:
			attributes = pm.textScrollList(self.AttrListView, q=True, selectItem=True) or []
			mode = pm.optionMenu(self.modeOptMenu, q=True, select=True)
			name = pm.textField(self.cntlrNameTextField, query=True, text=True)
			allow_overwrite = bool(pm.checkBox(self.overwriteCheck, q=True, value=True))
			parameters.share_parameters(self.nodeList, attributes, mode, name, allow_overwrite)
			if mode == 1:
				pm.optionMenu(self.modeOptMenu, e=True, select=2)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Parameters were not fully shared: {0}'.format(error))
			return False
		return True

	def runFilterAttrCB(self):
		return self.refreshAttrListCB()









