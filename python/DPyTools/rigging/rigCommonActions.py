import pymel.core as pm

import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.transformActions as DPTtrans

'''
to run:
>>> import DPyTools.rigging.rigCommonActions as DPTrigCAct
>>> DPTHumanRig.openWin()

'''

def createRigController(ctlObj=None, name='ctlobj', size=5.0, position=None):

	if ctlObj is None:
		ctlObj, shape = pm.circle( nr=(0, 1, 0) ,name=name, r=size/2.0)

	if position is not None:
		pm.move(ctlObj, position)

	return ctlObj


def createRigControllerWithAlign(ctlObj=None, name='ctlobj', size=5.0, alignObj=None):

	rig = createRigController(ctlObj, name, size)

	DPTtrans.setTransformToObjectZero( srcObj=rig, targetObj=alignObj, isTrans=True, isRot=True, isScale=False, isUnParent=True)

	return rig


def createFreezeRig(baseObject=None, position=(0,0,0), parent=None, isAlign=False):

	baseObject_frzGrp = DPTgrp.createEmpty( baseObject+'_frzGrp', parent, position=position)
	baseObject_zeroFrzGrp = DPTgrp.createEmpty( baseObject+'_zeroFrzGrp', baseObject_frzGrp, position=position )
	pm.parent( baseObject, baseObject_zeroFrzGrp )

	if isAlign:
		DPTattr.setMultiAttributes(baseObject_frzGrp,{'translateX':0,'translateY':0,'translateZ':0,'rotateX':0,'rotateY':0,'rotateZ':0} )
		DPTattr.setMultiAttributes(baseObject_zeroFrzGrp,{'translateX':0,'translateY':0,'translateZ':0,'rotateX':0,'rotateY':0,'rotateZ':0} )
		DPTattr.setMultiAttributes(baseObject,{'translateX':0,'translateY':0,'translateZ':0,'rotateX':0,'rotateY':0,'rotateZ':0} )

	return baseObject_frzGrp, baseObject_zeroFrzGrp


def createFreezeRigWithAlign(baseObject=None, parent=None, isTrans=True, isRot=True, isScale=False ):

	baseObject_frzGrp = DPTgrp.createEmptyWithAlign(name=baseObject+'_frzGrp', alignObj=baseObject, isTrans=isTrans, isRot=isRot, isScale=isScale, isUnParent=True)
	baseObject_zeroFrzGrp = DPTgrp.createEmptyWithAlign(name=baseObject+'_zeroFrzGrp', alignObj=baseObject, isTrans=isTrans, isRot=isRot, isScale=isScale, isUnParent=True)
	pm.parent( baseObject_zeroFrzGrp, baseObject_frzGrp )
	pm.parent( baseObject, baseObject_zeroFrzGrp )

	if parent is not None:
		pm.parent( baseObject_frzGrp, parent )

	return baseObject_frzGrp, baseObject_zeroFrzGrp


def createRigDistanceSet(start=None, end=None, name='strDist', parent=None):

	distShape= pm.createNode( 'distanceDimShape', n=name+'_distShape' )
	distTrans = distShape.getParent()



	dist_sp = pm.spaceLocator(n=name+'_dist_sp')
	dist_ep = pm.spaceLocator(n=name+'_dist_ep')

	dist_sp.getShape().worldPosition >> distShape.startPoint
	dist_ep.getShape().worldPosition >> distShape.endPoint

	if parent is not None:
		pm.parent(distTrans,parent)
		pm.parent(dist_sp,parent)
		pm.parent(dist_ep,parent)

	pm.pointConstraint(start, dist_sp)
	pm.pointConstraint(end, dist_ep)

	distTrans.hide()
	dist_sp.hide()
	dist_ep.hide()

	return distTrans, distShape, dist_sp, dist_ep


def createFKControl(grpTop='', partJnt=None, partName='part1',sidePrefix='L',  isSecondary=False):
	part_fk_ctrl = createRigControllerWithAlign(ctlObj=None, name=sidePrefix+partName+'_fk_ctrl', size=8.0, alignObj=partJnt)
	#part_fk_ctrl = createRigController(ctlObj=None, name=sidePrefix+partName+'_fk_ctrl', size=8.0, position=pm.xform(partJnt, q=True, ws=True, t=True ))
	#DPTtrans.setTransformToObjectZero(part_fk_ctrl, partJnt, True, True, False)
	part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp = createFreezeRig(baseObject=part_fk_ctrl, parent=partJnt, position=pm.xform(partJnt, q=True, ws=True, t=True ), isAlign=True)
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
		part_sec_fk_ctrl = createRigController(ctlObj=None, name=sidePrefix+partName+'_sec_fk_ctrl', size=3.0)
		DPTtrans.setTransformToObjectZero(part_sec_fk_ctrl, partJnt, True, True, False)
		pm.parent(part_sec_fk_ctrl, part_sec_fk_ctrl_zeroFrzGrp)
		DPTattr.hideAttributes(part_sec_fk_ctrl, ['translateX','translateY','translateZ','scaleX','scaleY','scaleZ','visibility'])
		part_sec_fk_ctrl.translate.lock()
		part_sec_fk_ctrl.scale.lock()
		return part_fk_ctrl, part_fk_ctrl_frzGrp_TOP, part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp, part_sec_fk_ctrl, part_sec_fk_ctrl_zeroFrzGrp

	return part_fk_ctrl, part_fk_ctrl_frzGrp_TOP, part_fk_ctrl_frzGrp, part_fk_ctrl_zeroFrzGrp