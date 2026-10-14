import pymel.core as pm
import maya.api.OpenMaya as om

import DPyTools.general.groupActions as DGrp
import DPyTools.node.DNodeOp as DNodeOp
import DPyTools.general.transformActions as DPTtrans


'''
to run:
	>>> import DPyTools.rigging.DRigControllerOp as DRgCnOp

'''


def setTemplateVis( item ):
	pm.setAttr( (item+'.overrideEnabled'), 1)
	pm.setAttr( (item+'.overrideDisplayType'), 2)

def createFaceOutLine(name='face_outLine1'):

	#crete curve
	face_crv = pm.curve( d=3, p=[(0, 23.346479, 0), (4.554257, 23.346479, 0), (9.800188, 22.001765, 0), (10.276632, 21.762912, 0), (12.885753, 17.094615, 0), (12.98401, 16.514428, 0), (13.135207, 7.017892, 0), (13.135207, 6.541486, 0), (13.336059, 6.506281, 0), (15.525855, 6.778144, 0), (15.47855, 6.496272, 0), (16.015169, 4.044989, 0), (16.015169, 3.51552, 0), (13.891308, -1.754623, 0), (13.549242, -1.884716, 0), (12.44178, -1.832276, 0), (12.36902, -1.890139, 0), (11.774562, -8.977726, 0), (11.533302, -9.595056, 0), (3.173743, -14.820311, 0), (2.582897, -15.057129, 0), (-0.0227551, -15.246851, 0), (-2.691876, -15.049053, 0), (-3.291904, -14.728864, 0), (-11.541286, -9.574628, 0), (-11.774562, -8.977726, 0), (-12.36902, -1.890139, 0), (-12.44178, -1.832276, 0), (-13.549242, -1.884716, 0), (-13.891308, -1.754623, 0), (-16.015169, 3.51552, 0), (-16.015169, 4.044989, 0), (-15.47855, 6.496272, 0), (-15.525855, 6.778144, 0), (-13.336059, 6.506281, 0), (-13.135207, 6.541486, 0), (-13.135207, 7.017892, 0), (-12.98401, 16.514428, 0), (-12.885753, 17.094615, 0), (-10.276632, 21.762912, 0), (-9.800188, 22.001765, 0), (-4.554257, 23.346479, 0), (0, 23.346479, 0)],
						k=[0, 0, 0, 1, 1.229474, 2, 3, 4, 5, 6, 6.53034, 7, 8, 9, 10, 11, 12, 13, 13.846895, 14, 15, 15.017466, 16, 17, 17.066182, 18, 18.153105, 19, 20, 21, 22, 23, 24, 25, 25.46966, 26, 27, 28, 29, 30, 30.770526, 31, 32, 32, 32],
						name=name )
	setTemplateVis(face_crv)

	return face_crv

def createFaceSideOutline(name="face_side_outline1"):

	#create curve
	face_side_line_crv = pm.curve( d=3, p=[(43.4609, 22.834508, 0),(38.521553, 22.834508, 0), (31.678363, 18.986189, 0), (31.405472, 18.905391, 0), (28.059632, 7.518401, 0), (28.017621, 7.460007, 0), (30.074833, 5.365884, 0), (29.869876, 4.86685, 0), (29.127613, 3.503431, 0), (26.273341, -1.402344, 0), (26.254459, -1.700968, 0), (26.750985, -2.399433, 0), (26.884194, -2.418314, 0), (29.417756, -2.683401, 0), (29.608686, -2.948153, 0), (29.146929, -3.942945, 0), (28.356365, -5.212738, 0), (28.309227, -5.307716, 0), (29.273032, -6.322213, 0), (29.767841, -6.507876, 0), (29.91712, -6.59564, 0), (29.580805, -8.134133, 0), (29.654282, -8.359709, 0), (31.348603, -8.539206, 0), (31.495803, -8.716862, 0), (31.331093, -12.428179, 0), (31.384369, -12.569674, 0), (32.701657, -13.553694, 0), (32.911498, -13.563517, 0), (35.923163, -13.551414, 0), (36.113861, -13.367953, 0), (48.694737, -5.979933, 0)],
						k=[0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 29],
						name=name )
	setTemplateVis(face_side_line_crv)

	return face_side_line_crv


class CnontrollerBase():
	ctlr = None
	ctlr_root = None
	m_con_size = 1.0
	name = ''

	def __init__(self, name='cntlr', cntlr_node=None):

		#reconstruct init
		#base init
		if cntlr_node is None:
			self.name = name+'_root'
			self.ctlr = None
			self.ctlr_root =  DGrp.createEmpty(name=self.name)
		else:

			self.ctlr = cntlr_node
			parents = pm.listRelatives( self.ctlr, allParents=True )
			self.ctlr_root = parents[0]
			self.name = str(self.ctlr_root)


	def setControllerVisible(self, item):
		pm.setAttr( (item+'.overrideEnabled'), 1)
		pm.setAttr( (item+'.overrideColor'), 14)



	def setKeepAttr(self, keepAttr=None):
		if keepAttr is None:
			keepAttr = []
		if self.ctlr is None:
			return

		#hide attribute
		all_attr = pm.listAttr(self.ctlr, keyable=True)
		for attr in all_attr:
			if attr not in keepAttr:
				pm.setAttr( (str(self.ctlr)+"."+attr.encode()), keyable=False, channelBox=False)

	def setNewAttr(self, newAttr=None):
		if newAttr is None:
			newAttr = []
		if self.ctlr is None:
			return

		#add attribute
		for attr in newAttr:
			pm.addAttr(self.ctlr,**attr)

	def addConAttr(self, typename='none'):
		#addAttrList = [{'longName':'cntlrType', 'attributeType':'string', 'keyable':True}]
		pm.addAttr(self.ctlr_root, longName='cntlrType', dataType='string', keyable=True )
		pm.setAttr((self.ctlr_root+'.cntlrType'), typename)



class ArrowSlider(CnontrollerBase):

	def __init__(self, length=1.0, name='arrowCtlr1', cntlr_node=None ):
		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:
			#init arrow
			length = 3.0
			side = 0.5
			top = length-1.0

			self.ctlr = pm.curve( d=1, p=[(0,0,0), (0,length,0), (side,top,0), (-side,top,0), (0,length,0)], k=[0,1,2,3,4], n=name )
			self.setControllerVisible(self.ctlr)
			#pm.setAttr( (self.ctlr+'.overrideEnabled'), 1)
			#pm.setAttr( (self.ctlr+'.overrideColor'), 14)

			pm.setAttr( (self.ctlr+'.translateX'), lock=True )
			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )
			pm.transformLimits( self.ctlr, ty=(0, length) )
			pm.transformLimits( self.ctlr, ety=(True, True) )

			#clean attribute
			self.setKeepAttr(['translateY'])

			#group
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('arrow')




class circleSlider(CnontrollerBase):

	sliderLine = None
	#circle_size = 0.3


	def __init__(self, length=1.0, name='sliderCtlr1', ratio=0.1, cntlr_node=None ):
		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:
			#init circle
			self.m_con_size = length
			self.ctlr = pm.circle(c=(0, 0, 0), nr=(0, 0, 1), sw=360, r=float(length)/10.0, d=1, ut=0, tol=0.01, s=3, ch=0, n=name)[0]
			self.setControllerVisible(self.ctlr)
			#pm.setAttr( (self.ctlr+'.overrideEnabled'), 1)
			#pm.setAttr( (self.ctlr+'.overrideColor'), 14)

			pm.setAttr( (self.ctlr+'.translateX'), lock=True )
			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )
			pm.transformLimits( self.ctlr, ty=(0, length) )
			pm.transformLimits( self.ctlr, ety=(True, True) )

			#clean attribute
			self.setKeepAttr(['translateY'])

			#init slider line
			self.sliderLine = pm.curve( d=1, p=[(0,0,0), (0,length,0)], k=[0,1] )

			pm.setAttr( (self.sliderLine+'.overrideEnabled'), 1)
			pm.setAttr( (self.sliderLine+'.overrideDisplayType'), 2)

			#group
			#self.ctlr_root =  DGrp.createEmpty(name=name)
			pm.parent(self.sliderLine, self.ctlr_root)
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('slider')


class splitSlider(CnontrollerBase):

	sliderLine = None
	#circle_size = 0.3

	def __init__(self, length=2.0, name='sliderCtlr1', ratio=0.1, cntlr_node=None ):
		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:
			#init circle
			self.m_con_size = length
			self.ctlr = pm.circle(c=(0, 0, 0), nr=(0, 0, 1), sw=360, r=float(length)/10.0, d=1, ut=0, tol=0.01, s=4, ch=0, n=name)[0]
			self.setControllerVisible(self.ctlr)
			#pm.setAttr( (self.ctlr+'.overrideEnabled'), 1)
			#pm.setAttr( (self.ctlr+'.overrideColor'), 14)

			pm.setAttr( (self.ctlr+'.translateX'), lock=True )
			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )
			pm.transformLimits( self.ctlr, ty=(-length*.5, length*.5) )
			pm.transformLimits( self.ctlr, ety=(True, True) )

			#clean attribute
			self.setKeepAttr(['translateY'])

			#init slider line
			self.sliderLine = pm.curve( d=1, p=[(0,-length*.5,0), (0,length*.5,0)], k=[0,1] )
			setTemplateVis(self.sliderLine)

			self.splitLine = pm.curve( d=1, p=[(float(length)/-20.0,0,0), (float(length)/20.0,0,0)], k=[0,1] )
			setTemplateVis(self.splitLine)


			#group
			#self.ctlr_root =  DGrp.createEmpty(name=name)
			pm.parent(self.sliderLine, self.ctlr_root)
			pm.parent(self.splitLine, self.ctlr_root)
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('split')


class squareBoxController(CnontrollerBase):

	boxLines = None

	def __init__(self, size=2.0, name='squareCtlr1', ratio=0.1, cntlr_node=None):
		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:
			halfS = size*.5

			#init circle
			self.ctlr = pm.circle(c=(0, 0, 0), nr=(0, 0, 1), sw=360, r=(size*ratio), d=3, ut=0, tol=0.01, s=8, ch=0, n=name)[0]
			self.setControllerVisible(self.ctlr)


			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )


			self.setKeepAttr(['translateX','translateY'])

			pm.transformLimits( self.ctlr, tx=(-halfS, halfS), ty=(-halfS, halfS) )
			pm.transformLimits( self.ctlr, etx=(True, True), ety=(True, True) )

			#init box line
			topCrv = pm.curve( d=1, p=[(-halfS,halfS,0), (halfS,halfS,0)], k=[0,1] )
			leftCrv = pm.curve( d=1, p=[(-halfS,halfS,0), (-halfS,-halfS,0)], k=[0,1] )
			rightCrv = pm.curve( d=1, p=[(halfS,halfS,0), (halfS,-halfS,0)], k=[0,1] )
			bottomCrv = pm.curve( d=1, p=[(-halfS,-halfS,0), (halfS,-halfS,0)], k=[0,1] )

			self.boxLines = DGrp.createEmpty(name=(self.name+'BoxLines'))
			pm.parent(topCrv, self.boxLines)
			pm.parent(leftCrv, self.boxLines)
			pm.parent(rightCrv, self.boxLines)
			pm.parent(bottomCrv, self.boxLines)
			setTemplateVis(self.boxLines)


			#group
			pm.parent(self.boxLines, self.ctlr_root)
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('box')

	def flipSpace(self):

		objScale = pm.xform(self.ctlr_root, q=True, relative=True, s=True)
		pm.scale( self.ctlr_root, objScale[0]*-1.0, objScale[1], objScale[2] )


	def addText(self, text='text', size=1.0,  padding=0.0 ):

		textObj = pm.textCurves( f='Helvetica', t=text )


		DPTtrans.setTransformToObjectZero( srcObj=textObj[0], targetObj=self.ctlr_root, isTrans=True, isRot=True, isScale=True, isUnParent=True)
		pm.parent(textObj, self.ctlr_root)
		pm.scale( textObj, size, size, 1 )
		pm.setAttr((textObj[0]+'.translate'), (-1,(1.0+padding),0))
		setTemplateVis(textObj[0])





class simpleCircleController(CnontrollerBase):

	def __init__(self, size=3.0, name='circleCtlr1', aspect=0.1, pos=(0.0, 0.0), scale=(1.0, 1.0), cntlr_node=None ):
		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:
			#create controller
			self.ctlr = pm.circle( nr=(0, 0, 1), c=(pos[0], pos[1], 0), n=name )[0]
			self.setControllerVisible(self.ctlr)
			pm.setAttr( (self.ctlr+'.translateX'), lock=True )
			pm.setAttr( (self.ctlr+'.translateY'), lock=True )
			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )

			#clean attribute
			self.setKeepAttr([])


			#group
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('simple')



class quadDirectBox(CnontrollerBase):

	dir_ul = None
	dir_ur = None
	dir_dl = None
	dir_dr = None

	def __init__(self, size=3.0, name='quadBox1', cntlr_node=None):

		#init super
		CnontrollerBase.__init__(self, name, cntlr_node)

		if cntlr_node is None:

			halfS = size*.5

			v1 = om.MVector(-halfS,halfS,0)
			v2 = om.MVector(halfS,-halfS,0)
			angleLen = (v2-v1).length()
			dirLen = angleLen*.5

			dir_offset = float(dirLen)*.3

			#init quad dir
			self.dir_ul = circleSlider(length=dirLen-dir_offset, name=name+('dir1'))
			self.dir_ur = circleSlider(length=dirLen-dir_offset, name=name+('dir2'))
			self.dir_dl = circleSlider(length=dirLen-dir_offset, name=name+('dir3'))
			self.dir_dr = circleSlider(length=dirLen-dir_offset, name=name+('dir4'))

			pm.move( 0, dir_offset, 0, self.dir_ul.ctlr_root )
			pm.move( 0, dir_offset, 0, self.dir_ur.ctlr_root )
			pm.move( 0, dir_offset, 0, self.dir_dl.ctlr_root )
			pm.move( 0, dir_offset, 0, self.dir_dr.ctlr_root )

			pm.rotate( self.dir_ul.ctlr_root, [0,0,-45], pivot=(0, 0, 0), ws=True )
			pm.rotate( self.dir_ur.ctlr_root, [0,0,45], pivot=(0, 0, 0), ws=True )
			pm.rotate( self.dir_dl.ctlr_root, [0,0,-135], pivot=(0, 0, 0), ws=True )
			pm.rotate( self.dir_dr.ctlr_root, [0,0,135], pivot=(0, 0, 0), ws=True )

			self.ctlr =  DGrp.createEmpty(name=name)

			pm.parent(self.dir_ul.ctlr_root, self.ctlr)
			pm.parent(self.dir_ur.ctlr_root, self.ctlr)
			pm.parent(self.dir_dl.ctlr_root, self.ctlr)
			pm.parent(self.dir_dr.ctlr_root, self.ctlr)

			pm.parent(self.ctlr, self.ctlr_root)


			#init box line
			topCrv = pm.curve( d=1, p=[(-halfS,halfS,0), (halfS,halfS,0)], k=[0,1] )
			leftCrv = pm.curve( d=1, p=[(-halfS,halfS,0), (-halfS,-halfS,0)], k=[0,1] )
			rightCrv = pm.curve( d=1, p=[(halfS,halfS,0), (halfS,-halfS,0)], k=[0,1] )
			bottomCrv = pm.curve( d=1, p=[(-halfS,-halfS,0), (halfS,-halfS,0)], k=[0,1] )

			self.boxLines =  DGrp.createEmpty(name=(self.name+'BoxLines'))
			pm.parent(topCrv, self.boxLines)
			pm.parent(leftCrv, self.boxLines)
			pm.parent(rightCrv, self.boxLines)
			pm.parent(bottomCrv, self.boxLines)
			setTemplateVis(self.boxLines)

			#group
			pm.parent(self.boxLines, self.ctlr_root)

			#add type string
			self.addConAttr('quad')

		else:

			#set dirs
			childlist = pm.listRelatives(cntlr_node, c=True)

			for c in childlist:
				#print ('check curve : {name} '.format(name=c))
				crvlist = pm.listRelatives(c, c=True)
				#print ('child list : {crvlist} '.format(crvlist=crvlist))
				for crv in crvlist:
					#print ('crv : {crv} '.format(crv=crv))
					node = DNodeOp.getItem(str(crv))

					if "dir1" in str(crv):
						self.dir_ul = circleSlider(cntlr_node=node)
						#print ('dir1 : {name} '.format(name=self.dir_ul))
					elif "dir2" in str(crv):
						self.dir_ur = circleSlider(cntlr_node=node)
						#print ('dir2 : {name} '.format(name=self.dir_ur))
					elif "dir3" in str(crv):
						self.dir_dl = circleSlider(cntlr_node=node)
						#print ('dir3 : {name} '.format(name=self.dir_dl))
					elif "dir4" in str(crv):
						self.dir_dr = circleSlider(cntlr_node=node)
						#print ('dir4 : {name} '.format(name=self.dir_dr))


	def addText(self, text='text', size=1.0,  padding=0.0 ):

		textObj = pm.textCurves( f='Helvetica', t=text )


		DPTtrans.setTransformToObjectZero( srcObj=textObj[0], targetObj=self.ctlr_root, isTrans=True, isRot=True, isScale=True, isUnParent=True)
		pm.parent(textObj, self.ctlr_root)
		pm.scale( textObj, size, size, 1 )
		pm.setAttr((textObj[0]+'.translate'), (-1.5,(1.5+padding),0))
		setTemplateVis(textObj[0])

	def getControllers(self):

		return [self.dir_ul, self.dir_ur, self.dir_dl, self.dir_dr]


class jawController(CnontrollerBase):

	def __init__(self, size=3.0, name='jawCtlr1', aspect=0.1, pos=(0.0, 0.0), scale=(1.0, 1.0), cntlr_node=None ):
		#init super
		CnontrollerBase.__init__(self,name, cntlr_node)

		if cntlr_node is None:
			#create controller
			maxWidth = 13
			nWidth = maxWidth-2
			halfWidth = 6
			bottom = -12
			midHeight = -8
			top = -4

			self.ctlr = pm.curve( d=1, p=[(-maxWidth,top,0), (-nWidth,midHeight,0), (-halfWidth,bottom,0), (0,bottom,0), (halfWidth,bottom,0), (nWidth,midHeight,0), (maxWidth,top,0)], k=[0,1,2,3,4,5,6], n=name )
			self.setControllerVisible(self.ctlr)

			pm.setAttr( (self.ctlr+'.translateX'), lock=True )
			pm.setAttr( (self.ctlr+'.translateY'), lock=True )
			pm.setAttr( (self.ctlr+'.translateZ'), lock=True )

			self.ctlr.setScalePivot([0,-12,0])
			self.ctlr.setRotatePivot([0,-12,0])

			#clean attribute
			self.setKeepAttr([])


			#group
			pm.parent(self.ctlr, self.ctlr_root)

			#add type string
			self.addConAttr('jaw')
