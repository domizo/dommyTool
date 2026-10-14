import pymel.core as pm

import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.transformActions as DPTtrans
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.nodeActions as DPTnode

import DPyTools.rigging.rigCommonActions as DPTrigCAct


'''
to run:
	>>> import DPyTools.rigging.humanHandRig as DPTHHand
	>>> DPTHArm.human_hand()

'''

class human_finger(object):

	#internal
	master_ctrl=None
	hand_inner_jnt=None


	def __init__(self, handobj=None, fingerNamespace='empty', fingerList=None, hand_inner_jnt=None ):
		if fingerList is None or len(fingerList) < 4:
			raise ValueError('fingerList must contain at least four joints')
		self.groupTop = handobj.groupTop
		self.sidePrefix=handobj.sidePrefix
		self.root_ctrl = handobj.root_ctrl
		self.hand_ctrl = handobj.hand_ctrl
		self.hand_jnt = handobj.hand_jnt
		self.rigSettings_ctrl = handobj.rigSettings_ctrl

		#space attribute
		self.rootSpaceCtrl = handobj.rootSpaceCtrl
		self.worldSpaceCtrl = handobj.worldSpaceCtrl
		self.cogSpaceCtrl = handobj.cogSpaceCtrl
		self.handSpaceCtrl = handobj.handSpaceCtrl
		self.LowSplineSpaceCtrl = handobj.LowSplineSpaceCtrl
		self.HiSplineSpaceCtrl = handobj.HiSplineSpaceCtrl
		self.headSpaceCtrl = handobj.headSpaceCtrl

		self.fingur_name_space=fingerNamespace
		self.fingur_list=fingerList
		self.base_finger=pm.ls(fingerList[0])[0]
		self.mid_finger=pm.ls(fingerList[1])[0]
		self.tip_finger=pm.ls(fingerList[2])[0]
		self.last_finger=pm.ls(fingerList[3])[0]

		if hand_inner_jnt is not None:
			self.hand_inner_jnt = pm.ls(hand_inner_jnt)[0]

		#set prefer angle
		self.base_finger.setAttr('preferredAngleZ', 45)
		self.mid_finger.setAttr('preferredAngleZ', 45)


	def createMasterControl(self):
		#create master_ctrl
		self.master_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+'master_'+self.fingur_name_space+'_ctrl',size=2.0,alignObj=self.last_finger)
		master_ctrl_frzGrp, master_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.master_ctrl, self.groupTop, True, True, False )
		#master_ctrl_frzGrp.rotateZ.set(180.000)

		DPTattr.hideAttributes(self.master_ctrl, ['rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])

		DPTattr.createAttributeFromDic(self.master_ctrl,
				[{'ln':'midCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':10.0,'dv':1.0},
				 {'ln':'tipCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':10.0,'dv':1.4},
				 {'ln':'handStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
				 {'ln':'fingerStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0}])

		pm.parentConstraint( self.hand_ctrl, master_ctrl_frzGrp, maintainOffset=True )
		self.master_ctrl.rotateY.lock()
		self.master_ctrl.rotateZ.lock()
		self.master_ctrl.scale.lock()

		self.hand_ctrl.fingerMasterCtrlVis >> self.master_ctrl.visibility


	def createFKControl(self):
		#create FK Control
		FkVis_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+self.fingur_name_space+'FkVis_cond',
													param={'operation':4,'colorIfTrueR':0,'colorIfTrueG':0,'colorIfTrueB':0,
															'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':1} )
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'IK', FkVis_cond+'.firstTerm')
		self.hand_ctrl.fkFingerCtrlVis >> FkVis_cond.colorIfTrueR

		self.finFrzGrpList = []
		self.finCtrlList = []
		fingur_node_list = []

		for jnt in self.fingur_list[0:-1]:
			fingur_node_list.append(pm.ls(jnt)[0])

			fk_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+jnt+'_fk_ctrl',size=2.0,alignObj=jnt)
			fk_ctrl_frzGrp, fk_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(fk_ctrl, None, True, True, False )

			DPTattr.hideAttributes(fk_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])
			fk_ctrl.translate.lock()
			fk_ctrl.scale.lock()

			fk_ctrl_frzGrp_top = pm.duplicate(fk_ctrl_frzGrp, n=fk_ctrl_frzGrp+'_top', parentOnly=True)[0]
			fk_ctrl_frzGrp_pCons = pm.duplicate(fk_ctrl_frzGrp, n=fk_ctrl_frzGrp+'_pCons', parentOnly=True)[0]

			pm.parent(fk_ctrl_frzGrp, fk_ctrl_frzGrp_pCons)
			pm.parent(fk_ctrl_frzGrp_pCons, fk_ctrl_frzGrp_top)
			pm.parent(fk_ctrl_frzGrp_top, self.groupTop)

			if jnt == self.fingur_list[0]:
				pm.parentConstraint(self.hand_jnt, fk_ctrl_frzGrp_top, maintainOffset=True)
			elif jnt == self.fingur_list[1]:
				pm.parentConstraint(self.fingur_list[0], fk_ctrl_frzGrp_top, maintainOffset=True)
			elif jnt == self.fingur_list[2]:
				pm.parentConstraint(self.fingur_list[1], fk_ctrl_frzGrp_top, maintainOffset=True)

			pm.pointConstraint(jnt, fk_ctrl_frzGrp_pCons)

			self.finFrzGrpList.append(fk_ctrl_frzGrp)
			self.finCtrlList.append(fk_ctrl)

			FkVis_cond.outColorR >> fk_ctrl.visibility


	def createIKControl(self):
		#create IK
		fingerIKHnd, fingerIKEffector = pm.ikHandle( sj=self.base_finger, ee=self.tip_finger, sol='ikRPsolver' )
		fingerIKHnd.hide()

		pm.parent(fingerIKHnd, self.groupTop)

		#create ik_ctrl
		self.finger_ik_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+self.fingur_name_space+'_ik_ctrl',size=5.0,alignObj=self.tip_finger)
		finger_ik_ctrl_frzGrp, finger_ik_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.finger_ik_ctrl, None, True, True, False )

		finger_switch_grp = DPTgrp.createEmpty(self.sidePrefix+self.fingur_name_space+'Ctrl_space_grp',self.groupTop)
		pm.parent(finger_ik_ctrl_frzGrp, finger_switch_grp)

		DPTattr.hideAttributes(self.finger_ik_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

		DPTattr.createAttributeFromDic(self.finger_ik_ctrl,
				[{'ln':'twist','k':True,'at':'float','hnv':True,'hxv':True,'min':-360.0,'max':360.0,'dv':0.0},
				 {'ln':'________SPACES___','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
				 {'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'handCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				 {'ln':'lowSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'hiSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'headCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}])

		DPTattr.showAttrInChanelBox(self.finger_ik_ctrl, ['________SPACES___'])
		self.finger_ik_ctrl.scale.lock()

		pm.parentConstraint( self.finger_ik_ctrl, fingerIKHnd, maintainOffset=True )



		ikCtrl_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+self.fingur_name_space+'ikCtrl_cond',
													param={'operation':0,'colorIfTrueR':0,'colorIfTrueG':0,'colorIfTrueB':0,
															'colorIfFalseR':1,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':0} )
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'IK' , ikCtrl_cond+'.firstTerm')
		ikCtrl_cond.outColorR >> self.finger_ik_ctrl.visibility


		#constrain cpace
		Cons = pm.parentConstraint(self.rootSpaceCtrl, self.worldSpaceCtrl, self.cogSpaceCtrl, self.handSpaceCtrl,
							 self.LowSplineSpaceCtrl, self.HiSplineSpaceCtrl,  self.headSpaceCtrl,
							 finger_switch_grp, maintainOffset=True)

		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='world', kwargs={'ud':True})
		cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='cog', kwargs={'ud':True})
		handAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='HandCtrl', kwargs={'ud':True})
		lowAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='low', kwargs={'ud':True})
		hiAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='hi', kwargs={'ud':True})
		headAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='head', kwargs={'ud':True})

		pm.connectAttr( self.finger_ik_ctrl+'.rootCtrl', Cons+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.worldSpaceCtrl', Cons+'.'+worldCtrlSpaceAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.cogCtrl', Cons+'.'+cogCtrlSpaceAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.handCtrl', Cons+'.'+handAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.lowSpineCtrl', Cons+'.'+lowAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.hiSpineCtrl', Cons+'.'+hiAttr )
		pm.connectAttr( self.finger_ik_ctrl+'.headCtrl', Cons+'.'+headAttr )



		#create tip IK
		tipIKHnd, tipIKEffector = pm.ikHandle( sj=self.tip_finger, ee=self.last_finger, sol='ikRPsolver' )
		tipIKHnd.hide()
		pm.parent(tipIKHnd, self.groupTop)


		pm.parentConstraint( self.finger_ik_ctrl, tipIKHnd, maintainOffset=True )

		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'IK', tipIKHnd+'.ikBlend' )
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'IK', fingerIKHnd+'.ikBlend' )
		self.finger_ik_ctrl.twist >> fingerIKHnd.twist


	def createConnectControl(self):
		#create connection

		Spread_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Spread_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.masterSpread >> Spread_multDiv.input1X
		sideName = 'left'
		if self.sidePrefix == 'R':
			sideName = 'right'
		pm.connectAttr(self.rigSettings_ctrl+'.'+sideName+self.fingur_name_space+'Amt', Spread_multDiv+'.input2X' )


		#my add
		Master_Z_Mult = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Master_Z_Mult', param={'operation':1,'input2X':-1} )
		self.master_ctrl.translateZ >> Master_Z_Mult.input1X



		Curl_mega_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Curl_mega_multDiv', param={'operation':1} )

		if self.sidePrefix == 'L':
			self.master_ctrl.translateX >> Curl_mega_multDiv.input1X
		else:
			SideTransX_Rev_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'SideTransX_Rev_multDiv', param={'operation':1,'input2X':-1} )
			self.master_ctrl.translateX >> SideTransX_Rev_multDiv.input1X
			SideTransX_Rev_multDiv.outputX >> Curl_mega_multDiv.input1X

		Master_Z_Mult.outputX >> Curl_mega_multDiv.input1Y
		self.master_ctrl.rotateX >> Curl_mega_multDiv.input1Z
		self.rigSettings_ctrl.lfCurlCtrlSpeed >> Curl_mega_multDiv.input2X
		self.rigSettings_ctrl.lfSpreadCtrlSpeed >> Curl_mega_multDiv.input2Y
		self.rigSettings_ctrl.lfRollCtrlSpeed >> Curl_mega_multDiv.input2Z


		MidTipCurl_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'MidTipCurl_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.master_ctrl.translateY >> MidTipCurl_multDiv.input1X
		self.master_ctrl.translateY >> MidTipCurl_multDiv.input1Y
		self.rigSettings_ctrl.lfMidCurlCtrlSpeed >> MidTipCurl_multDiv.input2X
		self.rigSettings_ctrl.lfTipCurlCtrlSpeed >> MidTipCurl_multDiv.input2Y


		self.Roll_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'Roll_plsMns', param={'operation':1} )
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Roll', self.Roll_plsMns+'.input1D[0]' )
		Curl_mega_multDiv.outputZ >> self.Roll_plsMns.input1D[1]

		####################OUT####################
		self.Roll_plsMns.output1D >> self.finFrzGrpList[0].rotateX


		self.Spread_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'Spread_plsMns', param={'operation':1} )
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Spread', self.Spread_plsMns+'.input1D[0]')
		Spread_multDiv.outputX >> self.Spread_plsMns.input1D[1]
		Curl_mega_multDiv.outputY >> self.Spread_plsMns.input1D[2]

		####################OUT####################
		self.Spread_plsMns.output1D >> self.finFrzGrpList[0].rotateY


		Curl_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'Curl_plsMns', param={'operation':1} )

		if self.fingur_name_space == 'thumb':
			CurlDamp_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'CurlDamp_multDiv',
				param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
			self.hand_ctrl.masterFist >> CurlDamp_multDiv.input1X
			self.hand_ctrl.thumbFistDamp >> CurlDamp_multDiv.input2X

			CurlDamp_multDiv.outputX >> Curl_plsMns.input1D[0]
			self.hand_ctrl.thumbCurl >> Curl_plsMns.input1D[1]
		else:
			self.hand_ctrl.masterFist >> Curl_plsMns.input1D[0]
			pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Curl', Curl_plsMns+'.input1D[1]')



		Curl_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Curl_multDiv',
			param={'operation':1,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.midCurlAmt >> Curl_multDiv.input1Y
		self.hand_ctrl.tipCurlAmt >> Curl_multDiv.input1Z
		Curl_plsMns.output1D >> Curl_multDiv.input2X
		Curl_plsMns.output1D >> Curl_multDiv.input2Y
		Curl_plsMns.output1D >> Curl_multDiv.input2Z


		MidTipCurl_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'MidTipCurl_plsMns', param={'operation':1} )
		self.master_ctrl.midCurl >> MidTipCurl_plsMns.input2D[0].input2Dx
		self.master_ctrl.tipCurl >> MidTipCurl_plsMns.input2D[0].input2Dy
		MidTipCurl_multDiv.outputX >> MidTipCurl_plsMns.input2D[1].input2Dx
		MidTipCurl_multDiv.outputY >> MidTipCurl_plsMns.input2D[1].input2Dy



		Curl_mega_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Curl_mega_multDiv1',
			param={'operation':1,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		MidTipCurl_plsMns.output2Dx >> Curl_mega_multDiv1.input1Y
		MidTipCurl_plsMns.output2Dy >> Curl_mega_multDiv1.input1Z
		Curl_mega_multDiv.outputX >> Curl_mega_multDiv1.input2X
		Curl_mega_multDiv.outputX >> Curl_mega_multDiv1.input2Y
		Curl_mega_multDiv.outputX >> Curl_mega_multDiv1.input2Z


		self.finplsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'_plsMns', param={'operation':1} )
		Curl_multDiv.outputX >> self.finplsMns.input3D[0].input3Dx
		Curl_multDiv.outputY >> self.finplsMns.input3D[0].input3Dy
		Curl_multDiv.outputZ >> self.finplsMns.input3D[0].input3Dz
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Base', self.finplsMns+'.input3D[1].input3Dx')
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Mid', self.finplsMns+'.input3D[1].input3Dy')
		pm.connectAttr(self.hand_ctrl+'.'+self.fingur_name_space+'Tip', self.finplsMns+'.input3D[1].input3Dz')
		Curl_mega_multDiv1.outputX >> self.finplsMns.input3D[2].input3Dx
		Curl_mega_multDiv1.outputY >> self.finplsMns.input3D[2].input3Dy
		Curl_mega_multDiv1.outputZ >> self.finplsMns.input3D[2].input3Dz


		####################OUT####################
		self.finplsMns.output3Dx >> self.finFrzGrpList[0].rotateZ
		self.finplsMns.output3Dy >> self.finFrzGrpList[1].rotateZ
		self.finplsMns.output3Dz >> self.finFrzGrpList[2].rotateZ


	def createOffsetRemapNode(self, name, inmin=0.0, inmax=1.0):
		remapNode = DPTnode.createNodeWithParam(node_type='remapValue', name=name, param={'inputMin':inmin,'inputMax':inmax})
		remapNode.setAttr('value[0].value_FloatValue', 0)
		remapNode.setAttr('value[0].value_Position', 0.0)
		remapNode.setAttr('value[0].value_Interp', 1)
		remapNode.setAttr('value[1].value_FloatValue', 0)
		remapNode.setAttr('value[1].value_Position', 0.5)
		remapNode.setAttr('value[1].value_Interp', 1)
		remapNode.setAttr('value[2].value_FloatValue', 0)
		remapNode.setAttr('value[2].value_Position', 1)
		remapNode.setAttr('value[2].value_Interp', 1)

		return remapNode


	def createConnectJont(self):
		####################################Base Connection####################################

		#init
		basePosX = self.base_finger.translateX.get()
		basePosY = self.base_finger.translateY.get()
		basePosZ = self.base_finger.translateZ.get()


		##############connect base_finger##############
		DPTattr.createAttributeFromDic(self.base_finger,
				[{'ln':'rx_add_fistRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'ry_add_fistRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'rz_add_fistRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'tx_add_fistRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'ty_add_fistRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'tz_add_fistRotate','k':True,'at':'float','dv':0.0}])

		offset_remap_transx_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_transx_a', inmin=-100, inmax=100)
		offset_remap_transy_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_transy_a', inmin=-100, inmax=100)
		offset_remap_transz_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_transz_a', inmin=-100, inmax=100)
		offset_remap_rotx_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_rotx_a', inmin=-100, inmax=100)
		offset_remap_roty_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_roty_a', inmin=-100, inmax=100)
		offset_remap_rotz_a = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_rotz_a', inmin=-100, inmax=100)

		self.hand_ctrl.masterFist >> offset_remap_transx_a.inputValue
		self.hand_ctrl.masterFist >> offset_remap_transy_a.inputValue
		self.hand_ctrl.masterFist >> offset_remap_transz_a.inputValue
		self.hand_ctrl.masterFist >> offset_remap_rotx_a.inputValue
		self.hand_ctrl.masterFist >> offset_remap_roty_a.inputValue
		self.hand_ctrl.masterFist >> offset_remap_rotz_a.inputValue

		offset_remap_transx_a.outValue >> self.base_finger.tx_add_fistRotate
		offset_remap_transy_a.outValue >> self.base_finger.ty_add_fistRotate
		offset_remap_transz_a.outValue >> self.base_finger.tz_add_fistRotate
		offset_remap_rotx_a.outValue >> self.base_finger.rx_add_fistRotate
		offset_remap_roty_a.outValue >> self.base_finger.ry_add_fistRotate
		offset_remap_rotz_a.outValue >> self.base_finger.rz_add_fistRotate


		#for translateZ
		tz_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_tz_addInput_mdl_fistRotate', param={'input1':basePosZ} )
		self.base_finger.tz_add_fistRotate >> tz_addInput_mdl_fistRotate.input2

		####################OUT####################
		tz_addInput_mdl_fistRotate.output >> self.base_finger.translateZ



		#for translateY
		ty_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_ty_addInput_mdl_fistRotate', param={'input1':0.0} )
		pm.connectAttr( self.base_finger+".ty_add_fistRotate", ty_addInput_mdl_fistRotate+'.input2')

		####################OUT####################
		pm.connectAttr( ty_addInput_mdl_fistRotate+".output", self.base_finger+'.translateY')


		#for translateX
		HandStretch_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'HandStretch_plsMns', param={'operation':1,'input1D[0]':1} )
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'HandStretch', HandStretch_plsMns+'.input1D[1]')
		self.hand_ctrl.masterHandStretch >> HandStretch_plsMns.input1D[2]
		#self.hand_ctrl.handStretch >> MidTipCurl_plsMns.input1D[3]
		self.master_ctrl.handStretch >> HandStretch_plsMns.input1D[3]


		HandStretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'HandStretch_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':basePosX,'input2Y':1,'input2Z':1} )
		HandStretch_plsMns.output1D >> HandStretch_multDiv.input1X


		tx_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_tx_addInput_mdl_fistRotate', param={} )
		HandStretch_multDiv.outputX >> tx_addInput_mdl_fistRotate.input1
		pm.connectAttr( self.base_finger+".tx_add_fistRotate", tx_addInput_mdl_fistRotate+'.input2')


		####################OUT####################
		pm.connectAttr( tx_addInput_mdl_fistRotate+".output", self.base_finger+'.translateX')


		#for rotateX
		TotalRoll_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'TotalRoll_plsMns', param={'operation':1} )
		self.Roll_plsMns.output1D >> TotalRoll_plsMns.input1D[0]
		self.finCtrlList[0].rotateX >> TotalRoll_plsMns.input1D[1]


		rx_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_rx_addInput_mdl_fistRotate', param={} )
		TotalRoll_plsMns.output1D >> rx_addInput_mdl_fistRotate.input1
		pm.connectAttr( self.base_finger+".rx_add_fistRotate", rx_addInput_mdl_fistRotate+'.input2')

		####################OUT####################
		pm.connectAttr( rx_addInput_mdl_fistRotate+".output", self.base_finger+'.rotateX')


		#for rotateY
		TotalSpread_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'TotalSpread_plsMns', param={'operation':1} )
		self.Spread_plsMns.output1D >> TotalSpread_plsMns.input1D[0]
		self.finCtrlList[0].rotateY >> TotalSpread_plsMns.input1D[1]


		ry_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_ry_addInput_mdl_fistRotate', param={} )
		TotalSpread_plsMns.output1D >> ry_addInput_mdl_fistRotate.input1
		pm.connectAttr( self.base_finger+".ry_add_fistRotate", ry_addInput_mdl_fistRotate+'.input2')


		####################OUT####################
		pm.connectAttr( ry_addInput_mdl_fistRotate+".output", self.base_finger+'.rotateY')



		#for rotateZ
		TotalCurl_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'TotalCurl_plsMns', param={'operation':1} )
		self.finplsMns.output3Dx >> TotalCurl_plsMns.input3D[0].input3Dx
		self.finplsMns.output3Dy >> TotalCurl_plsMns.input3D[0].input3Dy
		self.finplsMns.output3Dz >> TotalCurl_plsMns.input3D[0].input3Dz
		self.finCtrlList[0].rotateZ >> TotalCurl_plsMns.input3D[1].input3Dx
		self.finCtrlList[1].rotateZ >> TotalCurl_plsMns.input3D[1].input3Dy
		self.finCtrlList[2].rotateZ >> TotalCurl_plsMns.input3D[1].input3Dz


		rz_addInput_mdl_fistRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.base_finger+'_rz_addInput_mdl_fistRotate', param={} )
		TotalCurl_plsMns.output3Dx >> rz_addInput_mdl_fistRotate.input1
		pm.connectAttr( self.base_finger+".rz_add_fistRotate", rz_addInput_mdl_fistRotate+'.input2')

		####################OUT####################
		pm.connectAttr( rz_addInput_mdl_fistRotate+".output", self.base_finger+'.rotateZ')




		##############connect mid_finger##############
		DPTattr.createAttributeFromDic(self.mid_finger,
				[{'ln':'rx_add_ctlRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'ry_add_ctlRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'rz_add_fistRotate','k':True,'at':'float','dv':0.0}])


		pm.connectAttr( self.finCtrlList[1]+".rotateX", self.mid_finger+'.rx_add_ctlRotate')
		pm.connectAttr( self.finCtrlList[1]+".rotateY", self.mid_finger+'.ry_add_ctlRotate')

		offset_remap_rotz_b = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_rotz_b', inmin=-100, inmax=100)
		self.hand_ctrl.masterFist >> offset_remap_rotz_b.inputValue
		offset_remap_rotz_b.outValue >> self.mid_finger.rz_add_fistRotate


		###########for translateX

		#for ik stretch blender
		EnableStretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'EnableStretch_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.stretchyIkFingers >> EnableStretch_multDiv.input1X
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'IK', EnableStretch_multDiv+'.input2X')


		MaxStretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'MaxStretch_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.maxIkFingerStretch >> MaxStretch_multDiv.input2X

		#need calc
		MaxStretch_multDiv.setAttr('input1X',self.finIKDistShape.distance.get())



		MaxStretch_clamp = DPTnode.createNodeWithParam(node_type='clamp', name=self.sidePrefix+self.fingur_name_space+'MaxStretch_clamp', param={'minR':0} )
		MaxStretch_multDiv.outputX >> MaxStretch_clamp.maxR
		self.finIKDistShape.distance >> MaxStretch_clamp.inputR


		StretchyIK_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'StretchyIK_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':0.530,'input2Y':0.470,'input2Z':1} )
		MaxStretch_clamp.outputR >> StretchyIK_multDiv.input1X
		MaxStretch_clamp.outputR >> StretchyIK_multDiv.input1Y


		Stretch_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'Stretch_plsMns', param={'operation':1,'input1D[0]':1} )
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'Stretch', Stretch_plsMns+'.input1D[1]')
		self.hand_ctrl.masterFingerStretch >> Stretch_plsMns.input1D[2]
		self.master_ctrl.fingerStretch >> Stretch_plsMns.input1D[3]



		#need calc
		baseDist = DPTtrans.getDistanceBetweenObjects(source=self.base_finger, target=self.mid_finger)
		midDist = DPTtrans.getDistanceBetweenObjects(source=self.mid_finger, target=self.tip_finger)
		tipDist = DPTtrans.getDistanceBetweenObjects(source=self.tip_finger, target=self.last_finger)

		if self.sidePrefix == 'R':
			baseDist = -baseDist
			midDist = -midDist
			tipDist = -tipDist

		Stretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'Stretch_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':baseDist,'input2Y':midDist,'input2Z':tipDist} )
		Stretch_plsMns.output1D >> Stretch_multDiv.input1X
		Stretch_plsMns.output1D >> Stretch_multDiv.input1Y
		Stretch_plsMns.output1D >> Stretch_multDiv.input1Z


		MidStretchInherit_bldTwoAtt = DPTnode.createNodeWithParam(node_type='blendTwoAttr', name=self.sidePrefix+self.fingur_name_space+'MidStretchInherit_bldTwoAtt', param={'current':0,'input[0]':midDist} )
		self.hand_ctrl.midStretchInherit >> MidStretchInherit_bldTwoAtt.attributesBlender
		Stretch_multDiv.outputY >> MidStretchInherit_bldTwoAtt.input[1]


		BaseStretchInherit_bldTwoAtt = DPTnode.createNodeWithParam(node_type='blendTwoAttr', name=self.sidePrefix+self.fingur_name_space+'BaseStretchInherit_bldTwoAtt', param={'current':0,'input[0]':baseDist} )
		self.hand_ctrl.baseStretchInherit >> BaseStretchInherit_bldTwoAtt.attributesBlender
		Stretch_multDiv.outputX >> BaseStretchInherit_bldTwoAtt.input[1]


		StretchyIK_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+self.fingur_name_space+'StretchyIK_plsMns', param={'operation':1} )
		BaseStretchInherit_bldTwoAtt.output >> StretchyIK_plsMns.input1D[0]
		MidStretchInherit_bldTwoAtt.output >> StretchyIK_plsMns.input1D[1]


		StretchyIK_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+self.fingur_name_space+'StretchyIK_cond',
			param={'operation':2,'colorIfTrueR':0,'colorIfTrueG':0,'colorIfTrueB':0,'colorIfFalseR':1,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':1} )
		MaxStretch_clamp.outputR >> StretchyIK_cond.firstTerm
		StretchyIK_plsMns.output1D >> StretchyIK_cond.secondTerm
		StretchyIK_multDiv.outputX >> StretchyIK_cond.colorIfTrueR
		StretchyIK_multDiv.outputY >> StretchyIK_cond.colorIfTrueG
		BaseStretchInherit_bldTwoAtt.output >> StretchyIK_cond.colorIfFalseR
		MidStretchInherit_bldTwoAtt.output >> StretchyIK_cond.colorIfFalseG



		StretchyIK_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+self.fingur_name_space+'StretchyIK_blend', param={} )
		EnableStretch_multDiv.outputX >> StretchyIK_blend.blender
		StretchyIK_cond.outColorR >> StretchyIK_blend.color1R
		StretchyIK_cond.outColorG >> StretchyIK_blend.color1G
		BaseStretchInherit_bldTwoAtt.output >> StretchyIK_blend.color2R
		MidStretchInherit_bldTwoAtt.output >> StretchyIK_blend.color2G

		####################OUT####################
		pm.connectAttr( StretchyIK_blend+".outputR", self.mid_finger+'.translateX')


		# for rotateX
		RollInherit_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'RollInherit_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.midRollInherit >> RollInherit_multDiv.input1X
		self.hand_ctrl.tipRollInherit >> RollInherit_multDiv.input1Y
		TotalRoll_plsMns.output1D >> RollInherit_multDiv.input2X
		TotalRoll_plsMns.output1D >> RollInherit_multDiv.input2Y


		rx_addInput_mdl_ctlRotateB = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.mid_finger+'_rx_addInput_mdl_ctlRotate', param={} )

		RollInherit_multDiv.outputX >> rx_addInput_mdl_ctlRotateB.input1
		pm.connectAttr( self.mid_finger+".rx_add_ctlRotate", rx_addInput_mdl_ctlRotateB+'.input2')

		####################OUT####################
		pm.connectAttr( rx_addInput_mdl_ctlRotateB+".output", self.mid_finger+'.rotateX')


		#for rotateY
		SpreadInherit_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+self.fingur_name_space+'SpreadInherit_multDiv',
			param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
		self.hand_ctrl.midSpreadInherit >> SpreadInherit_multDiv.input1X
		self.hand_ctrl.tipSpreadInherit >> SpreadInherit_multDiv.input1Y
		TotalSpread_plsMns.output1D >> SpreadInherit_multDiv.input2X
		TotalSpread_plsMns.output1D >> SpreadInherit_multDiv.input2Y


		ry_addInput_mdl_ctlRotateB = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.mid_finger+'_ry_addInput_mdl_ctlRotate', param={} )
		SpreadInherit_multDiv.outputX >> ry_addInput_mdl_ctlRotateB.input1
		pm.connectAttr( self.mid_finger+".ry_add_ctlRotate", ry_addInput_mdl_ctlRotateB+'.input2')

		####################OUT####################
		pm.connectAttr( ry_addInput_mdl_ctlRotateB+".output", self.mid_finger+'.rotateY')


		#for rotateZ
		rz_addInput_mdl_ctlRotateB = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.mid_finger+'_rz_addInput_mdl_ctlRotate', param={} )
		TotalCurl_plsMns.output3Dy >> rz_addInput_mdl_ctlRotateB.input1
		self.mid_finger.rz_add_fistRotate >> rz_addInput_mdl_ctlRotateB.input2


		####################OUT####################
		rz_addInput_mdl_ctlRotateB.output >> self.mid_finger.rotateZ





		##############connect tip_finger##############
		DPTattr.createAttributeFromDic(self.tip_finger,
				[{'ln':'rx_add_ctlRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'ry_add_ctlRotate','k':True,'at':'float','dv':0.0},
				 {'ln':'rz_add_fistRotate','k':True,'at':'float','dv':0.0}])


		pm.connectAttr( self.finCtrlList[2]+".rotateX", self.tip_finger+'.rx_add_ctlRotate')
		pm.connectAttr( self.finCtrlList[2]+".rotateY", self.tip_finger+'.ry_add_ctlRotate')

		#rz_add_fistRotateCrv = aCrv.createAnimCurveUU('rz_add_fistRotateCrv', (0,100), (0,40))
		offset_remap_rotz_c = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_remap_rotz_c', inmin=-100, inmax=100)

		self.hand_ctrl.masterFist >> offset_remap_rotz_c.inputValue
		#pm.connectAttr( self.hand_ctrl+".masterFist", rz_add_fistRotateCrv+'.input')
		offset_remap_rotz_c.outValue >> self.tip_finger.rz_add_fistRotate
		#pm.connectAttr( rz_add_fistRotateCrv+".output", self.tip_finger+'.rz_add_fistRotate')


		#for translateX
		####################OUT####################
		pm.connectAttr( StretchyIK_blend+".outputG", self.tip_finger+'.translateX')


		#for rotateX
		tipOriCons = pm.orientConstraint( self.finger_ik_ctrl, self.tip_finger)
		tipOriCons.constraintRotateX // self.tip_finger.rotateX
		tipOriCons.constraintRotateY // self.tip_finger.rotateY
		tipOriCons.constraintRotateZ // self.tip_finger.rotateZ


		weightAttr = DPTattr.getMatchNameAttribute(node=tipOriCons, matchStr=self.fingur_name_space, kwargs={'ud':True})
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'IK', tipOriCons+'.'+weightAttr)


		TipSplice_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+self.fingur_name_space+'TipSplice_blend', param={} )
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'IK', TipSplice_blend+'.blender')
		tipOriCons.constraintRotateX >> TipSplice_blend.color1R
		RollInherit_multDiv.outputY >> TipSplice_blend.color2R



		rx_addInput_mdl_ctlRotateC = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.tip_finger+'_rx_addInput_mdl_ctlRotate', param={} )
		TipSplice_blend.outputR >> rx_addInput_mdl_ctlRotateC.input1
		pm.connectAttr( self.tip_finger+".rx_add_ctlRotate", rx_addInput_mdl_ctlRotateC+'.input2')

		####################OUT####################
		pm.connectAttr( rx_addInput_mdl_ctlRotateC+".output", self.tip_finger+'.rotateX')



		#for rotateY
		TipSplice_blendB = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+self.fingur_name_space+'TipSplice_blendB', param={} )
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'IK', TipSplice_blendB+'.blender')
		tipOriCons.constraintRotateY >> TipSplice_blendB.color1R
		SpreadInherit_multDiv.outputY >> TipSplice_blendB.color2R


		ry_addInput_mdl_ctlRotateC = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.tip_finger+'_ry_addInput_mdl_ctlRotate', param={} )
		TipSplice_blendB.outputR >> ry_addInput_mdl_ctlRotateC.input1
		pm.connectAttr( self.tip_finger+".ry_add_ctlRotate", ry_addInput_mdl_ctlRotateC+'.input2')

		####################OUT####################
		pm.connectAttr( ry_addInput_mdl_ctlRotateC+".output", self.tip_finger+'.rotateY')



		#for rotateZ
		TipSplice_blendC = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+self.fingur_name_space+'TipSplice_blendC', param={} )
		pm.connectAttr( self.hand_ctrl+"."+self.fingur_name_space+'IK', TipSplice_blendC+'.blender')
		tipOriCons.constraintRotateZ >> TipSplice_blendC.color1R
		TotalCurl_plsMns.output3Dz >> TipSplice_blendC.color2R


		rz_addInput_mdl_fistRotateC = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.sidePrefix+self.tip_finger+'_rz_addInput_mdl_fistRotate', param={} )
		TipSplice_blendC.outputR >> rz_addInput_mdl_fistRotateC.input1
		pm.connectAttr( self.tip_finger+".rz_add_fistRotate", rz_addInput_mdl_fistRotateC+'.input2')


		####################OUT####################
		pm.connectAttr( rz_addInput_mdl_fistRotateC+".output", self.tip_finger+'.rotateZ')



		##############connect last_finger##############
		#for translateX
		TipStretchInherit_bldTwoAtt = DPTnode.createNodeWithParam(node_type='blendTwoAttr', name=self.sidePrefix+self.fingur_name_space+'TipStretchInherit_bldTwoAtt', param={'current':0,'input[0]':tipDist} )
		self.hand_ctrl.tipStretchInherit >> TipStretchInherit_bldTwoAtt.attributesBlender
		pm.connectAttr( Stretch_multDiv+".outputZ", TipStretchInherit_bldTwoAtt+'.input[1]')

		####################OUT####################
		pm.connectAttr( TipStretchInherit_bldTwoAtt+".output", self.last_finger+'.translateX')

		'''
		#rot order
		if self.sidePrefix == 'L':
			self.rigSettings_ctrl.lfFingerRotOrder >> self.base_finger.rotateOrder
			self.rigSettings_ctrl.lfFingerRotOrder >> self.mid_finger.rotateOrder
			self.rigSettings_ctrl.lfFingerRotOrder >> self.tip_finger.rotateOrder
			self.rigSettings_ctrl.lfFingerRotOrder >> self.last_finger.rotateOrder
		else:
			self.rigSettings_ctrl.rtFingerRotOrder >> self.base_finger.rotateOrder
			self.rigSettings_ctrl.rtFingerRotOrder >> self.mid_finger.rotateOrder
			self.rigSettings_ctrl.rtFingerRotOrder >> self.tip_finger.rotateOrder
			self.rigSettings_ctrl.rtFingerRotOrder >> self.last_finger.rotateOrder
		'''



	def createInnerJointConnect(self):

		##############connect hand innder joint##############
		DPTattr.createAttributeFromDic(self.hand_inner_jnt,
				[{'ln':'rx_add_cup','k':True,'at':'float','dv':0.0},
				 {'ln':'ry_add_cup','k':True,'at':'float','dv':0.0},
				 {'ln':'rz_add_cup','k':True,'at':'float','dv':0.0}])

		offset_cup_rotx = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_cup_rotx', inmin=-100, inmax=100)
		offset_cup_roty = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_cup_roty', inmin=-100, inmax=100)
		offset_cup_rotz = self.createOffsetRemapNode(name=self.sidePrefix+self.fingur_name_space+'_offset_cup_rotz', inmin=-100, inmax=100)

		self.hand_ctrl.masterCup >> offset_cup_rotx.inputValue
		self.hand_ctrl.masterCup >> offset_cup_roty.inputValue
		self.hand_ctrl.masterCup >> offset_cup_rotz.inputValue

		offset_cup_rotx.outValue >> self.hand_inner_jnt.rx_add_cup
		offset_cup_roty.outValue >> self.hand_inner_jnt.ry_add_cup
		offset_cup_rotz.outValue >> self.hand_inner_jnt.rz_add_cup


		#initi value
		if self.fingur_name_space == 'index':
			offset_cup_rotx.setAttr('value[0].value_FloatValue', 0)
			offset_cup_rotx.setAttr('value[2].value_FloatValue', 0)
			offset_cup_rotz.setAttr('value[0].value_FloatValue', 0)
			offset_cup_rotz.setAttr('value[2].value_FloatValue', 0)
		elif self.fingur_name_space == 'middle':
			offset_cup_rotx.setAttr('value[0].value_FloatValue', 0)
			offset_cup_rotx.setAttr('value[2].value_FloatValue', 0)
			offset_cup_rotz.setAttr('value[0].value_FloatValue', -3)
			offset_cup_rotz.setAttr('value[2].value_FloatValue', 6)
		elif self.fingur_name_space == 'ring':
			offset_cup_rotx.setAttr('value[0].value_FloatValue', 1)
			offset_cup_rotx.setAttr('value[2].value_FloatValue', -3)
			offset_cup_rotz.setAttr('value[0].value_FloatValue', -17)
			offset_cup_rotz.setAttr('value[2].value_FloatValue', 20)
		elif self.fingur_name_space == 'pinky':
			offset_cup_rotx.setAttr('value[0].value_FloatValue', 3)
			offset_cup_rotx.setAttr('value[2].value_FloatValue', -7)
			offset_cup_rotz.setAttr('value[0].value_FloatValue', -34)
			offset_cup_rotz.setAttr('value[2].value_FloatValue', 40)


		'''
		sideName = 'left'
		if self.sidePrefix == 'R':
			sideName = 'right'

		pm.connectAttr(self.rigSettings_ctrl+'.'+sideName+self.fingur_name_space+'CupUpAmt',  offset_cup_rotz+'.value[0].value_FloatValue')
		pm.connectAttr(self.rigSettings_ctrl+'.'+sideName+self.fingur_name_space+'CupDownAmt',  offset_cup_rotz+'.value[2].value_FloatValue')

		pm.connectAttr(self.rigSettings_ctrl+'.'+sideName+self.fingur_name_space+'TwistUpAmt',  offset_cup_rotx+'.value[0].value_FloatValue')
		pm.connectAttr(self.rigSettings_ctrl+'.'+sideName+self.fingur_name_space+'TwistDownAmt',  offset_cup_rotx+'.value[2].value_FloatValue')
		'''


		#rotateX
		rx_addInput_mdl_CupRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.hand_inner_jnt+'_rx_addInput_mdl_CupRotate', param={'input1':0.0} )
		self.hand_inner_jnt.rx_add_cup >> rx_addInput_mdl_CupRotate.input2

		####################OUT####################
		rx_addInput_mdl_CupRotate.output >> self.hand_inner_jnt.rotateX

		#rotateY
		ry_addInput_mdl_CupRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.hand_inner_jnt+'_ry_addInput_mdl_CupRotate', param={'input1':0.0} )
		self.hand_inner_jnt.ry_add_cup >> ry_addInput_mdl_CupRotate.input2

		####################OUT####################
		ry_addInput_mdl_CupRotate.output >> self.hand_inner_jnt.rotateY

		#rotateZ
		rz_addInput_mdl_CupRotate = DPTnode.createNodeWithParam(node_type='addDoubleLinear', name=self.hand_inner_jnt+'_rz_addInput_mdl_CupRotate', param={'input1':0.0} )
		self.hand_inner_jnt.rz_add_cup >> rz_addInput_mdl_CupRotate.input2

		####################OUT####################
		rz_addInput_mdl_CupRotate.output >> self.hand_inner_jnt.rotateZ






	def setup_finger_rig(self):
		self.createMasterControl()
		self.createFKControl()
		self.createIKControl()

		#create Dist set
		finIKDist, self.finIKDistShape, finIKDist_dist_sp, finIKDist_dist_ep = DPTrigCAct.createRigDistanceSet(start=self.base_finger,
			end=self.finger_ik_ctrl,
			name=self.sidePrefix+self.fingur_name_space+'IkStretch_dist',
			parent=self.groupTop)

		self.createConnectControl()
		self.createConnectJont()

		if self.hand_inner_jnt is not None:
			self.createInnerJointConnect()






class human_hand(object):

	#external
	rigTop = None
	sidePrefix='L'
	root_ctrl = None
	hand_ctrl = None
	hand_jnt = None
	rigSettings_ctrl = None

	#internal
	groupTop = None


	#space attribute
	rootSpaceCtrl = None
	worldSpaceCtrl = None
	cogSpaceCtrl = None
	handSpaceCtrl = None#internal
	LowSplineSpaceCtrl = None
	HiSplineSpaceCtrl = None
	headSpaceCtrl = None






	def setup_hand_rig(self):

		#Create Hand space grp
		self.handSpaceCtrl = DPTgrp.createEmpty(self.sidePrefix+'HandCtrl_space_switch_grp',self.rigTop, pm.xform(self.hand_jnt, q=True, ws=True, t=True ))
		DPTattr.connectAttrToList(self.root_ctrl+'.masterScale',[self.handSpaceCtrl+'.scaleX',self.handSpaceCtrl+'.scaleY',self.handSpaceCtrl+'.scaleZ'])
		pm.parentConstraint( self.hand_ctrl, self.handSpaceCtrl, maintainOffset=True )


		#create rig top
		self.groupTop = DPTgrp.createEmpty(self.sidePrefix+'hand_rig_grp',self.rigTop)
		DPTattr.connectAttrToList(self.root_ctrl+'.masterScale',[self.groupTop+'.scaleX',self.groupTop+'.scaleY',self.groupTop+'.scaleZ',])


		#create finger top
		fin_name = 'thumb'
		fin_list = [fin_name+'_a_'+self.sidePrefix+'_jnt',fin_name+'_b_'+self.sidePrefix+'_jnt',fin_name+'_c_'+self.sidePrefix+'_jnt',fin_name+'_d_'+self.sidePrefix+'_jnt']

		thumbFinObj = human_finger(self, fin_name, fin_list)
		thumbFinObj.setup_finger_rig()


		fin_name = 'index'
		fin_list = [fin_name+'_a_'+self.sidePrefix+'_jnt',fin_name+'_b_'+self.sidePrefix+'_jnt',fin_name+'_c_'+self.sidePrefix+'_jnt',fin_name+'_d_'+self.sidePrefix+'_jnt']
		handIn_fingerName = self.sidePrefix+fin_name+'_mid_jnt'

		indexFinObj = human_finger(self, fin_name, fin_list, handIn_fingerName)
		indexFinObj.setup_finger_rig()

		fin_name = 'middle'
		fin_list = [fin_name+'_a_'+self.sidePrefix+'_jnt',fin_name+'_b_'+self.sidePrefix+'_jnt',fin_name+'_c_'+self.sidePrefix+'_jnt',fin_name+'_d_'+self.sidePrefix+'_jnt']
		handIn_fingerName = self.sidePrefix+fin_name+'_mid_jnt'

		middleFinObj = human_finger(self, fin_name, fin_list, handIn_fingerName)
		middleFinObj.setup_finger_rig()

		fin_name = 'ring'
		fin_list = [fin_name+'_a_'+self.sidePrefix+'_jnt',fin_name+'_b_'+self.sidePrefix+'_jnt',fin_name+'_c_'+self.sidePrefix+'_jnt',fin_name+'_d_'+self.sidePrefix+'_jnt']
		handIn_fingerName = self.sidePrefix+fin_name+'_mid_jnt'

		ringFinObj = human_finger(self, fin_name, fin_list, handIn_fingerName)
		ringFinObj.setup_finger_rig()

		fin_name = 'pinky'
		fin_list = [fin_name+'_a_'+self.sidePrefix+'_jnt',fin_name+'_b_'+self.sidePrefix+'_jnt',fin_name+'_c_'+self.sidePrefix+'_jnt',fin_name+'_d_'+self.sidePrefix+'_jnt']
		handIn_fingerName = self.sidePrefix+fin_name+'_mid_jnt'

		pinkyFinObj = human_finger(self, fin_name, fin_list, handIn_fingerName)
		pinkyFinObj.setup_finger_rig()


















