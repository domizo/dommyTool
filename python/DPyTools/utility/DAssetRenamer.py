# -*- coding: utf-8 -*-
import pymel.core as pm

from DPyTools.core import components, diagnostics
from DPyTools.pipeline.assets import operations as dasset
from DPyTools.pipeline.assets import rename_plan
from DPyTools.pipeline.assets import browser

from DPyTools.ui import windows as ui_windows


_LOGGER = diagnostics.get_logger(__name__)


'''
to run:
	>>> import DPyTools.utility.DAssetRenamer as DARenamer

'''





class DAssetRenamer(object):
	u"""アセット名前変更ツールクラス"""

	m_uiname = "DAssetRenamer"

	def __init__(self):
		self.m_uiname = "DAssetRenamer"


	def openWin(self):

		#template
		template = pm.uiTemplate('assetHierarchyTemplate', force=True)
		template.define(pm.frameLayout, collapsable=True, collapse=False, borderVisible=0)
		template.define(pm.rowColumnLayout, numberOfColumns=2, columnWidth=[(1, 150), (2, 200)])
		template.define(pm.columnLayout, rowSpacing=10, adjustableColumn=True)


		ui_windows.delete_window(self.m_uiname)

		win = pm.window(self.m_uiname, widthHeight=(790, 640))

		#set template
		pm.setUITemplate( 'assetHierarchyTemplate', pushTemplate=True )




		############################################################
		####################create form and column####################
		self.form = pm.formLayout(numberOfDivisions=100)


		#filter menu
		self.filterMenu = pm.optionMenu( label='Node Filter', changeCommand = lambda *args: self.changeSelectObject())
		pm.menuItem( label='None' )
		pm.menuItem( label='Transform' )
		pm.menuItem( label='Geometry' )
		pm.menuItem( label='Mesh' )
		pm.menuItem( label='NurbsCurve' )
		pm.menuItem( label='NurbsSurface' )
		pm.menuItem( label='Joint' )
		pm.menuItem( label='Light' )
		pm.menuItem( label='Camera' )
		pm.menuItem( label='Sets' )
		pm.menuItem( label='Constraint' )
		pm.menuItem( label='Material' )

		self.filterTextField = pm.textField(placeholderText='Enter Node Name', width=150,
							textChangedCommand=lambda *args: self.changeSelectObject() ,
							searchField=1)


		self.srcTextScroll = pm.textScrollList( allowMultiSelection=True)
		self.targetTextScroll = pm.textScrollList( allowMultiSelection=True,
													doubleClickCommand=lambda *args: self.manualNameChangeCB(),
													deleteKeyCommand=lambda *args: self.deleteSelectObjectCB())

		self.rightScroll = pm.scrollLayout(childResizable=1)
		# self.innerForm = pm.formLayout()
		self.rightColumn = pm.columnLayout(adjustableColumn=1)


		#######make button method
		def makeTagBtn(label='', checkList=None):
			if checkList is None:
				checkList = []
			pm.button(label=label , c = lambda *args: self.addNameTag(tagStr=label, switchList=checkList))

		btnSize = 80
		barCol1 = [0.4,.4,.4]


		pm.frameLayout( label="Tag", bgc=barCol1, collapsable=1, collapse=0, borderVisible=0)
		#######create tab
		self.tab = pm.tabLayout(innerMarginWidth=5, innerMarginHeight=5)

		###TransformTab
		optTransTab = pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize), (4, btnSize)] )

		for typ in dasset.transTagList:
			makeTagBtn(typ)

		pm.setParent( '..' )#rowColumnLayout

		###TypeTab
		optTypeTab = pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize), (4, btnSize)] )

		for typ in dasset.typeTagList:
			makeTagBtn(typ)

		pm.setParent( '..' )#rowColumnLayout

		###Sidetab
		optSideTab = pm.frameLayout( label="", collapsable=0, collapse=0, borderVisible=0,labelVisible=0)

		pm.text(label='Left Center Right', align='left')
		pm.rowColumnLayout( numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])

		for typ in dasset.sideTagList:
			makeTagBtn(typ, checkList=dasset.sideTagCheckList)

		pm.setParent( '..' )#rowColumnLayout

		pm.text(label='Front Back', align='left')
		pm.rowColumnLayout(numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])
		makeTagBtn('_fr', checkList=['_fr','_bk'])
		makeTagBtn('_bk', checkList=['_fr','_bk'])
		pm.setParent( '..' )#rowColumnLayout

		pm.text(label='In Out', align='left')
		pm.rowColumnLayout(numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])
		makeTagBtn('_in', checkList=['_in','_out'])
		makeTagBtn('_out', checkList=['_in','_out'])
		pm.setParent( '..' )#rowColumnLayout


		pm.setParent( '..' )#framelayout side tag


		###custom tab
		optCustomTab = pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Custom Tag', align='left')
		self.CustomTagText = pm.textField(placeholderText='Enter Custom Tag')
		pm.button(label='Add Custom' , c = lambda *args: self.addCustomTag())

		pm.setParent( '..' )#rowColumnLayout

		pm.setParent('..')#tab

		pm.setParent('..')#top frame


		pm.tabLayout(self.tab, e=1,
				tabLabel=[(optTransTab, 'Transform'),(optTypeTab,'Type'),(optSideTab,'Side'),(optCustomTab,'Custom')]
		)




		'''
		#######transform tag button
		pm.frameLayout( label="Option : Transform Tag", bgc=barCol1 )
		pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize), (4, btnSize)] )

		for typ in dasset.transTagList:
			makeTagBtn(typ)

		pm.setParent( '..' )#rowColumnLayout
		pm.setParent( '..' )#framelayout type tag

		#######create type tag button
		pm.frameLayout( label="Option : Type Tag", bgc=barCol1)
		pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize), (4, btnSize)] )

		for typ in dasset.typeTagList:
			makeTagBtn(typ)

		pm.setParent( '..' )#rowColumnLayout
		pm.setParent( '..' )#framelayout type tag


		#######create side tag button
		pm.frameLayout( label="Option : Side Tag", bgc=barCol1)

		pm.text(label='Left Center Right', align='left')
		pm.rowColumnLayout( numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])

		for typ in dasset.sideTagList:
			makeTagBtn(typ, checkList=dasset.sideTagCheckList)

		pm.setParent( '..' )#rowColumnLayout

		pm.text(label='Front Back', align='left')
		pm.rowColumnLayout(numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])
		makeTagBtn('_fr', checkList=['_fr','_bk'])
		makeTagBtn('_bk', checkList=['_fr','_bk'])
		pm.setParent( '..' )#rowColumnLayout

		pm.text(label='In Out', align='left')
		pm.rowColumnLayout(numberOfColumns=3, columnWidth=[(1, btnSize), (2, btnSize), (3, btnSize)])
		makeTagBtn('_in', checkList=['_in','_out'])
		makeTagBtn('_out', checkList=['_in','_out'])
		pm.setParent( '..' )#rowColumnLayout


		pm.setParent( '..' )#framelayout side tag



		#######create custom tag button
		pm.frameLayout( label="Option : Custom Tag", bgc=barCol1)

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Custom Tag', align='left')
		self.CustomTagText = pm.textField(placeholderText='Enter Custom Tag')
		pm.button(label='Add Custom' , c = lambda *args: self.addCustomTag())

		pm.setParent( '..' )#rowColumnLayout
		pm.setParent( '..' )#framelayout custom tag
		'''


		barCol = barCol1
		#######create replace frame
		pm.frameLayout( label="Replace String", bgc=barCol )

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Source Text', align='left')
		self.SourceRepText = pm.textField(placeholderText='Enter Source Text')
		pm.button(label='Set' , c = lambda *args: self.setSourceReplaceTextCB())
		pm.setParent( '..' )#rowColumnLayout

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Replace Text', align='left')
		self.ReplaceText = pm.textField(placeholderText='Enter Replace Text')
		pm.button(label='Replace' , c = lambda *args: self.runReplaceTextCB())
		pm.setParent( '..' )#rowColumnLayout

		pm.setParent( '..' )#framelayout replace

		#######create replace frame
		pm.frameLayout( label="Rename Multi Number", bgc=barCol )

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='New Name', align='left')
		self.RenameMultiText = pm.textField(placeholderText='Enter New Name')
		pm.button(label='Rename' , c = lambda *args: self.runRenameMultiCB())
		pm.setParent( '..' )#rowColumnLayout


		pm.setParent( '..' )#framelayout replace

		#######create Ignore frame
		pm.frameLayout( label="Ignore (POST)", bgc=barCol )

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Ignore Text', align='left')
		self.ignoreText = pm.textField(placeholderText='Enter ignore text', tcc=lambda *args: self.changeSelectObject())
		pm.button(label='Set' , c = lambda *args: self.setIgoreTextCB())
		pm.setParent( '..' )#rowColumnLayout

		pm.setParent( '..' )#framelayout ignore


		#######create Add Prefix
		pm.frameLayout( label="Prefix", bgc=barCol )

		pm.rowColumnLayout(adjustableColumn=2, numberOfColumns=3, columnWidth=[(1, 100), (2, 180), (3, 100)])
		pm.text(label='Prefix Text', align='left')
		self.prefixText = pm.textField(placeholderText='Enter prefix')
		pm.button(label='Add' , c = lambda *args: self.addPrefixTextCB())
		pm.setParent( '..' )#rowColumnLayout
		pm.setParent( '..' )#framelayout ignore



		#######finish Column
		pm.setParent( '..' )#right column
		# pm.setParent( '..' )#innerForm

		pm.setParent( '..')#right scroll
		#######run frame
		# pm.frameLayout( label="Run", bgc=[0.5,.0,.0], collapsable=0, borderVisible=0, backgroundShade=0)

		self.runBtn = pm.button(label='Run Rename',height=44, bgc=[0.5,.0,0.0] , c = lambda *args: self.runRename())
		#pm.checkBox(label="Edit Numbar Last")
		# pm.setParent( '..' )#framelayout run

		pm.setParent( '..' )#formLayout




		# pm.formLayout( self.innerForm, edit=True,
		# 		attachForm=[(self.rightColumn, 'top', 0), (self.rightColumn, 'left', 0),
		# 					(self.rightColumn, 'right', 0), (self.rightColumn, 'bottom', 0)]
		# 	)


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.filterMenu, 'top', 11), (self.filterMenu, 'left', 5),
			(self.filterTextField, 'top', 0),
			(self.srcTextScroll, 'left', 5),(self.srcTextScroll, 'bottom', 5),
			(self.targetTextScroll, 'bottom', 5),
			(self.rightScroll, 'right', 5),
			(self.runBtn, 'right', 5),(self.runBtn, 'bottom', 5)],
			attachControl=[(self.filterTextField, 'left', 5, self.filterMenu),
				(self.srcTextScroll, 'top', 5, self.filterMenu),
				(self.targetTextScroll, 'top', 5, self.filterMenu), (self.targetTextScroll, 'left', 5, self.srcTextScroll),
				(self.rightScroll, 'top', 5, self.filterMenu),(self.rightScroll, 'left', 5, self.targetTextScroll),(self.rightScroll, 'bottom', 5, self.runBtn),
				(self.runBtn, 'left', 5, self.targetTextScroll)
				],
			attachPosition=[(self.srcTextScroll, 'right', 0, 25),(self.targetTextScroll, 'right', 0, 50)]
			)


		pm.setParent( '..' )#form


		#create scriptJob
		jobNum = pm.scriptJob( e=['SelectionChanged', self.changeSelectObject], parent=self.m_uiname )

		#fin template
		pm.setUITemplate( popTemplate=True )

		#show window
		win.show()

		#option select Transform
		pm.optionMenu(self.filterMenu, e=1, select=1)






	def _preview_snapshot(self):
		return rename_plan.preview(
			pm.textScrollList(self.srcTextScroll, query=True, allItems=True),
			pm.textScrollList(self.targetTextScroll, query=True, allItems=True),
			pm.textScrollList(self.targetTextScroll, query=True, selectIndexedItem=True))

	def _write_preview(self, before, after):
		"""Readmit the retained model before bounded native view changes, not scene edits."""
		after = rename_plan.preview(after.sources, after.targets, after.selected)
		if self._preview_snapshot() != before:
			raise ValueError('Rename preview changed while the action was open; retry from the current view')
		if not before.targets:
			return False
		return self._apply_preview(after,
			replace_sources=after.sources != before.sources,
			replace_targets=after.targets != before.targets)

	def _apply_preview(self, model, replace_sources=True, replace_targets=True):
		"""Apply one complete model; native view failures may leave partial rows."""
		model = rename_plan.preview(model.sources, model.targets, model.selected)
		with browser.updating(self):
			if replace_sources:
				pm.textScrollList(self.srcTextScroll, edit=True, removeAll=True)
				if model.sources:
					pm.textScrollList(self.srcTextScroll, edit=True, append=list(model.sources))
			if replace_targets:
				pm.textScrollList(self.targetTextScroll, edit=True, removeAll=True)
				if model.targets:
					pm.textScrollList(self.targetTextScroll, edit=True, append=list(model.targets))
				if model.selected:
					pm.textScrollList(self.targetTextScroll, edit=True, selectIndexedItem=list(model.selected))
		return True

	@browser.action
	def addPrefixTextCB(self):
		before = self._preview_snapshot()
		prefix = pm.textField(self.prefixText, query=True, text=True)
		return self._write_preview(before, rename_plan.prefix_preview(before, prefix))



	@browser.action
	def runRenameMultiCB(self):
		before = self._preview_snapshot()
		if not before.selected:
			return False
		name = pm.textField(self.RenameMultiText, query=True, text=True)
		return self._write_preview(before, rename_plan.numbered_preview(before, name))


	@browser.action
	def setIgoreTextCB(self):
		before = self._preview_snapshot()
		value = before.targets[before.selected[0] - 1] if before.selected else ''
		pm.textField(self.ignoreText, edit=True, text=value)
		return True

	@browser.action
	def setSourceReplaceTextCB(self):
		before = self._preview_snapshot()
		value = before.targets[before.selected[0] - 1] if before.selected else ''
		pm.textField(self.SourceRepText, edit=True, text=value)
		return True

	@browser.action
	def runReplaceTextCB(self):
		before = self._preview_snapshot()
		source = pm.textField(self.SourceRepText, query=True, text=True)
		replacement = pm.textField(self.ReplaceText, query=True, text=True)
		return self._write_preview(before, rename_plan.replace_preview(before, source, replacement))



	@browser.action
	def manualNameChangeCB(self):
		before = self._preview_snapshot()
		if not before.selected:
			return False
		result = pm.promptDialog(title='Rename Object', message='Enter Name:',
			button=['OK', 'Cancel'], text=before.targets[before.selected[0] - 1],
			defaultButton='OK', cancelButton='Cancel', dismissString='Cancel')
		if result != 'OK':
			return False
		name = pm.promptDialog(query=True, text=True)
		return self._write_preview(before, rename_plan.manual_preview(before, name))


	@browser.action
	def deleteSelectObjectCB(self):
		before = self._preview_snapshot()
		if not before.selected:
			return False
		return self._write_preview(before, rename_plan.delete_preview_rows(before))






	@browser.action
	def addNameTag(self, tagStr='_ctrl', switchList=None):
		before = self._preview_snapshot()
		if not before.selected:
			pm.warning('Select at least one target row to change its tag.')
			return False
		return self._write_preview(before, rename_plan.tag_preview(before, tagStr, switchList))


	@browser.action
	def addCustomTag(self):
		before = self._preview_snapshot()
		if not before.selected:
			return False
		custom = pm.textField(self.CustomTagText, query=True, text=True)
		return self._write_preview(before, rename_plan.custom_tag_preview(before, custom))



	def _selection_preview(self):
		"""Collect and admit paired rows before touching the existing view."""
		filter_type = components.component_index(pm.optionMenu(self.filterMenu, query=True, select=True))
		if not 1 <= filter_type <= 12:
			raise ValueError('Rename filter must be one of the twelve menu choices')
		selection = pm.ls(sl=True) or []
		if not isinstance(selection, (list, tuple)) or len(selection) > rename_plan.MAX_RENAME_ITEMS:
			raise ValueError('Rename selection exceeds the supported row limit')
		if not selection:
			return rename_plan.preview(None, None)
		filter_text = pm.textField(self.filterTextField, query=True, text=True)
		ignore = pm.textField(self.ignoreText, query=True, text=True)
		if not all(isinstance(value, rename_plan.STRING_TYPES) for value in (filter_text, ignore)):
			raise TypeError('Rename filter and ignore fields must contain text')
		filter_text = filter_text.lower()

		classified = set()
		if filter_type in (8, 12):
			category = 'light' if filter_type == 8 else 'shader'
			types = pm.listNodeTypes(category) or []
			classified_nodes = (pm.ls(type=types) or []) if types else ()
			for node in classified_nodes:
				candidate = node.getParent() if filter_type == 8 else node
				if candidate is not None:
					classified.add(candidate)

		queries = {
			1: dasset.getAssetTransformNoShapeFromHierarchy,
			2: dasset.getAssetTransformFromHierarchy,
			3: dasset.getAssetGeometryFromHierarchy,
		}
		nodes, seen = [], set()
		for obj in selection:
			if filter_type in queries:
				candidates = queries[filter_type](obj)
			elif filter_type in (4, 5, 6):
				kind = ('mesh', 'nurbsCurve', 'nurbsSurface')[filter_type - 4]
				candidates = dasset.getAssetGeometryFromHierarchy(obj, geotypes=[kind])
			elif filter_type == 7:
				candidates = dasset.getAssetTypeNodeFromHierarchy(obj, typeStr='joint')
			elif filter_type == 9:
				candidates = dasset.getAssetShapeParentFromHierarchy(obj, shapetypes=['camera'])
			elif filter_type == 10:
				candidates = dasset.getAssetTypeNodeFromHierarchy(obj, typeStr='objectSet')
			elif filter_type == 11:
				candidates = obj.listRelatives(c=True, ad=True, type='constraint')
			else:
				candidates = [node for node in (dasset.getAssetTypeNodeFromHierarchy(obj) or [])
					if node in classified]
			for node in candidates or ():
				if node in seen:
					continue
				if len(nodes) >= rename_plan.MAX_RENAME_ITEMS:
					raise ValueError('Rename hierarchy exceeds the supported row limit')
				seen.add(node)
				nodes.append(node)

		sources, targets = [], []
		for node in nodes:
			name = node.name()
			if filter_text and filter_text not in name.lower():
				continue
			sources.append(name)
			targets.append(dasset.assetAutoRenameGen(obj=node, ignore=ignore))
		return rename_plan.preview(sources, targets)

	@browser.action
	def changeSelectObject(self):
		"""Prepare a complete selection preview, then replace both guarded lists."""
		_LOGGER.debug('Refreshing rename selection')
		with browser.updating(self):
			model = self._selection_preview()
			return self._apply_preview(model)


	def runRename(self):
		u"""Resolve every source identity before changing any node names."""
		try:
			srcStrList = pm.textScrollList(
				self.srcTextScroll, q=1, allItems=1) or []
			targetStrList = pm.textScrollList(
				self.targetTextScroll, q=1, allItems=1) or []
			plan = rename_plan.build_rename_plan(srcStrList, targetStrList)
			resolved = []
			seen_nodes = set()
			collisions = []
			for source_name, target_name in plan:
				# PyNodes retain identity when an ancestor/shape name changes.
				node = pm.PyNode(source_name)
				if node in seen_nodes:
					raise ValueError('More than one source names the same node')
				seen_nodes.add(node)
				resolved.append((node, target_name))
				if target_name != node.nodeName() and pm.objExists(target_name):
					collisions.append(target_name)
			if collisions:
				raise ValueError('Target node(s) already exist: {0}'.format(
					', '.join(collisions[:10])))
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Rename was cancelled: {0}'.format(exc))
			return False

		completed = 0
		succeeded = True
		try:
			for node, target_name in resolved:
				if target_name != node.nodeName():
					pm.rename(node, target_name)
					completed += 1
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Rename stopped after {0} completed rename(s): {1}. '
				'Completed changes remain; no automatic undo was attempted.'
				.format(completed, exc))
			succeeded = False
		try:
			if not browser.notify(self.changeSelectObject):
				return False
		except (RuntimeError, TypeError, ValueError) as exc:
			pm.warning('Rename view could not be refreshed: {0}'.format(exc))
			return False
		return succeeded
