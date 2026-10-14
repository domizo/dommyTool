# -*- coding: utf-8 -*-
import pymel.core as pm

import DPyTools.general.selectOp as selOp
import DPyTools.joint.JointOp as jntOp

import DPyTools.mesh.meshOp as meshOp
import DPyTools.mesh.uvOp as uvOp
from DPyTools.ui import windows as ui_windows
from DPyTools.core.geometry import position3
from DPyTools.core.numeric import finite_number
from DPyTools.core import strings
from DPyTools.scene import parameters, selection as scene_selection
from DPyTools.scene.selection import resolve_shape
from DPyTools.features.rigging.mirror import mirror_joints
from DPyTools.features.rigging import rivets as rivet_tools



'''
to run:
	>>> from DPyTools.rigging.DFaceJointTool import DFaceJointTool
	>>> DFaceJointTool().openWin()

'''


class DFaceJointTool(object):
	u"""フェイス用のジョイント設定ツール"""

	m_uiname = 'DFaceJointTool'

	def __init__(self):
		self.m_uiname = 'DFaceJointTool'
		self._binding_active = False


	def openWin(self):
		u"""ウィンドウを表示"""


		ui_windows.delete_window(self.m_uiname)

		win = pm.window(self.m_uiname, widthHeight=(320, 640))

		#layout
		self.form = pm.formLayout(numberOfDivisions=100)
		self.column = pm.columnLayout(rowSpacing=10, adjustableColumn=True)



		#############Fit Joint#############
		pm.frameLayout( label="Fit Joint to Mesh", collapsable=1 )

		pm.rowLayout(numberOfColumns=3, adjustableColumn=2)
		pm.text(label='Target Mesh', width=80, align='right')
		self.fitMeshNameText = pm.textField(placeholderText='mesh name')
		pm.button(label='Set', c=lambda *args: self.setFitMeshCB())
		pm.setParent( '..' )#rowLayout

		pm.button(label='Fit Joint to Mesh!!', c=lambda *args: self.runFitJntToMeshCB())
		pm.setParent( '..' )#frameLayout
		###################################


		#############Mirror Joint#############
		pm.frameLayout( label="Mirror", collapsable=1 )

		self.mirrorOrientCheck = pm.checkBox( label='Mirror Orient', value=0 )
		self.mirrorSrcRenameCheck = pm.checkBox( label='Source Rename', value=1 )
		self.mirroredRenameCheck = pm.checkBox( label='Mirrored Rename', value=1 )

		pm.button(label='Mirror Joint', c=lambda *args: self.runMirrorJntCB())

		pm.setParent( '..' )#frameLayout
		###################################

		############# Skin Bind #############

		pm.frameLayout( label="Skin Bind", collapsable=1 )

		pm.button(label='Bind Face', c=lambda *args: self.runBindCB())
		pm.button(label='ReSelect Only', c=lambda *args: self.runReSelectCB())

		pm.setParent( '..' )#frameLayout

		###################################


		#############Mirror Joint#############
		pm.frameLayout( label="Rivet Joint", collapsable=1 )
		self.rivetRotLink = pm.checkBox( label='Rotation Link', value=1 )
		pm.button(label='Create Rivet Joint', c=lambda *args: self.runCreateRivetCB())

		pm.setParent( '..' )#frameLayout

		###################################

		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.column, 'top', 5), (self.column, 'left', 5), (self.column, 'right', 5), (self.column, 'bottom', 5)]
			)


		#show window
		win.show()


	def setFitMeshCB(self) :
		u"""Set one admitted mesh; failed selection leaves the field unchanged."""
		try:
			selection = pm.ls(sl=True) or []
			if len(selection) != 1:
				raise ValueError('Please select exactly one target mesh.')
			mesh = resolve_shape(selection[0])
			pm.textField(self.fitMeshNameText, e=1, text=mesh.longName())
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning(str(error))
			return False
		return True




	def runFitJntToMeshCB(self) :
		u"""Dispatch admitted fitting; report failures without false completion text."""
		try:
			target_mesh = pm.textField(self.fitMeshNameText, q=1, text=1)
			if not isinstance(target_mesh, strings.STRING_TYPES) or not target_mesh:
				raise ValueError('Please set a target mesh shape.')
			joints = selOp.getJointlist() or []
			if not joints:
				raise ValueError('Please select joints for fitting to mesh.')
			completed = jntOp.fitJointToMesh(target_mesh=target_mesh, joint_list=joints)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Joints were not fully fitted: {0}'.format(error))
			return False
		return bool(completed)




	def runMirrorJntCB(self):
		u"""Mirror validated joint roots using every current UI option."""
		try:
			joints = selOp.getJointlist() or []
			if not joints:
				pm.warning('Please select joints for mirroring.')
				return False
			completed = mirror_joints(joints,
				mirror_behavior=pm.checkBox(self.mirrorOrientCheck, q=1, value=1),
				rename_mirrored=pm.checkBox(self.mirroredRenameCheck, q=1, value=1),
				rename_sources=pm.checkBox(self.mirrorSrcRenameCheck, q=1, value=1))
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Joints were not fully mirrored: {0}'.format(error))
			return False
		return bool(completed)


	def _binding_targets(self):
		"""Resolve the complete mesh/unique Joint batch before changing selection."""
		target_mesh = pm.textField(self.fitMeshNameText, q=1, text=1)
		if not isinstance(target_mesh, strings.STRING_TYPES) or not target_mesh:
			raise ValueError('Please set a target mesh shape.')
		mesh = resolve_shape(target_mesh)
		joints = parameters.resolve_nodes(selOp.getJointlist() or [])
		if any(joint.nodeType() != 'joint' for joint in joints):
			raise ValueError('Please select joints for binding to mesh.')
		return mesh, joints

	def _run_bind_selection(self, bind):
		"""Keep successful selection changes; restore retained inputs on failure.

		Selection restoration is best effort, not skin/history rollback or Undo.
		The guard covers same-window synchronous callbacks, including restoration.
		"""
		if self._binding_active:
			return False
		self._binding_active = True
		selection_attempted = completed = False
		selected = None
		try:
			mesh, joints = self._binding_targets()
			current = pm.ls(sl=True)
			current = [] if current is None else current
			if not isinstance(current, (list, tuple)):
				raise ValueError('Previous selection must be a sequence')
			selected = tuple(current)
			selection_attempted = True
			pm.select([mesh] + joints, replace=True)
			if bind:
				pm.skinCluster(joints, mesh, skinMethod=1, toSelectedBones=True,
					maximumInfluences=3, normalizeWeights=1, dropoffRate=3.0)
			completed = True
		except (RuntimeError, TypeError, ValueError) as error:
			if bind:
				pm.warning('Face binding did not complete; native skin/history edits may remain: {0}'.format(error))
			else:
				pm.warning('Joint reselection did not complete: {0}'.format(error))
			return False
		finally:
			try:
				if selection_attempted and not completed:
					scene_selection.restore_selection(selected)
			finally:
				self._binding_active = False
		return True

	def runBindCB(self):
		"""Bind admitted selected Joints using the historical skin settings."""
		return self._run_bind_selection(bind=True)

	def runReSelectCB(self):
		"""Select admitted mesh/Joints for Maya's default binding tools."""
		return self._run_bind_selection(bind=False)


	def runCreateRivetCB(self):
		u"""Create rivets after admitting all target vertices and world-space UVs."""
		try:
			target_mesh = resolve_shape(
				pm.textField(self.fitMeshNameText, q=1, text=1))
			vertex_ids, positions = meshOp.getSelectedMeshIds(target_mesh=target_mesh)
			if not positions:
				pm.warning('Please select vertices on the target mesh.')
				return False
			if len(vertex_ids) != len(positions):
				raise ValueError('Vertex IDs and positions must have matching counts')
			if len(positions) > rivet_tools.MAX_RIVETS:
				raise ValueError('Vertex rivet selection exceeds the generation limit')
			positions = [position3(point) for point in positions]
			uvs = uvOp.getUVfromPoint(meshObj=target_mesh, pt_list=positions)
			if len(uvs) != len(positions) or any(len(uv) != 2 for uv in uvs):
				raise ValueError('Every selected vertex requires a UV pair')
			uvs = [tuple(finite_number(value, 'Rivet UV') for value in uv) for uv in uvs]
			rotation_link = pm.checkBox(self.rivetRotLink, q=1, value=1)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Rivets were not created: {0}'.format(error))
			return False
		return bool(rivet_tools.createVertexRivets(target_mesh, positions, uvs, rotation_link))
