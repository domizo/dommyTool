import pymel.core as pm
import maya.api.OpenMaya as om

import DPyTools.node.DNodeOp as DNodeOp
import DPyTools.general.groupActions as DPGrp
from DPyTools.features.rigging.face import specs as face_specs
from DPyTools.ui import windows as ui_windows


'''
to run:
	>>> import DPyTools.rigging.DEyeRigOp as DEyeRT
	>>> DEyeRT.openWin()

'''


#create create setup window
class EyeRiggingTool(object):

	#eye object
	m_eye_l_name = 'eye_root_l'
	m_eye_r_name = 'eye_root_r'

	m_eye_l = None
	m_eye_r = None

	#eye joint
	m_eye_l_jnt_name = 'eye_l_jnt'
	m_eye_r_jnt_name = 'eye_r_jnt'

	m_eye_l_jnt = None
	m_eye_r_jnt = None

	###aim locator
	m_aim_loc_l = None
	m_aim_loc_r = None
	m_aim_loc_c = None
	m_eye_aim_root_trans = None

	###dummy transforms
	'''
	dummy is for blend constraint and controller eye motion
	'''

	#for aim
	m_eye_aim_dmy_loc_l_name = "m_eye_aim_dmy_loc_l"
	m_eye_aim_dmy_loc_r_name = "m_eye_aim_dmy_loc_r"
	m_eye_aim_dmy_loc_l = None
	m_eye_aim_dmy_loc_r = None

	#for controller
	m_eye_con_dmy_loc_l_name = "m_eye_con_dmy_loc_l"
	m_eye_con_dmy_loc_r_name = "m_eye_con_dmy_loc_r"
	m_eye_con_dmy_loc_l = None
	m_eye_con_dmy_loc_r = None


	#orient const for blend
	m_orientAimConsL_name = 'aim_con_blend_ori_cons_l'
	m_orientAimConsR_name = 'aim_con_blend_ori_cons_r'
	m_orientAimConsL = None
	m_orientAimConsR = None

	####eye controller
	m_con_lookAt_l_name = 'con_lookAt_l'
	m_con_lookAt_r_name = 'con_lookAt_r'
	m_con_lookAt_c_name = 'con_lookAt_c'
	m_con_lookAtTrans_name = 'lookAt_freeTransform'
	m_control_dist = 15.0


	##eye for blend shape
	m_face_bs_root = ''
	m_eye_look_bs_list = face_specs.EYE_LOOK_ALL
	m_eye_look_bs_list_l = face_specs.EYE_LOOK_LEFT
	m_eye_look_bs_list_r = face_specs.EYE_LOOK_RIGHT


	def __init__(self):
		#init items state
		self.updateEyeRigCB()

	"""
	Open window for creating eye rigging
	"""
	def openWin(self):
		uiname = "DEyeRigToolWin"

		ui_windows.delete_window(uiname)

		#create window component
		self.window = pm.window(uiname,widthHeight=(480, 320))
		self.form = pm.formLayout(numberOfDivisions=100)
		self.column = pm.columnLayout( columnAttach=('both', 5), rowSpacing=10, columnWidth=250 )

		pm.button(label='Create Eye Joint',c = lambda *args: self.createEyeBoneCB() )
		pm.button(label='Make Eye To Joint',c = lambda *args: self.makeEyeJointParentCB() )
		pm.button(label='Create Eye Controller',c = lambda *args: self.createEyeConstCB() )

		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.column, 'top', 5), (self.column, 'left', 5), (self.column, 'right', 5),(self.column, 'bottom', 5)]
			)

		#open window
		pm.showWindow( self.window )


	def setupEyeObject(self):
		#set eye item
		self.m_eye_l = DNodeOp.getItem(self.m_eye_l_name)
		self.m_eye_r = DNodeOp.getItem(self.m_eye_r_name)

	def setupEyeJoint(self):
		#set joint item
		self.m_eye_l_jnt = DNodeOp.getItem(self.m_eye_l_jnt_name)
		self.m_eye_r_jnt = DNodeOp.getItem(self.m_eye_r_jnt_name)

	def setupAimLoc(self):
		#set aim locator
		self.m_aim_loc_l = DNodeOp.getItem(self.m_con_lookAt_l_name)
		self.m_aim_loc_r = DNodeOp.getItem(self.m_con_lookAt_r_name)
		self.m_aim_loc_c = DNodeOp.getItem(self.m_con_lookAt_c_name)

		self.m_eye_aim_root_trans = DNodeOp.getItem(self.m_con_lookAtTrans_name)

	def setupAimDummy(self):
		#set aim dummy
		self.m_eye_aim_dmy_loc_l = DNodeOp.getItem(self.m_eye_aim_dmy_loc_l_name)
		self.m_eye_aim_dmy_loc_r = DNodeOp.getItem(self.m_eye_aim_dmy_loc_r_name)

	def setupConDummy(self):
		#set con dummy
		self.m_eye_con_dmy_loc_l = DNodeOp.getItem(self.m_eye_con_dmy_loc_l_name)
		self.m_eye_con_dmy_loc_r = DNodeOp.getItem(self.m_eye_con_dmy_loc_r_name)

	def setBlendConst(self):
		#set ori const
		self.m_orientAimConsL = DNodeOp.getItem(self.m_orientAimConsL_name)
		self.m_orientAimConsR = DNodeOp.getItem(self.m_orientAimConsR_name)


	def getConDummyList(self):
		return [self.m_eye_con_dmy_loc_l, self.m_eye_con_dmy_loc_r]


	def getBlendOrientConst(self):
		return [self.m_orientAimConsL, self.m_orientAimConsR]


	def updateEyeRigCB(self):

		##reset value
		self.m_eye_l=None
		self.m_eye_r=None
		self.m_eye_l_jnt=None
		self.m_eye_r_jnt=None

		self.m_aim_loc_l=None
		self.m_aim_loc_r=None
		self.m_aim_loc_c=None

		self.m_eye_aim_root_trans = None

		#set dummy
		self.m_eye_aim_dmy_loc_l = None
		self.m_eye_aim_dmy_loc_r = None

		self.m_eye_con_dmy_loc_l = None
		self.m_eye_con_dmy_loc_r = None

		#blend const
		self.m_orientAimConsL = None
		self.m_orientAimConsR = None


		#set items
		self.setupEyeObject()
		self.setupEyeJoint()

		self.setupAimLoc()

		#set dummy
		self.setupAimDummy()
		self.setupConDummy()

		#set blend
		self.setBlendConst()



	def echoState(self):

		print ('\n\nEye state------------\n')

		print ('m_eye_l : {name}'.format(name=self.m_eye_l))
		print ('m_eye_r : {name}'.format(name=self.m_eye_r))
		print ('m_eye_l_jnt : {name}'.format(name=self.m_eye_l_jnt))
		print ('m_eye_r_jnt : {name}'.format(name=self.m_eye_r_jnt))

		print ('\n')

		print ('m_aim_loc_l : {name}'.format(name=self.m_aim_loc_l))
		print ('m_aim_loc_r : {name}'.format(name=self.m_aim_loc_r))
		print ('m_aim_loc_c : {name}'.format(name=self.m_aim_loc_c))
		print ('m_eye_aim_root_trans : {name}'.format(name=self.m_eye_aim_root_trans))

		print ('\n')

		print ('m_eye_aim_dmy_loc_l : {name}'.format(name=self.m_eye_aim_dmy_loc_l))
		print ('m_eye_aim_dmy_loc_r : {name}'.format(name=self.m_eye_aim_dmy_loc_r))
		print ('m_eye_con_dmy_loc_l : {name}'.format(name=self.m_eye_con_dmy_loc_l))
		print ('m_eye_con_dmy_loc_r : {name}'.format(name=self.m_eye_con_dmy_loc_r))

		print ('\n')

		print ('m_orientAimConsL : {name}'.format(name=self.m_orientAimConsL))
		print ('m_orientAimConsR : {name}'.format(name=self.m_orientAimConsR))

		print ('\n')



	def checkEyeRigCB(self):

		#update data
		self.updateEyeRigCB()

		#check eye object
		if self.m_eye_l is None :
			pm.warning('Not exist {name} object!'.format(name=self.m_eye_l_name))
			return False

		if self.m_eye_r is None :
			pm.warning('Not exist {name} object!'.format(name=self.m_eye_r_name))
			return False


		return True

	def checkEyeObject(self):
		if self.m_eye_l is None or self.m_eye_r is None :
			return False

		return True

	def checkEyeJointExist(self):
		#check exist items
		if (pm.objExists(self.m_eye_l_jnt_name)
				and pm.objExists(self.m_eye_r_jnt_name)):
			self.setupEyeJoint()
			return True

		if self.m_eye_l_jnt is None or self.m_eye_r_jnt is None :
			return False

		return True

	def checkEyeAimLoc(self):

		#check exist
		if pm.objExists(self.m_con_lookAt_l_name) and pm.objExists(self.m_con_lookAt_r_name) and pm.objExists(self.m_con_lookAt_c_name) and pm.objExists(self.m_con_lookAtTrans_name):
			self.setupAimLoc()
			return True

		if (not self.m_aim_loc_l) or (not self.m_aim_loc_r) or (not self.m_aim_loc_c) or (not self.m_eye_aim_root_trans):
			return False

		return True

	def checkEyeAimDummy(self):
		#check dummy
		if (pm.objExists(self.m_eye_aim_dmy_loc_l_name)
				and pm.objExists(self.m_eye_aim_dmy_loc_r_name)):
			self.setupAimDummy()
			return True

		if (not self.m_eye_aim_dmy_loc_l) or (not self.m_eye_aim_dmy_loc_r):
			return False

		return True

	def checkEyeCntlrDummy(self):
		#check dummy exist
		if (pm.objExists(self.m_eye_con_dmy_loc_l_name)
				and pm.objExists(self.m_eye_con_dmy_loc_r_name)):
			self.setupConDummy()
			return True

		if (not self.m_eye_con_dmy_loc_l) or (not self.m_eye_con_dmy_loc_r):
			return False

		return True


	def resetEyeRig(self):

		if self.checkEyeAimLoc() :
			pm.setAttr( self.m_aim_loc_l+('.translate'), [0, 0, 0])
			pm.setAttr( self.m_aim_loc_r+('.translate'), [0, 0, 0])
			pm.setAttr( self.m_aim_loc_c+('.translate'), [0, 0, 0])



	def makeEyeJointParentCB(self):
		if not self.checkEyeObject():
			pm.warning('Not exist eye object!')
			return

		if not self.checkEyeJointExist():
			pm.warning('Not exist eye joint!')
			return


		#create parent constraint
		pm.parentConstraint( self.m_eye_l_jnt, self.m_eye_l )
		pm.parentConstraint( self.m_eye_r_jnt, self.m_eye_r )

	def getEyePosition(self):
		#get eye position
		eyeWorldPosL = om.MVector(pm.xform(self.m_eye_l, q=True, ws=True, t=True))
		eyeWorldPosR = om.MVector(pm.xform(self.m_eye_r, q=True, ws=True, t=True))
		eyeRotPivotL = om.MVector(pm.xform(self.m_eye_l, q=True, rotatePivot=True))
		eyeRotPivotR = om.MVector(pm.xform(self.m_eye_r, q=True, rotatePivot=True))

		eyePosL = eyeWorldPosL+eyeRotPivotL
		eyePosR = eyeWorldPosR+eyeRotPivotR

		return [eyePosL, eyePosR]

	def createEyeBoneCB(self):

		#init value
		self.updateEyeRigCB()

		#check eye joint exist
		if self.checkEyeJointExist():
			self.setupEyeJoint()
			pm.warning('Eye joint already exist!')
			return True

		if not self.checkEyeObject():
			return False

		joint_exists = (
			pm.objExists(self.m_eye_l_jnt_name),
			pm.objExists(self.m_eye_r_jnt_name),
		)
		if any(joint_exists):
			pm.warning('Only one eye joint exists; resolve the partial rig first.')
			return False

		'''
		#get eye position
		eyeWorldPosL = om.MVector(pm.xform(self.m_eye_l, q=True, ws=True, t=True))
		eyeWorldPosR = om.MVector(pm.xform(self.m_eye_r, q=True, ws=True, t=True))
		eyeRotPivotL = om.MVector(pm.xform(self.m_eye_l, q=True, rotatePivot=True))
		eyeRotPivotR = om.MVector(pm.xform(self.m_eye_r, q=True, rotatePivot=True))

		eyePosL = eyeWorldPosL+eyeRotPivotL
		eyePosR = eyeWorldPosR+eyeRotPivotR
		'''

		eyePosList = self.getEyePosition()

		pm.select( clear=True )

		#create eye joint
		self.m_eye_l_jnt = pm.joint( p=eyePosList[0], name=self.m_eye_l_jnt_name )
		pm.select( clear=True )
		pm.joint( self.m_eye_l_jnt, e=True, zso=True, oj='xyz' )

		self.m_eye_r_jnt = pm.joint( p=eyePosList[1], name=self.m_eye_r_jnt_name )
		pm.select( clear=True )
		pm.joint( self.m_eye_r_jnt, e=True, zso=True, oj='xyz' )

		return True

	def createEyeDummyItem( self, nameL='dummy_l', nameR='dummy_r' ):
		if pm.objExists(nameL) or pm.objExists(nameR):
			pm.warning(
				'Cannot create dummy pair while one of the requested names exists.')
			return []

		#get eye position
		eyePosList = self.getEyePosition()

		#create root Locator
		newDummyL = pm.createNode( 'transform', n=nameL )
		pm.move( eyePosList[0].x,eyePosList[0].y,eyePosList[0].z, newDummyL )
		newDummyR = pm.createNode( 'transform', n=nameR )
		pm.move( eyePosList[1].x,eyePosList[1].y,eyePosList[1].z, newDummyR )


		return[newDummyL,newDummyR]



	def parentToHeadCB(self, head='head_jnt'):
		#check head joint
		if not pm.objExists(head) :
			pm.warning('Head joint is not exist!(head_jnt)')
			return True

		# print ('m_eye_l_jnt : {0} type : {1}'.format(self.m_eye_l_jnt, type(self.m_eye_l_jnt)))
		# print ('m_eye_r_jnt : {0} type : {1}'.format(self.m_eye_r_jnt, type(self.m_eye_r_jnt)))
		# print ('head : {0}'.format(head))


		pm.parent( self.m_eye_l_jnt, head)
		pm.parent( self.m_eye_r_jnt, head)


	def createEyeAimLoc(self):
		"""
		Create locator for eye aim constraint.

		"""

		if self.checkEyeAimLoc():
			return

		#get eye position
		eyePosL = om.MVector(pm.xform(self.m_eye_l_jnt, q=True, ws=True, t=True))
		eyePosR = om.MVector(pm.xform(self.m_eye_r_jnt, q=True, ws=True, t=True))

		#get aim position
		eyeLAim_Pos = om.MVector(eyePosL.x,eyePosL.y, eyePosL.z+self.m_control_dist)
		eyeRAim_Pos = om.MVector(eyePosR.x,eyePosR.y, eyePosR.z+self.m_control_dist)

		#create aim Locator
		self.m_aim_loc_l = pm.spaceLocator( name=self.m_con_lookAt_l_name )
		self.m_aim_loc_r = pm.spaceLocator( name=self.m_con_lookAt_r_name )
		pm.move( eyeLAim_Pos.x,eyeLAim_Pos.y,eyeLAim_Pos.z, self.m_aim_loc_l )
		pm.move( eyeRAim_Pos.x,eyeRAim_Pos.y,eyeRAim_Pos.z, self.m_aim_loc_r )

		#create reset locator LR
		aim_l_freeze_loc = DPGrp.createEmpty(name='eye_aim_reset_l', parent=None, position=(eyeLAim_Pos.x,eyeLAim_Pos.y,eyeLAim_Pos.z))
		aim_r_freeze_loc = DPGrp.createEmpty(name='eye_aim_reset_r', parent=None, position=(eyeRAim_Pos.x,eyeRAim_Pos.y,eyeRAim_Pos.z))
		#aim_l_freeze_loc = pm.duplicate( self.m_aim_loc_l, name='eye_aim_reset_loc_l' )
		#aim_r_freeze_loc = pm.duplicate( self.m_aim_loc_r, name='eye_aim_reset_loc_r' )

		pm.parent(self.m_aim_loc_l, aim_l_freeze_loc)
		pm.parent(self.m_aim_loc_r, aim_r_freeze_loc)

		#calc mid position
		midPt = eyeLAim_Pos+(eyeRAim_Pos-eyeLAim_Pos)*.5

		#create center Locator
		self.m_aim_loc_c = pm.spaceLocator( name=self.m_con_lookAt_c_name )
		pm.move( midPt.x,midPt.y,midPt.z, self.m_aim_loc_c )

		#create root Locator
		self.m_eye_aim_root_trans = pm.createNode( 'transform', n=self.m_con_lookAtTrans_name )
		pm.move( midPt.x,midPt.y,midPt.z, self.m_eye_aim_root_trans )

		#parent
		pm.parent(aim_l_freeze_loc, self.m_aim_loc_c)
		pm.parent(aim_r_freeze_loc, self.m_aim_loc_c)
		pm.parent(self.m_aim_loc_c, self.m_eye_aim_root_trans)



	def createEyeConstCB(self):

		#check joint exist
		if not self.checkEyeJointExist():
			pm.warning('Eye joint is not exist!')
			return


		self.createEyeAimLoc()


		#create Aim Constraint
		aim_const_l = pm.aimConstraint( self.m_aim_loc_l, self.m_eye_l_jnt, skip=["z"], aimVector=(0,0,1), upVector=(0,1,0),w=1.0 )
		aim_const_r = pm.aimConstraint( self.m_aim_loc_r, self.m_eye_r_jnt, skip=["z"], aimVector=(0,0,1), upVector=(0,1,0),w=1.0 )


	def createBlendAimAndCons(self):

		#check joint exist
		if not self.checkEyeJointExist():
			pm.warning('Eye joint is not exist!')
			return

		#create aim locator
		self.createEyeAimLoc()

		#create aim dummy
		if not self.checkEyeAimDummy():
			self.createEyeDummyItem(
				self.m_eye_aim_dmy_loc_l_name,
				self.m_eye_aim_dmy_loc_r_name)
			self.setupAimDummy()

		#create con dummy
		if not self.checkEyeCntlrDummy():
			self.createEyeDummyItem(
				self.m_eye_con_dmy_loc_l_name,
				self.m_eye_con_dmy_loc_r_name)
			self.setupConDummy()

		if not self.checkEyeAimDummy() or not self.checkEyeCntlrDummy():
			pm.warning('Failed create eye dummy object!')
			return False

		#create Aim and Controller Dummy Constraint
		self.m_orientAimConsL = pm.orientConstraint( self.m_eye_aim_dmy_loc_l, self.m_eye_con_dmy_loc_l, self.m_eye_l_jnt, n=self.m_orientAimConsL_name )
		self.m_orientAimConsR = pm.orientConstraint( self.m_eye_aim_dmy_loc_r, self.m_eye_con_dmy_loc_r, self.m_eye_r_jnt, n=self.m_orientAimConsR_name )


		aim_const_l = pm.aimConstraint( self.m_aim_loc_l, self.m_eye_aim_dmy_loc_l, skip=["z"], aimVector=(0,0,1), upVector=(0,1,0),w=1.0 )
		aim_const_r = pm.aimConstraint( self.m_aim_loc_r, self.m_eye_aim_dmy_loc_r, skip=["z"], aimVector=(0,0,1), upVector=(0,1,0),w=1.0 )


		return True
