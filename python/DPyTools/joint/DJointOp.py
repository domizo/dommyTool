import pymel.core as pm
import maya.api.OpenMaya as om

'''
to run:
	>>> import DPyTools.joint.DJointOp as DJntOp


'''



class DJoint:

	m_joint = None

	def __init__(self, joint=None):
		self.m_joint = joint

	def name(self):
		return self.m_joint.name()

	def getFullPath(self):
		return self.m_joint.longName()

	def getParentJoint(self):
		return self.m_joint.getParent()

	def getChildNum(self):
		return len(self.getChildJoint())

	def getChildJoint(self):
		return pm.listRelatives(self.m_joint, children=True) or []

	def getPosition(self):
		return om.MVector(pm.xform(self.m_joint, q=True, ws=True, t=True))

	def getDir(self, childIndex=0):
		child = self.getChildJoint()[childIndex]
		v1 = self.getPosition()
		v2 = om.MVector(pm.xform(child, q=True, ws=True, t=True))
		return om.MVector(v2-v1).normal()

	def getLength(self):
		if self.getChildNum()>0:
			child = self.getChildJoint()[0]

			v1 = self.getPosition()
			v2 = om.MVector(pm.xform(child, q=True, ws=True, t=True))

			return om.MVector(v2-v1).length()
		else:
			return 0.0


