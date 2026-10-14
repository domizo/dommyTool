# -*- coding: utf-8 -*-
import pymel.core as pm

import DPyTools.general.transformActions as DPTtrans
import DPyTools.general.groupActions as DPTgrp
import DPyTools.rigging.DEyeRigOp as DEyeRT
import DPyTools.node.DNodeOp as DNodeOp
import DPyTools.general.groupActions as DGrp
import DPyTools.curve.curveTool as DNurbs
import DPyTools.rigging.DRigControllerOp as DRgCnOp
import DPyTools.general.attributeActions as DPAttr
import DPyTools.system.DWorkspace as DWkSp
import DPyTools.system.DFileOp as DFOp

import DPyTools.general.selectOp as selOp
import DPyTools.general.follicleActions as dfolicle

import DPyTools.mesh.uvOp as uvOp
from DPyTools.core import json_store
from DPyTools.features.rigging.face import models as face_models
from DPyTools.features.rigging.face import lip_edges
from DPyTools.features.rigging.face import specs as face_specs
from DPyTools.ui import windows as ui_windows
from DPyTools.features.animation.operations import createDrivenKeys


DControllerInfo = face_models.DControllerInfo
DFaceExpr = face_models.DFaceExpr
DValue = face_models.DValue





'''
to run:
	>>> import DPyTools.rigging.DFaceRigOp as DFaceRT
	>>> DFaceRT.openWin()

'''


#curveType 0:square 1:slider 2:split 3:dirbox 4:custom
def createController(name='', parent=None,
					pos=(0.0, 0.0),
					rotate=0.0,
					conType=0,
					keepAttr=None,
					addAttr=None,
					mov = 3.0):
	keepAttr = [] if keepAttr is None else keepAttr
	addAttr = [] if addAttr is None else addAttr

	#create controller class
	con = None

	if conType==0 :
		con = DRgCnOp.squareBoxController(name=name)
		#con = squareBoxController(name=name)
		pm.scale( con.ctlr_root, mov*.5, mov*.5, 1 )
		#con = simpleCircleController(name=name)
	elif conType==1:
		con = DRgCnOp.circleSlider(length=1.0, name=name)
		pm.scale( con.ctlr_root, mov, mov, 1 )
	elif conType==2:
		con = DRgCnOp.splitSlider(length=2.0, name=name)
		pm.scale( con.ctlr_root, mov*.5, mov*.5, 1 )
	elif conType==3:
		con = DRgCnOp.quadDirectBox(name=name)
		pm.scale( con.ctlr_root, mov*.5, mov*.5, 1 )
	else :
		con = DRgCnOp.jawController(name=name)

	#pm.scale( con.ctlr_root, 2, 2, 1 )
	pm.rotate( con.ctlr_root, [0,0,rotate] )
	pm.move( pos[0], pos[1], 0, con.ctlr_root )

	#pm.rename( con, name)

	#hide attribute
	con.setKeepAttr(keepAttr)

	#add attribute
	con.setNewAttr(addAttr)

	#parent root
	if parent is not None:
		pm.parent( con.ctlr_root, parent)

	return con


# Historical misspelling retained for archived scripts.
createContoller = createController




def createBSFromList(bslist=None, srcShape='', name=''):
	blend_shape_args = list(bslist or [])
	blend_shape_args.append(srcShape)
	return pm.blendShape(*blend_shape_args, name=name)[0]





def createCurveFromMeshEdgeList(edgelist=None, srcMesh=''):
	if edgelist is None:
		edgelist = []

	if len(edgelist)==0:
		return None


	#check edgeHistGrp

	dup_edge_grp = None
	dup_edge_grp_name = 'edgeHistGrp'
	if not pm.objExists(dup_edge_grp_name):
		dup_edge_grp = DGrp.createEmpty(name=dup_edge_grp_name, parent=None )
		pm.setAttr((dup_edge_grp.visibility), 0)
	else :
		dup_edge_grp = DNodeOp.getItem(dup_edge_grp_name)


	#create curve from edgelist
	preCurveShape = None
	outCurve = None
	preAttachNode = None

	for eindex in edgelist:

		crvFromEdge = pm.createNode( 'curveFromMeshEdge')

		#set attribute
		pm.setAttr( (crvFromEdge+'.edgeIndex[0]'), eindex )

		#connect source mesh
		pm.connectAttr( (srcMesh+'.worldMesh[0]'), (crvFromEdge+'.inputMesh'), force=True )

		#create out curve
		newCrvShape = pm.createNode('nurbsCurve')
		newCrv = newCrvShape.getParent()
		pm.rename(newCrv, 'dupEdge')
		pm.parent(newCrv, dup_edge_grp)

		pm.connectAttr( (crvFromEdge+'.outputCurve'), (newCrvShape+'.create'), force=True )

		'''
		if preCurveShape is not None:
			if preAttachNode is None:
				attachCurve = pm.createNode( 'attachCurve')
				pm.connectAttr( (preCurveShape+'.worldSpace[0]'), (attachCurve+'.inputCurve1'), force=True )
				pm.connectAttr( (newCrvShape+'.worldSpace[0]'), (attachCurve+'.inputCurve2'), force=True )

				preAttachNode = attachCurve
			else :
				attachCurve = pm.createNode( 'attachCurve')
				pm.connectAttr( (preAttachNode+'.outputCurve'), (attachCurve+'.inputCurve1'), force=True )
				pm.connectAttr( (newCrvShape+'.worldSpace[0]'), (attachCurve+'.inputCurve2'), force=True )

				preAttachNode = attachCurve
		else :
			preCurveShape = newCrvShape

		if eindex==edgelist[-1]:
			print ':ast element'
			newlastShape = pm.createNode('nurbsCurve')
			lastCrv = newlastShape.getParent()
			pm.rename(lastCrv, 'lastCrv')
			pm.connectAttr( (preAttachNode+'.outputCurve'), (newlastShape+'.create'), force=True )
			outCurve = newlastShape
		'''

		if outCurve is not None :
			#conect pre curve
			pm.attachCurve( outCurve, newCrvShape, kmk=False, method=1, rpo=1, ch=1 )
		elif preCurveShape is not None:
			outCurve = pm.attachCurve( preCurveShape, newCrvShape, kmk=False, method=1, rpo=0, ch=1 )
		else :
			preCurveShape = newCrvShape


	#rebuild curve to cubic
	if outCurve:

		pm.rebuildCurve(outCurve, rebuildType=0, degree=3 , endKnots=1, keepRange=0, keepTangents=0, ch=1)
		#return outCurve


		pm.parent(outCurve[0], dup_edge_grp)
		pm.rename(outCurve[0], 'mergedCurve')

		resampleNode = pm.createNode( 'DResampleCurve')
		pm.setAttr( (resampleNode+'.Sampling'), 20 )

		outShape = outCurve[0].getShape()
		pm.connectAttr( (outShape+'.worldSpace[0]'), (resampleNode+'.inCurve'), force=True )

		#create out curve
		newCrvShape = pm.createNode('nurbsCurve')
		newCrv = newCrvShape.getParent()
		pm.rename(newCrv, 'resampleCrv')
		pm.parent(newCrv, dup_edge_grp)

		pm.connectAttr( (resampleNode+'.outCurve'), (newCrvShape+'.create'), force=True )


		return (newCrv, newCrvShape)

	return None


def createClusterFromCurveCV(crv=None, centerCrv=None, stickyAttrL='', stickyAttrR='', parent=None):

	#calc curve cv num
	#degs = pm.getAttr( (crv+'.degree') )
	#spans = pm.getAttr( (crv+'.spans') )
	cvCount = DNurbs.getCvCount(crv)
	lCount = cvCount/2
	rCount = lCount
	amari = cvCount-(lCount+rCount)
	amariIndex = 0
	if amari != 0:
		amariIndex = lCount

	lStep = 1.0/(lCount+amari)
	rStep = 1.0/(rCount+amari)

	print ('Sticky info::CV Count:{cvs} left:{lcount} right:{rcount} center:{amari} '.format(cvs=cvCount, lcount=lCount, rcount=rCount, amari=amari))

	#create curveInfo
	curveInfoNodeName = 'centerLineCurveInfo'
	crvInfoNode = None
	if not pm.objExists(curveInfoNodeName):
		crvInfoNode = pm.createNode( 'curveInfo', n=curveInfoNodeName )
		pm.connectAttr( (centerCrv+'.worldSpace[0]'), (crvInfoNode+'.inputCurve'), force=True )
	else :
		crvInfoNode = DNodeOp.getItem(curveInfoNodeName)



	#create cluster loop
	cluster_hnd_list=[]

	for idx in range(cvCount):
		shape = crv.getShape()
		na = (shape+'.cv[{index}]'.format(index=idx) )
		pm.select(na, r=True)

		#create cluster
		clusterNode, clusterTrans  = pm.cluster( na )

		#connect curveInfo
		pm.connectAttr( (crvInfoNode+'.controlPoints[{index}]'.format(index=idx)), (clusterTrans+'.scalePivot'), force=True )

		cluster_hnd_list.append(clusterTrans)


	#left
	offsetIdx = 0
	for idx in range(cvCount-1,lCount-1, -1):
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[idx]+'.scaleX'),
						times=[(lStep*float(offsetIdx)), (lStep*float(offsetIdx+1))], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[idx]+'.scaleY'),
						times=[(lStep*float(offsetIdx)), (lStep*float(offsetIdx+1))], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[idx]+'.scaleZ'),
						times=[(lStep*float(offsetIdx)), (lStep*float(offsetIdx+1))], values=[1.0,0.0])

		offsetIdx = offsetIdx+1


	if amariIndex>0:
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[amariIndex]+'.scaleX'),
						times=[ (1.0-lStep), 1.0], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[amariIndex]+'.scaleY'),
						times=[ (1.0-lStep), 1.0], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrL, target=(cluster_hnd_list[amariIndex]+'.scaleZ'),
						times=[ (1.0-lStep), 1.0], values=[1.0,0.0])

	#right
	for idx in range(rCount):
		createDrivenKeys(driver=stickyAttrR, target=(cluster_hnd_list[idx]+'.scaleX'),
						times=[(lStep*float(idx)), (lStep*float(idx+1))], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrR, target=(cluster_hnd_list[idx]+'.scaleY'),
						times=[(lStep*float(idx)), (lStep*float(idx+1))], values=[1.0,0.0])
		createDrivenKeys(driver=stickyAttrR, target=(cluster_hnd_list[idx]+'.scaleZ'),
						times=[(lStep*float(idx)), (lStep*float(idx+1))], values=[1.0,0.0])

	#parent cluster
	if parent is not None:
		for clstr in cluster_hnd_list:
			pm.parent(clstr, parent)


def createLipWireDeform(baseCrv=None, deformCrv=None, baseShape=None, wireName=''):

	#create wire
	wire = pm.wire( baseShape, name=wireName )[0]

	pm.connectAttr( (baseCrv+'.worldSpace[0]'), (wire+'.baseWire[0]'), force=True )
	pm.connectAttr( (deformCrv+'.worldSpace[0]'), (wire+'.deformedWire[0]'), force=True )


	return wire




def createFaceOptionJointController(rivetTarget=''):

	link_dic = {'face_opt_jnt_grp':['face_dummy_jnt_none','face_sup_ctl_grp', 'face_base_jnt_grp'] ,
				'neck_opt_jnt_grp':['neck_dummy_jnt_none','neck_sup_ctl_grp', 'neck_base_jnt_grp']}

	#create top group node
	flcl_ctl_face_grp = DGrp.createEmpty(name='flcl_ctl_grp', parent=None )




	pm.setAttr((flcl_ctl_face_grp.visibility), 0)

	#mesh object
	mesh_obj = pm.general.PyNode('face')

	#create Rivet Mesh
	#
	rivet_mesh = pm.duplicate(rivetTarget, n=('rivet_mesh_geo'), parentOnly=0)[0]
	print ('rivet mesh : {0}'.format(rivet_mesh))
	rivet_mesh = pm.general.PyNode('rivet_mesh_geo')
	# target_pymesh = pm.general.PyNode(rivetTarget)
	# rivet_mesh.visibility = 0
	pm.setAttr("rivet_mesh_geo.visibility", 0)
	pm.parent(rivet_mesh, 'face_bs_grp')
	pm.connectAttr(rivetTarget+'.worldMesh[0]', rivet_mesh.getShape().name()+'.inMesh', f=1)

	#controller shader
	shader=pm.shadingNode("surfaceShader", asShader=True, name='face_opt_jnt_ctl_mtl')
	pm.setAttr ( (shader + '.outColor'), 0, 0.0, 1.0, type='double3' )


	#create opt joint

	is_joint_connect_error = None
	for target_grp in link_dic.keys() :

		const_jnt = link_dic[target_grp][0]
		face_sup_ctl_grp = link_dic[target_grp][1]
		face_base_jnt_grp = link_dic[target_grp][2]

		opt_jnt_grp = pm.general.PyNode(target_grp)
		pm.setAttr((opt_jnt_grp.visibility), 0)

		print ('face option controller group :{0}'.format(face_sup_ctl_grp))
		print ('face option const joint :{0}'.format(const_jnt))

		joint_list = selOp.getNodeFromHierarchy(obj=opt_jnt_grp, typeStr='joint')

		face_sup_ctl_grp = DGrp.createEmpty(name=face_sup_ctl_grp, parent=None )
		face_base_jnt_grp = DGrp.createEmpty(name=face_base_jnt_grp, parent=None )
		pm.setAttr((face_base_jnt_grp.visibility), 0)

		#check none joint
		tmp = []
		none_count = 0
		for jnt in joint_list:
			if '_none' in jnt.name() :
				none_count = none_count+1
				continue

			tmp.append(jnt)

		joint_list = tmp

		print ('none joint count:{0}'.format(none_count))

		# print ('joint list:{0}'.format(joint_list))
		# print ('joint list size:{0}'.format(len(joint_list)))

		########### Create follicle ###########
		const_jnt = pm.general.PyNode(const_jnt)
		DPTtrans.setTransformToObjectZero( srcObj=face_sup_ctl_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)
		DPTtrans.setTransformToObjectZero( srcObj=face_base_jnt_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)
		# DPTtrans.setTransformToObjectZero( srcObj=flcl_ctl_face_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)


		# pm.parentConstraint( const_jnt, face_sup_ctl_grp)
		pm.pointConstraint( const_jnt, face_sup_ctl_grp)
		pm.orientConstraint( const_jnt, face_sup_ctl_grp)


		for jnt in joint_list :
			wpos = pm.xform(jnt, q=1, ws=1, t=1)
			# pos = pm.xform(jnt, q=1, objectSpace=1, t=1)
			# clpt = meshOp.getClosestPointFromPoint(m_mesh=mesh_obj, pt_list=[pos])

			# if len(clpt)==0:
			# 	continue

			# print ('clpt : {0}'.format(clpt[0]))

			uv_result = uvOp.getUVfromPoint(meshObj=mesh_obj, pt_list=[wpos])


			if len(uv_result)==0:
				continue

			# print ('uv : {0}'.format(uv_result[0]))

			##### Create follicle Hierarchy #####
			fol = dfolicle.create_follicle(targetObj=rivet_mesh, uv=[uv_result[0][0],uv_result[0][1]])
			fol = fol.getParent()
			fol = pm.rename(fol, (jnt.name()+'_fol'))#rename

			pm.parent(fol, flcl_ctl_face_grp)
			# DGrp.createEmpty(name=(jnt.name()+'_grp'), parent=fol, position=pos )
			fol_grp = DGrp.createEmptyWithAlign(name=(jnt.name()+'_grp'), alignObj=fol, isTrans=True, isRot=0, isScale=True,isUnParent=0)

			fol_loc = pm.spaceLocator(n=(jnt.name()+'_loc'))
			DPTtrans.setTransformToObjectZero( srcObj=fol_loc, targetObj=fol_grp, isTrans=True, isRot=1, isScale=0, isUnParent=0)

			##### Create Base Joint Hierarchy #####

			jnt_cst_off = DGrp.createEmpty(name=(jnt.name()+'_cst_off'), parent=None, position=wpos )
			jnt_cst = DGrp.createEmpty(name=(jnt.name()+'_cst'), parent=jnt_cst_off, position=wpos )
			# jnt_base = pm.duplicate(jnt, n=(jnt.name()+'_base'), parentOnly=True)[0]
			jnt_base = pm.joint(n=(jnt.name()+'_base'), p=wpos )

			pm.parent(jnt_base, jnt_cst)
			pm.parent(jnt_cst_off, face_base_jnt_grp)

			##### Create Controller Hierarchy #####

			jnt_ctl_off = DGrp.createEmpty(name=(jnt.name()+'_ctl_off'), parent=None, position=wpos )
			jnt_ctl_trans = DGrp.createEmpty(name=(jnt.name()+'_ctl_trans'), parent=jnt_ctl_off, position=wpos )

			# sp_trans = pm.polySphere( n=(jnt.name()+'_ctl_trans'), r=0.15, createUVs=0, sa=8, sh=4, ch=0)[0]
			sp_trans = pm.sphere( n=(jnt.name()+'_ctl_trans'), r=0.15, ch=0)[0]
			DPTtrans.setTransformToObjectZero( srcObj=sp_trans, targetObj=jnt, isTrans=1, isRot=0, isScale=0, isUnParent=1)
			pm.parent(sp_trans, jnt_ctl_trans)

			##### assign shader #####
			pm.select(sp_trans)
			pm.hyperShade(assign=shader)

			pm.parent(jnt_ctl_off, face_sup_ctl_grp)

			########## connect relationship ##########

			#connect skinCluster pre matrix
			jnt_mtx_attr = jnt.name()+'.worldMatrix[0]'
			if pm.connectionInfo( jnt_mtx_attr, isSource=True):
				srcConnects = pm.connectionInfo(jnt_mtx_attr, destinationFromSource=True)
				if len(srcConnects) :
					sp_str = srcConnects[0].split('.')
					if len(sp_str) >1:
						clstr_node = sp_str[0]
						src_attr = sp_str[1]
						inv_mat_attr = sp_str[1].replace('matrix', 'bindPreMatrix')
						# print ('skin cluster :{0} joint attribute : {1}'.format(clstr_node,src_attr))

						#connect
						pm.connectAttr((str(jnt_base)+'.worldInverseMatrix[0]'), (clstr_node+'.'+inv_mat_attr), force=True )

					else :
						is_joint_connect_error = True
				else :
					is_joint_connect_error = True
			else :
				is_joint_connect_error = True



			#constraint joint to controller locator
			pm.pointConstraint( fol_loc, jnt_cst)
			pm.orientConstraint( fol_loc, jnt_cst)

			pm.pointConstraint( sp_trans, jnt)
			pm.orientConstraint( sp_trans, jnt)

			jnt_cst.translate >> jnt_ctl_trans.translate
			jnt_cst.rotate >> jnt_ctl_trans.rotate
			# break;


	if is_joint_connect_error:
		pm.warning('Error occured joint matrix connection!')

	'''
	face_opt_jnt_grp = 'face_opt_jnt_grp'
	neck_opt_jnt_grp = 'neck_opt_jnt_grp'
	face_opt_jnt_grp = pm.general.PyNode(face_opt_jnt_grp)
	neck_opt_jnt_grp = pm.general.PyNode(neck_opt_jnt_grp)
	pm.setAttr((face_opt_jnt_grp.visibility), 0)
	pm.setAttr((neck_opt_jnt_grp.visibility), 0)


	face_opt_jnt_list = selOp.getNodeFromHierarchy(obj=face_opt_jnt_grp, typeStr='joint')
	neck_opt_jnt_list = selOp.getNodeFromHierarchy(obj=neck_opt_jnt_grp, typeStr='joint')
	# print ('face joint list size:{0}'.format(len(face_opt_jnt_list)))
	# print ('neck joint list size:{0}'.format(len(neck_opt_jnt_list)))


	joint_list = face_opt_jnt_list + neck_opt_jnt_list

	#check none joint
	tmp = []
	none_count = 0
	for jnt in joint_list:
		if '_none' in jnt.name() :
			none_count = none_count+1
			continue

		tmp.append(jnt)

	joint_list = tmp

	print ('none joint count:{0}'.format(none_count))

	# print ('joint list:{0}'.format(joint_list))
	# print ('joint list size:{0}'.format(len(joint_list)))

	########### Create follicle ###########

	#create top group node
	flcl_ctl_face_grp = DGrp.createEmpty(name='flcl_ctl_face_grp', parent=None )
	face_base_jnt_grp = DGrp.createEmpty(name='face_base_jnt_grp', parent=None )
	face_sup_ctl_grp = DGrp.createEmpty(name='face_sup_ctl_grp', parent=None )

	pm.setAttr((face_base_jnt_grp.visibility), 0)
	pm.setAttr((flcl_ctl_face_grp.visibility), 0)


	#mesh object
	mesh_obj = pm.general.PyNode('face')
	const_jnt = pm.general.PyNode('face_dummy_jnt_none')
	DPTtrans.setTransformToObjectZero( srcObj=face_sup_ctl_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)
	DPTtrans.setTransformToObjectZero( srcObj=face_base_jnt_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)
	# DPTtrans.setTransformToObjectZero( srcObj=flcl_ctl_face_grp, targetObj=const_jnt, isTrans=True, isRot=1, isScale=0, isUnParent=1)

	# pm.parentConstraint( const_jnt, face_sup_ctl_grp)
	pm.pointConstraint( const_jnt, face_sup_ctl_grp)
	pm.orientConstraint( const_jnt, face_sup_ctl_grp)


	#create Rivet Mesh
	#
	rivet_mesh = pm.duplicate(rivetTarget, n=('rivet_mesh_geo'), parentOnly=0)[0]
	print ('rivet mesh : {0}'.format(rivet_mesh))
	rivet_mesh = pm.general.PyNode('rivet_mesh_geo')
	# target_pymesh = pm.general.PyNode(rivetTarget)
	# rivet_mesh.visibility = 0
	pm.setAttr("rivet_mesh_geo.visibility", 0)
	pm.parent(rivet_mesh, 'face_bs_grp')
	pm.connectAttr(rivetTarget+'.worldMesh[0]', rivet_mesh.getShape().name()+'.inMesh', f=1)

	#controller shader
	shader=pm.shadingNode("surfaceShader", asShader=True, name='face_opt_jnt_ctl_mtl')
	pm.setAttr ( (shader + '.outColor'), 0, 0.0, 1.0, type='double3' )



	is_joint_connect_error = None
	for jnt in joint_list :
		wpos = pm.xform(jnt, q=1, ws=1, t=1)
		# pos = pm.xform(jnt, q=1, objectSpace=1, t=1)
		# clpt = meshOp.getClosestPointFromPoint(m_mesh=mesh_obj, pt_list=[pos])

		# if len(clpt)==0:
		# 	continue

		# print ('clpt : {0}'.format(clpt[0]))

		uv_result = uvOp.getUVfromPoint(meshObj=mesh_obj, pt_list=[wpos])


		if len(uv_result)==0:
			continue

		# print ('uv : {0}'.format(uv_result[0]))

		##### Create follicle Hierarchy #####
		fol = dfolicle.create_follicle(targetObj=rivet_mesh, uv=[uv_result[0][0],uv_result[0][1]])
		fol = fol.getParent()
		fol = pm.rename(fol, (jnt.name()+'_fol'))#rename

		pm.parent(fol, flcl_ctl_face_grp)
		# DGrp.createEmpty(name=(jnt.name()+'_grp'), parent=fol, position=pos )
		fol_grp = DGrp.createEmptyWithAlign(name=(jnt.name()+'_grp'), alignObj=fol, isTrans=True, isRot=0, isScale=True,isUnParent=0)

		fol_loc = pm.spaceLocator(n=(jnt.name()+'_loc'))
		DPTtrans.setTransformToObjectZero( srcObj=fol_loc, targetObj=fol_grp, isTrans=True, isRot=1, isScale=0, isUnParent=0)

		##### Create Base Joint Hierarchy #####

		jnt_cst_off = DGrp.createEmpty(name=(jnt.name()+'_cst_off'), parent=None, position=wpos )
		jnt_cst = DGrp.createEmpty(name=(jnt.name()+'_cst'), parent=jnt_cst_off, position=wpos )
		# jnt_base = pm.duplicate(jnt, n=(jnt.name()+'_base'), parentOnly=True)[0]
		jnt_base = pm.joint(n=(jnt.name()+'_base'), p=wpos )

		pm.parent(jnt_base, jnt_cst)
		pm.parent(jnt_cst_off, face_base_jnt_grp)

		##### Create Controller Hierarchy #####

		jnt_ctl_off = DGrp.createEmpty(name=(jnt.name()+'_ctl_off'), parent=None, position=wpos )
		jnt_ctl_trans = DGrp.createEmpty(name=(jnt.name()+'_ctl_trans'), parent=jnt_ctl_off, position=wpos )

		# sp_trans = pm.polySphere( n=(jnt.name()+'_ctl_trans'), r=0.15, createUVs=0, sa=8, sh=4, ch=0)[0]
		sp_trans = pm.sphere( n=(jnt.name()+'_ctl_trans'), r=0.15, ch=0)[0]
		DPTtrans.setTransformToObjectZero( srcObj=sp_trans, targetObj=jnt, isTrans=1, isRot=0, isScale=0, isUnParent=1)
		pm.parent(sp_trans, jnt_ctl_trans)

		##### assign shader #####
		pm.select(sp_trans)
		pm.hyperShade(assign=shader)

		pm.parent(jnt_ctl_off, face_sup_ctl_grp)

		########## connect relationship ##########

		#connect skinCluster pre matrix
		jnt_mtx_attr = jnt.name()+'.worldMatrix[0]'
		if pm.connectionInfo( jnt_mtx_attr, isSource=True):
			srcConnects = pm.connectionInfo(jnt_mtx_attr, destinationFromSource=True)
			if len(srcConnects) :
				sp_str = srcConnects[0].split('.')
				if len(sp_str) >1:
					clstr_node = sp_str[0]
					src_attr = sp_str[1]
					inv_mat_attr = sp_str[1].replace('matrix', 'bindPreMatrix')
					# print ('skin cluster :{0} joint attribute : {1}'.format(clstr_node,src_attr))

					#connect
					pm.connectAttr((str(jnt_base)+'.worldInverseMatrix[0]'), (clstr_node+'.'+inv_mat_attr), force=True )

				else :
					is_joint_connect_error = True
			else :
				is_joint_connect_error = True
		else :
			is_joint_connect_error = True



		#constraint joint to controller locator
		pm.pointConstraint( fol_loc, jnt_cst)
		pm.orientConstraint( fol_loc, jnt_cst)

		pm.pointConstraint( sp_trans, jnt)
		pm.orientConstraint( sp_trans, jnt)

		jnt_cst.translate >> jnt_ctl_trans.translate
		jnt_cst.rotate >> jnt_ctl_trans.rotate
		# break;


	if is_joint_connect_error:
		pm.warning('Error occured joint matrix connection!')
	'''




#create create setup window
class DFaceRiggingTool(object):

	###################### common param ######################
	#arrow controller move size
	m_mov = 3.0


	###################### eye controller class ######################
	m_eye_rig = None


	###################### top group names ######################
	#face group
	m_face_bs_root_name = 'face_bs_grp'
	m_face_bs_root = None

	#base blend shape
	m_base_shape_name = 'bs_base'
	m_base_shape = None

	#bs_parts_prefix
	BSP_PREFIX = '_bs'


	###################### shape group list ######################
	m_eye_look_bs_grp = face_specs.EYE_LOOK_GROUP
	m_eye_bs_grp = face_specs.EYE_GROUP
	m_brow_bs_grp = face_specs.BROW_GROUP
	m_nose_bs_grp = face_specs.NOSE_GROUP
	m_cheek_bs_grp = face_specs.CHEEK_GROUP
	m_mouth_bs_grp = face_specs.MOUTH_GROUP
	m_lip_bs_grp = face_specs.LIP_GROUP
	m_jaw_bs_grp = face_specs.JAW_GROUP
	m_neck_bs_grp = face_specs.NECK_GROUP
	m_ear_bs_grp = face_specs.EAR_GROUP

	#all
	m_bs_grp_all = face_specs.FACE_BLEND_SHAPE_GROUPS


	###################### blend shape list ######################

	m_blow_bs_list = face_specs.BROW_BLEND_SHAPES
	m_eye_bs_list = face_specs.EYE_BLEND_SHAPES
	m_nose_bs_list = face_specs.NOSE_BLEND_SHAPES
	m_cheek_bs_list = face_specs.CHEEK_BLEND_SHAPES
	m_mouth_bs_list = face_specs.MOUTH_BLEND_SHAPES
	m_lip_bs_list = face_specs.LIP_BLEND_SHAPES
	m_jaw_bs_list = face_specs.JAW_BLEND_SHAPES
	m_neck_bs_list = face_specs.NECK_BLEND_SHAPES
	m_ear_bs_list = face_specs.EAR_BLEND_SHAPES
	m_bs_list_all = face_specs.FACE_BLEND_SHAPE_LISTS
	m_bs_list_dict = None


	###################### controller list ######################

	#root controller
	m_cntlr_root_name = 'face_cntrl_grp'
	m_cntlr_root = None

	######################brow controller
	m_con_browRaise_l = None
	m_con_browRaise_r = None
	m_con_browRaise_m = None

	m_con_browRaise_c_l = None
	m_con_browRaise_c_r = None
	m_con_browFrown_l = None
	m_con_browFrown_r = None
	m_con_browLower_l = None
	m_con_browLower_r = None

	m_con_brow_list = None

	######################eye controller
	m_con_eye_l = None
	m_con_eye_r = None
	m_con_eye_c = None

	m_con_eye_opcl_l = None
	m_con_eye_opcl_r = None
	m_con_eye_up_l = None
	m_con_eye_up_r = None
	m_con_eye_dn_l = None
	m_con_eye_dn_r = None

	m_con_eye_list = None

	######################nose controller
	m_con_nose_l = None
	m_con_nose_r = None

	m_con_nose_hole_l = None
	m_con_nose_hole_r = None

	m_con_nose_list = None

	######################cheek controller
	m_con_cheek_l = None
	m_con_cheek_r = None

	m_con_cheek_list = None

	######################cheek puff controller

	m_con_cheek_puff_l = None
	m_con_cheek_puff_r = None

	m_con_cheek_puff_list = None

	######################mouth controller
	m_con_mouth_c = None

	m_con_mouth_list = None

	######################lip controller
	m_con_lip_up_c = None
	m_con_lip_dn_c = None

	m_con_lip_corner_l = None
	m_con_lip_corner_r = None

	m_con_upperLipRaise_l = None
	m_con_upperLipRaise_r = None

	m_con_lipCornerDepress_l = None
	m_con_lipCornerDepress_r = None

	m_con_lowerLipDepress_l = None
	m_con_lowerLipDepress_r = None

	m_con_chinRaise_c = None

	m_con_lip_list = None

	######################mouth side controller
	m_con_lip_io_up = None
	m_con_lip_io_dn = None

	m_con_lip_protude_up = None
	m_con_lip_protude_dn = None




	######################jaw controller
	m_con_jaw_c = None

	m_con_jaw_list = None

	######################neck controller
	m_con_neck_l = None
	m_con_neck_r = None

	m_con_neck_pycon_l = None
	m_con_neck_pycon_r = None

	m_con_neck_contracted_l = None
	m_con_neck_contracted_r = None

	m_con_neck_sternomastoid_l = None
	m_con_neck_sternomastoid_r = None

	m_con_neck_list = None

	######################ear controller
	m_con_ear_l = None
	m_con_ear_r = None

	m_con_ear_list = None

	######################quad controller
	m_con_lip_funnel = None
	m_con_lip_up_bite = None
	m_con_lips_up_spread = None




	######################tongue controller
	m_con_tongue_box = None
	m_con_tongue_bar = None

	m_con_tongue_list = None

	######################sticky controller

	#m_con_sticky_list = []

	######################all controlelr list
	m_all_con_lists = None  # all
	m_mouth_con_lists = None  # mouth all
	###################### sticky effect param ######################

	#Sticky Data
	m_top_sticky_edge_list = None
	m_bot_sticky_edge_list = None

	###################### Face Expression List Data ######################

	m_face_expression_list = None
	m_mouth_expression_list = None

	###################### motion data ######################

	eye_mbias = face_specs.EYE_MOTION_BIASES

	###################### init ######################

	def __init__(self):
		# Runtime state must not be shared by multiple tool windows.
		self.m_bs_list_dict = dict(face_specs.FACE_BLEND_SHAPES_BY_GROUP)
		self.m_con_brow_list = []
		self.m_con_eye_list = []
		self.m_con_nose_list = []
		self.m_con_cheek_list = []
		self.m_con_cheek_puff_list = []
		self.m_con_mouth_list = []
		self.m_con_lip_list = []
		self.m_con_jaw_list = []
		self.m_con_neck_list = []
		self.m_con_ear_list = []
		self.m_con_tongue_list = []
		self.m_all_con_lists = []
		self.m_mouth_con_lists = []
		self.m_top_sticky_edge_list = []
		self.m_bot_sticky_edge_list = []
		self.m_face_expression_list = []
		self.m_mouth_expression_list = []

		#init items state
		self.setInitItem()

		#check face blendshape group root
		if not pm.objExists(self.m_face_bs_root_name):
			pm.warning('Not found face bs group root!(face_bs_grp)')
			return



	def openWin(self):
		uiname = "DFaceRigToolWin"

		ui_windows.delete_window(uiname)

		############################################################
		####################create window component####################
		self.window = pm.window(uiname,widthHeight=(480, 400))

		############################################################
		####################create main form####################
		self.mainForm = pm.formLayout(numberOfDivisions=100)

		#root group
		self.baseBSText = pm.textField(placeholderText='Enter Base DS Shape')
		self.refleshBtn = pm.button(label='Reflesh',c = lambda *args: self.refleshAllCB())



		####################create tab####################
		#self.tabform = pm.formLayout(numberOfDivisions=100)
		self.tab = pm.tabLayout(innerMarginWidth=5, innerMarginHeight=5)

		#set tab form
		#pm.formLayout( self.tabform, edit=True, attachForm=((self.tab, 'top', 0), (self.tab, 'left', 0), (self.tab, 'bottom', 0), (self.tab, 'right', 0)) )
		pm.formLayout( self.mainForm, edit=True,
			attachForm=[(self.baseBSText, 'top', 5), (self.baseBSText, 'left', 5),
						(self.tab, 'left', 5), (self.tab, 'right', 5),(self.tab, 'bottom', 5),
						(self.refleshBtn, 'top', 5), (self.refleshBtn, 'right', 5)] ,
			attachControl=[(self.refleshBtn,'left' , 5, self.baseBSText),(self.refleshBtn,'bottom' , 5, self.tab),
							(self.tab,'top' , 5, self.baseBSText)],
			attachPosition=[(self.baseBSText, 'right', 5, 80)]
			)
		############################################################
		####################create create tab form####################
		self.form = pm.formLayout(numberOfDivisions=100)
		self.column = pm.columnLayout( columnAttach=('both', 5), rowSpacing=10, adjustableColumn=True )

		bgColorBase = [0.5,0.0,0.0]

		#################### create eye rig ####################
		pm.frameLayout( label="Eye Rig", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		self.eyeParentJntText = pm.textFieldGrp(label='Eye Parent', placeholderText='Enter Eye Parent Object or Joint', text='head_jnt')
		pm.button(label='Eye Joint Setup', bgc=bgColorBase, c = lambda *args: self.CreateEyeJointCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		sepHeight = 20



		#################### Setup Blend Shape ####################
		pm.frameLayout( label="Blend Shape Setup", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		self.checkDummy = pm.checkBox( label='Create Dummy Mesh', value = False )
		pm.button(label='Check BS List', bgc=bgColorBase, c = lambda *args: self.checkBSListCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################


		#################### Create Controller ####################
		pm.frameLayout( label="Setup Fae Controller", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		pm.button(label='Generate Controllers', bgc=bgColorBase, c = lambda *args: self.CreateCntlrCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################


		#################### Setup Sticky Effect ####################
		pm.frameLayout( label="Sticky Effect Option", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		#lipLine
		pm.rowLayout( numberOfColumns=2, columnWidth2=(200, 50), adjustableColumn=2, columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0)] )
		self.topStickyText = pm.textField(placeholderText='')
		pm.button(label='SET',c = lambda *args: self.setTopStickyCB() )
		pm.setParent( '..' )

		pm.rowLayout( numberOfColumns=2, columnWidth2=(200, 50), adjustableColumn=2, columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0)] )
		self.botStickyText = pm.textField(placeholderText='')
		pm.button(label='SET',c = lambda *args: self.setBotStickyCB() )
		pm.setParent( '..' )

		pm.rowLayout( numberOfColumns=2, columnWidth2=(125, 125), adjustableColumn=1, columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0)] )
		pm.button(label='LOAD',c = lambda *args: self.loadLipEdgeCB() )
		pm.button(label='SAVE',c = lambda *args: self.saveLipEdgeCB() )
		pm.setParent( '..' )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################

		#################### Setup Rivet ####################
		pm.frameLayout( label="Skin Transfer Option", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		self.skinnedJntSets = pm.textFieldGrp(label='Joint Sets', placeholderText='Enter sets for joints', text='JointsForBind_sets')
		self.skinnedMeshText = pm.textFieldGrp(label='Skinned Mesh', placeholderText='Enter skinned mesh', text='skined_face_geo')

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################

		#################### Setup Rivet ####################
		pm.frameLayout( label="Rivet Option", collapsable=True, collapse=False, borderVisible=1)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		self.rivetMeshText = pm.textFieldGrp(label='Rivet mesh target', placeholderText='Enter rivet mesh target', text='stickyHeadShape')


		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################



		#################### Run Facail Rig ####################
		#create face rig
		self.checkSticky = pm.checkBox( label='Create Sticky Effect', value = False )
		pm.button(label='Create Face Rig', bgc=bgColorBase, c = lambda *args: self.CreateFaceRigCB(), height=44 )


		#edit sticky lip component ui
		#pm.separator(height=sepHeight)


		#self.topWireDistSlider = pm.floatSliderGrp( label='Top Lip Wire', field=True, minValue=0.0, maxValue=1.0, fieldMinValue=0.0, fieldMaxValue=1.0, value=0 )
		#self.botWireDistSlider = pm.floatSliderGrp( label='Bottom Lip Wire', field=True, minValue=0.0, maxValue=1.0, fieldMinValue=0.0, fieldMaxValue=1.0, value=0 )


		#edit layout
		pm.formLayout( self.form, edit=True,
			attachForm=[(self.column, 'top', 5), (self.column, 'left', 5), (self.column, 'right', 5),(self.column, 'bottom', 5)]
			)

		pm.setParent( '..' )#column
		pm.setParent( '..' )#form

		############################################################
		####################create edit tab####################
		self.editColumn = pm.columnLayout( columnAttach=('both', 5), rowSpacing=10, adjustableColumn=True )

		####################Create frame for Reset####################
		pm.frameLayout( label="Reset Controllers", collapsable=True, collapse=False, borderVisible=False)
		pm.columnLayout( rowSpacing=10, adjustableColumn=True )

		pm.rowColumnLayout( numberOfColumns=3, columnWidth=[(1, 100), (2, 100), (3, 100)] )
		pm.button(label='brow',c = lambda *args: self.resetPartControllerCB(self.m_con_brow_list) )
		pm.button(label='eye',c = lambda *args: self.resetPartControllerCB(self.m_con_eye_list) )
		pm.button(label='nose',c = lambda *args: self.resetPartControllerCB(self.m_con_nose_list) )
		pm.button(label='cheek',c = lambda *args: self.resetPartControllerCB(self.m_con_cheek_list) )
		pm.button(label='puff',c = lambda *args: self.resetPartControllerCB(self.m_con_cheek_puff_list) )
		pm.button(label='lip',c = lambda *args: self.resetPartControllerCB(self.m_con_lip_list) )
		pm.button(label='mouth',c = lambda *args: self.resetPartControllerCB(self.m_con_mouth_list) )
		pm.button(label='jaw',c = lambda *args: self.resetPartControllerCB(self.m_con_jaw_list) )
		pm.button(label='neck',c = lambda *args: self.resetPartControllerCB(self.m_con_neck_list) )
		pm.button(label='ear',c = lambda *args: self.resetPartControllerCB(self.m_con_ear_list) )
		pm.button(label='tongue',c = lambda *args: self.resetPartControllerCB(self.m_con_tongue_list) )
		#pm.button(label='sticky',c = lambda *args: self.resetPartControllerCB(self.m_con_sticky_list) )
		pm.setParent( '..' )#rowColumnLayout

		pm.button(label='All Reset', bgc=bgColorBase,c = lambda *args: self.resetAllControllerParamsCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		####################create frame for Reset####################
		pm.frameLayout( label="Modify Blend Bias", collapsable=True, collapse=False, borderVisible=False)
		pm.columnLayout( rowSpacing=10, adjustableColumn=True )

		pm.button(label='Modify Open Mouth Weight', bgc=bgColorBase, c = lambda *args: self.modifyOpenMouthWeightCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		####################Random Face####################

		pm.frameLayout( label="Random Expression", collapsable=True, collapse=False, borderVisible=False)
		pm.intSliderGrp(label="Rand Expr", field=True, minValue=0, fieldMinValue=0, fieldMaxValue=1000, value=0, cc= lambda *args: self.randFaceExprCB())

		pm.setParent( '..' )#frameLayout Random Expression

		############################################################
		####################create frame for Sticky####################
		pm.frameLayout(label="Sticky Effect", collapsable=True, collapse=False, borderVisible=False)
		pm.columnLayout(adjustableColumn=True)

		pm.rowLayout( numberOfColumns=2, columnWidth2=(125, 125), adjustableColumn=1, columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0)] )
		pm.button(label='Check Sticky',c = lambda *args: self.checkStickyEffectCB() )
		pm.button(label='RESET', c = lambda *args: self.resetFaceControllerCB() )
		pm.setParent( '..' )#rowLayout

		self.topWireDistSlider = pm.attrFieldSliderGrp( label='Top Lip Wire', minValue=0.0, maxValue=1.0, fieldMinValue=0.0, fieldMaxValue=1.0)
		self.botWireDistSlider = pm.attrFieldSliderGrp( label='Bottom Lip Wire', minValue=0.0, maxValue=1.0, fieldMinValue=0.0, fieldMaxValue=1.0)


		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout

		############################################################






		#################### Create Retarget for face parts ####################

		u''' 顔のパーツ(目や目の向きのロケーター)をコンストレイント等を使用して別のオブジェクトをペアレントに設定する '''
		pm.frameLayout(label="Retarget Face Parts", collapsable=True, collapse=False, borderVisible=False)
		pm.columnLayout(adjustableColumn=True)

		self.checkEyeReTar = pm.checkBox( label='Retarget Eyes', value=1 )
		self.checkMouthReTar = pm.checkBox( label='Retarget Mouth', value=1 )
		self.checkLacrimaReTar = pm.checkBox( label='Retarget Lacrimal', value=1 )



		pm.rowLayout( numberOfColumns=2, columnWidth2=(200, 80), adjustableColumn=1)
		self.reTargetText = pm.textFieldGrp(label='Retarget Object', placeholderText='Enter retarget object', text='head_jnt')
		pm.button(label='SET',c = lambda *args: self.setSelectObjectInTF(self.reTargetText) )
		pm.setParent( '..' )

		pm.button(label='Retarget', bgc=bgColorBase, c = lambda *args: self.reTargetFacialPartsCB() )

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout


		############################################################


		pm.setParent( '..' )#editColumn








		############################################################
		####################create anim tab####################
		self.animColumn = pm.columnLayout(adjustableColumn=True)

		####################create frame for Expression list
		pm.frameLayout( label="Expression List", collapsable=True, collapse=False, borderVisible=False)
		pm.columnLayout( rowSpacing=20, adjustableColumn=True )

		pm.frameLayout( label="Face", collapsable=True, collapse=False, borderVisible=False, bgc=[0.1,0.6,0.6])
		self.faceExprRowColumnLayout = pm.rowColumnLayout( numberOfColumns=3, columnWidth=[(1, 100), (2, 100), (3, 100)] )

		pm.setParent( '..' )#rowColumnLayout Face


		pm.frameLayout( label="Face Expr Manage", collapsable=True, collapse=False, borderVisible=False)
		pm.rowLayout( numberOfColumns=4, columnWidth4=(125, 125, 125, 125), columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0), (3, 'both', 0), (4, 'both', 0)] )
		pm.button(label='Load',c = lambda *args: self.loadExprData(parts_type=0) )
		pm.button(label='Save',c = lambda *args: self.saveExprData(parts_type=0) )
		pm.button(label='Add Expr',c = lambda *args: self.addExprData(parts_type=0, con_list_array=self.m_all_con_lists) )
		pm.button(label='DEL', bgc=[1.0,0.0,0.0],c = lambda *args: self.delExprData(parts_type=0) )
		pm.setParent( '..' )#rowLayout
		pm.setParent( '..' )#frameLayout Face Expr Manage

		pm.setParent( '..' )#frameLayout Face


		pm.frameLayout( label="Mouth", collapsable=True, collapse=False, borderVisible=False, bgc=[0.1,0.6,0.6])
		self.mouthExprRowColumnLayout = pm.rowColumnLayout( numberOfColumns=3, columnWidth=[(1, 100), (2, 100), (3, 100)] )

		pm.setParent( '..' )#rowColumnLayout Mouth

		pm.frameLayout( label="Mouth Expr Manage", collapsable=True, collapse=False, borderVisible=False)
		pm.rowLayout( numberOfColumns=4, columnWidth4=(125, 125, 125, 125), columnAlign=(1, 'left'), columnAttach=[(1, 'both', 0), (2, 'both', 0), (3, 'both', 0), (4, 'both', 0)] )
		pm.button(label='Load',c = lambda *args: self.loadExprData(parts_type=1) )
		pm.button(label='Save',c = lambda *args: self.saveExprData(parts_type=1) )
		pm.button(label='Add Expr',c = lambda *args: self.addExprData(parts_type=1, con_list_array=self.m_mouth_con_lists) )
		pm.button(label='DEL', bgc=[1.0,0.0,0.0],c = lambda *args: self.delExprData(parts_type=1) )
		pm.setParent( '..' )#rowLayout
		pm.setParent( '..' )#frameLayout Mouth Expr Manage

		pm.setParent( '..' )#frameLayout Mouth

		pm.button(label='RESET', bgc=[0.1,0.5,0.1],c = lambda *args: self.resetAllControllerParamsCB() )




		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout





		####################create frame for Eye Control
		pm.frameLayout(label="Eye Control", collapsable=True, collapse=False, borderVisible=False , bgc=[0.3,0.2,0.0])
		pm.columnLayout(rowSpacing=5, adjustableColumn=True)
		pm.rowColumnLayout( numberOfColumns=4, columnWidth=[(1, 100), (2, 100), (3, 100), (4, 100)] )

		pm.button(label='Open', c = lambda *args: self.openEyeCB(bias=self.eye_mbias[3]) )
		pm.button(label='Open Max', c = lambda *args: self.openEyeCB(bias=self.eye_mbias[4]))
		pm.button(label='Close', c = lambda *args: self.openEyeCB(bias=self.eye_mbias[1]))
		pm.button(label='Close Max', c = lambda *args: self.openEyeCB(bias=self.eye_mbias[0]))

		pm.setParent( '..' )#rowColumnLayout

		pm.setParent( '..' )#columnLayout
		pm.setParent( '..' )#frameLayout


		pm.setParent( '..' )#animColumn


		############################################################
		#set tab layout
		pm.tabLayout( self.tab, edit=True, tabLabel=((self.form, 'Create Rig'), (self.editColumn, 'Edit Rig'),
										(self.animColumn, 'Animation')) )



		############################################################
		#set update default face and mouth expression check

		if DWkSp.isExistDataDir('data'):
			datafolder = DWkSp.getProjDataPath('data')

			#face
			default_face_expr_file = datafolder+'/def_face_lib.json'
			if DFOp.isExist(default_face_expr_file):
				print ('Default Face Expr file is exist.')

				self.readExprFileJson(default_face_expr_file, parts_type=0)

			#mouth
			default_mouth_expr_file = datafolder+'/def_mouth_lib.json'
			if DFOp.isExist(default_mouth_expr_file):
				print ('Default Mouth Expr file is exist.')

				self.readExprFileJson(default_mouth_expr_file, parts_type=1)


		############################################################
		#open window
		pm.showWindow( self.window )



	def setInitItem(self):
		self.m_EyeRig = DEyeRT.EyeRiggingTool()

		#init shape info
		self.updateShapeInfo()


	def refleshAllCB(self):

		#reflesh eye rig state
		if self.m_EyeRig :
			self.m_EyeRig.updateEyeRigCB()

		#update controller
		self.updateControllerItems()

		print ('Updated contorller state!')


	# Manual diagnostics retained for Script Editor use, not exposed in the UI.
	def eyeDebugInfo(self):
		self.m_EyeRig.echoState()


	def echoState(self):

		print ('\nbrow con state------------\n')
		print ('m_con_browRaise_l : {name}'.format(name=self.m_con_browRaise_l))
		print ('m_con_browRaise_r : {name}'.format(name=self.m_con_browRaise_r))
		print ('m_con_browRaise_m : {name}'.format(name=self.m_con_browRaise_m))

		print ('m_con_browRaise_c_l : {name}'.format(name=self.m_con_browRaise_c_l))
		print ('m_con_browRaise_c_r : {name}'.format(name=self.m_con_browRaise_c_r))
		print ('m_con_browFrown_l : {name}'.format(name=self.m_con_browFrown_l))
		print ('m_con_browFrown_r : {name}'.format(name=self.m_con_browFrown_r))
		print ('m_con_browLower_l : {name}'.format(name=self.m_con_browLower_l))
		print ('m_con_browLower_r : {name}'.format(name=self.m_con_browLower_r))

		print ('\neye con state------------\n')

		print ('m_con_eye_l : {name}'.format(name=self.m_con_eye_l))
		print ('m_con_eye_r : {name}'.format(name=self.m_con_eye_r))
		print ('m_con_eye_c : {name}'.format(name=self.m_con_eye_c))

		print ('m_con_eye_opcl_l : {name}'.format(name=self.m_con_eye_opcl_l))
		print ('m_con_eye_opcl_r : {name}'.format(name=self.m_con_eye_opcl_r))
		print ('m_con_eye_up_l : {name}'.format(name=self.m_con_eye_up_l))
		print ('m_con_eye_up_r : {name}'.format(name=self.m_con_eye_up_r))
		print ('m_con_eye_dn_l : {name}'.format(name=self.m_con_eye_dn_l))
		print ('m_con_eye_dn_r : {name}'.format(name=self.m_con_eye_dn_r))

		print ('\nnnose con state------------\n')

		print ('m_con_nose_l : {name}'.format(name=self.m_con_nose_l))
		print ('m_con_nose_r : {name}'.format(name=self.m_con_nose_r))

		print ('m_con_nose_hole_l : {name}'.format(name=self.m_con_nose_hole_l))
		print ('m_con_nose_hole_r : {name}'.format(name=self.m_con_nose_hole_r))

		print ('\ncheek con state------------\n')

		print ('m_con_cheek_l : {name}'.format(name=self.m_con_cheek_l))
		print ('m_con_cheek_r : {name}'.format(name=self.m_con_cheek_r))

		print ('\ncheek puff con state------------\n')

		print ('m_con_cheek_puff_l : {name}'.format(name=self.m_con_cheek_puff_l))
		print ('m_con_cheek_puff_r : {name}'.format(name=self.m_con_cheek_puff_r))

		print ('\nmouth con state------------\n')

		print ('m_con_mouth_c : {name}'.format(name=self.m_con_mouth_c))

		print ('\nlip con state------------\n')

		print ('m_con_lip_up_c : {name}'.format(name=self.m_con_lip_up_c))
		print ('m_con_lip_dn_c : {name}'.format(name=self.m_con_lip_dn_c))

		print ('m_con_lip_corner_l : {name}'.format(name=self.m_con_lip_corner_l))
		print ('m_con_lip_corner_r : {name}'.format(name=self.m_con_lip_corner_r))

		print ('m_con_upperLipRaise_l : {name}'.format(name=self.m_con_upperLipRaise_l))
		print ('m_con_upperLipRaise_r : {name}'.format(name=self.m_con_upperLipRaise_r))

		print ('m_con_lipCornerDepress_l : {name}'.format(name=self.m_con_lipCornerDepress_l))
		print ('m_con_lipCornerDepress_r : {name}'.format(name=self.m_con_lipCornerDepress_r))

		print ('m_con_lowerLipDepress_l : {name}'.format(name=self.m_con_lowerLipDepress_l))
		print ('m_con_lowerLipDepress_r : {name}'.format(name=self.m_con_lowerLipDepress_r))

		print ('m_con_chinRaise_c : {name}'.format(name=self.m_con_chinRaise_c))

		print ('\nmouth side con state------------\n')

		print ('m_con_lip_io_up : {name}'.format(name=self.m_con_lip_io_up))
		print ('m_con_lip_io_dn : {name}'.format(name=self.m_con_lip_io_dn))

		print ('m_con_lip_protude_up : {name}'.format(name=self.m_con_lip_protude_up))
		print ('m_con_lip_protude_dn : {name}'.format(name=self.m_con_lip_protude_dn))

		print ('\njaw con state------------\n')

		print ('m_con_jaw_c : {name}'.format(name=self.m_con_jaw_c))

		print ('\nneck con state------------\n')

		print ('m_con_neck_l : {name}'.format(name=self.m_con_neck_l))
		print ('m_con_neck_r : {name}'.format(name=self.m_con_neck_r))
		print ('m_con_neck_pycon_l : {name}'.format(name=self.m_con_neck_pycon_l))
		print ('m_con_neck_pycon_r : {name}'.format(name=self.m_con_neck_pycon_r))
		print ('m_con_neck_contracted_l : {name}'.format(name=self.m_con_neck_contracted_l))
		print ('m_con_neck_contracted_r : {name}'.format(name=self.m_con_neck_contracted_r))
		print ('m_con_neck_sternomastoid_l : {name}'.format(name=self.m_con_neck_sternomastoid_l))
		print ('m_con_neck_sternomastoid_r : {name}'.format(name=self.m_con_neck_sternomastoid_r))

		print ('\near con state------------\n')

		print ('m_con_ear_l : {name}'.format(name=self.m_con_ear_l))
		print ('m_con_ear_r : {name}'.format(name=self.m_con_ear_r))

		print ('\nquad con state------------\n')

		print ('m_con_lip_funnel : {name}'.format(name=self.m_con_lip_funnel))
		print ('m_con_lip_up_bite : {name}'.format(name=self.m_con_lip_up_bite))
		print ('m_con_lips_up_spread : {name}'.format(name=self.m_con_lips_up_spread))

		print ('\ntongue con state------------\n')

		print ('m_con_tongue_box : {name}'.format(name=self.m_con_tongue_box))
		print ('m_con_tongue_bar : {name}'.format(name=self.m_con_tongue_bar))


	def updateControllerLists(self):

		#brow
		self.m_con_brow_list = [self.m_con_browRaise_l, self.m_con_browRaise_r, self.m_con_browRaise_m,
						self.m_con_browRaise_c_l, self.m_con_browRaise_c_r,
						self.m_con_browFrown_l, self.m_con_browFrown_r,
						self.m_con_browLower_l, self.m_con_browLower_r]

		#eye
		self.m_con_eye_list = [self.m_con_eye_l, self.m_con_eye_r,self.m_con_eye_c,
							self.m_con_eye_opcl_l, self.m_con_eye_opcl_r,
							self.m_con_eye_up_l, self.m_con_eye_up_r,
							self.m_con_eye_dn_l, self.m_con_eye_dn_r]

		#nose
		self.m_con_nose_list = [self.m_con_nose_l, self.m_con_nose_r,
								self.m_con_nose_hole_l, self.m_con_nose_hole_r]

		#cheek
		self.m_con_cheek_list = [self.m_con_cheek_l, self.m_con_cheek_r]

		#cheek puff
		self.m_con_cheek_puff_list = [self.m_con_cheek_puff_l, self.m_con_cheek_puff_r]

		#mouth
		self.m_con_mouth_list = [self.m_con_mouth_c]


		#lip
		self.m_con_lip_list = [self.m_con_lip_up_c, self.m_con_lip_dn_c,
					self.m_con_lip_corner_l, self.m_con_lip_corner_r,
					self.m_con_upperLipRaise_l, self.m_con_upperLipRaise_r,
					self.m_con_lipCornerDepress_l, self.m_con_lipCornerDepress_r,
					self.m_con_lowerLipDepress_l, self.m_con_lowerLipDepress_r,
					self.m_con_chinRaise_c]
		#mouth side
		self.m_con_lip_list.extend([self.m_con_lip_io_up, self.m_con_lip_io_dn,
								self.m_con_lip_protude_up, self.m_con_lip_protude_dn])
		#quad
		self.m_con_lip_list.extend([self.m_con_lip_funnel, self.m_con_lip_up_bite, self.m_con_lips_up_spread])

		#jaw
		self.m_con_jaw_list = [self.m_con_jaw_c]

		#neck
		self.m_con_neck_list = [self.m_con_neck_l, self.m_con_neck_r,
						self.m_con_neck_pycon_l, self.m_con_neck_pycon_r,
						self.m_con_neck_contracted_l, self.m_con_neck_contracted_r,
						self.m_con_neck_sternomastoid_l, self.m_con_neck_sternomastoid_r]

		#ear
		self.m_con_ear_list = [self.m_con_ear_l, self.m_con_ear_r]

		#tongue
		self.m_con_tongue_list = [self.m_con_tongue_box,self.m_con_tongue_bar]

		#all
		self.m_all_con_lists = [self.m_con_brow_list, self.m_con_eye_list,
						self.m_con_nose_list, self.m_con_cheek_list,
						self.m_con_cheek_puff_list, self.m_con_mouth_list,
						self.m_con_lip_list,
						self.m_con_jaw_list, self.m_con_neck_list,
						self.m_con_ear_list,
						self.m_con_tongue_list]

		self.m_mouth_con_lists = [self.m_con_mouth_list,
								self.m_con_lip_list,
								self.m_con_jaw_list,
								self.m_con_tongue_list]



	def getControllerType(self, con=None):
		parents = pm.listRelatives( con, allParents=True )
		c_type = pm.getAttr(parents[0]+'.cntlrType')
		return c_type


	def selectController(self, name=''):
		"""select controller item with class generate.

		"""

		node = DNodeOp.getItem(name)

		if node is not None:

			c_type = self.getControllerType(node)

			#print ('type : {name}'.format(name=c_type))

			return self.reGenerateController(type=c_type, node=node)

		return None


	def reGenerateController(self, type='', node=None):
		"""set controller class to variable

		"""

		con = None

		if type=='slider':
			con = DRgCnOp.circleSlider(cntlr_node=node)
		elif type=='box':
			con = DRgCnOp.squareBoxController(cntlr_node=node)
		elif type=='split':
			con = DRgCnOp.splitSlider(cntlr_node=node)
		elif type=='quad':
			con = DRgCnOp.quadDirectBox(cntlr_node=node)

		return con


	def updateControllerItems(self):

		#reflesh face rig state
		self.m_con_browRaise_l = self.selectController('con_browRaise_l')
		self.m_con_browRaise_r = self.selectController('con_browRaise_r')
		self.m_con_browRaise_m = self.selectController('con_browRaise_m')

		self.m_con_browRaise_c_l = self.selectController('con_browRaise_c_l')
		self.m_con_browRaise_c_r = self.selectController('con_browRaise_c_r')
		self.m_con_browFrown_l = self.selectController('con_browFrown_l')
		self.m_con_browFrown_r = self.selectController('con_browFrown_r')
		self.m_con_browLower_l = self.selectController('con_browLower_l')
		self.m_con_browLower_r = self.selectController('con_browLower_r')
		#eye
		self.m_con_eye_l = self.selectController('con_eye_l')
		self.m_con_eye_r = self.selectController('con_eye_r')
		self.m_con_eye_c = self.selectController('con_eye_c')

		self.m_con_eye_opcl_l = self.selectController('con_eye_opcl_l')
		self.m_con_eye_opcl_r = self.selectController('con_eye_opcl_r')
		self.m_con_eye_up_l = self.selectController('con_eye_up_l')
		self.m_con_eye_up_r = self.selectController('con_eye_up_r')
		self.m_con_eye_dn_l = self.selectController('con_eye_dn_l')
		self.m_con_eye_dn_r = self.selectController('con_eye_dn_r')


		#nose
		self.m_con_nose_l = self.selectController('con_nose_l')
		self.m_con_nose_r = self.selectController('con_nose_r')
		self.m_con_nose_hole_l = self.selectController('con_nose_hole_l')
		self.m_con_nose_hole_r = self.selectController('con_nose_hole_r')

		#cheek
		self.m_con_cheek_l = self.selectController('con_cheek_l')
		self.m_con_cheek_r = self.selectController('con_cheek_r')

		#cheek puff
		self.m_con_cheek_puff_l = self.selectController('con_cheek_puff_l')
		self.m_con_cheek_puff_r = self.selectController('con_cheek_puff_r')

		#mouth
		self.m_con_mouth_c = self.selectController('con_mouth_c')


		#lip
		self.m_con_lip_up_c = self.selectController('con_lip_up_c')
		self.m_con_lip_dn_c = self.selectController('con_lip_dn_c')

		self.m_con_lip_corner_l = self.selectController('con_lip_corner_l')
		self.m_con_lip_corner_r = self.selectController('con_lip_corner_r')

		self.m_con_upperLipRaise_l = self.selectController('con_upperLipRaise_l')
		self.m_con_upperLipRaise_r = self.selectController('con_upperLipRaise_r')

		self.m_con_lipCornerDepress_l = self.selectController('con_lipCornerDepress_l')
		self.m_con_lipCornerDepress_r = self.selectController('con_lipCornerDepress_r')

		self.m_con_lowerLipDepress_l = self.selectController('con_lowerLipDepress_l')
		self.m_con_lowerLipDepress_r = self.selectController('con_lowerLipDepress_r')

		self.m_con_chinRaise_c = self.selectController('con_lowerLipDepress_r')

		#mouth side lips
		self.m_con_lip_io_up = self.selectController('con_lip_io_up')
		self.m_con_lip_io_dn = self.selectController('con_lip_io_dn')
		self.m_con_lip_protude_up = self.selectController('con_lip_protude_up')
		self.m_con_lip_protude_dn = self.selectController('con_lip_protude_dn')

		#jaw
		self.m_con_jaw_c = self.selectController('con_jaw_c')

		#neck
		self.m_con_neck_l = self.selectController('con_neck_l')
		self.m_con_neck_r = self.selectController('con_neck_r')

		self.m_con_neck_pycon_l = self.selectController('con_neck_pycon_l')
		self.m_con_neck_pycon_r = self.selectController('con_neck_pycon_r')

		self.m_con_neck_contracted_l = self.selectController('con_neck_contracted_l')
		self.m_con_neck_contracted_r = self.selectController('con_neck_contracted_r')

		self.m_con_neck_sternomastoid_l = self.selectController('con_neck_sternomastoid_l')
		self.m_con_neck_sternomastoid_r = self.selectController('con_neck_sternomastoid_r')

		#ear
		self.m_con_ear_l = self.selectController('con_ear_l')
		self.m_con_ear_r = self.selectController('con_ear_r')

		#quad
		self.m_con_lip_funnel = self.selectController('con_lip_funnel')
		self.m_con_lip_up_bite = self.selectController('con_lip_up_bite')
		self.m_con_lips_up_spread = self.selectController('con_lips_up_spread')

		#tongue
		self.m_con_tongue_box = self.selectController('con_tongue_box')
		self.m_con_tongue_bar = self.selectController('con_tongue_bar')


		#update controller list
		self.updateControllerLists()




	def CreateEyeJointCB(self):
		#eye joint create
		self.m_EyeRig.createEyeBoneCB()

		# self.m_EyeRig.echoState()

		eyeJointParentText = pm.textFieldGrp(self.eyeParentJntText, q=1, text=1)
		print ('Eye parent joint is {0}'.format(eyeJointParentText))
		#eye joint parent to head
		self.m_EyeRig.parentToHeadCB(head=eyeJointParentText)



	def updateShapeInfo(self):

		#facial blend shape list group root
		self.m_face_bs_root = None
		rootTransList = pm.ls( (self.m_face_bs_root_name+'*'), type='transform')

		if len(rootTransList):
			for root_trans in rootTransList :

				#set face bs root group
				self.m_face_bs_root = root_trans

				#check face parts list
				faceParts_list = pm.listRelatives(self.m_face_bs_root, children=True, type='transform')


				if self.m_face_bs_root is not None :

					#init base blend shape
					self.m_base_shape = None

					#get base shape
					for partGrp in faceParts_list:
						if '_bs' in str(partGrp):
							print( str(partGrp).replace("_bs", "") )
						elif self.m_base_shape_name == str(partGrp) :
							self.m_base_shape = DNodeOp.getItem(self.m_base_shape_name)


		'''
		self.m_face_bs_root = DNodeOp.getItem(self.m_face_bs_root_name)

		if self.m_face_bs_root is not None :

			#get base shape
			self.m_base_shape = None
			self.m_base_shape = DNodeOp.getItem(self.m_base_shape_name)
		'''




	def updateUIComponent(self):
		#update UIText
		pm.textField(self.baseBSText, e=True, text=self.m_face_bs_root)



	def checkBSListCB(self):

		isEyeState = self.m_EyeRig.checkEyeRigCB()
		if not isEyeState :
			pm.warning("Eye data is not valid.")
			return


		#check base shape
		self.updateShapeInfo()
		self.updateUIComponent()

		isCreateDummy = pm.checkBox( self.checkDummy, q=True,  value=True )
		print ('create dummy mode:{isDummy}'.format(isDummy=isCreateDummy))

		if self.m_base_shape is None :
			pm.warning("Not found base shape for blend shape.({bsname})".format(bsname=self.m_base_shape_name))
			return


		#check group. if not exist, create group node
		clist = pm.listRelatives(self.m_face_bs_root, children=True)
		for grp in self.m_bs_grp_all:

			if grp not in clist:
				print ('{groupname} is not exist.'.format(groupname=grp))
				DGrp.createEmpty(name=grp, parent=self.m_face_bs_root )
				print ('{groupname} is created.'.format(groupname=grp))


		#not_found_list = []

		#get eye look group
		eye_look_list = self.m_EyeRig.m_eye_look_bs_list

		#print ('eye list:{eyeSize}'.format( eyeSize=len(eye_look_list) ) )

		for bs in eye_look_list:
			if not pm.objExists(bs):
				#create base shape
				new_bs = pm.duplicate(self.m_base_shape, n=bs)[0]
				pm.setAttr((new_bs+'.visibility'), 0)
				pm.parent( new_bs, self.m_eye_look_bs_grp )
				print ('{bsname} is created dummy.'.format(bsname=bs))

		#other all bs list
		for k, bslist in self.m_bs_list_dict.items() :
			for bs in bslist:
				if not pm.objExists(bs):
					print ('{bsname} is not exist.'.format(bsname=bs))
					#not_found_list.append(bs)
					if isCreateDummy:
						new_bs = pm.duplicate(self.m_base_shape, n=bs)[0]
						pm.parent( new_bs, k )
						print ('{bsname} is created dummy.'.format(bsname=bs))

		print 'Finish check all shape.'



	def CreateCntlrCB(self):

		#check controller
		self.m_cntlr_root = None
		self.m_cntlr_root = DNodeOp.getItem(self.m_cntlr_root_name)

		if self.m_cntlr_root is None:
			self.m_cntlr_root = DGrp.createEmpty(name=self.m_cntlr_root_name, parent=None )



		######################    Brow Controller
		self.m_con_browRaise_l = createController('con_browRaise_l', parent=self.m_cntlr_root, pos=(5.0,17.0), rotate=-30,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )
		self.m_con_browRaise_r = createController('con_browRaise_r', parent=self.m_cntlr_root, pos=(-5.0,17.0), rotate=30,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )
		self.m_con_browRaise_m = createController('con_browRaise_m', parent=self.m_cntlr_root, pos=(0.0,17.5), rotate=0,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )



		self.m_con_browRaise_c_l = createController('con_browRaise_c_l', parent=self.m_cntlr_root, pos=(1.0,12.0), rotate=-10,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )
		self.m_con_browRaise_c_r = createController('con_browRaise_c_r', parent=self.m_cntlr_root, pos=(-1,12.0), rotate=10,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )
		self.m_con_browFrown_l = createController('con_browFrown_l', parent=self.m_cntlr_root, pos=(6.0,13.5), rotate=100,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )
		self.m_con_browFrown_r = createController('con_browFrown_r', parent=self.m_cntlr_root, pos=(-6,13.5), rotate=-100,
												conType=1, keepAttr=['translateY'], mov = self.m_mov )

		self.m_con_browLower_l = createController('con_browLower_l', parent=self.m_cntlr_root, pos=(8.0,14.5), rotate=180,
												conType=1, keepAttr=['translateY'], mov = self.m_mov*.66 )
		self.m_con_browLower_r = createController('con_browLower_r', parent=self.m_cntlr_root, pos=(-8,14.5), rotate=-180,
												conType=1, keepAttr=['translateY'], mov = self.m_mov*.66 )



		######################    Eye Controller

		#eye look
		#pm.addAttr('con_browLower_l', longName='test', attributeType='double', keyable=True, maxValue=10.0, minValue=0.0,softMaxValue=10.0,softMinValue=1.0)
		addAttrList = [{'longName':'pupil', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_eye_l = createController('con_eye_l', parent=self.m_cntlr_root, pos=(4.0,9.0), rotate=0,
												conType=0, keepAttr=['translateX', 'translateY'], addAttr=[], mov = self.m_mov*.8 )
		self.m_con_eye_r = createController('con_eye_r', parent=self.m_cntlr_root, pos=(-4.0,9.0), rotate=0,
												conType=0, keepAttr=['translateX', 'translateY'], addAttr=[], mov = self.m_mov*.8 )
		addAttrList = [{'longName':'pupil', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'conWeight', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':1.0, 'defaultValue':1.0},
						{'longName':'aimWeight', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':1.0, 'defaultValue':0.0}]
		self.m_con_eye_c = createController('con_eye_c', parent=self.m_cntlr_root, pos=(0.0,9.0), rotate=0,
												conType=0, keepAttr=['translateX', 'translateY'], addAttr=addAttrList )

		#eye open close
		addAttrList = [{'longName':'lidTight', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'cheekRaise', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_eye_opcl_l = createController('con_eye_opcl_l', parent=self.m_cntlr_root, pos=(6.5,9), rotate=0,
												conType=2, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_eye_opcl_r = createController('con_eye_opcl_r', parent=self.m_cntlr_root, pos=(-6.5,9), rotate=0,
												conType=2, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )

		#eye tight
		addAttrList = [{'longName':'eye_lid_smile', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_eye_up_l = createController('con_eye_up_l', parent=self.m_cntlr_root, pos=(8.0,11), rotate=-185,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov*.5  )
		self.m_con_eye_up_r = createController('con_eye_up_r', parent=self.m_cntlr_root, pos=(-8.0,11), rotate=185,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov*.5  )
		addAttrList = [{'longName':'eye_lid_smile', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'eye_lid_dn_in_up', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'eye_lid_dn_out_up', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'eye_lid_dn_smile', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_eye_dn_l = createController('con_eye_dn_l', parent=self.m_cntlr_root, pos=(8.1,7.5), rotate=15,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov*.5  )
		self.m_con_eye_dn_r = createController('con_eye_dn_r', parent=self.m_cntlr_root, pos=(-8.1,7.5), rotate=-15,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov*.5  )




		######################    Nose controller
		self.m_con_nose_l = createController('con_nose_l', parent=self.m_cntlr_root, pos=(1.0,4.4), rotate=5,
												conType=1, keepAttr=['translateY'], mov = self.m_mov*.66  )
		self.m_con_nose_r = createController('con_nose_r', parent=self.m_cntlr_root, pos=(-1,4.4), rotate=-5,
												conType=1, keepAttr=['translateY'], mov = self.m_mov*.66  )


		addAttrList = [{'longName':'delated', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'delatedExr', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_nose_hole_l = createController('con_nose_hole_l', parent=self.m_cntlr_root, pos=(1.6,2.5), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov*.5 )
		self.m_con_nose_hole_r = createController('con_nose_hole_r', parent=self.m_cntlr_root, pos=(-1.6,2.5), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov*.5 )
		self.m_con_nose_hole_r.flipSpace()


		######################    Cheek controller
		addAttrList = [{'longName':'cheek_raised', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'cheek_contracted', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'cheek_pop', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_cheek_l = createController('con_cheek_l', parent=self.m_cntlr_root, pos=(6.5,1.5), rotate=-25,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_cheek_r = createController('con_cheek_r', parent=self.m_cntlr_root, pos=(-6.5,1.5), rotate=25,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )


		######################    Cheek Puff controller
		addAttrList = [{'longName':'cheek_inhaled_no_mouth', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_cheek_puff_l = createController('con_cheek_puff_l', parent=self.m_cntlr_root, pos=(12.1,-4.5), rotate=-90,
												conType=2, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_cheek_puff_r = createController('con_cheek_puff_r', parent=self.m_cntlr_root, pos=(-12.1,-4.5), rotate=90,
												conType=2, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )



		######################    Mouth Move Controller
		addAttrList = [{'longName':'mouth_closed_contracted_jaw', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_closed_chin_up', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_opened_closed_teeth', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_kiss', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_protude', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_smile_lip_closed', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_smile_lip_opened', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_inner', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_frown', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_closed_blown', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'pucker_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'pucker_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'oo', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'oh', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						#{'longName':'funnel_u_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						#{'longName':'funnel_u_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						#{'longName':'funnel_d_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						#{'longName':'funnel_d_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'suck_u_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'suck_u_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'suck_d_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'suck_d_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'press_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'press_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'sideWays', 'attributeType':'double', 'keyable':True,'minValue':-10.0,'maxValue':10.0},
						{'longName':'puffSuck_l', 'attributeType':'double', 'keyable':True,'minValue':-10,'maxValue':10.0},
						{'longName':'puffSuck_r', 'attributeType':'double', 'keyable':True,'minValue':-10,'maxValue':10.0},
						{'longName':'stickylips', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':0.0},
						{'longName':'sticky_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'sticky_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'envelope_u', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'envelope_d', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}
						#{'longName':'tongue', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':0.0}
						]
		self.m_con_mouth_c = createController('con_mouth_c', parent=self.m_cntlr_root, pos=(0.0,-4.5), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList )




		######################    Lip Controller

		#ver 2
		#lip center
		self.m_con_lip_up_c = createController('con_lip_up_c', parent=self.m_cntlr_root, pos=(0.0, -2.3), rotate=0,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_lip_dn_c = createController('con_lip_dn_c', parent=self.m_cntlr_root, pos=(0.0, -7.7), rotate=180,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )



		#lip corner
		addAttrList = [{'longName':'lips_frown_expr', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_opend', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_opened_closed_teeth', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_opened_pinched_corner', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_protude_d', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_smile_lip_closed', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'mouth_smile_lip_opened', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]

		self.m_con_lip_corner_l = createController('con_lip_corner_l', parent=self.m_cntlr_root, pos=(7.0,-5.0), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov*.8 )
		self.m_con_lip_corner_r = createController('con_lip_corner_r', parent=self.m_cntlr_root, pos=(-7.0,-5.0), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov*.8 )
		self.m_con_lip_corner_r.flipSpace()

		#lip up
		#ver 1 2
		addAttrList = [{'longName':'lips_up_tight', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_upperLipRaise_l = createController('con_upperLipRaise_l', parent=self.m_cntlr_root, pos=(3.3,-2.4), rotate=-10,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_upperLipRaise_r = createController('con_upperLipRaise_r', parent=self.m_cntlr_root, pos=(-3.3,-2.4), rotate=10,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )

		'''
		addAttrList = [{'longName':'lipCornorPullOpen', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'dimple', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_lipCornerPull_l = createController('con_lipCornerPull_l', parent=self.m_cntlr_root, pos=(6.4,-3.8), rotate=-20,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_lipCornerPull_r = createController('con_lipCornerPull_r', parent=self.m_cntlr_root, pos=(-6.4,-3.8), rotate=20,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		'''

		'''
		addAttrList = [{'longName':'lipStretchOpen', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_lipStretch_l = createController('con_lipStretch_l', parent=self.m_cntlr_root, pos=(7.0,-5.0), rotate=-90,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_lipStretch_r = createController('con_lipStretch_r', parent=self.m_cntlr_root, pos=(-7.0,-5.0), rotate=90,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		'''
		'''
		ver 1
		self.m_con_lipCornerDepress_l = createController('con_lipCornerDepress_l', parent=self.m_cntlr_root, pos=(6.0,-6.2), rotate=-160,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_lipCornerDepress_r = createController('con_lipCornerDepress_r', parent=self.m_cntlr_root, pos=(-6.0,-6.2), rotate=160,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		'''

		#ver 2
		self.m_con_lipCornerDepress_l = createController('con_lipCornerDepress_l', parent=self.m_cntlr_root, pos=(5.3,-7.2), rotate=-145,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_lipCornerDepress_r = createController('con_lipCornerDepress_r', parent=self.m_cntlr_root, pos=(-5.3,-7.2), rotate=145,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )

		#lip dn
		addAttrList = [{'longName':'lips_dn_raised', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_dn_lower', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
						{'longName':'lips_dn_raised_m', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_lowerLipDepress_l = createController('con_lowerLipDepress_l', parent=self.m_cntlr_root, pos=(3.1,-7.5), rotate=-170,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )
		self.m_con_lowerLipDepress_r = createController('con_lowerLipDepress_r', parent=self.m_cntlr_root, pos=(-3.1,-7.5), rotate=170,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )

		addAttrList = [{'longName':'upperChinRaise', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'lowerLipProtude', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_chinRaise_c = createController('con_chinRaise_c', parent=self.m_cntlr_root, pos=(0.0,-15.0), rotate=0,
												conType=1, keepAttr=['translateY'], addAttr=addAttrList, mov = self.m_mov )





		######################    Mouth Side Controller
		self.m_con_lip_io_up = createController('con_lip_io_up', parent=self.m_cntlr_root, pos=(29.5,-5.52), rotate=92,
												conType=2, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_lip_io_dn = createController('con_lip_io_dn', parent=self.m_cntlr_root, pos=(29.643,-7.81), rotate=100,
												conType=2, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )

		self.m_con_lip_protude_up = createController('con_lip_protude_up', parent=self.m_cntlr_root, pos=(26.828,-5.556), rotate=-272.421,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_lip_protude_dn = createController('con_lip_protude_dn', parent=self.m_cntlr_root, pos=(27.0,-8.258), rotate=-255.211,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )


		######################    Jaw Side Controller
		addAttrList = [{'longName':'jaw_move_out', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'jaw_move_lower', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'jaw_up_offset', 'attributeType':'double', 'keyable':True,'minValue':-100.0,'maxValue':100.0},
					{'longName':'jaw_dn_offset', 'attributeType':'double', 'keyable':True,'minValue':-100.0,'maxValue':100.0},
					{'longName':'jaw_dn_weight', 'attributeType':'double', 'keyable':True,'minValue':0,'maxValue':1.0,'dv':1.0},
					{'longName':'trust', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'scream', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0},
					{'longName':'chew', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':10.0}]
		self.m_con_jaw_c = createController('con_jaw_c', parent=self.m_cntlr_root, pos=(0.0,-19), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov*2 )
		#self.m_con_jaw_c = createController('con_jaw_c', parent=self.m_cntlr_root, pos=(0.0,0.0), rotate=0,
		#										conType=2, keepAttr=['translateX','translateY'], addAttr=addAttrList, mov = self.m_mov )



		######################    Neck Controller
		self.m_con_neck_l = createController('con_neck_l', parent=self.m_cntlr_root, pos=(6.5,-15), rotate=-155,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_neck_r = createController('con_neck_r', parent=self.m_cntlr_root, pos=(-6.5,-15), rotate=155,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )

		self.m_con_neck_pycon_l = createController('con_neck_pycon_l', parent=self.m_cntlr_root, pos=(10.5,-14.5), rotate=-90,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_neck_pycon_r = createController('con_neck_pycon_r', parent=self.m_cntlr_root, pos=(-10.5,-14.5), rotate=90,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_neck_contracted_l = createController('con_neck_contracted_l', parent=self.m_cntlr_root, pos=(4.4,-22), rotate=0,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_neck_contracted_r = createController('con_neck_contracted_r', parent=self.m_cntlr_root, pos=(-4.4,-22), rotate=0,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )

		self.m_con_neck_sternomastoid_l = createController('con_neck_sternomastoid_l', parent=self.m_cntlr_root, pos=(4.4,-15.5), rotate=180,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_neck_sternomastoid_r = createController('con_neck_sternomastoid_r', parent=self.m_cntlr_root, pos=(-4.4,-15.5), rotate=-180,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )




		######################    Ear Controller
		self.m_con_ear_l = createController('con_ear_l', parent=self.m_cntlr_root, pos=(12,2), rotate=-10,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )
		self.m_con_ear_r = createController('con_ear_r', parent=self.m_cntlr_root, pos=(-12,2), rotate=10,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )



		######################    Face OutLine
		self.m_face_outline_crv = DRgCnOp.createFaceOutLine('con_face_outline_crv')
		pm.parent(self.m_face_outline_crv, self.m_cntlr_root )

		self.m_face_side_outline_crv = DRgCnOp.createFaceSideOutline('con_face_side_outline_crv')
		pm.parent(self.m_face_side_outline_crv, self.m_cntlr_root )


		######################    Quad Dirs
		self.m_con_lip_funnel = createController('con_lip_funnel', parent=self.m_cntlr_root, pos=(-7.0,-27), rotate=0,
												conType=3, keepAttr=[], addAttr=[], mov = self.m_mov )
		self.m_con_lip_funnel.addText(text='funnel', size=.3, padding=0.2)
		self.m_con_lip_up_bite = createController('con_lip_up_bite', parent=self.m_cntlr_root, pos=(-0.0,-27), rotate=0,
												conType=3, keepAttr=[], addAttr=[], mov = self.m_mov )
		self.m_con_lip_up_bite.addText(text='bite', size=.3, padding=0.2)
		self.m_con_lips_up_spread = createController('con_lips_up_spread', parent=self.m_cntlr_root, pos=(7.0,-27), rotate=0,
												conType=3, keepAttr=[], addAttr=[], mov = self.m_mov )
		self.m_con_lips_up_spread.addText(text='spread', size=.3, padding=0.2)



		######################    Tongue
		self.m_con_tongue_box = createController('con_tongue_box', parent=self.m_cntlr_root, pos=(28.0,-19), rotate=0,
												conType=0, keepAttr=['translateX','translateY'], addAttr=[], mov = self.m_mov*2 )
		self.m_con_tongue_box.addText(text='tongue', size=.3, padding=0.2)


		self.m_con_tongue_bar = createController('con_tongue_bar', parent=self.m_cntlr_root, pos=(29,-23), rotate=90,
												conType=1, keepAttr=['translateY'], addAttr=[], mov = self.m_mov )


		self.updateControllerLists()



	def setTopStickyCB(self):
		try:
			selfList = selOp.getSelectedEdgeIdsList()
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Top sticky edges were not changed: {0}'.format(error))
			return False

		if len(selfList):
			mergeText = ' '.join(str(e) for e in selfList)
			pm.textField( self.topStickyText, e=True, tx=mergeText)
			self.m_top_sticky_edge_list = selfList
		else :
			pm.textField( self.topStickyText, e=True, tx='')
			self.m_top_sticky_edge_list = []


	def setBotStickyCB(self):
		try:
			selfList = selOp.getSelectedEdgeIdsList()
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Bottom sticky edges were not changed: {0}'.format(error))
			return False

		if len(selfList):
			mergeText = ' '.join(str(e) for e in selfList)
			pm.textField( self.botStickyText, e=True, tx=mergeText)
			self.m_bot_sticky_edge_list = selfList
		else :
			pm.textField( self.botStickyText, e=True, tx='')
			self.m_bot_sticky_edge_list = []


	def loadLipEdgeCB(self):
		filters = "*.elip"
		file = pm.fileDialog2(cap='Load Edge List',fileFilter=filters, okc='Load', fileMode=1)

		if file is None:
			return False

		try:
			data = lip_edges.load(file[0])
		except (IOError, OSError, lip_edges.LipEdgeFormatError) as error:
			pm.warning('Lip-edge data was not loaded: {0}'.format(error))
			return False

		self.m_top_sticky_edge_list = list(data.top)
		self.m_bot_sticky_edge_list = list(data.bottom)
		pm.textField(
			self.topStickyText, edit=True,
			text=' '.join(str(edge) for edge in data.top))
		pm.textField(
			self.botStickyText, edit=True,
			text=' '.join(str(edge) for edge in data.bottom))
		return True


	def saveLipEdgeCB(self):

		#check edge list
		if len(self.m_top_sticky_edge_list) == 0 or len(self.m_bot_sticky_edge_list) == 0:
			pm.warning('Edge list is not exist. First set to top and bottom edge list.')
			return

		filters = "*.elip"
		file = pm.fileDialog2(cap='Save Edge List',fileFilter=filters, dialogStyle=2, okc='Save')
		print file

		if file is not None:

			ext = file[0].rpartition('.')[-1]

			if ext != 'elip':
				pm.warning('File extension is invalid.')
				return

			try:
				lip_edges.save(
					file[0], self.m_top_sticky_edge_list,
					self.m_bot_sticky_edge_list)
			except (IOError, OSError, lip_edges.LipEdgeFormatError) as error:
				pm.warning('Lip-edge data was not saved: {0}'.format(error))
				return False
			return True



	def transferSkinnedMesh(self, basemesh = ''):
		u"""事前準備されたスキンされたメッシュから現在のメッシュにスキンデータをトランスファーします。"""


		jnts_stes = pm.textFieldGrp(self.skinnedJntSets, q=1, text=1)
		skinned_mesh = pm.textFieldGrp(self.skinnedMeshText, q=1, text=1)
		print ('Joint Sets:{0}'.format(jnts_stes))
		print ('Skinned Mesh:{0}'.format(skinned_mesh))

		jntSets = None
		try:
			jntSets = pm.general.PyNode(jnts_stes)
		except (RuntimeError, TypeError):
			jntSets = None

		if jntSets is None or jntSets.type() != 'objectSet':
			pm.warning('Not Found joint sets for skin transfer!')
			return
		if (not pm.objExists(basemesh) or not pm.objExists(skinned_mesh)
				or basemesh == skinned_mesh):
			pm.warning('Skin transfer requires different existing source and target meshes.')
			return

		#get joint list from select sets
		pm.select(cl=1)
		pm.select( jnts_stes )
		jnt_list = pm.ls(sl=1)


		#select for skining mesh and joint
		pm.select(cl=1)
		pm.select(basemesh, tgl=1)
		pm.select(jnt_list, tgl=1)

		#skinned mesh
		print ('Skin bind joint set ....')
		pm.skinCluster( jnt_list, basemesh,
									skinMethod=1,
									toSelectedBones=True,
									maximumInfluences=3,
									normalizeWeights=1,
									dropoffRate=3.0)

		print ('Finish Skining prosess!\n\n\n')


		print ('Start Copy weight ....')

		pm.select(cl=1)
		pm.select(skinned_mesh, tgl=1)
		pm.select(basemesh, tgl=1)
		pm.copySkinWeights( noMirror=1, surfaceAssociation='closestPoint', influenceAssociation=['closestJoint', 'oneToOne', 'name'])

		print ('Finish copy weight prosess!\n\n\n')

		# Keep the source mesh; deletion is an explicit user responsibility.
		print ('Skin transfer source retained: {0}\n'.format(skinned_mesh))








	def CreateFaceRigCB(self):

		########prepare########

		#check sticky
		if len(self.m_top_sticky_edge_list) == 0 or len(self.m_bot_sticky_edge_list) == 0:
			pm.warning('Sticky edge list is no valid. Select mesh edge, and click SET.')
			return


		#hide base shape
		pm.setAttr((self.m_base_shape.visibility), 0)


		########Create Eye Bone###############################################################################

		isCreatedEyeBone = self.m_EyeRig.createEyeBoneCB()
		if not isCreatedEyeBone :
			pm.warning('Sticky edge list is no valid. Select mesh edge, and click SET.')
			return

		#parent to head joint
		self.m_EyeRig.parentToHeadCB()

		########create Eye Constaraint ########

		#eye constraint to joint
		self.m_EyeRig.makeEyeJointParentCB()

		#self.m_EyeRig.createEyeConstCB()
		######## Create Eye Blend aim and controller ########
		if not self.m_EyeRig.createBlendAimAndCons() :
			pm.warning('Failed Create blend aim and controller dummy locator.')
			return

		######## Create Eye LookAt Link to box########

		conDummyList = self.m_EyeRig.getConDummyList()

		#L Eye
		createDrivenKeys(driver=(self.m_con_eye_c.ctlr+'.translateX'), target=(conDummyList[0]+'.rotateY'),
						times=[-1,0,1], values=[-40,0,40])
		createDrivenKeys(driver=(self.m_con_eye_c.ctlr+'.translateY'), target=(conDummyList[0]+'.rotateX'),
						times=[-1,0,1], values=[40,0,-40])

		#R Eye
		createDrivenKeys(driver=(self.m_con_eye_c.ctlr+'.translateX'), target=(conDummyList[1]+'.rotateY'),
						times=[-1,0,1], values=[-40,0,40])
		createDrivenKeys(driver=(self.m_con_eye_c.ctlr+'.translateY'), target=(conDummyList[1]+'.rotateX'),
						times=[-1,0,1], values=[40,0,-40])

		######## Connect blend attribute ########
		blend_cons_list = self.m_EyeRig.getBlendOrientConst()
		pm.connectAttr( (self.m_con_eye_c.ctlr+'.aimWeight'), (blend_cons_list[0]+'.m_eye_aim_dmy_loc_lW0'), force=True )
		pm.connectAttr( (self.m_con_eye_c.ctlr+'.aimWeight'), (blend_cons_list[1]+'.m_eye_aim_dmy_loc_rW0'), force=True )
		pm.connectAttr( (self.m_con_eye_c.ctlr+'.conWeight'), (blend_cons_list[0]+'.m_eye_con_dmy_loc_lW1'), force=True )
		pm.connectAttr( (self.m_con_eye_c.ctlr+'.conWeight'), (blend_cons_list[1]+'.m_eye_con_dmy_loc_rW1'), force=True )


		########create look eye BS########

		#get eye shape list
		eye_look_list_l = self.m_EyeRig.m_eye_look_bs_list_l
		eye_look_list_r = self.m_EyeRig.m_eye_look_bs_list_r

		leftSoftEyes = pm.duplicate(self.m_base_shape, n='leftSoftEyes')[0]
		leftSoftEyesBS = createBSFromList(bslist=eye_look_list_l, srcShape=leftSoftEyes, name='leftSoftEyesBS')
		print ('Created BS:{name}'.format(name=leftSoftEyesBS) )

		rightSoftEyes = pm.duplicate(self.m_base_shape, n='rightSoftEyes')[0]
		rightSoftEyesBS = createBSFromList(bslist=eye_look_list_r, srcShape=rightSoftEyes, name='rightSoftEyesBS')
		print ('Created BS:{name}'.format(name=rightSoftEyesBS) )


		########create shape blends###############################################################################

		#set one list bs list
		allLiat = []
		for bslist in self.m_bs_list_all:
			allLiat = allLiat+bslist

		shapes = pm.duplicate(self.m_base_shape, n='shapes')[0]
		shapesBS = createBSFromList(bslist=allLiat, srcShape=shapes, name='shapesBS')
		print ('Created BS:{name}'.format(name=shapesBS) )


		#create parallel BS
		parallelHead = pm.duplicate(self.m_base_shape, n='parallelHead')[0]
		parallelBlender = createBSFromList(bslist=[leftSoftEyes, rightSoftEyes, shapes], srcShape=parallelHead, name='parallelBlender')
		print ('Created BS:{name}'.format(name=parallelBlender) )

		#set weight all 1.0
		pm.blendShape(parallelBlender, edit=True, w=[(0,1.0), (1,1.0), (2,1.0)])


		#create sticky effect
		stickyHead = pm.duplicate(self.m_base_shape, n='stickyHead')[0]
		parallelToStickyBS = createBSFromList(bslist=[parallelHead], srcShape=stickyHead, name='parallelToStickyBS')
		pm.blendShape( parallelToStickyBS, edit=True, w=[ (0,1.0) ] )
		print ('Created BS:{name}'.format(name=parallelToStickyBS) )

		#add sticky attribute
		addAttrList = [{'longName':'sticky_l', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':1.0},
						{'longName':'sticky_r', 'attributeType':'double', 'keyable':True,'minValue':0.0,'maxValue':1.0}
						]
		for attr in addAttrList:
			pm.addAttr(stickyHead, **attr)


		#create sticky sub data group
		sticky_stuff_grp = DGrp.createEmpty(name='sticky_stuff_grp', parent=None )
		pm.setAttr((sticky_stuff_grp.visibility), 0)


		########create lip curves########


		#top lip
		topLipCurveFromEdges = createCurveFromMeshEdgeList(self.m_top_sticky_edge_list, parallelHead.getShape())[0]
		pm.rename(topLipCurveFromEdges, 'topLipCurveFromEdges')
		pm.parent(topLipCurveFromEdges, sticky_stuff_grp)


		#bot lip
		botLipCurveFromEdges = createCurveFromMeshEdgeList(self.m_bot_sticky_edge_list, parallelHead.getShape())[0]
		pm.rename(botLipCurveFromEdges, 'botLipCurveFromEdges')
		pm.parent(botLipCurveFromEdges, sticky_stuff_grp)

		#create center line
		centerLine = pm.duplicate(topLipCurveFromEdges, n='centerLine')[0]
		centerLineBS = createBSFromList(bslist=[topLipCurveFromEdges, botLipCurveFromEdges], srcShape=centerLine, name='centerLineBS')
		pm.blendShape( centerLineBS, edit=True, w=[ (0,0.5),(1,0.5) ] )
		pm.parent(centerLine, sticky_stuff_grp)


		########create lip cluster########

		#top curve for cluster
		topLipWire = pm.duplicate(topLipCurveFromEdges, n='topLipWire')[0]
		topLipCurveFromEdgesToTopLipWireBS = createBSFromList(bslist=[topLipCurveFromEdges], srcShape=topLipWire, name='topLipCurveFromEdgesToTopLipWireBS')
		pm.blendShape( topLipCurveFromEdgesToTopLipWireBS, edit=True, w=[ (0,1.0) ] )
		pm.parent(topLipWire, sticky_stuff_grp)
		print ('Created BS:{name}'.format(name=topLipCurveFromEdgesToTopLipWireBS) )

		#bot curve for cluster
		botLipWire = pm.duplicate(botLipCurveFromEdges, n='botLipWire')[0]
		botLipCurveFromEdgesToBotLipWireBS = createBSFromList(bslist=[botLipCurveFromEdges], srcShape=botLipWire, name='botLipCurveFromEdgesToBotLipWireBS')
		pm.blendShape( botLipCurveFromEdgesToBotLipWireBS, edit=True, w=[ (0,1.0) ] )
		pm.parent(botLipWire, sticky_stuff_grp)
		print ('Created BS:{name}'.format(name=botLipCurveFromEdgesToBotLipWireBS) )

		#create cluster
		#top
		createClusterFromCurveCV(crv=topLipWire, centerCrv=centerLine, stickyAttrL=(stickyHead+'.sticky_l'),stickyAttrR=(stickyHead+'.sticky_r'), parent=sticky_stuff_grp)
		#bot
		createClusterFromCurveCV(crv=botLipWire, centerCrv=centerLine, stickyAttrL=(stickyHead+'.sticky_l'),stickyAttrR=(stickyHead+'.sticky_r'), parent=sticky_stuff_grp)


		########create lip wire deformer########

		#top
		topLipWireBaseWire = pm.duplicate(topLipCurveFromEdges, n='topLipWireBaseWire')[0]
		topLipCurveFromEdgesToTopLipWireBaseWireBS = createBSFromList(bslist=[topLipCurveFromEdges], srcShape=topLipWireBaseWire, name='topLipCurveFromEdgesToTopLipWireBaseWireBS')
		pm.blendShape( topLipCurveFromEdgesToTopLipWireBaseWireBS, edit=True, w=[ (0,1.0) ] )
		pm.setAttr((topLipWireBaseWire.visibility), 0)
		pm.parent(topLipWireBaseWire, sticky_stuff_grp)
		print ('Created BS:{name}'.format(name=topLipCurveFromEdgesToTopLipWireBaseWireBS) )

		#create top wire
		wireNodeTop = createLipWireDeform(baseCrv=topLipWireBaseWire, deformCrv=topLipWire, baseShape=stickyHead, wireName='topLipWireDef')
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.envelope_u'), target=(wireNodeTop+'.envelope'),
						times=[0.0,10.0], values=[0.0,1.0])


		#bot
		botLipWireBaseWire = pm.duplicate(botLipCurveFromEdges, n='botLipWireBaseWire')[0]
		botLipCurveFromEdgesToBotLipWireBaseWireBS = createBSFromList(bslist=[botLipCurveFromEdges], srcShape=botLipWireBaseWire, name='botLipCurveFromEdgesToBotLipWireBaseWireBS')
		pm.blendShape( botLipCurveFromEdgesToBotLipWireBaseWireBS, edit=True, w=[ (0,1.0) ] )
		pm.setAttr((botLipWireBaseWire.visibility), 0)
		pm.parent(botLipWireBaseWire, sticky_stuff_grp)
		print ('Created BS:{name}'.format(name=botLipCurveFromEdgesToBotLipWireBaseWireBS) )

		#create bot wire
		wireNodeBot = createLipWireDeform(baseCrv=botLipWireBaseWire, deformCrv=botLipWire, baseShape=stickyHead, wireName='botLipWireDef')
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.envelope_d'), target=(wireNodeBot+'.envelope'),
						times=[0.0,10.0], values=[0.0,1.0])


		#sticky connect controller
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.sticky_l'), target=(stickyHead+'.sticky_l'),
						times=[0.0,10.0], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.sticky_r'), target=(stickyHead+'.sticky_r'),
						times=[0.0,10.0], values=[0.0,1.0])



		########create final Face########

		face = pm.duplicate(self.m_base_shape, n='face')[0]
		stickyHeadBS = createBSFromList(bslist=[stickyHead], srcShape=face, name='stickyHeadBS')
		pm.blendShape( stickyHeadBS, edit=True, w=[ (0,1.0) ] )
		print ('Created BS:{name}'.format(name=stickyHeadBS) )
		pm.setAttr((face.visibility), 1)




		########   Connect Face Controller   ########

		######## brow
		createDrivenKeys(driver=(self.m_con_browRaise_l.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_l'),
						times=[0.0,self.m_mov], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_browRaise_r.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_r'),
						times=[0.0,self.m_mov], values=[0.0,1.0])
		#createDrivenKeys(driver=(self.m_con_browRaise_c_l.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_c_l'),
		#				times=[0.0,1], values=[0.0,1.0])
		#createDrivenKeys(driver=(self.m_con_browRaise_c_r.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_c_r'),
		#				times=[0.0,1], values=[0.0,1.0])

		createDrivenKeys(driver=(self.m_con_browFrown_l.ctlr+'.translateY'), target=(shapesBS+'.brow_frown_l'),
						times=[0.0,self.m_mov], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_browFrown_r.ctlr+'.translateY'), target=(shapesBS+'.brow_frown_r'),
						times=[0.0,self.m_mov], values=[0.0,1.0])

		createDrivenKeys(driver=(self.m_con_browRaise_c_l.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_c_l'),
						times=[0.0,self.m_mov], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_browRaise_c_r.ctlr+'.translateY'), target=(shapesBS+'.brow_raise_c_r'),
						times=[0.0,self.m_mov], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_browLower_l.ctlr+'.translateY'), target=(shapesBS+'.brow_lower_l'),
						times=[0.0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_browLower_r.ctlr+'.translateY'), target=(shapesBS+'.brow_lower_r'),
						times=[0.0,1], values=[0.0,1.0])



		######## eye left
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.translateY'), target=(shapesBS+'.eye_blink1_l'),
						times=[self.eye_mbias[1],self.eye_mbias[2],self.eye_mbias[3]], values=[0.0,1.0,0.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.translateY'), target=(shapesBS+'.eye_blink2_l'),
						times=[self.eye_mbias[0],self.eye_mbias[1],self.eye_mbias[2]], values=[0,1,0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.translateY'), target=(shapesBS+'.eye_blink_max_l'),
						times=[self.eye_mbias[0],self.eye_mbias[1]], values=[1.0,0.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.translateY'), target=(shapesBS+'.eye_upperLidRaise_l'),
						times=[self.eye_mbias[3],self.eye_mbias[4]], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.translateY'), target=(leftSoftEyesBS+'.envelope'),
						times=[self.eye_mbias[1],self.eye_mbias[3]], values=[0,1])

		#open close
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.lidTight'), target=(shapesBS+'.eye_lidTight_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_l.ctlr+'.cheekRaise'), target=(shapesBS+'.cheek_raise_l'),
						times=[0,10], values=[0.0,1.0])

		#tight up
		createDrivenKeys(driver=(self.m_con_eye_up_l.ctlr+'.translateY'), target=(shapesBS+'.eye_lidTight_u_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_up_l.ctlr+'.eye_lid_smile'), target=(shapesBS+'.eye_lid_smile_u_l'),
						times=[0,10], values=[0.0,1.0])

		#tight dn
		createDrivenKeys(driver=(self.m_con_eye_dn_l.ctlr+'.translateY'), target=(shapesBS+'.eye_lidTight_d_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_l.ctlr+'.eye_lid_smile'), target=(shapesBS+'.eye_lid_smile_d_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_l.ctlr+'.eye_lid_dn_in_up'), target=(shapesBS+'.eye_lid_dn_in_up_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_l.ctlr+'.eye_lid_dn_out_up'), target=(shapesBS+'.eye_lid_dn_out_up_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_l.ctlr+'.eye_lid_dn_smile'), target=(shapesBS+'.eye_lid_dn_smile_l'),
						times=[0,10], values=[0.0,1.0])



		######## eye right
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.translateY'), target=(shapesBS+'.eye_blink1_r'),
						times=[self.eye_mbias[1],self.eye_mbias[2],self.eye_mbias[3]], values=[0.0,1.0,0.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.translateY'), target=(shapesBS+'.eye_blink2_r'),
						times=[self.eye_mbias[0],self.eye_mbias[1],self.eye_mbias[2]], values=[0,1,0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.translateY'), target=(shapesBS+'.eye_blink_max_r'),
						times=[self.eye_mbias[0],self.eye_mbias[1]], values=[1.0,0.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.translateY'), target=(shapesBS+'.eye_upperLidRaise_r'),
						times=[self.eye_mbias[3],self.eye_mbias[4]], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.translateY'), target=(rightSoftEyesBS+'.envelope'),
						times=[self.eye_mbias[1],self.eye_mbias[3]], values=[0,1])

		#open close
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.lidTight'), target=(shapesBS+'.eye_lidTight_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_opcl_r.ctlr+'.cheekRaise'), target=(shapesBS+'.cheek_raise_r'),
						times=[0,10], values=[0.0,1.0])

		#tight up
		createDrivenKeys(driver=(self.m_con_eye_up_r.ctlr+'.translateY'), target=(shapesBS+'.eye_lidTight_u_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_up_r.ctlr+'.eye_lid_smile'), target=(shapesBS+'.eye_lid_smile_u_r'),
						times=[0,10], values=[0.0,1.0])

		#tight dn
		createDrivenKeys(driver=(self.m_con_eye_dn_r.ctlr+'.translateY'), target=(shapesBS+'.eye_lidTight_d_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_r.ctlr+'.eye_lid_smile'), target=(shapesBS+'.eye_lid_smile_d_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_r.ctlr+'.eye_lid_dn_in_up'), target=(shapesBS+'.eye_lid_dn_in_up_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_r.ctlr+'.eye_lid_dn_out_up'), target=(shapesBS+'.eye_lid_dn_out_up_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_eye_dn_r.ctlr+'.eye_lid_dn_smile'), target=(shapesBS+'.eye_lid_dn_smile_r'),
						times=[0,10], values=[0.0,1.0])


		######## nose
		createDrivenKeys(driver=(self.m_con_nose_l.ctlr+'.translateY'), target=(shapesBS+'.nose_wrinkle_l'),
						times=[0,1], values=[0,1])

		createDrivenKeys(driver=(self.m_con_nose_r.ctlr+'.translateY'), target=(shapesBS+'.nose_wrinkle_r'),
						times=[0,1], values=[0,1])

		#nose in out
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.translateX'), target=(shapesBS+'.nose_nostrils_out_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.translateX'), target=(shapesBS+'.nose_nostrils_compressed_l'),
						times=[-1,0], values=[1,0])

		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.translateX'), target=(shapesBS+'.nose_nostrils_out_r'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.translateX'), target=(shapesBS+'.nose_nostrils_compressed_r'),
						times=[-1,0], values=[1,0])

		#nose height
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.translateY'), target=(shapesBS+'.nose_nostrils_delated_max_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.translateY'), target=(shapesBS+'.nose_nostrils_delated_max_r'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.translateY'), target=(shapesBS+'.nose_nostrils_lower_l'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.translateY'), target=(shapesBS+'.nose_nostrils_lower_r'),
						times=[-1,0], values=[1,0])

		#nose hole
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.delated'), target=(shapesBS+'.nose_nostrils_delated_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.delated'), target=(shapesBS+'.nose_nostrils_delated_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_l.ctlr+'.delatedExr'), target=(shapesBS+'.nose_nostrils_delated_exr_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_nose_hole_r.ctlr+'.delatedExr'), target=(shapesBS+'.nose_nostrils_delated_exr_r'),
						times=[0,10], values=[0,1])

		#cheek
		createDrivenKeys(driver=(self.m_con_cheek_l.ctlr+'.translateY'), target=(shapesBS+'.cheek_raised_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_r.ctlr+'.translateY'), target=(shapesBS+'.cheek_raised_r'),
						times=[0,1], values=[0,1])
		#createDrivenKeys(driver=(self.m_con_cheek_l.ctlr+'.cheek_raised'), target=(shapesBS+'.cheek_raised_l'),
		#				times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_l.ctlr+'.cheek_contracted'), target=(shapesBS+'.cheek_contracted_l'),
						times=[0,10], values=[0,1])
		#createDrivenKeys(driver=(self.m_con_cheek_r.ctlr+'.cheek_raised'), target=(shapesBS+'.cheek_raised_r'),
		#				times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_r.ctlr+'.cheek_contracted'), target=(shapesBS+'.cheek_contracted_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_l.ctlr+'.cheek_pop'), target=(shapesBS+'.cheek_pop_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_r.ctlr+'.cheek_pop'), target=(shapesBS+'.cheek_pop_r'),
						times=[0,10], values=[0,1])


		#cheek puff
		createDrivenKeys(driver=(self.m_con_cheek_puff_l.ctlr+'.translateY'), target=(shapesBS+'.cheek_puff_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_puff_l.ctlr+'.translateY'), target=(shapesBS+'.cheek_inhaled_l'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_cheek_puff_r.ctlr+'.translateY'), target=(shapesBS+'.cheek_puff_r'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_puff_r.ctlr+'.translateY'), target=(shapesBS+'.cheek_inhaled_r'),
						times=[-1,0], values=[1,0])

		createDrivenKeys(driver=(self.m_con_cheek_puff_l.ctlr+'.cheek_inhaled_no_mouth'), target=(shapesBS+'.cheek_inhaled_no_mouth_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_cheek_puff_r.ctlr+'.cheek_inhaled_no_mouth'), target=(shapesBS+'.cheek_inhaled_no_mouth_r'),
						times=[0,10], values=[0,1])


		#mouth
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.translateX'), target=(shapesBS+'.mouth_sideways_l2'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.translateX'), target=(shapesBS+'.mouth_sideways_r2'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.translateY'), target=(shapesBS+'.lip_push_up'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.translateY'), target=(shapesBS+'.lip_push_down'),
						times=[-1,0], values=[1,0])


		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.mouth_closed_contracted_jaw'), target=(shapesBS+'.mouth_closed_contracted_jaw'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.mouth_closed_chin_up'), target=(shapesBS+'.mouth_closed_chin_up'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.mouth_opened_closed_teeth'), target=(shapesBS+'.mouth_opened_closed_teeth'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.lips_kiss'), target=(shapesBS+'.lips_kiss'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.lips_protude'), target=(shapesBS+'.lips_protude'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.mouth_smile_lip_closed'), target=(shapesBS+'.mouth_smile_lip_closed1'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.mouth_smile_lip_opened'), target=(shapesBS+'.mouth_smile_lip_opened1'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.lips_inner'), target=(shapesBS+'.lips_inner'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.lips_frown'), target=(shapesBS+'.lips_frown'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.lips_closed_blown'), target=(shapesBS+'.lips_closed_blown'),
						times=[0,10], values=[0.0,1.0])


		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.pucker_l'), target=(shapesBS+'.mouth_pucker_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.pucker_r'), target=(shapesBS+'.mouth_pucker_r'),
						times=[0,10], values=[0.0,1.0])

		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.oo'), target=(shapesBS+'.mouth_oo_c'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.oh'), target=(shapesBS+'.mouth_oh_c'),
						times=[0,10], values=[0.0,1.0])

		'''
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.funnel_u_l'), target=(shapesBS+'.mouth_funnel_u_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.funnel_u_r'), target=(shapesBS+'.mouth_funnel_u_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.funnel_d_l'), target=(shapesBS+'.mouth_funnel_d_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.funnel_d_r'), target=(shapesBS+'.mouth_funnel_d_r'),
						times=[0,10], values=[0.0,1.0])
		'''

		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.suck_u_l'), target=(shapesBS+'.mouth_suck_u_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.suck_u_r'), target=(shapesBS+'.mouth_suck_u_r'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.suck_d_l'), target=(shapesBS+'.mouth_suck_d_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.suck_d_r'), target=(shapesBS+'.mouth_suck_d_r'),
						times=[0,10], values=[0.0,1.0])

		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.press_l'), target=(shapesBS+'.mouth_press_l'),
						times=[0,10], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.press_r'), target=(shapesBS+'.mouth_press_r'),
						times=[0,10], values=[0.0,1.0])

		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.sideWays'), target=(shapesBS+'.mouth_sideways_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.sideWays'), target=(shapesBS+'.mouth_sideways_r'),
						times=[-10,0], values=[1,0])

		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.puffSuck_l'), target=(shapesBS+'.cheek_puff_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.puffSuck_l'), target=(shapesBS+'.cheek_suck_l'),
						times=[-10,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.puffSuck_r'), target=(shapesBS+'.cheek_puff_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_mouth_c.ctlr+'.puffSuck_r'), target=(shapesBS+'.cheek_suck_r'),
						times=[-10,0], values=[1,0])


		########Lips connect########
		#ver2
		createDrivenKeys(driver=(self.m_con_lip_up_c.ctlr+'.translateY'), target=(shapesBS+'.lips_up_raised_c'),
						times=[0,self.m_mov], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_dn_c.ctlr+'.translateY'), target=(shapesBS+'.lips_up_lower_c'),
						times=[0,self.m_mov], values=[0,1])

		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.translateX'), target=(shapesBS+'.lips_protude_l'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.translateX'), target=(shapesBS+'.lips_stretch_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.translateX'), target=(shapesBS+'.lips_protude_r'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.translateX'), target=(shapesBS+'.lips_stretch_r'),
						times=[0,1], values=[0,1])

		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_closed_corner_lower_l'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_closed_corner_raised_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_closed_corner_lower_r'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_closed_corner_raised_r'),
						times=[0,1], values=[0,1])

		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.lips_frown_expr'), target=(shapesBS+'.lips_frown_expr_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.lips_frown_expr'), target=(shapesBS+'.lips_frown_expr_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.mouth_opend'), target=(shapesBS+'.mouth_opend_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.mouth_opend'), target=(shapesBS+'.mouth_opend_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.mouth_opened_closed_teeth'), target=(shapesBS+'.mouth_opened_closed_teeth_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.mouth_opened_closed_teeth'), target=(shapesBS+'.mouth_opened_closed_teeth_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.mouth_opened_pinched_corner'), target=(shapesBS+'.mouth_opened_pinched_corner_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.mouth_opened_pinched_corner'), target=(shapesBS+'.mouth_opened_pinched_corner_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.lips_protude_d'), target=(shapesBS+'.lips_protude_d_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.lips_protude_d'), target=(shapesBS+'.lips_protude_d_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.mouth_smile_lip_closed'), target=(shapesBS+'.mouth_smile_lip_closed_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.mouth_smile_lip_closed'), target=(shapesBS+'.mouth_smile_lip_closed_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_l.ctlr+'.mouth_smile_lip_opened'), target=(shapesBS+'.mouth_smile_lip_opened_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_corner_r.ctlr+'.mouth_smile_lip_opened'), target=(shapesBS+'.mouth_smile_lip_opened_r'),
						times=[0,10], values=[0,1])


		#ver1 ver2
		createDrivenKeys(driver=(self.m_con_upperLipRaise_l.ctlr+'.translateY'), target=(shapesBS+'.lips_up_raised_l'),
						times=[0,self.m_mov], values=[0,1])
		createDrivenKeys(driver=(self.m_con_upperLipRaise_r.ctlr+'.translateY'), target=(shapesBS+'.lips_up_raised_r'),
						times=[0,self.m_mov], values=[0,1])

		createDrivenKeys(driver=(self.m_con_upperLipRaise_l.ctlr+'.lips_up_tight'), target=(shapesBS+'.lips_up_tight_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_upperLipRaise_r.ctlr+'.lips_up_tight'), target=(shapesBS+'.lips_up_tight_r'),
						times=[0,10], values=[0,1])

		'''
		ver1
		createDrivenKeys(driver=(self.m_con_lipCornerPull_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipCornerPull_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerPull_l.ctlr+'.lipCornorPullOpen'), target=(shapesBS+'.mouth_lipCornerPullOpen_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerPull_l.ctlr+'.dimple'), target=(shapesBS+'.mouth_dimple_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerPull_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipCornerPull_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerPull_r.ctlr+'.lipCornorPullOpen'), target=(shapesBS+'.mouth_lipCornerPullOpen_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerPull_r.ctlr+'.dimple'), target=(shapesBS+'.mouth_dimple_r'),
						times=[0,10], values=[0,1])
		'''


		'''
		#ver1
		createDrivenKeys(driver=(self.m_con_lipStretch_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipStretch_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipStretch_l.ctlr+'.lipStretchOpen'), target=(shapesBS+'.mouth_lipStretchOpen_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipStretch_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipStretch_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipStretch_r.ctlr+'.lipStretchOpen'), target=(shapesBS+'.mouth_lipStretchOpen_r'),
						times=[0,10], values=[0,1])
		'''

		'''
		ver 1
		createDrivenKeys(driver=(self.m_con_lipCornerDepress_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipCornerDepress_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerDepress_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_lipCornerDepress_r'),
						times=[0,10], values=[0,1])
		'''
		#ver 2
		createDrivenKeys(driver=(self.m_con_lipCornerDepress_l.ctlr+'.translateY'), target=(shapesBS+'.lips_dn_lower_m_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lipCornerDepress_r.ctlr+'.translateY'), target=(shapesBS+'.lips_dn_lower_m_r'),
						times=[0,1], values=[0,1])

		#lip dn
		#ver 2
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_lowerLipDepress_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.lips_dn_raised'), target=(shapesBS+'.lips_dn_raised_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.lips_dn_lower'), target=(shapesBS+'.lips_dn_lower_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.lips_dn_raised_m'), target=(shapesBS+'.lips_dn_raised_m_l'),
						times=[0,10], values=[0,1])
		#createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.lips_dn_lower_m'), target=(shapesBS+'.lips_dn_lower_m_l'),
		#				times=[0,10], values=[0,1])

		createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_lowerLipDepress_r'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.lips_dn_raised'), target=(shapesBS+'.lips_dn_raised_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.lips_dn_lower'), target=(shapesBS+'.lips_dn_lower_r'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.lips_dn_raised_m'), target=(shapesBS+'.lips_dn_raised_m_r'),
						times=[0,10], values=[0,1])
		#createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.lips_dn_lower_m'), target=(shapesBS+'.lips_dn_lower_m_r'),
		#				times=[0,10], values=[0,1])


		#ver 1
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_l.ctlr+'.translateY'), target=(shapesBS+'.mouth_lowerLipDepress_l'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lowerLipDepress_r.ctlr+'.translateY'), target=(shapesBS+'.mouth_lowerLipDepress_r'),
						times=[0,10], values=[0,1])


		#chin
		createDrivenKeys(driver=(self.m_con_chinRaise_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_chinRaise_d'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_chinRaise_c.ctlr+'.upperChinRaise'), target=(shapesBS+'.mouth_chinRaise_u'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_chinRaise_c.ctlr+'.lowerLipProtude'), target=(shapesBS+'.mouth_lowerLipProtude_c'),
						times=[0,10], values=[0,1])




		########Mouth Side########
		createDrivenKeys(driver=(self.m_con_lip_io_up.ctlr+'.translateY'), target=(shapesBS+'.lips_up_out'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_io_up.ctlr+'.translateY'), target=(shapesBS+'.lips_up_in'),
						times=[-1,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_lip_io_dn.ctlr+'.translateY'), target=(shapesBS+'.lips_dn_out'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_lip_io_dn.ctlr+'.translateY'), target=(shapesBS+'.lips_dn_in'),
						times=[-1,0], values=[1,0])

		########Jaw ########
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateX'), target=(shapesBS+'.jaw_sideways_l'),
						times=[0,self.m_mov], values=[0,1])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateX'), target=(shapesBS+'.jaw_sideways_r'),
						times=[-self.m_mov,0], values=[1,0])
		#createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_stretch_c'),
		#				times=[-self.m_mov*3,0], values=[1,0])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_little_opened'),
						times=[-0.666,-0.333,0], values=[0,1,0])
		#createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_opened'),
		#				times=[-0.75,-0.5,-0.25], values=[0,1,0])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_exlarged_opened1'),
						times=[-1.0, -0.666, -0.333], values=[0,1,0])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=(shapesBS+'.mouth_wide_opened'),
						times=[-1.0,-0.666], values=[1,0])

		#jaw to teeth_dn_grp
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateY'), target=('teeth_dn_root'+'.rotateX'),
						times=[-1.0 ,-0.666,-0.333,0], values=[38.295,34.151,4.529,0])

		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateX'), target=('teeth_dn_root'+'.rotateZ'),
						times=[-1.0,0], values=[-7.56,0])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.translateX'), target=('teeth_dn_root'+'.rotateZ'),
						times=[0,1.0], values=[0, 7.56])


		#jaw option motion
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.jaw_move_out'), target=(shapesBS+'.jaw_move_out'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.jaw_move_lower'), target=(shapesBS+'.jaw_move_lower'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.scream'), target=(shapesBS+'.mouth_scream_c'),
						times=[0,10], values=[0,1])
		createDrivenKeys(driver=(self.m_con_jaw_c.ctlr+'.chew'), target=(shapesBS+'.jaw_thrust_c'),
						times=[0,10], values=[0,1])



		########Quad Dirs ########
		#funnel
		createDrivenKeys(driver=(self.m_con_lip_funnel.dir_ul.ctlr+'.translateY'), target=(shapesBS+'.lips_funnel_u_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_funnel.dir_ur.ctlr+'.translateY'), target=(shapesBS+'.lips_funnel_u_r'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_funnel.dir_dl.ctlr+'.translateY'), target=(shapesBS+'.lips_funnel_d_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_funnel.dir_dr.ctlr+'.translateY'), target=(shapesBS+'.lips_funnel_d_r'),
						times=[0,1], values=[0.0,1.0])

		#up_bite
		createDrivenKeys(driver=(self.m_con_lip_up_bite.dir_ul.ctlr+'.translateY'), target=(shapesBS+'.lips_up_bite_u_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_up_bite.dir_ur.ctlr+'.translateY'), target=(shapesBS+'.lips_up_bite_u_r'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_up_bite.dir_dl.ctlr+'.translateY'), target=(shapesBS+'.lips_up_bite_d_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lip_up_bite.dir_dr.ctlr+'.translateY'), target=(shapesBS+'.lips_up_bite_d_r'),
						times=[0,1], values=[0.0,1.0])

		#spread
		createDrivenKeys(driver=(self.m_con_lips_up_spread.dir_ul.ctlr+'.translateY'), target=(shapesBS+'.lips_up_spread_u_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lips_up_spread.dir_ur.ctlr+'.translateY'), target=(shapesBS+'.lips_up_spread_u_r'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lips_up_spread.dir_dl.ctlr+'.translateY'), target=(shapesBS+'.lips_up_spread_d_l'),
						times=[0,1], values=[0.0,1.0])
		createDrivenKeys(driver=(self.m_con_lips_up_spread.dir_dr.ctlr+'.translateY'), target=(shapesBS+'.lips_up_spread_d_r'),
						times=[0,1], values=[0.0,1.0])



		########Neck ########
		createDrivenKeys(driver=(self.m_con_neck_l.ctlr+'.translateY'), target=(shapesBS+'.neck_platysma_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_neck_r.ctlr+'.translateY'), target=(shapesBS+'.neck_platysma_r'),
						times=[0,1], values=[0,1])

		createDrivenKeys(driver=(self.m_con_neck_pycon_l.ctlr+'.translateY'), target=(shapesBS+'.neck_platysma_cntracted_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_neck_pycon_r.ctlr+'.translateY'), target=(shapesBS+'.neck_platysma_cntracted_r'),
						times=[0,1], values=[0,1])

		createDrivenKeys(driver=(self.m_con_neck_contracted_l.ctlr+'.translateY'), target=(shapesBS+'.neck_contracted_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_neck_contracted_r.ctlr+'.translateY'), target=(shapesBS+'.neck_contracted_r'),
						times=[0,1], values=[0,1])


		createDrivenKeys(driver=(self.m_con_neck_sternomastoid_l.ctlr+'.translateY'), target=(shapesBS+'.neck_sternomastoid_contracted_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_neck_sternomastoid_r.ctlr+'.translateY'), target=(shapesBS+'.neck_sternomastoid_contracted_r'),
						times=[0,1], values=[0,1])

		########Ear ########
		createDrivenKeys(driver=(self.m_con_ear_l.ctlr+'.translateY'), target=(shapesBS+'.ear_up_l'),
						times=[0,1], values=[0,1])
		createDrivenKeys(driver=(self.m_con_ear_r.ctlr+'.translateY'), target=(shapesBS+'.ear_up_r'),
						times=[0,1], values=[0,1])


		########Connect Eye Controller########

		#left Eye
		#print self.m_EyeRig.m_eye_jnt_l
		#print self.m_EyeRig.m_eye_l_jnt_name
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'), target=(leftSoftEyesBS+'.eye_lookUp_l'),
						times=[-23,0], values=[1,0])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'), target=(leftSoftEyesBS+'.eye_lookDown1_l'),
						times=[0,16,32], values=[0,1,0])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'), target=(leftSoftEyesBS+'.eye_lookDown2_l'),
						times=[16,32], values=[0,1])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_l_jnt_name+'.rotateY'), target=(leftSoftEyesBS+'.eye_lookLeft_l'),
						times=[0,35], values=[0,1])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_l_jnt_name+'.rotateY'), target=(leftSoftEyesBS+'.eye_lookRight_l'),
						times=[-35,0], values=[1,0])

		#right Eye
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'), target=(rightSoftEyesBS+'.eye_lookUp_r'),
						times=[-23,0], values=[1,0])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'), target=(rightSoftEyesBS+'.eye_lookDown1_r'),
						times=[0,16,32], values=[0,1,0])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'), target=(rightSoftEyesBS+'.eye_lookDown2_r'),
						times=[16,32], values=[0,1])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_r_jnt_name+'.rotateY'), target=(rightSoftEyesBS+'.eye_lookLeft_r'),
						times=[0,35], values=[0,1])
		createDrivenKeys(driver=(self.m_EyeRig.m_eye_r_jnt_name+'.rotateY'), target=(rightSoftEyesBS+'.eye_lookRight_r'),
						times=[-35,0], values=[1,0])

		################Left Eye Offset################
		is_connected_rotXPlug = pm.connectionInfo( (self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'), isDestination=True)
		is_connected_rotYPlug = pm.connectionInfo( (self.m_EyeRig.m_eye_l_jnt_name+'.rotateY'), isDestination=True)
		connected_rotXPlug = pm.connectionInfo((self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'),sourceFromDestination=True)
		connected_rotYPlug = pm.connectionInfo((self.m_EyeRig.m_eye_l_jnt_name+'.rotateY'),sourceFromDestination=True)

		print( 'Connected rotateX: %s' % is_connected_rotXPlug )
		print( 'Connected rotateY: %s' % is_connected_rotYPlug )
		print( 'Source: %s' % connected_rotXPlug )
		print( 'Source: %s' % connected_rotYPlug )

		eye_look_l_x_sub_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='eye_look_l_x_sub_offset_addDoubleLinear' )
		eye_look_l_y_sub_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='eye_look_l_y_sub_offset_addDoubleLinear' )

		#connect sub eye con to multiply node
		eye_l_x_sub_param_multDoubleLinear = pm.createNode( 'multDoubleLinear', n='eye_l_x_sub_param_multDoubleLinear' )
		eye_l_y_sub_param_multDoubleLinear = pm.createNode( 'multDoubleLinear', n='eye_l_y_sub_param_multDoubleLinear' )
		pm.setAttr((eye_l_x_sub_param_multDoubleLinear+'.input2'),30)
		pm.setAttr((eye_l_y_sub_param_multDoubleLinear+'.input2'),-30)
		pm.connectAttr( (self.m_con_eye_l.ctlr+'.translateX'), (eye_l_x_sub_param_multDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (self.m_con_eye_l.ctlr+'.translateY'), (eye_l_y_sub_param_multDoubleLinear+'.input1'), force=True )

		#conect to Joint RotX
		pm.connectAttr( connected_rotXPlug, (eye_look_l_x_sub_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (eye_l_y_sub_param_multDoubleLinear+'.output'), (eye_look_l_x_sub_offset_addDoubleLinear+'.input2'), force=True )

		pm.connectAttr( (eye_look_l_x_sub_offset_addDoubleLinear+'.output'), (self.m_EyeRig.m_eye_l_jnt_name+'.rotateX'), force=True )

		#conect to Joint RotY
		pm.connectAttr( connected_rotYPlug, (eye_look_l_y_sub_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (eye_l_x_sub_param_multDoubleLinear+'.output'), (eye_look_l_y_sub_offset_addDoubleLinear+'.input2'), force=True )

		pm.connectAttr( (eye_look_l_y_sub_offset_addDoubleLinear+'.output'), (self.m_EyeRig.m_eye_l_jnt_name+'.rotateY'), force=True )


		################Right Eye Offset################
		is_connected_rotXPlug = pm.connectionInfo( (self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'), isDestination=True)
		is_connected_rotYPlug = pm.connectionInfo( (self.m_EyeRig.m_eye_r_jnt_name+'.rotateY'), isDestination=True)
		connected_rotXPlug = pm.connectionInfo((self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'),sourceFromDestination=True)
		connected_rotYPlug = pm.connectionInfo((self.m_EyeRig.m_eye_r_jnt_name+'.rotateY'),sourceFromDestination=True)


		'''
		print( 'Connected rotateX: %s' % is_connected_rotXPlug )
		print( 'Connected rotateY: %s' % is_connected_rotYPlug )
		print( 'Source: %s' % connected_rotXPlug )
		print( 'Source: %s' % connected_rotYPlug )
		'''


		eye_look_r_x_sub_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='eye_look_r_x_sub_offset_addDoubleLinear' )
		eye_look_r_y_sub_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='eye_look_r_y_sub_offset_addDoubleLinear' )

		#connect sub eye con to multiply node
		eye_r_x_sub_param_multDoubleLinear = pm.createNode( 'multDoubleLinear', n='eye_r_x_sub_param_multDoubleLinear' )
		eye_r_y_sub_param_multDoubleLinear = pm.createNode( 'multDoubleLinear', n='eye_r_y_sub_param_multDoubleLinear' )
		pm.setAttr((eye_r_x_sub_param_multDoubleLinear+'.input2'),30)
		pm.setAttr((eye_r_y_sub_param_multDoubleLinear+'.input2'),-30)
		pm.connectAttr( (self.m_con_eye_r.ctlr+'.translateX'), (eye_r_x_sub_param_multDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (self.m_con_eye_r.ctlr+'.translateY'), (eye_r_y_sub_param_multDoubleLinear+'.input1'), force=True )

		#conect to Joint RotX
		pm.connectAttr( connected_rotXPlug, (eye_look_r_x_sub_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (eye_r_y_sub_param_multDoubleLinear+'.output'), (eye_look_r_x_sub_offset_addDoubleLinear+'.input2'), force=True )

		pm.connectAttr( (eye_look_r_x_sub_offset_addDoubleLinear+'.output'), (self.m_EyeRig.m_eye_r_jnt_name+'.rotateX'), force=True )

		#conect to Joint RotY
		pm.connectAttr( connected_rotYPlug, (eye_look_r_y_sub_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (eye_r_x_sub_param_multDoubleLinear+'.output'), (eye_look_r_y_sub_offset_addDoubleLinear+'.input2'), force=True )

		pm.connectAttr( (eye_look_r_y_sub_offset_addDoubleLinear+'.output'), (self.m_EyeRig.m_eye_r_jnt_name+'.rotateY'), force=True )








		################Connect Mouth inner grp################
		#constraint parent
		teeth_up_const = pm.parentConstraint( 'teeth_up_root', 'mouth_in_grp|teeth_grp_u', maintainOffset=True )
		teeth_dn_const = pm.parentConstraint( 'teeth_dn_root', 'mouth_in_grp|teeth_grp_d', maintainOffset=True )

		#connect offset dn
		teeth_up_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='teeth_offset_teeth_up_root_addDoubleLinear' )
		teeth_dn_offset_addDoubleLinear = pm.createNode( 'addDoubleLinear', n='teeth_offset_teeth_dn_root_addDoubleLinear' )
		teeth_dn_jaw_weight_multDoubleLinear = pm.createNode( 'multDoubleLinear', n='teeth_dn_jaw_weight_multDoubleLinear' )

		pm.connectAttr( (teeth_dn_const+'.constraintRotateX'), (teeth_dn_jaw_weight_multDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (teeth_dn_jaw_weight_multDoubleLinear+'.output'), (teeth_dn_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (teeth_dn_offset_addDoubleLinear+'.output'), ('mouth_in_grp|teeth_grp_d.rotateX'), force=True )
		pm.connectAttr( (self.m_con_jaw_c.ctlr+'.jaw_dn_offset'), (teeth_dn_offset_addDoubleLinear+'.input2'), force=True )
		pm.connectAttr( (self.m_con_jaw_c.ctlr+'.jaw_dn_weight'), (teeth_dn_jaw_weight_multDoubleLinear+'.input2'), force=True )

		#connect offset up
		pm.connectAttr( (teeth_up_const+'.constraintRotateX'), (teeth_up_offset_addDoubleLinear+'.input1'), force=True )
		pm.connectAttr( (teeth_up_offset_addDoubleLinear+'.output'), ('mouth_in_grp|teeth_grp_u.rotateX'), force=True )
		pm.connectAttr( (self.m_con_jaw_c.ctlr+'.jaw_up_offset'), (teeth_up_offset_addDoubleLinear+'.input2'), force=True )



		########Tongue Setup########
		#tongue connect
		#tongue_root_const = pm.parentConstraint( 'teeth_dn_root', 'tongue_jnt_grp', maintainOffset=True )

		#get joint list
		tongue_cntrl_grp = DGrp.createEmpty(name='tongue_cntrl_grp', parent=None )
		jnt_list = pm.listRelatives( 'tongue_jnt1', type='joint', allDescendents=True )
		jnt_list.append('tongue_jnt1')
		jnt_list.reverse()

		#create tongue controllers
		index = 1
		preParent = None
		preJnt = None
		offset_trans_lists = []
		auto_trans_lists = []
		#auto_trans_lists_tip = []

		for jnt in jnt_list:
			#print jnt

			#create empty transforms
			offset_trans = DPTgrp.createEmptyWithAlign(name='tongue_'+str(index)+'_offset_grp', alignObj=jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			frz_trans = DPTgrp.createEmptyWithAlign(name='tongue_'+str(index)+'_frz', alignObj=jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
			ctrl_trans = DPTgrp.createEmptyWithAlign(name='tongue_'+str(index)+'_ctrl', alignObj=jnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)

			pm.parent(frz_trans, offset_trans)
			pm.parent(ctrl_trans, frz_trans)

			#create relations
			if index == 1:
				pm.parentConstraint( 'teeth_dn_root', offset_trans, maintainOffset=True )
			elif index != 1 :
				cst_trans = DPTgrp.createEmptyWithAlign(name='loc_tongue_'+str(index)+'_cst', alignObj=preJnt, isTrans=True, isRot=True, isScale=False, isUnParent=True)
				pm.parent(cst_trans, preParent)
				pm.parentConstraint(  cst_trans, offset_trans, maintainOffset=True )

			auto_trans_lists.append(frz_trans)


			#constraint joint to controller locator
			pm.pointConstraint( ctrl_trans, jnt)
			pm.orientConstraint( ctrl_trans, jnt)

			#for parent cache
			preParent = ctrl_trans
			preJnt = jnt

			pm.parent(offset_trans, tongue_cntrl_grp)

			index = index+1


		####connect tongue controllers

		#extend tongue
		tongue_extend_mdl = pm.createNode( 'multDoubleLinear', n='tongue_extend_mdl' )
		pm.connectAttr( (self.m_con_tongue_bar.ctlr+'.translateY'), (tongue_extend_mdl+'.input1'), force=True )
		pm.setAttr((tongue_extend_mdl+'.input2'),1)

		for ajnt in auto_trans_lists :
			pm.connectAttr( (tongue_extend_mdl+'.output'), (ajnt+'.translateX'), force=True )

		#twist and roll tongue
		index = 1
		#base_rotX = 5
		rot_max = 100.0
		step = 100.0/float(len(auto_trans_lists))
		for ajnt in auto_trans_lists :

			mdl_tongue_rot_Y = pm.createNode( 'multDoubleLinear', n=('tongue_mdl_'+str(index)+'_rot_Y') )
			pm.setAttr((mdl_tongue_rot_Y+'.input2'), step*float(index) )
			mdl_tongue_rot_X = pm.createNode( 'multDoubleLinear', n=('tongue_mdl_'+str(index)+'_rot_X') )
			pm.setAttr((mdl_tongue_rot_X+'.input2'), step*float(index) )

			pm.connectAttr( (self.m_con_tongue_box.ctlr+'.translateX'), (mdl_tongue_rot_Y+'.input1'), force=True )
			pm.connectAttr( (self.m_con_tongue_box.ctlr+'.translateY'), (mdl_tongue_rot_X+'.input1'), force=True )

			pm.connectAttr( (mdl_tongue_rot_Y+'.output'), (ajnt+'.rotateY'), force=True )
			pm.connectAttr( (mdl_tongue_rot_X+'.output'), (ajnt+'.rotateZ'), force=True )

			index = index+1


		#constraint mouth controller tojoint

		pm.parentConstraint(  'mouth_axis_grp', 'head_jnt', maintainOffset=True )


		#skinning transfer prosess
		self.transferSkinnedMesh(basemesh=face)

		#create face option joint
		rivetTarget = pm.textFieldGrp(self.rivetMeshText, q=1, tx=1)
		createFaceOptionJointController(rivetTarget=rivetTarget)






	def checkStickyEffectCB(self):

		pm.setAttr( (self.m_con_mouth_c.ctlr+'.sticky_l'), 10)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.sticky_r'), 10)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.envelope_u'), 10)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.envelope_d'), 10)


		pm.setAttr( (self.m_con_jaw_c.ctlr+'.translateY'), -1)

		if (pm.objExists('topLipWireDef')) :
			pm.attrFieldSliderGrp(self.topWireDistSlider, e=True, enable=True, at=('topLipWireDef.dropoffDistance[0]') )
		else :
			pm.attrFieldSliderGrp(self.topWireDistSlider, e=True, enable=False  )

		if (pm.objExists('botLipWireDef')) :
			pm.attrFieldSliderGrp(self.botWireDistSlider, e=True, enable=True, at=('botLipWireDef.dropoffDistance[0]')  )
		else :
			pm.attrFieldSliderGrp(self.botWireDistSlider, e=True, enable=False  )


	def setSelectObjectInTF(self, textField=''):
		u"""選択しているオブジェクトをテキストフィールドに入力します"""


		pm.textFieldGrp(textField, e=1, text='')

		sels = pm.ls(sl=1)

		if len(sels) :
			pm.textFieldGrp(textField, e=1, text=sels[0])


	def duplicateSelectedJoint(self):
		u"""選択されているジョイントを複製します"""

		pm.duplicate( parentOnly=1 )

	def reTargetFacialPartsCB(self):
		u"""フェイスのパーツを他のオブジェクトへりターゲットします"""
		u"""現在目のオブジェクトはインスタンスされます"""

		retargetObjectText = pm.textFieldGrp(self.reTargetText, q=1, text=1)

		if len(retargetObjectText) == 0 :
			pm.warning('Please enter retarget object name!')
			return


		targetObj = None
		try:
			targetObj = pm.general.PyNode(retargetObjectText)
		except (RuntimeError, TypeError):
			pm.warning('Not exist retarget object : {0}'.format(retargetObjectText))
			return

		print ('Target object : {0}'.format(targetObj))

		#Get UI info

		isEye = pm.checkBox(self.checkEyeReTar, q=1, value=1 )
		isMouth = pm.checkBox( self.checkMouthReTar, q=1, value=1 )
		isLacrimal = pm.checkBox( self.checkLacrimaReTar, q=1, value=1 )



		#Create Group
		target_loc = pm.spaceLocator(n=('retarget_loc'))
		DPTtrans.setTransformToObjectZero( srcObj=target_loc, targetObj=targetObj, isTrans=1, isRot=1, isScale=0, isUnParent=1)

		render_geo_grp = DGrp.createEmpty(name=('render_geo_grp') )
		DPTtrans.setTransformToObjectZero( srcObj=render_geo_grp, targetObj=targetObj, isTrans=1, isRot=0, isScale=0, isUnParent=1)

		pm.parent(render_geo_grp, target_loc)



		#eye retarget
		# eye_list = ['eye_root_l', 'eye_root_r']
		# for eye in eye_list:

		# 	inst = pm.instance( eye, leaf=0, name=(eye+'_inst'))
		# 	pm.parent(inst[0],target_loc)
		# 	pm.orientConstraint(eye, inst[0])
		if isEye :
			eye_inst = pm.instance( 'eye_grp', leaf=0, name=('eye_grp_inst'))
			pm.parent(eye_inst[0],render_geo_grp)
			pm.parent('lookAt_freeTransform', render_geo_grp)
			pm.parent('m_eye_aim_dmy_loc_l', render_geo_grp)
			pm.parent('m_eye_aim_dmy_loc_r', render_geo_grp)

		#mouth retarget
		if isMouth :
			mouth_inst = pm.instance( 'mouth_in_grp', leaf=0, name=('mouth_in_grp_inst'))
			pm.parent(mouth_inst[0],render_geo_grp)

		#lacrimal retarget
		if isLacrimal :
			lacrimal_grp_inst = pm.instance( 'lacrimal_grp', leaf=0, name=('lacrimal_grp_inst'))
			pm.parent(lacrimal_grp_inst[0],render_geo_grp)

		print ('Finish retarget object.')



	def resetFaceControllerCB(self):

		#reset ui
		if self.topWireDistSlider is not None:
			pm.attrFieldSliderGrp(self.topWireDistSlider, e=True, enable=False  )
		if self.botWireDistSlider is not None:
			pm.attrFieldSliderGrp(self.botWireDistSlider, e=True, enable=False  )


		#mouth
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.sticky_l'), 0)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.sticky_r'), 0)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.envelope_u'), 0)
		pm.setAttr( (self.m_con_mouth_c.ctlr+'.envelope_d'), 0)

		#jaw
		pm.setAttr( (self.m_con_jaw_c.ctlr+'.translateY'), 0)


	def resetPartControllerCB(self, controllers=None):
		"""Specific Controller Reset Func.

		"""

		#print controllers
		#loop controller
		for con in controllers:
			#print con
			#print ('type:{tyepName}'.format(tyepName=type(con)))
			if con is None:
				continue


			if isinstance(con, DRgCnOp.quadDirectBox):

				dirList = con.getControllers()
				for dir in dirList:
					keyAttrList = pm.listAttr(dir.ctlr, keyable=True)
					for attr in keyAttrList:
						DPAttr.setAttrDefaultValue(dir.ctlr, attr)
			else:

				keyAttrList = pm.listAttr(con.ctlr, keyable=True)

				for attr in keyAttrList:
					DPAttr.setAttrDefaultValue(con.ctlr, attr)


	def resetAllControllerParamsCB(self):
		"""All Controller Reset Func.

		"""
		#loop controller list
		for conlist in self.m_all_con_lists:

			self.resetPartControllerCB(conlist)

		#reset eye rig (for aim locator)
		self.m_EyeRig.resetEyeRig()



	def modifyOpenMouthWeightCB(self):
		"""modify mouth open weight for blend shapes.

		"""
		#pm.select( clear=True )
		#pm.select('shapesBS_mouth_little_opened', r=True)
		#pm.select('shapesBS_mouth_exlarged_opened1', add=True)
		#pm.select('shapesBS_mouth_wide_opened', add=True)
		#pm.select('teeth_dn_root_rotateX', add=True)

		try:
			pm.select( clear=True )
			pm.select('shapesBS_mouth_little_opened', r=True)
			pm.select('shapesBS_mouth_exlarged_opened1', add=True)
			pm.select('shapesBS_mouth_wide_opened', add=True)
			pm.select('teeth_dn_root_rotateX', add=True)

		except RuntimeError:
			pm.warning('Faild select open mouth weight node!')


	############## Face and mouth expression Functions ##############

	############## UI ##############
	def deleteExprButtonUI(self, layout=None):
		childCount = pm.rowColumnLayout(layout, q=True, numberOfChildren=True)
		childList = pm.rowColumnLayout(layout, q=True, childArray=True)

		if childCount:
			for ch in childList:
				pm.deleteUI( ch, control=True )


	def deleteFaceExprButtonAll(self):
		self.deleteExprButtonUI(layout=self.faceExprRowColumnLayout)


	def deleteMouthExprButtonAll(self):
		self.deleteExprButtonUI(layout=self.mouthExprRowColumnLayout)


	def addExprBtn(self, idx=-1, name ='', parts_type=0):

		if parts_type==0:
			#face
			pm.setParent( self.faceExprRowColumnLayout )
		elif parts_type==1:
			#mouth
			pm.setParent( self.mouthExprRowColumnLayout )


		pm.button(label=name, c = lambda *args: self.changeExpressionCB(exprNum=idx, parts_type=parts_type))
		pm.popupMenu()
		pm.menuItem(label='Update Expr', c = lambda *args: self.updateExprCB(exprNum=idx, parts_type=parts_type))

	def updateExprCB(self, exprNum=-1, parts_type=0):
		"""
		Update specific expr data to current expression data.
		"""

		if parts_type==0:
			#face
			if not len(self.m_face_expression_list)>exprNum:
				return

			nowExprData = self.m_face_expression_list[exprNum]
			exprData = self.createCurrentFaceData(name=nowExprData.m_name, con_list_array=self.m_all_con_lists)
			self.m_face_expression_list[exprNum] = exprData

		elif parts_type==1:
			#mouth
			if not len(self.m_mouth_expression_list)>exprNum:
				return

			nowExprData = self.m_mouth_expression_list[exprNum]

			#print ('index:{idx}'.format(idx=exprNum))
			#print ('name:{name}'.format(name=nowExprData.m_name))
			exprData = self.createCurrentFaceData(name=nowExprData.m_name, con_list_array=self.m_mouth_con_lists)
			#print ('new name:{name}'.format(name=exprData.m_name))
			self.m_mouth_expression_list[exprNum] = exprData


		print ('Updated Expression data.')



	def updateFaceListUI(self):

		if len(self.m_face_expression_list)==0:
			return

		#delete all button
		self.deleteFaceExprButtonAll()

		count = len(self.m_face_expression_list)

		for i in range(count):
			expr = self.m_face_expression_list[i]
			self.addExprBtn(name=expr.m_name, idx=i, parts_type=0)

	def updateMouthListUI(self):

		if len(self.m_mouth_expression_list)==0:
			return

		#delete all button
		self.deleteMouthExprButtonAll()

		count = len(self.m_mouth_expression_list)

		for i in range(count):
			expr = self.m_mouth_expression_list[i]
			self.addExprBtn(name=expr.m_name, idx=i, parts_type=1)



	def createCurrentFaceData(self, name='', con_list_array=None):
		"""
		Create Face Data in current face expression
		"""
		if con_list_array is None:
			con_list_array = []


		#new expr data
		exprData = DFaceExpr(name=name)

		#loop controller list
		for conlist in con_list_array:

			#loop controller
			for con in conlist:
				#print ('con name:{name}'.format(name=con.ctlr))

				if con is None:
					continue


				c_type = self.getControllerType(con.ctlr)
				#print ('con type:{name}'.format(name=c_type))


				#check quad controller
				if isinstance(con, DRgCnOp.quadDirectBox):

					dirList = con.getControllers()
					for dir in dirList:

						#create info data
						cntlr_info_data = DControllerInfo(name=dir.ctlr, ctype=c_type)

						keyAttrList = pm.listAttr(dir.ctlr, keyable=True)
						for attr in keyAttrList:
							value = pm.getAttr( dir.ctlr+'.'+attr )
							#print ('-attr:{attr} v:{value}'.format(attr=attr, value=value))
							cntlr_info_data.addAttrValue(attr_name=attr, attr_value=value)

						exprData.addControllerInfo(cntlr_info_data)

				else:

					#create info data
					cntlr_info_data = DControllerInfo(name=con.ctlr, ctype=c_type)

					keyAttrList = pm.listAttr(con.ctlr, keyable=True)

					for attr in keyAttrList:
						value = pm.getAttr( con.ctlr+'.'+attr )
						#print ('-attr:{attr} v:{value}'.format(attr=attr, value=value))
						cntlr_info_data.addAttrValue(attr_name=attr, attr_value=value)

					#add controller
					exprData.addControllerInfo(cntlr_info_data)

		return exprData


	def addExprData(self, parts_type=0, con_list_array=None):

		expreName = ''

		result = pm.promptDialog(
			title='Expression Name',
			message='Enter Name:',
			button=['Add', 'Cancel'],
			defaultButton='Add',
			cancelButton='Cancel',
			dismissString='Cancel')

		if result == 'Add':
			expreName = pm.promptDialog(query=True, text=True)

		#print ('expr name:{name}'.format(name=expreName))


		if len(expreName)==0:
			return

		exprData = self.createCurrentFaceData(name=expreName, con_list_array=con_list_array)


		if parts_type==0: #face
			self.m_face_expression_list.append(exprData)
			print ('Added face expression data')

			#update face expr button
			self.updateFaceListUI()

		elif parts_type==1: #mouth
			self.m_mouth_expression_list.append(exprData)
			print ('Added mouth expression data')

			#update mouth expr button
			self.updateMouthListUI()





	def readExprFileJson(self, filepath='', parts_type=0):
		if parts_type not in (0, 1):
			raise ValueError('parts_type must be 0 (face) or 1 (mouth)')

		try:
			jsonDic = json_store.load(filepath, expected_type=dict)
			expressions = face_models.expressions_from_collection(jsonDic)
		except (IOError, OSError, TypeError, ValueError) as error:
			pm.warning('Expression data was not loaded: {0}'.format(error))
			return False

		# Replace the current UI state only after the complete file is valid.
		if parts_type == 0:
			self.clearAllFaceData()
			self.m_face_expression_list = expressions
			self.updateFaceListUI()
		else:
			self.clearAllMouthData()
			self.m_mouth_expression_list = expressions
			self.updateMouthListUI()
		return True



	def loadExprData(self, parts_type=0):
		"""Load Expression List"""

		filters = "*.json"
		file = pm.fileDialog2(cap='Load Expression',fileFilter=filters, okc='Load', fileMode=1)

		if file is not None:
			#read json file
			self.readExprFileJson(file[0],parts_type=parts_type)
		else:
			pm.warning('Failed load expression file.')




	def saveExprData(self, parts_type=0):
		"""Save Expression List"""
		if parts_type == 0:
			expressions = self.m_face_expression_list
		elif parts_type == 1:
			expressions = self.m_mouth_expression_list
		else:
			pm.warning('Expression part type is invalid.')
			return False

		if not expressions:
			pm.warning('Expression data is not exist!')
			return False

		try:
			out_dic = face_models.collection_from_expressions(expressions)
		except ValueError as error:
			pm.warning('Expression data is invalid: {0}'.format(error))
			return False


		filters = "*.json"
		file = pm.fileDialog2(cap='Save Expression List',fileFilter=filters, dialogStyle=2, okc='Save')
		print ('file : {name}'.format(name=file))


		if file is None:
			return False

		ext = file[0].rpartition('.')[-1].lower()
		if ext != 'json':
			pm.warning('File extension is invalid; select a JSON file.')
			return False

		try:
			json_store.dump(file[0], out_dic)
		except (IOError, OSError, TypeError, ValueError) as error:
			pm.warning('Expression data was not saved: {0}'.format(error))
			return False
		return True


	def clearAllFaceData(self):
		#clear data
		self.m_face_expression_list = []
		#delete all button
		self.deleteFaceExprButtonAll()


	def clearAllMouthData(self):
		#clear data
		self.m_mouth_expression_list = []
		#delete all butoon
		self.deleteMouthExprButtonAll()


	def delExprData(self, parts_type=0):

		result = pm.confirmDialog( title='Delete Expr List', message='Are you delete all expr data?', button=['Yes','No'], defaultButton='Yes', cancelButton='No', dismissString='No' )

		if result == 'Yes':
			if parts_type==0:
				self.clearAllFaceData()
			elif parts_type==1:
				self.clearAllMouthData()




	def changeExpressionCB(self, exprNum=-1, parts_type=0):
		"""Change Expression

		"""

		#print ('expr index:{idx}'.format(idx=exprNum))
		#print('index id:{idx}'.format(idx=id(exprNum)) )

		expr_list = []

		if parts_type==0:
			#face
			expr_list = self.m_face_expression_list
		elif parts_type==1:
			#moth
			expr_list = self.m_mouth_expression_list


		if exprNum==-1 or len(expr_list)==0:
			pm.warning('Not found specific expr number.')
			return

		expr = expr_list[exprNum]

		#print ('Set expr:{fname}'.format(fname=expr.m_name))

		for con in expr.m_controller_list:
			con_name = con.m_cntlr_name
			attr_list = con.m_attr_list

			#print ('name:{name} type:{ctype} attr size:{size}'.format(name=con_name, ctype=con_type, size=len(attr_list)))

			for attr in attr_list:
				pm.setAttr( (con_name+'.'+attr.m_name), attr.m_value)


	def openEyeCB(self, bias=0):
		"""Open Close Eye Rig

		"""
		pm.setAttr( (self.m_con_eye_opcl_l.ctlr+'.translateY'), bias)
		pm.setAttr( (self.m_con_eye_opcl_r.ctlr+'.translateY'), bias)


	def randFaceExprCB(self):


		index=0
		for conlist in self.m_all_con_lists:

			for con in conlist:

				#check quad controller
				if isinstance(con, DRgCnOp.quadDirectBox):

					dirList = con.getControllers()
					for dir in dirList:

						keyAttrList = pm.listAttr(dir.ctlr, keyable=True)
						for attr in keyAttrList:
							#value = pm.getAttr( dir.ctlr+'.'+attr )
							index = index+1
				else:

					keyAttrList = pm.listAttr(con.ctlr, keyable=True)

					for attr in keyAttrList:
						#value = pm.getAttr( con.ctlr+'.'+attr )
						index = index+1


		print ('con key attr count:{count}'.format(count=index))
