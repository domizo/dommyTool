import pymel.core as pm
import pymel.util as pmu

import DPyTools.curve.curveTool as DPTCurveTool

import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.stringActions as DPTStr
import DPyTools.general.transformActions as DPTtrans
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.nodeActions as DPTnode

import DPyTools.rigging.jointActions as DPTJoint
import DPyTools.rigging.rigCommonActions as DPTrigCAct


'''
to run:
	>>> import DPyTools.rigging.humanRigActions as DPThAct
	>>> DPThAct.createLegRigSet()

'''


def createHeelCntrl(grpTop=None, sidePrefix='L',
			upLeg='up_leg_jnt', knee='knee_jnt',ankle='ankle_jnt',ball='ball',toe='toe',heelJnt='heel'):

	legIkCtrl_space_grpTop = DPTgrp.createEmpty( sidePrefix+'legIkCtrl_space_grp', grpTop )
	heel_ik_ctrl_frzGrpTop = DPTgrp.createEmpty( sidePrefix+'heel_ik_ctrl_frzGrp', legIkCtrl_space_grpTop, pm.xform(ankle, q=True, ws=True, t=True ) )
	heel_ik_ctrl_zeroFrzGrpTop = DPTgrp.createEmpty( sidePrefix+'heel_ik_ctrl_zeroFrzGrp')


	DPTtrans.setTransformToObjectZero(heel_ik_ctrl_zeroFrzGrpTop, ankle, True, False, False)

	srcVec = pmu.VectorN(pm.xform(ankle, q=True, ws=True, t=True))
	targetVec = pmu.VectorN(pm.xform(toe, q=True, ws=True, t=True))
	srcVec[1]=0
	targetVec[1]=0
	aimVec = targetVec-srcVec
	DPTtrans.setRotateAimAxis(heel_ik_ctrl_zeroFrzGrpTop,(0,0,1), aimVec)


	pm.parent(heel_ik_ctrl_zeroFrzGrpTop, heel_ik_ctrl_frzGrpTop)

	heel_ik_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'heel_ik_ctrl', size=20.0)
	heel_ik_ctrl.translate.set(pm.xform(ball, q=True, ws=True, t=True ))
	pm.parent(heel_ik_ctrl, heel_ik_ctrl_zeroFrzGrpTop)
	heel_ik_ctrl.setAttr('rotateY',0)
	pm.makeIdentity( heel_ik_ctrl, apply=True, translate=True, rotate=True, scale=True )
	pm.runtime.ResetTransformations(heel_ik_ctrl)
	heel_ik_ctrl.scale.lock()

	DPTattr.hideAttributes(heel_ik_ctrl, ['scaleX','scaleY','scaleZ','visibility'])


	DPTattr.createAttributeFromDic(heel_ik_ctrl,
			[{'ln':'legTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'footRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'footBreak','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':-40.0},
			{'ln':'ballRaise','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'toeRaise','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'revHeelRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'revBallRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'toeRoll','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'toeTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'ballTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'heelTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-180.0,'max':180.0,'dv':0.0},
			{'ln':'pvControl','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
			{'ln':'stretchyLeg','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'maxStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':3.0},
			{'ln':'stretchValue','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':3.0},
			{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			])

	heel_ik_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

	return legIkCtrl_space_grpTop, heel_ik_ctrl_frzGrpTop, heel_ik_ctrl_zeroFrzGrpTop, heel_ik_ctrl


def createFootCntrl(grpTop=None, sidePrefix='L',
			upLeg='up_leg_jnt', knee='knee_jnt',ankle='ankle_jnt',ball='ball',toe='toe',heelJnt='heel'):

	foot_ctrl_zeroFrzGrp = DPTgrp.createEmpty( sidePrefix+'foot_ctrl_zeroFrzGrp', grpTop, pm.xform(ball, q=True, ws=True, t=True ) )
	srcVec = pmu.VectorN(pm.xform(ankle, q=True, ws=True, t=True))
	targetVec = pmu.VectorN(pm.xform(toe, q=True, ws=True, t=True))
	srcVec[1]=0
	targetVec[1]=0
	aimVec = targetVec-srcVec
	DPTtrans.setRotateAimAxis(foot_ctrl_zeroFrzGrp,(0,0,1), aimVec)

	footCntrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'foot_ctrl', size=3.0)
	DPTtrans.setTransformToObjectZero(footCntrl, ankle, True, False, False)
	pm.parent(footCntrl,foot_ctrl_zeroFrzGrp)
	footCntrl.setAttr('rotateY',0)
	pm.move(0,0,-5, footCntrl, r=True)
	pm.makeIdentity( footCntrl, apply=True, translate=True, rotate=True, scale=True )
	pm.runtime.ResetTransformations(footCntrl)
	DPTattr.hideAttributes(footCntrl, ['translateX','translateY','translateZ','rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])

	DPTattr.createAttributeFromDic(footCntrl,
			[{'ln':'ikFkBlend','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'ikVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
			{'ln':'fkVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'hipCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
			{'ln':'legStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
			{'ln':'kneeStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
			{'ln':'ankleStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':-10.0,'max':20.0,'dv':0.0},
			{'ln':'upLimbAutoTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'upLimbTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':-360.0,'max':360.0,'dv':0.0},
			{'ln':'upLimbTwistScalar','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'midJntUpLimbTwistScalar','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'useAimTwist','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'lowLegTwistCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'rubberHoseCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'kneeSoftness','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'autoSquashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'squashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':1.0}
			])

	return foot_ctrl_zeroFrzGrp, footCntrl


def createFKControl(grpTop='', partJnt=None, partName='part1',sidePrefix='L',  isSecondary=False):
	part_fk_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+partName+'_fk_ctrl', size=8.0, position=pm.xform(partJnt, q=True, ws=True, t=True ))
	DPTtrans.setTransformToObjectZero(part_fk_ctrl, partJnt, True, True, False)
	part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=part_fk_ctrl, parent=partJnt, position=pm.xform(partJnt, q=True, ws=True, t=True ), isAlign=True)
	pm.parent(part_fk_ctrl_frzGrp, world=True)
	part_fk_ctrl_frzGrp_TOP = pm.duplicate(part_fk_ctrl_frzGrp, n=part_fk_ctrl_frzGrp+'_top', parentOnly=True)
	pm.parent(part_fk_ctrl_frzGrp_TOP, grpTop)
	pm.parent(part_fk_ctrl_frzGrp, part_fk_ctrl_frzGrp_TOP[0])
	part_fk_ctrl.translate.lock()
	part_fk_ctrl.scale.lock()
	DPTattr.hideAttributes(part_fk_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])

	if isSecondary:
		part_sec_fk_ctrl_zeroFrzGrp = pm.duplicate(part_fk_ctrl_zeroFrzGrp, n=sidePrefix+partName+'_sec_fk_ctrl_zeroFrzGrp', parentOnly=True)[0]
		pm.parent(part_sec_fk_ctrl_zeroFrzGrp, part_fk_ctrl)
		part_sec_fk_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+partName+'_sec_fk_ctrl', size=3.0)
		DPTtrans.setTransformToObjectZero(part_sec_fk_ctrl, partJnt, True, True, False)
		pm.parent(part_sec_fk_ctrl, part_sec_fk_ctrl_zeroFrzGrp)
		DPTattr.hideAttributes(part_sec_fk_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])
		part_sec_fk_ctrl.translate.lock()
		part_sec_fk_ctrl.scale.lock()
		return part_fk_ctrl, part_fk_ctrl_frzGrp_TOP, part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp, part_sec_fk_ctrl, part_sec_fk_ctrl_zeroFrzGrp

	return part_fk_ctrl, part_fk_ctrl_frzGrp_TOP, part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp




########################################################################Create Leg Rig########################################################################
def createLegRigSet(grpTop=None,hip='hip_jnt',
				upLeg='up_leg_jnt', knee='knee_jnt',ankle='ankle_jnt',ball='ball',toe='toe',heelJnt='heel',
				sidePrefix='L',
				rootCntrl='',legSpaceCntrl=''):

	#############################Create FK IK joints#############################
	FKGrpTop = DPTgrp.createEmpty(sidePrefix+'legFK_grp',grpTop, position=pm.xform(hip, q=True, ws=True, t=True ))
	IKGrpTop = DPTgrp.createEmpty(sidePrefix+'legIK_grp',grpTop)

	FKJoints = DPTJoint.duplicateSameJointSet(upLeg, FKGrpTop, {'jnt':'fk_jnt'})
	IKJoints = DPTJoint.duplicateSameJointSet(upLeg, IKGrpTop, {'jnt':'ik_jnt'})

	FKUplegJnt = DPTnode.getObjectFromList('up_leg',FKJoints)
	FKKneeJnt = DPTnode.getObjectFromList('knee',FKJoints)
	FKAnkleJnt = DPTnode.getObjectFromList('ankle',FKJoints)
	FKBallJnt = DPTnode.getObjectFromList('ball',FKJoints)



	#############################Create Reverse joints#############################
	#create reverse joint
	revGrpTop = DPTgrp.createEmpty(sidePrefix+'reverseFoot_rig_grp',grpTop)
	heelRevJnt = pm.joint( name=heelJnt+'_rev_rig_jnt', p=pm.xform(heelJnt, q=True, ws=True, t=True ))

	toeRevJnt = pm.duplicate(toe, n=DPTStr.replaceText(toe,{'jnt':'rev_rig_jnt'}),parentOnly=True)[0]
	ballRevJnt = pm.duplicate(ball, n=DPTStr.replaceText(ball,{'jnt':'rev_rig_jnt'}),parentOnly=True)[0]
	ankleRevJnt = pm.duplicate(ankle, n=DPTStr.replaceText(ankle,{'jnt':'rev_rig_jnt'}),parentOnly=True)[0]

	pm.parent(ankleRevJnt,ballRevJnt)
	pm.parent(ballRevJnt,toeRevJnt)
	pm.parent(toeRevJnt,heelRevJnt)

	ballToeIkTopJnt = pm.duplicate(ball, n=sidePrefix+'ballToePar_ik_jnt',parentOnly=True)[0]
	ballIKJnt = pm.duplicate(ball, n=DPTStr.replaceText(ball,{'jnt':'ik_jnt'}),parentOnly=True)[0]
	toeIkJnt = pm.duplicate(toe, n=DPTStr.replaceText(toe,{'jnt':'ik_jnt'}),parentOnly=True)[0]

	pm.parent(ballIKJnt,ballToeIkTopJnt)
	pm.parent(toeIkJnt,ballIKJnt)

	pm.parent(ballToeIkTopJnt,toeRevJnt)
	pm.parent(heelRevJnt,revGrpTop)

	#correct axis for heel
	if sidePrefix == 'L':
		DPTJoint.crossCorrectJointAxis( [heelRevJnt,toeIkJnt,ankle], aimAxis=(1,0,0), upAxis=(0,0,1), reverse=False)
		DPTJoint.crossCorrectJointAxis( [heelRevJnt,toeRevJnt,ankle], aimAxis=(1,0,0), upAxis=(0,0,1), reverse=False)
	else :
		DPTJoint.crossCorrectJointAxis( [heelRevJnt,toeIkJnt,ankle], aimAxis=(-1,0,0), upAxis=(0,0,-1), reverse=True)
		DPTJoint.crossCorrectJointAxis( [heelRevJnt,toeRevJnt,ankle], aimAxis=(-1,0,0), upAxis=(0,0,-1), reverse=True)


	#############################Create Controller#############################

	#create foot controller
	foot_ctrl_zeroFrzGrp, foot_ctrl = createFootCntrl(grpTop, sidePrefix, upLeg, knee, ankle, ball, toe, heelJnt )
	pm.parentConstraint( ankle, foot_ctrl, maintainOffset=True )
	foot_ctrl.translate.lock()
	foot_ctrl.rotate.lock()
	foot_ctrl.scale.lock()


	#create f_hip control
	foot_hip_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'f_hip_ctrl', size=2.0, position=pm.xform(upLeg, q=True, ws=True, t=True ))
	foot_hip_ctrl_frzGrp, foot_hip_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=foot_hip_ctrl, parent=grpTop, position=pm.xform(upLeg, q=True, ws=True, t=True ))
	foot_hip_ctrl.rotate.lock()
	foot_hip_ctrl.scale.lock()
	DPTattr.hideAttributes(foot_hip_ctrl, ['rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])
	pm.parentConstraint( hip, foot_hip_ctrl_frzGrp, maintainOffset=True )

	foot_ctrl.hipCtrlVis >> foot_hip_ctrl.visibility

	#create lowLegTwist_ctrl
	srcTwistVec = pmu.VectorN(pm.xform(knee, q=True, ws=True, t=True))
	targetTwistVec = pmu.VectorN(pm.xform(ankle, q=True, ws=True, t=True))
	twistLocVec = srcTwistVec+(targetTwistVec-srcTwistVec)*0.5
	lowLegTwist_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'lowLegTwist_ctrl', size=5.0, position=twistLocVec)
	lowLegTwist_ctrl_frzGrp, lowLegTwist_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=lowLegTwist_ctrl, parent=grpTop, position=twistLocVec)
	DPTtrans.setTransformToObjectZero( srcObj=lowLegTwist_ctrl_frzGrp, targetObj=knee, isTrans=False, isRot=True)
	pm.parent(lowLegTwist_ctrl_frzGrp, grpTop)
	lowLegTwist_ctrl_zeroFrzGrp.rotate.set(0,0,0)
	lowLegTwist_ctrl.rotate.set(0,0,0)

	pm.parentConstraint( knee, lowLegTwist_ctrl_frzGrp, maintainOffset=True )

	lowLegManualRotCtrl_cond = DPTnode.createNodeWithParam(node_type='condition', name=sidePrefix+'lowLegManualRotCtrl_cond', param={'operation':2,'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																												'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':0} )

	foot_ctrl.lowLegTwistCtrl >> lowLegManualRotCtrl_cond.firstTerm
	lowLegManualRotCtrl_cond.outColorR >> lowLegTwist_ctrl.visibility

	lowLegTwist_ctrl.translate.lock()
	lowLegTwist_ctrl.rotateY.lock()
	lowLegTwist_ctrl.rotateZ.lock()
	lowLegTwist_ctrl.scale.lock()



	#create  upLeg_upJntFkCtrlAlign_grp
	upLeg_upJntFkCtrlAlign_grp = DPTgrp.createEmpty( sidePrefix+'upLeg_upJntFkCtrlAlign_grp', grpTop, pm.xform(upLeg, q=True, ws=True, t=True ) )
	DPTtrans.setTransformToObjectZero( srcObj=upLeg_upJntFkCtrlAlign_grp, targetObj=upLeg, isTrans=True, isRot=True)
	pm.parent(upLeg_upJntFkCtrlAlign_grp, grpTop)

	pm.parentConstraint( hip, upLeg_upJntFkCtrlAlign_grp, maintainOffset=True )


	#create upLeg_fk_ctrl
	upLeg_fk_ctrl, upLeg_fk_ctrl_frzGrp_TOP, upLeg_fk_ctrl_frzGrp, upLeg_fk_ctrl_zeroFrzGrp,upLeg_sec_fk_ctrl, upLeg_sec_fk_ctrl_zeroFrzGrp = createFKControl(grpTop=grpTop,
																					partJnt=upLeg,
																					partName='upLeg' ,sidePrefix=sidePrefix,
																					isSecondary=True)
	'''
	upLeg_fk_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'upLeg_fk_ctrl', size=4.0, position=pm.xform(upLeg, q=True, ws=True, t=True ))
	DPTtrans.setTransformToObjectZero(upLeg_fk_ctrl, upLeg, True, True, False)
	upLeg_fk_ctrl_frzGrp, upLeg_fk_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=upLeg_fk_ctrl, parent=upLeg, position=pm.xform(upLeg, q=True, ws=True, t=True ), isAlign=True)
	pm.parent(upLeg_fk_ctrl_frzGrp, world=True)
	upLeg_fk_ctrl_frzGrp_TOP = pm.duplicate(upLeg_fk_ctrl_frzGrp, n=upLeg_fk_ctrl_frzGrp+'_top', parentOnly=True)
	pm.parent(upLeg_fk_ctrl_frzGrp_TOP, grpTop)
	pm.parent(upLeg_fk_ctrl_frzGrp, upLeg_fk_ctrl_frzGrp_TOP)
	upLeg_fk_ctrl.translate.lock()
	upLeg_fk_ctrl.scale.lock()
	DPTattr.hideAttributes(upLeg_fk_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])
	'''
	DPTattr.createAttributeFromDic(upLeg_fk_ctrl,
			[{'ln':'align','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'localAlign','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'secondaryFkCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False}
			])

	'''
	upLeg_sec_fk_ctrl_zeroFrzGrp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_sec_fk_ctrl_zeroFrzGrp', parentOnly=True)[0]
	pm.parent(upLeg_sec_fk_ctrl_zeroFrzGrp, upLeg_fk_ctrl)
	upLeg_sec_fk_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'upLeg_sec_fk_ctrl', size=3.0)
	DPTtrans.setTransformToObjectZero(upLeg_sec_fk_ctrl, upLeg, True, True, False)
	pm.parent(upLeg_sec_fk_ctrl, upLeg_sec_fk_ctrl_zeroFrzGrp)
	upLeg_sec_fk_ctrl.translate.lock()
	upLeg_sec_fk_ctrl.scale.lock()
	'''

	legFkVis_condNode = DPTnode.createNodeWithParam(node_type='condition', name=sidePrefix+'legFkVis_cond', param={'operation':4,'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																												'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':1} )
	upLeg_fk_ctrl.secondaryFkCtrlVis >> upLeg_sec_fk_ctrl.visibility



	#alignTrue and alignFalse
	upLeg_alignTrue_grp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_alignTrue_grp', parentOnly=True)[0]
	upLeg_alignFalse_grp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_alignFalse_grp', parentOnly=True)[0]
	upLeg_alignFalse_grp_frzGrp = pm.duplicate(upLeg_alignFalse_grp, n=sidePrefix+'upLeg_alignFalse_grp_frzGrp', parentOnly=True)[0]
	pm.parent(upLeg_alignTrue_grp, upLeg_fk_ctrl_frzGrp_TOP[0])
	pm.parent(upLeg_alignFalse_grp_frzGrp, upLeg_fk_ctrl_frzGrp_TOP[0])
	pm.parent(upLeg_alignFalse_grp, upLeg_alignFalse_grp_frzGrp)

	orientCons = pm.orientConstraint( upLeg_alignTrue_grp, upLeg_alignFalse_grp, upLeg_fk_ctrl_frzGrp )
	pm.parentConstraint( upLeg_upJntFkCtrlAlign_grp, upLeg_alignFalse_grp_frzGrp, maintainOffset=True )


	alignTrueAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrue', kwargs={'ud':True})
	alignFalseAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignFalse', kwargs={'ud':True})

	pm.connectAttr( upLeg_fk_ctrl+'.align', orientCons+'.'+alignTrueAttr )
	upLeg_plsMnsNode = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'upLeg_plsMns', param={'operation':2,'input1D[0]':1} )
	upLeg_fk_ctrl.align >> upLeg_plsMnsNode.input1D[1]
	pm.connectAttr( upLeg_plsMnsNode+'.output1D', orientCons+'.'+alignFalseAttr )



	#upLeg alignTrueLocTarg
	upLeg_alignTrueLocTarg_grp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_alignTrueLocTarg_grp', parentOnly=True)[0]
	upLeg_alignTrueLocTarg_grp_frzGrp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_alignTrueLocTarg_grp_frzGrp', parentOnly=True)[0]
	upLeg_alignTrueLocTarg_grp_frzGrp_TOP = DPTgrp.createEmpty(sidePrefix+'upLeg_alignTrueLocTarg_grp_frzGrp_top',upLeg_fk_ctrl_frzGrp_TOP, position=pm.xform('spline_low_jnt', q=True, ws=True, t=True ))
	pm.parent(upLeg_alignTrueLocTarg_grp_frzGrp,upLeg_alignTrueLocTarg_grp_frzGrp_TOP)
	pm.parent(upLeg_alignTrueLocTarg_grp,upLeg_alignTrueLocTarg_grp_frzGrp)

	pm.parentConstraint( 'altCog_ctrl', upLeg_alignTrueLocTarg_grp_frzGrp_TOP, maintainOffset=True )
	pm.pointConstraint( FKJoints[0], upLeg_alignTrueLocTarg_grp_frzGrp )

	#upLeg autoAlign
	upLeg_autoAlign_rig_grp = DPTgrp.createEmpty(sidePrefix+'upLeg_autoAlign_rig_grp',parent=None, position=pm.xform(upLeg, q=True, ws=True, t=True ))
	upLeg_autoAlign_rig_grp_frzGrp = pm.duplicate(upLeg_autoAlign_rig_grp, n=sidePrefix+'upLeg_autoAlign_rig_grp_frzGrp', parentOnly=True)[0]
	upLeg_alignTrueWrldTarg_grp = pm.duplicate(upLeg_fk_ctrl_zeroFrzGrp, n=sidePrefix+'upLeg_alignTrueWrldTarg_grp', parentOnly=True)[0]
	pm.parent(upLeg_autoAlign_rig_grp, upLeg_autoAlign_rig_grp_frzGrp)
	pm.parent(upLeg_alignTrueWrldTarg_grp, upLeg_autoAlign_rig_grp)
	pm.parent(upLeg_autoAlign_rig_grp_frzGrp, upLeg_fk_ctrl_frzGrp_TOP[0])

	root_ctrl = pm.ls('root_ctrl')[0]
	root_ctrl.rotate >> upLeg_autoAlign_rig_grp.rotate


	orientCons = pm.orientConstraint( upLeg_alignTrueLocTarg_grp, upLeg_alignTrueWrldTarg_grp, upLeg_alignTrue_grp )

	alignTrueLocAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueLoc', kwargs={'ud':True})
	alignTrueWrldAttr = DPTattr.getMatchNameAttribute(node=orientCons, matchStr='alignTrueWrld', kwargs={'ud':True})

	upLegAlignTrue_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'upLegAlignTrue_plsMns', param={'operation':2,'input1D[0]':1} )

	pm.connectAttr( upLeg_fk_ctrl+'.localAlign', orientCons+'.'+alignTrueLocAttr )
	upLeg_fk_ctrl.localAlign >> upLegAlignTrue_plsMns.input1D[1]
	pm.connectAttr( upLegAlignTrue_plsMns+'.output1D', orientCons+'.'+alignTrueWrldAttr )


	#create ball_upJntFkCtrl_grp
	ball_upJntFkCtrl_grp = DPTgrp.createEmpty( sidePrefix+'ball_upJntFkCtrl_grp', grpTop, pm.xform(ball, q=True, ws=True, t=True ) )
	DPTtrans.setTransformToObjectZero( srcObj=ball_upJntFkCtrl_grp, targetObj=ball, isTrans=True, isRot=True)
	pm.parent(ball_upJntFkCtrl_grp, grpTop)


	if FKAnkleJnt is not None:
		pm.parentConstraint( FKAnkleJnt, ball_upJntFkCtrl_grp, maintainOffset=True )

	#create ball_fk_ctrl
	ball_fk_ctrl, ball_fk_ctrl_frzGrp_TOP, ball_fk_ctrl_frzGrp, ball_fk_ctrl_zeroFrzGrp = createFKControl(grpTop=grpTop,
																					partJnt=ball,
																					partName='ball' ,sidePrefix=sidePrefix,
																					isSecondary=False)

	pm.pointConstraint( FKBallJnt, ball_fk_ctrl_frzGrp_TOP )
	pm.orientConstraint( ball_upJntFkCtrl_grp, ball_fk_ctrl_frzGrp )

	#create ankle_upJntFkCtrl_grp
	ankle_upJntFkCtrl_grp = DPTgrp.createEmpty( sidePrefix+'ankle_upJntFkCtrl_grp', grpTop, pm.xform(ankle, q=True, ws=True, t=True ) )
	DPTtrans.setTransformToObjectZero( srcObj=ankle_upJntFkCtrl_grp, targetObj=ankle, isTrans=True, isRot=True)
	pm.parent(ankle_upJntFkCtrl_grp, grpTop)


	if FKKneeJnt is not None:
		pm.parentConstraint( FKKneeJnt, ankle_upJntFkCtrl_grp, maintainOffset=True )

	#create ankle_fk_ctrl
	ankle_fk_ctrl, ankle_fk_ctrl_frzGrp_TOP, ankle_fk_ctrl_frzGrp, ankle_fk_ctrl_zeroFrzGrp, ankle_sec_fk_ctrl, ankle_sec_fk_ctrl_zeroFrzGrp  = createFKControl(grpTop=grpTop,
																					partJnt=ankle,
																					partName='ankle' ,sidePrefix=sidePrefix,
																					isSecondary=True)

	DPTattr.createAttributeFromDic(ankle_fk_ctrl,
			[{'ln':'secondaryFkCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False}])

	ankle_fk_ctrl.secondaryFkCtrlVis >>  ankle_sec_fk_ctrl.visibility

	pm.pointConstraint( FKAnkleJnt, ankle_fk_ctrl_frzGrp_TOP )
	pm.orientConstraint( ankle_upJntFkCtrl_grp, ankle_fk_ctrl_frzGrp )


	#create knee_upJntFkCtrl_grp
	knee_upJntFkCtrl_grp = DPTgrp.createEmpty( sidePrefix+'knee_upJntFkCtrl_grp', grpTop, pm.xform(knee, q=True, ws=True, t=True ) )
	DPTtrans.setTransformToObjectZero( srcObj=knee_upJntFkCtrl_grp, targetObj=knee, isTrans=True, isRot=True)
	pm.parent(knee_upJntFkCtrl_grp, grpTop)


	if FKUplegJnt is not None:
		pm.parentConstraint( FKUplegJnt, knee_upJntFkCtrl_grp, maintainOffset=True )

	#create knee_fk_ctrl_frzGrp
	knee_fk_ctrl, knee_fk_ctrl_frzGrp_TOP, knee_fk_ctrl_frzGrp, knee_fk_ctrl_zeroFrzGrp = createFKControl(grpTop=grpTop,
																					partJnt=knee,
																					partName='knee' ,sidePrefix=sidePrefix,
																					isSecondary=False)

	pm.pointConstraint( FKKneeJnt, knee_fk_ctrl_frzGrp_TOP )
	pm.orientConstraint( knee_upJntFkCtrl_grp, knee_fk_ctrl_frzGrp )
	DPTattr.hideAttributes(knee_fk_ctrl, ['rotateX'])
	knee_fk_ctrl.rotateX.lock()


	####################################Create FK joint constraint and connect####################################
	pm.parentConstraint( hip, FKGrpTop, maintainOffset=True )
	pm.pointConstraint( foot_hip_ctrl, FKJoints[0] )
	pm.orientConstraint( upLeg_sec_fk_ctrl, FKJoints[0]  )

	pm.orientConstraint( knee_fk_ctrl, FKKneeJnt )
	pm.orientConstraint( ankle_sec_fk_ctrl, FKAnkleJnt )
	pm.orientConstraint( ball_fk_ctrl, FKBallJnt )


	pm.pointConstraint( FKJoints[0] , upLeg_fk_ctrl_frzGrp_TOP )

	#FK
	foot_ctrl.fkVis >> FKJoints[0].visibility

	foot_ctrl.ikFkBlend >> legFkVis_condNode.firstTerm
	legFkVis_condNode.outColorR >> upLeg_fk_ctrl.visibility
	legFkVis_condNode.outColorR >> knee_fk_ctrl.visibility
	legFkVis_condNode.outColorR >> ankle_fk_ctrl.visibility
	legFkVis_condNode.outColorR >> ball_fk_ctrl.visibility

	####################################Reverse joint constraint####################################

	#create heel controller
	legIkCtrl_space_grpTop, heel_ik_ctrl_frzGrpTop, heel_ik_ctrl_zeroFrzGrpTop, heel_ik_ctrl = createHeelCntrl(grpTop,sidePrefix,upLeg, knee, ankle, ball, toe, heelJnt )

	pm.parentConstraint( heel_ik_ctrl,legSpaceCntrl, maintainOffset=True)


	legIkVis_condNode = DPTnode.createNodeWithParam(node_type='condition', name=sidePrefix+'legIkVis_cond', param={'operation':2,'colorIfTrueR':1,'colorIfTrueG':0,'colorIfTrueB':0,
																												'colorIfFalseR':0,'colorIfFalseG':1,'colorIfFalseB':1,'secondTerm':0} )
	foot_ctrl.ikFkBlend >> legIkVis_condNode.firstTerm
	legIkVis_condNode.outColorR >> heel_ik_ctrl.visibility


	legIkCtrlCons = pm.parentConstraint('rootCtrl_space_switch_grp', 'worldSpaceCtrl_space_switch_grp', 'cogCtrl_space_switch_grp', legIkCtrl_space_grpTop, maintainOffset=True)

	rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legIkCtrlCons, matchStr='root', kwargs={'ud':True})
	worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legIkCtrlCons, matchStr='world', kwargs={'ud':True})
	cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legIkCtrlCons, matchStr='cog', kwargs={'ud':True})

	pm.connectAttr( heel_ik_ctrl+'.rootCtrl', legIkCtrlCons+'.'+rootCtrlSpaceAttr )
	pm.connectAttr( heel_ik_ctrl+'.worldSpaceCtrl', legIkCtrlCons+'.'+worldCtrlSpaceAttr )
	pm.connectAttr( heel_ik_ctrl+'.cogCtrl', legIkCtrlCons+'.'+cogCtrlSpaceAttr )






	#connect root param

	DPTattr.connectAttrToList(rootCntrl+'.masterScale',[grpTop+'.scaleX',grpTop+'.scaleY',grpTop+'.scaleZ'])

	#rev joints connect and constraint
	pm.parentConstraint( heel_ik_ctrl, revGrpTop, maintainOffset=True )

	#heel rev
	DPTattr.connectAttrToList(heel_ik_ctrl+'.heelTwist',[heelRevJnt+'.rotateY'])

	heelClampNode = DPTnode.createNodeWithParam(node_type='clamp', name=sidePrefix+'heel_clamp', param={'maxR':50.0} )
	heelPlsMnsNode = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'heel_plsMns', param={'operation':2} )

	heel_ik_ctrl.footRoll >> heelClampNode.inputR
	heelClampNode.outputR >> heelPlsMnsNode.input1D[1]
	heelPlsMnsNode.output1D >> heelRevJnt.rotateZ

	DPTattr.connectAttrToList(foot_ctrl+'.ikVis',[heelRevJnt+'.visibility'])

	#toe rev
	heel_ik_ctrl.revHeelRoll >> toeRevJnt.rotateX
	heel_ik_ctrl.toeTwist >> toeRevJnt.rotateY

	toe_plsMnsNode = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'toe_plsMns', param={'operation':2} )
	toe_clampNode = DPTnode.createNodeWithParam(node_type='clamp', name=sidePrefix+'toe_clamp', param={'minR':-90.0} )
	toe2_plsMnsNode = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'toe2_plsMns', param={'operation':1} )

	heel_ik_ctrl.footRoll >> toe_plsMnsNode.input1D[0]
	heel_ik_ctrl.footBreak >> toe_plsMnsNode.input1D[1]
	toe_plsMnsNode.output1D >> toe_clampNode.inputR
	toe_clampNode.outputR >> toe2_plsMnsNode.input1D[0]
	heel_ik_ctrl.toeRaise >> toe2_plsMnsNode.input1D[1]
	toe2_plsMnsNode.output1D >> toeRevJnt.rotateZ

	#ball rev
	heel_ik_ctrl.ballTwist >> ballRevJnt.rotateY
	heel_ik_ctrl.revBallRoll >> ballRevJnt.rotateX

	ball_clampNode = DPTnode.createNodeWithParam(node_type='clamp', name=sidePrefix+'ball_clamp', param={} )
	ball_plsMnsNode = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'ball_plsMns', param={'operation':1} )

	heel_ik_ctrl.footRoll >> ball_clampNode.inputR
	heel_ik_ctrl.footBreak >> ball_clampNode.minR
	ball_clampNode.outputR >> ball_plsMnsNode.input1D[0]
	heel_ik_ctrl.ballRaise >> ball_plsMnsNode.input1D[1]
	ball_plsMnsNode.output1D >> ballRevJnt.rotateZ

	#ball_ik_jnt
	heel_ik_ctrl.ballTwist >> ballIKJnt.rotateY
	heel_ik_ctrl.toeRoll >> ballIKJnt.rotateZ



	####################################Create IK####################################
	pm.parentConstraint( hip, IKGrpTop, maintainOffset=True )

	IKupLegJnt = DPTnode.getObjectFromList('up_leg',IKJoints)
	IKKneeJnt = DPTnode.getObjectFromList('knee',IKJoints)
	IKAnkleJnt = DPTnode.getObjectFromList('ankle',IKJoints)

	anklechilds = DPTJoint.getJointChilds(IKAnkleJnt)
	pm.delete(anklechilds)

	pm.pointConstraint(foot_hip_ctrl, IKupLegJnt)
	foot_ctrl.ikVis >> IKupLegJnt.visibility

	legIKHnd, legIKEffector = pm.ikHandle( sj=IKupLegJnt, ee=IKAnkleJnt, sol='ikRPsolver' )
	pm.parent(legIKHnd, grpTop)
	legIKHnd.hide()

	pm.parentConstraint( ankleRevJnt, legIKHnd, maintainOffset=True )

	#create knee_pv_ctrl
	legPvCtrlGrp_space_grp = DPTgrp.createEmpty( sidePrefix+'legPvCtrlGrp_space_grp', grpTop )

	knee_pv_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'knee_pv_ctrl', size=10)
	pm.parent(knee_pv_ctrl, knee)
	knee_pv_ctrl.translate.set([0,0,0])
	pm.move( 0, 0, 30, knee_pv_ctrl, r=True, os=True, wd=True )
	knee_pv_ctrl_frzGrp, knee_pv_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(baseObject=knee_pv_ctrl, parent=legPvCtrlGrp_space_grp, position=pm.xform(knee_pv_ctrl, q=True, ws=True, t=True ))
	knee_pv_ctrl.scale.lock()
	DPTattr.hideAttributes(knee_pv_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

	legIkVis_condNode.outColorR >> knee_pv_ctrl_zeroFrzGrp.visibility

	DPTattr.createAttributeFromDic(knee_pv_ctrl,
			[{'ln':'snapKnee','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'hipCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'legIkCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0}
			])
	knee_pv_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

	legPvCtrlGrpCons = pm.parentConstraint('rootCtrl_space_switch_grp', 'worldSpaceCtrl_space_switch_grp','cogCtrl_space_switch_grp',
		'hipCtrl_space_switch_grp',legSpaceCntrl,legPvCtrlGrp_space_grp, maintainOffset=True)

	rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legPvCtrlGrpCons, matchStr='root', kwargs={'ud':True})
	worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legPvCtrlGrpCons, matchStr='world', kwargs={'ud':True})
	cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legPvCtrlGrpCons, matchStr='cog', kwargs={'ud':True})
	hipCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legPvCtrlGrpCons, matchStr='hip', kwargs={'ud':True})
	LegIkCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=legPvCtrlGrpCons, matchStr='LegIkCtrl', kwargs={'ud':True})

	pm.connectAttr( knee_pv_ctrl+'.rootCtrl', legPvCtrlGrpCons+'.'+rootCtrlSpaceAttr )
	pm.connectAttr( knee_pv_ctrl+'.worldSpaceCtrl', legPvCtrlGrpCons+'.'+worldCtrlSpaceAttr )
	pm.connectAttr( knee_pv_ctrl+'.cogCtrl', legPvCtrlGrpCons+'.'+cogCtrlSpaceAttr )
	pm.connectAttr( knee_pv_ctrl+'.hipCtrl', legPvCtrlGrpCons+'.'+hipCtrlSpaceAttr )
	pm.connectAttr( knee_pv_ctrl+'.legIkCtrl', legPvCtrlGrpCons+'.'+LegIkCtrlSpaceAttr )

	legIkVis_condNode.outColorR >> knee_pv_ctrl_frzGrp.visibility
	heel_ik_ctrl.pvControl >> knee_pv_ctrl.visibility

	#create pole vector locator
	kneeLineLoc = pm.spaceLocator(n=sidePrefix+'knee_line_loc')
	pos = pm.xform(knee, q=True, ws=True, t=True )
	pm.move( pos[0], pos[1], pos[2], kneeLineLoc, a=True, ws=True, wd=True )
	pm.parent(kneeLineLoc, knee_pv_ctrl_frzGrp)

	heel_ik_ctrl.pvControl >> kneeLineLoc.visibility
	pm.pointConstraint(knee, kneeLineLoc)

	#create pole vector arrow
	arrowShape= pm.createNode( 'annotationShape', n=sidePrefix+'knee_line_arrow' )
	arrowShape.setAttr('displayArrow', 1)
	arrowShape.setAttr('text', '')

	arrowTrans = arrowShape.getParent()
	pm.parent( arrowTrans, kneeLineLoc, relative=True )

	knee_pv_ctrl_annLoc = pm.spaceLocator(n=sidePrefix+'knee_pv_ctrl_annLoc')
	pos = pm.xform(knee_pv_ctrl, q=True, ws=True, t=True )
	pm.move( pos[0], pos[1], pos[2], knee_pv_ctrl_annLoc, a=True, ws=True, wd=True )
	knee_pv_ctrl_annLoc.hide()
	pm.parent(knee_pv_ctrl_annLoc, knee_pv_ctrl)

	pm.pointConstraint(knee_pv_ctrl,knee_pv_ctrl_annLoc)
	knee_pv_ctrl_annLoc.getShape().worldMatrix >> arrowShape.dagObjectMatrix[0]


	#create pole vector constraint
	poleVecCons = pm.poleVectorConstraint( knee_pv_ctrl, legIKHnd )

	kneePvAttr = DPTattr.getMatchNameAttribute(node=poleVecCons, matchStr='knee_pv', kwargs={'ud':True})
	pm.connectAttr( heel_ik_ctrl+'.pvControl', poleVecCons+'.'+kneePvAttr )

	legPvCtrl_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'legPvCtrl_blend', param={'color2R':0,'color2G':0,'color2B':0} )

	heel_ik_ctrl.pvControl >> legPvCtrl_blend.blender
	poleVecCons.constraintTranslateX // legIKHnd.poleVectorX
	poleVecCons.constraintTranslateY // legIKHnd.poleVectorY
	poleVecCons.constraintTranslateZ // legIKHnd.poleVectorZ
	poleVecCons.constraintTranslateX >> legPvCtrl_blend.color1R
	poleVecCons.constraintTranslateY >> legPvCtrl_blend.color1G
	poleVecCons.constraintTranslateZ >> legPvCtrl_blend.color1B
	legPvCtrl_blend.outputR >> legIKHnd.poleVectorX
	legPvCtrl_blend.outputG >> legIKHnd.poleVectorY
	legPvCtrl_blend.outputB >> legIKHnd.poleVectorZ
	heel_ik_ctrl.legTwist >> legIKHnd.twist


	####################################Create Distance Nodes####################################

	legStrDist, legStrDistShape, legStrDist_dist_sp, legStrDist_dist_ep = DPTrigCAct.createRigDistanceSet(start=IKupLegJnt, end=ankleRevJnt, name=sidePrefix+'legStrDist', parent=grpTop)

	upLegPvCtrlDist, upLegPvCtrlDistShape, upLegPvCtrl_dist_sp, upLegPvCtrl_dist_ep = DPTrigCAct.createRigDistanceSet(start=IKupLegJnt, end=knee_pv_ctrl, name=sidePrefix+'upLegPvCtrl', parent=grpTop)

	legPvIkCtrlDist, legPvIkCtrlDistShape, legPvIkCtrl_dist_sp, legPvIkCtrl_dist_ep = DPTrigCAct.createRigDistanceSet(start=knee_pv_ctrl, end=ankleRevJnt, name=sidePrefix+'legPvIkCtrl', parent=grpTop)


	####################################Create UpLeg Reader####################################

	#create upLegRotReaderParent_jnt_grp
	upLegRotReaderParent_jnt_frzGrpTop = DPTgrp.createEmpty( sidePrefix+'upLegRotReaderParent_jnt_frzGrp_top', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=upLegRotReaderParent_jnt_frzGrpTop, targetObj=upLeg, isTrans=True, isRot=True, isScale=False)
	upLegRotReaderParent_jnt_frzGrp = pm.duplicate(upLegRotReaderParent_jnt_frzGrpTop, n=sidePrefix+'upLegRotReaderParent_jnt_frzGrpTop',parentOnly=True)[0]
	pm.parent(upLegRotReaderParent_jnt_frzGrpTop, grpTop)
	pm.parent(upLegRotReaderParent_jnt_frzGrp,upLegRotReaderParent_jnt_frzGrpTop)

	upLegRotReaderParent_jnt = pm.duplicate(upLeg, n=sidePrefix+'upLegRotReaderParent_jnt',parentOnly=True)[0]
	knee_jnt1 = pm.duplicate(knee, n=sidePrefix+'knee_jnt1',parentOnly=True)[0]
	pm.parent(upLegRotReaderParent_jnt,upLegRotReaderParent_jnt_frzGrp)
	pm.parent(knee_jnt1,upLegRotReaderParent_jnt)
	upLegRotReaderParent_jnt.hide()


	#create IK
	readUpLegIKHnd, readUpLegIKEffector = pm.ikHandle( sj=upLegRotReaderParent_jnt, ee=knee_jnt1, sol='ikRPsolver' )

	upLegTwistReader_loc = pm.spaceLocator(n=sidePrefix+'upLegTwistReader_loc1')
	DPTtrans.setTransformToObjectZero(srcObj=upLegTwistReader_loc, targetObj=upLegRotReaderParent_jnt, isTrans=True, isRot=True, isScale=False,isUnParent=False)
	upLegTwistReader_loc.hide()


	#create upLegtwistJntReaderFollow_grp
	upLegtwistJntReaderFollow_grp = DPTgrp.createEmpty( sidePrefix+'upLegtwistJntReaderFollow_grp', grpTop )

	upLegFollowHipJnt = pm.duplicate(hip, n=sidePrefix+'upLegupParJntFollow_jnt1',parentOnly=True)[0]
	upLegFollowUpLegJnt = pm.duplicate(upLeg, n=sidePrefix+'upLegupParJntFollow_jnt2',parentOnly=True)[0]
	upLegFollowKneeJnt = pm.duplicate(knee, n=sidePrefix+'upLegupParJntFollow_jnt3',parentOnly=True)[0]

	pm.parent(upLegFollowHipJnt,upLegtwistJntReaderFollow_grp)
	pm.parent(upLegFollowUpLegJnt,upLegFollowHipJnt)
	pm.parent(upLegFollowKneeJnt,upLegFollowUpLegJnt)
	pm.parent(readUpLegIKHnd,upLegFollowKneeJnt)

	upLegFollowHipJnt.hide()
	readUpLegIKHnd.hide()

	#constrain and connect
	pm.parentConstraint(hip, upLegFollowHipJnt, maintainOffset=True)

	pm.parentConstraint(upLegFollowHipJnt, upLegRotReaderParent_jnt_frzGrpTop)
	pm.pointConstraint(upLegFollowUpLegJnt, upLegRotReaderParent_jnt_frzGrp)

	pm.orientConstraint(upLegFollowUpLegJnt, upLegTwistReader_loc, skip=("y","z"))

	pm.connectAttr( 'rigSettings_ctrl.lfUpLegReaderRotOrder', upLegRotReaderParent_jnt+'.rotateOrder' )
	#pm.connectAttr( 'rigSettings_ctrl.lfUpLegReaderRotOrder', knee_jnt1+'.rotateOrder' )
	pm.connectAttr( 'rigSettings_ctrl.lfUpLegReaderRotOrder', upLegTwistReader_loc+'.rotateOrder' )

	pm.connectAttr( 'rigSettings_ctrl.lfUpLegReaderRotOrder', upLegFollowUpLegJnt+'.rotateOrder' )

	pm.connectAttr( upLeg+'.translate', upLegFollowUpLegJnt+'.translate' )
	pm.connectAttr( upLeg+'.rotate', upLegFollowUpLegJnt+'.rotate' )
	pm.connectAttr( upLeg+'.scale', upLegFollowUpLegJnt+'.scale' )

	pm.connectAttr( knee+'.translate', upLegFollowKneeJnt+'.translate' )
	pm.connectAttr( knee+'.rotate', upLegFollowKneeJnt+'.rotate' )
	pm.connectAttr( knee+'.scale', upLegFollowKneeJnt+'.scale' )


	####################################Create Mid Leg Reader####################################

	#create upLegMidJntRzReader_loc_frzGrp
	upLegMidJntRzReader_loc_frzGrp = DPTgrp.createEmpty( sidePrefix+'upLegMidJntRzReader_loc_frzGrp', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=upLegMidJntRzReader_loc_frzGrp, targetObj=upLeg, isTrans=True, isRot=True, isScale=False)
	pm.parent(upLegMidJntRzReader_loc_frzGrp, grpTop)

	upLegMidJntRzReader_loc = pm.spaceLocator(n=sidePrefix+'upLegMidJntRzReader_loc')
	DPTtrans.setTransformToObjectZero(srcObj=upLegMidJntRzReader_loc, targetObj=upLegMidJntRzReader_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
	upLegMidJntRzReader_loc.hide()

	pm.parentConstraint(upLeg, upLegMidJntRzReader_loc_frzGrp, maintainOffset=True)
	upLegMidJntRzReader_loc_aimCons = pm.aimConstraint( knee, upLegMidJntRzReader_loc, aimVector=(1,0,0), upVector=(0,0,1), worldUpVector=(0,0,1), worldUpType=2, worldUpObject=knee )


	####################################Create Low Leg Reader####################################

	#create lowLegTwistTarget_loc_frzGrp
	lowLegTwistTarget_loc_frzGrp = DPTgrp.createEmpty( sidePrefix+'lowLegTwistTarget_loc_frzGrp', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=lowLegTwistTarget_loc_frzGrp, targetObj=ankle, isTrans=True, isRot=True, isScale=False)
	pm.parent(lowLegTwistTarget_loc_frzGrp, grpTop)

	lowLegTwistTarget_loc = pm.spaceLocator(n=sidePrefix+'lowLegTwistTarget_loc')
	DPTtrans.setTransformToObjectZero(srcObj=lowLegTwistTarget_loc, targetObj=lowLegTwistTarget_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
	lowLegTwistTarget_loc.hide()

	pm.parentConstraint(ankle, lowLegTwistTarget_loc_frzGrp, maintainOffset=True)


	#create lowLegTwist_loc_frzGrp
	lowLegTwist_loc_frzGrp = DPTgrp.createEmpty( sidePrefix+'lowLegTwist_loc_frzGrp', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=lowLegTwist_loc_frzGrp, targetObj=knee, isTrans=True, isRot=True, isScale=False)
	pm.parent(lowLegTwist_loc_frzGrp, grpTop)

	lowLegTwist_loc = pm.spaceLocator(n=sidePrefix+'lowLegTwist_loc')
	DPTtrans.setTransformToObjectZero(srcObj=lowLegTwist_loc, targetObj=lowLegTwist_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
	lowLegTwist_loc.hide()

	pm.parentConstraint(knee, lowLegTwist_loc_frzGrp, maintainOffset=True)
	upLegMidJntRzReader_loc_aimCons = pm.aimConstraint( lowLegTwistTarget_loc, lowLegTwist_loc, aimVector=(1,0,0), upVector=(0,0,1), worldUpVector=(0,0,1), worldUpType=2, worldUpObject=lowLegTwistTarget_loc )


	#create lowLegUpTwistReader_jnt_frzGrp
	lowLegUpTwistReader_jnt_frzGrp = DPTgrp.createEmpty( sidePrefix+'lowLegUpTwistReader_jnt_frzGrp', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=lowLegUpTwistReader_jnt_frzGrp, targetObj=ankle, isTrans=True, isRot=True, isScale=False)
	pm.parent(lowLegUpTwistReader_jnt_frzGrp, grpTop)

	lowLegUpTwistReaderAnkleJnt1 = pm.duplicate(knee, n=sidePrefix+'lowLegUpTwistReader_jnt1',parentOnly=True)[0]
	DPTtrans.setTransformToObjectZero(srcObj=lowLegUpTwistReaderAnkleJnt1, targetObj=ankle, isTrans=True, isRot=False, isScale=False)
	lowLegUpTwistReaderAnkleJnt2 = pm.duplicate(ankle, n=sidePrefix+'lowLegUpTwistReader_jnt2',parentOnly=True)[0]
	pm.parent(lowLegUpTwistReaderAnkleJnt2,lowLegUpTwistReaderAnkleJnt1)
	pm.parent(lowLegUpTwistReaderAnkleJnt1,lowLegUpTwistReader_jnt_frzGrp)
	lowLegUpTwistReaderAnkleJnt2.translateX.set(1)


	pm.parentConstraint(knee, lowLegUpTwistReader_jnt_frzGrp, maintainOffset=True)
	pm.pointConstraint(ankle, lowLegUpTwistReaderAnkleJnt1)

	lowLegUpTwistReaderAnkleJnt1.hide()

	#create IK
	readLowLegIKHnd, readLowLegIKEffector = pm.ikHandle( sj=lowLegUpTwistReaderAnkleJnt1, ee=lowLegUpTwistReaderAnkleJnt2, sol='ikRPsolver' )
	pm.parent(readLowLegIKHnd,grpTop)
	readLowLegIKHnd.hide()

	pm.parentConstraint(ankle, readLowLegIKHnd, maintainOffset=True)


	#create lowLegPvTwistReader_loc_frzGrp
	lowLegPvTwistReader_loc_frzGrp = DPTgrp.createEmpty( sidePrefix+'lowLegPvTwistReader_loc_frzGrp', grpTop )
	DPTtrans.setTransformToObjectZero(srcObj=lowLegPvTwistReader_loc_frzGrp, targetObj=ankle, isTrans=True, isRot=False, isScale=False)
	pm.parent(lowLegPvTwistReader_loc_frzGrp, grpTop)

	lowLegPvTwistReader_loc = pm.spaceLocator(n=sidePrefix+'lowLegPvTwistReader_loc')
	DPTtrans.setTransformToObjectZero(srcObj=lowLegPvTwistReader_loc, targetObj=lowLegPvTwistReader_loc_frzGrp, isTrans=True, isRot=True, isScale=False, isUnParent=False)
	lowLegPvTwistReader_loc.hide()

	DPTattr.createAttributeFromDic(lowLegPvTwistReader_loc,
			[{'ln':'readerRot','h':True,'at':'float'}])

	pm.parentConstraint(ankle, lowLegPvTwistReader_loc_frzGrp, maintainOffset=True)
	pm.orientConstraint(lowLegUpTwistReaderAnkleJnt1, lowLegPvTwistReader_loc)


	####################################Create legRubberHose_grp####################################
	legRubberHose_grp = DPTgrp.createEmpty( sidePrefix+'legRubberHose_grp', grpTop )


	#create upLeg
	upLegRubberHose_grp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'upLegRubberHose_grp', alignObj=upLeg, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	upLegRubberHoseCtrl_jnt = pm.duplicate(upLeg, n=sidePrefix+'upLegRubberHoseCtrl_jnt',parentOnly=True)[0]
	upLegMidPos = DPTtrans.getPositionBetweenObjects(source=upLeg, target=knee, param=0.5)
	pm.move( upLegMidPos[0], upLegMidPos[1], upLegMidPos[2], upLegRubberHoseCtrl_jnt, a=True, ws=True, wd=True )
	upLegRubberHoseCtrl_jnt.hide()

	upLegRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'upLegRubberHose_ctrl', size=10.0, position=pm.xform(upLegRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
	#upLegRubberHose_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=sidePrefix+'upLegRubberHose_ctrl', size=10.0, alignObj=upLegRubberHoseCtrl_jnt)
	upLegRubberHose_ctrl_frzGrp, upLegRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(upLegRubberHose_ctrl, None, True, False, False)
	DPTattr.hideAttributes(upLegRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
	upLegRubberHose_ctrl.scale.lock()

	upLegRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'upLegRubberHoseCtrl_jnt_frzGrp', alignObj=upLegRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	pm.parent(upLegRubberHoseCtrl_jnt, upLegRubberHose_ctrl)
	pm.parent(upLegRubberHose_ctrl_frzGrp, upLegRubberHoseCtrl_jnt_frzGrp)
	pm.parent(upLegRubberHoseCtrl_jnt_frzGrp, upLegRubberHose_grp)
	pm.parent(upLegRubberHose_grp,legRubberHose_grp)

	pm.parentConstraint(upLeg, upLegRubberHose_grp, maintainOffset=True)
	foot_ctrl.rubberHoseCtrlVis >> upLegRubberHose_ctrl.visibility

	#create knee
	kneeRubberHose_grp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'kneeRubberHose_grp', alignObj=knee, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	pm.parent(kneeRubberHose_grp,legRubberHose_grp)


	kneeRubberHoseCtrl_jnt = pm.duplicate(knee, n=sidePrefix+'kneeRubberHoseCtrl_jnt',parentOnly=True)[0]
	kneeRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'kneeRubberHose_ctrl', size=10.0, position=pm.xform(kneeRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
	#kneeRubberHose_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=sidePrefix+'kneeRubberHose_ctrl', size=10.0, alignObj=kneeRubberHoseCtrl_jnt)
	kneeRubberHose_ctrl_frzGrp, kneeRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(kneeRubberHose_ctrl, None, True, False, False)
	kneeRubberHose_ctrl.scale.lock()
	kneeRubberHoseCtrl_jnt.hide()
	DPTattr.hideAttributes(kneeRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

	kneeRubberHose_ctrl_frzGrpTop = pm.duplicate(kneeRubberHose_ctrl_frzGrp, n=kneeRubberHose_ctrl_frzGrp+'_top',parentOnly=True)[0]
	pm.parent(kneeRubberHose_ctrl_frzGrp,kneeRubberHose_ctrl_frzGrpTop)

	kneeRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'kneeRubberHoseCtrl_jnt_frzGrp', alignObj=kneeRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	pm.parent(kneeRubberHose_ctrl_frzGrpTop,kneeRubberHoseCtrl_jnt_frzGrp)

	pm.parent(kneeRubberHoseCtrl_jnt,kneeRubberHose_ctrl)
	pm.parent(kneeRubberHoseCtrl_jnt_frzGrp,kneeRubberHose_grp)


	pm.parentConstraint(knee, kneeRubberHose_grp, maintainOffset=True)
	pm.pointConstraint(knee, kneeRubberHose_ctrl_frzGrpTop)
	foot_ctrl.rubberHoseCtrlVis >> kneeRubberHose_ctrl.visibility

	#knee upleg rot reader
	kneeUpJntRotReaderLoc_loc = pm.spaceLocator(n=sidePrefix+'kneeUpJntRotReaderLoc_loc')
	DPTtrans.setTransformToObjectZero(srcObj=kneeUpJntRotReaderLoc_loc, targetObj=upLeg, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	kneeUpJntRotReaderLoc_loc_frzGrp, kneeUpJntRotReaderLoc_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(kneeUpJntRotReaderLoc_loc)

	pm.parent(kneeUpJntRotReaderLoc_loc_frzGrp,kneeRubberHose_grp)

	pm.parentConstraint(hip, kneeUpJntRotReaderLoc_loc_frzGrp, maintainOffset=True)
	pm.pointConstraint(upLeg, kneeUpJntRotReaderLoc_loc_zeroFrzGrp)
	pm.orientConstraint(upLeg, kneeUpJntRotReaderLoc_loc)

	#knee lowleg rot reader
	kneeLowJntRotReaderLoc_loc = pm.spaceLocator(n=sidePrefix+'kneeLowJntRotReaderLoc_loc')
	DPTtrans.setTransformToObjectZero(srcObj=kneeLowJntRotReaderLoc_loc, targetObj=knee, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	kneeLowJntRotReaderLoc_loc_frzGrp, kneeLowJntRotReaderLoc_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(kneeLowJntRotReaderLoc_loc)

	pm.parent(kneeLowJntRotReaderLoc_loc_frzGrp,kneeRubberHose_grp)

	pm.parentConstraint(upLeg, kneeLowJntRotReaderLoc_loc_frzGrp, maintainOffset=True)
	pm.pointConstraint(knee, kneeLowJntRotReaderLoc_loc_zeroFrzGrp)
	pm.orientConstraint(knee, kneeLowJntRotReaderLoc_loc)


	#lowLeg
	lowLegRubberHose_grp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'lowLegRubberHose_grp', alignObj=knee, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	lowLegRubberHoseCtrl_jnt = pm.duplicate(knee, n=sidePrefix+'lowLegRubberHoseCtrl_jnt',parentOnly=True)[0]
	lowLegMidPos = DPTtrans.getPositionBetweenObjects(source=knee, target=ankle, param=0.5)
	pm.move( lowLegMidPos[0], lowLegMidPos[1], lowLegMidPos[2], lowLegRubberHoseCtrl_jnt, a=True, ws=True, wd=True )
	lowLegRubberHoseCtrl_jnt.hide()

	#lowLegRubberHose_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name=sidePrefix+'lowLegRubberHose_ctrl', size=10.0, alignObj=lowLegRubberHoseCtrl_jnt)
	lowLegRubberHose_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=sidePrefix+'lowLegRubberHose_ctrl', size=10.0, position=pm.xform(lowLegRubberHoseCtrl_jnt, q=True, ws=True, t=True ))
	lowLegRubberHose_ctrl_frzGrp, lowLegRubberHose_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(lowLegRubberHose_ctrl, None, True, False, False)
	lowLegRubberHose_ctrl_frzGrp.scale.lock()
	DPTattr.hideAttributes(lowLegRubberHose_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
	lowLegRubberHose_ctrl.scale.lock()

	lowLegRubberHoseCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'lowLegRubberHoseCtrl_jnt_frzGrp', alignObj=lowLegRubberHoseCtrl_jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	leg_ribbonLowEndBind_jnt = pm.duplicate(ankle, n=sidePrefix+'leg_ribbonLowEndBind_jnt',parentOnly=True)[0]
	leg_ribbonLowEndBind_jnt.hide()

	pm.parent(lowLegRubberHoseCtrl_jnt, lowLegRubberHose_ctrl)
	pm.parent(lowLegRubberHose_ctrl_frzGrp, lowLegRubberHoseCtrl_jnt_frzGrp)
	pm.parent(lowLegRubberHoseCtrl_jnt_frzGrp, lowLegRubberHose_grp)
	pm.parent(leg_ribbonLowEndBind_jnt, lowLegRubberHose_grp)
	pm.parent(lowLegRubberHose_grp,legRubberHose_grp)

	pm.parentConstraint(knee, lowLegRubberHose_grp, maintainOffset=True)
	pm.pointConstraint(ankle, leg_ribbonLowEndBind_jnt)
	foot_ctrl.rubberHoseCtrlVis >> lowLegRubberHose_ctrl.visibility


	#create mid up bind joint
	legUpMidSoftJnt_jnt = pm.duplicate(upLeg, n=sidePrefix+'legUpMidSoftJnt_jnt',parentOnly=True)[0]
	param = 0.99
	if DPTStr.isContain(sidePrefix, 'R'):
		param = 1.01
	legUpMidSoftPos = DPTtrans.getPositionBetweenObjects(source=upLeg, target=knee, param=param)
	kneePos = DPTtrans.getPositionBetweenObjects(source=upLeg, target=knee, param=1.0)
	pm.move( kneePos[0], kneePos[1], kneePos[2], legUpMidSoftJnt_jnt, a=True, ws=True, wd=True )
	legUpMidSoftJnt_jnt.hide()

	legUpMidSoftJnt_jnt_frzGrp, legUpMidSoftJnt_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(legUpMidSoftJnt_jnt)
	pm.move( legUpMidSoftPos[0], legUpMidSoftPos[1], legUpMidSoftPos[2], legUpMidSoftJnt_jnt, a=True, ws=True, wd=True )

	legUpMidSoftJnt_jnt_frzGrp_AimCons = pm.duplicate(legUpMidSoftJnt_jnt_frzGrp, n=legUpMidSoftJnt_jnt_frzGrp+'_AimCons',parentOnly=True)[0]
	legUpMidSoftJnt_jnt_frzGrp_Top = pm.duplicate(legUpMidSoftJnt_jnt_frzGrp, n=legUpMidSoftJnt_jnt_frzGrp+'_top',parentOnly=True)[0]

	pm.parent(legUpMidSoftJnt_jnt_frzGrp, legUpMidSoftJnt_jnt_frzGrp_AimCons)
	pm.parent(legUpMidSoftJnt_jnt_frzGrp_AimCons, legUpMidSoftJnt_jnt_frzGrp_Top)
	pm.parent(legUpMidSoftJnt_jnt_frzGrp_Top, legRubberHose_grp)

	pm.aimConstraint( upLegRubberHoseCtrl_jnt, legUpMidSoftJnt_jnt_frzGrp_AimCons, aimVector=(-1,0,0), upVector=(0,-1,0), worldUpVector=(0,1,0), worldUpType=1, worldUpObject=upLegRubberHoseCtrl_jnt )


	#create mid down bind joint
	legDnMidSoftJnt_jnt = pm.duplicate(knee, n=sidePrefix+'legDnMidSoftJnt_jnt',parentOnly=True)[0]
	param = 1.01
	if DPTStr.isContain(sidePrefix, 'R'):
		param = 0.99

	legDnMidSoftPos = DPTtrans.getPositionBetweenObjects(source=ankle, target=knee, param=param)
	legDnMidSoftJnt_jnt.hide()

	legDnMidSoftJnt_jnt_frzGrp, legDnMidSoftJnt_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(legDnMidSoftJnt_jnt)
	pm.move( legDnMidSoftPos[0], legDnMidSoftPos[1], legDnMidSoftPos[2], legDnMidSoftJnt_jnt, a=True, ws=True, wd=True )

	legDnMidSoftJnt_jnt_frzGrp_AimCons = pm.duplicate(legDnMidSoftJnt_jnt_frzGrp, n=legDnMidSoftJnt_jnt_frzGrp+'_AimCons',parentOnly=True)[0]
	legDnMidSoftJnt_jnt_frzGrp_Top = pm.duplicate(legDnMidSoftJnt_jnt_frzGrp, n=legDnMidSoftJnt_jnt_frzGrp+'_top',parentOnly=True)[0]

	pm.parent(legDnMidSoftJnt_jnt_frzGrp, legDnMidSoftJnt_jnt_frzGrp_AimCons)
	pm.parent(legDnMidSoftJnt_jnt_frzGrp_AimCons, legDnMidSoftJnt_jnt_frzGrp_Top)
	pm.parent(legDnMidSoftJnt_jnt_frzGrp_Top, legRubberHose_grp)

	pm.aimConstraint( lowLegRubberHoseCtrl_jnt, legDnMidSoftJnt_jnt_frzGrp_AimCons, aimVector=(1,0,0), upVector=(0,1,0), worldUpVector=(0,1,0), worldUpType=1, worldUpObject=lowLegRubberHoseCtrl_jnt )


	#create up leg soft loc
	legMidUpSoftTarg_loc = pm.spaceLocator(n=sidePrefix+'legMidUpSoftTarg_loc')
	DPTtrans.setTransformToObjectZero(srcObj=legMidUpSoftTarg_loc, targetObj=upLeg, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	legMidUpSoftTarg_loc.hide()

	legMidUpSoftTarg_loc_frzGrp, legMidUpSoftTarg_loc_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(legMidUpSoftTarg_loc)
	pm.parent(legMidUpSoftTarg_loc_frzGrp, legRubberHose_grp)

	legMidUpHardTarg_loc = pm.spaceLocator(n=sidePrefix+'legMidUpHardTarg_loc')
	DPTtrans.setTransformToObjectZero(srcObj=legMidUpHardTarg_loc, targetObj=legMidUpSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	legMidUpHardTarg_loc.hide()

	legMidUpHardTarg_loc_frzGrp= DPTgrp.createEmptyWithAlign(name=legMidUpHardTarg_loc+'_frzGrp', alignObj=legMidUpHardTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	pm.parent(legMidUpHardTarg_loc, legMidUpHardTarg_loc_frzGrp)
	pm.parent(legMidUpHardTarg_loc_frzGrp, legMidUpSoftTarg_loc_zeroFrzGrp)

	pm.parentConstraint(upLeg, legMidUpSoftTarg_loc_frzGrp, maintainOffset=True)
	pm.pointConstraint(knee, legMidUpSoftTarg_loc_zeroFrzGrp)

	pm.pointConstraint(kneeRubberHoseCtrl_jnt, legMidUpHardTarg_loc_frzGrp)

	#create down leg soft loc
	legMidDnSoftTarg_loc = pm.spaceLocator(n=sidePrefix+'legMidDnSoftTarg_loc')
	DPTtrans.setTransformToObjectZero(srcObj=legMidDnSoftTarg_loc, targetObj=knee, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	legMidDnSoftTarg_loc.hide()

	legMidDnSoftTarg_loc_frzGrp= DPTgrp.createEmptyWithAlign(name=legMidDnSoftTarg_loc+'_frzGrp', alignObj=legMidDnSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	pm.parent(legMidDnSoftTarg_loc,legMidDnSoftTarg_loc_frzGrp)
	pm.parent(legMidDnSoftTarg_loc_frzGrp, legRubberHose_grp)

	legMidDnHardTarg_loc = pm.spaceLocator(n=sidePrefix+'legMidDnHardTarg_loc')
	DPTtrans.setTransformToObjectZero(srcObj=legMidDnHardTarg_loc, targetObj=legMidDnSoftTarg_loc, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	#legMidDnHardTarg_loc = pm.duplicate(legMidDnSoftTarg_loc, n=sidePrefix+'legMidDnHardTarg_loc',parentOnly=True)[0]
	legMidDnHardTarg_loc_frzGrp = pm.duplicate(legMidDnSoftTarg_loc_frzGrp, n=sidePrefix+'legMidDnHardTarg_loc_frzGrp',parentOnly=True)[0]
	legMidDnHardTarg_loc.hide()

	pm.parent(legMidDnHardTarg_loc,legMidDnHardTarg_loc_frzGrp)
	pm.parent(legMidDnHardTarg_loc_frzGrp,legMidDnSoftTarg_loc_frzGrp)


	pm.parentConstraint(knee, legMidDnSoftTarg_loc_frzGrp, maintainOffset=True)
	pm.parentConstraint(kneeRubberHoseCtrl_jnt, legMidDnHardTarg_loc_frzGrp, maintainOffset=True)

	#other constrain and connect
	upLegMidCons = pm.parentConstraint(legMidUpSoftTarg_loc, legMidUpHardTarg_loc, legUpMidSoftJnt_jnt_frzGrp_Top, maintainOffset=True )

	softAttr = DPTattr.getMatchNameAttribute(node=upLegMidCons, matchStr='Soft', kwargs={'ud':True})
	hardAttr = DPTattr.getMatchNameAttribute(node=upLegMidCons, matchStr='Hard', kwargs={'ud':True})

	kneeSoftness_reverse = DPTnode.createNodeWithParam(node_type='reverse', name=sidePrefix+'kneeSoftness_reverse', param={} )

	foot_ctrl.kneeSoftness >> kneeSoftness_reverse.inputX

	pm.connectAttr( foot_ctrl+'.kneeSoftness', upLegMidCons+'.'+softAttr )
	pm.connectAttr( kneeSoftness_reverse+'.outputX', upLegMidCons+'.'+hardAttr )



	lowLegMidCons = pm.parentConstraint(legMidDnSoftTarg_loc, legMidDnHardTarg_loc, legDnMidSoftJnt_jnt_frzGrp_Top, maintainOffset=True )

	softAttr = DPTattr.getMatchNameAttribute(node=lowLegMidCons, matchStr='Soft', kwargs={'ud':True})
	hardAttr = DPTattr.getMatchNameAttribute(node=lowLegMidCons, matchStr='Hard', kwargs={'ud':True})

	pm.connectAttr( foot_ctrl+'.kneeSoftness', lowLegMidCons+'.'+softAttr )
	pm.connectAttr( kneeSoftness_reverse+'.outputX', lowLegMidCons+'.'+hardAttr )



	####################################Create legNoScale_rig_grp####################################
	legNoScale_rig_grp = DPTgrp.createEmpty( sidePrefix+'legNoScale_rig_grp', grpTop )
	legNoScale_rig_grp.setAttr('inheritsTransform',0)


	#create ik curve
	legCurve = DPTCurveTool.createCurveAlongObject(name=sidePrefix+'legCurve', objects=[upLeg,upLegRubberHoseCtrl_jnt,legUpMidSoftJnt_jnt,kneeRubberHoseCtrl_jnt,legDnMidSoftJnt_jnt,lowLegRubberHoseCtrl_jnt,leg_ribbonLowEndBind_jnt],degree=3)
	legCurveShape = legCurve.getShape()
	pm.parent(legCurve, legNoScale_rig_grp)
	legCurve.hide()

	#create ik surf and follicle
	legCurve_crvLoft1 = pm.duplicate(legCurve, n=legCurve+'_loft1')[0]
	legCurve_crvLoft2 = pm.duplicate(legCurve, n=legCurve+'_loft2')[0]
	pm.move( -0.5, 0, 0,legCurve_crvLoft1, r=True )
	pm.move( 0.5, 0, 0,legCurve_crvLoft2, r=True )

	leg_ribbon_surf = pm.loft(legCurve_crvLoft1,legCurve_crvLoft2, ch=False, rn=True, degree=1, name=sidePrefix+'leg_ribbon_surf' )[0]
	pm.delete(legCurve_crvLoft1, legCurve_crvLoft2)
	pm.parent(leg_ribbon_surf, legNoScale_rig_grp)
	leg_ribbon_surf.hide()

	lowLegRhFollicles_grp = DPTgrp.createEmpty(sidePrefix+'lowLegRhFollicles_grp',legNoScale_rig_grp)
	#floList=[]
	jNum = 1
	#posParamList = [0.0, 0.25, 0.3333, 0.5, 0.6666, 0.8333, 1.0]

	jList = [upLeg,upLegRubberHoseCtrl_jnt,legUpMidSoftJnt_jnt,kneeRubberHoseCtrl_jnt,legDnMidSoftJnt_jnt,lowLegRubberHoseCtrl_jnt,leg_ribbonLowEndBind_jnt]
	pJntList = ['up_leg'+sidePrefix+'_weight_jnt1','up_leg'+sidePrefix+'_weight_jnt2','up_leg'+sidePrefix+'_weight_jnt3',
		'knee'+sidePrefix+'_weight_jnt1','knee'+sidePrefix+'_weight_jnt2','knee'+sidePrefix+'_weight_jnt3','ankle'+sidePrefix+'_weight_jnt1']
	#maxval = legCurveShape.getAttr ( "maxValue" )
	#minval = legCurveShape.getAttr ( "minValue" )
	#paramLen = maxval-minval

	folOriJoints = []

	for jnt in jList:
		legRibbon_loc = pm.spaceLocator(n=sidePrefix+'legRibbon_'+str(jNum)+'_loc')

		vParam = DPTCurveTool.closestPointOnCurve(location=pm.xform(pJntList[jNum-1], q=True, ws=True, t=True ), curveObject=legCurve)
		#print vParam

		#legFollOrient_grp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'legFollOrient'+str(jNum)+'_grp', alignObj=jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
		legFollOrient_grp = DPTgrp.createEmpty(name=sidePrefix+'legFollOrient'+str(jNum)+'_grp')
		legFollOrient_grp_frzGrp = pm.duplicate(legFollOrient_grp, n=sidePrefix+'legFollOrient'+str(jNum)+'_grp_frzGrp',parentOnly=True)[0]

		pm.parent(legFollOrient_grp, legFollOrient_grp_frzGrp)
		pm.parent(legFollOrient_grp_frzGrp, legRibbon_loc)
		pm.parent(legRibbon_loc, lowLegRhFollicles_grp)

		folOriJoints.append(legFollOrient_grp)

		onSrfPtNode = DPTnode.createNodeWithParam(node_type='pointOnSurfaceInfo', name='pointOnSurfaceInfo1', param={'parameterU':0.5,'parameterV':vParam} )
		jntAimCons = DPTnode.createNodeWithParam(node_type='aimConstraint', name='rivetAimConstraint1',param={'aimVectorX':0,'aimVectorY':1,'aimVectorZ':0,
																									'upVectorX':-1,'upVectorY':0,'upVectorZ':0,'worldUpType':3} )

		pm.parent(jntAimCons, legRibbon_loc)

		leg_ribbon_surf.worldSpace >> onSrfPtNode.inputSurface
		onSrfPtNode.result.position >> legRibbon_loc.translate
		onSrfPtNode.result.normal >> jntAimCons.target[0].targetTranslate
		onSrfPtNode.result.tangentU >> jntAimCons.worldUpVector
		jntAimCons.constraintRotateX >> legRibbon_loc.rotateX
		jntAimCons.constraintRotateY >> legRibbon_loc.rotateY
		jntAimCons.constraintRotateZ >> legRibbon_loc.rotateZ

		legRibbon_loc.hide()

		jNum+=1


	#smoothbind ik curve and loft surf

	legRibbonSurfSkinClster = pm.skinCluster(jList, leg_ribbon_surf, sm=0, tsb=True, mi=1, nw=1)
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][0]', transformValue=[(upLeg, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][1]', transformValue=[(upLegRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][2]', transformValue=[(legUpMidSoftJnt_jnt, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][3]', transformValue=[(kneeRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][4]', transformValue=[(legDnMidSoftJnt_jnt, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][5]', transformValue=[(lowLegRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( legRibbonSurfSkinClster, leg_ribbon_surf+'.cv[0:1][6]', transformValue=[(leg_ribbonLowEndBind_jnt, 1.0)])

	ikSkinClster = pm.skinCluster(jList, legCurve, sm=0, tsb=True, mi=1, nw=1)
	pm.skinPercent( ikSkinClster, legCurve+'.cv[0]', transformValue=[(upLeg, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[1]', transformValue=[(upLegRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[2]', transformValue=[(legUpMidSoftJnt_jnt, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[3]', transformValue=[(kneeRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[4]', transformValue=[(legDnMidSoftJnt_jnt, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[5]', transformValue=[(lowLegRubberHoseCtrl_jnt, 1.0)])
	pm.skinPercent( ikSkinClster, legCurve+'.cv[6]', transformValue=[(leg_ribbonLowEndBind_jnt, 1.0)])






	####################################Create Leg Weight Joint####################################

	legWtCtrlJnt_grp = DPTgrp.createEmpty( sidePrefix+'legWtCtrlJnt_grp', grpTop )

	upLegDnSplitCtrlParent_jnt = pm.duplicate(knee, n=sidePrefix+'upLegDnSplitCtrlParent_jnt',parentOnly=True)[0]
	pm.parent(upLegDnSplitCtrlParent_jnt, legWtCtrlJnt_grp)
	upLegDnSplitCtrlParent_jnt.hide()

	pm.parentConstraint(knee, upLegDnSplitCtrlParent_jnt, maintainOffset=True)

	#create down leg weight joint
	kneeCtrlSplit_1_jnt = pm.duplicate('knee'+sidePrefix+'_weight_jnt2', n=sidePrefix+'kneeCtrlSplit_1_jnt',parentOnly=True)[0]
	kneeCtrlSplit_2_jnt = pm.duplicate('knee'+sidePrefix+'_weight_jnt3', n=sidePrefix+'kneeCtrlSplit_2_jnt',parentOnly=True)[0]
	ankleCtrl_jnt = pm.duplicate('ankle'+sidePrefix+'_weight_jnt1', n=sidePrefix+'ankleCtrl_jnt',parentOnly=True)[0]
	kneeCtrlSplit_1_jnt.hide()
	kneeCtrlSplit_2_jnt.hide()
	ankleCtrl_jnt.hide()

	kneeCtrlSplit_1_jnt_frzGrp, kneeCtrlSplit_1_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=kneeCtrlSplit_1_jnt, parent=upLegDnSplitCtrlParent_jnt)
	kneeCtrlSplit_2_jnt_frzGrp, kneeCtrlSplit_2_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=kneeCtrlSplit_2_jnt, parent=upLegDnSplitCtrlParent_jnt)
	ankleCtrl_jnt_frzGrp, ankleCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=ankleCtrl_jnt, parent=upLegDnSplitCtrlParent_jnt)


	pm.parentConstraint(folOriJoints[4], kneeCtrlSplit_1_jnt_frzGrp, maintainOffset=True)
	pm.parentConstraint(folOriJoints[5], kneeCtrlSplit_2_jnt_frzGrp, maintainOffset=True)
	pm.parentConstraint(folOriJoints[6], ankleCtrl_jnt_frzGrp, maintainOffset=True)

	pm.parentConstraint(ankle, ankleCtrl_jnt, maintainOffset=True)


	#create Up leg weight joint
	upLegUpSplitCtrlParent_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'upLegUpSplitCtrlParent_jnt_frzGrp', alignObj=upLeg, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	upLegUpSplitCtrlParent_jnt = pm.duplicate(upLeg, n=sidePrefix+'upLegUpSplitCtrlParent_jnt',parentOnly=True)[0]
	pm.parent(upLegUpSplitCtrlParent_jnt,upLegUpSplitCtrlParent_jnt_frzGrp)
	pm.parent(upLegUpSplitCtrlParent_jnt_frzGrp, legWtCtrlJnt_grp)
	upLegUpSplitCtrlParent_jnt.hide()

	pm.parentConstraint(upLeg, upLegUpSplitCtrlParent_jnt_frzGrp)


	upLegCtrlSplit_1_jnt = pm.duplicate('up_leg'+sidePrefix+'_weight_jnt2', n=sidePrefix+'upLegCtrlSplit_1_jnt',parentOnly=True)[0]
	upLegCtrlSplit_2_jnt = pm.duplicate('up_leg'+sidePrefix+'_weight_jnt3', n=sidePrefix+'upLegCtrlSplit_2_jnt',parentOnly=True)[0]
	upLegCtrlSplit_1_jnt.hide()
	upLegCtrlSplit_2_jnt.hide()

	upLegCtrlSplit_1_jnt_frzGrp, upLegCtrlSplit_1_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=upLegCtrlSplit_1_jnt, parent=upLegUpSplitCtrlParent_jnt)
	upLegCtrlSplit_2_jnt_frzGrp, upLegCtrlSplit_2_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=upLegCtrlSplit_2_jnt, parent=upLegUpSplitCtrlParent_jnt)

	pm.parentConstraint(folOriJoints[1], upLegCtrlSplit_1_jnt_frzGrp, maintainOffset=True)
	pm.parentConstraint(folOriJoints[2], upLegCtrlSplit_2_jnt_frzGrp, maintainOffset=True)


	#create other
	upLegWtCtrl_jnt_frzGrp = DPTgrp.createEmptyWithAlign(name=sidePrefix+'upLegWtCtrl_jnt_frzGrp', alignObj=upLeg, isTrans=True, isRot=True, isScale=False, isUnParent=True)
	pm.parent(upLegWtCtrl_jnt_frzGrp, legWtCtrlJnt_grp)

	pm.parentConstraint(upLegUpSplitCtrlParent_jnt, upLegWtCtrl_jnt_frzGrp, maintainOffset=True)



	####################################Create legRhUpMidDnCtrlJnt_grp####################################

	#top group
	legRhUpMidDnCtrlJnt_grp = DPTgrp.createEmpty( sidePrefix+'legRhUpMidDnCtrlJnt_grp', grpTop )


	upLegCtrl_jnt = pm.duplicate(upLeg, n=sidePrefix+'upLegCtrl_jnt',parentOnly=True)[0]
	kneeCtrl_jnt = pm.duplicate(knee, n=sidePrefix+'kneeCtrl_jnt',parentOnly=True)[0]
	upLegCtrl_jnt.hide()
	kneeCtrl_jnt.hide()

	upLegCtrl_jnt_frzGrp, upLegCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=upLegCtrl_jnt, parent=legRhUpMidDnCtrlJnt_grp)
	kneeCtrl_jnt_frzGrp, kneeCtrl_jnt_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(baseObject=kneeCtrl_jnt, parent=legRhUpMidDnCtrlJnt_grp)

	pm.parentConstraint(folOriJoints[0], upLegCtrl_jnt_frzGrp, maintainOffset=True)
	pm.parentConstraint(folOriJoints[3], kneeCtrl_jnt_frzGrp, maintainOffset=True)


	####################################Create ankleBallReader_UpLeg_jnt####################################


	ankleBallReader_UpLeg_jnt = pm.duplicate(upLeg, n=sidePrefix+'ankleBallReader_UpLeg_jnt',parentOnly=True)[0]
	ankleBallReader_Knee_jnt = pm.duplicate(knee, n=sidePrefix+'ankleBallReader_Knee_jnt',parentOnly=True)[0]
	ankleBallReader_Ankle_jnt = pm.duplicate(ankle, n=sidePrefix+'ankleBallReader_Ankle_jnt',parentOnly=True)[0]
	ankleBallReader_Ball_jnt = pm.duplicate(ball, n=sidePrefix+'ankleBallReader_Ball_jnt',parentOnly=True)[0]

	pm.parent(ankleBallReader_Ball_jnt, ankleBallReader_Ankle_jnt)
	pm.parent(ankleBallReader_Ankle_jnt, ankleBallReader_Knee_jnt)
	pm.parent(ankleBallReader_Knee_jnt, ankleBallReader_UpLeg_jnt)
	pm.parent(ankleBallReader_UpLeg_jnt, grpTop)

	pm.pointConstraint(upLeg, ankleBallReader_UpLeg_jnt)
	pm.orientConstraint(upLeg, ankleBallReader_UpLeg_jnt)

	pm.orientConstraint(knee, ankleBallReader_Knee_jnt)
	pm.orientConstraint(ankleRevJnt, ankleBallReader_Ankle_jnt)
	pm.orientConstraint(ballIKJnt, ankleBallReader_Ball_jnt)

	ankleBallReader_UpLeg_jnt.hide()


	####################################Create Other Connection####################################

	upLegDistVal = DPTtrans.getDistanceBetweenObjects(source=upLeg, target=knee)
	lowLegDistVal = DPTtrans.getDistanceBetweenObjects(source=knee, target=ankle)


	#FK Translate
	legStrAtt_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStrAtt_multDiv', param={'operation':2,'input2X':10,'input2Y':10,'input2Z':10} )
	legTotStrAmt_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legTotStrAmt_plsMns', param={'operation':1} )
	legRefStr_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legRefStr_multDiv', param={'operation':1,'input2Z':1} )
	legUpStr_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legUpStr_plsMns', param={'operation':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		legUpStr_plsMns.setAttr('input1D[1]', upLegDistVal)
		legRefStr_multDiv.setAttr('input2X', upLegDistVal)
		legRefStr_multDiv.setAttr('input2Y', lowLegDistVal)
	else:
		legUpStr_plsMns.setAttr('input1D[1]', -upLegDistVal)
		legRefStr_multDiv.setAttr('input2X', -upLegDistVal)
		legRefStr_multDiv.setAttr('input2Y', -lowLegDistVal)


	foot_ctrl.ankleStretch >> legStrAtt_multDiv.input1Z
	foot_ctrl.kneeStretch >> legStrAtt_multDiv.input1Y
	foot_ctrl.legStretch >> legStrAtt_multDiv.input1X

	DPTattr.connectAttrToList(legStrAtt_multDiv+'.outputX', [legTotStrAmt_plsMns+'.input2D[0].input2Dx',legTotStrAmt_plsMns+'.input2D[0].input2Dy'])
	legStrAtt_multDiv.outputY >> legTotStrAmt_plsMns.input2D[1].input2Dx
	legStrAtt_multDiv.outputZ >> legTotStrAmt_plsMns.input2D[1].input2Dy

	legTotStrAmt_plsMns.output2Dx >> legRefStr_multDiv.input1X
	legTotStrAmt_plsMns.output2Dy >> legRefStr_multDiv.input1Y

	legRefStr_multDiv.outputX >> legUpStr_plsMns.input1D[0]
	##########################OUT##########################
	legUpStr_plsMns.output1D >> FKKneeJnt.translateX


	#low leg Translate
	legLowStr_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legLowStr_plsMns', param={'operation':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		legLowStr_plsMns.setAttr('input1D[1]', lowLegDistVal)
	else:
		legLowStr_plsMns.setAttr('input1D[1]', -lowLegDistVal)

	legRefStr_multDiv.outputY >> legLowStr_plsMns.input1D[0]
	##########################OUT##########################
	legLowStr_plsMns.output1D >> FKAnkleJnt.translateX



	#Denom Translate
	legStrDenom_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legStrDenom_plsMns', param={'operation':1} )

	legLowStr_plsMns.output1D >> legStrDenom_plsMns.input1D[0]
	legUpStr_plsMns.output1D >> legStrDenom_plsMns.input1D[1]



	#from legStrDistShape
	legDistScaleAdjust_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legDistScaleAdjust_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legStrDistShape.distance >> legDistScaleAdjust_multDiv.input1X
	pm.connectAttr('root_ctrl.masterScale', legDistScaleAdjust_multDiv+'.input2X')

	legStrNum_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStrNum_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legDistScaleAdjust_multDiv.outputX >> legStrNum_multDiv.input1X
	pm.connectAttr(heel_ik_ctrl+'.stretchyLeg', legStrNum_multDiv+'.input2X')


	legStrScalar_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStrScalar_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legStrNum_multDiv.outputX >> legStrScalar_multDiv.input1X
	legStrDenom_plsMns.output1D >> legStrScalar_multDiv.input2X


	legStr_clamp = DPTnode.createNodeWithParam(node_type='clamp', name=sidePrefix+'legStr_clamp', param={'minR':1} )


	if DPTStr.isContain(sidePrefix, 'L'):
		legStrScalar_multDiv.outputX >> legStr_clamp.inputR
		heel_ik_ctrl.maxStretch >> legStr_clamp.maxR
	else:
		legNegateTx_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legNegateTx_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )
		legStrScalar_multDiv.outputX >> legNegateTx_multDiv.input1X
		legNegateTx_multDiv.outputX >> legStr_clamp.inputR
		heel_ik_ctrl.maxStretch >> legStr_clamp.maxR




	legStr_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStr_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legUpStr_plsMns.output1D >> legStr_multDiv.input1X
	legLowStr_plsMns.output1D >> legStr_multDiv.input1Y
	legStr_clamp.outputR >> legStr_multDiv.input2X
	legStr_clamp.outputR >> legStr_multDiv.input2Y


	#from legPvIkCtrlDistShape and upLegPvCtrlDistShape
	LegPvDistScaleAdjust_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'LegPvDistScaleAdjust_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


	upLegPvCtrlDistShape.distance >> LegPvDistScaleAdjust_multDiv.input1X
	legPvIkCtrlDistShape.distance >> LegPvDistScaleAdjust_multDiv.input1Y
	pm.connectAttr('root_ctrl.masterScale', LegPvDistScaleAdjust_multDiv+'.input2X')
	pm.connectAttr('root_ctrl.masterScale', LegPvDistScaleAdjust_multDiv+'.input2Y')










	#blendColors
	upLegSnapSplice_blend  = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'upLegSnapSplice_blend', param={} )
	kneeSnapSplice_blend  = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'kneeSnapSplice_blend', param={} )

	knee_pv_ctrl.snapKnee >> upLegSnapSplice_blend.blender
	knee_pv_ctrl.snapKnee >> kneeSnapSplice_blend.blender
	legStr_multDiv.outputX >> upLegSnapSplice_blend.color2R
	legStr_multDiv.outputY >> kneeSnapSplice_blend.color2R


	if DPTStr.isContain(sidePrefix, 'L'):
		LegPvDistScaleAdjust_multDiv.outputX >> upLegSnapSplice_blend.color1R
		LegPvDistScaleAdjust_multDiv.outputY >> kneeSnapSplice_blend.color1R
	else:
		legPvNegate_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legPvNegate_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )
		LegPvDistScaleAdjust_multDiv.outputX >> legPvNegate_multDiv.input1X
		LegPvDistScaleAdjust_multDiv.outputY >> legPvNegate_multDiv.input1Y
		legPvNegate_multDiv.outputX >> upLegSnapSplice_blend.color1R
		legPvNegate_multDiv.outputY >> kneeSnapSplice_blend.color1R






	##########################OUT##########################
	upLegSnapSplice_blend.outputR >> IKKneeJnt.translateX

	##########################OUT##########################
	kneeSnapSplice_blend.outputR >> IKAnkleJnt.translateX



	#strech value
	leg_crvInfo  = DPTnode.createNodeWithParam(node_type='curveInfo', name=sidePrefix+'leg_crvInfo', param={} )
	legStretch_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStretch_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
	scaleAdjustedDefaultArcLen_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'scaleAdjustedDefaultArcLen_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


	legCurveShape.worldSpace >> leg_crvInfo.inputCurve
	pm.connectAttr('root_ctrl.masterScale', scaleAdjustedDefaultArcLen_multDiv+'.input2X')
	#print pm.arclen( legCurve )
	scaleAdjustedDefaultArcLen_multDiv.setAttr('input1X', pm.arclen( legCurve ) )

	leg_crvInfo.arcLength >> legStretch_multDiv.input1X
	scaleAdjustedDefaultArcLen_multDiv.outputX >> legStretch_multDiv.input2X

	##########################OUT##########################
	legStretch_multDiv.outputX >> heel_ik_ctrl.stretchValue


	legNormStretchVal_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legNormStretchVal_plsMns', param={'operation':2,'input1D[0]':1} )
	legStretch_multDiv.outputX >> legNormStretchVal_plsMns.input1D[1]


	legNormStretchNegator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legNormStretchNegator_multDiv', param={'operation':1,'input1X':-1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )
	legNormStretchVal_plsMns.output1D >> legNormStretchNegator_multDiv.input2X


	legNormStretch_cond = DPTnode.createNodeWithParam(node_type='condition', name=sidePrefix+'legNormStretch_cond', param={'operation':3,'secondTerm':0} )

	legNormStretchVal_plsMns.output1D >> legNormStretch_cond.firstTerm
	legNormStretchVal_plsMns.output1D >> legNormStretch_cond.colorIfTrueR
	legNormStretchNegator_multDiv.outputX >> legNormStretch_cond.colorIfFalseR



	legStretchSpeed_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStretchSpeed_multDiv', param={'operation':2,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftLegStretchSpeed',legStretchSpeed_multDiv+'.input2X' )
	else:
		pm.connectAttr('rigSettings_ctrl.rightLegStretchSpeed',legStretchSpeed_multDiv+'.input2X' )


	legStretchNumerator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStretchNumerator_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legNormStretch_cond.outColorR >> legStretchNumerator_multDiv.input1X

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftLegStretchSpeed',legStretchNumerator_multDiv+'.input2X' )
	else:
		pm.connectAttr('rigSettings_ctrl.rightLegStretchSpeed',legStretchNumerator_multDiv+'.input2X' )




	legStretchDenominator_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legStretchDenominator_plsMns', param={'operation':1} )

	legNormStretch_cond.outColorR >> legStretchDenominator_plsMns.input1D[0]
	legStretchSpeed_multDiv.outputX >> legStretchDenominator_plsMns.input1D[1]




	legStretch_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStretch_multDiv1', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legStretchNumerator_multDiv.outputX >> legStretch_multDiv1.input1X
	legStretchDenominator_plsMns.output1D >> legStretch_multDiv1.input2X


	legStretchNegator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legStretchNegator_multDiv', param={'operation':1,'input1X':-1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legStretch_multDiv1.outputX >> legStretchNegator_multDiv.input2X



	legSquashSpeed_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquashSpeed_multDiv', param={'operation':2,'input1X':1,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftLegSquashSpeed', legSquashSpeed_multDiv+'.input2X')
	else:
		pm.connectAttr('rigSettings_ctrl.rightLegSquashSpeed', legSquashSpeed_multDiv+'.input2X')



	legSquashDenominator_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legSquashDenominator_plsMns', param={'operation':1} )

	legNormStretch_cond.outColorR >> legSquashDenominator_plsMns.input1D[0]
	legSquashSpeed_multDiv.outputX >> legSquashDenominator_plsMns.input1D[1]


	legSquashNumerator_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquashNumerator_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legNormStretch_cond.outColorR >> legSquashNumerator_multDiv.input1X

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftLegMaxSquashAmt', legSquashNumerator_multDiv+'.input2X')
	else:
		pm.connectAttr('rigSettings_ctrl.rightLegMaxSquashAmt', legSquashNumerator_multDiv+'.input2X')


	legSquash_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquash_multDiv', param={'operation':2,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	legSquashNumerator_multDiv.outputX >> legSquash_multDiv.input1X
	legSquashDenominator_plsMns.output1D >> legSquash_multDiv.input2X


	legScale_cond = DPTnode.createNodeWithParam(node_type='condition', name=sidePrefix+'legScale_cond', param={'operation':3,'secondTerm':1} )


	legStretch_multDiv.outputX >> legScale_cond.firstTerm
	legStretchNegator_multDiv.outputX >> legScale_cond.colorIfTrueR
	legSquash_multDiv.outputX >> legScale_cond.colorIfFalseR



	legManualSquashStretchNormalize_1_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legManualSquashStretchNormalize_1_plsMns', param={'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )

	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dx
	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dy
	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dz




	legManualSquashStretchRotAmt_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legManualSquashStretchRotAmt_1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfUpLeg_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1X')
		pm.connectAttr('rigSettings_ctrl.lfUpLegSplit_1_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.lfUpLegSplit_2_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1Z')
	else:
		pm.connectAttr('rigSettings_ctrl.rtUpLeg_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1X')
		pm.connectAttr('rigSettings_ctrl.rtUpLegSplit_1_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.rtUpLegSplit_2_SsScale', legManualSquashStretchRotAmt_1_multDiv+'.input1Z')

	legManualSquashStretchNormalize_1_plsMns.output3Dx >> legManualSquashStretchRotAmt_1_multDiv.input2X
	legManualSquashStretchNormalize_1_plsMns.output3Dy >> legManualSquashStretchRotAmt_1_multDiv.input2Y
	legManualSquashStretchNormalize_1_plsMns.output3Dz >> legManualSquashStretchRotAmt_1_multDiv.input2Z


	legSquashStretch_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquashStretch_1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfUpLeg_SsScale', legSquashStretch_1_multDiv+'.input1X')
		pm.connectAttr('rigSettings_ctrl.lfUpLegSplit_1_SsScale', legSquashStretch_1_multDiv+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.lfUpLegSplit_2_SsScale', legSquashStretch_1_multDiv+'.input1Z')
	else:
		pm.connectAttr('rigSettings_ctrl.rtUpLeg_SsScale', legSquashStretch_1_multDiv+'.input1X')
		pm.connectAttr('rigSettings_ctrl.rtUpLegSplit_1_SsScale', legSquashStretch_1_multDiv+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.rtUpLegSplit_2_SsScale', legSquashStretch_1_multDiv+'.input1Z')


	legScale_cond.outColorR >> legSquashStretch_1_multDiv.input2X
	legScale_cond.outColorR >> legSquashStretch_1_multDiv.input2Y
	legScale_cond.outColorR >> legSquashStretch_1_multDiv.input2Z


	legSquashStretchAutoEnable1_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'legSquashStretchAutoEnable1_blend', param={} )

	foot_ctrl.autoSquashStretch >> legSquashStretchAutoEnable1_blend.blender
	legSquashStretch_1_multDiv.outputX >> legSquashStretchAutoEnable1_blend.color1R
	legSquashStretch_1_multDiv.outputY >> legSquashStretchAutoEnable1_blend.color1G
	legSquashStretch_1_multDiv.outputZ >> legSquashStretchAutoEnable1_blend.color1B
	legManualSquashStretchRotAmt_1_multDiv.outputX >> legSquashStretchAutoEnable1_blend.color2R
	legManualSquashStretchRotAmt_1_multDiv.outputY >> legSquashStretchAutoEnable1_blend.color2G
	legManualSquashStretchRotAmt_1_multDiv.outputZ >> legSquashStretchAutoEnable1_blend.color2B



	legSquashStretchFinalVal_1_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legSquashStretchFinalVal_1_plsMns', param={'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )

	legSquashStretchAutoEnable1_blend.outputR >> legSquashStretchFinalVal_1_plsMns.input3D[1].input3Dx
	legSquashStretchAutoEnable1_blend.outputG >> legSquashStretchFinalVal_1_plsMns.input3D[1].input3Dy
	legSquashStretchAutoEnable1_blend.outputB >> legSquashStretchFinalVal_1_plsMns.input3D[1].input3Dz

	##########################OUT##########################
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dx', 'up_leg'+sidePrefix+'_weight_jnt1.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dx', 'up_leg'+sidePrefix+'_weight_jnt1.scaleZ' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dy', 'up_leg'+sidePrefix+'_weight_jnt2.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dy', 'up_leg'+sidePrefix+'_weight_jnt2.scaleZ' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dz', 'up_leg'+sidePrefix+'_weight_jnt3.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns+'.output3D.output3Dz', 'up_leg'+sidePrefix+'_weight_jnt3.scaleZ' )




	legManualSquashStretchNormalize_1_plsMns1 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legManualSquashStretchNormalize_1_plsMns1', param={'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )

	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dx
	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dy
	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dz



	legManualSquashStretchRotAmt_1_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legManualSquashStretchRotAmt_1_multDiv1', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfKnee_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1X')
		pm.connectAttr('rigSettings_ctrl.lfLowLegSplit_1_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.lfLowLegSplit_2_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1Z')
	else:
		pm.connectAttr('rigSettings_ctrl.rtKnee_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1X')
		pm.connectAttr('rigSettings_ctrl.rtLowLegSplit_1_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.rtLowLegSplit_2_SsScale', legManualSquashStretchRotAmt_1_multDiv1+'.input1Z')


	legManualSquashStretchNormalize_1_plsMns1.output3Dx >> legManualSquashStretchRotAmt_1_multDiv1.input2X
	legManualSquashStretchNormalize_1_plsMns1.output3Dy >> legManualSquashStretchRotAmt_1_multDiv1.input2Y
	legManualSquashStretchNormalize_1_plsMns1.output3Dz >> legManualSquashStretchRotAmt_1_multDiv1.input2Z


	legSquashStretch_1_multDiv1 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquashStretch_1_multDiv1', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfKnee_SsScale', legSquashStretch_1_multDiv1+'.input1X')
		pm.connectAttr('rigSettings_ctrl.lfLowLegSplit_1_SsScale', legSquashStretch_1_multDiv1+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.lfLowLegSplit_2_SsScale', legSquashStretch_1_multDiv1+'.input1Z')
	else:
		pm.connectAttr('rigSettings_ctrl.rtKnee_SsScale', legSquashStretch_1_multDiv1+'.input1X')
		pm.connectAttr('rigSettings_ctrl.rtLowLegSplit_1_SsScale', legSquashStretch_1_multDiv1+'.input1Y')
		pm.connectAttr('rigSettings_ctrl.rtLowLegSplit_2_SsScale', legSquashStretch_1_multDiv1+'.input1Z')


	legScale_cond.outColorR >> legSquashStretch_1_multDiv1.input2X
	legScale_cond.outColorR >> legSquashStretch_1_multDiv1.input2Y
	legScale_cond.outColorR >> legSquashStretch_1_multDiv1.input2Z



	legSquashStretchAutoEnable1_blend1 = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'legSquashStretchAutoEnable1_blend1', param={} )

	foot_ctrl.autoSquashStretch >> legSquashStretchAutoEnable1_blend1.blender
	legSquashStretch_1_multDiv1.outputX >> legSquashStretchAutoEnable1_blend1.color1R
	legSquashStretch_1_multDiv1.outputY >> legSquashStretchAutoEnable1_blend1.color1G
	legSquashStretch_1_multDiv1.outputZ >> legSquashStretchAutoEnable1_blend1.color1B
	legManualSquashStretchRotAmt_1_multDiv1.outputX >> legSquashStretchAutoEnable1_blend1.color2R
	legManualSquashStretchRotAmt_1_multDiv1.outputY >> legSquashStretchAutoEnable1_blend1.color2G
	legManualSquashStretchRotAmt_1_multDiv1.outputZ >> legSquashStretchAutoEnable1_blend1.color2B


	legSquashStretchFinalVal_1_plsMns1 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legSquashStretchFinalVal_1_plsMns1', param={'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )

	legSquashStretchAutoEnable1_blend1.outputR >> legSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dx
	legSquashStretchAutoEnable1_blend1.outputG >> legSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dy
	legSquashStretchAutoEnable1_blend1.outputB >> legSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dz


	##########################OUT##########################
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dx', 'knee'+sidePrefix+'_weight_jnt1.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dx', 'knee'+sidePrefix+'_weight_jnt1.scaleZ' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dy', 'knee'+sidePrefix+'_weight_jnt2.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dy', 'knee'+sidePrefix+'_weight_jnt2.scaleZ' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dz', 'knee'+sidePrefix+'_weight_jnt3.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns1+'.output3D.output3Dz', 'knee'+sidePrefix+'_weight_jnt3.scaleZ' )





	legManualSquashStretchNormalize_1_plsMns2 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legManualSquashStretchNormalize_1_plsMns2', param={'operation':2,'input3D[0].input3Dx':1,'input3D[1].input3Dx':1} )

	foot_ctrl.squashStretch >> legManualSquashStretchNormalize_1_plsMns2.input3D[0].input3Dx


	legManualSquashStretchRotAmt_1_multDiv2 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legManualSquashStretchRotAmt_1_multDiv2', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfAnkle_SsScale', legManualSquashStretchRotAmt_1_multDiv2+'.input1X')
	else:
		pm.connectAttr('rigSettings_ctrl.rtAnkle_SsScale', legManualSquashStretchRotAmt_1_multDiv2+'.input1X')

	legManualSquashStretchNormalize_1_plsMns2.output3Dx >> legManualSquashStretchRotAmt_1_multDiv2.input2X



	legSquashStretch_1_multDiv2 = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legSquashStretch_1_multDiv2', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfAnkle_SsScale', legSquashStretch_1_multDiv2+'.input1X')
	else:
		pm.connectAttr('rigSettings_ctrl.rtAnkle_SsScale', legSquashStretch_1_multDiv2+'.input1X')

	legScale_cond.outColorR >> legSquashStretch_1_multDiv2.input2X


	legSquashStretchAutoEnable1_blend2 = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'legSquashStretchAutoEnable1_blend2', param={} )

	foot_ctrl.autoSquashStretch >> legSquashStretchAutoEnable1_blend2.blender
	legSquashStretch_1_multDiv2.outputX >> legSquashStretchAutoEnable1_blend2.color1R
	legManualSquashStretchRotAmt_1_multDiv2.outputX >> legSquashStretchAutoEnable1_blend2.color2R


	legSquashStretchFinalVal_1_plsMns2 = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'legSquashStretchFinalVal_1_plsMns2', param={'operation':1,'input3D[0].input3Dx':1,'input3D[1].input3Dx':1} )

	legSquashStretchAutoEnable1_blend2.outputR >> legSquashStretchFinalVal_1_plsMns2.input3D[1].input3Dx

	##########################OUT##########################
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns2+'.output3D.output3Dx', 'ankle'+sidePrefix+'_weight_jnt1.scaleY' )
	pm.connectAttr( legSquashStretchFinalVal_1_plsMns2+'.output3D.output3Dx', 'ankle'+sidePrefix+'_weight_jnt1.scaleZ' )



	####################################low leg Twist Connection####################################

	lowLegTwistReverser_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'lowLegTwistReverser_plsMns', param={'operation':2,'input1D[0]':0} )
	lowLegAimPvTwist_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'lowLegAimPvTwist_blend', param={} )

	lowLegPvTwistReader_loc.rotateX >> lowLegTwistReverser_plsMns.input1D[1]


	foot_ctrl.useAimTwist >> lowLegAimPvTwist_blend.blender
	lowLegTwist_loc.rotateX >> lowLegAimPvTwist_blend.color1R
	lowLegPvTwistReader_loc.readerRot >> lowLegAimPvTwist_blend.color2R

	lowLegTwistReverser_plsMns.output1D >> lowLegPvTwistReader_loc.readerRot



	lowLegManualRotCtrl_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'lowLegManualRotCtrl_blend', param={} )

	foot_ctrl.lowLegTwistCtrl >> lowLegManualRotCtrl_blend.blender
	lowLegTwist_ctrl.rotateX >> lowLegManualRotCtrl_blend.color1R
	lowLegAimPvTwist_blend.outputR >> lowLegManualRotCtrl_blend.color2R


	lowLegSplitRx1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'lowLegSplitRx1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	lowLegManualRotCtrl_blend.outputR >> lowLegSplitRx1_multDiv.input1X
	lowLegManualRotCtrl_blend.outputR >> lowLegSplitRx1_multDiv.input1Y

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftLowLeg_1', lowLegSplitRx1_multDiv+'.input2X')
		pm.connectAttr('rigSettings_ctrl.leftLowLeg_2', lowLegSplitRx1_multDiv+'.input2Y')
	else:
		pm.connectAttr('rigSettings_ctrl.rightLowLeg_1', lowLegSplitRx1_multDiv+'.input2X')
		pm.connectAttr('rigSettings_ctrl.rightLowLeg_2', lowLegSplitRx1_multDiv+'.input2Y')



	##########################OUT##########################
	lowLegSplitRx1_multDiv.outputX >> kneeCtrlSplit_1_jnt.rotateX
	lowLegSplitRx1_multDiv.outputY >> kneeCtrlSplit_2_jnt.rotateX


	####################################up leg Twist Connection####################################


	upLegMidJntTwistReverser_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'upLegMidJntTwistReverser_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


	upLegMidJntRzReader_loc.rotateX >> upLegMidJntTwistReverser_multDiv.input1X



	upLegTwistScalar_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'upLegTwistScalar_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


	upLegTwistReader_loc.rotateX >> upLegTwistScalar_multDiv.input1X
	upLegMidJntTwistReverser_multDiv.outputX >> upLegTwistScalar_multDiv.input1Y
	foot_ctrl.upLimbTwistScalar >> upLegTwistScalar_multDiv.input2X
	foot_ctrl.midJntUpLimbTwistScalar >> upLegTwistScalar_multDiv.input2Y


	upLegManualTwist_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'upLegManualTwist_plsMns', param={'operation':1} )


	upLegTwistScalar_multDiv.outputX >> upLegManualTwist_plsMns.input1D[0]
	foot_ctrl.upLimbTwist >> upLegManualTwist_plsMns.input1D[1]


	upLegManualTwist_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'upLegManualTwist_blend', param={} )

	foot_ctrl.upLimbAutoTwist >> upLegManualTwist_blend.blender
	upLegManualTwist_plsMns.output1D >> upLegManualTwist_blend.color1R
	foot_ctrl.upLimbTwist >> upLegManualTwist_blend.color2R


	upLegMidJntTwistReverser_plsMns = DPTnode.createNodeWithParam(node_type='plusMinusAverage', name=sidePrefix+'upLegMidJntTwistReverser_plsMns', param={'operation':1} )

	upLegTwistScalar_multDiv.outputY >> upLegMidJntTwistReverser_plsMns.input1D[0]
	upLegManualTwist_blend.outputR >> upLegMidJntTwistReverser_plsMns.input1D[1]



	upLegSplitRx1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'upLegSplitRx1_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )


	upLegMidJntTwistReverser_plsMns.output1D >> upLegSplitRx1_multDiv.input1X
	upLegMidJntTwistReverser_plsMns.output1D >> upLegSplitRx1_multDiv.input1Y
	upLegMidJntTwistReverser_plsMns.output1D >> upLegSplitRx1_multDiv.input1Z

	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.leftUpLeg', upLegSplitRx1_multDiv+'.input2X')
		pm.connectAttr('rigSettings_ctrl.leftUpLeg_2', upLegSplitRx1_multDiv+'.input2Y')
		pm.connectAttr('rigSettings_ctrl.leftUpLeg_3', upLegSplitRx1_multDiv+'.input2Z')
	else:
		pm.connectAttr('rigSettings_ctrl.rightUpLeg', upLegSplitRx1_multDiv+'.input2X')
		pm.connectAttr('rigSettings_ctrl.rightUpLeg_2', upLegSplitRx1_multDiv+'.input2Y')
		pm.connectAttr('rigSettings_ctrl.rightUpLeg_3', upLegSplitRx1_multDiv+'.input2Z')

	##########################OUT##########################
	upLegSplitRx1_multDiv.outputY >> upLegCtrl_jnt.rotateX
	upLegSplitRx1_multDiv.outputY >> upLegCtrlSplit_1_jnt.rotateX
	upLegSplitRx1_multDiv.outputZ >> upLegCtrlSplit_2_jnt.rotateX




	legRhJntRx0_reverse_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'legRhJntRx0_reverse_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0,'input2X':-1,'input2Y':-1,'input2Z':-1} )

	upLegManualTwist_blend.outputR >> legRhJntRx0_reverse_multDiv.input1X
	upLegManualTwist_blend.outputR >> legRhJntRx0_reverse_multDiv.input1Y
	upLegManualTwist_blend.outputR >> legRhJntRx0_reverse_multDiv.input1Z


	##########################OUT##########################
	legRhJntRx0_reverse_multDiv.outputX >> upLegCtrl_jnt_zeroFrzGrp.rotateX
	legRhJntRx0_reverse_multDiv.outputY >> upLegCtrlSplit_1_jnt_zeroFrzGrp.rotateX
	legRhJntRx0_reverse_multDiv.outputZ >> upLegCtrlSplit_2_jnt_zeroFrzGrp.rotateX




	####################################RubberHose Connection####################################

	upLegSplit_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'upLegSplit_1_multDiv', param={'operation':1,'input1X':0.5,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	pm.connectAttr(knee+'.translateX', upLegSplit_1_multDiv+'.input2X' )
	upLegSplit_1_multDiv.outputX >> upLegRubberHoseCtrl_jnt_frzGrp.translateX


	lowLegSplit_1_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'lowLegSplit_1_multDiv', param={'operation':1,'input1X':0.5,'input1Y':0,'input1Z':0,'input2X':1,'input2Y':1,'input2Z':1} )

	pm.connectAttr(ankle+'.translateX', lowLegSplit_1_multDiv+'.input2X' )

	##########################OUT##########################
	lowLegSplit_1_multDiv.outputX >> lowLegRubberHoseCtrl_jnt_frzGrp.translateX



	kneeSoftness_multDiv = DPTnode.createNodeWithParam(node_type='multiplyDivide', name=sidePrefix+'kneeSoftness_multDiv', param={'operation':1,'input1X':0,'input1Y':0,'input1Z':0} )

	kneeSoftness_multDiv.setAttr('input2X', upLegDistVal*-0.5)
	kneeSoftness_multDiv.setAttr('input2Y', upLegDistVal*0.5)
	'''
	if DPTStr.isContain(sidePrefix, 'L'):
		kneeSoftness_multDiv.setAttr('input2X', upLegDistVal*-0.5)
		kneeSoftness_multDiv.setAttr('input2Y', upLegDistVal*0.5)
	else:
		kneeSoftness_multDiv.setAttr('input2X', upLegDistVal*0.5)
		kneeSoftness_multDiv.setAttr('input2Y', upLegDistVal*-0.5)
	'''


	foot_ctrl.kneeSoftness >> kneeSoftness_multDiv.input1X
	foot_ctrl.kneeSoftness >> kneeSoftness_multDiv.input1Y

	##########################OUT##########################
	kneeSoftness_multDiv.outputX >> legUpMidSoftJnt_jnt_zeroFrzGrp.translateX
	kneeSoftness_multDiv.outputY >> legDnMidSoftJnt_jnt_zeroFrzGrp.translateX






	####################################Base Joint Connection####################################



	##########################upLeg##########################

	#translate
	pm.pointConstraint( foot_hip_ctrl , upLeg )

	#orient
	baseOriCons = pm.orientConstraint( FKUplegJnt, IKupLegJnt, upLeg )

	fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
	ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

	legIkFkSwitch_reverse = DPTnode.createNodeWithParam(node_type='reverse', name=sidePrefix+'legIkFkSwitch_reverse', param={} )
	foot_ctrl.ikFkBlend >> legIkFkSwitch_reverse.inputX

	pm.connectAttr( legIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
	pm.connectAttr( foot_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )


	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfUpLegReaderRotOrder', upLeg+'.rotateOrder')
	else:
		pm.connectAttr('rigSettings_ctrl.rtUpLegReaderRotOrder', upLeg+'.rotateOrder')



	##########################knee##########################

	legTrans_blend = DPTnode.createNodeWithParam(node_type='blendColors', name=sidePrefix+'legTrans_blend', param={} )

	foot_ctrl.ikFkBlend >> legTrans_blend.blender
	IKKneeJnt.translateX >> legTrans_blend.color1R
	IKAnkleJnt.translateX >> legTrans_blend.color1G
	FKKneeJnt.translateX >> legTrans_blend.color2R
	FKAnkleJnt.translateX >> legTrans_blend.color2G

	#translate
	pm.connectAttr( legTrans_blend+'.outputR', knee+'.translateX' )

	#rotate
	baseOriCons = pm.orientConstraint( FKKneeJnt, IKKneeJnt, knee )
	fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
	ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

	pm.connectAttr( legIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
	pm.connectAttr( foot_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )


	if DPTStr.isContain(sidePrefix, 'L'):
		pm.connectAttr('rigSettings_ctrl.lfUpLegReaderRotOrder', knee+'.rotateOrder')
	else:
		pm.connectAttr('rigSettings_ctrl.rtUpLegReaderRotOrder', knee+'.rotateOrder')

	##########################ankle##########################

	#translate
	pm.connectAttr( legTrans_blend+'.outputG', ankle+'.translateX' )

	#rotate
	baseOriCons = pm.orientConstraint( FKAnkleJnt, ankleRevJnt, ankle )
	fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
	ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='rev', kwargs={'ud':True})

	pm.connectAttr( legIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
	pm.connectAttr( foot_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )



	##########################ball##########################

	#rotate
	baseOriCons = pm.orientConstraint( FKBallJnt, ballIKJnt, ball )
	fkAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='fk', kwargs={'ud':True})
	ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

	pm.connectAttr( legIkFkSwitch_reverse+'.outputX', baseOriCons+'.'+fkAttr )
	pm.connectAttr( foot_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )


	##########################Toe##########################
	#rotate
	baseOriCons = pm.orientConstraint( toeIkJnt, toe )
	ikAttr = DPTattr.getMatchNameAttribute(node=baseOriCons, matchStr='ik', kwargs={'ud':True})

	pm.connectAttr( foot_ctrl+'.ikFkBlend', baseOriCons+'.'+ikAttr )

	##########################weight joint##########################

	pm.parentConstraint(upLegCtrl_jnt, 'up_leg'+sidePrefix+'_weight_jnt1', maintainOffset=True)
	pm.parentConstraint(upLegCtrlSplit_1_jnt, 'up_leg'+sidePrefix+'_weight_jnt2', maintainOffset=True)
	pm.parentConstraint(upLegCtrlSplit_2_jnt, 'up_leg'+sidePrefix+'_weight_jnt3', maintainOffset=True)

	pm.parentConstraint(kneeCtrl_jnt, 'knee'+sidePrefix+'_weight_jnt1', maintainOffset=True)
	pm.parentConstraint(kneeCtrlSplit_1_jnt, 'knee'+sidePrefix+'_weight_jnt2', maintainOffset=True)
	pm.parentConstraint(kneeCtrlSplit_2_jnt, 'knee'+sidePrefix+'_weight_jnt3', maintainOffset=True)
	pm.parentConstraint(ankleCtrl_jnt, 'ankle'+sidePrefix+'_weight_jnt1', maintainOffset=True)


	#update curve length
	scaleAdjustedDefaultArcLen_multDiv.setAttr('input1X', pm.arclen( legCurve ) )






