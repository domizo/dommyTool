import pymel.core as pm

import DPyTools.general.groupActions as DPTgrp
import DPyTools.general.attributeActions as DPTattr
import DPyTools.general.nodeActions as DPTnode

import DPyTools.rigging.rigCommonActions as DPTrigCAct


'''
to run:
	>>> import DPyTools.rigging.humanHeadRig as DPTHHead
	>>> DPTHHead.human_head()

'''

class human_head(object):

	rigTop = None
	groupTop = None


	#external
	root_ctrl = None
	spine_end_jnt = None

	#internal
	headSpaceCtrl = None
	head_ctrl = None




	def __init__(self, neck='neck_jnt', head='head_jnt'):
		self.neck = neck
		self.head = head


	def createNeckFK(self):
		#create top
		nexk_FkCtrl_Grp_Top = DPTgrp.createEmptyWithAlign('neck_fk_ctrl_Grp', self.neck, True, True, False, True)
		pm.parent(nexk_FkCtrl_Grp_Top, self.groupTop)


		#create neck_ctrl
		self.neck_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name='neck_ctrl', size=10.0, alignObj=self.neck)
		neck_ctrl_frzGrp, neck_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.neck_ctrl, None, True, True, False )

		neck_ctrl_grp_Top = DPTgrp.createEmptyWithAlign('neck_ctrl_frzGrp_Top', self.neck, True, True, False, True)
		pm.parent(neck_ctrl_frzGrp, neck_ctrl_grp_Top)
		pm.parent(neck_ctrl_grp_Top, nexk_FkCtrl_Grp_Top)

		DPTattr.hideAttributes(self.neck_ctrl, ['visibility'])

		#create align grp
		neckAlignFalse_Grp = DPTgrp.createEmptyWithAlign('neckAlignFalse_Grp', self.neck, True, True, False, True)
		pm.parent(neckAlignFalse_Grp, nexk_FkCtrl_Grp_Top)

		neckAutoAlign_Grp = DPTgrp.createEmptyWithAlign('neckAutoAlign_Grp', self.neck, True, True, False, True)
		neckAlignTrue_Grp = DPTgrp.createEmptyWithAlign('neckAlignTrue_Grp', self.neck, True, True, False, True)
		pm.parent(neckAlignTrue_Grp, neckAutoAlign_Grp)
		pm.parent(neckAutoAlign_Grp, nexk_FkCtrl_Grp_Top)

		#add attribute
		DPTattr.createAttributeFromDic(self.neck_ctrl,
				[{'ln':'align','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'useRootSpace','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}
				 ])


		#constraints
		pm.pointConstraint( neckAlignFalse_Grp, neckAlignTrue_Grp )
		pm.parentConstraint( self.root_ctrl, neckAutoAlign_Grp, maintainOffset=True )

		Cons = pm.parentConstraint( neckAlignTrue_Grp, neckAlignFalse_Grp, neck_ctrl_frzGrp, maintainOffset=True )

		trueAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='True', kwargs={'ud':True})
		falseAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='False', kwargs={'ud':True})


		pm.connectAttr(self.neck_ctrl+'.align', Cons+'.'+trueAttr )

		align_reverse = DPTnode.createNodeWithParam(node_type='reverse', name='neck_align_reverse', param={})
		pm.connectAttr( self.neck_ctrl+'.align', align_reverse+'.inputX' )
		pm.connectAttr( self.neck_ctrl+'.useRootSpace', align_reverse+'.inputY' )
		pm.connectAttr( align_reverse+'.outputX', Cons+'.'+falseAttr )


		Cons = pm.parentConstraint( self.root_ctrl, self.spine_end_jnt, neckAlignFalse_Grp, maintainOffset=True )

		rootAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='root', kwargs={'ud':True})
		spineAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='end', kwargs={'ud':True})

		pm.connectAttr(self.neck_ctrl+'.useRootSpace', Cons+'.'+rootAttr )
		pm.connectAttr( align_reverse+'.outputY', Cons+'.'+spineAttr )



	def createHeadFK(self):
		#create top
		head_FkCtrl_Grp_Top = DPTgrp.createEmptyWithAlign('head_fk_ctrl_Grp', self.head, True, True, False, True)
		pm.parent(head_FkCtrl_Grp_Top, self.groupTop)

		#create neck_ctrl
		self.head_ctrl = DPTrigCAct.createRigControllerWithAlign(ctlObj=None, name='head_ctrl',size=10.0,alignObj=self.head)
		head_ctrl_frzGrp, head_ctrl_zeroFrzGrp = DPTrigCAct.createFreezeRigWithAlign(self.head_ctrl, None, True, True, False )

		pm.parent(head_ctrl_frzGrp, head_FkCtrl_Grp_Top)

		DPTattr.hideAttributes(self.head_ctrl, ['scaleX','scaleY','scaleZ','visibility'])

		#add attribute
		DPTattr.createAttributeFromDic(self.head_ctrl,
				[{'ln':'align','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0},
				 {'ln':'useRootSpace','k':True,'at':'float','hnv':True,'hxv':True,'min':0.0,'max':1.0,'dv':0.0}
				 ])

		#create align grp
		headAlignFalse_Grp = DPTgrp.createEmptyWithAlign('headAlignFalse_Grp', self.neck, True, True, False, True)
		pm.parent(headAlignFalse_Grp, head_FkCtrl_Grp_Top)

		headAutoAlign_Grp = DPTgrp.createEmptyWithAlign('headAutoAlign_Grp', self.neck, True, True, False, True)
		headAlignTrue_Grp = DPTgrp.createEmptyWithAlign('headAlignTrue_Grp', self.neck, True, True, False, True)
		pm.parent(headAlignTrue_Grp, headAutoAlign_Grp)
		pm.parent(headAutoAlign_Grp, head_FkCtrl_Grp_Top)


		#constraints
		pm.pointConstraint( headAlignFalse_Grp, headAlignTrue_Grp )
		pm.parentConstraint( self.root_ctrl, headAutoAlign_Grp, maintainOffset=True )


		Cons = pm.parentConstraint( headAlignTrue_Grp, headAlignFalse_Grp, head_ctrl_frzGrp, maintainOffset=True )

		trueAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='True', kwargs={'ud':True})
		falseAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='False', kwargs={'ud':True})


		pm.connectAttr(self.head_ctrl+'.align', Cons+'.'+trueAttr )

		align_reverse = DPTnode.createNodeWithParam(node_type='reverse', name='head_align_reverse', param={})
		pm.connectAttr( self.head_ctrl+'.align', align_reverse+'.inputX' )
		pm.connectAttr( self.head_ctrl+'.useRootSpace', align_reverse+'.inputY' )
		pm.connectAttr( align_reverse+'.outputX', Cons+'.'+falseAttr )


		Cons = pm.parentConstraint( self.root_ctrl, self.spine_end_jnt, headAlignFalse_Grp, maintainOffset=True )

		rootAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='root', kwargs={'ud':True})
		spineAttr = DPTattr.getMatchNameAttribute(node=Cons, matchStr='end', kwargs={'ud':True})

		pm.connectAttr(self.head_ctrl+'.useRootSpace', Cons+'.'+rootAttr )
		pm.connectAttr( align_reverse+'.outputY', Cons+'.'+spineAttr )




	def createConnectJont(self):
		#connect neck
		pm.parentConstraint( self.neck_ctrl, self.neck, maintainOffset=True )

		pm.connectAttr(self.neck_ctrl+'.scale', self.neck+'.scale' )

		#connect head
		pm.parentConstraint( self.head_ctrl, self.head, maintainOffset=True )
		pm.connectAttr(self.head_ctrl+'.scale', self.head+'.scale' )


	def setup_rig(self):
		#Create Head space grp
		self.headSpaceCtrl = DPTgrp.createEmptyWithAlign(name='headCtrl_space_switch_grp', alignObj='head_jnt', isTrans=True, isRot=True, isScale=True, isUnParent=True)
		DPTattr.connectAttrToList(self.root_ctrl+'.masterScale',[self.headSpaceCtrl+'.scaleX',self.headSpaceCtrl+'.scaleY',self.headSpaceCtrl+'.scaleZ',])
		pm.parent(self.headSpaceCtrl,self.rigTop)

		#create rig top
		self.groupTop = DPTgrp.createEmpty('head_rig_grp',self.rigTop)
		DPTattr.connectAttrToList(self.root_ctrl+'.masterScale',[self.groupTop+'.scaleX',self.groupTop+'.scaleY',self.groupTop+'.scaleZ',])



		self.createNeckFK()
		self.createHeadFK()
		self.createConnectJont()









