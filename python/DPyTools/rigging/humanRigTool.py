import pymel.core as pm
import pymel.util as pmu

import DPyTools.curve.curveTool as DPTCurveTool

import DPyTools.general.follicleActions as DPTfollicle
import DPyTools.general.stringActions as DPTStr
import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.nodeActions as DPTnode
import DPyTools.general.transformActions as DPTtrans


import DPyTools.rigging.jointActions as DPTJoint

import DPyTools.rigging.humanArmRig as DPTHArm
import DPyTools.rigging.humanHeadRig as DPTHHead
import DPyTools.rigging.humanRigActions as DPThAct
import DPyTools.rigging.rigCommonActions as DPTrigCAct
from DPyTools.features.rigging.human import specs as human_specs
from DPyTools.ui import windows as ui_windows
from DPyTools.scene import selection as scene_selection
from DPyTools.ui import table_selection



'''
to run:
	>>> import DPyTools.rigging.humanRigTool as DPTHumanRig
	>>> DPTHumanRig.openWin()

'''

#CHARACTOR GRP
CharaGroupTop = 'char_top_grp'


#DUMMY GRP
DummyRigName = 'dummy_rig_grp'

#JOINT GRP
JointGrpName = 'joints_grp'
spineWeight_grp_Name = 'splineWeight_grp'
LlegWeightJnt_grp_Name = 'LlegWeightJnt_grp'
RlegWeightJnt_grp_Name = 'RlegWeightJnt_grp'
LarmWeightJnt_grp_Name = 'LarmWeightJnt_grp'
RarmWeightJnt_grp_Name = 'RarmWeightJnt_grp'

#RIG GRP
RigGrpName = 'rig_grp'
root_ctrlName = 'root_ctrl'
master_ctrlName = 'master_ctrl'
subMaster_ctrlName = 'subMaster_ctrl'


LegRigGroupName = 'leg_rig_grp'




humanBaseHeight = 170
hipHeight = humanBaseHeight/2
headHeight = 25
neckheight = 10
shoulderHeight = humanBaseHeight-headHeight-neckheight

footWidth = 15
ankleheight = 5
kneeHeight = ankleheight+35
legHeight = kneeHeight+40

LFootStrList = [('up_legL',(footWidth,legHeight,0)),('kneeL',(footWidth,kneeHeight,5)),('ankleL',(footWidth,ankleheight,0))]
RFootStrList = list(human_specs.mirrored_position_items(LFootStrList))
AnkleStrList = {'ball':(0,0,14),'toe':(footWidth,0,28),'heel':(0,0,-3)}
LAnkleList = list(human_specs.LEFT_ANKLE)
RAnkleList = list(human_specs.RIGHT_ANKLE)
LFootJointList = list(human_specs.LEFT_FOOT)
RFootJointList = list(human_specs.RIGHT_FOOT)


clavicleX = 3
upArmX = clavicleX+15
elbowX = upArmX+25
wristX = elbowX+25
LArmStrList = [('clavicleL',(clavicleX,shoulderHeight,-5)),('up_armL',(upArmX,shoulderHeight,0)),('elbowL',(elbowX,shoulderHeight,-5)),('wristL',(wristX,shoulderHeight,0))]
RArmStrList = list(human_specs.mirrored_position_items(LArmStrList))
LArmJointList = list(human_specs.LEFT_ARM)
RArmJointList = list(human_specs.RIGHT_ARM)
LHandJointList = list(human_specs.LEFT_HAND)
RHandJointList = list(human_specs.RIGHT_HAND)

splineHeight = abs((shoulderHeight-5-hipHeight)/4)
SplineStrList = [('hip',(0,hipHeight,0)),('spline_low',(0,hipHeight+splineHeight,0)),('spline_mid',(0,hipHeight+splineHeight*2,-0.5)),('spline_hi',(0,hipHeight+splineHeight*3,-1.5)),('spline_end',(0,hipHeight+splineHeight*4,-2.5))]
NeckStrList = [('neck',(0,shoulderHeight+5,-2.5)),('head',(0,shoulderHeight+5+neckheight,0)),('head_tip',(0,shoulderHeight+5+neckheight+headHeight,0))]
SplineJointList = ['spline_low','spline_mid','spline_hi','spline_end']
HipJointList = ['hip']
HeadJointList = ['neck','head','head_tip']

realClavicleLength = 10
LClavicleList = [('real_clavicleL',(clavicleX,shoulderHeight,6)),('real_clavicle_tipL',(clavicleX+realClavicleLength,shoulderHeight,3))]
RClavicleList = list(human_specs.mirrored_position_items(LClavicleList))
LClavicleJointList = list(human_specs.LEFT_CLAVICLE)
RClavicleJointList = list(human_specs.RIGHT_CLAVICLE)

hendLength = 5
fingurLength = 2
fingurWidth = 2
thumbX = wristX+hendLength/2
fingerAlength = wristX+hendLength
fingerBlength = wristX+hendLength+fingurLength
fingerClength = wristX+hendLength+fingurLength*2
fingerDlength = wristX+hendLength+fingurLength*3

#center is middle finger
LThumbList = [('thumb_a_L',(thumbX,shoulderHeight,fingurWidth*2)),('thumb_b_L',(thumbX+fingurLength,shoulderHeight,fingurWidth*2)),('thumb_c_L',(thumbX+fingurLength*2,shoulderHeight,fingurWidth*2)),('thumb_d_L',(thumbX+fingurLength*3,shoulderHeight,fingurWidth*2))]
LIndexList = [('index_a_L',(fingerAlength,shoulderHeight,fingurWidth)),('index_b_L',(fingerBlength,shoulderHeight,fingurWidth)),('index_c_L',(fingerClength,shoulderHeight,fingurWidth)),('index_d_L',(fingerDlength,shoulderHeight,fingurWidth))]
LMiddleList = [('middle_a_L',(fingerAlength,shoulderHeight,0)),('middle_b_L',(fingerBlength,shoulderHeight,0)),('middle_c_L',(fingerClength,shoulderHeight,0)),('middle_d_L',(fingerDlength,shoulderHeight,0))]
LRingList = [('ring_a_L',(fingerAlength,shoulderHeight,-fingurWidth)),('ring_b_L',(fingerBlength,shoulderHeight,-fingurWidth)),('ring_c_L',(fingerClength,shoulderHeight,-fingurWidth)),('ring_d_L',(fingerDlength,shoulderHeight,-fingurWidth))]
LPinkyList = [('pinky_a_L',(fingerAlength,shoulderHeight,-fingurWidth*2)),('pinky_b_L',(fingerBlength,shoulderHeight,-fingurWidth*2)),('pinky_c_L',(fingerClength,shoulderHeight,-fingurWidth*2)),('pinky_d_L',(fingerDlength,shoulderHeight,-fingurWidth*2))]

RThumbList = list(human_specs.mirrored_position_items(LThumbList))
RIndexList = list(human_specs.mirrored_position_items(LIndexList))
RMiddleList = list(human_specs.mirrored_position_items(LMiddleList))
RRingList = list(human_specs.mirrored_position_items(LRingList))
RPinkyList = list(human_specs.mirrored_position_items(LPinkyList))

LThumbJointList = list(human_specs.LEFT_THUMB)
LIndexJointList = list(human_specs.LEFT_INDEX)
LMiddleJointList = list(human_specs.LEFT_MIDDLE)
LRingJointList = list(human_specs.LEFT_RING)
LPinkyJointList = list(human_specs.LEFT_PINKY)

RThumbJointList = list(human_specs.RIGHT_THUMB)
RIndexJointList = list(human_specs.RIGHT_INDEX)
RMiddleJointList = list(human_specs.RIGHT_MIDDLE)
RRingJointList = list(human_specs.RIGHT_RING)
RPinkyJointList = list(human_specs.RIGHT_PINKY)


dummyLFoot='LFoot'
dummyRFoot='RFoot'
dummyLArm='LArm'
dummyRArm='RArm'
dummyLHand='LHand'
dummyRHand='RHand'
dummySpline='Spline'
dummyHead='Head'
dummyLClavicle='LRealClavicle'
dummyRClavicle='RRealClavicle'
dummyPartsList=[dummyLFoot,dummyRFoot,dummyLArm,dummyRArm,dummyLHand,dummyRHand,dummySpline,dummyHead,dummyLClavicle,dummyRClavicle]


def _confirm_action(title, message, action_label):
	result = pm.confirmDialog(
		title=title, message=message,
		button=[action_label, 'Cancel'],
		defaultButton='Cancel', cancelButton='Cancel', dismissString='Cancel')
	return result == action_label

#check group methods
def checkCharaGroup():
	if not pm.objExists(CharaGroupTop):
		rootTrans= pm.createNode( 'transform', n=CharaGroupTop )

def checkRigGroup():
	checkCharaGroup()
	if not pm.objExists(RigGrpName):
		rootTrans= pm.createNode( 'transform', n=RigGrpName )
		pm.parent(RigGrpName, CharaGroupTop)


#set template object attribute
def setTemplateObjAttr(targetTrans=None):
	targetTrans.setAttr('overrideEnabled', 1)
	targetTrans.setAttr('overrideShading', 0)
	targetTrans.setAttr('overrideTexturing', 0)
	targetTrans.setAttr('overrideColor', 17)#17 is yellow


#create cube and annotation
def createTemplateCube(name="emptyCube",position=(0,0,0), cubeSize=2):

	objTrans, cubeShape = pm.polyCube( w=cubeSize, h=cubeSize, d=cubeSize, n=name )

	objTrans.setAttr('displayHandle', 1)
	setTemplateObjAttr(objTrans)
	objTrans.setAttr('translate', position)
	objTrans.rotate.lock()
	objTrans.scale.lock()

	anShape= pm.createNode( 'annotationShape', n=name+'AnoTx' )
	anShape.setAttr('displayArrow', 0)
	anShape.setAttr('text', name)

	anTrans = anShape.getParent()
	anTrans.setAttr('hiddenInOutliner', 1)

	pm.parent( anTrans, objTrans, relative=True )#relative is move to cube position
	return objTrans


def createLinePlane(name='linepln1', parent=None, target=None):

	if parent is None or target is None:
		return

	objTrans, planeShape = pm.polyPlane( w=0.0001, h=1, sx=1, sy=1, ax=(0,0,1), cuv=0)
	setTemplateObjAttr(objTrans)
	objTrans.setAttr('overrideColor', 0)

	pm.move(objTrans, [0,0.5,0])
	pm.makeIdentity( objTrans, apply=True, t=1, r=1, s=1, n=0, pn=1 )
	pm.runtime.ResetTransformations(objTrans)
	pm.runtime.DeleteHistory(objTrans)

	pm.parent( objTrans, parent, relative=True )#relative is move to cube position

	#create constraint
	pm.aimConstraint( target, objTrans, aim=(0, 1, 0), upVector=(1, 0, 0), worldUpVector=(0, 1, 0), worldUpType='vector',  weight =1.0 )


	exp = 'float $base[3] = `xform -q -ws -t '+parent+'`;\nfloat $target[3] = `xform -q -ws -t '+target+'`;\n\n\nfloat $transX = '+target+'.translateX;\n$transX = '+target+'.translateY;\n$transX = '+target+'.translateZ;\n\nvector $targetVec = <<$target[0]-$base[0],$target[1]-$base[1],$target[2]-$base[2]>>;\n'+objTrans+'.scaleY = mag($targetVec);'
	pm.expression( s=exp, ae=1, uc='all', o= objTrans )

	return objTrans


#create pole vec sphere
def addPoleVecSphere( parent=None, name='pole1', polePosition=(0,1,0) ):
	objTrans, sphereShape = pm.polySphere( sx=8, sy=4, r=0.5, n=name)
	objTrans.setAttr('displayHandle', 1)
	setTemplateObjAttr(objTrans)
	objTrans.setAttr('translate', polePosition)

	objTrans.rotate.lock()
	objTrans.scale.lock()

	crvTrans = DPTCurveTool.createLinearCurve( name=name+'Crv', points=[(0,0,0),(0,3,0)] )
	crvShape = crvTrans.getShape()

	if parent is not None:
		pm.parent( objTrans, parent, relative=True )
		pm.parent( crvTrans, parent, relative=True )

		pm.connectAttr( objTrans+'.translate', "%s.controlPoints[1]" % (crvShape) )

	return objTrans


#create symmetry expression
def createSymmetryExp(LList=None, RList=None):
	if LList is None:
		LList = []
	if RList is None:
		RList = []
	for i in range(len(LList)):
		exp = '%(RItem)s.translateX = -%(LItem)s.translateX;\n%(RItem)s.translateY = %(LItem)s.translateY;\n%(RItem)s.translateZ = %(LItem)s.translateZ;' % {"RItem": RList[i], "LItem": LList[i]}
		pm.expression( s=exp, ae=1, uc='all', o=RList[i] )

		if LList[i] == 'ankleL':
			expRot = RList[i]+'.rotateY = -'+LList[i]+'.rotateY;'
			pm.expression( s=expRot, ae=1, uc='all', o=RList[i] )


#create spline
def createSplineSet(setName=None, dataList=None, cubeSize=2):
	if dataList is None:
		dataList = []

	if len(dataList) == 0:
		return None

	grpTrans= pm.createNode( 'transform', n=setName )

	pts = []
	cubeLists = []
	for i in dataList:
		newCube = createTemplateCube(name=i[0],position=i[1], cubeSize=cubeSize )
		cubeLists.append(newCube )
		pts.append(i[1])
		pm.parent( newCube, grpTrans)

	crvTrans = DPTCurveTool.createLinearCurve( name=setName+'Crv', points=pts )
	crvShape = crvTrans.getShape()

	for num in range(len(cubeLists)):
		pm.connectAttr( cubeLists[num]+'.translate', "%s.controlPoints[%d]" % (crvShape,num) )

	pm.parent( crvTrans, grpTrans)

	return grpTrans, cubeLists


#connect Curve
def createConnectCurve(name='empty', items=None, postions=None):
	if items is None:
		items = []
	if postions is None:
		postions = []

	crvTrans = DPTCurveTool.createLinearCurve( name=name+'Crv', points=postions )
	crvShape = crvTrans.getShape()

	for num in range(len(items)):
		pm.connectAttr( items[num]+'.translate', "%s.controlPoints[%d]" % (crvShape,num) )

	return crvTrans


#create ankle hierarchy
def createAnkleSet(topItem=None, ankleList=None, side='L', cubeSize=2, group=None):
	if ankleList is None:
		ankleList = {}

	cubes = []
	#create cube
	for key in ankleList:
		pos= ankleList[key]
		witdh = 0
		if side == 'L':
			witdh = footWidth
		else:
			witdh = -footWidth

		newCube = createTemplateCube( name=key+side, position=(witdh,pos[1],pos[2]), cubeSize=cubeSize )
		cubes.append(newCube)

	#create parent
	topItem.rotate.unlock()
	topItem.rotateX.lock()
	topItem.rotateZ.lock()
	pm.parent( 'ball'+side, topItem )
	pm.parent( 'toe'+side, 'ball'+side )
	pm.parent( 'heel'+side, 'ball'+side )

	#create line
	plane1 = createLinePlane(name='lineplnball'+side, parent=topItem, target='ball'+side)
	plane2 = createLinePlane(name='lineplntoe'+side, parent='ball'+side, target='toe'+side)
	plane3 = createLinePlane(name='lineplnheel'+side, parent='ball'+side, target='heel'+side)

	pm.parent( plane1, topItem )
	pm.parent( plane2, 'ball'+side )
	pm.parent( plane3, 'ball'+side )

	return cubes


def lockAnkleTranslate(items=None):
	if items is None:
		items = []

	#lock attribute
	for c in items:
		transObj = pm.ls(c)[0]
		transObj.rotate.lock()
		transObj.scale.lock()
		transObj.translateX.lock()
		if DPTStr.isContain(transObj.name(),'heel') or DPTStr.isContain(transObj.name(),'toe'):
			transObj.translateY.lock()


def createHandMidCube(handObj=None, rootFingers=None, nameList=None):
	if rootFingers is None:
		rootFingers = []
	if nameList is None:
		nameList = []
	worldHandPos = pm.xform(handObj, q=True, ws=True, t=True )

	cubes = []

	for i in range(len(rootFingers)):
		worldIndexPos = pm.xform(rootFingers[i], q=True, ws=True, t=True )
		posx = worldHandPos[0]+(worldIndexPos[0]-worldHandPos[0])*0.5
		posy = worldHandPos[1]+(worldIndexPos[1]-worldHandPos[1])*0.5
		posz = worldHandPos[2]+(worldIndexPos[2]-worldHandPos[2])*0.5
		cubes.append(createTemplateCube(nameList[i],(posx,posy,posz), 1))

	return cubes


#create dummy rig set
def SetupDummyRig(symmetry=True, isClavicle=True, replace_existing=False):
	print 'SetupDummyRig'
	print symmetry

	checkCharaGroup()

	if pm.objExists(DummyRigName):
		if not replace_existing:
			pm.warning('Dummy rig already exists; explicit replacement is required.')
			return False
		pm.delete( DummyRigName )


	rootTrans= pm.createNode( 'transform', n=DummyRigName )
	rootTrans.setAttr('displayHandle', 1)
	pm.parent(rootTrans,CharaGroupTop)

	baseCubeSize = 2


	#create dummy box and curves
	#foot
	LFootGrp, LFootBoxes = createSplineSet( dummyLFoot, LFootStrList, baseCubeSize )
	RFootGrp, RFootBoxes = createSplineSet( dummyRFoot, RFootStrList, baseCubeSize )

	LfootPole = addPoleVecSphere(LFootBoxes[0], 'Lfoot_pole', (0,0,15))
	RfootPole = addPoleVecSphere(RFootBoxes[0], 'Rfoot_pole', (0,0,15))


	#ankle
	createAnkleSet(LFootBoxes[-1], AnkleStrList, 'L' ,baseCubeSize, LFootGrp)
	createAnkleSet(RFootBoxes[-1], AnkleStrList, 'R' ,baseCubeSize, RFootGrp)


	#arm
	LArmGrp, LArmBoxes = createSplineSet( dummyLArm, LArmStrList, baseCubeSize )
	RArmGrp, RArmBoxes = createSplineSet( dummyRArm, RArmStrList, baseCubeSize )

	LupArmPole = addPoleVecSphere(LArmBoxes[1], 'Lup_arm_pole', (0,0,-15))
	RupArmPole = addPoleVecSphere(RArmBoxes[1], 'Rup_arm_pole', (0,0,-15))


	#handL
	LThumbGrp, LThumbBoxes = createSplineSet( 'Lthumb', LThumbList, 1 )
	LIndexGrp, LIndexBoxes = createSplineSet( 'Lindex', LIndexList, 1 )
	LMiddleGrp, LMiddleBoxes = createSplineSet( 'Lmiddle', LMiddleList, 1 )
	LRingGrp, LRingBoxes = createSplineSet( 'Lring', LRingList, 1 )
	LPinkyGrp, LPinkyBoxes = createSplineSet( 'Lpinky', LPinkyList, 1 )


	#handL Pole
	fingerPolePos = (0,3,0)
	LThumbPole = addPoleVecSphere(LThumbBoxes[0], 'Lthumb_pole', fingerPolePos)
	LIndexPole = addPoleVecSphere(LIndexBoxes[0], 'Lindex_pole', fingerPolePos)
	LMiddlePole = addPoleVecSphere(LMiddleBoxes[0],'Lmiddle_pole', fingerPolePos)
	LRingPole = addPoleVecSphere(LRingBoxes[0],'Lring_pole', fingerPolePos)
	LPinkyPole = addPoleVecSphere(LPinkyBoxes[0],'Lpinky_pole', fingerPolePos)


	LHandTrans= pm.createNode( 'transform', n=dummyLHand )
	pm.move( LArmBoxes[-1].translateX,LArmBoxes[-1].translateY,LArmBoxes[-1].translateZ, LHandTrans.scalePivot, LHandTrans.rotatePivot, absolute=True )
	pm.group( LThumbGrp, LIndexGrp, LMiddleGrp, LRingGrp, LPinkyGrp, parent=LHandTrans )

	#handL mid cube for hand
	LmidCubes = createHandMidCube(handObj=LArmBoxes[-1], rootFingers=[LIndexBoxes[0],LMiddleBoxes[0],LRingBoxes[0],LPinkyBoxes[0]], nameList=['Lindex_mid','Lmiddle_mid','Lring_mid','Lpinky_mid'])

	LThumbRootLCrv = createConnectCurve('Lthumb_root', [LArmBoxes[-1].name(),LThumbBoxes[0].name()], [LArmBoxes[0].translate.get(),LThumbBoxes[0].translate.get()])

	LIndexRootLCrv = createConnectCurve('Lindex_root', [LArmBoxes[-1].name(),LmidCubes[0].name()], [LArmBoxes[0].translate.get(),LmidCubes[0].translate.get()])
	LMiddleRootLCrv = createConnectCurve('Lmiddle_root', [LArmBoxes[-1].name(),LmidCubes[1].name()], [LArmBoxes[0].translate.get(),LmidCubes[1].translate.get()])
	LRingRootLCrv = createConnectCurve('Lring_root', [LArmBoxes[-1].name(),LmidCubes[2].name()], [LArmBoxes[0].translate.get(),LmidCubes[2].translate.get()])
	LPinkyRootLCrv = createConnectCurve('Lpinky_root', [LArmBoxes[-1].name(),LmidCubes[3].name()], [LArmBoxes[0].translate.get(),LmidCubes[3].translate.get()])


	#handL other connect

	LIndexMidLCrv = createConnectCurve('Lindex_midhand', [LmidCubes[0].name(),LIndexBoxes[0].name()], [LmidCubes[0].translate.get(),LIndexBoxes[0].translate.get()])
	LMiddleMidLCrv = createConnectCurve('Lmiddle_midhand', [LmidCubes[1].name(),LMiddleBoxes[0].name()], [LmidCubes[1].translate.get(),LMiddleBoxes[0].translate.get()])
	LRingMidLCrv = createConnectCurve('Lring_midhand', [LmidCubes[2].name(),LRingBoxes[0].name()], [LmidCubes[2].translate.get(),LRingBoxes[0].translate.get()])
	LPinkyMidLCrv = createConnectCurve('Lpinky_midhand', [LmidCubes[3].name(),LPinkyBoxes[0].name()], [LmidCubes[3].translate.get(),LPinkyBoxes[0].translate.get()])
	pm.group( LThumbRootLCrv, LIndexRootLCrv, LMiddleRootLCrv, LRingRootLCrv, LPinkyRootLCrv, LIndexMidLCrv, LMiddleMidLCrv, LRingMidLCrv, LPinkyMidLCrv, parent=LHandTrans )

	pm.parent( LmidCubes, LHandTrans )



	#handR
	RThumbGrp, RThumbBoxes = createSplineSet( 'Rthumb', RThumbList, 1 )
	RIndexGrp, RIndexBoxes = createSplineSet( 'Rindex', RIndexList, 1 )
	RMiddleGrp, RMiddleBoxes = createSplineSet( 'Rmiddle', RMiddleList, 1 )
	RRingGrp, RRingBoxes = createSplineSet( 'Rring', RRingList, 1 )
	RPinkyGrp, RPinkyBoxes = createSplineSet( 'Rpinky', RPinkyList, 1 )

	#handR Pole
	RThumbPole = addPoleVecSphere(RThumbBoxes[0], 'Rthumb_pole', fingerPolePos)
	RIndexPole = addPoleVecSphere(RIndexBoxes[0], 'Rindex_pole', fingerPolePos)
	RMiddlePole = addPoleVecSphere(RMiddleBoxes[0], 'Rmiddle_pole', fingerPolePos)
	RRingPole = addPoleVecSphere(RRingBoxes[0], 'Rring_pole', fingerPolePos)
	RPinkyPole = addPoleVecSphere(RPinkyBoxes[0], 'Rpinky_pole', fingerPolePos)

	RHandTrans= pm.createNode( 'transform', n=dummyRHand )
	pm.move( RArmBoxes[-1].translateX,RArmBoxes[-1].translateY,RArmBoxes[-1].translateZ, RHandTrans.scalePivot, RHandTrans.rotatePivot, absolute=True )
	pm.group( RThumbGrp, RIndexGrp, RMiddleGrp, RRingGrp, RPinkyGrp, parent=RHandTrans )


	#handR mid cube for hand
	RmidCubes = createHandMidCube(handObj=RArmBoxes[-1], rootFingers=[RIndexBoxes[0],RMiddleBoxes[0],RRingBoxes[0],RPinkyBoxes[0]], nameList=['Rindex_mid','Rmiddle_mid','Rring_mid','Rpinky_mid'])

	RThumbRootLCrv = createConnectCurve('Rthumb_root', [RArmBoxes[-1].name(),RThumbBoxes[0].name()], [RArmBoxes[0].translate.get(),RThumbBoxes[0].translate.get()])

	RIndexRootLCrv = createConnectCurve('Rindex_root', [RArmBoxes[-1].name(),RmidCubes[0].name()], [RArmBoxes[0].translate.get(),RmidCubes[0].translate.get()])
	RMiddleRootLCrv = createConnectCurve('Rmiddle_root', [RArmBoxes[-1].name(),RmidCubes[1].name()], [RArmBoxes[0].translate.get(),RmidCubes[1].translate.get()])
	RRingRootLCrv = createConnectCurve('Rring_root', [RArmBoxes[-1].name(),RmidCubes[2].name()], [RArmBoxes[0].translate.get(),RmidCubes[2].translate.get()])
	RPinkyRootLCrv = createConnectCurve('Rpinky_root', [RArmBoxes[-1].name(),RmidCubes[3].name()], [RArmBoxes[0].translate.get(),RmidCubes[3].translate.get()])


	#handR other connect
	RIndexMidLCrv = createConnectCurve('Rindex_midhand', [RmidCubes[0].name(),RIndexBoxes[0].name()], [RmidCubes[0].translate.get(),RIndexBoxes[0].translate.get()])
	RMiddleMidLCrv = createConnectCurve('Rmiddle_midhand', [RmidCubes[1].name(),RMiddleBoxes[0].name()], [RmidCubes[1].translate.get(),RMiddleBoxes[0].translate.get()])
	RRingMidLCrv = createConnectCurve('Rring_midhand', [RmidCubes[2].name(),RRingBoxes[0].name()], [RmidCubes[2].translate.get(),RRingBoxes[0].translate.get()])
	RPinkyMidLCrv = createConnectCurve('Rpinky_midhand', [RmidCubes[3].name(),RPinkyBoxes[0].name()], [RmidCubes[3].translate.get(),RPinkyBoxes[0].translate.get()])
	pm.group( RThumbRootLCrv,RIndexRootLCrv, RMiddleRootLCrv, RRingRootLCrv, RPinkyRootLCrv,RIndexMidLCrv, RMiddleMidLCrv, RRingMidLCrv, RPinkyMidLCrv, parent=RHandTrans )

	pm.parent( RmidCubes, RHandTrans )







	#spline
	splineGrp, splineBoxes = createSplineSet( dummySpline, SplineStrList, baseCubeSize )

	#neck
	headGrp, headBoxes = createSplineSet( dummyHead, NeckStrList, baseCubeSize )




	#create symmetry expression
	if symmetry:
		#foot
		createSymmetryExp(LFootBoxes, RFootBoxes)
		createSymmetryExp([LfootPole], [RfootPole])
		#anckle
		createSymmetryExp(LAnkleList, RAnkleList)
		#arm
		createSymmetryExp(LArmBoxes, RArmBoxes)
		createSymmetryExp([LupArmPole], [RupArmPole])

		createSymmetryExp(LmidCubes, RmidCubes)

		#fingur
		createSymmetryExp(LThumbBoxes, RThumbBoxes)
		createSymmetryExp(LIndexBoxes, RIndexBoxes)
		createSymmetryExp(LMiddleBoxes, RMiddleBoxes)
		createSymmetryExp(LRingBoxes, RRingBoxes)
		createSymmetryExp(LPinkyBoxes, RPinkyBoxes)

		createSymmetryExp( [LThumbPole], [RThumbPole])
		createSymmetryExp( [LIndexPole], [RIndexPole])
		createSymmetryExp( [LMiddlePole], [RMiddlePole])
		createSymmetryExp( [LRingPole], [RRingPole])
		createSymmetryExp( [LPinkyPole], [RPinkyPole])

	#lock anckle translate
	lockAnkleTranslate(LAnkleList)
	if symmetry:
		lockAnkleTranslate(RAnkleList)


	#create real clavicle
	if isClavicle:
		LClavicleGrp, LClavicleBoxes = createSplineSet( dummyLClavicle, LClavicleList, baseCubeSize )
		RClavicleGrp, RClavicleBoxes = createSplineSet( dummyRClavicle, RClavicleList, baseCubeSize )

		if symmetry:
			createSymmetryExp(LClavicleBoxes, RClavicleBoxes)

		realClaRootLCrv = createConnectCurve('Lreal_cla_root', [LClavicleBoxes[0],splineBoxes[-1]], [LClavicleBoxes[0].translate.get(),splineBoxes[-1].translate.get()])
		realClaRootRCrv = createConnectCurve('Rreal_cla_root', [RClavicleBoxes[0],splineBoxes[-1]], [RClavicleBoxes[0].translate.get(),splineBoxes[-1].translate.get()])

		pm.parent( realClaRootLCrv, LClavicleGrp)
		pm.parent( realClaRootRCrv, RClavicleGrp)

		pm.parent( LClavicleGrp, rootTrans)
		pm.parent( RClavicleGrp, rootTrans)


	#create other connect crv
	hipLCrv = createConnectCurve('Lhip', [LFootBoxes[0],splineBoxes[0]], [LFootBoxes[0].translate.get(),splineBoxes[0].translate.get()])
	hipRCrv = createConnectCurve('Rhip', [RFootBoxes[0],splineBoxes[0]], [RFootBoxes[0].translate.get(),splineBoxes[0].translate.get()])
	pm.parent( hipLCrv, LFootGrp)
	pm.parent( hipRCrv, RFootGrp)

	armRootLCrv = createConnectCurve('Larm_root', [LArmBoxes[0],splineBoxes[-1]], [LArmBoxes[0].translate.get(),splineBoxes[-1].translate.get()])
	armRootRCrv = createConnectCurve('Rarm_root', [RArmBoxes[0],splineBoxes[-1]], [RArmBoxes[0].translate.get(),splineBoxes[-1].translate.get()])
	pm.parent( armRootLCrv, LArmGrp)
	pm.parent( armRootRCrv, RArmGrp)

	neckRootCrv = createConnectCurve('neck_root', [splineBoxes[-1],headBoxes[0]], [splineBoxes[0].translate.get(),headBoxes[0].translate.get()])
	pm.parent( neckRootCrv, headGrp)


	pm.parent( LFootGrp, rootTrans)
	pm.parent( RFootGrp, rootTrans)
	pm.parent( LArmGrp, rootTrans)
	pm.parent( RArmGrp, rootTrans)
	pm.parent( LHandTrans, rootTrans)
	pm.parent( RHandTrans, rootTrans)
	pm.parent( splineGrp, rootTrans)
	pm.parent( headGrp, rootTrans)

	pm.select(rootTrans)


#Dummy ball height fit to ground height
def setBallToGround(symmetry=True):
	LAnkle = ''
	RAnkle = ''
	LBall = ''
	RBall = ''

	try:
		LAnkle = pm.ls('ankleL')[0]
		RAnkle = pm.ls('ankleR')[0]

		LBall = pm.ls('ballL')[0]
		RBall = pm.ls('ballR')[0]
	except (IndexError, RuntimeError):
		print 'Not exist ankle or ball'
		return

	LAnkleHeight = LAnkle.translateY.get()
	RAnkleHeight = RAnkle.translateY.get()

	LBall.translateY.set(-LAnkleHeight)
	if not symmetry:
		RBall.translateY.set(-RAnkleHeight)


def exportRigSet():
	if not pm.objExists(CharaGroupTop):
		pm.warning('Rig group does not exist')
		return False
	selected = None
	restored = True
	try:
		files = pm.fileDialog2(cap='Export Rig',
			fileFilter='Maya Files (*.ma *.mb);;Maya ASCII (*.ma);;Maya Binary (*.mb)',
			dialogStyle=2, okc='Export')
		filename = table_selection.selected_text(files)
		if not filename:
			return False
		if any(ord(character) < 32 for character in filename):
			raise ValueError('Rig export filename must not contain control characters')
		root = pm.PyNode(CharaGroupTop)
		current = pm.ls(selection=True)
		current = [] if current is None else current
		if not isinstance(current, (list, tuple)):
			raise ValueError('Previous selection must be a sequence')
		selected = tuple(current)
		pm.select(root, replace=True)
		# Preserve the native serializer/overwrite policy; host verification is pending.
		pm.exportSelected(filename, force=True)
	except (IOError, OSError, RuntimeError, TypeError, ValueError) as error:
		pm.warning('Rig export did not complete; native output may remain: {0}'.format(error))
		return False
	finally:
		if selected is not None:
			restored = scene_selection.restore_selection(selected)
	return bool(restored)


def importRigSet():

	multipleFilters = "Maya Files (*.ma *.mb);;Maya ASCII (*.ma);;Maya Binary (*.mb)"
	file = pm.fileDialog2(cap='Import Rig',fileFilter=multipleFilters, okc='Import', fileMode=1)

	if file:
		print 'import :'+file[0]

		if pm.objExists(CharaGroupTop):
			if not _confirm_action(
					'Replace current character rig',
					'Importing this file will delete the existing {0}.'.format(
						CharaGroupTop), 'Replace and Import'):
				return False
			pm.delete( CharaGroupTop )

		pm.importFile(file[0])
		return True
	return False


#delete dummy rig set
def DeleteDummyRig():
	if pm.objExists(DummyRigName):
		if not _confirm_action(
				'Delete dummy rig',
				'Delete {0}? This operation changes the current scene.'.format(
					DummyRigName), 'Delete'):
			return False
		pm.delete( DummyRigName )
		return True
	else:
		print 'DummyRigSet is not exist!'
	return False


#chenge dummy rig visible
def changeVisibleDummyParts( type='all' ):

	items = pm.ls(dummyPartsList )

	for item in items:
		item.hide()

	if type == 'head':
		pm.ls(dummyHead)[0].show()
	elif type == 'spline':
		pm.ls(dummySpline)[0].show()
	elif type == 'hand':
		pm.ls(dummyLHand)[0].show()
		pm.ls(dummyRHand)[0].show()
	elif type == 'arm':
		pm.ls(dummyLArm)[0].show()
		pm.ls(dummyRArm)[0].show()
	elif type == 'foot':
		pm.ls(dummyLFoot)[0].show()
		pm.ls(dummyRFoot)[0].show()
	elif type == 'clavicle':
		pm.ls(dummyLClavicle)[0].show()
		pm.ls(dummyRClavicle)[0].show()
	else:
		pm.ls(dummyHead)[0].show()
		pm.ls(dummySpline)[0].show()
		pm.ls(dummyLHand)[0].show()
		pm.ls(dummyRHand)[0].show()
		pm.ls(dummyLArm)[0].show()
		pm.ls(dummyRArm)[0].show()
		pm.ls(dummyLFoot)[0].show()
		pm.ls(dummyRFoot)[0].show()
		pm.ls(dummyLClavicle)[0].show()
		pm.ls(dummyRClavicle)[0].show()


#chenge create joint
def createJointSet(dataList=None):
	if dataList is None:
		dataList = []

	pm.select( clear=True )

	preJnt = None

	newJntList = []
	for jnt in dataList:
		#joint orient adjust
		if preJnt is not None:
			#print 'edit jnt '+ preJnt
			pm.joint( preJnt, e=True, zso=True, oj='xyz' )

		#create joint
		try:
			trans = pm.ls(jnt)[0]
			jnt_Trans = pm.joint( name=jnt+'_jnt', p=pm.xform(trans, q=True, ws=True, t=True ) )
			newJntList.append(jnt_Trans)
			preJnt = jnt_Trans
		except (IndexError, RuntimeError):
			print 'Not exist '+jnt
			break

	return newJntList


#create base rig set
def GenerateHumanRig(symmetry=True, isClavicle=True, footPoleType=1, armPoleType=1, isAnklePlane=True, isWeightSplineJnt=True, replace_existing=False):
	print 'GenerateHumanRig'

	if not pm.objExists(DummyRigName):
		print 'DummyRigSet is not exist!'
		return


	'''check groups'''
	checkCharaGroup()

	if pm.objExists(JointGrpName):
		if not replace_existing:
			pm.warning('Joint rig already exists; explicit replacement is required.')
			return False
		pm.delete( JointGrpName )

	rootTrans= pm.createNode( 'transform', n=JointGrpName )
	pm.parent(rootTrans,CharaGroupTop)




	'''create joint'''

	#foot
	LfootJnts = createJointSet(LFootJointList)

	#arm
	LarmJnts = createJointSet(LArmJointList)

	#hand
	LthumbJnts = createJointSet(LThumbJointList)
	LindexJnts = createJointSet(LIndexJointList)
	LmiddleJnts = createJointSet(LMiddleJointList)
	LringJnts = createJointSet(LRingJointList)
	LpinkyJnts = createJointSet(LPinkyJointList)

	#create L hand joint
	LhandJnt = pm.duplicate(LarmJnts[-1])
	LhandJnt = pm.rename(LhandJnt, 'handL_jnt')

	#create L hand mid joint
	LindexMidJnts = createJointSet(['Lindex_mid'])
	LmiddleMidJnts = createJointSet(['Lmiddle_mid'])
	LringMidJnts = createJointSet(['Lring_mid'])
	LpinkyMidJnts = createJointSet(['Lpinky_mid'])

	pm.parent(LmiddleJnts[0], LmiddleMidJnts[0])

	pm.parent(LmiddleMidJnts[0], LhandJnt)#add first for axis
	pm.parent(LindexMidJnts[0], LhandJnt)
	pm.parent(LringMidJnts[0], LhandJnt)
	pm.parent(LpinkyMidJnts[0], LhandJnt)

	pm.parent(LthumbJnts[0], LhandJnt)
	pm.parent(LindexJnts[0], LindexMidJnts[0])
	pm.parent(LringJnts[0], LringMidJnts[0])
	pm.parent(LpinkyJnts[0], LpinkyMidJnts[0])
	pm.parent(LhandJnt, LarmJnts[-1])


	#prepare R side
	RfootJnts = []
	RarmJnts = []
	RthumbJnts = []
	RindexJnts = []
	RmiddleJnts = []
	RringJnts = []
	RpinkyJnts = []
	RClavicleJnts = []

	if not symmetry:
		RfootJnts = createJointSet(RFootJointList)

		RarmJnts = createJointSet(RArmJointList)

		RthumbJnts = createJointSet(RThumbJointList)
		RindexJnts = createJointSet(RIndexJointList)
		RmiddleJnts = createJointSet(RMiddleJointList)
		RringJnts = createJointSet(RRingJointList)
		RpinkyJnts = createJointSet(RPinkyJointList)

		#create R hand joint
		RhandJnt = pm.duplicate(RarmJnts[-1])
		RhandJnt = pm.rename(RhandJnt, 'handR_jnt')

		RindexMidJnts = createJointSet(['Rindex_mid'])
		RmiddleMidJnts = createJointSet(['Rmiddle_mid'])
		RringMidJnts = createJointSet(['Rring_mid'])
		RpinkyMidJnts = createJointSet(['Rpinky_mid'])

		pm.parent(RmiddleJnts[0], RmiddleMidJnts[0])

		pm.parent(RmiddleMidJnts[0], RhandJnt)#add first for axis
		pm.parent(RindexMidJnts[0], RhandJnt)
		pm.parent(RringMidJnts[0], RhandJnt)
		pm.parent(RpinkyMidJnts[0], RhandJnt)

		pm.parent(RthumbJnts[0], RhandJnt)
		pm.parent(RindexJnts[0], RindexMidJnts[0])
		pm.parent(RringJnts[0], RringMidJnts[0])
		pm.parent(RpinkyJnts[0], RpinkyMidJnts[0])
		pm.parent(RhandJnt, RarmJnts[-1])




	#spline
	splineJnts = createJointSet(SplineJointList)
	hipJnts = createJointSet(HipJointList)

	#head
	headJnts = createJointSet(HeadJointList)


	'''connect parent'''

	#spline connect and correct axis before connect arm and foot
	pm.parent(headJnts[0],splineJnts[-1])
	pm.parent(hipJnts[0],splineJnts[0])
	pm.joint( headJnts,splineJnts, e=True, zso=True, oj='xzy', secondaryAxisOrient='xup' )

	#parent joints
	pm.parent(LfootJnts[0],hipJnts[0])
	pm.parent(LarmJnts[0],splineJnts[-1])

	if not symmetry:
		pm.parent(RfootJnts[0],hipJnts[0])
		pm.parent(RarmJnts[0],splineJnts[-1])


	#clavicle
	if isClavicle:
		LClavicleJnts = createJointSet( LClavicleJointList )
		pm.parent( LClavicleJnts[0], splineJnts[-1] )
		pm.joint( LClavicleJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='zup' )

		if not symmetry:
			RClavicleJnts = createJointSet( RClavicleJointList )
			pm.parent( RClavicleJnts[0], splineJnts[-1] )
			pm.joint( RClavicleJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='zup' )
			DPTJoint.reverseBehaviorOrientArm( RClavicleJnts[0] )


	'''correct joint orient'''
	#foot
	pm.joint( LfootJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='xup' )
	if not symmetry:
		pm.joint( RfootJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='xup' )
		DPTJoint.reverseBehaviorOrient( RfootJnts[0] )


	if footPoleType == 3:
		DPTJoint.crossCorrectJointAxis( ['up_legL_jnt','kneeL_jnt','ankleL_jnt'], aimAxis=(1,0,0), upAxis=(0,0,1), reverse=False)
		if not symmetry:
			DPTJoint.crossCorrectJointAxis( ['up_legR_jnt','kneeR_jnt','ankleR_jnt'], aimAxis=(-1,0,0), upAxis=(0,0,-1), reverse=True)
	elif footPoleType == 2:
		DPTJoint.crossCorrectJointAxis( ['up_legL_jnt','kneeL_jnt','Lfoot_pole'], aimAxis=(1,0,0), upAxis=(0,0,1), reverse=False)
		if not symmetry:
			DPTJoint.crossCorrectJointAxis( ['up_legR_jnt','kneeR_jnt','Rfoot_pole'], aimAxis=(-1,0,0), upAxis=(0,0,-1), reverse=True)


	if isAnklePlane:
		DPTJoint.crossCorrectJointAxis( ['ankleL_jnt','ballL_jnt','toeL_jnt'], aimAxis=(1,0,0), upAxis=(0,0,1), reverse=False)
		if not symmetry:
			DPTJoint.crossCorrectJointAxis( ['ankleR_jnt','ballR_jnt','toeR_jnt'], aimAxis=(-1,0,0), upAxis=(0,0,-1), reverse=True)


	#arm
	pm.joint( LarmJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='zup' )


	if not symmetry:
		pm.joint( RarmJnts[0], e=True, zso=True, ch=True, oj='xzy', secondaryAxisOrient='zup' )
		DPTJoint.reverseBehaviorOrientArm( RarmJnts[0] )

	if armPoleType == 3:
		DPTJoint.crossCorrectJointAxis( ['up_armL_jnt','elbowL_jnt','wristL_jnt'], aimAxis=(1,0,0), upAxis=(0,1,0), reverse=False)
		if not symmetry:
			DPTJoint.crossCorrectJointAxis( ['up_armR_jnt','elbowR_jnt','wristR_jnt'], aimAxis=(-1,0,0), upAxis=(0,-1,0), reverse=True)
	elif armPoleType == 2:
		DPTJoint.crossCorrectJointAxis( ['up_armL_jnt','elbowL_jnt','Lup_arm_pole'], aimAxis=(1,0,0), upAxis=(0,1,0), reverse=False)
		if not symmetry:
			DPTJoint.crossCorrectJointAxis( ['up_armR_jnt','elbowR_jnt','Rup_arm_pole'], aimAxis=(-1,0,0), upAxis=(0,-1,0), reverse=True)



	#correct wrist and hand axis
	pm.parent(LhandJnt,LthumbJnts[0],LindexMidJnts[0],LmiddleMidJnts[0],LringMidJnts[0],LpinkyMidJnts[0],world=True)
	pm.parent(LmiddleJnts[0],LarmJnts[-1])
	pm.joint( LarmJnts[-1], e=True, zso=True, oj='xzy', secondaryAxisOrient='zup' )
	pm.parent(LmiddleJnts[0],world=True)
	pm.parent(LmiddleJnts[0],LmiddleMidJnts[0])
	pm.parent(LthumbJnts[0],LindexMidJnts[0],LmiddleMidJnts[0],LringMidJnts[0],LpinkyMidJnts[0],LhandJnt)
	pm.parent(LhandJnt,LarmJnts[-1])

	if not symmetry:
		pm.parent(RhandJnt,RthumbJnts[0],RindexMidJnts[0],RmiddleMidJnts[0],RringMidJnts[0],RpinkyMidJnts[0],world=True)
		pm.parent(RmiddleJnts[0],RarmJnts[-1])
		pm.joint( RarmJnts[-1], e=True, zso=True, oj='xzy', secondaryAxisOrient='zup' )
		pm.parent(RmiddleJnts[0],world=True)
		pm.parent(RmiddleJnts[0],RmiddleMidJnts[0])
		pm.parent(RthumbJnts[0],RindexMidJnts[0],RmiddleMidJnts[0],RringMidJnts[0],RpinkyMidJnts[0],RhandJnt)
		pm.parent(RhandJnt,RarmJnts[-1])

		RWristOrient = RarmJnts[-1].getAttr( "jointOrient")
		RarmJnts[-1].setAttr( "jointOrientX", -(RWristOrient[0]+180))
		RarmJnts[-1].setAttr( "jointOrientY", -RWristOrient[1])
		RarmJnts[-1].setAttr( "jointOrientZ", RWristOrient[2]-180)

		RhandJnt.setAttr("jointOrient", (0,0,0))




	#finguer axis correct

	LFinAimAxis=(1,0,0)
	LFinUpAxis=(0,0,-1)
	pm.joint( LindexMidJnts[0], e=True, zso=True, oj='xzy', secondaryAxisOrient='zdown' )
	pm.joint( LmiddleMidJnts[0], e=True, zso=True, oj='xzy', secondaryAxisOrient='zdown' )
	pm.joint( LringMidJnts[0], e=True, zso=True, oj='xzy', secondaryAxisOrient='zdown' )
	pm.joint( LpinkyMidJnts[0], e=True, zso=True, oj='xzy', secondaryAxisOrient='zdown' )
	DPTJoint.crossCorrectJointAxisForFingur(LthumbJnts,'Lthumb_pole',aimAxis=LFinAimAxis, upAxis=LFinUpAxis, reverse=False)
	DPTJoint.crossCorrectJointAxisForFingur(LindexJnts,'Lindex_pole',aimAxis=LFinAimAxis, upAxis=LFinUpAxis, reverse=False)
	DPTJoint.crossCorrectJointAxisForFingur(LmiddleJnts,'Lmiddle_pole',aimAxis=LFinAimAxis, upAxis=LFinUpAxis, reverse=False)
	DPTJoint.crossCorrectJointAxisForFingur(LringJnts,'Lring_pole',aimAxis=LFinAimAxis, upAxis=LFinUpAxis, reverse=False)
	DPTJoint.crossCorrectJointAxisForFingur(LpinkyJnts,'Lpinky_pole',aimAxis=LFinAimAxis, upAxis=LFinUpAxis, reverse=False)






	if not symmetry:
		RFinAimAxis=(-1,0,0)
		RFinUpAxis=(0,0,-1)
		DPTJoint.crossCorrectJointAxisForFingur(RthumbJnts,'Rthumb_pole',aimAxis=RFinAimAxis, upAxis=RFinUpAxis, reverse=True)
		DPTJoint.crossCorrectJointAxisForFingur(RindexJnts,'Rindex_pole',aimAxis=RFinAimAxis, upAxis=RFinUpAxis, reverse=True)
		DPTJoint.crossCorrectJointAxisForFingur(RmiddleJnts,'Rmiddle_pole',aimAxis=RFinAimAxis, upAxis=RFinUpAxis, reverse=True)
		DPTJoint.crossCorrectJointAxisForFingur(RringJnts,'Rring_pole',aimAxis=RFinAimAxis, upAxis=RFinUpAxis, reverse=True)
		DPTJoint.crossCorrectJointAxisForFingur(RpinkyJnts,'Rpinky_pole',aimAxis=RFinAimAxis, upAxis=RFinUpAxis, reverse=True)


	#symmetry
	if symmetry:
		pm.mirrorJoint(LfootJnts[0],mirrorYZ=True, mirrorBehavior=True, searchReplace=('L', 'R') )
		pm.mirrorJoint(LarmJnts[0],mirrorYZ=True, mirrorBehavior=True, searchReplace=('L', 'R') )
		pm.mirrorJoint(LClavicleJnts[0],mirrorYZ=True, mirrorBehavior=True, searchReplace=('L', 'R') )



	pm.parent(splineJnts[0], rootTrans)


	#create weight spline joint
	if isWeightSplineJnt:

		#LLeg
		LLegGrpTop = DPTgrp.createEmpty(LlegWeightJnt_grp_Name,rootTrans)

		LuplegWeightJoints = DPTJoint.makeSplitJoint(srcJoint='up_legL_jnt', targetJoint='kneeL_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'})
		LkneeWeightJoints = DPTJoint.makeSplitJoint(srcJoint='kneeL_jnt', targetJoint='ankleL_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'},withEndJoint=True)

		DPTJoint.parentJointFromlist(LuplegWeightJoints)
		DPTJoint.parentJointFromlist(LkneeWeightJoints)
		pm.parent(LkneeWeightJoints[0],LuplegWeightJoints[-1])

		pm.parent(LuplegWeightJoints[0], LLegGrpTop)

		#RLeg
		RLegGrpTop = DPTgrp.createEmpty(RlegWeightJnt_grp_Name,rootTrans)

		RuplegWeightJoints = DPTJoint.makeSplitJoint(srcJoint='up_legR_jnt', targetJoint='kneeR_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'})
		RkneeWeightJoints = DPTJoint.makeSplitJoint(srcJoint='kneeR_jnt', targetJoint='ankleR_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'},withEndJoint=True)

		DPTJoint.parentJointFromlist(RuplegWeightJoints)
		DPTJoint.parentJointFromlist(RkneeWeightJoints)
		pm.parent(RkneeWeightJoints[0],RuplegWeightJoints[-1])

		pm.parent(RuplegWeightJoints[0], RLegGrpTop)

		#Larm
		LArmGrpTop = DPTgrp.createEmpty(LarmWeightJnt_grp_Name,rootTrans)

		LuparmWeightJoints = DPTJoint.makeSplitJoint(srcJoint='up_armL_jnt', targetJoint='elbowL_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'})
		LelbowWeightJoints = DPTJoint.makeSplitJoint(srcJoint='elbowL_jnt', targetJoint='wristL_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'},withEndJoint=True)

		DPTJoint.parentJointFromlist(LuparmWeightJoints)
		DPTJoint.parentJointFromlist(LelbowWeightJoints)
		pm.parent(LelbowWeightJoints[0],LuparmWeightJoints[-1])

		pm.parent(LuparmWeightJoints[0], LArmGrpTop)

		#Rarm
		RArmGrpTop = DPTgrp.createEmpty(RarmWeightJnt_grp_Name,rootTrans)

		RuparmWeightJoints = DPTJoint.makeSplitJoint(srcJoint='up_armR_jnt', targetJoint='elbowR_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'})
		RelbowWeightJoints = DPTJoint.makeSplitJoint(srcJoint='elbowR_jnt', targetJoint='wristR_jnt', numJoint=3, nameDic={'jnt':'weight_jnt1'},withEndJoint=True)

		DPTJoint.parentJointFromlist(RuparmWeightJoints)
		DPTJoint.parentJointFromlist(RelbowWeightJoints)
		pm.parent(RelbowWeightJoints[0],RuparmWeightJoints[-1])

		pm.parent(RuparmWeightJoints[0], RArmGrpTop)

		#spline
		SplineGrpTop = DPTgrp.createEmpty(spineWeight_grp_Name,rootTrans)
		crvTrans = DPTCurveTool.createCurveAlongObject(name='sp_curve_tmp', objects= ['hip_jnt','spline_low_jnt','spline_mid_jnt','spline_hi_jnt','spline_end_jnt'])
		crvShape = crvTrans.getShape()
		maxval = crvShape.getAttr ( "maxValue" )
		minval = crvShape.getAttr ( "minValue" )
		paramLen = maxval-minval
		stepLen = paramLen/7.0

		sp_jntList=[]

		for i in range(7):
			cpos = pm.pointOnCurve(crvTrans, pr=stepLen*float(i+1), p=True )
			spjnt = pm.joint( name='spline_weight_jnt1', p=cpos )
			pm.parent(spjnt,SplineGrpTop)
			sp_jntList.append(spjnt)

		pm.delete(crvTrans)

		DPTJoint.parentJointFromlist(sp_jntList)#connect joint
		pm.joint( sp_jntList, e=True, zso=True, oj='xzy', secondaryAxisOrient='xup' )#correct axis
		pm.delete(sp_jntList[-1])#delete last joint
		del sp_jntList[-1]#delete array joint

		#re parent spline joints
		for j in sp_jntList:
			pm.parent(j,SplineGrpTop)

		#create untwist joint for weight spline joint
		for j in sp_jntList:
			newTwistJnt = pm.duplicate(j, n=j.name()+'_untwist', parentOnly=True)
			pm.parent(newTwistJnt,j)


		#create untwist joint for hip
		hipJntUntwist = pm.duplicate(hipJnts, n='hip_jnt_untwist',parentOnly=True)
		pm.parent(hipJntUntwist,hipJnts[0])



	pm.select( splineJnts[0] )







#create create setup window
class openWin(object):

	def __init__(self):
		uiname = "DPTHumanRiggingToolWin"

		ui_windows.delete_window(uiname)

		with pm.window(uiname,widthHeight=(480, 640)):
			with pm.scrollLayout(horizontalScrollBarThickness=16, verticalScrollBarThickness=16, childResizable=True):
				with pm.columnLayout(adjustableColumn=True):
					with pm.frameLayout( l='Common Parameter', labelVisible=True,collapsable=True):
						pm.textFieldGrp( label='Character Name', text='character1' )
						pm.separator()
						with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 100), (2, 100)] ):
								pm.button( label='Import', c = lambda *args: self.runImportCB() )
								pm.button( label='Export', c = lambda *args: self.runExportCB() )
						pm.separator()
						self.dummySymmetry = pm.checkBox( label='Symmetry', value=True )
						self.withRealClavicle = pm.checkBox( label='With Real Clavicle', value=True )
					with pm.frameLayout( l='Setup Dummy Rig', labelVisible=True, collapsable=True, collapse=True):
						with pm.columnLayout(adjustableColumn=True):
							pm.button( label='Create Dummy Rig',c = lambda *args: self.runSetupDummyRigCB() )
							pm.separator()
							with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 80), (2, 80), (3, 80), (4, 80)] ):
								pm.button( label='All', c = lambda *args: self.runShowDummyPartsCB( type='all') )
								pm.button( label='Head', c = lambda *args: self.runShowDummyPartsCB( type='head'))
								pm.button( label='Spline', c = lambda *args: self.runShowDummyPartsCB( type='spline'))
								pm.button( label='Hand', c = lambda *args: self.runShowDummyPartsCB( type='hand'))
								pm.button( label='Arm', c = lambda *args: self.runShowDummyPartsCB( type='arm'))
								pm.button( label='Foot', c = lambda *args: self.runShowDummyPartsCB( type='foot'))
								pm.button( label='Clavicle', c = lambda *args: self.runShowDummyPartsCB( type='clavicle'))
							pm.separator()
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 100), (2, 100)] ):
								pm.button( label='Ball to Ground', c = lambda *args: self.runSetBallToGroundCB() )
							pm.separator()
							pm.button( label='Delete Dummy Rig',c = lambda *args: self.runDeleteDummyRigCB() )
					with pm.frameLayout( l='Create Joint', labelVisible=True, collapsable=True, collapse=True):
						with pm.columnLayout(adjustableColumn=True):
							self.footPoleVectorType = pm.radioButtonGrp( nrb=3, label='Foot Axis', la3=['None', 'Pole Vector', 'Auto'], cw4=(60, 100, 100, 100), select=2 )
							self.armPoleVectorType = pm.radioButtonGrp( nrb=3, label='Arm Axis', la3=['None', 'Pole Vector', 'Auto'], cw4=(60, 100, 100, 100), select=2 )
							self.PreserveAnklePlane = pm.checkBox( label='Keep Ankle Plane', value=True )
							pm.separator()
							self.WeightSplineJntCheck = pm.checkBox(l="Generate Weight Spline Joint", v=1)
							pm.separator()
							self.hideDummyRigCheck = pm.checkBox(l="Hide Dummy Rig Set", v=1)
							pm.separator()
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 300), (2, 120)] ):
								pm.button( label='Generate Joint',c = lambda *args: self.runGenerateJointSetCB() )
								pm.button( label='Delete Joint',c = lambda *args: self.runDeleteJointSetCB() )
					with pm.frameLayout( l='Edit Joint', labelVisible=True, collapsable=True, collapse=True):
						with pm.columnLayout(adjustableColumn=True):
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 140), (2, 140)] ):
								pm.button( label='Show Local Axis', c = lambda *args: self.runVisibleAxisCB(visible=True) )
								pm.button( label='Hide Local Axis', c = lambda *args: self.runVisibleAxisCB(visible=False) )
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 200), (2, 80)] ):
								self.AimAxisGrp = pm.radioButtonGrp( nrb=3, label='Aim Axis', la3=['X', 'Y', 'Z'], cw4=(80, 40, 40, 40), select=1 )
								self.AimAxisRevCheck = pm.checkBox(l="Reverse", v=0)
								self.UpAxisGrp = pm.radioButtonGrp( nrb=3, label='Up Axis', la3=['X', 'Y', 'Z'], cw4=(80, 40, 40, 40), select=2 )
								self.UpAxisRevCheck = pm.checkBox(l="Reverse", v=0)
							pm.separator()
							self.WorldUpAxisGrp = pm.floatFieldGrp(nf=3, label="World Up Dir", v1=0.0, v2=1.0, v3=0.0, cw4=(80, 50, 50, 50))
							pm.separator()
							pm.button( label='Orient Axis', c = lambda *args: self.runEditOrientAxisCB() )
							pm.separator()
							pm.separator()
							self.CrossAimAxisGrp = pm.floatFieldGrp(nf=3, label="Aim Axis", v1=1.0, v2=0.0, v3=0.0, cw4=(80, 50, 50, 50))
							self.CrossUpAxisGrp = pm.floatFieldGrp(nf=3, label="Up Axis", v1=0.0, v2=1.0, v3=0.0, cw4=(80, 50, 50, 50))
							self.CrossRevCheck = pm.checkBox( label='Reverse' )
							pm.text( label='Select 3 joint. Then click below button.' )
							pm.button( label='Set Cross Plane Axis', c = lambda *args: self.runCrossOrientAxisCB() )
							pm.separator()
							pm.separator()
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 140), (2, 140)] ):
								pm.button( label='Mirror L > R', c = lambda *args: self.runMirrorCB(LSide=1) )
								pm.button( label='Mirror R > L', c = lambda *args: self.runMirrorCB(LSide=0) )
					with pm.frameLayout( l='Create Rig Set', labelVisible=True, collapsable=True, collapse=True):
						with pm.columnLayout(adjustableColumn=True):
							pm.button( label='Create Rig Set', c = lambda *args: self.runCreateRigCB() )
					with pm.frameLayout( l='Edit Hand Pose', labelVisible=True, collapsable=True, collapse=True):
						with pm.columnLayout(adjustableColumn=True):
							with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 100), (2, 100)]):
								pm.button( label='Update Param', c = lambda *args: self.updateHandPose() )
								pm.button( label='Reset Pose', c = lambda *args: self.resetHandPose() )
							with pm.frameLayout( l='L Hand', labelVisible=True, collapsable=True, collapse=False, backgroundShade=True, marginWidth=5):
								with pm.columnLayout(adjustableColumn=True):
									self.LhandFistSlide = pm.floatSliderGrp(label='Fist', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Thumb', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandThumbBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.LhandThumbBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.LhandThumbBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.LhandThumbBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.LhandThumbBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.LhandThumbBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.LhandThumbMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.LhandThumbTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Thumb Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Thumb Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Thumb Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lthumb_offset_remap_rotz_c') )
									with pm.frameLayout( l='Index', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandIndexBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.LhandIndexBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.LhandIndexBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.LhandIndexBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.LhandIndexBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.LhandIndexBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.LhandIndexMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.LhandIndexTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Index Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Index Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Index Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_remap_rotz_c') )
									with pm.frameLayout( l='Middle', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandMiddleBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.LhandMiddleBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.LhandMiddleBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.LhandMiddleBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.LhandMiddleBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.LhandMiddleBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.LhandMiddleMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.LhandMiddleTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Middle Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Middle Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Middle Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_remap_rotz_c') )
									with pm.frameLayout( l='Ring', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandRingBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.LhandRingBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.LhandRingBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.LhandRingBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.LhandRingBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.LhandRingBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.LhandRingMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.LhandRingTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Ring Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Ring Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Ring Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_remap_rotz_c') )
									with pm.frameLayout( l='Pinky', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandPinkyBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.LhandPinkyBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.LhandPinkyBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.LhandPinkyBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.LhandPinkyBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.LhandPinkyBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.LhandPinkyMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.LhandPinkyTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Pinky Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Pinky Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Pinky Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_remap_rotz_c') )
									self.LhandSpreadSlide = pm.floatSliderGrp(label='Spread', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Spread', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandThumbSpreadFields = pm.floatSliderGrp(label='Thumb', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.LhandIndexSpreadFields = pm.floatSliderGrp(label='Index', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.LhandIMiddleSpreadFields = pm.floatSliderGrp(label='Middle', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.LhandRingSpreadFields = pm.floatSliderGrp(label='Ring', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.LhandPinkySpreadFields = pm.floatSliderGrp(label='Pinky', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )

									self.LhandCupSlide = pm.floatSliderGrp(label='Cup', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Index', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandIndexCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Index RX', value1=0, value2=0, value3=0 )
											self.LhandIndexCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Index RY', value1=0, value2=0, value3=0 )
											self.LhandIndexCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Index RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Index In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lindex_offset_cup_rotz') )
									with pm.frameLayout( l='Middle', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandMiddleCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RX', value1=0, value2=0, value3=0 )
											self.LhandMiddleCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RY', value1=0, value2=0, value3=0 )
											self.LhandMiddleCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Middle In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lmiddle_offset_cup_rotz') )
									with pm.frameLayout( l='Ring', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandRingCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RX', value1=0, value2=0, value3=0 )
											self.LhandRingCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RY', value1=0, value2=0, value3=0 )
											self.LhandRingCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Ring In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lring_offset_cup_rotz') )
									with pm.frameLayout( l='Pinky', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.LhandPinkyCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RX', value1=0, value2=0, value3=0 )
											self.LhandPinkyCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RY', value1=0, value2=0, value3=0 )
											self.LhandPinkyCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Pinky In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Lpinky_offset_cup_rotz') )
							with pm.frameLayout( l='R Hand', labelVisible=True, collapsable=True, collapse=False, backgroundShade=True, marginWidth=5):
								with pm.columnLayout(adjustableColumn=True):
									self.RhandFistSlide = pm.floatSliderGrp(label='Fist', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Thumb', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandThumbBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.RhandThumbBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.RhandThumbBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.RhandThumbBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.RhandThumbBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.RhandThumbBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.RhandThumbMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.RhandThumbTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Thumb Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Thumb Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Thumb Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rthumb_offset_remap_rotz_c') )
									with pm.frameLayout( l='Index', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandIndexBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.RhandIndexBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.RhandIndexBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.RhandIndexBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.RhandIndexBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.RhandIndexBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.RhandIndexMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.RhandIndexTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Index Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Index Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Index Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_remap_rotz_c') )
									with pm.frameLayout( l='Middle', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandMiddleBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.RhandMiddleBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.RhandMiddleBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.RhandMiddleBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.RhandMiddleBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.RhandMiddleBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.RhandMiddleMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.RhandMiddleTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Middle Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Middle Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Middle Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_remap_rotz_c') )
									with pm.frameLayout( l='Ring', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandRingBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.RhandRingBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.RhandRingBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.RhandRingBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.RhandRingBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.RhandRingBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.RhandRingMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.RhandRingTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Ring Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Ring Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Ring Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_remap_rotz_c') )
									with pm.frameLayout( l='Pinky', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandPinkyBaseTransXFields = pm.floatFieldGrp(numberOfFields=3, label='Base TX', value1=0, value2=0, value3=0 )
											self.RhandPinkyBaseTransYFields = pm.floatFieldGrp(numberOfFields=3, label='Base TY', value1=0, value2=0, value3=0 )
											self.RhandPinkyBaseTransZFields = pm.floatFieldGrp(numberOfFields=3, label='Base TZ', value1=0, value2=0, value3=0 )
											self.RhandPinkyBaseRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Base RX', value1=0, value2=0, value3=0 )
											self.RhandPinkyBaseRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Base RY', value1=0, value2=0, value3=0 )
											self.RhandPinkyBaseRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Base RZ', value1=0, value2=0, value3=0 )
											self.RhandPinkyMidRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Mid RZ', value1=0, value2=0, value3=0 )
											self.RhandPinkyTipRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Tip RZ', value1=0, value2=0, value3=0 )
											with pm.rowColumnLayout( numberOfColumns=7, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Pinky Base' )
												pm.button( label='tx', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_transx_a') )
												pm.button( label='ty', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_transy_a') )
												pm.button( label='tz', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_transz_a') )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_rotx_a') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_roty_a') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_rotz_a') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Pinky Mid' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_rotz_b') )
											with pm.rowColumnLayout( numberOfColumns=2, columnWidth=[(1, 150), (2, 50)] ):
												pm.text( label='Pinky Tip' )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_remap_rotz_c') )
									self.RhandSpreadSlide = pm.floatSliderGrp(label='Spread', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Spread', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandThumbSpreadFields = pm.floatSliderGrp(label='Thumb', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.RhandIndexSpreadFields = pm.floatSliderGrp(label='Index', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.RhandIMiddleSpreadFields = pm.floatSliderGrp(label='Middle', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.RhandRingSpreadFields = pm.floatSliderGrp(label='Ring', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )
											self.RhandPinkySpreadFields = pm.floatSliderGrp(label='Pinky', field=True, minValue=-5,
												maxValue=5,fieldMinValue=-5,fieldMaxValue=5,value=0 )

									self.RhandCupSlide = pm.floatSliderGrp(label='Cup', field=True, minValue=-100, maxValue=100, fieldMinValue=-100, fieldMaxValue=100, value=0 )
									with pm.frameLayout( l='Index', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandIndexCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Index RX', value1=0, value2=0, value3=0 )
											self.RhandIndexCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Index RY', value1=0, value2=0, value3=0 )
											self.RhandIndexCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Index RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Index In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rindex_offset_cup_rotz') )
									with pm.frameLayout( l='Middle', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandMiddleCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RX', value1=0, value2=0, value3=0 )
											self.RhandMiddleCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RY', value1=0, value2=0, value3=0 )
											self.RhandMiddleCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Middle RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Middle In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rmiddle_offset_cup_rotz') )
									with pm.frameLayout( l='Ring', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandRingCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RX', value1=0, value2=0, value3=0 )
											self.RhandRingCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RY', value1=0, value2=0, value3=0 )
											self.RhandRingCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Ring RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Ring In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rring_offset_cup_rotz') )
									with pm.frameLayout( l='Pinky', labelVisible=True, collapsable=True, collapse=True, backgroundShade=True, marginWidth=20):
										with pm.columnLayout(adjustableColumn=True):
											self.RhandPinkyCupRotXFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RX', value1=0, value2=0, value3=0 )
											self.RhandPinkyCupRotYFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RY', value1=0, value2=0, value3=0 )
											self.RhandPinkyCupRotZFields = pm.floatFieldGrp(numberOfFields=3, label='Pinky RZ', value1=0, value2=0, value3=0 )

											with pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 150), (2, 50), (3, 50), (4, 50), (5, 50), (6, 50), (7, 50)] ):
												pm.text( label='Pinky In Hand' )
												pm.button( label='rx', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_cup_rotx') )
												pm.button( label='ry', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_cup_roty') )
												pm.button( label='rz', c = lambda *args: self.selectCharNode(nodeName='Rpinky_offset_cup_rotz') )




	def runSetupDummyRigCB(self):
		kwargs={}
		kwargs['symmetry'] = self.dummySymmetry.getValue()
		kwargs['isClavicle'] = self.withRealClavicle.getValue()
		if pm.objExists(DummyRigName):
			if not _confirm_action(
					'Replace dummy rig',
					'Replace the existing {0}?'.format(DummyRigName), 'Replace'):
				return
			kwargs['replace_existing'] = True
		SetupDummyRig(**kwargs)

	def runShowDummyPartsCB(self, type='All'):
		kwargs={}
		kwargs['type'] = type
		changeVisibleDummyParts(**kwargs)

	def runSetBallToGroundCB(self):
		kwargs={}
		kwargs['symmetry'] = self.dummySymmetry.getValue()
		setBallToGround(**kwargs)

	def runExportCB(self):
		return exportRigSet()

	def runImportCB(self):
		importRigSet()

	def runDeleteDummyRigCB(self):
		DeleteDummyRig()

	def runGenerateJointSetCB(self):
		if not pm.objExists(DummyRigName):
			pm.warning('Create the dummy rig before generating joints.')
			return

		footAxisType = self.footPoleVectorType.getSelect()
		armAxisType = self.armPoleVectorType.getSelect()

		kwargs={}
		kwargs['symmetry'] = self.dummySymmetry.getValue()
		kwargs['isClavicle'] = self.withRealClavicle.getValue()
		kwargs['armPoleType'] = armAxisType
		kwargs['footPoleType'] = footAxisType
		kwargs['isAnklePlane'] = self.PreserveAnklePlane.getValue()
		kwargs['isWeightSplineJnt'] = self.WeightSplineJntCheck.getValue()
		if pm.objExists(JointGrpName):
			if not _confirm_action(
					'Replace joint rig',
					'Replace the existing {0}?'.format(JointGrpName), 'Replace'):
				return
			kwargs['replace_existing'] = True
		GenerateHumanRig(**kwargs)

		if self.hideDummyRigCheck.getValue():
			pm.ls(DummyRigName)[0].hide()


	def runDeleteJointSetCB(self):
		if pm.objExists(JointGrpName):
			if _confirm_action(
					'Delete joint rig',
					'Delete {0}? This operation changes the current scene.'.format(
						JointGrpName), 'Delete'):
				pm.delete( JointGrpName )

	def runVisibleAxisCB(self, visible=True):
		DPTJoint.showLocalAxis('spline_low_jnt',visible)


	def runEditOrientAxisCB(self):
		nAimAxis = self.AimAxisGrp.getSelect()
		nUpAxis = self.UpAxisGrp.getSelect()
		#print 'nAimAxis:'+str(nAimAxis)
		#print 'nUpAxis:'+str(nUpAxis)
		aimAxis=[0,0,0]
		upAxis=[0,0,0]

		revAim = 1.0
		if self.AimAxisRevCheck.getValue():
			revAim = -1.0

		revUp = 1.0
		if self.UpAxisRevCheck.getValue():
			revUp = -1.0

		if nAimAxis == nUpAxis:
			pm.warning( "The aim and up axis are the same! Orientaiton probably won't work!" )
			return

		aimAxis[nAimAxis-1] = revAim
		upAxis[nUpAxis-1] = revUp
		upDir = (self.WorldUpAxisGrp.getValue1(), self.WorldUpAxisGrp.getValue2(), self.WorldUpAxisGrp.getValue3())

		joints = pm.ls(sl=True)

		if len(joints) == 0:
			pm.warning( "Please select joint!" )
			return

		for jnt in joints:
			DPTJoint.correctJointAxis( joint=jnt, aimAxis=aimAxis, upAxis=upAxis, upDir=upDir )

	def runCrossOrientAxisCB(self):

		joints = pm.ls(sl=True)

		if len(joints) != 3:
			pm.warning( "Please select 3 joint!" )
			return

		aim = (self.CrossAimAxisGrp.getValue1(), self.CrossAimAxisGrp.getValue2(), self.CrossAimAxisGrp.getValue3())
		up = (self.CrossUpAxisGrp.getValue1(), self.CrossUpAxisGrp.getValue2(), self.CrossUpAxisGrp.getValue3())

		DPTJoint.crossCorrectJointAxis( [joints[0].name(),joints[1].name(),joints[2].name()], aimAxis=aim, upAxis=up, reverse=self.CrossRevCheck.getValue())

	def runMirrorCB(self, LSide=1):
		print 'mirror'
		Rjoints = ('up_legR_jnt','clavicleR_jnt', 'real_clavicleR_jnt')
		Ljoints = ('up_legL_jnt','clavicleL_jnt', 'real_clavicleL_jnt')

		if LSide:
			pm.delete( Rjoints )

			for jnt in Ljoints:
				pm.mirrorJoint( jnt, mirrorYZ=True, mirrorBehavior=True, searchReplace=('L', 'R') )
		else:
			pm.delete( Ljoints )

			for jnt in Rjoints:
				pm.mirrorJoint( jnt, mirrorYZ=True, mirrorBehavior=True, searchReplace=('R', 'L') )

	def runCreateRigCB(self):

		########################################################Check Rig groups######################################################################
		checkRigGroup()


		########################################################Create Root Controller######################################################################
		root_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=root_ctrlName, size=60.0)
		root_ctrl_frzGrp, root_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(root_ctrl,(0,0,0),RigGrpName)

		DPTattr.hideAttributes(root_ctrl, ['scaleX','scaleY','scaleZ'])

		DPTattr.createAttributeFromDic(root_ctrl,
			[{'ln':'masterScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':1.0},
			{'ln':'skeletonVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'masterCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'subMasterCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'rigSettingsCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'masterSpineCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'worldSpaceCtrlVis','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0}])

		root_ctrl.masterScale >> root_ctrl.scaleX
		root_ctrl.masterScale >> root_ctrl.scaleY
		root_ctrl.masterScale >> root_ctrl.scaleZ
		root_ctrl.scale.lock()


		#root_ctrl connect joint_grp
		skeletonVis_reverse = DPTnode.createNodeWithParam('reverse', 'skeletonVis_reverse')

		jointGrpTrans = pm.ls(JointGrpName)[0]
		root_ctrl.masterScale >> jointGrpTrans.scaleX
		root_ctrl.masterScale >> jointGrpTrans.scaleY
		root_ctrl.masterScale >> jointGrpTrans.scaleZ
		root_ctrl.skeletonVis >> skeletonVis_reverse.inputX

		skeletonVis_reverse.outputX >> jointGrpTrans.overrideLevelOfDetail
		skeletonVis_reverse.outputX >> jointGrpTrans.overrideEnabled



		#create master ctrl
		master_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=master_ctrlName, size=70.0)
		master_ctrl_frzGrp, master_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(master_ctrl,(0,0,0),RigGrpName)
		master_ctrl.scale.lock()
		DPTattr.hideAttributes(master_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

		root_ctrl.masterCtrlVis >> master_ctrl.visibility

		#create sub master ctrl
		subMaster_ctrl = DPTrigCAct.createRigController(ctlObj=None, name=subMaster_ctrlName, size=65.0)
		subMaster_ctrl_frzGrp, subMaster_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(subMaster_ctrl,(0,0,0),RigGrpName)
		subMaster_ctrl.scale.lock()
		DPTattr.hideAttributes(subMaster_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

		root_ctrl.subMasterCtrlVis >> subMaster_ctrl.visibility

		pm.parentConstraint( subMaster_ctrl, root_ctrl_frzGrp, maintainOffset=True )
		pm.parentConstraint( master_ctrl, subMaster_ctrl_frzGrp, maintainOffset=True )

		#rootCtrl_space_switch_grp
		rootCtrl_space_switch_grp = DPTgrp.createEmpty('rootCtrl_space_switch_grp',RigGrpName)

		pm.parentConstraint( root_ctrl, rootCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> rootCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> rootCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> rootCtrl_space_switch_grp.scaleZ

		#worldSpaceCtrl_space_switch_grp
		worldSpaceCtrl_space_switch_grp = DPTgrp.createEmpty('worldSpaceCtrl_space_switch_grp',RigGrpName)


		root_ctrl.masterScale >> worldSpaceCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> worldSpaceCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> worldSpaceCtrl_space_switch_grp.scaleZ

		#hipCtrl_space_switch_grp
		hipCtrl_space_switch_grp = DPTgrp.createEmpty('hipCtrl_space_switch_grp',RigGrpName, pm.xform('spline_low_jnt', q=True, ws=True, t=True ))

		#cogCtrl_space_switch_grp
		cogCtrl_space_switch_grp = DPTgrp.createEmpty('cogCtrl_space_switch_grp',RigGrpName, pm.xform('spline_low_jnt', q=True, ws=True, t=True ))

		#lowSpineCtrl_space_switch_grp
		lowSpineCtrl_space_switch_grp = DPTgrp.createEmpty('lowSpineCtrl_space_switch_grp',RigGrpName, pm.xform('spline_low_jnt', q=True, ws=True, t=True ))

		#midSpineCtrl_1_space_switch_grp
		midSpineCtrl_1_space_switch_grp = DPTgrp.createEmpty('midSpineCtrl_1_space_switch_grp',RigGrpName, pm.xform('spline_mid_jnt', q=True, ws=True, t=True ))

		#hiSpineCtrl_space_switch_grp
		hiSpineCtrl_space_switch_grp = DPTgrp.createEmpty('hiSpineCtrl_space_switch_grp',RigGrpName, pm.xform('spline_hi_jnt', q=True, ws=True, t=True ))

		#masterSpineCtrl_space_switch_grp
		masterSpineCtrl_space_switch_grp = DPTgrp.createEmpty('masterSpineCtrl_space_switch_grp',RigGrpName, pm.xform('spline_mid_jnt', q=True, ws=True, t=True ))



		########################################################Create Spline Controller######################################################################
		spine_rig_grp = DPTgrp.createEmpty('spline_rig_grp',RigGrpName)

		root_ctrl.masterScale >> spine_rig_grp.scaleX
		root_ctrl.masterScale >> spine_rig_grp.scaleY
		root_ctrl.masterScale >> spine_rig_grp.scaleZ

		#create spineNoScale_rig_grp
		spineNoScale_rig_grp = DPTgrp.createEmpty('splineNoScale_rig_grp',spine_rig_grp)
		spineNoScale_rig_grp.setAttr('inheritsTransform', 0)


		#create spineShaper_jnt
		jntPos = DPTtrans.getPositionBetweenObjects(source='spline_hi_jnt', target='spline_mid_jnt', param=0.5)
		spineShaper_jnt = pm.joint( name='splineShaper_jnt', p=jntPos)
		pm.parent(spineShaper_jnt, spine_rig_grp)
		spineShaper_jnt.hide()


		#create ik curve
		ikSpline_crv = DPTCurveTool.createCurveAlongObject(name='ikSpline_crv', objects= ['hip_jnt','spline_low_jnt','spline_mid_jnt',spineShaper_jnt,'spline_hi_jnt','spline_end_jnt'],degree=3)
		ikSpline_crvShape = ikSpline_crv.getShape()
		pm.parent(ikSpline_crv, spineNoScale_rig_grp)

		#my add
		pm.rebuildCurve(ikSpline_crv,rpo=1, rt=0, end=1, kr=0, kcp=0, kep=1, kt=0, s=3, d=3, tol=0.01)

		#create ik surf and follicle
		ikSpline_crvLoft1 = pm.duplicate(ikSpline_crv, n=ikSpline_crv+'_loft1')[0]
		ikSpline_crvLoft2 = pm.duplicate(ikSpline_crv, n=ikSpline_crv+'_loft2')[0]
		pm.move( -0.5, 0, 0,ikSpline_crvLoft1, r=True )
		pm.move( 0.5, 0, 0,ikSpline_crvLoft2, r=True )
		spine_ribbon_surf = pm.loft(ikSpline_crvLoft1,ikSpline_crvLoft2, ch=False, rn=True, degree=1, name='spline_ribbon_surf' )[0]
		pm.delete(ikSpline_crvLoft1, ikSpline_crvLoft2)
		pm.parent(spine_ribbon_surf, spineNoScale_rig_grp)

		spineRhFollicles_grp = DPTgrp.createEmpty('splineRhFollicles_grp',spineNoScale_rig_grp)
		floList=[]

		for i in range(6):
			fol = DPTfollicle.create_follicle(spine_ribbon_surf, uv=[0.5,(1.0/7.0)*float(i+1)])
			fol.getParent().hide()
			floList.append(fol)
			pm.parent(fol,spineRhFollicles_grp)

		#create worldSpace_ctrl
		worldSpace_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='worldSpace_ctrl', size=5.0)
		worldSpace_ctrl_frzGrp, worldSpace_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(worldSpace_ctrl,(0,0,0),spine_rig_grp)
		root_ctrl.worldSpaceCtrlVis >> worldSpace_ctrl.visibility

		pm.parentConstraint( worldSpace_ctrl, worldSpaceCtrl_space_switch_grp, maintainOffset=True )

		'''
		#create spineShaper_jnt
		jntPos = DPTCurveTool.getCurveParamPosition(curve=ikSpline_crv, param=0.6)
		spineShaper_jnt = pm.joint( name='splineShaper_jnt', p=jntPos)
		pm.parent(spineShaper_jnt, spine_rig_grp)
		spineShaper_jnt.hide()
		'''

		#create ctrlSpine_rig_grp
		ctrlSpine_rig_grp = DPTgrp.createEmpty('ctrlSpline_rig_grp',spine_rig_grp)

		weightJoints = pm.listRelatives(spineWeight_grp_Name, children=True, type=('joint'))
		weightJointCount=0

		for j in weightJoints:
			jntName = DPTStr.replaceText(j,{'weight_jnt':'ctrl_jnt'})
			newJnt = pm.duplicate(j, n=jntName, parentOnly=True)[0]
			newJnt.hide()
			#ctrlSpine_frzGrp = DPTgrp.createEmpty(newJnt+'_frzGrp',ctrlSpine_rig_grp)
			ctrlSpine_frzGrp = DPTgrp.createEmptyWithAlign(name=newJnt+'_frzGrp', alignObj=newJnt, isTrans=True, isRot=False, isScale=False, isUnParent=True)
			pm.parent(newJnt, ctrlSpine_frzGrp)
			pm.parent(ctrlSpine_frzGrp, ctrlSpine_rig_grp)
			pm.parentConstraint( newJnt, j, maintainOffset=True )
			pCons = pm.parentConstraint( floList[weightJointCount].getParent(), ctrlSpine_frzGrp, maintainOffset=True )
			weightJointCount+=1


		#create hip_ctrl
		hip_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='hip_ctrl', size=20.0,position=pm.xform('hip_jnt', q=True, ws=True, t=True))
		hip_ctrl_frzGrp = DPTgrp.createEmpty('hip_ctrl_frzGrp', spine_rig_grp, position=pm.xform('spline_low_jnt', q=True, ws=True, t=True))
		hipCtrl_space_grp = DPTgrp.createEmpty('hipCtrl_space_grp', hip_ctrl_frzGrp)
		lowHip_ctrl_frzGrp = DPTgrp.createEmpty('lowHip_ctrl_frzGrp', hipCtrl_space_grp, position=pm.xform('hip_jnt', q=True, ws=True, t=True))
		lowHip_ctrl_zeroFrzGrp = DPTgrp.createEmpty('lowHip_ctrl_zeroFrzGrp', lowHip_ctrl_frzGrp, position=pm.xform('hip_jnt', q=True, ws=True, t=True))
		pm.parent(hip_ctrl,lowHip_ctrl_zeroFrzGrp)
		DPTattr.hideAttributes(hip_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		hip_ctrl.scale.lock()

		altHip_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='altHip_ctrl', size=10.0,position=pm.xform('spline_low_jnt', q=True, ws=True, t=True))
		altHip_ctrl_frzGrp, altHip_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(altHip_ctrl,pm.xform('spline_low_jnt', q=True, ws=True, t=True),hip_ctrl)
		DPTattr.hideAttributes(altHip_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		altHip_ctrl.scale.lock()

		DPTattr.createAttributeFromDic(hip_ctrl,
			[{'ln':'splineInfluence','k':True,'at':'float','hnv':True,'min':0.0,'dv':15.0},
			{'ln':'altPivotVis','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':False},
			{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},])

		hip_ctrl.setAttr('altPivotVis', e=True, channelBox=True)
		hip_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

		pm.parentConstraint( altHip_ctrl, 'hip_jnt', maintainOffset=True )
		pm.parentConstraint( altHip_ctrl, hipCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> hipCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> hipCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> hipCtrl_space_switch_grp.scaleZ


		#create splineLow_ctrl
		splineLowPos = pm.xform('spline_low_jnt', q=True, ws=True, t=True)
		splineLow_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='splineLow_ctrl', size=15.0,position=splineLowPos)
		splineLow_ctrl_frzGrp_top = DPTgrp.createEmpty('splineLow_ctrl_frzGrp_top', spine_rig_grp, position=splineLowPos)
		splineLow_ctrl_space_grp = DPTgrp.createEmpty('splineLow_ctrl_space_grp', splineLow_ctrl_frzGrp_top)
		splineLow_ctrl_frzGrp = DPTgrp.createEmpty('splineLow_ctrl_frzGrp', splineLow_ctrl_space_grp, position=splineLowPos)
		splineLow_ctrl_frzGrpForCons = DPTgrp.createEmpty('splineLow_ctrl_frzGrp_forCons', splineLow_ctrl_frzGrp, position=splineLowPos)
		splineLow_ctrl_zeroFrzGrp = DPTgrp.createEmpty('splineLow_ctrl_zeroFrzGrp', splineLow_ctrl_frzGrpForCons, position=splineLowPos)
		pm.parent(splineLow_ctrl,splineLow_ctrl_zeroFrzGrp)

		DPTattr.hideAttributes(splineLow_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		splineLow_ctrl.scale.lock()

		DPTattr.createAttributeFromDic(splineLow_ctrl,
			[{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0}])

		splineLow_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

		pm.parentConstraint(  splineLow_ctrl,'spline_low_jnt', maintainOffset=True )
		pm.parentConstraint(  splineLow_ctrl, lowSpineCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> lowSpineCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> lowSpineCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> lowSpineCtrl_space_switch_grp.scaleZ

		#create splineHigh_ctrl_frzGrp
		splineHiPos = pm.xform('spline_hi_jnt', q=True, ws=True, t=True)
		splineHigh_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='splineHigh_ctrl', size=15.0, position=splineHiPos)
		splineHigh_ctrl_frzGrp_top = DPTgrp.createEmpty('splineHigh_ctrl_frzGrp_top', spine_rig_grp, position=splineHiPos)
		splineHigh_ctrl_space_grp = DPTgrp.createEmpty('spineHigh_ctrl_space_grp', splineHigh_ctrl_frzGrp_top)
		splineHigh_ctrl_frzGrp = DPTgrp.createEmpty('splineHigh_ctrl_frzGrp', splineHigh_ctrl_space_grp, position=splineHiPos)
		splineHigh_ctrl_frzGrpForCons = DPTgrp.createEmpty('splineHigh_ctrl_frzGrp_forCons', splineHigh_ctrl_frzGrp, position=splineHiPos)
		splineHigh_ctrl_zeroFrzGrp = DPTgrp.createEmpty('splineHigh_ctrl_zeroFrzGrp', splineHigh_ctrl_frzGrpForCons, position=splineHiPos)
		pm.parent(splineHigh_ctrl,splineHigh_ctrl_zeroFrzGrp)

		DPTattr.hideAttributes(splineHigh_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		splineHigh_ctrl.scale.lock()

		DPTattr.createAttributeFromDic(splineHigh_ctrl,
			[{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'midSpineCtrl_1','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'useCogSpace','k':True,'at':'float','dv':0.0}])

		splineHigh_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

		pm.parentConstraint( splineHigh_ctrl, 'spline_hi_jnt', maintainOffset=True )
		pm.parentConstraint(  splineHigh_ctrl, hiSpineCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> hiSpineCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> hiSpineCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> hiSpineCtrl_space_switch_grp.scaleZ

		#create spineMid_1_ctrl
		spineMid1ctrlPos = pm.xform('spline_mid_jnt', q=True, ws=True, t=True)
		spineMid_ctrl_space_grp = DPTgrp.createEmpty('spineMid_ctrl_space_grp', spine_rig_grp)
		splineMid_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='splineMid_ctrl', size=15.0, position=spineMid1ctrlPos)
		splineMid_ctrl_frzGrp = DPTgrp.createEmpty('splineMid_ctrl_frzGrp', spineMid_ctrl_space_grp, position=spineMid1ctrlPos)
		splineMid_ctrl_frzGrpForCons = DPTgrp.createEmpty('splineMid_ctrl_frzGrp_forCons', splineMid_ctrl_frzGrp, position=spineMid1ctrlPos)
		splineHigh_ctrl_zeroFrzGrp = DPTgrp.createEmpty('splineHigh_ctrl_zeroFrzGrp', splineMid_ctrl_frzGrpForCons, position=spineMid1ctrlPos)
		pm.parent(splineMid_ctrl,splineHigh_ctrl_zeroFrzGrp)

		DPTattr.hideAttributes(splineMid_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		splineMid_ctrl.scale.lock()

		DPTattr.createAttributeFromDic(splineMid_ctrl,
			[{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'cogCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'lowSpineCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'useCogSpace','k':True,'at':'float','dv':0.0}])
		splineMid_ctrl.setAttr('________SPACES___', e=True, channelBox=True)

		pm.parentConstraint( splineMid_ctrl, 'spline_mid_jnt', maintainOffset=True )
		pm.parentConstraint(  splineMid_ctrl, midSpineCtrl_1_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> midSpineCtrl_1_space_switch_grp.scaleX
		root_ctrl.masterScale >> midSpineCtrl_1_space_switch_grp.scaleY
		root_ctrl.masterScale >> midSpineCtrl_1_space_switch_grp.scaleZ


		#create masterSpine_ctrl
		masSplinePos = pm.xform('spline_mid_jnt', q=True, ws=True, t=True)
		masterSpine_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='masterSpine_ctrl', size=30, position=masSplinePos)
		masterSpine_ctrl_frzGrp, masterSpine_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(masterSpine_ctrl,masSplinePos,spine_rig_grp)

		DPTattr.hideAttributes(masterSpine_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		masterSpine_ctrl.scale.lock()

		root_ctrl.masterSpineCtrlVis >> masterSpine_ctrl.visibility

		pm.parentConstraint(  masterSpine_ctrl, masterSpineCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> masterSpineCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> masterSpineCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> masterSpineCtrl_space_switch_grp.scaleZ


		#create spineShaper_ctrl
		spineShaperJntPos = pm.xform(spineShaper_jnt, q=True, ws=True, t=True)
		spineShaper_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='spineShaper_ctrl', size=30, position=spineShaperJntPos)
		spineShaper_ctrl_frzGrp, spineShaper_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRig(spineShaper_ctrl,spineShaperJntPos,None)
		spineShaper_ctrl_frzGrp_top = DPTgrp.createEmpty('spineShaper_ctrl_frzGrp_top', spine_rig_grp, position=spineShaperJntPos)
		pm.parent(spineShaper_ctrl_frzGrp, spineShaper_ctrl_frzGrp_top)

		DPTattr.hideAttributes(spineShaper_ctrl, ['rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])
		spineShaper_ctrl.rotate.lock()
		spineShaper_ctrl.scale.lock()

		pm.parentConstraint( spineShaper_ctrl, spineShaper_jnt, maintainOffset=True )
		pm.parentConstraint( splineMid_ctrl, spineShaper_ctrl_frzGrp_top, maintainOffset=True )

		pm.parentConstraint( splineMid_ctrl, splineHigh_ctrl, spineShaper_ctrl_frzGrp, maintainOffset=True )


		#create rigSettings_ctrl
		rigSettings_ctrl = DPTrigCAct.createRigController(ctlObj=None, name='rigSettings_ctrl', size=10.0, position=(35,0,-35))
		rigSettings_ctrl_zeroFrzGrp = DPTgrp.createEmpty('rigSettings_ctrl_zeroFrzGrp', spine_rig_grp, position=(35,0,-35))
		pm.parent(rigSettings_ctrl, rigSettings_ctrl_zeroFrzGrp)

		DPTattr.hideAttributes(rigSettings_ctrl, ['translateX','translateY','translateZ','rotateX','rotateY','rotateZ','scaleX','scaleY','scaleZ','visibility'])


		DPTattr.createAttributeFromDic(rigSettings_ctrl,
			[{'ln':'Spine_Master_Control_______','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'spineLow_Ctrl_Rotation','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.333},
			{'ln':'spineMid_1_Ctrl_Rotation','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.333},
			{'ln':'spineHigh_Ctrl_Rotation','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.333},
			{'ln':'Spine_Squash_And_Stretch___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'spineMaxStretchAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.8},
			{'ln':'spineStretchSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.9},
			{'ln':'spineMaxSquashAmt','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.8},
			{'ln':'spineSquashSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.8},
			{'ln':'spine_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.981},
			{'ln':'spine_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'spine_3_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.865},
			{'ln':'spine_4_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.577},
			{'ln':'spine_5_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.135},
			{'ln':'spine_6_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
			{'ln':'lfUpLegReaderRotOrder','k':True,'at':'enum','enumName':'xyz:yzx:zxy:xzy:yxz:zyx','dv':5},
			{'ln':'______LF___UPLEG___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftUpLeg','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.0},
			{'ln':'leftUpLeg_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.327},
			{'ln':'leftUpLeg_3','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.655},
			{'ln':'______LF__LOWLEG___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftLowLeg_1','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'leftLowLeg_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'LeftLeg_Squash_And_Stretch___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftLegMaxStretchAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.8},
			{'ln':'leftLegStretchSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.2},
			{'ln':'leftLegMaxSquashAmt','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.5},
			{'ln':'leftLegSquashSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.7},
			{'ln':'lfUpLeg_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'lfUpLegSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.675},
			{'ln':'lfUpLegSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'lfKnee_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'lfLowLegSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'lfLowLegSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'lfAnkle_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'rtUpLegReaderRotOrder','k':True,'at':'enum','enumName':'xyz:yzx:zxy:xzy:yxz:zyx','dv':5},
			{'ln':'______RT___UPLEG___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightUpLeg','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.0},
			{'ln':'rightUpLeg_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.327},
			{'ln':'rightUpLeg_3','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.655},
			{'ln':'______RT__LOWLEG___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightLowLeg_1','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'rightLowLeg_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'RightLeg_Squash_And_Stretch___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightLegMaxStretchAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.8},
			{'ln':'rightLegStretchSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.2},
			{'ln':'rightLegMaxSquashAmt','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.5},
			{'ln':'rightLegSquashSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.7},
			{'ln':'rtUpLeg_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'rtUpLegSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'rtUpLegSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'rtKnee_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'rtLowLegSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'rtLowLegSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'rtAnkle_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'lfUpArmReaderRotOrder','k':True,'at':'enum','enumName':'xyz:yzx:zxy:xzy:yxz:zyx','dv':0},
			{'ln':'______LF___UPARM___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftUpArm','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.0},
			{'ln':'leftUpArm_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'leftUpArm_3','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'______LF_FOREARM___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftForearm_1','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'leftForearm_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'leftWrist','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':1.0},
			{'ln':'LeftArm_Squash_And_Stretch___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftArmMaxStretchAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.8},
			{'ln':'leftArmStretchSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.2},
			{'ln':'leftArmMaxSquashAmt','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.5},
			{'ln':'leftArmSquashSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.7},
			{'ln':'lfUpArm_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'lfUpArmSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'lfUpArmSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'lfElbow_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'lfForeArmSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'lfForeArmSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'lfWrist_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'rtUpArmReaderRotOrder','k':True,'at':'enum','enumName':'xyz:yzx:zxy:xzy:yxz:zyx','dv':1},
			{'ln':'______RT___UPARM___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightUpArm','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.0},
			{'ln':'rightUpArm_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'rightUpArm_3','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'______RT_FOREARM___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightForearm_1','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.333},
			{'ln':'rightForearm_2','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':0.667},
			{'ln':'rightWrist','k':True,'at':'float','hnv':True,'hxv':True,'min':-1.0,'max':1.0,'dv':1.0},
			{'ln':'RightArm_Squash_And_Stretch___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightArmMaxStretchAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.8},
			{'ln':'rightArmStretchSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.2},
			{'ln':'rightArmMaxSquashAmt','k':True,'at':'float','hnv':True,'min':0.0,'dv':1.5},
			{'ln':'rightArmSquashSpeed','k':True,'at':'float','hnv':True,'min':0.0,'dv':0.7},
			{'ln':'rtUpArm_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'rtUpArmSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'rtUpArmSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'rtElbow_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'rtForeArmSplit_1_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.918},
			{'ln':'rtForeArmSplit_2_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.673},
			{'ln':'rtWrist_SsScale','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.265},
			{'ln':'_LF_MASTER_SPREAD___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'leftthumbAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':1},
			{'ln':'leftindexAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':0.5},
			{'ln':'leftmiddleAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':0.0},
			{'ln':'leftringAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':-0.5},
			{'ln':'leftpinkyAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':-1},
			{'ln':'LF_MASTER_FINGER_CTRLS','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'lfCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':-15},
			{'ln':'lfSpreadCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':8},
			{'ln':'lfRollCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':1},
			{'ln':'lfMidCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':-3},
			{'ln':'lfTipCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':-4},
			{'ln':'_RT_MASTER_SPREAD___','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rightthumbAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':1},
			{'ln':'rightindexAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':0.5},
			{'ln':'rightmiddleAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':0.0},
			{'ln':'rightringAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':-0.5},
			{'ln':'rightpinkyAmt','k':True,'at':'float','hnv':True,'hxv':True,'min':-100.0,'max':100.0,'dv':-1},
			{'ln':'RT_MASTER_FINGER_CTRLS','k':True,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rtCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':15},
			{'ln':'rtSpreadCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':8},
			{'ln':'rtRollCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':1},
			{'ln':'rtMidCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':-3},
			{'ln':'rtTipCurlCtrlSpeed','k':True,'at':'float','hnv':True,'hxv':True,'min':-50.0,'max':50.0,'dv':-4}])


		root_ctrl.rigSettingsCtrlVis >> rigSettings_ctrl.visibility

		pm.parentConstraint(root_ctrl, rigSettings_ctrl, maintainOffset=True)
		rigSettings_ctrl.translate.lock()
		rigSettings_ctrl.rotate.lock()
		rigSettings_ctrl.scale.lock()






		#create cog
		cog_ctrl_frzGrp = DPTgrp.createEmpty('cog_ctrl_frzGrp', spine_rig_grp,  pm.xform('spline_low_jnt', q=True, ws=True, t=True ))
		cogCtrl_space_grp = DPTgrp.createEmpty('cogCtrl_space_grp', cog_ctrl_frzGrp)

		srcVec = pmu.VectorN(pm.xform('up_legL_jnt', q=True, ws=True, t=True))
		targetVec = pmu.VectorN(pm.xform('up_legR_jnt', q=True, ws=True, t=True))
		cogCenterPosVec = srcVec+(targetVec-srcVec)*0.5

		lowCog_ctrl_frzGrp = DPTgrp.createEmpty('lowCog_ctrl_frzGrp', cogCtrl_space_grp, (cogCenterPosVec[0],cogCenterPosVec[1],cogCenterPosVec[2]))
		lowCog_ctrl_zeroFrzGrp = pm.duplicate(lowCog_ctrl_frzGrp, n='lowCog_ctrl_zeroFrzGrp', parentOnly=True)[0]
		pm.parent(lowCog_ctrl_zeroFrzGrp, lowCog_ctrl_frzGrp)
		cog_ctrl = DPTrigCAct.createRigController(None,'cog_ctrl', 60.0, (cogCenterPosVec[0],cogCenterPosVec[1],cogCenterPosVec[2]))
		pm.parent(cog_ctrl, lowCog_ctrl_zeroFrzGrp)
		DPTattr.hideAttributes(cog_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

		DPTattr.createAttributeFromDic(cog_ctrl,
			[{'ln':'altPivotVis','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':True},
			{'ln':'spineShaper','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':True},
			{'ln':'stretchValue','h':False,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'autoSquashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'squashStretch','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':10.0,'dv':1.0},
			{'ln':'________SPACES___','h':False,'at':'bool','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':False},
			{'ln':'rootCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':1.0},
			{'ln':'worldSpaceCtrl','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}])

		cog_ctrl.setAttr('altPivotVis', e=True, channelBox=True)
		cog_ctrl.setAttr('spineShaper', e=True, channelBox=True)
		cog_ctrl.setAttr('stretchValue', e=True, channelBox=True)
		cog_ctrl.setAttr('________SPACES___', e=True, channelBox=True)
		cog_ctrl.scale.lock()


		altcog_ctrl_frzGrp = DPTgrp.createEmpty('altcog_ctrl_frzGrp', cog_ctrl,pm.xform('spline_low_jnt', q=True, ws=True, t=True ))
		altcog_ctrl_zeroFrzGrp = DPTgrp.createEmpty('altcog_ctrl_zeroFrzGrp', altcog_ctrl_frzGrp,pm.xform('spline_low_jnt', q=True, ws=True, t=True ))
		altCog_ctrl = DPTrigCAct.createRigController(None,'altCog_ctrl', 50.0, pm.xform('spline_low_jnt', q=True, ws=True, t=True ))
		pm.parent(altCog_ctrl, altcog_ctrl_zeroFrzGrp)
		DPTattr.hideAttributes(altCog_ctrl, ['scaleX','scaleY','scaleZ','visibility'])
		altCog_ctrl.scale.lock()

		pm.parentConstraint( altCog_ctrl, cogCtrl_space_switch_grp, maintainOffset=True )
		root_ctrl.masterScale >> cogCtrl_space_switch_grp.scaleX
		root_ctrl.masterScale >> cogCtrl_space_switch_grp.scaleY
		root_ctrl.masterScale >> cogCtrl_space_switch_grp.scaleZ

		cog_ctrl.spineShaper >> spineShaper_ctrl.visibility

		pm.parentConstraint( altCog_ctrl, masterSpine_ctrl_frzGrp, maintainOffset=True )



		#smoothbind ik curve and loft surf
		splineRibbonSurfSkinClster = pm.skinCluster('hip_jnt','spline_low_jnt','spline_mid_jnt',spineShaper_jnt,'spline_hi_jnt','spline_end_jnt', spine_ribbon_surf, sm=0, tsb=True, mi=1, nw=1)
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][0]', transformValue=[('hip_jnt', 1.0)])
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][1]', transformValue=[('hip_jnt', 0.2),('spline_low_jnt', 0.8)])
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][2]', transformValue=[('spline_mid_jnt', 1.0)])
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][3]', transformValue=[(spineShaper_jnt, 1.0)])
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][4]', transformValue=[('spline_hi_jnt', 1.0)])
		pm.skinPercent( splineRibbonSurfSkinClster, spine_ribbon_surf+'.cv[0:1][5]', transformValue=[('spline_end_jnt', 1.0)])



		ikSkinClster = pm.skinCluster('hip_jnt','spline_low_jnt','spline_mid_jnt',spineShaper_jnt,'spline_hi_jnt','spline_end_jnt', ikSpline_crv, sm=0, tsb=True, mi=1, nw=1)
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[0]', transformValue=[('hip_jnt', 1.0)])
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[1]', transformValue=[('hip_jnt', 0.2),('spline_low_jnt', 0.8)])
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[2]', transformValue=[('spline_mid_jnt', 1.0)])
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[3]', transformValue=[(spineShaper_jnt, 1.0)])
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[4]', transformValue=[('spline_hi_jnt', 1.0)])
		pm.skinPercent( ikSkinClster, ikSpline_crv+'.cv[5]', transformValue=[('spline_end_jnt', 1.0)])


		pm.hide(spine_ribbon_surf,ikSpline_crv)

		#create lattice ikcurve and loft surf
		dfmFFD, dfmLattice, dfmLatticeBase = pm.lattice( ikSpline_crv+'.cv[1:5]',spine_ribbon_surf+'.cv[0:1][1:5]', dv=(2,5,2), oc=True, s=(1,1,1) )

		hipInfluence_dfmLatticeGroup = pm.group(dfmLattice,dfmLatticeBase,n='hipInfluence_dfmLatticeGroup')
		hipInfluence_dfmLatticeGroup.hide()
		pm.parent(hipInfluence_dfmLatticeGroup,lowHip_ctrl_zeroFrzGrp)

		hipInfluence_dfmLatticeGroup.setAttr('translate', (0,0,0))
		dfmLattice.setAttr('translate', (0,0,0))
		dfmLatticeBase.setAttr('translate', (0,0,0))
		dfmLatticeBase.setAttr('scale', (1,1,1))
		dfmLattice.setAttr('scale', (1,1,1))
		dfmFFD.setAttr('outsideLattice', 2)

		pm.parentConstraint( hip_ctrl, dfmLattice, maintainOffset=True )
		hip_ctrl.splineInfluence >> dfmFFD.outsideFalloffDist

		#create ik curve network
		crvInfoNode = pm.createNode( 'curveInfo', n='spline_crvInfo' )
		ikSpline_crv.worldSpace >> crvInfoNode.inputCurve

		spineStretch_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineStretch_multDiv', {'operation':2} )
		scaleAdjustedDefaultArcLen_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'scaleAdjustedDefaultArcLen_multDiv', {'operation':1} )

		#crvInfoNode.arcLength >> scaleAdjustedDefaultArcLen_multDiv.input1X
		root_ctrl.masterScale >> scaleAdjustedDefaultArcLen_multDiv.input2X
		crvInfoNode.arcLength >> spineStretch_multDiv.input1X
		scaleAdjustedDefaultArcLen_multDiv.outputX >> spineStretch_multDiv.input2X

		scaleAdjustedDefaultArcLen_multDiv.setAttr('input1X', pm.arclen( ikSpline_crv ) )

		spineStretch_multDiv.outputX >> cog_ctrl.stretchValue

		spineNormStretchVal_plsMns = DPTnode.createNodeWithParam('plusMinusAverage', 'splineNormStretchVal_plsMns', {'operation':2,'input1D[0]':1} )
		spineStretch_multDiv.outputX >> spineNormStretchVal_plsMns.input1D[1]

		splineNormStretchNegator_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'splineNormStretchNegator_multDiv', {'operation':1,'input1X':-1} )
		spineNormStretchVal_plsMns.output1D >> splineNormStretchNegator_multDiv.input2X

		spineNormStretch_cond = DPTnode.createNodeWithParam('condition', 'splineNormStretch_cond', {'operation':3,'secondTerm':0} )
		spineNormStretchVal_plsMns.output1D >> spineNormStretch_cond.firstTerm
		spineNormStretchVal_plsMns.output1D >> spineNormStretch_cond.colorIfTrueR
		splineNormStretchNegator_multDiv.outputX >> spineNormStretch_cond.colorIfFalseR


		spineStretchDenominator_plsMns = DPTnode.createNodeWithParam('plusMinusAverage', 'spineStretchDenominator_plsMns', {'operation':1} )
		spineStretchSpeed_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineStretchSpeed_multDiv', {'operation':2,'input1X':1} )

		rigSettings_ctrl.spineStretchSpeed >> spineStretchSpeed_multDiv.input2X
		spineNormStretch_cond.outColorR >> spineStretchDenominator_plsMns.input1D[0]
		spineStretchSpeed_multDiv.outputX >> spineStretchDenominator_plsMns.input1D[1]


		spineStretchNumerator_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineStretchNumerator_multDiv', {'operation':1} )
		spineNormStretch_cond.outColorR >> spineStretchNumerator_multDiv.input1X
		rigSettings_ctrl.spineMaxStretchAmt >> spineStretchNumerator_multDiv.input2X

		spineSquashSpeed_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineSquashSpeed_multDiv', {'operation':2,'input1X':1} )
		rigSettings_ctrl.spineSquashSpeed >> spineSquashSpeed_multDiv.input2X


		spineSquashNumerator_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineSquashNumerator_multDiv', {'operation':1} )
		spineNormStretch_cond.outColorR >> spineSquashNumerator_multDiv.input1X
		rigSettings_ctrl.spineMaxSquashAmt >> spineSquashNumerator_multDiv.input2X



		spineSquashDenominator_plsMns = DPTnode.createNodeWithParam('plusMinusAverage', 'spineSquashDenominator_plsMns', {'operation':1} )
		spineNormStretch_cond.outColorR >> spineSquashDenominator_plsMns.input1D[0]
		spineSquashSpeed_multDiv.outputX >> spineSquashDenominator_plsMns.input1D[1]

		spineSquash_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineSquash_multDiv', {'operation':2} )
		spineSquashNumerator_multDiv.outputX >> spineSquash_multDiv.input1X
		spineSquashDenominator_plsMns.output1D >> spineSquash_multDiv.input2X

		spineStretch_multDiv1 = DPTnode.createNodeWithParam('multiplyDivide', 'spineStretch_multDiv1', {'operation':2} )
		spineStretchNumerator_multDiv.outputX >> spineStretch_multDiv1.input1X
		spineStretchDenominator_plsMns.output1D >> spineStretch_multDiv1.input2X


		spineStretchNegator_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineStretchNegator_multDiv', {'operation':1,'input1X':-1} )
		spineStretch_multDiv1.outputX >> spineStretchNegator_multDiv.input2X

		spineScale_cond = DPTnode.createNodeWithParam('condition', 'spineScale_cond', {'operation':3,'secondTerm':1} )
		spineSquash_multDiv.outputX >> spineScale_cond.colorIfFalseR
		spineStretch_multDiv.outputX >> spineScale_cond.firstTerm
		spineStretchNegator_multDiv.outputX >> spineScale_cond.colorIfTrueR


		spineSquashStretch_1_multDiv1 = DPTnode.createNodeWithParam('multiplyDivide', 'spineSquashStretch_1_multDiv1', {'operation':1} )
		rigSettings_ctrl.spine_4_SsScale >> spineSquashStretch_1_multDiv1.input1X
		rigSettings_ctrl.spine_5_SsScale >> spineSquashStretch_1_multDiv1.input1Y
		rigSettings_ctrl.spine_6_SsScale >> spineSquashStretch_1_multDiv1.input1Z
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv1.input2X
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv1.input2Y
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv1.input2Z

		spineSquashStretch_1_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineSquashStretch_1_multDiv', {'operation':1} )
		rigSettings_ctrl.spine_1_SsScale >> spineSquashStretch_1_multDiv.input1X
		rigSettings_ctrl.spine_2_SsScale >> spineSquashStretch_1_multDiv.input1Y
		rigSettings_ctrl.spine_3_SsScale >> spineSquashStretch_1_multDiv.input1Z
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv.input2X
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv.input2Y
		spineScale_cond.outColorR >> spineSquashStretch_1_multDiv.input2Z


		spineManualSquashStretchNormalize_1_plsMns1 = DPTnode.createNodeWithParam('plusMinusAverage', 'spineManualSquashStretchNormalize_1_plsMns1', {'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dx
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dy
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns1.input3D[0].input3Dz

		spineManualSquashStretchRotAmt_1_multDiv1 = DPTnode.createNodeWithParam('multiplyDivide', 'spineManualSquashStretchRotAmt_1_multDiv1', {'operation':1} )
		rigSettings_ctrl.spine_4_SsScale >> spineManualSquashStretchRotAmt_1_multDiv1.input1X
		rigSettings_ctrl.spine_5_SsScale >> spineManualSquashStretchRotAmt_1_multDiv1.input1Y
		rigSettings_ctrl.spine_6_SsScale >> spineManualSquashStretchRotAmt_1_multDiv1.input1Z
		spineManualSquashStretchNormalize_1_plsMns1.output3Dx >> spineManualSquashStretchRotAmt_1_multDiv1.input2X
		spineManualSquashStretchNormalize_1_plsMns1.output3Dy >> spineManualSquashStretchRotAmt_1_multDiv1.input2Y
		spineManualSquashStretchNormalize_1_plsMns1.output3Dz >> spineManualSquashStretchRotAmt_1_multDiv1.input2Z


		spineManualSquashStretchNormalize_1_plsMns = DPTnode.createNodeWithParam('plusMinusAverage', 'spineManualSquashStretchNormalize_1_plsMns', {'operation':2,'input3D[1].input3Dx':1,'input3D[1].input3Dy':1,'input3D[1].input3Dz':1} )
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dx
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dy
		cog_ctrl.squashStretch >> spineManualSquashStretchNormalize_1_plsMns.input3D[0].input3Dz

		spineManualSquashStretchRotAmt_1_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'spineManualSquashStretchRotAmt_1_multDiv', {'operation':1} )
		rigSettings_ctrl.spine_1_SsScale >> spineManualSquashStretchRotAmt_1_multDiv.input1X
		rigSettings_ctrl.spine_2_SsScale >> spineManualSquashStretchRotAmt_1_multDiv.input1Y
		rigSettings_ctrl.spine_3_SsScale >> spineManualSquashStretchRotAmt_1_multDiv.input1Z
		spineManualSquashStretchNormalize_1_plsMns.output3Dx >> spineManualSquashStretchRotAmt_1_multDiv.input2X
		spineManualSquashStretchNormalize_1_plsMns.output3Dy >> spineManualSquashStretchRotAmt_1_multDiv.input2Y
		spineManualSquashStretchNormalize_1_plsMns.output3Dz >> spineManualSquashStretchRotAmt_1_multDiv.input2Z


		spineSquashStretchAutoEnable1_blend1 = DPTnode.createNodeWithParam('blendColors', 'spineSquashStretchAutoEnable1_blend1', {} )
		cog_ctrl.autoSquashStretch >> spineSquashStretchAutoEnable1_blend1.blender
		spineSquashStretch_1_multDiv1.outputX >> spineSquashStretchAutoEnable1_blend1.color1R
		spineSquashStretch_1_multDiv1.outputY >> spineSquashStretchAutoEnable1_blend1.color1G
		spineSquashStretch_1_multDiv1.outputZ >> spineSquashStretchAutoEnable1_blend1.color1B
		spineManualSquashStretchRotAmt_1_multDiv1.outputX >> spineSquashStretchAutoEnable1_blend1.color2R
		spineManualSquashStretchRotAmt_1_multDiv1.outputY >> spineSquashStretchAutoEnable1_blend1.color2G
		spineManualSquashStretchRotAmt_1_multDiv1.outputZ >> spineSquashStretchAutoEnable1_blend1.color2B


		spineSquashStretchAutoEnable1_blend = DPTnode.createNodeWithParam('blendColors', 'spineSquashStretchAutoEnable1_blend', {} )
		cog_ctrl.autoSquashStretch >> spineSquashStretchAutoEnable1_blend.blender
		spineSquashStretch_1_multDiv.outputX >> spineSquashStretchAutoEnable1_blend.color1R
		spineSquashStretch_1_multDiv.outputY >> spineSquashStretchAutoEnable1_blend.color1G
		spineSquashStretch_1_multDiv.outputZ >> spineSquashStretchAutoEnable1_blend.color1B
		spineManualSquashStretchRotAmt_1_multDiv.outputX >> spineSquashStretchAutoEnable1_blend.color2R
		spineManualSquashStretchRotAmt_1_multDiv.outputY >> spineSquashStretchAutoEnable1_blend.color2G
		spineManualSquashStretchRotAmt_1_multDiv.outputZ >> spineSquashStretchAutoEnable1_blend.color2B


		spineSquashStretchFinalVal_1_plsMns1 = DPTnode.createNodeWithParam('plusMinusAverage', 'spineSquashStretchFinalVal_1_plsMns1', {'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )
		spineSquashStretchAutoEnable1_blend1.outputR >> spineSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dx
		spineSquashStretchAutoEnable1_blend1.outputG >> spineSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dy
		spineSquashStretchAutoEnable1_blend1.outputB >> spineSquashStretchFinalVal_1_plsMns1.input3D[1].input3Dz

		spineSquashStretchFinalVal_1_plsMns = DPTnode.createNodeWithParam('plusMinusAverage', 'spineSquashStretchFinalVal_1_plsMns', {'operation':1,'input3D[0].input3Dx':1,'input3D[0].input3Dy':1,'input3D[0].input3Dz':1} )
		spineSquashStretchAutoEnable1_blend.outputR >> spineSquashStretchFinalVal_1_plsMns.input3D[1].input3Dx
		spineSquashStretchAutoEnable1_blend.outputG >> spineSquashStretchFinalVal_1_plsMns.input3D[1].input3Dy
		spineSquashStretchAutoEnable1_blend.outputB >> spineSquashStretchFinalVal_1_plsMns.input3D[1].input3Dz

		spineSquashStretchFinalVal_1_plsMns.output3Dx >> weightJoints[0].scaleY
		spineSquashStretchFinalVal_1_plsMns.output3Dx >> weightJoints[0].scaleZ
		spineSquashStretchFinalVal_1_plsMns.output3Dy >> weightJoints[1].scaleY
		spineSquashStretchFinalVal_1_plsMns.output3Dy >> weightJoints[1].scaleZ
		spineSquashStretchFinalVal_1_plsMns.output3Dz >> weightJoints[2].scaleY
		spineSquashStretchFinalVal_1_plsMns.output3Dz >> weightJoints[2].scaleZ

		spineSquashStretchFinalVal_1_plsMns1.output3Dx >> weightJoints[3].scaleY
		spineSquashStretchFinalVal_1_plsMns1.output3Dx >> weightJoints[3].scaleZ
		spineSquashStretchFinalVal_1_plsMns1.output3Dy >> weightJoints[4].scaleY
		spineSquashStretchFinalVal_1_plsMns1.output3Dy >> weightJoints[4].scaleZ
		spineSquashStretchFinalVal_1_plsMns1.output3Dz >> weightJoints[5].scaleY
		spineSquashStretchFinalVal_1_plsMns1.output3Dz >> weightJoints[5].scaleZ


		#create splineLow_ctrl_space_grp constraint and connect
		lowSpineCtrl_cogCtrlSpcParCon = pm.parentConstraint( rootCtrl_space_switch_grp, worldSpaceCtrl_space_switch_grp, cogCtrl_space_switch_grp, splineLow_ctrl_space_grp,  maintainOffset=True )
		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=lowSpineCtrl_cogCtrlSpcParCon, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=lowSpineCtrl_cogCtrlSpcParCon, matchStr='world', kwargs={'ud':True})
		cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=lowSpineCtrl_cogCtrlSpcParCon, matchStr='cog', kwargs={'ud':True})

		pm.connectAttr( splineLow_ctrl+'.rootCtrl', lowSpineCtrl_cogCtrlSpcParCon+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( splineLow_ctrl+'.worldSpaceCtrl', lowSpineCtrl_cogCtrlSpcParCon+'.'+worldCtrlSpaceAttr )
		pm.connectAttr( splineLow_ctrl+'.cogCtrl', lowSpineCtrl_cogCtrlSpcParCon+'.'+cogCtrlSpaceAttr )

		masterSpineTrans_0_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineTrans_0_multDiv', {'operation':1} )
		masterSpine_ctrl.translate >> masterSpineTrans_0_multDiv.input1
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineTrans_0_multDiv.input2X
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineTrans_0_multDiv.input2Y
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineTrans_0_multDiv.input2Z

		masterSpineRot_0_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineRot_0_multDiv', {'operation':1} )
		masterSpine_ctrl.rotate >> masterSpineRot_0_multDiv.input1
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineRot_0_multDiv.input2X
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineRot_0_multDiv.input2Y
		rigSettings_ctrl.spineLow_Ctrl_Rotation >> masterSpineRot_0_multDiv.input2Z

		masterSpineTrans_0_multDiv.output >> splineLow_ctrl_frzGrpForCons.translate
		masterSpineRot_0_multDiv.output >> splineLow_ctrl_frzGrpForCons.rotate


		#create spineMid_ctrl_space_grp constraint and connect
		midSpineCtrl_1_lowSpineCtrlSpcParCon = pm.parentConstraint( rootCtrl_space_switch_grp, worldSpaceCtrl_space_switch_grp, cogCtrl_space_switch_grp, lowSpineCtrl_space_switch_grp, spineMid_ctrl_space_grp,  maintainOffset=True )
		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=midSpineCtrl_1_lowSpineCtrlSpcParCon, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=midSpineCtrl_1_lowSpineCtrlSpcParCon, matchStr='world', kwargs={'ud':True})
		cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=midSpineCtrl_1_lowSpineCtrlSpcParCon, matchStr='cog', kwargs={'ud':True})
		spineLowAttr = DPTattr.getMatchNameAttribute(node=midSpineCtrl_1_lowSpineCtrlSpcParCon, matchStr='lowSpine', kwargs={'ud':True})

		pm.connectAttr( splineMid_ctrl+'.rootCtrl', midSpineCtrl_1_lowSpineCtrlSpcParCon+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( splineMid_ctrl+'.worldSpaceCtrl', midSpineCtrl_1_lowSpineCtrlSpcParCon+'.'+worldCtrlSpaceAttr )
		pm.connectAttr( splineMid_ctrl+'.cogCtrl', midSpineCtrl_1_lowSpineCtrlSpcParCon+'.'+cogCtrlSpaceAttr )
		pm.connectAttr( splineMid_ctrl+'.lowSpineCtrl', midSpineCtrl_1_lowSpineCtrlSpcParCon+'.'+spineLowAttr )

		masterSpineTrans_1_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineTrans_1_multDiv', {'operation':1} )
		masterSpine_ctrl.translate >> masterSpineTrans_1_multDiv.input1
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineTrans_1_multDiv.input2X
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineTrans_1_multDiv.input2Y
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineTrans_1_multDiv.input2Z

		masterSpineRot_1_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineRot_1_multDiv', {'operation':1} )
		masterSpine_ctrl.rotate >> masterSpineRot_1_multDiv.input1
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineRot_1_multDiv.input2X
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineRot_1_multDiv.input2Y
		rigSettings_ctrl.spineMid_1_Ctrl_Rotation >> masterSpineRot_1_multDiv.input2Z

		masterSpineTrans_1_multDiv.output >> splineMid_ctrl_frzGrpForCons.translate
		masterSpineRot_1_multDiv.output >> splineMid_ctrl_frzGrpForCons.rotate


		#create splineHigh_ctrl_frzGrp constraint and connect
		hiSpineCtrl_midSpineCtrl_1SpcParCon = pm.parentConstraint( rootCtrl_space_switch_grp, worldSpaceCtrl_space_switch_grp, cogCtrl_space_switch_grp, midSpineCtrl_1_space_switch_grp, splineHigh_ctrl_space_grp,  maintainOffset=True )
		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hiSpineCtrl_midSpineCtrl_1SpcParCon, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hiSpineCtrl_midSpineCtrl_1SpcParCon, matchStr='world', kwargs={'ud':True})
		cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hiSpineCtrl_midSpineCtrl_1SpcParCon, matchStr='cog', kwargs={'ud':True})
		spineMidAttr = DPTattr.getMatchNameAttribute(node=hiSpineCtrl_midSpineCtrl_1SpcParCon, matchStr='midSpine', kwargs={'ud':True})

		pm.connectAttr( splineHigh_ctrl+'.rootCtrl', hiSpineCtrl_midSpineCtrl_1SpcParCon+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( splineHigh_ctrl+'.worldSpaceCtrl', hiSpineCtrl_midSpineCtrl_1SpcParCon+'.'+worldCtrlSpaceAttr )
		pm.connectAttr( splineHigh_ctrl+'.cogCtrl', hiSpineCtrl_midSpineCtrl_1SpcParCon+'.'+cogCtrlSpaceAttr )
		pm.connectAttr( splineHigh_ctrl+'.midSpineCtrl_1', hiSpineCtrl_midSpineCtrl_1SpcParCon+'.'+spineMidAttr )


		masterSpineTrans_2_multDiv = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineTrans_2_multDiv', {'operation':1} )
		masterSpine_ctrl.translate >> masterSpineTrans_2_multDiv.input1
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineTrans_2_multDiv.input2X
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineTrans_2_multDiv.input2Y
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineTrans_2_multDiv.input2Z

		masterSpineRot_2_multDiv  = DPTnode.createNodeWithParam('multiplyDivide', 'masterSpineRot_2_multDiv', {'operation':1} )
		masterSpine_ctrl.rotate >> masterSpineRot_2_multDiv.input1
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineRot_2_multDiv.input2X
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineRot_2_multDiv.input2Y
		rigSettings_ctrl.spineHigh_Ctrl_Rotation >> masterSpineRot_2_multDiv.input2Z

		masterSpineTrans_2_multDiv.output >> splineHigh_ctrl_frzGrpForCons.translate
		masterSpineRot_2_multDiv.output >> splineHigh_ctrl_frzGrpForCons.rotate


		#create hipCtrl_space_grp constraint and connect
		hipCtrl_cogCtrlSpcParCon = pm.parentConstraint( rootCtrl_space_switch_grp, worldSpaceCtrl_space_switch_grp, cogCtrl_space_switch_grp, hipCtrl_space_grp,  maintainOffset=True )

		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hipCtrl_cogCtrlSpcParCon, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hipCtrl_cogCtrlSpcParCon, matchStr='world', kwargs={'ud':True})
		cogCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=hipCtrl_cogCtrlSpcParCon, matchStr='cog', kwargs={'ud':True})

		pm.connectAttr( hip_ctrl+'.rootCtrl', hipCtrl_cogCtrlSpcParCon+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( hip_ctrl+'.worldSpaceCtrl', hipCtrl_cogCtrlSpcParCon+'.'+worldCtrlSpaceAttr )
		pm.connectAttr( hip_ctrl+'.cogCtrl', hipCtrl_cogCtrlSpcParCon+'.'+cogCtrlSpaceAttr )



		#create cogCtrl_space_grp constraint and connect
		cogCtrl_worldSpaceCtrlSpcParCon = pm.parentConstraint( rootCtrl_space_switch_grp, worldSpaceCtrl_space_switch_grp, cogCtrl_space_grp,  maintainOffset=True )

		rootCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=cogCtrl_worldSpaceCtrlSpcParCon, matchStr='root', kwargs={'ud':True})
		worldCtrlSpaceAttr = DPTattr.getMatchNameAttribute(node=cogCtrl_worldSpaceCtrlSpcParCon, matchStr='world', kwargs={'ud':True})

		pm.connectAttr( cog_ctrl+'.rootCtrl', cogCtrl_worldSpaceCtrlSpcParCon+'.'+rootCtrlSpaceAttr )
		pm.connectAttr( cog_ctrl+'.worldSpaceCtrl', cogCtrl_worldSpaceCtrlSpcParCon+'.'+worldCtrlSpaceAttr )


		########################################################Create Head Controller######################################################################

		headargs={}
		headargs['neck'] = 'neck_jnt'
		headargs['head'] = 'head_jnt'

		headRig = DPTHHead.human_head(**headargs)
		headRig.rigTop = RigGrpName
		headRig.root_ctrl = root_ctrl
		headRig.spine_end_jnt = 'spline_end_jnt'
		headRig.setup_rig()


		########################################################Create Leg Controller######################################################################

		LLegIkCtrl_space_switch_grp = DPTgrp.createEmpty('lfLegIkCtrl_space_switch_grp',RigGrpName, pm.xform('ankleL_jnt', q=True, ws=True, t=True ))

		DPTattr.connectAttrToList(root_ctrl+'.masterScale',[LLegIkCtrl_space_switch_grp+'.scaleX',LLegIkCtrl_space_switch_grp+'.scaleY',LLegIkCtrl_space_switch_grp+'.scaleZ',])


		LLegGrpName = 'L'+LegRigGroupName
		if not pm.objExists(LLegGrpName):
			rootTrans= pm.createNode( 'transform', n=LLegGrpName )
			pm.parent(rootTrans,RigGrpName)

		DPThAct.createLegRigSet(grpTop=LLegGrpName, hip='hip_jnt',
													upLeg='up_legL_jnt',
													knee='kneeL_jnt',
													ankle='ankleL_jnt',
													ball='ballL_jnt',
													toe='toeL_jnt',
													heelJnt='heelL',
													sidePrefix='L',
													rootCntrl=root_ctrlName,
													legSpaceCntrl=LLegIkCtrl_space_switch_grp)


		RLegIkCtrl_space_switch_grp = DPTgrp.createEmpty('rtLegIkCtrl_space_switch_grp',RigGrpName, pm.xform('ankleR_jnt', q=True, ws=True, t=True ))
		DPTattr.connectAttrToList(root_ctrl+'.masterScale',[RLegIkCtrl_space_switch_grp+'.scaleX',RLegIkCtrl_space_switch_grp+'.scaleY',RLegIkCtrl_space_switch_grp+'.scaleZ',])

		RLegGrpName = 'R'+LegRigGroupName
		rootTrans= pm.createNode('transform', n=RLegGrpName)
		pm.parent(rootTrans,RigGrpName)

		DPThAct.createLegRigSet(grpTop=RLegGrpName, hip='hip_jnt',
													upLeg='up_legR_jnt',
													knee='kneeR_jnt',
													ankle='ankleR_jnt',
													ball='ballR_jnt',
													toe='toeR_jnt',
													heelJnt='heelR',
													sidePrefix='R',
													rootCntrl=root_ctrlName,
													legSpaceCntrl=RLegIkCtrl_space_switch_grp)



		#Create LArm Rig
		kwargs={}
		kwargs['clavicle'] = 'clavicleL_jnt'
		kwargs['upArm'] = 'up_armL_jnt'
		kwargs['elbow'] = 'elbowL_jnt'
		kwargs['wrist'] = 'wristL_jnt'
		kwargs['hand'] = 'handL_jnt'

		l_arm_rig = DPTHArm.human_arm(**kwargs)
		l_arm_rig.rigTop=RigGrpName
		l_arm_rig.root_ctrl = root_ctrl
		l_arm_rig.rootSpaceCtrl = rootCtrl_space_switch_grp
		l_arm_rig.worldSpaceCtrl = worldSpaceCtrl_space_switch_grp
		l_arm_rig.cog_ctrl = cog_ctrl
		l_arm_rig.cogSpaceCtrl = cogCtrl_space_switch_grp
		l_arm_rig.master_spline = masterSpine_ctrl
		l_arm_rig.head_ctrl = None
		l_arm_rig.headSpaceCtrl = headRig.headSpaceCtrl
		l_arm_rig.LowSplineSpaceCtrl = lowSpineCtrl_space_switch_grp
		l_arm_rig.HiSplineSpaceCtrl = hiSpineCtrl_space_switch_grp
		l_arm_rig.sidePrefix = 'L'
		l_arm_rig.spine_end_jnt = 'spline_end_jnt'
		l_arm_rig.rigSettings_ctrl = rigSettings_ctrl

		l_arm_rig.setup_arm_rig()

		#Create RArm Rig
		Rkwargs={}
		Rkwargs['clavicle'] = 'clavicleR_jnt'
		Rkwargs['upArm'] = 'up_armR_jnt'
		Rkwargs['elbow'] = 'elbowR_jnt'
		Rkwargs['wrist'] = 'wristR_jnt'
		Rkwargs['hand'] = 'handR_jnt'

		rt_arm_rig = DPTHArm.human_arm(**Rkwargs)
		rt_arm_rig.rigTop=RigGrpName
		rt_arm_rig.root_ctrl = root_ctrl
		rt_arm_rig.rootSpaceCtrl = rootCtrl_space_switch_grp
		rt_arm_rig.worldSpaceCtrl = worldSpaceCtrl_space_switch_grp
		rt_arm_rig.cog_ctrl = cog_ctrl
		rt_arm_rig.cogSpaceCtrl = cogCtrl_space_switch_grp
		rt_arm_rig.master_spline = masterSpine_ctrl
		rt_arm_rig.head_ctrl = None
		rt_arm_rig.headSpaceCtrl = headRig.headSpaceCtrl
		rt_arm_rig.LowSplineSpaceCtrl = lowSpineCtrl_space_switch_grp
		rt_arm_rig.HiSplineSpaceCtrl = hiSpineCtrl_space_switch_grp
		rt_arm_rig.sidePrefix = 'R'
		rt_arm_rig.spine_end_jnt = 'spline_end_jnt'
		rt_arm_rig.rigSettings_ctrl = rigSettings_ctrl

		rt_arm_rig.setup_arm_rig()






	def updateHandPose(self):

		if pm.objExists('rigSettings_ctrl'):
			print 'updateHandPose'
		if pm.objExists('Lhand_ctrl'):
			print 'Exist Lhand_ctrl'
			pm.connectControl(self.LhandFistSlide ,'Lhand_ctrl.masterFist')

			#sideL
			#masterFist
			finger_list = ['thumb','index','middle','ring','pinky']

			ui_ctrl_list = {'thumb':{'tx':self.LhandThumbBaseTransXFields, 'ty':self.LhandThumbBaseTransYFields, 'tz':self.LhandThumbBaseTransZFields,
					'rx':self.LhandThumbBaseRotXFields, 'ry':self.LhandThumbBaseRotYFields, 'rz':self.LhandThumbBaseRotZFields,
					'mid':self.LhandThumbMidRotZFields, 'tip':self.LhandThumbTipRotZFields},
					'index':{'tx':self.LhandIndexBaseTransXFields, 'ty':self.LhandIndexBaseTransYFields, 'tz':self.LhandIndexBaseTransZFields,
					'rx':self.LhandIndexBaseRotXFields, 'ry':self.LhandIndexBaseRotYFields, 'rz':self.LhandIndexBaseRotZFields,
					'mid':self.LhandIndexMidRotZFields, 'tip':self.LhandIndexTipRotZFields},
					'middle':{'tx':self.LhandMiddleBaseTransXFields, 'ty':self.LhandMiddleBaseTransYFields, 'tz':self.LhandMiddleBaseTransZFields,
					'rx':self.LhandMiddleBaseRotXFields, 'ry':self.LhandMiddleBaseRotYFields, 'rz':self.LhandMiddleBaseRotZFields,
					'mid':self.LhandMiddleMidRotZFields, 'tip':self.LhandMiddleTipRotZFields},
					'ring':{'tx':self.LhandRingBaseTransXFields, 'ty':self.LhandRingBaseTransYFields, 'tz':self.LhandRingBaseTransZFields,
					'rx':self.LhandRingBaseRotXFields, 'ry':self.LhandRingBaseRotYFields, 'rz':self.LhandRingBaseRotZFields,
					'mid':self.LhandRingMidRotZFields, 'tip':self.LhandRingTipRotZFields},
					'pinky':{'tx':self.LhandPinkyBaseTransXFields, 'ty':self.LhandPinkyBaseTransYFields, 'tz':self.LhandPinkyBaseTransZFields,
					'rx':self.LhandPinkyBaseRotXFields, 'ry':self.LhandPinkyBaseRotYFields, 'rz':self.LhandPinkyBaseRotZFields,
					'mid':self.LhandPinkyMidRotZFields, 'tip':self.LhandPinkyTipRotZFields},
					}


			for fin in finger_list:
				#translate
				pm.connectControl(ui_ctrl_list[fin]['tx'],'L'+fin+'_offset_remap_transx_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['tx'],'L'+fin+'_offset_remap_transx_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['tx'],'L'+fin+'_offset_remap_transx_a.value[2].value_FloatValue', index=4 )

				pm.connectControl(ui_ctrl_list[fin]['ty'],'L'+fin+'_offset_remap_transy_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['ty'],'L'+fin+'_offset_remap_transy_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['ty'],'L'+fin+'_offset_remap_transy_a.value[2].value_FloatValue', index=4 )

				pm.connectControl(ui_ctrl_list[fin]['tz'],'L'+fin+'_offset_remap_transz_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['tz'],'L'+fin+'_offset_remap_transz_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['tz'],'L'+fin+'_offset_remap_transz_a.value[2].value_FloatValue', index=4 )

				#rotate
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'L'+fin+'_offset_remap_rotx_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'L'+fin+'_offset_remap_rotx_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'L'+fin+'_offset_remap_rotx_a.value[2].value_FloatValue', index=4)

				pm.connectControl(ui_ctrl_list[fin]['ry'], 'L'+fin+'_offset_remap_roty_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['ry'], 'L'+fin+'_offset_remap_roty_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['ry'], 'L'+fin+'_offset_remap_roty_a.value[2].value_FloatValue', index=4)

				pm.connectControl(ui_ctrl_list[fin]['rz'], 'L'+fin+'_offset_remap_rotz_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['rz'], 'L'+fin+'_offset_remap_rotz_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['rz'], 'L'+fin+'_offset_remap_rotz_a.value[2].value_FloatValue', index=4)

				#'Mid
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'L'+fin+'_offset_remap_rotz_b.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'L'+fin+'_offset_remap_rotz_b.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'L'+fin+'_offset_remap_rotz_b.value[2].value_FloatValue', index=4)

				#Tip
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'L'+fin+'_offset_remap_rotz_c.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'L'+fin+'_offset_remap_rotz_c.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'L'+fin+'_offset_remap_rotz_c.value[2].value_FloatValue', index=4)

			'''
			#thumb Base
			#translate
			pm.connectControl(self.LhandThumbBaseTransXFields,'Lthumb_offset_remap_transx_a.value[0].value_FloatValue', index=2 )
			pm.connectControl(self.LhandThumbBaseTransXFields,'Lthumb_offset_remap_transx_a.value[1].value_FloatValue', index=3 )
			pm.connectControl(self.LhandThumbBaseTransXFields,'Lthumb_offset_remap_transx_a.value[2].value_FloatValue', index=4 )

			pm.connectControl(self.LhandThumbBaseTransYFields,'Lthumb_offset_remap_transy_a.value[0].value_FloatValue', index=2 )
			pm.connectControl(self.LhandThumbBaseTransYFields,'Lthumb_offset_remap_transy_a.value[1].value_FloatValue', index=3 )
			pm.connectControl(self.LhandThumbBaseTransYFields,'Lthumb_offset_remap_transy_a.value[2].value_FloatValue', index=4 )

			pm.connectControl(self.LhandThumbBaseTransZFields,'Lthumb_offset_remap_transz_a.value[0].value_FloatValue', index=2 )
			pm.connectControl(self.LhandThumbBaseTransZFields,'Lthumb_offset_remap_transz_a.value[1].value_FloatValue', index=3 )
			pm.connectControl(self.LhandThumbBaseTransZFields,'Lthumb_offset_remap_transz_a.value[2].value_FloatValue', index=4 )

			#rotate
			pm.connectControl(self.LhandThumbBaseRotXFields, 'Lthumb_offset_remap_rotx_a.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandThumbBaseRotXFields, 'Lthumb_offset_remap_rotx_a.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandThumbBaseRotXFields, 'Lthumb_offset_remap_rotx_a.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandThumbBaseRotYFields, 'Lthumb_offset_remap_roty_a.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandThumbBaseRotYFields, 'Lthumb_offset_remap_roty_a.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandThumbBaseRotYFields, 'Lthumb_offset_remap_roty_a.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandThumbBaseRotZFields, 'Lthumb_offset_remap_rotz_a.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandThumbBaseRotZFields, 'Lthumb_offset_remap_rotz_a.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandThumbBaseRotZFields, 'Lthumb_offset_remap_rotz_a.value[2].value_FloatValue', index=4)

			#thumb Mid
			pm.connectControl(self.LhandThumbMidRotZFields, 'Lthumb_offset_remap_rotz_b.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandThumbMidRotZFields, 'Lthumb_offset_remap_rotz_b.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandThumbMidRotZFields, 'Lthumb_offset_remap_rotz_b.value[2].value_FloatValue', index=4)

			#thumb Tip
			pm.connectControl(self.LhandThumbTipRotZFields, 'Lthumb_offset_remap_rotz_c.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandThumbTipRotZFields, 'Lthumb_offset_remap_rotz_c.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandThumbTipRotZFields, 'Lthumb_offset_remap_rotz_c.value[2].value_FloatValue', index=4)
			'''



			#spread
			pm.connectControl(self.LhandSpreadSlide ,'Lhand_ctrl.masterSpread')

			pm.connectControl(self.LhandThumbSpreadFields, 'rigSettings_ctrl.leftthumbAmt')
			pm.connectControl(self.LhandIndexSpreadFields, 'rigSettings_ctrl.leftindexAmt')
			pm.connectControl(self.LhandIMiddleSpreadFields, 'rigSettings_ctrl.leftmiddleAmt')
			pm.connectControl(self.LhandRingSpreadFields, 'rigSettings_ctrl.leftringAmt')
			pm.connectControl(self.LhandPinkySpreadFields, 'rigSettings_ctrl.leftpinkyAmt')


			#cup
			pm.connectControl(self.LhandCupSlide ,'Lhand_ctrl.masterCup')

			#index
			pm.connectControl(self.LhandIndexCupRotXFields, 'Lindex_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandIndexCupRotXFields, 'Lindex_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandIndexCupRotXFields, 'Lindex_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandIndexCupRotYFields, 'Lindex_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandIndexCupRotYFields, 'Lindex_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandIndexCupRotYFields, 'Lindex_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandIndexCupRotZFields, 'Lindex_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandIndexCupRotZFields, 'Lindex_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandIndexCupRotZFields, 'Lindex_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#middle
			pm.connectControl(self.LhandMiddleCupRotXFields, 'Lmiddle_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandMiddleCupRotXFields, 'Lmiddle_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandMiddleCupRotXFields, 'Lmiddle_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandMiddleCupRotYFields, 'Lmiddle_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandMiddleCupRotYFields, 'Lmiddle_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandMiddleCupRotYFields, 'Lmiddle_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandMiddleCupRotZFields, 'Lmiddle_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandMiddleCupRotZFields, 'Lmiddle_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandMiddleCupRotZFields, 'Lmiddle_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#ring
			pm.connectControl(self.LhandRingCupRotXFields, 'Lring_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandRingCupRotXFields, 'Lring_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandRingCupRotXFields, 'Lring_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandRingCupRotYFields, 'Lring_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandRingCupRotYFields, 'Lring_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandRingCupRotYFields, 'Lring_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandRingCupRotZFields, 'Lring_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandRingCupRotZFields, 'Lring_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandRingCupRotZFields, 'Lring_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#pinky
			pm.connectControl(self.LhandPinkyCupRotXFields, 'Lpinky_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandPinkyCupRotXFields, 'Lpinky_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandPinkyCupRotXFields, 'Lpinky_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandPinkyCupRotYFields, 'Lpinky_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandPinkyCupRotYFields, 'Lpinky_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandPinkyCupRotYFields, 'Lpinky_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.LhandPinkyCupRotZFields, 'Lpinky_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.LhandPinkyCupRotZFields, 'Lpinky_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.LhandPinkyCupRotZFields, 'Lpinky_offset_cup_rotz.value[2].value_FloatValue', index=4)




		else:
			pm.warning('Not exist Lhand_ctrl')

		if pm.objExists('Rhand_ctrl'):
			print 'Exist Rhand_ctrl'
			pm.connectControl(self.RhandFistSlide ,'Rhand_ctrl.masterFist')

			#sideL
			#masterFist
			finger_list = ['thumb','index','middle','ring','pinky']

			ui_ctrl_list = {'thumb':{'tx':self.RhandThumbBaseTransXFields, 'ty':self.RhandThumbBaseTransYFields, 'tz':self.RhandThumbBaseTransZFields,
					'rx':self.RhandThumbBaseRotXFields, 'ry':self.RhandThumbBaseRotYFields, 'rz':self.RhandThumbBaseRotZFields,
					'mid':self.RhandThumbMidRotZFields, 'tip':self.RhandThumbTipRotZFields},
					'index':{'tx':self.RhandIndexBaseTransXFields, 'ty':self.RhandIndexBaseTransYFields, 'tz':self.RhandIndexBaseTransZFields,
					'rx':self.RhandIndexBaseRotXFields, 'ry':self.RhandIndexBaseRotYFields, 'rz':self.RhandIndexBaseRotZFields,
					'mid':self.RhandIndexMidRotZFields, 'tip':self.RhandIndexTipRotZFields},
					'middle':{'tx':self.RhandMiddleBaseTransXFields, 'ty':self.RhandMiddleBaseTransYFields, 'tz':self.RhandMiddleBaseTransZFields,
					'rx':self.RhandMiddleBaseRotXFields, 'ry':self.RhandMiddleBaseRotYFields, 'rz':self.RhandMiddleBaseRotZFields,
					'mid':self.RhandMiddleMidRotZFields, 'tip':self.RhandMiddleTipRotZFields},
					'ring':{'tx':self.RhandRingBaseTransXFields, 'ty':self.RhandRingBaseTransYFields, 'tz':self.RhandRingBaseTransZFields,
					'rx':self.RhandRingBaseRotXFields, 'ry':self.RhandRingBaseRotYFields, 'rz':self.RhandRingBaseRotZFields,
					'mid':self.RhandRingMidRotZFields, 'tip':self.RhandRingTipRotZFields},
					'pinky':{'tx':self.RhandPinkyBaseTransXFields, 'ty':self.RhandPinkyBaseTransYFields, 'tz':self.RhandPinkyBaseTransZFields,
					'rx':self.RhandPinkyBaseRotXFields, 'ry':self.RhandPinkyBaseRotYFields, 'rz':self.RhandPinkyBaseRotZFields,
					'mid':self.RhandPinkyMidRotZFields, 'tip':self.RhandPinkyTipRotZFields},
					}


			for fin in finger_list:
				#translate
				pm.connectControl(ui_ctrl_list[fin]['tx'],'R'+fin+'_offset_remap_transx_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['tx'],'R'+fin+'_offset_remap_transx_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['tx'],'R'+fin+'_offset_remap_transx_a.value[2].value_FloatValue', index=4 )

				pm.connectControl(ui_ctrl_list[fin]['ty'],'R'+fin+'_offset_remap_transy_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['ty'],'R'+fin+'_offset_remap_transy_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['ty'],'R'+fin+'_offset_remap_transy_a.value[2].value_FloatValue', index=4 )

				pm.connectControl(ui_ctrl_list[fin]['tz'],'R'+fin+'_offset_remap_transz_a.value[0].value_FloatValue', index=2 )
				pm.connectControl(ui_ctrl_list[fin]['tz'],'R'+fin+'_offset_remap_transz_a.value[1].value_FloatValue', index=3 )
				pm.connectControl(ui_ctrl_list[fin]['tz'],'R'+fin+'_offset_remap_transz_a.value[2].value_FloatValue', index=4 )

				#rotate
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'R'+fin+'_offset_remap_rotx_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'R'+fin+'_offset_remap_rotx_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['rx'], 'R'+fin+'_offset_remap_rotx_a.value[2].value_FloatValue', index=4)

				pm.connectControl(ui_ctrl_list[fin]['ry'], 'R'+fin+'_offset_remap_roty_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['ry'], 'R'+fin+'_offset_remap_roty_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['ry'], 'R'+fin+'_offset_remap_roty_a.value[2].value_FloatValue', index=4)

				pm.connectControl(ui_ctrl_list[fin]['rz'], 'R'+fin+'_offset_remap_rotz_a.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['rz'], 'R'+fin+'_offset_remap_rotz_a.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['rz'], 'R'+fin+'_offset_remap_rotz_a.value[2].value_FloatValue', index=4)

				#'Mid
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'R'+fin+'_offset_remap_rotz_b.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'R'+fin+'_offset_remap_rotz_b.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['mid'], 'R'+fin+'_offset_remap_rotz_b.value[2].value_FloatValue', index=4)

				#Tip
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'R'+fin+'_offset_remap_rotz_c.value[0].value_FloatValue', index=2)
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'R'+fin+'_offset_remap_rotz_c.value[1].value_FloatValue', index=3)
				pm.connectControl(ui_ctrl_list[fin]['tip'], 'R'+fin+'_offset_remap_rotz_c.value[2].value_FloatValue', index=4)


			#spread
			pm.connectControl(self.RhandSpreadSlide ,'Rhand_ctrl.masterSpread')

			pm.connectControl(self.RhandThumbSpreadFields, 'rigSettings_ctrl.rightthumbAmt')
			pm.connectControl(self.RhandIndexSpreadFields, 'rigSettings_ctrl.rightindexAmt')
			pm.connectControl(self.RhandIMiddleSpreadFields, 'rigSettings_ctrl.rightmiddleAmt')
			pm.connectControl(self.RhandRingSpreadFields, 'rigSettings_ctrl.rightringAmt')
			pm.connectControl(self.RhandPinkySpreadFields, 'rigSettings_ctrl.rightpinkyAmt')


			#cup
			pm.connectControl(self.RhandCupSlide ,'Rhand_ctrl.masterCup')

			#index
			pm.connectControl(self.RhandIndexCupRotXFields, 'Rindex_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandIndexCupRotXFields, 'Rindex_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandIndexCupRotXFields, 'Rindex_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandIndexCupRotYFields, 'Rindex_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandIndexCupRotYFields, 'Rindex_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandIndexCupRotYFields, 'Rindex_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandIndexCupRotZFields, 'Rindex_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandIndexCupRotZFields, 'Rindex_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandIndexCupRotZFields, 'Rindex_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#middle
			pm.connectControl(self.RhandMiddleCupRotXFields, 'Rmiddle_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandMiddleCupRotXFields, 'Rmiddle_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandMiddleCupRotXFields, 'Rmiddle_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandMiddleCupRotYFields, 'Rmiddle_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandMiddleCupRotYFields, 'Rmiddle_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandMiddleCupRotYFields, 'Rmiddle_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandMiddleCupRotZFields, 'Rmiddle_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandMiddleCupRotZFields, 'Rmiddle_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandMiddleCupRotZFields, 'Rmiddle_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#ring
			pm.connectControl(self.RhandRingCupRotXFields, 'Rring_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandRingCupRotXFields, 'Rring_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandRingCupRotXFields, 'Rring_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandRingCupRotYFields, 'Rring_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandRingCupRotYFields, 'Rring_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandRingCupRotYFields, 'Rring_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandRingCupRotZFields, 'Rring_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandRingCupRotZFields, 'Rring_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandRingCupRotZFields, 'Rring_offset_cup_rotz.value[2].value_FloatValue', index=4)

			#pinky
			pm.connectControl(self.RhandPinkyCupRotXFields, 'Rpinky_offset_cup_rotx.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandPinkyCupRotXFields, 'Rpinky_offset_cup_rotx.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandPinkyCupRotXFields, 'Rpinky_offset_cup_rotx.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandPinkyCupRotYFields, 'Rpinky_offset_cup_roty.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandPinkyCupRotYFields, 'Rpinky_offset_cup_roty.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandPinkyCupRotYFields, 'Rpinky_offset_cup_roty.value[2].value_FloatValue', index=4)

			pm.connectControl(self.RhandPinkyCupRotZFields, 'Rpinky_offset_cup_rotz.value[0].value_FloatValue', index=2)
			pm.connectControl(self.RhandPinkyCupRotZFields, 'Rpinky_offset_cup_rotz.value[1].value_FloatValue', index=3)
			pm.connectControl(self.RhandPinkyCupRotZFields, 'Rpinky_offset_cup_rotz.value[2].value_FloatValue', index=4)




		else:
			pm.warning('Not exist Rhand_ctrl')


	def selectCharNode(self, nodeName=None):

		if nodeName is not None:
			pm.select( nodeName, r=True)


	def resetHandPose(self):
		if pm.objExists('Lhand_ctrl'):
			pm.setAttr('Lhand_ctrl.masterFist',0)
			pm.setAttr('Lhand_ctrl.masterSpread',0)
			pm.setAttr('Lhand_ctrl.masterCup',0)
		if pm.objExists('Rhand_ctrl'):
			pm.setAttr('Rhand_ctrl.masterFist',0)
			pm.setAttr('Rhand_ctrl.masterSpread',0)
			pm.setAttr('Rhand_ctrl.masterCup',0)



