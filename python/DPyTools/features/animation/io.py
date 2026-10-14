# -*- coding: utf-8 -*-
import pymel.core as pm
import maya.cmds as cmds
from DPyTools.features.animation.format import infinity_name as _infinity_name
from DPyTools.features.animation.format import remap_node_name as _remap_node_name
from DPyTools.features.animation.format import channel_matches as _channel_matches
from DPyTools.features.animation import parser as animation_parser
from DPyTools.features.animation import serializer as animation_serializer
from DPyTools.core.atomic_text import AtomicTextWriter
from DPyTools.ui import windows as ui_windows
from DPyTools.scene import selection as scene_selection

import platform
import os
import functools


'''
to run:
	>>> import DPyTools.features.animation.io as danimIO

'''


def _preserve_selection(function):
	"""Restore Maya selection even when a writer operation raises."""
	@functools.wraps(function)
	def wrapped(*args, **kwargs):
		selected = pm.ls(selection=True) or []
		try:
			return function(*args, **kwargs)
		finally:
			_restore_selection(selected)
	return wrapped


def getAttributeFromChBoxVis(node=''):
	attributes = pm.listAttr(node, channelBox=True) or []
	attributes.extend(pm.listAttr(node, keyable=True) or [])
	seen = set()
	result = []
	for attribute in attributes:
		if attribute in seen:
			continue
		seen.add(attribute)
		result.append(node + '.' + attribute)
	return result


def _long_node_name(node):
	matches = pm.ls(node, long=True) or []
	resolved = matches[0] if matches else node
	long_name = getattr(resolved, 'longName', None)
	return long_name() if callable(long_name) else str(resolved)


def _node_path_and_parent(node):
	matches = pm.ls(node, long=True) or []
	resolved = matches[0] if matches else node
	return _long_node_name(resolved), bool(
		pm.listRelatives(resolved, parent=True) or [])


def _split_attribute(attribute):
	node_name, separator, attribute_name = str(attribute).rpartition('.')
	if not separator or not node_name or not attribute_name:
		raise animation_serializer.AnimationSerializationError(
			'Animation destination must be a node attribute')
	return node_name, attribute_name


def _destination_attributes(curve, target_node):
	destinations = pm.listConnections(
		curve, plugs=True, source=False, destination=True,
		skipConversionNodes=True) or []
	target_path = _long_node_name(target_node)
	matched = []
	for destination in destinations:
		node_name, attribute_name = _split_attribute(destination)
		if _long_node_name(node_name) == target_path:
			matched.append((node_name, attribute_name))
	if not matched:
		raise animation_serializer.AnimationSerializationError(
			'Animation curve {0} has no destination on {1}'.format(
				curve, target_node))
	return matched


def _animation_records_from_curve(curve, target_node):
	times = pm.keyframe(curve, query=True)
	query = {
		'times': times,
		'values': pm.keyframe(curve, query=True, valueChange=True),
		'in_tangents': pm.keyTangent(
			curve, query=True, inTangentType=True),
		'out_tangents': pm.keyTangent(
			curve, query=True, outTangentType=True),
		'tangent_locks': pm.keyTangent(curve, query=True, lock=True),
		'weight_locks': pm.keyTangent(
			curve, query=True, weightLock=True),
		'breakdown_flags': [
			curve.isBreakdown(index)
			for index in range(len(times or []))],
		'in_angles': pm.keyTangent(curve, query=True, inAngle=True),
		'out_angles': pm.keyTangent(curve, query=True, outAngle=True),
		'in_weights': pm.keyTangent(curve, query=True, inWeight=True),
		'out_weights': pm.keyTangent(curve, query=True, outWeight=True),
	}
	weighted = pm.getAttr(str(curve) + '.weightedTangents')
	pre_infinity = _infinity_name(
		pm.getAttr(str(curve) + '.preInfinity'))
	post_infinity = _infinity_name(
		pm.getAttr(str(curve) + '.postInfinity'))

	records = []
	for node_name, attribute_name in _destination_attributes(
			curve, target_node):
		node_path, has_parent = _node_path_and_parent(node_name)
		records.append(animation_serializer.build_animation_record(
			attribute=attribute_name,
			node=node_path,
			has_parent=has_parent,
			weighted=weighted,
			pre_infinity=pre_infinity,
			post_infinity=post_infinity,
			**query))
	return records


def _static_records_from_node(node):
	records = []
	for current_attribute in getAttributeFromChBoxVis(str(node)):
		keys = pm.keyframe(current_attribute, query=True) or []
		incoming = pm.listConnections(
			current_attribute, source=True, destination=False) or []
		if keys or incoming:
			continue
		node_name, attribute_name = _split_attribute(current_attribute)
		node_path, has_parent = _node_path_and_parent(node_name)
		records.append(animation_serializer.build_static_record(
			attribute_name, node_path, has_parent,
			pm.getAttr(node_path + '.' + attribute_name)))
	return records


@_preserve_selection
def d_animWrite(filename='', hi=0):
	"""Write selected animation through the validated DAnim record model."""
	if os.path.isfile(filename):
		confirm = pm.confirmDialog(
			title='Confirm',
			message='File Exists,  Do you want to Overwrite it?',
			button=['Yes', 'No'],
			defaultButton='No',
			cancelButton='No',
			dismissString='No')
		if confirm != 'Yes':
			return False

	if hi:
		pm.select(hierarchy=True)
	objects = pm.ls(selection=True, long=True) or []
	if not objects:
		pm.warning('No objects are selected for animation export.')
		return False

	print "\nWriting Animation Curves...\n"
	try:
		with AtomicTextWriter(filename, mode='wb') as output:
			writer = animation_serializer.AnimationWriter(output)
			scene_path = (cmds.file(query=True, sceneName=True) or '')
			scene_path = scene_path.replace('\r', ' ').replace('\n', ' ')
			writer.write_header(
				'#Generated by DAnimIO\n#\n'
				'#DAnimIO written by domizo\n'
				'#Written out of {0}\n#\n\n'.format(scene_path))

			seen_destinations = set()
			for item in objects:
				curves = pm.listConnections(
					item, type='animCurve',
					source=True, destination=False,
					skipConversionNodes=True) or []
				for curve in curves:
					for record in _animation_records_from_curve(curve, item):
						identity = (
							str(curve), record.node, record.attribute)
						if identity in seen_destinations:
							continue
						seen_destinations.add(identity)
						writer.write_record(record)
				for record in _static_records_from_node(item):
					writer.write_record(record)

			writer.finish()
	except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
		pm.warning('Animation file was not written: {0}'.format(error))
		return False

	print "\nDone Writing Animation Curves\n"
	return True


def dk_filter_nodes(line='', paths=0, isReplace=1, search='', replace='',
		addTopOnly=0, prefix='', useChannel=0, chWin='',
		targetNamespace=''):
	"""Return historical node/attribute fields using the pure name mapper."""
	tokens = line.split()
	if len(tokens) not in (6, 7) or tokens[0] not in ('anim', 'static'):
		return ['dk_skip', 'dk_skip', 'dk_skip', 'dk_skip']

	node = _remap_node_name(
		tokens[3],
		has_parent=tokens[4] == '1',
		paths=bool(paths),
		namespace=targetNamespace,
		search=search if isReplace else '',
		replace=replace,
		prefix=prefix,
		prefix_top_only=bool(addTopOnly))
	attribute = tokens[2]
	return [node, attribute, node, attribute]


def _restore_selection(selected):
	return scene_selection.restore_selection(selected)


def _apply_static_record(current_attribute, record):
	connections = pm.listConnections(current_attribute, destination=False) or []
	if not pm.getAttr(current_attribute, lock=True) and not connections:
		pm.setAttr(current_attribute, record.value)
		return
	print ('Warning: Attribute is locked or connected - {0}\n'
		   .format(current_attribute))


def _apply_animation_record(current_attribute, record):
	# Create all keys first so the curve exists before setting curve-wide options.
	for key in record.keys:
		pm.setKeyframe(
			current_attribute, time=key.time, value=key.value,
			breakdown=key.breakdown)

	pm.keyTangent(
		current_attribute, edit=True, weightedTangents=record.weighted)

	for key in record.keys:
		pm.keyTangent(
			current_attribute, lock=key.tangent_locked, time=key.time)
		if record.weighted:
			pm.keyTangent(
				current_attribute, time=key.time,
				weightLock=key.weight_locked)

		options = {
			'edit': True,
			'absolute': True,
			'time': key.time,
			'inTangentType': key.in_tangent,
			'outTangentType': key.out_tangent,
		}
		if key.in_tangent == 'fixed':
			options['inAngle'] = key.in_angle
			options['inWeight'] = key.in_weight
		if key.out_tangent == 'fixed':
			options['outAngle'] = key.out_angle
			options['outWeight'] = key.out_weight
		pm.keyTangent(current_attribute, **options)

	pm.setInfinity(
		current_attribute,
		preInfinite=record.pre_infinity,
		postInfinite=record.post_infinity)


def d_animRead(filename='', paths=0, unkeyed=1, namespace='', is_replace=0,
		search='', replace='', prefix='', prefix_top_only=0, channels=None):
	"""Read one validated DAnim document and apply it to the current scene."""
	if not os.path.isfile(filename) or not os.access(filename, os.R_OK):
		pm.confirmDialog(
			title='Warning', message="File Doesn't Exist", button='Ok',
			defaultButton='Ok', cancelButton='Ok', dismissString='Ok')
		return False

	try:
		document = animation_parser.parse_animation_file(filename)
	except (IOError, OSError, animation_parser.AnimationFormatError) as error:
		pm.warning('Animation file was not loaded: {0}'.format(error))
		return False

	selected = pm.ls(selection=True) or []
	channel_scope = None if channels is None else set(channels)
	print "\nReading Animation Curves...\n"
	try:
		for record in document.records:
			node = _remap_node_name(
				record.node,
				has_parent=record.has_parent,
				paths=bool(paths),
				namespace=namespace,
				search=search if is_replace else '',
				replace=replace,
				prefix=prefix,
				prefix_top_only=bool(prefix_top_only))
			current_attribute = node + '.' + record.attribute
			if (channel_scope is not None
					and current_attribute not in channel_scope):
				continue

			if not pm.objExists(node):
				print ('Warning: {0} does not exist... Skipping\n'.format(node))
				continue
			if not (pm.ls(current_attribute) or []):
				print ('Warning: {0} does not exist... Skipping\n'
					   .format(current_attribute))
				continue

			if isinstance(record, animation_parser.StaticRecord):
				if unkeyed:
					_apply_static_record(current_attribute, record)
			else:
				_apply_animation_record(current_attribute, record)
	finally:
		_restore_selection(selected)

	print "\nDone Reading Animation Curves\n"
	return True


class dkAnim(object) :
	u"""AnimationIOツール"""


	m_uiname = "DAnimIO"
	m_uiname_ch = "dkAnim_channels"

	DKANIM_REFRESH = 1
	DKANIM_REFRESH_KEEP = 1

	def __init__(self):
		self.m_uiname = "DAnimIO"
		self.m_uiname_ch = "dkAnim_channels"

		self.DKANIM_REFRESH = 1
		self.DKANIM_REFRESH_KEEP = 1


	def openWin(self):
		u"""ウィンドウを表示"""

		#new window
		ui_windows.delete_window(self.m_uiname)
		ui_windows.delete_window(self.m_uiname_ch)

		win = pm.window(self.m_uiname, s=0, ip=1, iconName='dkAnim', title="dkAnim V .96", widthHeight=(600, 495))

		pm.columnLayout( co=('left', 10), rs=10)

		pm.frameLayout(borderVisible=1, labelVisible=1, li=10, h=125, w=575, labelAlign='center', label="WriteOptions", marginWidth=5, marginHeight=5)
		pm.columnLayout(rs=5)

		pm.rowColumnLayout(nc=2, cw=[(1, 500), (2, 50)], cs=(2, 5))
		self.dk_outname = pm.textField( text="out.danim", ed=1)
		pm.button( label="Browse", c=lambda *args: self.dk_browse_output(0))
		pm.setParent('..')#rowColumnLayout

		self.dk_hierarchy = pm.checkBox(v=1, label=" Save Hierarchy")
		pm.button(label="Write Anim", w=560, c=lambda *args: self.writeCB())
		pm.setParent('..')#columnLayout
		pm.setParent('..')#frameLayout


		pm.frameLayout(borderVisible=1, labelVisible=1, li=10, h=310, w=575, labelAlign="center", label="ReadOptions", marginWidth=5, marginHeight=5)
		pm.columnLayout(rs=5)


		pm.rowColumnLayout(nc=2, cw=[(1, 500), (2, 50)],cs=(2, 5))
		self.dk_inname = pm.textField( text="out.danim", cc=lambda *args: self.dk_setRefresh(keep_selection=0) )
		pm.button(label="Browse", c=lambda *args: self.dk_browse_output(1))
		pm.setParent('..')#rowColumnLayout

		pm.separator(height=10, w=565, style="out")


		pm.rowColumnLayout(nc=1, cs=(1, 30), cw=(1, 550))
		self.dk_doReplace = pm.checkBox(al="left", v=0, label="Use Search and Replace", cc=lambda *args: self.dk_setRefresh(keep_selection=1))

		pm.rowColumnLayout(nc=2, cw=[(1, 250), (2, 260)], cs=(2, 0))
		self.dk_search = pm.textFieldGrp(cw=[(1, 90), (2, 160)], label="Search For:", cc=lambda *args: self.dk_setRefresh(keep_selection=1) )
		self.dk_replace = pm.textFieldGrp(cw=[(1, 100), (2, 160)], label="Replace With:", cc=lambda *args: self.dk_setRefresh(keep_selection=1))
		pm.setParent('..')#rowColumnLayout

		pm.setParent('..')#rowColumnLayout

		pm.separator(height=5, w=565, style="out")


		pm.columnLayout(rs=5)
		pm.rowColumnLayout(nc=2, cs=[(1, 20),(2, 0)], cw=[(1, 300), (2, 175)])
		self.dk_prefix = pm.textFieldGrp(cw=[(1, 100), (2, 160)], label="Add Prefix:", cc=lambda *args: self.dk_setRefresh(keep_selection=1))
		self.dk_topNodes= pm.checkBox(v=0, label="Add To Top Nodes Only", cc=lambda *args: self.dk_setRefresh (keep_selection=1))
		pm.setParent('..')#rowColumnLayout
		pm.setParent('..')#columnLayout

		pm.separator( height=5, w=565, style="out")

		pm.columnLayout(rs=5)
		pm.rowColumnLayout(nc=2, cw=[(1, 290),  (2, 175)], cs=[(2, 0),(1, 30)])
		self.dk_paths = pm.checkBox(al="left", v=1, label="Load Explicit Node Paths", cc=lambda *args: self.dk_setRefresh(keep_selection=1))
		self.dk_unKeyed = pm.checkBox(v=0, label="Load Un-Keyed Attributes", cc=lambda *args: self.dk_setRefresh(keep_selection=0))
		pm.setParent('..')#rowColumnLayout
		pm.setParent('..')#columnLayout

		pm.separator(height=5, w=565, style="out")

		pm.rowColumnLayout(cs=(1, 30), cw=[(1, 290),(2, 200)], nc=2)
		self.dk_useChannels = pm.checkBox( label="Limit Channels to Scope", al="left")
		pm.button(label='Define Channel Scope',  c=lambda *args: self.dk_channels())
		pm.setParent('..')#rowColumnLayout

		pm.separator( height=5, w=565, style="out")

		pm.button(label="Read Anim", w=560, c=lambda *args: self.runReadAnimCB())

		pm.setParent('..')
		pm.setParent('..')
		pm.setParent('..')

		#show window
		win.show()
		#showWindow dkAnim
		pm.window(self.m_uiname, e=1, w=600, h=495)



	def dk_setRefresh(self, keep_selection=0):

		self.DKANIM_REFRESH = 1
		self.DKANIM_REFRESH_KEEP = keep_selection

		if pm.window(self.m_uiname_ch, ex=1) :
			self.dk_updateChanLabel()


	def dk_channels(self):

		if pm.window(self.m_uiname_ch, ex=1):
			if self.DKANIM_REFRESH == 1 :
				self.dk_loadChannels()

		else :

			pm.window(self.m_uiname_ch, ret=1,ip=1, iconName="Channels", w=375, h=400, title="Channels")
			self.dk_scroll_layout = pm.scrollLayout(horizontalScrollBarThickness=16, verticalScrollBarThickness=16, rc=lambda *args: self.dk_resize_chanList())
			pm.columnLayout(rs=5)
			pm.rowColumnLayout(cs=[(2, 5),( 3, 5)], cw=[(1, 75),(2, 75), (3, 150)], nc=3)
			pm.button(label="Add", c=lambda *args: self.dk_matchChannels(1))
			pm.button(label="Remove", c=lambda *args: self.dk_matchChannels(0))
			self.dk_wildCard = pm.textField(tx="*")
			pm.setParent('..')#rowColumnLayout

			pm.rowColumnLayout(cs=(2, 5), cw=[(1, 100), (2, 250)],  nc=2)
			pm.button(label="Refresh List", c=lambda *args: self.dk_loadChannels())
			self.dk_chanLabel = pm.text(label="1000 Channels Scoped")
			pm.setParent('..')#rowColumnLayout

			self.dk_chanList = pm.textScrollList( h=(pm.scrollLayout(self.dk_scroll_layout, q=1, h=1)-60),
								allowMultiSelection=1, sc=lambda *args: self.dk_updateChanLabel())


			self.dk_loadChannels()

			pm.window(self.m_uiname_ch, e=1, w=375, h=400)
			self.dk_matchChannels(1)

		pm.showWindow(self.m_uiname_ch)
		self.dk_resize_chanList()


	def dk_resize_chanList(self):

		filteredNames = []
		nu = pm.textScrollList(self.dk_chanList, q=1, ni=1)
		# print ('nu:{n}'.format(n=nu))
		pm.textScrollList(self.dk_chanList, e=1, nr=nu )
		pm.textScrollList(self.dk_chanList, e=1,  a="tempSizerLine")
		pm.textScrollList(self.dk_chanList, e=1,  ri="tempSizerLine")

		if pm.textScrollList(self.dk_chanList, q=1, w=1) < pm.scrollLayout(self.dk_scroll_layout, q=1, w=1) :
			pm.textScrollList(self.dk_chanList, e=1,  w=(pm.scrollLayout(self.dk_scroll_layout, q=1, w=1)-25) )


	def dk_updateChanLabel(self):


		num = pm.textScrollList(self.dk_chanList, q=1, nsi=1)

		s = "s"
		state = "Refreshed"

		if num == 1 :
			s = ""

		if self.DKANIM_REFRESH :
			state = "Refresh Needed"



		pm.text(self.dk_chanLabel, e=1, label=(str(num) + " Channel" + s + " Scoped (" + state + ")") )





	def dk_loadChannels(self, filename='', selected=None):
		if selected is None:
			selected = []
		if not filename:
			filename = pm.textField(self.dk_inname, query=True, text=True)

		pm.textScrollList(
			self.dk_chanList, edit=True, width=10, height=10,
			visible=False, allowMultiSelection=False, removeAll=True)

		if not os.path.isfile(filename) or not os.access(filename, os.R_OK):
			document = None
		else:
			try:
				document = animation_parser.parse_animation_file(filename)
			except (IOError, OSError, animation_parser.AnimationFormatError) as error:
				pm.warning('Animation channel list was not loaded: {0}'.format(error))
				document = None

		if document is not None:
			paths = pm.checkBox(self.dk_paths, query=True, value=True)
			include_static = pm.checkBox(
				self.dk_unKeyed, query=True, value=True)
			is_replace = pm.checkBox(
				self.dk_doReplace, query=True, value=True)
			search = pm.textFieldGrp(self.dk_search, query=True, text=True)
			replace = pm.textFieldGrp(self.dk_replace, query=True, text=True)
			prefix = pm.textFieldGrp(self.dk_prefix, query=True, text=True)
			prefix_top_only = pm.checkBox(
				self.dk_topNodes, query=True, value=True)

			for record in document.records:
				if (isinstance(record, animation_parser.StaticRecord)
						and not include_static):
					continue
				node = _remap_node_name(
					record.node,
					has_parent=record.has_parent,
					paths=paths,
					search=search if is_replace else '',
					replace=replace,
					prefix=prefix,
					prefix_top_only=prefix_top_only)
				pm.textScrollList(
					self.dk_chanList, edit=True,
					append=node + '.' + record.attribute,
					width=10, height=10, visible=False,
					allowMultiSelection=False)

		self.DKANIM_REFRESH = 0
		if self.DKANIM_REFRESH_KEEP == 1:
			for item in selected:
				pm.textScrollList(
					self.dk_chanList, edit=True, selectIndexedItem=item)

		pm.textScrollList(
			self.dk_chanList, edit=True, width=10, height=10,
			visible=True, allowMultiSelection=True)
		self.DKANIM_REFRESH_KEEP = 1
		self.dk_updateChanLabel()
		self.dk_resize_chanList()


	def dk_matchChannels(self, mode=0) :


		all_items = pm.textScrollList(self.dk_chanList, q=1, ai=1) or []
		text = pm.textField(self.dk_wildCard, q=1, tx=1)

		for index, item in enumerate(all_items, 1):
			if _channel_matches(item, text):
				if mode :
					pm.textScrollList(self.dk_chanList, e=1, sii=index)
				else :
					pm.textScrollList(self.dk_chanList, e=1, dii=index)

		self.dk_updateChanLabel()


	def dk_browse_output(self, field=0 ):

		file =''
		if field == 0:

			if platform.system() == 'Windows' :
				pm.fileBrowserDialog( m=1, fc=lambda *args: self.dk_edit_output(), ft="text", an="Save")
			else  :

				file = pm.fileDialog()
				pm.textField(self.dk_outname, e=1, tx=file)


		if  field == 1:

			file = pm.fileDialog()
			pm.textField(self.dk_inname, e=1, tx=file)


	def dk_edit_output(self, fileName='',  fileType='') :

		pm.textField(self.dk_outname, e=1, tx=fileName)



	def writeCB(self) :

		filename = (pm.textField(self.dk_outname, q=1, text=1))
		hi = (pm.checkBox(self.dk_hierarchy, q=1, v=1))

		d_animWrite(filename=filename, hi=hi)






	def runReadAnimCB(self):

		f = pm.textField(self.dk_inname, q=1, text=1)
		p = pm.checkBox(self.dk_paths, q=1, v=1)
		unkey = pm.checkBox(self.dk_unKeyed, q=1, v=1)
		channels = None
		if pm.window(self.m_uiname_ch, exists=True):
			channels = pm.textScrollList(
				self.dk_chanList, query=True, selectItem=True) or []
		d_animRead(
			filename=f,
			paths=p,
			unkeyed=unkey,
			is_replace=pm.checkBox(self.dk_doReplace, q=1, v=1),
			search=pm.textFieldGrp(self.dk_search, q=1, text=1),
			replace=pm.textFieldGrp(self.dk_replace, q=1, text=1),
			prefix=pm.textFieldGrp(self.dk_prefix, q=1, text=1),
			prefix_top_only=pm.checkBox(self.dk_topNodes, q=1, v=1),
			channels=channels)
