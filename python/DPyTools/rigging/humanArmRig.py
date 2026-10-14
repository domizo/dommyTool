import pymel.core as pm

import DPyTools.curve.curveTool as DPTCurveTool

import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.stringActions as DPTStr
import DPyTools.general.transformActions as DPTtrans
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.nodeActions as DPTnode

import DPyTools.rigging.jointActions as DPTJoint
import DPyTools.rigging.rigCommonActions as DPTrigCAct
import DPyTools.rigging.humanHandRig as DPTHHand

'''
to run:
	>>> import DPyTools.rigging.humanArmRig as DPTHArm
	>>> DPTHArm.humanArm()

'''



########################################################################Create arm Rig########################################################################

class human_arm(object):

		rigTop = None
		groupTop = None

		root_ctrl = None
		rootSpaceCtrl = None
		worldSpaceCtrl = None
		cog_ctrl = None
		cogSpaceCtrl = None
		master_spline = None
		head_ctrl = None
		headSpaceCtrl = None
		LowSplineSpaceCtrl = None
		HiSplineSpaceCtrl = None
		armSpaceCtrl = None

		sidePrefix='L'
		spine_end_jnt = None
		rigSettings_ctrl = None


		handRig = None


		def __init__(self, clavicle='clavicle_jnt', upArm='up_arm_jnt', elbow='elbow_jnt',wrist='wrist_jnt',hand='hand_jnt'):
			self.clavicle = clavicle
			self.upArm = upArm
			self.elbow = elbow
			self.wrist = wrist
			self.hand = hand



		def createFKJoint(self):
			self.FKGrpTop = DPTgrp.createEmpty(self.sidePrefix+'armFK_Grp', self.groupTop, position=pm.xform(self.clavicle, q=True, ws=True, t=True ))

			self.FKJoints = DPTJoint.duplicateSameJointSet(self.upArm, self.FKGrpTop, {'jnt':'fk_jnt'})

			self.FKUpArmJnt = DPTnode.getObjectFromList('up_arm',self.FKJoints)
			self.FKElbowJnt = DPTnode.getObjectFromList('elbow',self.FKJoints)
			self.FKWristJnt = DPTnode.getObjectFromList('wrist',self.FKJoints)

			wristchilds = DPTJoint.getJointChilds(self.FKWristJnt)
			pm.delete(wristchilds)

			self.createFKControls()


		def createFKControls(self):
			########################create upjoints########################
			wrist_upJntFkCtrlAlign_grp = DPTgrp.createEmptyWithAlign(self.sidePrefix+'wrist_upJntFkCtrlAlign_grp', self.wrist, True, True, False)
			elbow_upJntFkCtrl_grp = DPTgrp.createEmptyWithAlign(self.sidePrefix+'elbow_upJntFkCtrl_grp', self.elbow, True, True, False)
			upArm_upJntFkCtrlAlign_grp = DPTgrp.createEmptyWithAlign(self.sidePrefix+'upArm_upJntFkCtrlAlign_grp', self.upArm, True, True, False)

			pm.parent(wrist_upJntFkCtrlAlign_grp, self.groupTop)
			pm.parent(elbow_upJntFkCtrl_grp, self.groupTop)
			pm.parent(upArm_upJntFkCtrlAlign_grp, self.groupTop)

			#connect and constraint
			pm.parentConstraint( self.clavicle, upArm_upJntFkCtrlAlign_grp, maintainOffset=True )
			pm.parentConstraint( self.FKUpArmJnt, elbow_upJntFkCtrl_grp, maintainOffset=True )
			pm.parentConstraint( self.FKElbowJnt, wrist_upJntFkCtrlAlign_grp, maintainOffset=True )

			########################create control########################
			############upArm_fk_ctrl############
			upArm_fk_ctrl, upArm_fk_ctrl_frzGrp_TOP, upArm_fk_ctrl_frzGrp, upArm_fk_ctrl_zeroFrzGrp, upArm_sec_fk_ctrl,upArm_sec_fk_ctrl_zeroFrzGrp = DPTrigCAct.createFKControl(grpTop=self.groupTop,
																					partJnt=self.upArm,
																					partName='upArm',
																					sidePrefix=self.sidePrefix,
																					isSecondary=True)


			DPTattr.createAttributeFromDic(upArm_fk_ctrl,
				[{'ln':'align','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				{'ln':'localAlign','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				{'ln':'secondaryFkCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False}
				])


			upArm_fk_ctrl.secondaryFkCtrlVis >> upArm_sec_fk_ctrl.visibility



			############alignTrue and alignFalse############
			upArm_alignTrue_grp = pm.duplicate(upArm_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'upArm_alignTrue_grp', parentOnly=True)[0]
			upArm_alignFalse_grp = pm.duplicate(upArm_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'upArm_alignFalse_grp', parentOnly=True)[0]
			upArm_alignFalse_grp_frzGrp = pm.duplicate(upArm_alignFalse_grp, n=self.sidePrefix+'upArm_alignFalse_grp_frzGrp', parentOnly=True)[0]
			pm.parent(upArm_alignTrue_grp, upArm_fk_ctrl_frzGrp_TOP[0])
			pm.parent(upArm_alignFalse_grp_frzGrp, upArm_fk_ctrl_frzGrp_TOP[0])
			pm.parent(upArm_alignFalse_grp, upArm_alignFalse_grp_frzGrp)


			#upArm_fk_ctrl connect and constraint
			pm.pointConstraint(self.FKUpArmJnt, upArm_fk_ctrl_frzGrp_TOP)

			orientCons = pm.orientConstraint( upArm_alignTrue_grp, upArm_alignFalse_grp, upArm_fk_ctrl_frzGrp )
			pm.parentConstraint( upArm_upJntFkCtrlAlign_grp, upArm_alignFalse_grp_frzGrp, maintainOffset=True )

			alignTrueAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrue', kwargs={'ud':True})
			alignFalseAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignFalse', kwargs={'ud':True})

			pm.connectAttr( upArm_fk_ctrl+'.align', orientCons+'.'+alignTrueAttr )
			upArm_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'upArm_plsMns', param={'operation':2,'input1D[0]':1} )
			upArm_fk_ctrl.align >> upArm_plsMns.input1D[1]
			pm.connectAttr( upArm_plsMns+'.output1D', orientCons+'.'+alignFalseAttr )


			############upArm alignTrueLocTarg############
			upArm_alignTrueLocTarg_grp = pm.duplicate(upArm_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'upArm_alignTrueLocTarg_grp', parentOnly=True)[0]
			upArm_alignTrueLocTarg_grp_frzGrp = pm.duplicate(upArm_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'upArm_alignTrueLocTarg_grp_frzGrp', parentOnly=True)[0]
			upArm_alignTrueLocTarg_grp_frzGrp_TOP = DPTgrp.createEmpty(self.sidePrefix+'upArm_alignTrueLocTarg_grp_frzGrp_top',upArm_fk_ctrl_frzGrp_TOP, position=pm.xform(self.spine_end_jnt, q=True, ws=True, t=True ))
			pm.parent(upArm_alignTrueLocTarg_grp_frzGrp,upArm_alignTrueLocTarg_grp_frzGrp_TOP)
			pm.parent(upArm_alignTrueLocTarg_grp,upArm_alignTrueLocTarg_grp_frzGrp)

			#connect and constraint
			pm.parentConstraint( self.spine_end_jnt, upArm_alignTrueLocTarg_grp_frzGrp_TOP, maintainOffset=True )
			pm.pointConstraint( self.FKUpArmJnt, upArm_alignTrueLocTarg_grp_frzGrp )


			############upArm autoAlign############
			upArm_autoAlign_rig_grp = DPTgrp.createEmpty(self.sidePrefix+'upArm_autoAlign_rig_grp',parent=None, position=pm.xform(self.upArm, q=True, ws=True, t=True ))
			upArm_autoAlign_rig_grp_frzGrp = pm.duplicate(upArm_autoAlign_rig_grp, n=self.sidePrefix+'upArm_autoAlign_rig_grp_frzGrp', parentOnly=True)[0]
			upArm_alignTrueWrldTarg_grp = pm.duplicate(upArm_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'upArm_alignTrueWrldTarg_grp', parentOnly=True)[0]

			pm.parent(upArm_autoAlign_rig_grp, upArm_autoAlign_rig_grp_frzGrp)
			pm.parent(upArm_alignTrueWrldTarg_grp, upArm_autoAlign_rig_grp)
			pm.parent(upArm_autoAlign_rig_grp_frzGrp, upArm_fk_ctrl_frzGrp_TOP[0])

			#connect and constraint
			self.root_ctrl.rotate >> upArm_autoAlign_rig_grp.rotate

			orientCons = pm.orientConstraint( upArm_alignTrueLocTarg_grp, upArm_alignTrueWrldTarg_grp, upArm_alignTrue_grp )

			alignTrueLocAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueLoc', kwargs={'ud':True})
			alignTrueWrldAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueWrld', kwargs={'ud':True})

			upArmAlignTrue_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'upArmAlignTrue_plsMns', param={'operation':2,'input1D[0]':1} )

			pm.connectAttr( upArm_fk_ctrl+'.localAlign', orientCons+'.'+alignTrueLocAttr )
			upArm_fk_ctrl.localAlign >> upArmAlignTrue_plsMns.input1D[1]
			pm.connectAttr( upArmAlignTrue_plsMns+'.output1D', orientCons+'.'+alignTrueWrldAttr )


			############elbow_fk_ctrl############
			elbow_fk_ctrl, elbow_fk_ctrl_frzGrp_TOP, elbow_fk_ctrl_frzGrp, elbow_fk_ctrl_zeroFrzGrp = DPTrigCAct.createFKControl(grpTop=self.groupTop,
																						partJnt=self.elbow,
																						partName='elbow' ,sidePrefix=self.sidePrefix,
																						isSecondary=False)

			pm.pointConstraint( self.FKElbowJnt, elbow_fk_ctrl_frzGrp_TOP )
			pm.orientConstraint( elbow_upJntFkCtrl_grp, elbow_fk_ctrl_frzGrp )
			DPTattr.hideAttributes(elbow_fk_ctrl, ['rotateX'])
			elbow_fk_ctrl.rotateX.lock()


			############wrist_fk_ctrl############
			wrist_fk_ctrl, wrist_fk_ctrl_frzGrp_TOP, wrist_fk_ctrl_frzGrp, wrist_fk_ctrl_zeroFrzGrp, wrist_sec_fk_ctrl,wrist_sec_fk_ctrl_zeroFrzGrp = DPTrigCAct.createFKControl(grpTop=self.groupTop,
																					partJnt=self.wrist,
																					partName='wrist',
																					sidePrefix=self.sidePrefix,
																					isSecondary=True)


			DPTattr.createAttributeFromDic(wrist_fk_ctrl,
				[{'ln':'align','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				{'ln':'localAlign','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				{'ln':'secondaryFkCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False}
				])

			wrist_fk_ctrl.secondaryFkCtrlVis >> wrist_sec_fk_ctrl.visibility

			wristOrientTarget_loc = pm.spaceLocator(n=self.sidePrefix+'wristOrientTarget_loc')
			DPTtrans.setTransformToObjectZero( srcObj=wristOrientTarget_loc, targetObj=self.wrist, isTrans=True, isRot=True, isScale=True, isUnParent=True)

			pm.parent(wristOrientTarget_loc, wrist_sec_fk_ctrl)
			wristOrientTarget_loc.setAttr('visibility',0)


			############wrist alignTrue and alignFalse############
			wrist_alignTrue_grp = pm.duplicate(wrist_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'wrist_alignTrue_grp', parentOnly=True)[0]
			wrist_alignFalse_grp = pm.duplicate(wrist_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'wrist_alignFalse_grp', parentOnly=True)[0]
			wrist_alignFalse_grp_frzGrp = pm.duplicate(wrist_alignFalse_grp, n=self.sidePrefix+'wrist_alignFalse_grp_frzGrp', parentOnly=True)[0]
			pm.parent(wrist_alignTrue_grp, wrist_fk_ctrl_frzGrp_TOP[0])
			pm.parent(wrist_alignFalse_grp_frzGrp, wrist_fk_ctrl_frzGrp_TOP[0])
			pm.parent(wrist_alignFalse_grp, wrist_alignFalse_grp_frzGrp)

			pm.parentConstraint(wrist_upJntFkCtrlAlign_grp, wrist_alignFalse_grp_frzGrp)

			#upArm_fk_ctrl connect and constraint
			pm.pointConstraint(self.FKWristJnt, wrist_fk_ctrl_frzGrp_TOP)

			orientCons = pm.orientConstraint( wrist_alignTrue_grp, wrist_alignFalse_grp, wrist_fk_ctrl_frzGrp )
			pm.parentConstraint( upArm_upJntFkCtrlAlign_grp, upArm_alignFalse_grp_frzGrp, maintainOffset=True )

			alignTrueAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrue', kwargs={'ud':True})
			alignFalseAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignFalse', kwargs={'ud':True})

			pm.connectAttr( wrist_fk_ctrl+'.align', orientCons+'.'+alignTrueAttr )
			wrist_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'wrist_plsMns', param={'operation':2,'input1D[0]':1} )
			wrist_fk_ctrl.align >> wrist_plsMns.input1D[1]
			pm.connectAttr( wrist_plsMns+'.output1D', orientCons+'.'+alignFalseAttr )


			############wrist alignTrueLocTarg############
			wrist_alignTrueLocTarg_grp = pm.duplicate(wrist_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'wrist_alignTrueLocTarg_grp', parentOnly=True)[0]
			wrist_alignTrueLocTarg_grp_frzGrp = pm.duplicate(wrist_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'wrist_alignTrueLocTarg_grp_frzGrp', parentOnly=True)[0]
			wrist_alignTrueLocTarg_grp_frzGrp_TOP = DPTgrp.createEmpty(self.sidePrefix+'wrist_alignTrueLocTarg_grp_frzGrp_top',wrist_fk_ctrl_frzGrp_TOP, position=pm.xform(self.spine_end_jnt, q=True, ws=True, t=True ))
			pm.parent(wrist_alignTrueLocTarg_grp_frzGrp,wrist_alignTrueLocTarg_grp_frzGrp_TOP)
			pm.parent(wrist_alignTrueLocTarg_grp,wrist_alignTrueLocTarg_grp_frzGrp)

			#connect and constraint
			pm.parentConstraint( self.spine_end_jnt, wrist_alignTrueLocTarg_grp_frzGrp_TOP, maintainOffset=True )
			pm.pointConstraint( self.FKWristJnt, wrist_alignTrueLocTarg_grp_frzGrp )


			############upArm autoAlign############
			wrist_autoAlign_rig_grp = DPTgrp.createEmpty(self.sidePrefix+'wrist_autoAlign_rig_grp',parent=None, position=pm.xform(self.wrist, q=True, ws=True, t=True ))
			wrist_autoAlign_rig_grp_frzGrp = pm.duplicate(wrist_autoAlign_rig_grp, n=self.sidePrefix+'wrist_autoAlign_rig_grp_frzGrp', parentOnly=True)[0]
			wrist_alignTrueWrldTarg_grp = pm.duplicate(wrist_fk_ctrl_zeroFrzGrp, n=self.sidePrefix+'wrist_alignTrueWrldTarg_grp', parentOnly=True)[0]

			pm.parent(wrist_autoAlign_rig_grp, wrist_autoAlign_rig_grp_frzGrp)
			pm.parent(wrist_alignTrueWrldTarg_grp, wrist_autoAlign_rig_grp)
			pm.parent(wrist_autoAlign_rig_grp_frzGrp, wrist_fk_ctrl_frzGrp_TOP[0])

			#connect and constraint
			self.root_ctrl.rotate >> wrist_autoAlign_rig_grp.rotate

			orientCons = pm.orientConstraint( wrist_alignTrueLocTarg_grp, wrist_alignTrueWrldTarg_grp, wrist_alignTrue_grp )

			alignTrueLocAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueLoc', kwargs={'ud':True})
			alignTrueWrldAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueWrld', kwargs={'ud':True})

			wristAlignTrue_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'wristAlignTrue_plsMns', param={'operation':2,'input1D[0]':1} )

			pm.connectAttr( wrist_fk_ctrl+'.localAlign', orientCons+'.'+alignTrueLocAttr )
			wrist_fk_ctrl.localAlign >> wristAlignTrue_plsMns.input1D[1]
			pm.connectAttr( wristAlignTrue_plsMns+'.output1D', orientCons+'.'+alignTrueWrldAttr )



			################FKJoint connect and constraint################
			pm.parentConstraint( self.clavicle, self.FKGrpTop, maintainOffset=True )

			pm.pointConstraint( self.shoulder_ctrl, self.FKUpArmJnt )
			pm.orientConstraint( upArm_sec_fk_ctrl, self.FKUpArmJnt  )

			pm.orientConstraint( elbow_fk_ctrl, self.FKElbowJnt )
			pm.orientConstraint( wristOrientTarget_loc, self.FKWristJnt )


			#Vis
			self.hand_ctrl.fkVis >> self.FKUpArmJnt.visibility

			armFkVis_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+'armFkVis_cond', param={'operation':4,'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																												'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':1} )
			self.hand_ctrl.ikFkBlend >> armFkVis_cond.firstTerm
			armFkVis_cond.outColorR >> upArm_fk_ctrl.visibility
			armFkVis_cond.outColorR >> elbow_fk_ctrl.visibility
			armFkVis_cond.outColorR >> wrist_fk_ctrl.visibility




		def createIKJoint(self):
			self.IKGrpTop = DPTgrp.createEmpty(self.sidePrefix+'armIK_grp',self.groupTop)

			self.IKJoints = DPTJoint.duplicateSameJointSet(self.upArm, self.IKGrpTop, {'jnt':'ik_jnt'})

			self.IKUpArmJnt = DPTnode.getObjectFromList('up_arm',self.IKJoints)
			self.IKElbowJnt = DPTnode.getObjectFromList('elbow',self.IKJoints)
			self.IKWristJnt = DPTnode.getObjectFromList('wrist',self.IKJoints)

			wristchilds = DPTJoint.getJointChilds(self.IKWristJnt)
			pm.delete(wristchilds)

			#connect and constraint
			pm.parentConstraint( self.clavicle, self.IKGrpTop, maintainOffset=True )



			############################Create IK Handle############################
			self.armIKHnd, armIKEffector = pm.ikHandle(sj=self.IKUpArmJnt, ee=self.IKWristJnt, sol='ikRPsolver')
			pm.parent(self.armIKHnd, self.groupTop)
			self.armIKHnd.hide()

			self.createArmIKControl()

			pm.parentConstraint(self.arm_ik_ctrl, self.armIKHnd)

			self.create_pv_control()



		def createHandControl(self):

			self.hand_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+'hand_ctrl', size=3.0, alignObj=self.hand)
			hand_ctrl_frzGrp, hand_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.hand_ctrl, self.groupTop, True, True, False )

			DPTattr.hideAttributes(self.hand_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])

			DPTattr.createAttributeFromDic(self.hand_ctrl,
					[{'ln':'ikFkBlend','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'ikVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'fkVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
					{'ln':'shoulderCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'armStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
					{'ln':'elbowStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
					{'ln':'wristStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
					{'ln':'upLimbAutoTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'upLimbTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-360.0,'max':360.0,'dv':0.0},
					{'ln':'upLimbTwistScalar','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'midJntUpLimbTwistScalar','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'useAimTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'forearmTwistCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'rubberHoseCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
					{'ln':'elbowSoftness','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'autoSquashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'squashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':1.0},
					{'ln':'FINGER_SETTINGS___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':False},
					{'ln':'fkFingerCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'fingerMasterCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
					{'ln':'thumbFistDamp','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.3},
					{'ln':'midCurlAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':1},
					{'ln':'tipCurlAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.75},
					{'ln':'midSpreadInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'tipSpreadInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'midRollInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'tipRollInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'baseStretchInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'midStretchInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'tipStretchInherit','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
					{'ln':'________FINGER_IK___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'stretchyIkFingers','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'maxIkFingerStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':1.0,'max':10.0,'dv':2.0},
					{'ln':'thumbIK','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'indexIK','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'middleIK','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'ringIK','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'pinkyIK','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
					{'ln':'___FINGER_STRETCH_______','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'masterFingerStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'thumbStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'indexStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'middleStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'ringStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'pinkyStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'masterHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'thumbHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'indexHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'middleHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'ringHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'pinkyHandStretch','h':False,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':5.0,'dv':0.0},
					{'ln':'_MASTER_FINGER_ATTS___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'masterFist','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'masterSpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'masterCup','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'_FINGER_CURL___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'thumbCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'indexCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'middleCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'ringCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'pinkyCurl','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'________THUMB___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'thumbBase','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'thumbMid','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'thumbTip','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'thumbSpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-50,'max':50,'dv':0.0},
					{'ln':'thumbRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'________INDEX___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'indexBase','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'indexMid','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'indexTip','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'indexSpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-50,'max':50,'dv':0.0},
					{'ln':'indexRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'________MIDDLE___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'middleBase','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'middleMid','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'middleTip','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'middleSpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-50,'max':50,'dv':0.0},
					{'ln':'middleRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'________RING___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'ringBase','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'ringMid','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'ringTip','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'ringSpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-50,'max':50,'dv':0.0},
					{'ln':'ringRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'________PINKY___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
					{'ln':'pinkyBase','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'pinkyMid','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'pinkyTip','k':True,'at':'float','hnv':True,'hxv':True,'min':-90,'max':150,'dv':0.0},
					{'ln':'pinkySpread','k':True,'at':'float','hnv':True,'hxv':True,'min':-50,'max':50,'dv':0.0},
					{'ln':'pinkyRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-100,'max':100,'dv':0.0},
					{'ln':'baseCurlAmt','k':True,'at':'float','dv':0.8}
					])



			DPTattr.showAttrInChanelBox(self.hand_ctrl, ['FINGER_SETTINGS___','_MASTER_FINGER_ATTS___','_FINGER_CURL___','________FINGER_IK___',
						'________THUMB___','________INDEX___','________MIDDLE___','________RING___','________PINKY___'])

			pm.parentConstraint(self.wrist, hand_ctrl_frzGrp)
			self.hand_ctrl.translate.lock()
			self.hand_ctrl.scale.lock()


			#create IkVis
			self.armIkVis_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+'armIkVis_cond', param={'operation':2,'secondTerm':0,
																															 'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																															 'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1} )

			self.hand_ctrl.ikFkBlend >> self.armIkVis_cond.firstTerm


		def createClavicleCtrl(self):
			#clavicle_ctrl_frzGrp_top = DPTgrp.createEmpty(self.sidePrefix+'clavicle_ctrl_frzGrp_top',self.groupTop)
			clavicle_ctrl_frzGrp_top = DPTgrp.createEmptyWithAlign(self.sidePrefix+'clavicle_ctrl_frzGrp_top', self.clavicle, True, True, True, True)
			pm.parent(clavicle_ctrl_frzGrp_top, self.groupTop)

			#clavicle_ctrl
			self.clavicle_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+'clavicle_ctrl', size=10.0, alignObj=self.clavicle)
			#self.clavicle_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'clavicle_ctrl', size=10.0,position=pm.xform(self.clavicle,q=True,ws=True,t=True))
			clavicle_ctrl_frzGrp, clavicle_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.clavicle_ctrl, clavicle_ctrl_frzGrp_top, True, True, False )

			self.clavicle_ctrl.scale.lock()
			DPTattr.hideAttributes(self.clavicle_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

			DPTattr.createAttributeFromDic(self.clavicle_ctrl,
				[{'ln':'clavicleTransCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False}])

			#clavicleTrans_ctrl
			self.clavicleTrans_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'clavicleTrans_ctrl', size=8.0,position=pm.xform(self.clavicle_ctrl,q=True,ws=True,t=True))
			clavicleTrans_ctrl_frzGrp, clavicleTrans_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.clavicleTrans_ctrl, clavicle_ctrl_frzGrp_top, True, True, False )

			self.clavicleTrans_ctrl.rotate.lock()
			self.clavicleTrans_ctrl.scale.lock()
			DPTattr.hideAttributes(self.clavicleTrans_ctrl, ['rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])

			#connect and constraint
			pm.parentConstraint( self.spine_end_jnt, clavicle_ctrl_frzGrp_top, maintainOffset=True )

			pm.pointConstraint( self.clavicleTrans_ctrl, clavicle_ctrl_frzGrp )
			self.clavicle_ctrl.clavicleTransCtrlVis >> self.clavicleTrans_ctrl.visibility


		def createShoulderCtrl(self):
			self.shoulder_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'shoulder_ctrl', size=10.0, position=pm.xform(self.upArm,q=True,ws=True,t=True))
			#shoulder_ctrl_frzGrp, shoulder_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(self.shoulder_ctrl, pm.xform(self.upArm,q=True,ws=True,t=True), parent=self.groupTop, isAlign=True)
			shoulder_ctrl_frzGrp, shoulder_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.shoulder_ctrl, self.groupTop, True, False, False )
			DPTattr.hideAttributes(self.shoulder_ctrl, ['rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])
			self.shoulder_ctrl.rotate.lock()
			self.shoulder_ctrl.scale.lock()

			self.hand_ctrl.shoulderCtrlVis >> self.shoulder_ctrl.visibility

			pm.parentConstraint(self.clavicle, shoulder_ctrl_frzGrp, maintainOffset=True)



		def createArmIKControl(self):

			armIkCtrl_space_grp = DPTgrp.createEmpty(name=self.sidePrefix+'armIkCtrl_space_grp', parent=self.groupTop)

			self.arm_ik_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=self.sidePrefix+'arm_ik_ctrl', size=10.0, alignObj=self.wrist)
			arm_ik_ctrl_frzGrp, arm_ik_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.arm_ik_ctrl, armIkCtrl_space_grp, True, True, False)

			ikCtrlCrvOrientOffset_loc = pm.spaceLocator(n=self.sidePrefix+'ikCtrlCrvOrientOffset_loc')
			DPTtrans.setTransformToObjectZero(srcObj=ikCtrlCrvOrientOffset_loc, targetObj=self.wrist, isTrans=True, isRot=False, isScale=False, isUnParent=True)
			ikCtrlCrvOrientOffset_loc.hide()

			ikCtrlCrvOrientOffset_loc_frzGrp = DPTgrp.createEmptyWithAlign(self.sidePrefix+'ikCtrlCrvOrientOffset_loc_frzGrp', ikCtrlCrvOrientOffset_loc, True, True, False)

			pm.parent(ikCtrlCrvOrientOffset_loc, ikCtrlCrvOrientOffset_loc_frzGrp)
			pm.parent(ikCtrlCrvOrientOffset_loc_frzGrp, self.arm_ik_ctrl)


			arm_ik_ctrl_frzGrp.rotateX.set(0)
			arm_ik_ctrl_frzGrp.rotateY.set(0)
			arm_ik_ctrl_frzGrp.rotateZ.set(0)

			if self.sidePrefix == 'R':
				ikCtrlCrvOrientOffset_loc_frzGrp.rotateX.set(180.0)

			self.arm_ik_ctrl.scale.lock()
			DPTattr.hideAttributes(self.arm_ik_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

			DPTattr.createAttributeFromDic(self.arm_ik_ctrl,
				[{'ln':'armTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
				 {'ln':'pvControl','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
				 {'ln':'stretchyArm','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				 {'ln':'maxStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':1.0,'max':10.0,'dv':3.0},
				 {'ln':'stretchValue','h':False,'at':'float','dv':0.0},
				 {'ln':'________SPACES___','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
				 {'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				 {'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'lowSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'hiSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'masterSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'headCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}])

			DPTattr.showAttrInChanelBox(self.arm_ik_ctrl, ['________SPACES___'])


			#connect and constraint
			self.armIkVis_cond.outColorR >> self.arm_ik_ctrl.visibility

			Cons = pm.parentConstraint(self.rootSpaceCtrl, self.worldSpaceCtrl, self.cogSpaceCtrl, self.LowSplineSpaceCtrl, self.HiSplineSpaceCtrl, self.master_spline, self.headSpaceCtrl, armIkCtrl_space_grp, maintainOffset=True)

			rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='root', kwargs={'ud':True})
			worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='world', kwargs={'ud':True})
			cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='cog', kwargs={'ud':True})
			lowAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='low', kwargs={'ud':True})
			hiAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='hi', kwargs={'ud':True})
			masterAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='master', kwargs={'ud':True})
			headAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='head', kwargs={'ud':True})

			pm.connectAttr( self.arm_ik_ctrl+'.rootCtrl', Cons+'.'+rootCtrlSpaceAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.worldSpaceCtrl', Cons+'.'+worldCtrlSpaceAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.cogCtrl', Cons+'.'+cogCtrlSpaceAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.lowSpineCtrl', Cons+'.'+lowAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.hiSpineCtrl', Cons+'.'+hiAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.masterSpineCtrl', Cons+'.'+masterAttr )
			pm.connectAttr( self.arm_ik_ctrl+'.headCtrl', Cons+'.'+headAttr )

			pm.pointConstraint(self.shoulder_ctrl, self.IKUpArmJnt)
			pm.orientConstraint(ikCtrlCrvOrientOffset_loc, self.IKWristJnt)

			self.hand_ctrl.ikVis >> self.IKUpArmJnt.visibility

			#space constarain
			pm.parentConstraint( self.arm_ik_ctrl, self.armSpaceCtrl, maintainOffset=True )



		def create_pv_control(self):
			#create elbow_pv_ctrl
			armPvCtrlGrp_space_grp = DPTgrp.createEmpty( self.sidePrefix+'armPvCtrlGrp_space_grp', self.groupTop )

			self.elbow_pv_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'elbow_pv_ctrl', size=10)
			pm.parent(self.elbow_pv_ctrl, self.elbow)
			self.elbow_pv_ctrl.translate.set([0,0,0])
			pm.move( 0, 0, -30, self.elbow_pv_ctrl, r=True, os=True, wd=True )
			elbow_pv_ctrl_frzGrp, elbow_pv_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=self.elbow_pv_ctrl, parent=armPvCtrlGrp_space_grp, position=pm.xform(self.elbow_pv_ctrl, q=True, ws=True, t=True ))
			self.elbow_pv_ctrl.scale.lock()
			DPTattr.hideAttributes(self.elbow_pv_ctrl, ['scaleX','scaleY','scaleZ','visibility'])


			DPTattr.createAttributeFromDic(self.elbow_pv_ctrl,
				[{'ln':'snapElbow','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
				 {'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
				 {'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'lowSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'hiSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'masterSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'lfArmIkCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}
				])
			DPTattr.showAttrInChanelBox(self.elbow_pv_ctrl, ['________SPACES___'])

			Cons = pm.parentConstraint(self.rootSpaceCtrl, self.worldSpaceCtrl,self.cogSpaceCtrl,
							self.LowSplineSpaceCtrl,self.HiSplineSpaceCtrl,self.master_spline,self.armSpaceCtrl, armPvCtrlGrp_space_grp, maintainOffset=True)

			rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='root', kwargs={'ud':True})
			worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='world', kwargs={'ud':True})
			cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='cog', kwargs={'ud':True})
			lowAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='low', kwargs={'ud':True})
			hiAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='hi', kwargs={'ud':True})
			masterAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='master', kwargs={'ud':True})
			ArmIkCtrlAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='ArmIk', kwargs={'ud':True})

			pm.connectAttr( self.elbow_pv_ctrl+'.rootCtrl', Cons+'.'+rootCtrlSpaceAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.worldSpaceCtrl', Cons+'.'+worldCtrlSpaceAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.cogCtrl', Cons+'.'+cogCtrlSpaceAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.lowSpineCtrl', Cons+'.'+lowAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.hiSpineCtrl', Cons+'.'+hiAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.masterSpineCtrl', Cons+'.'+masterAttr )
			pm.connectAttr( self.elbow_pv_ctrl+'.lfArmIkCtrl', Cons+'.'+ArmIkCtrlAttr )

			self.arm_ik_ctrl.pvControl >> self.elbow_pv_ctrl.visibility
			self.armIkVis_cond.outColorR >> elbow_pv_ctrl_frzGrp.visibility


			#create pole vector locator
			elbowLineLoc = pm.spaceLocator(n=self.sidePrefix+'elbow_line_loc')
			pos = pm.xform(self.elbow, q=True, ws=True, t=True )
			pm.move( pos[0], pos[1], pos[2], elbowLineLoc, a=True, ws=True, wd=True )
			pm.parent(elbowLineLoc, elbow_pv_ctrl_frzGrp)

			self.arm_ik_ctrl.pvControl >> elbowLineLoc.visibility
			pm.pointConstraint(self.elbow, elbowLineLoc)

			#create pole vector arrow
			arrowShape= pm.createNode( 'annotationShape', n=self.sidePrefix+'elbow_line_arrow' )
			arrowShape.setAttr('displayArrow', 1)
			arrowShape.setAttr('text', '')

			arrowTrans = arrowShape.getParent()
			pm.parent( arrowTrans, elbowLineLoc, relative=True )
			self.arm_ik_ctrl.pvControl >> arrowTrans.visibility


			elbow_pv_ctrl_annLoc = pm.spaceLocator(n=self.sidePrefix+'elbow_pv_ctrl_annLoc')
			pos = pm.xform(self.elbow_pv_ctrl, q=True, ws=True, t=True )
			pm.move( pos[0], pos[1], pos[2], elbow_pv_ctrl_annLoc, a=True, ws=True, wd=True )
			elbow_pv_ctrl_annLoc.hide()
			pm.parent(elbow_pv_ctrl_annLoc, self.elbow_pv_ctrl)

			pm.pointConstraint(self.elbow_pv_ctrl,elbow_pv_ctrl_annLoc)
			elbow_pv_ctrl_annLoc.getShape().worldMatrix >> arrowShape.dagObjectMatrix[0]


			#create pole vector constraint
			poleVecCons = pm.poleVectorConstraint( self.elbow_pv_ctrl, self.armIKHnd )

			elbowPvAttr = DPTattr.getMatchNameAttribute(node=poleVecCons, matchStr='elbow_pv', kwargs={'ud':True})
			pm.connectAttr( self.arm_ik_ctrl+'.pvControl', poleVecCons+'.'+elbowPvAttr )

			armPvCtrl_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'armPvCtrl_blend', param={'color2R':0,'color2G':0,'color2B':0} )

			self.arm_ik_ctrl.pvControl >> armPvCtrl_blend.blender
			poleVecCons.constraintTranslateX // self.armIKHnd.poleVectorX
			poleVecCons.constraintTranslateY // self.armIKHnd.poleVectorY
			poleVecCons.constraintTranslateZ // self.armIKHnd.poleVectorZ
			poleVecCons.constraintTranslateX >> armPvCtrl_blend.color1R
			poleVecCons.constraintTranslateY >> armPvCtrl_blend.color1G
			poleVecCons.constraintTranslateZ >> armPvCtrl_blend.color1B
			armPvCtrl_blend.outputR >> self.armIKHnd.poleVectorX
			armPvCtrl_blend.outputG >> self.armIKHnd.poleVectorY
			armPvCtrl_blend.outputB >> self.armIKHnd.poleVectorZ
			self.arm_ik_ctrl.armTwist >> self.armIKHnd.twist



		def createReaders(self):

			#create armRotReaderParent_jnt_grp
			upArmRotReaderParent_jnt_frzGrp_top = DPTgrp.createEmpty( self.sidePrefix+'upArmRotReaderParent_jnt_frzGrp_top', self.groupTop )
			DPTtrans.setTransformToObjectZero(srcObj=upArmRotReaderParent_jnt_frzGrp_top, targetObj=self.upArm, isTrans=True, isRot=True, isScale=False)
			upArmRotReaderParent_jnt_frzGrp = pm.duplicate(upArmRotReaderParent_jnt_frzGrp_top, n=self.sidePrefix+'upArmRotReaderParent_jnt_frzGrp',parentOnly=True)[0]
			pm.parent(upArmRotReaderParent_jnt_frzGrp_top, self.groupTop)
			pm.parent(upArmRotReaderParent_jnt_frzGrp,upArmRotReaderParent_jnt_frzGrp_top)

			upArmRotReaderParent_jnt = pm.duplicate(self.upArm, n=self.sidePrefix+'upArmRotReaderParent_jnt', parentOnly=True)[0]
			elbow_jnt1 = pm.duplicate(self.elbow, n=self.sidePrefix+'elbow_jnt1', parentOnly=True)[0]
			pm.parent(upArmRotReaderParent_jnt,upArmRotReaderParent_jnt_frzGrp)
			pm.parent(elbow_jnt1,upArmRotReaderParent_jnt)
			upArmRotReaderParent_jnt.hide()


			#create IK
			readUpArmIKHnd, readUpArmIKEffector = pm.ikHandle( sj=upArmRotReaderParent_jnt, ee=elbow_jnt1, sol='ikRPsolver' )

			self.upArmTwistReader_loc = pm.spaceLocator(n=self.sidePrefix+'upArmTwistReader_loc')
			DPTtrans.setTransformToObjectZero(srcObj=self.upArmTwistReader_loc, targetObj=upArmRotReaderParent_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=False)
			self.upArmTwistReader_loc.hide()

			#create upArmtwistJntReaderFollow_grp
			upArmtwistJntReaderFollow_grp = DPTgrp.createEmpty(self.sidePrefix+'upArmtwistJntReaderFollow_grp', self.groupTop)

			upArmFollowClavJnt = pm.duplicate(self.clavicle, n=self.sidePrefix+'upArmupJntFollow_jnt',parentOnly=True)[0]
			upArmFollowUpArmJnt = pm.duplicate(self.upArm, n=self.sidePrefix+'upArmupJntFollow_jnt2',parentOnly=True)[0]
			upArmFollowElbowJnt = pm.duplicate(self.elbow, n=self.sidePrefix+'upArmupJntFollow_jnt3',parentOnly=True)[0]

			pm.parent(upArmFollowClavJnt,upArmtwistJntReaderFollow_grp)
			pm.parent(upArmFollowUpArmJnt,upArmFollowClavJnt)
			pm.parent(upArmFollowElbowJnt,upArmFollowUpArmJnt)
			pm.parent(readUpArmIKHnd,upArmFollowElbowJnt)

			upArmFollowClavJnt.hide()
			readUpArmIKHnd.hide()

			#constrain and connect
			pm.parentConstraint(self.clavicle, upArmFollowClavJnt, maintainOffset=True)

			pm.parentConstraint(upArmFollowClavJnt, upArmRotReaderParent_jnt_frzGrp_top, maintainOffset=True)
			pm.pointConstraint(upArmFollowUpArmJnt, upArmRotReaderParent_jnt_frzGrp)

			pm.orientConstraint(upArmFollowUpArmJnt, self.upArmTwistReader_loc, skip=("y","z"))

			self.rigSettings_ctrl.lfUpArmReaderRotOrder >> upArmRotReaderParent_jnt.rotateOrder
			self.rigSettings_ctrl.lfUpArmReaderRotOrder >> self.upArmTwistReader_loc.rotateOrder
			self.rigSettings_ctrl.lfUpArmReaderRotOrder >> upArmFollowUpArmJnt.rotateOrder

			pm.connectAttr( self.upArm+'.translate', upArmFollowUpArmJnt+'.translate' )
			pm.connectAttr( self.upArm+'.rotate', upArmFollowUpArmJnt+'.rotate' )
			pm.connectAttr( self.upArm+'.scale', upArmFollowUpArmJnt+'.scale' )

			pm.connectAttr( self.elbow+'.translate', upArmFollowElbowJnt+'.translate' )
			pm.connectAttr( self.elbow+'.rotate', upArmFollowElbowJnt+'.rotate' )
			pm.connectAttr( self.elbow+'.scale', upArmFollowElbowJnt+'.scale' )

			####################################Create Mid Arm Reader####################################

			#create upArmMidJntRzReader_loc_frzGrp
			upArmMidJntRzReader_loc_frzGrp = DPTgrp.createEmpty( self.sidePrefix+'upArmMidJntRzReader_loc_frzGrp', self.groupTop )
			DPTtrans.setTransformToObjectZero(srcObj=upArmMidJntRzReader_loc_frzGrp, targetObj=self.upArm, isTrans=True, isRot=True, isScale=False)
			pm.parent(upArmMidJntRzReader_loc_frzGrp, self.groupTop)

			self.upArmMidJntRzReader_loc = pm.spaceLocator(n=self.sidePrefix+'upArmMidJntRzReader_loc')
			DPTtrans.setTransformToObjectZero(srcObj=self.upArmMidJntRzReader_loc, targetObj=upArmMidJntRzReader_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
			self.upArmMidJntRzReader_loc.hide()

			pm.parentConstraint(self.upArm, upArmMidJntRzReader_loc_frzGrp, maintainOffset=True)
			upArmMidJntRzReader_loc_aimCons = pm.aimConstraint( self.elbow, self.upArmMidJntRzReader_loc, aimVector=(1,0,0), upVector=(0,1,0), worldUpVector=(0,1,0), worldUpType=2, worldUpObject=self.elbow )


			####################################Create forearm Reader####################################

			#create forearmTwistTarget_loc_frzGrp
			forearmTwistTarget_loc_frzGrp = DPTgrp.createEmpty(self.sidePrefix+'forearmTwistTarget_loc_frzGrp', self.groupTop)
			DPTtrans.setTransformToObjectZero(srcObj=forearmTwistTarget_loc_frzGrp, targetObj=self.hand, isTrans=True, isRot=True, isScale=False)
			pm.parent(forearmTwistTarget_loc_frzGrp, self.groupTop)

			forearmTwistTarget_loc = pm.spaceLocator(n=self.sidePrefix+'forearmTwistTarget_loc')
			DPTtrans.setTransformToObjectZero(srcObj=forearmTwistTarget_loc, targetObj=forearmTwistTarget_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
			forearmTwistTarget_loc.hide()

			pm.parentConstraint(self.hand, forearmTwistTarget_loc_frzGrp, maintainOffset=True)


			#create forearmTwist_loc_frzGrp
			forearmTwist_loc_frzGrp = DPTgrp.createEmpty( self.sidePrefix+'forearmTwist_loc_frzGrp', self.groupTop )
			DPTtrans.setTransformToObjectZero(srcObj=forearmTwist_loc_frzGrp, targetObj=self.elbow, isTrans=True, isRot=True, isScale=False)
			pm.parent(forearmTwist_loc_frzGrp, self.groupTop)

			self.forearmTwist_loc = pm.spaceLocator(n=self.sidePrefix+'forearmTwist_loc')
			DPTtrans.setTransformToObjectZero(srcObj=self.forearmTwist_loc, targetObj=forearmTwist_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
			self.forearmTwist_loc.hide()

			pm.parentConstraint(self.elbow, forearmTwist_loc_frzGrp, maintainOffset=True)
			upArmMidJntRzReader_loc_aimCons = pm.aimConstraint( forearmTwistTarget_loc, self.forearmTwist_loc, aimVector=(0,0,0), upVector=(0,0,1), worldUpVector=(0,0,1), worldUpType=2, worldUpObject=forearmTwistTarget_loc )


			#create forearmUpTwistReader_jnt_frzGrp
			forearmUpTwistReader_jnt_frzGrp = DPTgrp.createEmpty( self.sidePrefix+'forearmUpTwistReader_jnt_frzGrp', self.groupTop )
			DPTtrans.setTransformToObjectZero(srcObj=forearmUpTwistReader_jnt_frzGrp, targetObj=self.hand, isTrans=True, isRot=True, isScale=False)
			pm.parent(forearmUpTwistReader_jnt_frzGrp, self.groupTop)

			forearmUpTwistReader_jnt1 = pm.duplicate(self.elbow, n=self.sidePrefix+'forearmUpTwistReader_jnt1',parentOnly=True)[0]
			DPTtrans.setTransformToObjectZero(srcObj=forearmUpTwistReader_jnt1, targetObj=self.hand, isTrans=True, isRot=False, isScale=False)
			forearmUpTwistReader_jnt2 = pm.duplicate(self.wrist, n=self.sidePrefix+'forearmUpTwistReader_jnt2',parentOnly=True)[0]
			pm.parent(forearmUpTwistReader_jnt2,forearmUpTwistReader_jnt1)
			pm.parent(forearmUpTwistReader_jnt1,forearmUpTwistReader_jnt_frzGrp)
			forearmUpTwistReader_jnt2.translateX.set(1)


			pm.parentConstraint(self.elbow, forearmUpTwistReader_jnt_frzGrp, maintainOffset=True)
			pm.pointConstraint(self.wrist, forearmUpTwistReader_jnt1)

			forearmUpTwistReader_jnt1.hide()

			#create IK
			forearmIKHnd, forearmIKEffector = pm.ikHandle( sj=forearmUpTwistReader_jnt1, ee=forearmUpTwistReader_jnt2, sol='ikRPsolver' )
			pm.parent(forearmIKHnd,self.groupTop)
			forearmIKHnd.hide()

			pm.parentConstraint(self.hand, forearmIKHnd, maintainOffset=True)


			#create forearmPvTwistReader_loc_frzGrpTop
			forearmPvTwistReader_loc_frzGrpTop = DPTgrp.createEmpty( self.sidePrefix+'forearmPvTwistReader_loc_frzGrp_top', self.groupTop )
			DPTtrans.setTransformToObjectZero(srcObj=forearmPvTwistReader_loc_frzGrpTop, targetObj=self.hand, isTrans=True, isRot=False, isScale=False)
			forearmPvTwistReader_loc_frzGrp = pm.duplicate(forearmPvTwistReader_loc_frzGrpTop, n=self.sidePrefix+'forearmPvTwistReader_loc_frzGrp',parentOnly=True)[0]
			pm.parent(forearmPvTwistReader_loc_frzGrpTop, self.groupTop)
			pm.parent(forearmPvTwistReader_loc_frzGrp, forearmPvTwistReader_loc_frzGrpTop)

			self.forearmPvTwistReader_loc = pm.spaceLocator(n=self.sidePrefix+'forearmPvTwistReader_loc')
			DPTtrans.setTransformToObjectZero(srcObj=self.forearmPvTwistReader_loc, targetObj=forearmPvTwistReader_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
			self.forearmPvTwistReader_loc.hide()

			DPTattr.createAttributeFromDic(self.forearmPvTwistReader_loc,
				[{'ln':'readerRot','h':True,'at':'float'}])

			pm.parentConstraint(self.wrist, forearmPvTwistReader_loc_frzGrpTop, maintainOffset=True)
			pm.parentConstraint(self.hand, forearmPvTwistReader_loc_frzGrp, maintainOffset=True)
			pm.orientConstraint(forearmUpTwistReader_jnt1, self.forearmPvTwistReader_loc)



		def createForearmTwistCtrl(self):
			#create forearmTwist_ctrl
			twistLocVec = DPTtrans.getPositionBetweenObjects(source=self.elbow, target=self.wrist, param=0.5)
			#srcTwistVec = pmu.VectorN(pm.xform(knee, q=True, ws=True, t=True))
			#targetTwistVec = pmu.VectorN(pm.xform(ankle, q=True, ws=True, t=True))
			#twistLocVec = srcTwistVec+(targetTwistVec-srcTwistVec)*0.5
			self.forearmTwist_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'forearmTwist_ctrl', size=5.0, position=twistLocVec)
			forearmTwist_ctrl_frzGrp, forearmTwist_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=self.forearmTwist_ctrl, parent=self.groupTop, position=twistLocVec)
			DPTtrans.setTransformToObjectZero( srcObj=forearmTwist_ctrl_frzGrp, targetObj=self.elbow, isTrans=False, isRot=True)
			pm.parent(forearmTwist_ctrl_frzGrp, self.groupTop)
			forearmTwist_ctrl_zeroFrzGrp.rotate.set(0,0,0)
			self.forearmTwist_ctrl.rotate.set(0,0,0)

			pm.parentConstraint(self.elbow, forearmTwist_ctrl_frzGrp, maintainOffset=True)

			forearmManualRotCtrl_cond = DPTnode.createNodeWithParam(node_type='condition',
																	 name=self.sidePrefix+'forearmManualRotCtrl_cond',
																	 param={'operation':2,'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																			'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':0} )

			self.hand_ctrl.forearmTwistCtrl >> forearmManualRotCtrl_cond.firstTerm
			forearmManualRotCtrl_cond.outColorR >> self.forearmTwist_ctrl.visibility

			self.forearmTwist_ctrl.translate.lock()
			self.forearmTwist_ctrl.rotateY.lock()
			self.forearmTwist_ctrl.rotateZ.lock()
			self.forearmTwist_ctrl.scale.lock()



		def createArmRubberHose(self):
			armRubberHose_grp = DPTgrp.createEmpty(self.sidePrefix+'armRubberHose_grp', self.groupTop)

			################create upArm################
			upArmRubberHose_grp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'upArmRubberHose_grp', alignObj=self.upArm, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			self.upArmRubberHoseCtrl_jnt = pm.duplicate(self.upArm, n=self.sidePrefix+'upArmRubberHoseCtrl_jnt',parentOnly=True)[0]
			upArmMidPos = DPTtrans.getPositionBetweenObjects(source=self.upArm, target=self.elbow, param=0.5)
			pm.move( upArmMidPos[0], upArmMidPos[1], upArmMidPos[2], self.upArmRubberHoseCtrl_jnt, a=True, ws=True, wd=True )
			self.upArmRubberHoseCtrl_jnt.hide()

			upArmRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'upArmRubberHose_ctrl', size=10.0, position=pm.xform(self.upArmRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
			upArmRubberHose_ctrl_frzGrp, upArmRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(upArmRubberHose_ctrl, None, True, False, False)
			DPTattr.hideAttributes(upArmRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
			upArmRubberHose_ctrl.scale.lock()

			self.upArmRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'upArmRubberHoseCtrl_jnt_frzGrp', alignObj=self.upArmRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			pm.parent(self.upArmRubberHoseCtrl_jnt, upArmRubberHose_ctrl)
			pm.parent(upArmRubberHose_ctrl_frzGrp, self.upArmRubberHoseCtrl_jnt_frzGrp)
			pm.parent(self.upArmRubberHoseCtrl_jnt_frzGrp, upArmRubberHose_grp)
			pm.parent(upArmRubberHose_grp,armRubberHose_grp)

			pm.parentConstraint(self.upArm, upArmRubberHose_grp, maintainOffset=True)
			self.hand_ctrl.rubberHoseCtrlVis >> upArmRubberHose_ctrl.visibility

			################create elbow################
			elbowRubberHose_grp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'elbowRubberHose_grp', alignObj=self.elbow, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			pm.parent(elbowRubberHose_grp,armRubberHose_grp)

			self.elbowRubberHoseCtrl_jnt = pm.duplicate(self.elbow, n=self.sidePrefix+'elbowRubberHoseCtrl_jnt',parentOnly=True)[0]
			elbowRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'elbowRubberHose_ctrl', size=10.0, position=pm.xform(self.elbowRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
			elbowRubberHose_ctrl_frzGrp, elbowRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(elbowRubberHose_ctrl, None, True, False, False)
			elbowRubberHose_ctrl.scale.lock()
			self.elbowRubberHoseCtrl_jnt.hide()
			DPTattr.hideAttributes(elbowRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

			elbowRubberHose_ctrl_frzGrpTop = pm.duplicate(elbowRubberHose_ctrl_frzGrp, n=elbowRubberHose_ctrl_frzGrp+'_top',parentOnly=True)[0]
			pm.parent(elbowRubberHose_ctrl_frzGrp,elbowRubberHose_ctrl_frzGrpTop)

			elbowRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'elbowRubberHoseCtrl_jnt_frzGrp', alignObj=self.elbowRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			pm.parent(elbowRubberHose_ctrl_frzGrpTop,elbowRubberHoseCtrl_jnt_frzGrp)

			pm.parent(self.elbowRubberHoseCtrl_jnt,elbowRubberHose_ctrl)
			pm.parent(elbowRubberHoseCtrl_jnt_frzGrp,armRubberHose_grp)

			pm.parentConstraint(self.elbow, elbowRubberHose_grp, maintainOffset=True)
			pm.pointConstraint(self.elbow, elbowRubberHose_ctrl_frzGrp)
			self.hand_ctrl.rubberHoseCtrlVis >> elbowRubberHose_ctrl.visibility

			#elbow upArm rot reader
			elbowUpJntRotReaderLoc_loc = pm.spaceLocator(n=self.sidePrefix+'elbowUpJntRotReaderLoc_loc')
			DPTtrans.setTransformToObjectZero(srcObj=elbowUpJntRotReaderLoc_loc, targetObj=self.upArm, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			elbowUpJntRotReaderLoc_loc.hide()

			elbowUpJntRotReaderLoc_loc_frzGrp, elbowUpJntRotReaderLoc_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(elbowUpJntRotReaderLoc_loc)

			pm.parent(elbowUpJntRotReaderLoc_loc_frzGrp,elbowRubberHose_grp)

			pm.parentConstraint(self.clavicle, elbowUpJntRotReaderLoc_loc_frzGrp, maintainOffset=True)
			pm.pointConstraint(self.upArm, elbowUpJntRotReaderLoc_loc_zeroFrzGrp)
			pm.orientConstraint(self.upArm, elbowUpJntRotReaderLoc_loc)

			#elbow foearm rot reader
			elbowLowJntRotReaderLoc_loc = pm.spaceLocator(n=self.sidePrefix+'elbowLowJntRotReaderLoc_loc')
			DPTtrans.setTransformToObjectZero(srcObj=elbowLowJntRotReaderLoc_loc, targetObj=self.elbow, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			elbowLowJntRotReaderLoc_loc.hide()

			elbowLowJntRotReaderLoc_loc_frzGrp, elbowLowJntRotReaderLoc_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(elbowLowJntRotReaderLoc_loc)

			pm.parent(elbowLowJntRotReaderLoc_loc_frzGrp, elbowRubberHose_grp)

			pm.parentConstraint(self.upArm, elbowLowJntRotReaderLoc_loc_frzGrp, maintainOffset=True)
			pm.pointConstraint(self.elbow, elbowLowJntRotReaderLoc_loc_zeroFrzGrp)
			pm.orientConstraint(self.elbow, elbowLowJntRotReaderLoc_loc)

			################create forearm################
			forearmRubberHose_grp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'forearmRubberHose_grp', alignObj=self.elbow, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			self.forearmRubberHoseCtrl_jnt = pm.duplicate(self.elbow, n=self.sidePrefix+'forearmRubberHoseCtrl_jnt',parentOnly=True)[0]
			forearmMidPos = DPTtrans.getPositionBetweenObjects(source=self.elbow, target=self.wrist, param=0.5)
			pm.move( forearmMidPos[0], forearmMidPos[1], forearmMidPos[2], self.forearmRubberHoseCtrl_jnt, a=True, ws=True, wd=True )
			self.forearmRubberHoseCtrl_jnt.hide()


			forearmRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=self.sidePrefix+'forearmRubberHose_ctrl', size=10.0, position=pm.xform(self.forearmRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
			forearmRubberHose_ctrl_frzGrp, forearmRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(forearmRubberHose_ctrl, None, True, False, False)
			forearmRubberHose_ctrl_frzGrp.scale.lock()
			DPTattr.hideAttributes(forearmRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
			forearmRubberHose_ctrl.scale.lock()

			self.forearmRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'forearmRubberHoseCtrl_jnt', alignObj=self.forearmRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			self.arm_ribbonLowEndBind_jnt = pm.duplicate(self.wrist, n=self.sidePrefix+'arm_ribbonLowEndBind_jnt',parentOnly=True)[0]
			self.arm_ribbonLowEndBind_jnt.hide()

			pm.parent(self.forearmRubberHoseCtrl_jnt, forearmRubberHose_ctrl)
			pm.parent(forearmRubberHose_ctrl_frzGrp, self.forearmRubberHoseCtrl_jnt_frzGrp)
			pm.parent(self.forearmRubberHoseCtrl_jnt_frzGrp, forearmRubberHose_grp)
			pm.parent(self.arm_ribbonLowEndBind_jnt, forearmRubberHose_grp)
			pm.parent(forearmRubberHose_grp,armRubberHose_grp)

			pm.parentConstraint(self.elbow, forearmRubberHose_grp, maintainOffset=True)
			pm.pointConstraint(self.wrist, self.arm_ribbonLowEndBind_jnt)
			self.hand_ctrl.rubberHoseCtrlVis >> forearmRubberHose_ctrl.visibility


			################armUpMidSoftJnt_jnt################
			self.armUpMidSoftJnt_jnt = pm.duplicate(self.upArm, n=self.sidePrefix+'armUpMidSoftJnt_jnt',parentOnly=True)[0]
			param = 0.99
			if DPTStr.isContain(self.sidePrefix, 'R'):
				param = 1.01
			armUpMidSoftPos = DPTtrans.getPositionBetweenObjects(source=self.upArm, target=self.elbow, param=param)
			elbowPos = DPTtrans.getPositionBetweenObjects(source=self.upArm, target=self.elbow, param=1.0)
			pm.move( elbowPos[0], elbowPos[1], elbowPos[2], self.armUpMidSoftJnt_jnt, a=True, ws=True, wd=True )
			self.armUpMidSoftJnt_jnt.hide()

			armUpMidSoftJnt_jnt_frzGrp, self.armUpMidSoftJnt_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.armUpMidSoftJnt_jnt)
			pm.move( armUpMidSoftPos[0], armUpMidSoftPos[1], armUpMidSoftPos[2], self.armUpMidSoftJnt_jnt, a=True, ws=True, wd=True )

			armUpMidSoftJnt_jnt_frzGrp_AimCons = pm.duplicate(armUpMidSoftJnt_jnt_frzGrp, n=armUpMidSoftJnt_jnt_frzGrp+'_AimCons',parentOnly=True)[0]
			armUpMidSoftJnt_jnt_frzGrp_Top = pm.duplicate(armUpMidSoftJnt_jnt_frzGrp, n=armUpMidSoftJnt_jnt_frzGrp+'_top',parentOnly=True)[0]

			pm.parent(armUpMidSoftJnt_jnt_frzGrp, armUpMidSoftJnt_jnt_frzGrp_AimCons)
			pm.parent(armUpMidSoftJnt_jnt_frzGrp_AimCons, armUpMidSoftJnt_jnt_frzGrp_Top)
			pm.parent(armUpMidSoftJnt_jnt_frzGrp_Top, armRubberHose_grp)

			pm.aimConstraint( self.upArmRubberHoseCtrl_jnt, armUpMidSoftJnt_jnt_frzGrp_AimCons, aimVector=(-1,0,0), upVector=(0,-1,0), worldUpVector=(0,1,0), worldUpType=1, worldUpObject=self.upArmRubberHoseCtrl_jnt )


			################create armDnMidSoftJnt_jnt################
			self.armDnMidSoftJnt_jnt = pm.duplicate(self.elbow, n=self.sidePrefix+'armDnMidSoftJnt_jnt',parentOnly=True)[0]
			param = 1.01
			if DPTStr.isContain(self.sidePrefix, 'R'):
				param = 0.99

			armDnMidSoftPos = DPTtrans.getPositionBetweenObjects(source=self.wrist, target=self.elbow, param=param)
			self.armDnMidSoftJnt_jnt.hide()

			armDnMidSoftJnt_jnt_frzGrp, self.armDnMidSoftJnt_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.armDnMidSoftJnt_jnt)
			pm.move( armDnMidSoftPos[0], armDnMidSoftPos[1], armDnMidSoftPos[2], self.armDnMidSoftJnt_jnt, a=True, ws=True, wd=True )

			armDnMidSoftJnt_jnt_frzGrp_AimCons = pm.duplicate(armDnMidSoftJnt_jnt_frzGrp, n=armDnMidSoftJnt_jnt_frzGrp+'_AimCons',parentOnly=True)[0]
			armDnMidSoftJnt_jnt_frzGrp_Top = pm.duplicate(armDnMidSoftJnt_jnt_frzGrp, n=armDnMidSoftJnt_jnt_frzGrp+'_top',parentOnly=True)[0]

			pm.parent(armDnMidSoftJnt_jnt_frzGrp, armDnMidSoftJnt_jnt_frzGrp_AimCons)
			pm.parent(armDnMidSoftJnt_jnt_frzGrp_AimCons, armDnMidSoftJnt_jnt_frzGrp_Top)
			pm.parent(armDnMidSoftJnt_jnt_frzGrp_Top, armRubberHose_grp)

			pm.aimConstraint(self.forearmRubberHoseCtrl_jnt, armDnMidSoftJnt_jnt_frzGrp_AimCons, aimVector=(1,0,0), upVector=(0,1,0), worldUpVector=(0,1,0), worldUpType=1, worldUpObject=self.forearmRubberHoseCtrl_jnt )


			################create armMidUpSoftTarg_loc################
			armMidUpSoftTarg_loc = pm.spaceLocator(n=self.sidePrefix+'armMidUpSoftTarg_loc')
			DPTtrans.setTransformToObjectZero(srcObj=armMidUpSoftTarg_loc, targetObj=self.upArm, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			armMidUpSoftTarg_loc.hide()

			armMidUpSoftTarg_loc_frzGrp, armMidUpSoftTarg_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(armMidUpSoftTarg_loc)
			pm.parent(armMidUpSoftTarg_loc_frzGrp, armRubberHose_grp)

			armMidUpHardTarg_loc = pm.spaceLocator(n=self.sidePrefix+'armMidUpHardTarg_loc')
			DPTtrans.setTransformToObjectZero(srcObj=armMidUpHardTarg_loc, targetObj=armMidUpSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			armMidUpHardTarg_loc.hide()

			armMidUpHardTarg_loc_frzGrp= DPTgrp.createEmptyWithAlign(name=armMidUpHardTarg_loc+'_frzGrp', alignObj=armMidUpHardTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			pm.parent(armMidUpHardTarg_loc, armMidUpHardTarg_loc_frzGrp)
			pm.parent(armMidUpHardTarg_loc_frzGrp, armMidUpSoftTarg_loc_zeroFrzGrp)

			pm.parentConstraint(self.upArm, armMidUpSoftTarg_loc_frzGrp, maintainOffset=True)
			pm.pointConstraint(self.elbow, armMidUpSoftTarg_loc_zeroFrzGrp)

			pm.pointConstraint(self.elbowRubberHoseCtrl_jnt, armMidUpHardTarg_loc_frzGrp)

			################create armMidDnSoftTarg_loc################
			armMidDnSoftTarg_loc = pm.spaceLocator(n=self.sidePrefix+'armMidDnSoftTarg_loc')
			DPTtrans.setTransformToObjectZero(srcObj=armMidDnSoftTarg_loc, targetObj=self.elbow, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			armMidDnSoftTarg_loc.hide()

			armMidDnSoftTarg_loc_frzGrp= DPTgrp.createEmptyWithAlign(name=armMidDnSoftTarg_loc+'_frzGrp', alignObj=armMidDnSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			pm.parent(armMidDnSoftTarg_loc,armMidDnSoftTarg_loc_frzGrp)
			pm.parent(armMidDnSoftTarg_loc_frzGrp, armRubberHose_grp)

			armMidDnHardTarg_loc = pm.spaceLocator(n=self.sidePrefix+'armMidDnHardTarg_loc')
			DPTtrans.setTransformToObjectZero(srcObj=armMidDnHardTarg_loc, targetObj=armMidDnSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			armMidDnHardTarg_loc_frzGrp = pm.duplicate(armMidDnSoftTarg_loc_frzGrp, n=self.sidePrefix+'armMidDnHardTarg_loc_frzGrp',parentOnly=True)[0]
			armMidDnHardTarg_loc.hide()

			pm.parent(armMidDnHardTarg_loc, armMidDnHardTarg_loc_frzGrp)
			pm.parent(armMidDnHardTarg_loc_frzGrp, armMidDnSoftTarg_loc_frzGrp)


			pm.parentConstraint(self.elbow, armMidDnSoftTarg_loc_frzGrp, maintainOffset=True)
			pm.parentConstraint(self.elbowRubberHoseCtrl_jnt, armMidDnHardTarg_loc_frzGrp, maintainOffset=True)

			################other constrain and connect################
			upArmMidCons = pm.parentConstraint(armMidUpSoftTarg_loc, armMidUpHardTarg_loc, armUpMidSoftJnt_jnt_frzGrp_Top, maintainOffset=True )

			softAttr = DPTattr.getMatchNameAttribute(node=upArmMidCons, matchStr='Soft', kwargs={'ud':True})
			hardAttr = DPTattr.getMatchNameAttribute(node=upArmMidCons, matchStr='Hard', kwargs={'ud':True})

			elbowSoftness_reverse = DPTnode.createNodeWithParam(node_type='reverse', name=self.sidePrefix+'elbowSoftness_reverse', param={} )

			self.hand_ctrl.elbowSoftness >> elbowSoftness_reverse.inputX

			pm.connectAttr( self.hand_ctrl+'.elbowSoftness', upArmMidCons+'.'+softAttr )
			pm.connectAttr( elbowSoftness_reverse+'.outputX', upArmMidCons+'.'+hardAttr )



			forearmMidCons = pm.parentConstraint(armMidDnSoftTarg_loc, armMidDnHardTarg_loc, armDnMidSoftJnt_jnt_frzGrp_Top, maintainOffset=True )

			softAttr = DPTattr.getMatchNameAttribute(node=forearmMidCons, matchStr='Soft', kwargs={'ud':True})
			hardAttr = DPTattr.getMatchNameAttribute(node=forearmMidCons, matchStr='Hard', kwargs={'ud':True})

			pm.connectAttr( self.hand_ctrl+'.elbowSoftness', forearmMidCons+'.'+softAttr )
			pm.connectAttr( elbowSoftness_reverse+'.outputX', forearmMidCons+'.'+hardAttr )


		def createCrvAndSurf(self):
			armNoScale_rig_grp = DPTgrp.createEmpty(self.sidePrefix+'armNoScale_rig_grp', self.groupTop)
			armNoScale_rig_grp.setAttr('inheritsTransform',0)

			#create ik curve
			self.armCurve = DPTCurveTool.createCurveAlongObject(name=self.sidePrefix+'armCurve', objects=[self.upArm,self.upArmRubberHoseCtrl_jnt,self.armUpMidSoftJnt_jnt,self.elbowRubberHoseCtrl_jnt,
																									 self.armDnMidSoftJnt_jnt,self.forearmRubberHoseCtrl_jnt,self.arm_ribbonLowEndBind_jnt],degree=3)
			self.armCurveShape = self.armCurve.getShape()
			pm.parent(self.armCurve, armNoScale_rig_grp)
			self.armCurve.hide()

			#create ik surf and follicle
			armCurve_crvLoft1 = pm.duplicate(self.armCurve, n=self.armCurve+'_loft1')[0]
			armCurve_crvLoft2 = pm.duplicate(self.armCurve, n=self.armCurve+'_loft2')[0]
			pm.move( 0, 0.5, 0,armCurve_crvLoft1, r=True )
			pm.move( 0, -0.5, 0,armCurve_crvLoft2, r=True )

			arm_ribbon_surf = pm.loft(armCurve_crvLoft1,armCurve_crvLoft2, ch=False, rn=True, degree=1, name=self.sidePrefix+'arm_ribbon_surf' )[0]
			pm.delete(armCurve_crvLoft1, armCurve_crvLoft2)
			pm.parent(arm_ribbon_surf, armNoScale_rig_grp)
			arm_ribbon_surf.hide()

			forearmRhFollicles_grp = DPTgrp.createEmpty(self.sidePrefix+'forearmRhFollicles_grp',armNoScale_rig_grp)

			jNum = 1
			jList = [self.upArm,self.upArmRubberHoseCtrl_jnt,self.armUpMidSoftJnt_jnt,self.elbowRubberHoseCtrl_jnt,self.armDnMidSoftJnt_jnt,self.forearmRubberHoseCtrl_jnt,self.arm_ribbonLowEndBind_jnt]
			pJntList = ['up_arm'+self.sidePrefix+'_weight_jnt1','up_arm'+self.sidePrefix+'_weight_jnt2','up_arm'+self.sidePrefix+'_weight_jnt3',
						'elbow'+self.sidePrefix+'_weight_jnt1','elbow'+self.sidePrefix+'_weight_jnt2','elbow'+self.sidePrefix+'_weight_jnt3','wrist'+self.sidePrefix+'_weight_jnt1']


			self.folOriJoints = []

			for jnt in jList:
				armRibbon_loc = pm.spaceLocator(n=self.sidePrefix+'armRibbon_'+str(jNum)+'_loc')

				vParam = DPTCurveTool.closestPointOnCurve(location=pm.xform(pJntList[jNum-1], q=True, ws=True, t=True ), curveObject=self.armCurve)
				#print vParam

				armFollOrient_grp = DPTgrp.createEmpty(name=self.sidePrefix+'armFollOrient'+str(jNum)+'_grp')
				armFollOrient_grp_frzGrp = pm.duplicate(armFollOrient_grp, n=self.sidePrefix+'legFollOrient'+str(jNum)+'_grp_frzGrp',parentOnly=True)[0]

				pm.parent(armFollOrient_grp, armFollOrient_grp_frzGrp)
				pm.parent(armFollOrient_grp_frzGrp, armRibbon_loc)
				pm.parent(armRibbon_loc, forearmRhFollicles_grp)

				self.folOriJoints.append(armFollOrient_grp)

				onSrfPtNode = DPTnode.createNodeWithParam(node_type='pointOnSurfaceInfo', name='pointOnSurfaceInfo1', param={'parameterU':0.5,'parameterV':vParam} )
				jntAimCons = DPTnode.createNodeWithParam(node_type='aimConstraint', name='rivetAimConstraint1',param={'aimVectorX':0,'aimVectorY':1,'aimVectorZ':0,
																									'upVectorX':-1,'upVectorY':0,'upVectorZ':0,'worldUpType':3} )

				pm.parent(jntAimCons, armRibbon_loc)

				arm_ribbon_surf.worldSpace >> onSrfPtNode.inputSurface
				onSrfPtNode.result.position >> armRibbon_loc.translate
				onSrfPtNode.result.normal >> jntAimCons.target[0].targetTranslate
				onSrfPtNode.result.tangentU >> jntAimCons.worldUpVector
				jntAimCons.constraintRotateX >> armRibbon_loc.rotateX
				jntAimCons.constraintRotateY >> armRibbon_loc.rotateY
				jntAimCons.constraintRotateZ >> armRibbon_loc.rotateZ
				armRibbon_loc.hide()

				jNum+=1


			#smoothbind ik curve and loft surf

			armRibbonSurfSkinClster = pm.skinCluster(jList, arm_ribbon_surf, sm=0, tsb=True, mi=1, nw=1)
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][0]', transformValue=[(jList[0], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][1]', transformValue=[(jList[1], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][2]', transformValue=[(jList[2], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][3]', transformValue=[(jList[3], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][4]', transformValue=[(jList[4], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][5]', transformValue=[(jList[5], 1.0)])
			pm.skinPercent( armRibbonSurfSkinClster, arm_ribbon_surf+'.cv[0:1][6]', transformValue=[(jList[6], 1.0)])

			ikSkinClster = pm.skinCluster(jList, self.armCurve, sm=0, tsb=True, mi=1, nw=1)
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[0]', transformValue=[(jList[0], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[1]', transformValue=[(jList[1], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[2]', transformValue=[(jList[2], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[3]', transformValue=[(jList[3], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[4]', transformValue=[(jList[4], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[5]', transformValue=[(jList[5], 1.0)])
			pm.skinPercent( ikSkinClster, self.armCurve+'.cv[6]', transformValue=[(jList[6], 1.0)])



		def createWeightJoints(self):
			#create top group
			armWtCtrlJnt_grp = DPTgrp.createEmpty( self.sidePrefix+'armWtCtrlJnt_grp', self.groupTop )

			#upArmDnSplitCtrlParent_jnt
			upArmDnSplitCtrlParent_jnt = pm.duplicate(self.elbow, n=self.sidePrefix+'upArmDnSplitCtrlParent_jnt',parentOnly=True)[0]
			pm.parent(upArmDnSplitCtrlParent_jnt, armWtCtrlJnt_grp)
			upArmDnSplitCtrlParent_jnt.hide()

			pm.parentConstraint(self.elbow, upArmDnSplitCtrlParent_jnt, maintainOffset=True)


			#create forearm weight joint
			self.elbowCtrlSplit_1_jnt = pm.duplicate('elbow'+self.sidePrefix+'_weight_jnt2', n=self.sidePrefix+'elbowCtrlSplit_1_jnt',parentOnly=True)[0]
			self.elbowCtrlSplit_2_jnt = pm.duplicate('elbow'+self.sidePrefix+'_weight_jnt3', n=self.sidePrefix+'elbowCtrlSplit_2_jnt',parentOnly=True)[0]
			self.wristCtrl_jnt = pm.duplicate('wrist'+self.sidePrefix+'_weight_jnt1', n=self.sidePrefix+'wristCtrl_jnt',parentOnly=True)[0]
			self.elbowCtrlSplit_1_jnt.hide()
			self.elbowCtrlSplit_2_jnt.hide()
			self.wristCtrl_jnt.hide()

			elbowCtrlSplit_1_jnt_frzGrp, elbowCtrlSplit_1_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.elbowCtrlSplit_1_jnt, parent=upArmDnSplitCtrlParent_jnt)
			elbowCtrlSplit_2_jnt_frzGrp, elbowCtrlSplit_2_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.elbowCtrlSplit_2_jnt, parent=upArmDnSplitCtrlParent_jnt)
			wristCtrl_jnt_frzGrp, wristCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.wristCtrl_jnt, parent=upArmDnSplitCtrlParent_jnt)


			pm.parentConstraint(self.folOriJoints[4], elbowCtrlSplit_1_jnt_frzGrp, maintainOffset=True)
			pm.parentConstraint(self.folOriJoints[5], elbowCtrlSplit_2_jnt_frzGrp, maintainOffset=True)
			pm.parentConstraint(self.folOriJoints[6], wristCtrl_jnt_frzGrp, maintainOffset=True)


			#create upArm weight joint
			upArmUpSplitCtrlParent_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'upArmUpSplitCtrlParent_jnt_frzGrp', alignObj=self.upArm, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			upArmUpSplitCtrlParent_jnt = pm.duplicate(self.upArm, n=self.sidePrefix+'upArmUpSplitCtrlParent_jnt',parentOnly=True)[0]
			pm.parent(upArmUpSplitCtrlParent_jnt,upArmUpSplitCtrlParent_jnt_frzGrp)
			pm.parent(upArmUpSplitCtrlParent_jnt_frzGrp, armWtCtrlJnt_grp)
			upArmUpSplitCtrlParent_jnt.hide()

			pm.parentConstraint(self.upArm, upArmUpSplitCtrlParent_jnt_frzGrp)


			self.upArmCtrlSplit_1_jnt = pm.duplicate('up_arm'+self.sidePrefix+'_weight_jnt2', n=self.sidePrefix+'upArmCtrlSplit_1_jnt',parentOnly=True)[0]
			self.upArmCtrlSplit_2_jnt = pm.duplicate('up_arm'+self.sidePrefix+'_weight_jnt3', n=self.sidePrefix+'upArmCtrlSplit_2_jnt',parentOnly=True)[0]
			self.upArmCtrlSplit_1_jnt.hide()
			self.upArmCtrlSplit_2_jnt.hide()

			upArmCtrlSplit_1_jnt_frzGrp, self.upArmCtrlSplit_1_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.upArmCtrlSplit_1_jnt, parent=upArmUpSplitCtrlParent_jnt)
			upArmCtrlSplit_2_jnt_frzGrp, self.upArmCtrlSplit_2_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.upArmCtrlSplit_2_jnt, parent=upArmUpSplitCtrlParent_jnt)

			pm.parentConstraint(self.folOriJoints[1], upArmCtrlSplit_1_jnt_frzGrp, maintainOffset=True)
			pm.parentConstraint(self.folOriJoints[2], upArmCtrlSplit_2_jnt_frzGrp, maintainOffset=True)

			#create other
			upArmWtCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=self.sidePrefix+'upArmWtCtrl_jnt_frzGrp', alignObj=self.upArm, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			pm.parent(upArmWtCtrl_jnt_frzGrp, armWtCtrlJnt_grp)

			pm.parentConstraint(upArmUpSplitCtrlParent_jnt, upArmWtCtrl_jnt_frzGrp, maintainOffset=True)


		def createRhUpMidDn(self):
			#top group
			armRhUpMidDnCtrlJnt_grp = DPTgrp.createEmpty( self.sidePrefix+'armRhUpMidDnCtrlJnt_grp', self.groupTop )

			self.upArmCtrl_jnt = pm.duplicate(self.upArm, n=self.sidePrefix+'upArmCtrl_jnt',parentOnly=True)[0]
			self.elbowCtrl_jnt = pm.duplicate(self.elbow, n=self.sidePrefix+'elbowCtrl_jnt',parentOnly=True)[0]
			self.upArmCtrl_jnt.hide()
			self.elbowCtrl_jnt.hide()

			upArmCtrl_jnt_frzGrp, self.upArmCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.upArmCtrl_jnt, parent=armRhUpMidDnCtrlJnt_grp)
			elbowCtrl_jnt_frzGrp, elbowCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=self.elbowCtrl_jnt, parent=armRhUpMidDnCtrlJnt_grp)

			pm.parentConstraint(self.folOriJoints[0], upArmCtrl_jnt_frzGrp, maintainOffset=True)
			pm.parentConstraint(self.folOriJoints[3], elbowCtrl_jnt_frzGrp, maintainOffset=True)


		def createConnections(self):
			upArmDistVal = DPTtrans.getDistanceBetweenObjects(source=self.upArm, target=self.elbow)
			forearmDistVal = DPTtrans.getDistanceBetweenObjects(source=self.elbow, target=self.wrist)

			#FK Translate
			armStrAtt_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStrAtt_multDiv', param={'operation':2,'input2X':10,'input2Y':10,'input2Z':10} )
			armTotStrAmt_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armTotStrAmt_plsMns', param={'operation':1} )
			armRefStr_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armRefStr_multDiv', param={'operation':1,'input2Z':1} )
			armUpStr_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armUpStr_plsMns', param={'operation':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				armUpStr_plsMns.setAttr('input1D[1]', upArmDistVal)
				armRefStr_multDiv.setAttr('input2X', upArmDistVal)
				armRefStr_multDiv.setAttr('input2Y', forearmDistVal)
			else:
				armUpStr_plsMns.setAttr('input1D[1]', -upArmDistVal)
				armRefStr_multDiv.setAttr('input2X', -upArmDistVal)
				armRefStr_multDiv.setAttr('input2Y', -forearmDistVal)


			self.hand_ctrl.wristStretch >> armStrAtt_multDiv.input1Z
			self.hand_ctrl.elbowStretch >> armStrAtt_multDiv.input1Y
			self.hand_ctrl.armStretch >> armStrAtt_multDiv.input1X

			DPTattr.connectAttrToList(armStrAtt_multDiv+'.outputX', [armTotStrAmt_plsMns+'.input2D[0].input2Dx',armTotStrAmt_plsMns+'.input2D[0].input2Dy'])
			armStrAtt_multDiv.outputY >> armTotStrAmt_plsMns.input2D[1].input2Dx
			armStrAtt_multDiv.outputZ >> armTotStrAmt_plsMns.input2D[1].input2Dy

			armTotStrAmt_plsMns.output2Dx >> armRefStr_multDiv.input1X
			armTotStrAmt_plsMns.output2Dy >> armRefStr_multDiv.input1Y

			armRefStr_multDiv.outputX >> armUpStr_plsMns.input1D[0]
			##########################OUT##########################
			armUpStr_plsMns.output1D >> self.FKElbowJnt.translateX


			#forearm Translate
			armLowStr_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armLowStr_plsMns', param={'operation':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				armLowStr_plsMns.setAttr('input1D[1]', forearmDistVal)
			else:
				armLowStr_plsMns.setAttr('input1D[1]', -forearmDistVal)

			armRefStr_multDiv.outputY >> armLowStr_plsMns.input1D[0]
			##########################OUT##########################
			armLowStr_plsMns.output1D >> self.FKWristJnt.translateX


			#Denom Translate
			armStrDenom_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armStrDenom_plsMns', param={'operation':1} )

			armLowStr_plsMns.output1D >> armStrDenom_plsMns.input1D[0]
			armUpStr_plsMns.output1D >> armStrDenom_plsMns.input1D[1]


			#from armStrDistShape
			armDistScaleAdjust_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armDistScaleAdjust_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			self.armStrDistShape.distance >> armDistScaleAdjust_multDiv.input1X
			self.root_ctrl.masterScale >> armDistScaleAdjust_multDiv.input2X


			armStrNum_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStrNum_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armDistScaleAdjust_multDiv.outputX >> armStrNum_multDiv.input1X
			self.arm_ik_ctrl.stretchyArm >> armStrNum_multDiv.input2X


			armStrScalar_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStrScalar_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armStrNum_multDiv.outputX >> armStrScalar_multDiv.input1X
			armStrDenom_plsMns.output1D >> armStrScalar_multDiv.input2X


			armStr_clamp = DPTnode.createNodeWithParam(node_type='clamp', name=self.sidePrefix+'armStr_clamp', param={'minR':1} )


			if DPTStr.isContain(self.sidePrefix, 'L'):
				armStrScalar_multDiv.outputX >> armStr_clamp.inputR
				self.arm_ik_ctrl.maxStretch >> armStr_clamp.maxR
			else:
				armNegateTx_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armNegateTx_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )
				armStrScalar_multDiv.outputX >> armNegateTx_multDiv.input1X
				armNegateTx_multDiv.outputX >> armStr_clamp.inputR
				self.arm_ik_ctrl.maxStretch >> armStr_clamp.maxR




			armStr_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStr_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armUpStr_plsMns.output1D >> armStr_multDiv.input1X
			armLowStr_plsMns.output1D >> armStr_multDiv.input1Y
			armStr_clamp.outputR >> armStr_multDiv.input2X
			armStr_clamp.outputR >> armStr_multDiv.input2Y


			#from armPvIkCtrlDistShape and upArmPvCtrlDistShape
			armPvDistScaleAdjust_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armPvDistScaleAdjust_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			self.upArmPvCtrlDistShape.distance >> armPvDistScaleAdjust_multDiv.input1X
			self.armPvIkCtrlDistShape.distance >> armPvDistScaleAdjust_multDiv.input1Y

			self.root_ctrl.masterScale >> armPvDistScaleAdjust_multDiv.input2X
			self.root_ctrl.masterScale >> armPvDistScaleAdjust_multDiv.input2Y


			#blendColors
			upArmSnapSplice_blend  = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'upArmSnapSplice_blend', param={} )
			elbowSnapSplice_blend  = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'elbowSnapSplice_blend', param={} )

			self.elbow_pv_ctrl.snapElbow >> upArmSnapSplice_blend.blender
			self.elbow_pv_ctrl.snapElbow >> elbowSnapSplice_blend.blender
			armStr_multDiv.outputX >> upArmSnapSplice_blend.color2R
			armStr_multDiv.outputY >> elbowSnapSplice_blend.color2R


			if DPTStr.isContain(self.sidePrefix, 'L'):
				armPvDistScaleAdjust_multDiv.outputX >> upArmSnapSplice_blend.color1R
				armPvDistScaleAdjust_multDiv.outputY >> elbowSnapSplice_blend.color1R
			else:
				armPvNegate_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armPvNegate_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )
				armPvDistScaleAdjust_multDiv.outputX >> armPvNegate_multDiv.input1X
				armPvDistScaleAdjust_multDiv.outputY >> armPvNegate_multDiv.input1Y
				armPvNegate_multDiv.outputX >> upArmSnapSplice_blend.color1R
				armPvNegate_multDiv.outputY >> elbowSnapSplice_blend.color1R

			##########################OUT##########################
			upArmSnapSplice_blend.outputR >> self.IKElbowJnt.translateX

			##########################OUT##########################
			elbowSnapSplice_blend.outputR >> self.IKWristJnt.translateX



			#strech value
			arm_crvInfo  = DPTnode.createNodeWithParam(node_type='curveInfo', name=self.sidePrefix+'arm_crvInfo', param={} )
			armStretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStretch_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
			scaleAdjustedDefaultArcLen_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'scaleAdjustedDefaultArcLen_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			self.armCurveShape.worldSpace >> arm_crvInfo.inputCurve
			self.root_ctrl.masterScale >> scaleAdjustedDefaultArcLen_multDiv.input2X

			scaleAdjustedDefaultArcLen_multDiv.setAttr('input1X', pm.arclen( self.armCurve ) )

			arm_crvInfo.arcLength >> armStretch_multDiv.input1X
			scaleAdjustedDefaultArcLen_multDiv.outputX >> armStretch_multDiv.input2X

			##########################OUT##########################
			armStretch_multDiv.outputX >> self.arm_ik_ctrl.stretchValue




			armNormStretchVal_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armNormStretchVal_plsMns', param={'operation':2,'input1D[0]':1} )
			armStretch_multDiv.outputX >> armNormStretchVal_plsMns.input1D[1]


			armNormStretchNegator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armNormStretchNegator_multDiv', param={'operation':1,'input1X':-1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
			armNormStretchVal_plsMns.output1D >> armNormStretchNegator_multDiv.input2X


			armNormStretch_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+'armNormStretch_cond', param={'operation':3,'secondTerm':0} )

			armNormStretchVal_plsMns.output1D >> armNormStretch_cond.firstTerm
			armNormStretchVal_plsMns.output1D >> armNormStretch_cond.colorIfTrueR
			armNormStretchNegator_multDiv.outputX >> armNormStretch_cond.colorIfFalseR



			armStretchSpeed_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStretchSpeed_multDiv', param={'operation':2,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftArmStretchSpeed >> armStretchSpeed_multDiv.input2X
			else:
				self.rigSettings_ctrl.rightArmStretchSpeed >> armStretchSpeed_multDiv.input2X


			armStretchNumerator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStretchNumerator_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armNormStretch_cond.outColorR >> armStretchNumerator_multDiv.input1X

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftArmMaxStretchAmt >> armStretchNumerator_multDiv.input2X
			else:
				self.rigSettings_ctrl.rightArmMaxStretchAmt >> armStretchNumerator_multDiv.input2X



			armStretchDenominator_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armStretchDenominator_plsMns', param={'operation':1} )

			armNormStretch_cond.outColorR >> armStretchDenominator_plsMns.input1D[0]
			armStretchSpeed_multDiv.outputX >> armStretchDenominator_plsMns.input1D[1]




			armStretch_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStretch_multDiv1', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armStretchNumerator_multDiv.outputX >> armStretch_multDiv1.input1X
			armStretchDenominator_plsMns.output1D >> armStretch_multDiv1.input2X


			armStretchNegator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armStretchNegator_multDiv', param={'operation':1,'input1X':-1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armStretch_multDiv1.outputX >> armStretchNegator_multDiv.input2X



			armSquashSpeed_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquashSpeed_multDiv', param={'operation':2,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftArmSquashSpeed >> armSquashSpeed_multDiv.input2X
			else:
				self.rigSettings_ctrl.rightArmSquashSpeed >> armSquashSpeed_multDiv.input2X


			armSquashDenominator_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armSquashDenominator_plsMns', param={'operation':1} )

			armNormStretch_cond.outColorR >> armSquashDenominator_plsMns.input1D[0]
			armSquashSpeed_multDiv.outputX >> armSquashDenominator_plsMns.input1D[1]


			armSquashNumerator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquashNumerator_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armNormStretch_cond.outColorR >> armSquashNumerator_multDiv.input1X

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftArmMaxSquashAmt >> armSquashNumerator_multDiv.input2X
			else:
				self.rigSettings_ctrl.rightArmMaxSquashAmt >> armSquashNumerator_multDiv.input2X


			armSquash_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquash_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			armSquashNumerator_multDiv.outputX >> armSquash_multDiv.input1X
			armSquashDenominator_plsMns.output1D >> armSquash_multDiv.input2X


			armScale_cond = DPTnode.createNodeWithParam(node_type='condition', name=self.sidePrefix+'armScale_cond', param={'operation':3,'secondTerm':1} )


			armStretch_multDiv.outputX >> armScale_cond.firstTerm
			armStretchNegator_multDiv.outputX >> armScale_cond.colorIfTrueR
			armSquash_multDiv.outputX >> armScale_cond.colorIfFalseR



			armManualSquashStretchNormalize_1_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armManualSquashStretchNormalize_1_plsMns', param={'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )

			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dx
			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dy
			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dz


			armManualSquashStretchRotAmt_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armManualSquashStretchRotAmt_1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfUpArm_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1X
				self.rigSettings_ctrl.lfUpArmSplit_1_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1Y
				self.rigSettings_ctrl.lfUpArmSplit_2_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1Z
			else:
				self.rigSettings_ctrl.rtUpArm_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1X
				self.rigSettings_ctrl.rtUpArmSplit_1_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1Y
				self.rigSettings_ctrl.rtUpArmSplit_2_SsScale >> armManualSquashStretchRotAmt_1_multDiv.input1Z

			armManualSquashStretchNormalize_1_plsMns.output3Dx >> armManualSquashStretchRotAmt_1_multDiv.input2X
			armManualSquashStretchNormalize_1_plsMns.output3Dy >> armManualSquashStretchRotAmt_1_multDiv.input2Y
			armManualSquashStretchNormalize_1_plsMns.output3Dz >> armManualSquashStretchRotAmt_1_multDiv.input2Z


			armSquashStretch_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquashStretch_1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfUpArm_SsScale >> armSquashStretch_1_multDiv.input1X
				self.rigSettings_ctrl.lfUpArmSplit_1_SsScale >> armSquashStretch_1_multDiv.input1Y
				self.rigSettings_ctrl.lfUpArmSplit_2_SsScale >> armSquashStretch_1_multDiv.input1Z
			else:
				self.rigSettings_ctrl.rtUpArm_SsScale >> armSquashStretch_1_multDiv.input1X
				self.rigSettings_ctrl.rtUpArmSplit_1_SsScale >> armSquashStretch_1_multDiv.input1Y
				self.rigSettings_ctrl.rtUpArmSplit_2_SsScale >> armSquashStretch_1_multDiv.input1Z


			armScale_cond.outColorR >> armSquashStretch_1_multDiv.input2X
			armScale_cond.outColorR >> armSquashStretch_1_multDiv.input2Y
			armScale_cond.outColorR >> armSquashStretch_1_multDiv.input2Z


			armSquashStretchAutoEnable1_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'armSquashStretchAutoEnable1_blend', param={} )

			self.hand_ctrl.autoSquashStretch >> armSquashStretchAutoEnable1_blend.blender
			armSquashStretch_1_multDiv.outputX >> armSquashStretchAutoEnable1_blend.color1R
			armSquashStretch_1_multDiv.outputY >> armSquashStretchAutoEnable1_blend.color1G
			armSquashStretch_1_multDiv.outputZ >> armSquashStretchAutoEnable1_blend.color1B
			armManualSquashStretchRotAmt_1_multDiv.outputX >> armSquashStretchAutoEnable1_blend.color2R
			armManualSquashStretchRotAmt_1_multDiv.outputY >> armSquashStretchAutoEnable1_blend.color2G
			armManualSquashStretchRotAmt_1_multDiv.outputZ >> armSquashStretchAutoEnable1_blend.color2B



			armSquashStretchFinalVal_1_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armSquashStretchFinalVal_1_plsMns', param={'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )

			armSquashStretchAutoEnable1_blend.outputR >> armSquashStretchFinalVal_1_plsMns.input3D[1].input3Dx
			armSquashStretchAutoEnable1_blend.outputG >> armSquashStretchFinalVal_1_plsMns.input3D[1].input3Dy
			armSquashStretchAutoEnable1_blend.outputB >> armSquashStretchFinalVal_1_plsMns.input3D[1].input3Dz

			##########################OUT##########################
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dx', 'up_arm'+self.sidePrefix+'_weight_jnt1.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dx', 'up_arm'+self.sidePrefix+'_weight_jnt1.scaleZ' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dy', 'up_arm'+self.sidePrefix+'_weight_jnt2.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dy', 'up_arm'+self.sidePrefix+'_weight_jnt2.scaleZ' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dz', 'up_arm'+self.sidePrefix+'_weight_jnt3.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns+'.output3D.output3Dz', 'up_arm'+self.sidePrefix+'_weight_jnt3.scaleZ' )




			armManualSquashStretchNormalize_1_plsMns1 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armManualSquashStretchNormalize_1_plsMns1', param={'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )

			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dx
			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dy
			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dz



			armManualSquashStretchRotAmt_1_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armManualSquashStretchRotAmt_1_multDiv1', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfElbow_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1X
				self.rigSettings_ctrl.lfForeArmSplit_1_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1Y
				self.rigSettings_ctrl.lfForeArmSplit_2_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1Z
			else:
				self.rigSettings_ctrl.rtElbow_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1X
				self.rigSettings_ctrl.rtForeArmSplit_1_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1Y
				self.rigSettings_ctrl.rtForeArmSplit_2_SsScale >> armManualSquashStretchRotAmt_1_multDiv1.input1Z

			armManualSquashStretchNormalize_1_plsMns1.output3Dx >> armManualSquashStretchRotAmt_1_multDiv1.input2X
			armManualSquashStretchNormalize_1_plsMns1.output3Dy >> armManualSquashStretchRotAmt_1_multDiv1.input2Y
			armManualSquashStretchNormalize_1_plsMns1.output3Dz >> armManualSquashStretchRotAmt_1_multDiv1.input2Z


			armSquashStretch_1_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquashStretch_1_multDiv1', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfElbow_SsScale >> armSquashStretch_1_multDiv1.input1X
				self.rigSettings_ctrl.lfForeArmSplit_1_SsScale >> armSquashStretch_1_multDiv1.input1Y
				self.rigSettings_ctrl.lfForeArmSplit_2_SsScale >> armSquashStretch_1_multDiv1.input1Z
			else:
				self.rigSettings_ctrl.rtElbow_SsScale >> armSquashStretch_1_multDiv1.input1X
				self.rigSettings_ctrl.rtForeArmSplit_1_SsScale >> armSquashStretch_1_multDiv1.input1Y
				self.rigSettings_ctrl.rtForeArmSplit_2_SsScale >> armSquashStretch_1_multDiv1.input1Z


			armScale_cond.outColorR >> armSquashStretch_1_multDiv1.input2X
			armScale_cond.outColorR >> armSquashStretch_1_multDiv1.input2Y
			armScale_cond.outColorR >> armSquashStretch_1_multDiv1.input2Z



			armSquashStretchAutoEnable1_blend1 = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'armSquashStretchAutoEnable1_blend1', param={} )

			self.hand_ctrl.autoSquashStretch >> armSquashStretchAutoEnable1_blend1.blender
			armSquashStretch_1_multDiv1.outputX >> armSquashStretchAutoEnable1_blend1.color1R
			armSquashStretch_1_multDiv1.outputY >> armSquashStretchAutoEnable1_blend1.color1G
			armSquashStretch_1_multDiv1.outputZ >> armSquashStretchAutoEnable1_blend1.color1B
			armManualSquashStretchRotAmt_1_multDiv1.outputX >> armSquashStretchAutoEnable1_blend1.color2R
			armManualSquashStretchRotAmt_1_multDiv1.outputY >> armSquashStretchAutoEnable1_blend1.color2G
			armManualSquashStretchRotAmt_1_multDiv1.outputZ >> armSquashStretchAutoEnable1_blend1.color2B


			armSquashStretchFinalVal_1_plsMns1 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armSquashStretchFinalVal_1_plsMns1', param={'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )

			armSquashStretchAutoEnable1_blend1.outputR >> armSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dx
			armSquashStretchAutoEnable1_blend1.outputG >> armSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dy
			armSquashStretchAutoEnable1_blend1.outputB >> armSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dz


			##########################OUT##########################
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dx', 'elbow'+self.sidePrefix+'_weight_jnt1.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dx', 'elbow'+self.sidePrefix+'_weight_jnt1.scaleZ' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dy', 'elbow'+self.sidePrefix+'_weight_jnt2.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dy', 'elbow'+self.sidePrefix+'_weight_jnt2.scaleZ' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dz', 'elbow'+self.sidePrefix+'_weight_jnt3.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dz', 'elbow'+self.sidePrefix+'_weight_jnt3.scaleZ' )





			armManualSquashStretchNormalize_1_plsMns2 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armManualSquashStretchNormalize_1_plsMns2', param={'operation':2,'input3D[0].input3Dx':1,'input3D[1].input3Dx':1} )

			self.hand_ctrl.squashStretch >> armManualSquashStretchNormalize_1_plsMns2.input3D[0].input3Dx


			armManualSquashStretchRotAmt_1_multDiv2 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armManualSquashStretchRotAmt_1_multDiv2', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfWrist_SsScale >> armManualSquashStretchRotAmt_1_multDiv2.input1X
			else:
				self.rigSettings_ctrl.rtWrist_SsScale >> armManualSquashStretchRotAmt_1_multDiv2.input1X


			armManualSquashStretchNormalize_1_plsMns2.output3Dx >> armManualSquashStretchRotAmt_1_multDiv2.input2X



			armSquashStretch_1_multDiv2 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armSquashStretch_1_multDiv2', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.lfWrist_SsScale >> armSquashStretch_1_multDiv2.input1X
			else:
				self.rigSettings_ctrl.rtWrist_SsScale >> armSquashStretch_1_multDiv2.input1X


			armScale_cond.outColorR >> armSquashStretch_1_multDiv2.input2X


			armSquashStretchAutoEnable1_blend2 = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'armSquashStretchAutoEnable1_blend2', param={} )

			self.hand_ctrl.autoSquashStretch >> armSquashStretchAutoEnable1_blend2.blender
			armSquashStretch_1_multDiv2.outputX >> armSquashStretchAutoEnable1_blend2.color1R
			armManualSquashStretchRotAmt_1_multDiv2.outputX >> armSquashStretchAutoEnable1_blend2.color2R


			armSquashStretchFinalVal_1_plsMns2 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'armSquashStretchFinalVal_1_plsMns2', param={'operation':1,'input3D[0].input3Dx':1,'input3D[1].input3Dx':1} )

			armSquashStretchAutoEnable1_blend2.outputR >> armSquashStretchFinalVal_1_plsMns2.input3D[1].input3Dx

			##########################OUT##########################
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns2+'.output3D.output3Dx', 'wrist'+self.sidePrefix+'_weight_jnt1.scaleY' )
			pm.connectAttr( armSquashStretchFinalVal_1_plsMns2+'.output3D.output3Dx', 'wrist'+self.sidePrefix+'_weight_jnt1.scaleZ' )



			####################################forearm Twist Connection####################################

			forearmTwistReverser_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'forearmTwistReverser_plsMns', param={'operation':2,'input1D[0]':0} )
			forearmAimPvTwist_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'forearmAimPvTwist_blend', param={} )

			self.forearmPvTwistReader_loc.rotateX >> forearmTwistReverser_plsMns.input1D[1]


			self.hand_ctrl.useAimTwist >> forearmAimPvTwist_blend.blender
			self.forearmTwist_loc.rotateX >> forearmAimPvTwist_blend.color1R
			self.forearmPvTwistReader_loc.readerRot >> forearmAimPvTwist_blend.color2R

			forearmTwistReverser_plsMns.output1D >> self.forearmPvTwistReader_loc.readerRot


			forearmManualRotCtrl_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'forearmManualRotCtrl_blend', param={} )

			self.hand_ctrl.forearmTwistCtrl >> forearmManualRotCtrl_blend.blender
			self.forearmTwist_ctrl.rotateX >> forearmManualRotCtrl_blend.color1R
			forearmAimPvTwist_blend.outputR >> forearmManualRotCtrl_blend.color2R


			forearmSplitRx1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'forearmSplitRx1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			forearmManualRotCtrl_blend.outputR >> forearmSplitRx1_multDiv.input1X
			forearmManualRotCtrl_blend.outputR >> forearmSplitRx1_multDiv.input1Y
			forearmManualRotCtrl_blend.outputR >> forearmSplitRx1_multDiv.input1Z


			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftForearm_1 >> forearmSplitRx1_multDiv.input2X
				self.rigSettings_ctrl.leftForearm_2 >> forearmSplitRx1_multDiv.input2Y
				self.rigSettings_ctrl.leftWrist >> forearmSplitRx1_multDiv.input2Z
			else:
				self.rigSettings_ctrl.rightForearm_1 >> forearmSplitRx1_multDiv.input2X
				self.rigSettings_ctrl.rightForearm_2 >> forearmSplitRx1_multDiv.input2Y
				self.rigSettings_ctrl.rightWrist >> forearmSplitRx1_multDiv.input2Z


			##########################OUT##########################
			forearmSplitRx1_multDiv.outputX >> self.elbowCtrlSplit_1_jnt.rotateX
			forearmSplitRx1_multDiv.outputY >> self.elbowCtrlSplit_2_jnt.rotateX
			forearmSplitRx1_multDiv.outputZ >> self.wristCtrl_jnt.rotateX


			####################################upArm Twist Connection####################################
			upArmMidJntTwistReverser_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'upArmMidJntTwistReverser_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			self.upArmMidJntRzReader_loc.rotateX >> upArmMidJntTwistReverser_multDiv.input1X


			upArmTwistScalar_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'upArmTwistScalar_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


			self.upArmTwistReader_loc.rotateX >> upArmTwistScalar_multDiv.input1X
			upArmMidJntTwistReverser_multDiv.outputX >> upArmTwistScalar_multDiv.input1Y
			self.hand_ctrl.upLimbTwistScalar >> upArmTwistScalar_multDiv.input2X
			self.hand_ctrl.midJntUpLimbTwistScalar >> upArmTwistScalar_multDiv.input2Y


			upArmManualTwist_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'upArmManualTwist_plsMns', param={'operation':1} )


			upArmTwistScalar_multDiv.outputX >> upArmManualTwist_plsMns.input1D[0]
			self.hand_ctrl.upLimbTwist >> upArmManualTwist_plsMns.input1D[1]


			upArmManualTwist_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'upArmManualTwist_blend', param={} )

			self.hand_ctrl.upLimbAutoTwist >> upArmManualTwist_blend.blender
			upArmManualTwist_plsMns.output1D >> upArmManualTwist_blend.color1R
			self.hand_ctrl.upLimbTwist >> upArmManualTwist_blend.color2R


			upArmMidJntTwistReverser_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=self.sidePrefix+'upArmMidJntTwistReverser_plsMns', param={'operation':1} )

			upArmTwistScalar_multDiv.outputY >> upArmMidJntTwistReverser_plsMns.input1D[0]
			upArmManualTwist_blend.outputR >> upArmMidJntTwistReverser_plsMns.input1D[1]



			upArmSplitRx1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'upArmSplitRx1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


			upArmMidJntTwistReverser_plsMns.output1D >> upArmSplitRx1_multDiv.input1X
			upArmMidJntTwistReverser_plsMns.output1D >> upArmSplitRx1_multDiv.input1Y
			upArmMidJntTwistReverser_plsMns.output1D >> upArmSplitRx1_multDiv.input1Z

			if DPTStr.isContain(self.sidePrefix, 'L'):
				self.rigSettings_ctrl.leftUpArm >> upArmSplitRx1_multDiv.input2X
				self.rigSettings_ctrl.leftUpArm_2 >> upArmSplitRx1_multDiv.input2Y
				self.rigSettings_ctrl.leftUpArm_3 >> upArmSplitRx1_multDiv.input2Z
			else:
				self.rigSettings_ctrl.rightUpArm >> upArmSplitRx1_multDiv.input2X
				self.rigSettings_ctrl.rightUpArm_2 >> upArmSplitRx1_multDiv.input2Y
				self.rigSettings_ctrl.rightUpArm_3 >> upArmSplitRx1_multDiv.input2Z



			##########################OUT##########################
			upArmSplitRx1_multDiv.outputY >> self.upArmCtrl_jnt.rotateX
			upArmSplitRx1_multDiv.outputY >> self.upArmCtrlSplit_1_jnt.rotateX
			upArmSplitRx1_multDiv.outputZ >> self.upArmCtrlSplit_2_jnt.rotateX


			armRhJntRx0_reverse_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'armRhJntRx0_reverse_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )

			upArmManualTwist_blend.outputR >> armRhJntRx0_reverse_multDiv.input1X
			upArmManualTwist_blend.outputR >> armRhJntRx0_reverse_multDiv.input1Y
			upArmManualTwist_blend.outputR >> armRhJntRx0_reverse_multDiv.input1Z


			##########################OUT##########################
			armRhJntRx0_reverse_multDiv.outputX >> self.upArmCtrl_jnt_zeroFrzGrp.rotateX
			armRhJntRx0_reverse_multDiv.outputY >> self.upArmCtrlSplit_1_jnt_zeroFrzGrp.rotateX
			armRhJntRx0_reverse_multDiv.outputZ >> self.upArmCtrlSplit_2_jnt_zeroFrzGrp.rotateX




			####################################RubberHose Connection####################################

			upArmSplit_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'upArmSplit_1_multDiv', param={'operation':1,'input1X':0.5,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			pm.connectAttr(self.elbow+'.translateX', upArmSplit_1_multDiv+'.input2X' )
			##########################OUT##########################
			upArmSplit_1_multDiv.outputX >> self.upArmRubberHoseCtrl_jnt_frzGrp.translateX


			forearmSplit_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'forearmSplit_1_multDiv', param={'operation':1,'input1X':0.5,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

			pm.connectAttr(self.wrist+'.translateX', forearmSplit_1_multDiv+'.input2X' )
			##########################OUT##########################
			forearmSplit_1_multDiv.outputX >> self.forearmRubberHoseCtrl_jnt_frzGrp.translateX



			elbowSoftness_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=self.sidePrefix+'elbowSoftness_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0} )

			elbowSoftness_multDiv.setAttr('input2X', upArmDistVal*-0.5)
			elbowSoftness_multDiv.setAttr('input2Y', upArmDistVal*0.5)


			self.hand_ctrl.elbowSoftness >> elbowSoftness_multDiv.input1X
			self.hand_ctrl.elbowSoftness >> elbowSoftness_multDiv.input1Y

			##########################OUT##########################
			elbowSoftness_multDiv.outputX >> self.armUpMidSoftJnt_jnt_zeroFrzGrp.translateX
			elbowSoftness_multDiv.outputY >> self.armDnMidSoftJnt_jnt_zeroFrzGrp.translateX



			####################################Base Joint Connection####################################

			##########################clavicle##########################
			pm.parentConstraint(self.clavicle_ctrl, self.clavicle, maintainOffset=True)

			##########################upArm##########################

			#translate
			pm.pointConstraint( self.shoulder_ctrl , self.upArm )

			#orient
			baseOriCons = pm.orientConstraint( self.FKUpArmJnt, self.IKUpArmJnt, self.upArm )

			fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
			ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

			armIkFkSwitch_reverse = DPTnode.createNodeWithParam(node_type='reverse', name=self.sidePrefix+'armIkFkSwitch_reverse', param={} )
			self.hand_ctrl.ikFkBlend >> armIkFkSwitch_reverse.inputX

			pm.connectAttr( armIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
			pm.connectAttr( self.hand_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )


			if DPTStr.isContain(self.sidePrefix, 'L'):
				pm.connectAttr(self.rigSettings_ctrl.lfUpArmReaderRotOrder, self.upArm+'.rotateOrder')
			else:
				pm.connectAttr(self.rigSettings_ctrl.rtUpArmReaderRotOrder, self.upArm+'.rotateOrder')




			##########################elbow##########################

			armTrans_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=self.sidePrefix+'armTrans_blend', param={} )

			self.hand_ctrl.ikFkBlend >> armTrans_blend.blender
			self.IKElbowJnt.translateX >> armTrans_blend.color1R
			self.IKWristJnt.translateX >> armTrans_blend.color1G
			self.FKElbowJnt.translateX >> armTrans_blend.color2R
			self.FKWristJnt.translateX >> armTrans_blend.color2G

			#translate
			pm.connectAttr( armTrans_blend+'.outputR', self.elbow+'.translateX' )

			#rotate
			baseOriCons = pm.orientConstraint( self.FKElbowJnt, self.IKElbowJnt, self.elbow )
			fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
			ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

			pm.connectAttr( armIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
			pm.connectAttr( self.hand_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )


			if DPTStr.isContain(self.sidePrefix, 'L'):
				pm.connectAttr(self.rigSettings_ctrl.lfUpArmReaderRotOrder, self.elbow+'.rotateOrder')
			else:
				pm.connectAttr(self.rigSettings_ctrl.rtUpArmReaderRotOrder, self.elbow+'.rotateOrder')



			##########################wrist##########################

			#translate
			pm.connectAttr( armTrans_blend+'.outputG', self.wrist+'.translateX' )

			#rotate
			baseOriCons = pm.orientConstraint( self.FKWristJnt, self.IKWristJnt, self.wrist )
			fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
			ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

			pm.connectAttr( armIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
			pm.connectAttr( self.hand_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )



			##########################hand##########################

			#rotate
			pm.orientConstraint( self.hand_ctrl, self.hand )




			##########################weight joint##########################

			pm.parentConstraint(self.upArmCtrl_jnt, 'up_arm'+self.sidePrefix+'_weight_jnt1', maintainOffset=True)
			pm.parentConstraint(self.upArmCtrlSplit_1_jnt, 'up_arm'+self.sidePrefix+'_weight_jnt2', maintainOffset=True)
			pm.parentConstraint(self.upArmCtrlSplit_2_jnt, 'up_arm'+self.sidePrefix+'_weight_jnt3', maintainOffset=True)

			pm.parentConstraint(self.elbowCtrl_jnt, 'elbow'+self.sidePrefix+'_weight_jnt1', maintainOffset=True)
			pm.parentConstraint(self.elbowCtrlSplit_1_jnt, 'elbow'+self.sidePrefix+'_weight_jnt2', maintainOffset=True)
			pm.parentConstraint(self.elbowCtrlSplit_2_jnt, 'elbow'+self.sidePrefix+'_weight_jnt3', maintainOffset=True)
			pm.parentConstraint(self.wristCtrl_jnt, 'wrist'+self.sidePrefix+'_weight_jnt1', maintainOffset=True)


			#update curve length
			scaleAdjustedDefaultArcLen_multDiv.setAttr('input1X', pm.arclen( self.armCurve ) )



		def setup_arm_rig(self):

			#Create Arm space grp
			self.armSpaceCtrl = DPTgrp.createEmpty(self.sidePrefix+'ArmIkCtrl_space_switch_grp',self.rigTop, pm.xform(self.wrist, q=True, ws=True, t=True ))
			DPTattr.connectAttrToList(self.root_ctrl+'.masterScale',[self.armSpaceCtrl+'.scaleX',self.armSpaceCtrl+'.scaleY',self.armSpaceCtrl+'.scaleZ'])


			#create rig top
			self.groupTop = pm.createNode('transform', n=self.sidePrefix+'arm_rig_grp')
			pm.parent(self.groupTop,self.rigTop)



			self.createClavicleCtrl()
			self.createHandControl()
			self.createShoulderCtrl()
			self.createForearmTwistCtrl()

			self.createIKJoint()
			self.createFKJoint()

			#create dist node
			armStrDist, self.armStrDistShape, armStrDist_dist_sp, armStrDist_dist_ep = DPTrigCAct.createRigDistanceSet(start=self.IKUpArmJnt, end=self.arm_ik_ctrl, name=self.sidePrefix+'armStrDist', parent=self.groupTop)
			upArmPvCtrlDist, self.upArmPvCtrlDistShape, upArmPvCtrl_dist_sp, upArmPvCtrl_dist_ep = DPTrigCAct.createRigDistanceSet(start=self.IKUpArmJnt, end=self.elbow_pv_ctrl, name=self.sidePrefix+'upArmPvCtrl_dist', parent=self.groupTop)
			armPvIkCtrlDist, self.armPvIkCtrlDistShape, armPvIkCtrl_dist_sp, armPvIkCtrl_dist_ep = DPTrigCAct.createRigDistanceSet(start=self.elbow_pv_ctrl, end=self.arm_ik_ctrl, name=self.sidePrefix+'armPvIkCtrl_dist', parent=self.groupTop)

			self.createReaders()
			self.createArmRubberHose()
			self.createCrvAndSurf()

			self.createWeightJoints()

			self.createRhUpMidDn()

			self.createConnections()


			#create hand rig set
			self.handRig = DPTHHand.human_hand()

			self.handRig.rigTop = self.rigTop
			self.handRig.sidePrefix=self.sidePrefix
			self.handRig.root_ctrl = self.root_ctrl
			self.handRig.hand_ctrl = self.hand_ctrl
			self.handRig.hand_jnt = self.hand
			self.handRig.rigSettings_ctrl = self.rigSettings_ctrl

			#space attribute
			self.handRig.rootSpaceCtrl = self.rootSpaceCtrl
			self.handRig.worldSpaceCtrl = self.worldSpaceCtrl
			self.handRig.cogSpaceCtrl = self.cogSpaceCtrl
			self.handRig.LowSplineSpaceCtrl = self.LowSplineSpaceCtrl
			self.handRig.HiSplineSpaceCtrl = self.HiSplineSpaceCtrl
			self.handRig.headSpaceCtrl = self.headSpaceCtrl

			self.handRig.setup_hand_rig()

