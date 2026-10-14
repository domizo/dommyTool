import pymel.core as pm

from DPyTools.ui import windows as ui_windows
from DPyTools.scene import parameters

'''
to run:
	>>> import DPyTools.operation.DCopyParamOp as DCopyParamOp
	>>> DCopyParamOp.openWin()

'''


#open  window
class openWin(object):



	def __init__(self):
		self.sourceNode = None
		self.sourceLabel = ''
		self.targetNodes = []
		self.targetLabels = []
		uiname = "DCopyWin"

		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname,widthHeight=(480, 640))
		self.form = pm.formLayout(numberOfDivisions=100)

		self.SrcObjTextField = pm.textField(placeholderText='Enter Souce Object Name')
		self.setSrcBtn = pm.button(label='Set Source',c = lambda *args: self.setSourceCB() )
		self.setTargetBtn = pm.button(label='Set Target',c = lambda *args: self.setTargetCB() )
		self.objectListView =pm.textScrollList()
		self.runBtn = pm.button(label='Copy Numeric/String Values',c = lambda *args: self.runCopyCB() )

		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.SrcObjTextField, 'top', 5), (self.SrcObjTextField, 'left', 5),
				(self.setSrcBtn, 'top', 5),(self.setSrcBtn, 'right', 5),
        		(self.setTargetBtn, 'left', 5),
        		(self.objectListView, 'left', 5),
        		(self.objectListView, 'right', 5),
        		(self.runBtn, 'left', 5),(self.runBtn, 'bottom', 5),(self.runBtn, 'right', 5)],
    		attachControl=[(self.setSrcBtn, 'left', 5, self.SrcObjTextField),
        		(self.setTargetBtn, 'top', 5, self.SrcObjTextField),
        		(self.objectListView, 'top', 5, self.setTargetBtn),
        		(self.objectListView, 'bottom', 5, self.runBtn)],
    		attachPosition=[(self.SrcObjTextField, 'right', 0, 70)],
    		attachNone=(self.runBtn, 'top') )

		pm.showWindow( self.window )


	def setSourceCB(self):
		try:
			selected = pm.ls(selection=True) or []
			if len(selected) != 1:
				raise ValueError('Select exactly one copy source')
			node = parameters.resolve_nodes(selected)[0]
			label = parameters.node_label(node)
			pm.textField(self.SrcObjTextField, e=True, text=label)
			self.sourceNode, self.sourceLabel = node, label
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Copy source was not changed: {0}'.format(error))
			return False
		return True

	def setTargetCB(self):
		try:
			nodes = parameters.resolve_nodes(pm.ls(selection=True) or [],
				allow_empty=True, require_same_type=False)
			labels = [parameters.node_label(node) for node in nodes]
			pm.textScrollList(self.objectListView, e=True, removeAll=True)
			for label in labels:
				pm.textScrollList(self.objectListView, e=True, append=label)
			self.targetNodes, self.targetLabels = nodes, labels
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Copy targets were not changed: {0}'.format(error))
			return False
		return True

	def runCopyCB(self):
		try:
			text = pm.textField(self.SrcObjTextField, q=True, text=True)
			if not text:
				raise ValueError('Set a copy source')
			items = pm.textScrollList(self.objectListView, q=True, allItems=True) or []
			if items != self.targetLabels:
				raise ValueError('Copy target view is stale; set the targets again')
			source = self.sourceNode if text == self.sourceLabel else pm.PyNode(text)
			parameters.copy_parameters(source, self.targetNodes)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Parameters were not copied completely: {0}'.format(error))
			return False
		return True
