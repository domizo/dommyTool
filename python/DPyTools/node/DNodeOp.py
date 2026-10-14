import pymel.core as pm
import maya.cmds as cmds

from DPyTools.ui import windows as ui_windows
from DPyTools.ui import node_table, table_selection
from DPyTools.core import components
from DPyTools.scene import parameters

'''
to run:
	>>> import DPyTools.node.DNodeOp as DNodeOp
	>>> DNodeOp.networkInfo()

'''


def getItem(name=''):
	items = pm.ls(name)
	return items[0] if items else None


class DAttributeData(object):
	def __init__(self, attr, nodeName):
		self.m_node = nodeName
		self.m_name = attr
		self.m_fullName = self.m_node + '.' + self.m_name
		self.m_type = 'NULL'
		self.m_size = 0
		self.m_array_size = 0
		self.m_val = None
		self.m_src_node = None
		self.m_dest_nodes = None

		try:
			self.m_type = str(pm.getAttr(self.m_fullName, type=True))
		except RuntimeError:
			return

		if self.m_type == 'TdataCompound':
			return

		self.m_size = int(pm.getAttr(self.m_fullName, size=True))
		try:
			self.m_val = pm.getAttr(self.m_fullName)
		except RuntimeError:
			self.m_val = None

		if isinstance(self.m_val, list):
			self.m_array_size = len(self.m_val)

		if pm.connectionInfo(self.m_fullName, isDestination=True):
			self.m_src_node = pm.connectionInfo(
				self.m_fullName, sourceFromDestination=True)
		if pm.connectionInfo(self.m_fullName, isSource=True):
			self.m_dest_nodes = pm.connectionInfo(
				self.m_fullName, destinationFromSource=True)

	def __str__(self):
		if isinstance(self.m_val, list):
			baseLine = (
				'{0:<40} type:{1:<20} size:{2:<10} array:{3:<10} value:{4}'
				.format(
					self.m_fullName, str(self.m_type), str(self.m_size),
					str(self.m_array_size), self.m_val))
		else:
			baseLine = '{0:<40} type:{1:<20} size:{2:<10} value:{3}'.format(
				self.m_fullName, str(self.m_type), str(self.m_size), self.m_val)

		if self.m_src_node is not None:
			baseLine += '\n\t-->IN: %s' % self.m_src_node
		if self.m_dest_nodes:
			for destination in self.m_dest_nodes:
				baseLine += '\n\tOUT-->: %s' % destination
		return baseLine

	def getConnectSrc(self):
		return self.m_src_node

	def isSource(self):
		return self.m_src_node is not None

	def isDestination(self):
		return bool(self.m_dest_nodes)

	def getConnectDestination(self):
		return self.m_dest_nodes

	def getDestinationCount(self):
		return len(self.m_dest_nodes) if self.m_dest_nodes is not None else 0


def networkInfo():
	'''Display selected-node network information.'''
	selObjs = pm.ls(selection=True)
	if not selObjs:
		return

	nodeName = selObjs[0]
	for attribute in pm.listAttr(nodeName) or []:
		if '.' in attribute:
			continue
		print DAttributeData(attribute, nodeName)


def _multi_attribute_data(node, attribute, remaining):
	if remaining < 0:
		raise ValueError('Attribute table exceeds the supported row limit')
	indices = pm.getAttr(node + '.' + attribute, multiIndices=True)
	if indices is None:
		return []
	if not isinstance(indices, (list, tuple)) or len(indices) > remaining:
		raise ValueError('Sparse attribute rows exceed the supported limit')
	indices = [components.component_index(index) for index in indices]
	if len(set(indices)) != len(indices):
		raise ValueError('Sparse attribute indices must be unique')
	return [DAttributeData(attribute + '[{0}]'.format(index), node) for index in indices]


def _attribute_data(node):
	attributes = pm.listAttr(node)
	attributes = [] if attributes is None else attributes
	if not isinstance(attributes, (list, tuple)) or len(attributes) > table_selection.MAX_TEXT_ITEMS:
		raise ValueError('Attribute names must be a bounded sequence')
	data = []
	for attribute in attributes:
		attribute = table_selection.cell_text(attribute)
		if '.' in attribute:
			continue
		if len(data) >= table_selection.MAX_TEXT_ITEMS:
			raise ValueError('Attribute table exceeds the supported row limit')
		data.append(DAttributeData(attribute, node))
		data.extend(_multi_attribute_data(node, attribute, table_selection.MAX_TEXT_ITEMS - len(data)))
	return data


#open  window
class nodeInfoView(object):

	script_job = None
	selected_node = None

	def __init__(self):
		self.script_job = None
		self.selected_node = None
		self.m_attr_data_array = []
		self.m_filter_array = []
		self._rows = ()
		self._visible_rows = ()
		self._display_cells = ()
		self._pending_cells = None
		self._updating_view = False
		self._view_valid = True

		uiname = "NodeInfoView"

		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname,widthHeight=(1400, 640))
		self.form = pm.formLayout(numberOfDivisions=100)
		self.setBtn = pm.button(label='Set Node',c = lambda *args: self.updateList() )
		self.sortTextField = pm.textField(width=200, textChangedCommand= lambda *args: self.changeSortText())
		self.connectCheckBox = pm.checkBox( label='isConnected', value=False )
		self.table = pm.scriptTable(rows=0, columns=7, label=[(1,"Attribute"), (2,"Type"), (3,"Size"), (4,"Array"), (5,"Src"), (6,"Dest"), (7,"Value")],
			columnWidth=([1,200],[2,100],[3,40],[4,40],[5,150],[6,150],[7,600]), editable=False, selectionMode=1)
		#self.attrListView =pm.textScrollList(selectCommand = lambda *args: self.selectAttributeFromList())

		#edit layout
		#pm.formLayout( self.form, edit=True,
		#	attachForm=[(self.attrListView, 'top', 5), (self.attrListView, 'left', 5), (self.attrListView, 'bottom', 5)],
		#	attachPosition=[(self.attrListView, 'right', 0, 30)]
		#	)
		pm.formLayout(self.form, edit=True,
			attachForm=[(self.setBtn, 'top', 0), (self.setBtn, 'left', 0),
				(self.sortTextField, 'top', 0),
				(self.connectCheckBox, 'top', 0),
				(self.table, 'left', 0), (self.table, 'right', 0), (self.table, 'bottom', 0)],
			attachPosition=[(self.setBtn, 'right', 0, 10)],
			attachControl=[(self.table, 'top', 5, self.setBtn),
				(self.sortTextField, 'left', 5, self.setBtn), (self.sortTextField, 'bottom', 5, self.table),
				(self.connectCheckBox, 'left', 5, self.sortTextField), (self.connectCheckBox, 'bottom', 5, self.table)]
			)

		#create job
		#self.script_job = pm.scriptJob(e=("SelectionChanged", pm.Callback(self.updateList)), parent=self.window)

		#pm.scriptTable(self.table, e=True, gcc = lambda *args: getCellVal())
		cmds.scriptTable(self.table, e=True, gcc=self.getCellVal, selectionChangedCmd=self.changeCellSelect)
		cmds.checkBox( self.connectCheckBox, e=True, changeCommand=lambda *args: self.changeSortText() )

		pm.showWindow( self.window )

	def getActiveDataArray(self):
		"""Return the last committed visible model, including zero matches."""
		return self.m_filter_array

	def _replace_view(self, rows, reset_filters=False):
		"""Publish prepared cells; a failed native commit disables stale selection."""
		cells = tuple(row.cells for row in rows)
		data = [row.data for row in rows]
		self._pending_cells = cells
		try:
			if reset_filters:
				pm.textField(self.sortTextField, e=True, text='')
				pm.checkBox(self.connectCheckBox, e=True, value=False)
			pm.scriptTable(self.table, e=True, clearTable=True)
			pm.scriptTable(self.table, e=True, rows=len(rows))
		except BaseException:
			self._view_valid = False
			raise
		else:
			self._visible_rows = rows
			self._display_cells = cells
			self.m_filter_array = data
			self._view_valid = True
		finally:
			self._pending_cells = None

	def changeCellSelect(self):
		if self._updating_view or not self._view_valid:
			return False
		self._updating_view = True
		try:
			cell = node_table.selected_cell(pm.scriptTable(self.table, q=True, selectedCells=True), len(self._visible_rows))
			if not cell:
				return False
			row, column = cell
			targets = node_table.connection_targets(self._visible_rows[row - 1], column)
			if not targets:
				return False
			nodes = parameters.resolve_nodes(targets, require_same_type=False)
			pm.select(nodes, replace=True)
			return True
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node Info selection was not completed: {0}'.format(error))
			return False
		finally:
			self._updating_view = False

	def changeSortText(self):
		if self._updating_view:
			return False
		self._updating_view = True
		try:
			rows = node_table.filter_rows(self._rows,
				pm.textField(self.sortTextField, q=True, text=True),
				pm.checkBox(self.connectCheckBox, q=True, value=True))
			self._replace_view(rows)
			return True
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node Info filtering was not completed: {0}'.format(error))
			return False
		finally:
			self._updating_view = False




	def getCellVal(self, row, column):
		cells = self._pending_cells
		if cells is None:
			cells = self._display_cells if self._view_valid else ()
		return table_selection.display_cell(cells, row, column)


	def checkMultiIndexAttr(self, attrname=''):
		if self._updating_view or self.selected_node is None:
			return False
		self._updating_view = True
		try:
			additional = _multi_attribute_data(self.selected_node, table_selection.cell_text(attrname),
				table_selection.MAX_TEXT_ITEMS - len(self.m_attr_data_array))
			if not additional:
				return False
			data = self.m_attr_data_array + additional
			rows = node_table.snapshot(data)
			visible = node_table.filter_rows(rows, pm.textField(self.sortTextField, q=True, text=True),
				pm.checkBox(self.connectCheckBox, q=True, value=True))
			self._replace_view(visible)
			self.m_attr_data_array, self._rows = data, rows
			return bool(additional)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node Info sparse rows were not updated: {0}'.format(error))
			return False
		finally:
			self._updating_view = False



	def updateList(self):
		if self._updating_view:
			return False
		self._updating_view = True
		try:
			selected = pm.ls(selection=True) or []
			if not selected:
				return False
			node = selected[0]
			data = _attribute_data(node)
			rows = node_table.snapshot(data)
			self._replace_view(rows, reset_filters=True)
			self.selected_node = node
			self.m_attr_data_array, self._rows = data, rows
			return True
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Node Info update was not completed; native UI may be partial: {0}'.format(error))
			return False
		finally:
			self._updating_view = False



