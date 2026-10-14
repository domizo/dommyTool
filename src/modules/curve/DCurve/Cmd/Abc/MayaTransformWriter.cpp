// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaTransformWriter.cpp

//
//  MayaTransformWriter.cpp
//  dommyTool
//
//  Created by yuzo on 3/18/17.
//
//

#include "MayaTransformWriter.h"

#include "DDataExportUtil.h"
#include "common/utility/DGenerationLimits.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
double finiteTransformValue(double value)
{
    if (!std::isfinite(value)) throw std::invalid_argument("Non-finite Alembic transform value");
    return value;
}

double transformDouble(const MPlug& plug)
{
    return finiteTransformValue(ddata_export_util::readExportValue<double>(plug));
}

MTransformationMatrix::RotationOrder transformRotationOrder(const MFnTransform& node)
{
    MStatus status;
    const auto order = node.rotationOrder(&status);
    ddata_export_util::requireExportQuery(status, "Unable to read Alembic rotation order");
    unsigned int x, y, z;
    if (!ddata_export_util::getRotOrder(order, x, y, z))
        throw std::invalid_argument("Invalid Alembic rotation order");
    return order;
}

bool transformIsJoint(const MDagPath& path)
{
    MStatus status;
    const bool joint = path.hasFn(MFn::kJoint, &status);
    ddata_export_util::requireExportQuery(status, "Unable to identify an Alembic transform path");
    return joint;
}
} // namespace

void addTranslate(const MFnDependencyNode & iTrans,
				  MString parentName, MString xName, MString yName, MString zName,
				  Alembic::Util::uint8_t iHint, bool inverse, bool forceStatic,
				  bool forceAnimated, Alembic::AbcGeom::XformSample & oSample,
				  std::vector < AnimChan > & oAnimChanList)
{
	Alembic::AbcGeom::XformOp op(Alembic::AbcGeom::kTranslateOperation, iHint);
	
	MPlug xPlug = ddata_export_util::requiredExportPlug(iTrans, xName);
	int xSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
            xSamp = ddata_export_util::getSampledType(xPlug);
		else
			xSamp = 1;
	}
	double xVal = transformDouble(xPlug);
	
	MPlug yPlug = ddata_export_util::requiredExportPlug(iTrans, yName);
	int ySamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			ySamp = ddata_export_util::getSampledType(yPlug);
		else
			ySamp = 1;
	}
	double yVal = transformDouble(yPlug);
	
	MPlug zPlug = ddata_export_util::requiredExportPlug(iTrans, zName);
	int zSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			zSamp = ddata_export_util::getSampledType(zPlug);
		else
			zSamp = 1;
	}
	double zVal = transformDouble(zPlug);
	
	// this is to handle the case where there is a connection to the parent
	// plug but not to the child plugs, if the connection is there then all
	// of the children are considered animated
	MPlug parentPlug = ddata_export_util::requiredExportPlug(iTrans, parentName);
	int parentSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			parentSamp = ddata_export_util::getSampledType(parentPlug);
		else
			parentSamp = 1;
	}
	
	if (parentSamp != 0)
	{
		xSamp = 1;
		ySamp = 1;
		zSamp = 1;
	}
	
	// something is sampled or not identity, add it to the stack
	if (xSamp != 0 || ySamp != 0 || zSamp != 0 || xVal != 0.0 || yVal != 0.0 ||
		zVal != 0.0)
	{
		if (inverse)
		{
			xVal = -xVal;
			yVal = -yVal;
			zVal = -zVal;
		}
		
		op.setChannelValue(0, xVal);
		op.setChannelValue(1, yVal);
		op.setChannelValue(2, zVal);
		
		if (xSamp != 0)
		{
			AnimChan chan;
			chan.plug = xPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 0;
			oAnimChanList.push_back(chan);
		}
		
		if (ySamp != 0)
		{
			AnimChan chan;
			chan.plug = yPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 1;
			oAnimChanList.push_back(chan);
		}
		
		if (zSamp != 0)
		{
			AnimChan chan;
			chan.plug = zPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 2;
			oAnimChanList.push_back(chan);
		}
		
		oSample.addOp(op);
	}
}

// names need to be passed in x,y,z order, iOrder is the order to
// use these indices
void addRotate(const MFnDependencyNode & iTrans,
			   MString parentName, const MString* iNames, const unsigned int* iOrder,
			   Alembic::Util::uint8_t iHint, bool forceStatic, bool forceAnimated,
			   Alembic::AbcGeom::XformSample & oSample,
			   std::vector < AnimChan > & oAnimChanList,
			   size_t oOpIndex[3])
{
	// for each possible rotation axis
	static const Alembic::AbcGeom::XformOperationType rots[3] = {
		Alembic::AbcGeom::kRotateXOperation,
		Alembic::AbcGeom::kRotateYOperation,
		Alembic::AbcGeom::kRotateZOperation
	};
	
	// this is to handle the case where there is a connection to the parent
	// plug but not to the child plugs, if the connection is there then all
	// of the children are considered animated
	MPlug parentPlug = ddata_export_util::requiredExportPlug(iTrans, parentName);
	int parentSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			parentSamp = ddata_export_util::getSampledType(parentPlug);
		else
			parentSamp = 1;
	}
	
	// whether we are using the XYZ rotation order
	bool isXYZ = ((iOrder[0] == 0) && (iOrder[1] == 1) && (iOrder[2] == 2));
	
	// add them in backwards since we are dealing with a stack
	int i = 2;
	for (; i > -1; i--)
	{
		unsigned int index = iOrder[i];
        if (index >= 3U) throw std::invalid_argument("Invalid Alembic rotation axis");
		MPlug plug = ddata_export_util::requiredExportPlug(iTrans, iNames[index]);
		int samp = 0;
		if (!forceStatic)
		{
			if (!forceAnimated)
				samp = ddata_export_util::getSampledType(plug);
			else
				samp = 1;
			
			if (samp == 0)
				samp = parentSamp;
		}
		
		double plugVal = transformDouble(plug);
		
		
		Alembic::AbcGeom::XformOp op(rots[index], iHint);
		op.setChannelValue(0, finiteTransformValue(Alembic::AbcGeom::RadiansToDegrees(plugVal)));
		
		// the sampled case
		if (samp != 0)
		{
			AnimChan chan;
			chan.plug = plug;
			chan.scale = Alembic::AbcGeom::RadiansToDegrees(1.0);
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 0;
			oAnimChanList.push_back(chan);
		}
		// non sampled, XYZ axis and the angle is 0, do not add to the stack
		else if (isXYZ && plugVal == 0.0)
			continue;
		
		oOpIndex[index] = oSample.addOp(op);
	}
}

// the test on whether or not to add it is very similiar to addTranslate
// but the operation it creates is very different
void addShear(const MFnDependencyNode & iTrans, bool forceStatic,
			  Alembic::AbcGeom::XformSample & oSample,
			  std::vector < AnimChan > & oAnimChanList)
{
	Alembic::AbcGeom::XformOp op(Alembic::AbcGeom::kMatrixOperation,
								 Alembic::AbcGeom::kMayaShearHint);
	
	MString str = "shearXY";
	MPlug xyPlug = ddata_export_util::requiredExportPlug(iTrans, str);
	int xySamp = 0;
	if (!forceStatic)
	{
		xySamp = ddata_export_util::getSampledType(xyPlug);
	}
	double xyVal = transformDouble(xyPlug);
	
	str = "shearXZ";
	MPlug xzPlug = ddata_export_util::requiredExportPlug(iTrans, str);
	int xzSamp = 0;
	if (!forceStatic)
	{
		xzSamp = ddata_export_util::getSampledType(xzPlug);
	}
	double xzVal = transformDouble(xzPlug);
	
	str = "shearYZ";
	MPlug yzPlug = ddata_export_util::requiredExportPlug(iTrans, str);
	int yzSamp = 0;
	if (!forceStatic)
	{
		yzSamp = ddata_export_util::getSampledType(yzPlug);
	}
	double yzVal = transformDouble(yzPlug);
	
	// this is to handle the case where there is a connection to the parent
	// plug but not to the child plugs, if the connection is there then all
	// of the children are considered animated
	str = "shear";
	MPlug parentPlug = ddata_export_util::requiredExportPlug(iTrans, str);
	if (!forceStatic && ddata_export_util::getSampledType(parentPlug) != 0)
	{
		xySamp = 1;
		xzSamp = 1;
		yzSamp = 1;
	}
	
	// something is sampled or not identity, add it to the stack
	if (xySamp != 0 || xzSamp != 0 || yzSamp != 0 ||
		xyVal != 0.0 || xzVal != 0.0 || yzVal != 0.0)
	{
		Alembic::Abc::M44d m;
		m.makeIdentity();
		op.setMatrix(m);
		op.setChannelValue(4, xyVal);
		op.setChannelValue(8, xzVal);
		op.setChannelValue(9, yzVal);
		
		if (xySamp != 0)
		{
			AnimChan chan;
			chan.plug = xyPlug;
			chan.scale = 1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 4;
			oAnimChanList.push_back(chan);
		}
		
		if (xzSamp != 0)
		{
			AnimChan chan;
			chan.plug = xzPlug;
			chan.scale = 1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 8;
			oAnimChanList.push_back(chan);
		}
		
		if (yzSamp != 0)
		{
			AnimChan chan;
			chan.plug = yzPlug;
			chan.scale = 1.0;
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 9;
			oAnimChanList.push_back(chan);
		}
		
		oSample.addOp(op);
	}
}

// this test is very similiar to addTranslate, except that it doesn't add it
// to the stack if x,y, and z are 1.0
void addScale(const MFnDependencyNode & iTrans,
			  MString parentName, MString xName, MString yName, MString zName, bool inverse,
			  bool forceStatic, bool forceAnimated, Alembic::AbcGeom::XformSample & oSample,
			  std::vector < AnimChan > & oAnimChanList)
{
	
	Alembic::AbcGeom::XformOp op(Alembic::AbcGeom::kScaleOperation,
								 Alembic::AbcGeom::kScaleHint);
	
	MPlug xPlug = ddata_export_util::requiredExportPlug(iTrans, xName);
	int xSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			xSamp = ddata_export_util::getSampledType(xPlug);
		else
			xSamp = 1;
	}
	double xVal = transformDouble(xPlug);
	
	MPlug yPlug = ddata_export_util::requiredExportPlug(iTrans, yName);
	int ySamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			ySamp = ddata_export_util::getSampledType(yPlug);
		else
			ySamp = 1;
	}
	double yVal = transformDouble(yPlug);
	
	MPlug zPlug = ddata_export_util::requiredExportPlug(iTrans, zName);
	int zSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			zSamp = ddata_export_util::getSampledType(zPlug);
		else
			zSamp = 1;
	}
	double zVal = transformDouble(zPlug);
	
	// this is to handle the case where there is a connection to the parent
	// plug but not to the child plugs, if the connection is there then all
	// of the children are considered animated
	MPlug parentPlug = ddata_export_util::requiredExportPlug(iTrans, parentName);
	int parentSamp = 0;
	if (!forceStatic)
	{
		if (!forceAnimated)
			parentSamp = ddata_export_util::getSampledType(parentPlug);
		else
			parentSamp = 1;
	}
	
	if (parentSamp != 0)
	{
		xSamp = 1;
		ySamp = 1;
		zSamp = 1;
	}
	
	// something is sampled or not identity, add it to the stack
	if (xSamp != 0 || ySamp != 0 || zSamp != 0 || xVal != 1.0 || yVal != 1.0 ||
		zVal != 1.0)
	{
		if (inverse)
		{
			xVal = ddata_export_util::inverseScale(xVal);
			yVal = ddata_export_util::inverseScale(yVal);
			zVal = ddata_export_util::inverseScale(zVal);
		}
		
		op.setChannelValue(0, xVal);
		op.setChannelValue(1, yVal);
		op.setChannelValue(2, zVal);
		
		if (xSamp != 0)
		{
			AnimChan chan;
			chan.plug = xPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -std::numeric_limits<double>::infinity();
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 0;
			oAnimChanList.push_back(chan);
		}
		
		if (ySamp != 0)
		{
			AnimChan chan;
			chan.plug = yPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -std::numeric_limits<double>::infinity();
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 1;
			oAnimChanList.push_back(chan);
		}
		
		if (zSamp != 0)
		{
			AnimChan chan;
			chan.plug = zPlug;
			chan.scale = 1.0;
			if (inverse)
				chan.scale = -std::numeric_limits<double>::infinity();
			chan.opNum = oSample.getNumOps();
			chan.channelNum = 2;
			oAnimChanList.push_back(chan);
		}
		
		oSample.addOp(op);
	}
}

bool getSampledRotation(const Alembic::AbcGeom::XformSample& sample,
						const size_t opIndex[3], double& xx, double& yy, double& zz)
{
	bool success = false;
	
	xx = 0.0;
	if (opIndex[0] < sample.getNumOps())
	{
		double angleX = sample[opIndex[0]].getChannelValue(0);
		xx = Alembic::AbcGeom::DegreesToRadians(angleX);
		success = true;
	}
	
	yy = 0.0;
	if (opIndex[1] < sample.getNumOps())
	{
		double angleY = sample[opIndex[1]].getChannelValue(0);
		yy = Alembic::AbcGeom::DegreesToRadians(angleY);
		success = true;
	}
	
	zz = 0.0;
	if (opIndex[2] < sample.getNumOps())
	{
		double angleZ = sample[opIndex[2]].getChannelValue(0);
		zz = Alembic::AbcGeom::DegreesToRadians(angleZ);
		success = true;
	}
	
	return success;
}

bool setSampledRotation(Alembic::AbcGeom::XformSample& sample,
						const size_t opIndex[3], double xx, double yy, double zz)
{
	bool success = false;
	
	if (opIndex[0] < sample.getNumOps())
	{
		sample[opIndex[0]].setChannelValue(0, finiteTransformValue(Alembic::AbcGeom::RadiansToDegrees(xx)));
		success = true;
	}
	
	if (opIndex[1] < sample.getNumOps())
	{
		sample[opIndex[1]].setChannelValue(0, finiteTransformValue(Alembic::AbcGeom::RadiansToDegrees(yy)));
		success = true;
	}
	
	if (opIndex[2] < sample.getNumOps())
	{
		sample[opIndex[2]].setChannelValue(0, finiteTransformValue(Alembic::AbcGeom::RadiansToDegrees(zz)));
		success = true;
	}
	
	return success;
}

MayaTransformWriter::MayaTransformWriter(Alembic::AbcGeom::OObject & iParent,
										 MDagPath & iDag, Alembic::Util::uint32_t iTimeIndex, const JobArgs & iArgs)
{
    mInspectionBudget = iArgs.exportAttributeBudget;
    if (!mInspectionBudget) throw std::invalid_argument("Missing Alembic transform inspection budget");
	mFilterEulerRotations = iArgs.filterEulerRotations;
	mJointOrientOpIndex[0] = mJointOrientOpIndex[1] = mJointOrientOpIndex[2] =
	mRotateOpIndex[0]      = mRotateOpIndex[1]      = mRotateOpIndex[2]      =
	mRotateAxisOpIndex[0]  = mRotateAxisOpIndex[1]  = mRotateAxisOpIndex[2]  = ~size_t(0);
	
	if (transformIsJoint(iDag))
	{
		MStatus bindStatus;
		MFnIkJoint joint(iDag, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		const MString jointName = ddata_export_util::exportObjectName(joint, iArgs.stripNamespace);
		
		
		Alembic::AbcGeom::OXform obj(iParent, jointName.asChar(),
									 iTimeIndex);
		mSchema = obj.getSchema();
		
		Alembic::Abc::OCompoundProperty cp;
		Alembic::Abc::OCompoundProperty up;
		if (MayaAttributeWriter::hasAnyAttr(joint, iArgs))
		{
			cp = mSchema.getArbGeomParams();
			up = mSchema.getUserProperties();
		}
		
		mAttrs = MayaAttributeWriterPtr(new MayaAttributeWriter(cp, up, obj, joint,
														  iTimeIndex, iArgs));
		
		if (!iArgs.worldSpace)
		{
			pushTransformStack(joint, iTimeIndex == 0);
			
			// need to look at inheritsTransform
			MStatus bindStatus;
			MFnDagNode dagNode(iDag, &bindStatus);
			ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
			MPlug inheritPlug = ddata_export_util::requiredExportPlug(dagNode, "inheritsTransform");
			if (!inheritPlug.isNull())
			{
				if (ddata_export_util::getSampledType(inheritPlug) != 0)
				{
					mInheritsPlug = inheritPlug;
				}
				mSample.setInheritsXforms(ddata_export_util::readExportValue<bool>(inheritPlug));
			}
			
			// no animated inherits plug and no animated samples?
			// then use the default time sampling
			if (mAnimChanList.empty() && mInheritsPlug.isNull() && !mStructuralControlsAnimated)
			{
				mSchema.setTimeSampling(0);
			}
			
			// everything is default, don't write anything
			if (mSample.getNumOps() == 0 && mSample.getInheritsXforms())
			{
				return;
			}
			
			mSchema.set(mSample);
			return;
		}
	}
	else
	{
		MStatus bindStatus;
		MFnTransform trans(iDag, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		const MString transName = ddata_export_util::exportObjectName(trans, iArgs.stripNamespace);
		
		
		Alembic::AbcGeom::OXform obj(iParent, transName.asChar(),
									 iTimeIndex);
		mSchema = obj.getSchema();
		
		Alembic::Abc::OCompoundProperty cp;
		Alembic::Abc::OCompoundProperty up;
		if (MayaAttributeWriter::hasAnyAttr(trans, iArgs))
		{
			cp = mSchema.getArbGeomParams();
			up = mSchema.getUserProperties();
		}
		
		mAttrs = MayaAttributeWriterPtr(new MayaAttributeWriter(cp, up, obj, trans,
														  iTimeIndex, iArgs));
		
		if (!iArgs.worldSpace)
		{
			pushTransformStack(trans, iTimeIndex == 0);
			
			// need to look at inheritsTransform
			MStatus bindStatus;
			MFnDagNode dagNode(iDag, &bindStatus);
			ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
			MPlug inheritPlug = ddata_export_util::requiredExportPlug(dagNode, "inheritsTransform");
			if (!inheritPlug.isNull())
			{
				if (ddata_export_util::getSampledType(inheritPlug) != 0)
				{
					mInheritsPlug = inheritPlug;
				}
				mSample.setInheritsXforms(ddata_export_util::readExportValue<bool>(inheritPlug));
			}
			
			// no animated inherits plug and no animated samples?
			// then use the default time sampling
			if (mAnimChanList.empty() && mInheritsPlug.isNull() && !mStructuralControlsAnimated)
			{
				mSchema.setTimeSampling(0);
			}
			
			// everything is default, don't write anything
			if (mSample.getNumOps() == 0 && mSample.getInheritsXforms())
			{
				return;
			}
			
			mSchema.set(mSample);
			return;
		}
	}
	
	// if we didn't bail early then we need to add all the transform
	// information at the current node and above
	
	// copy the dag path because we'll be popping from it
	MDagPath dag(iDag);
	
	int i;
	MStatus pathStatus;
    const unsigned int numPaths = dag.length(&pathStatus);
    ddata_export_util::requireExportQuery(pathStatus, "Unable to count Alembic transform ancestors");
    if (numPaths == 0U || numPaths > dommy::exporting::kMaxAbcDagDepth) {
        throw std::length_error("Invalid Alembic transform ancestor count");
    }
	std::vector< MDagPath > dagList;
	for (i = static_cast<int>(numPaths) - 1; i > -1; i--)
	{
		dagList.push_back(dag);
		
		// inheritsTransform exists on both joints and transforms
		MStatus bindStatus;
		MFnDagNode dagNode(dag, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		MPlug inheritPlug = ddata_export_util::requiredExportPlug(dagNode, "inheritsTransform");
        // A flattened world-space stack cannot switch its ancestor chain.
        trackStructuralControl(dagNode, "inheritsTransform", true, iTimeIndex == 0);
		
		// if inheritsTransform exists and is set to false, then we
		// don't need to worry about ancestor nodes above this one
		if (!inheritPlug.isNull() && !ddata_export_util::readExportValue<bool>(inheritPlug))
			break;

		if (i > 0 && dag.pop() != MS::kSuccess)
		{
			throw std::runtime_error(
				"Unable to walk the Maya transform ancestor path");
		}
	}
	
	
	std::vector< MDagPath >::iterator iStart = dagList.begin();
	
	std::vector< MDagPath >::iterator iCur = dagList.end();
	iCur--;
	
	// now loop backwards over our dagpath list so we push ancestor nodes
	// first, all the way down to the current node
	for (; iCur != iStart; iCur--)
	{
		// only add it to the stack don't write it yet!
		
		if (transformIsJoint(*iCur))
		{
			MStatus bindStatus;
			MFnIkJoint joint(*iCur, &bindStatus);
			ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
			pushTransformStack(joint, iTimeIndex == 0);
		}
		else
		{
			MStatus bindStatus;
			MFnTransform trans(*iCur, &bindStatus);
			ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
			pushTransformStack(trans, iTimeIndex == 0);
		}
	}
	
	// finally add any transform info on the final node and write it
	if (transformIsJoint(*iCur))
	{
		MStatus bindStatus;
		MFnIkJoint joint(*iCur, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		pushTransformStack(joint, iTimeIndex == 0);
	}
	else
	{
		MStatus bindStatus;
		MFnTransform trans(*iCur, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		pushTransformStack(trans, iTimeIndex == 0);
	}
	
	// need to look at inheritsTransform
	MStatus bindStatus;
	MFnDagNode dagNode(iDag, &bindStatus);
	ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
	MPlug inheritPlug = ddata_export_util::requiredExportPlug(dagNode, "inheritsTransform");
	if (!inheritPlug.isNull())
	{
		if (ddata_export_util::getSampledType(inheritPlug) != 0)
		{
			mInheritsPlug = inheritPlug;
		}
		mSample.setInheritsXforms(ddata_export_util::readExportValue<bool>(inheritPlug));
	}
	
	// no animated inherits plug and no animated samples?
	// then use the default time sampling
	if (mAnimChanList.empty() && mInheritsPlug.isNull() && !mStructuralControlsAnimated)
	{
		mSchema.setTimeSampling(0);
	}
	
	// everything is default, don't write anything and use the default
	// time sampling
	if (mSample.getNumOps() == 0 && mSample.getInheritsXforms())
	{
		return;
	}
	
	mSchema.set(mSample);
	
}

MayaTransformWriter::MayaTransformWriter(MayaTransformWriter & iParent,
										 MDagPath & iDag, Alembic::Util::uint32_t iTimeIndex, const JobArgs & iArgs)
{
    mInspectionBudget = iArgs.exportAttributeBudget;
    if (!mInspectionBudget) throw std::invalid_argument("Missing Alembic transform inspection budget");
	mFilterEulerRotations = iArgs.filterEulerRotations;
	mJointOrientOpIndex[0] = mJointOrientOpIndex[1] = mJointOrientOpIndex[2] =
	mRotateOpIndex[0]      = mRotateOpIndex[1]      = mRotateOpIndex[2]      =
	mRotateAxisOpIndex[0]  = mRotateAxisOpIndex[1]  = mRotateAxisOpIndex[2]  = ~size_t(0);
	
	if (transformIsJoint(iDag))
	{
		MStatus bindStatus;
		MFnIkJoint joint(iDag, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		const MString jointName = ddata_export_util::exportObjectName(joint, iArgs.stripNamespace);
		
		
		Alembic::AbcGeom::OXform obj(iParent.getObject(), jointName.asChar(),
									 iTimeIndex);
		mSchema = obj.getSchema();
		
		Alembic::Abc::OCompoundProperty cp;
		Alembic::Abc::OCompoundProperty up;
		if (MayaAttributeWriter::hasAnyAttr(joint, iArgs))
		{
			cp = mSchema.getArbGeomParams();
			up = mSchema.getUserProperties();
		}
		
		mAttrs = MayaAttributeWriterPtr(new MayaAttributeWriter(cp, up, obj, joint,
														  iTimeIndex, iArgs));
		
		pushTransformStack(joint, iTimeIndex == 0);
	}
	else
	{
		MStatus bindStatus;
		MFnTransform trans(iDag, &bindStatus);
		ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
		const MString transName = ddata_export_util::exportObjectName(trans, iArgs.stripNamespace);
		
		
		Alembic::AbcGeom::OXform obj(iParent.getObject(), transName.asChar(),
									 iTimeIndex);
		mSchema = obj.getSchema();
		
		Alembic::Abc::OCompoundProperty cp;
		Alembic::Abc::OCompoundProperty up;
		if (MayaAttributeWriter::hasAnyAttr(trans, iArgs))
		{
			cp = mSchema.getArbGeomParams();
			up = mSchema.getUserProperties();
		}
		
		mAttrs = MayaAttributeWriterPtr(new MayaAttributeWriter(cp, up, obj, trans,
														  iTimeIndex, iArgs));
		
		pushTransformStack(trans, iTimeIndex == 0);
	}
	
	
	// need to look at inheritsTransform
	MStatus bindStatus;
	MFnDagNode dagNode(iDag, &bindStatus);
	ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");
	MPlug inheritPlug = ddata_export_util::requiredExportPlug(dagNode, "inheritsTransform");
	if (!inheritPlug.isNull())
	{
		if (ddata_export_util::getSampledType(inheritPlug) != 0)
			mInheritsPlug = inheritPlug;
		
		mSample.setInheritsXforms(ddata_export_util::readExportValue<bool>(inheritPlug));
	}
	
	// everything is default, don't write anything
	if (mSample.getNumOps() == 0 && mSample.getInheritsXforms())
		return;
	
	mSchema.set(mSample);
}


MayaTransformWriter::~MayaTransformWriter()
{
}

void MayaTransformWriter::write()
{
    requireStructuralControls();
    auto candidateSample = mSample;
    auto candidateJoint = mPrevJointOrientSolution;
    auto candidateRotate = mPrevRotateSolution;
    auto candidateAxis = mPrevRotateAxisSolution;

	const size_t sampleCount = mAnimChanList.size();
	if (sampleCount > 0 || !mInheritsPlug.isNull() || mStructuralControlsAnimated)
	{
		
        for (auto it:mAnimChanList)
		{
			double val = transformDouble(it.plug);
			
			if (it.scale == -std::numeric_limits<double>::infinity())
				val = ddata_export_util::inverseScale(val);
			else if (it.scale != 1.0)
				val *= it.scale;
			
			if (it.opNum >= candidateSample.getNumOps())
                throw std::invalid_argument("Invalid Alembic transform operation index");
            candidateSample[it.opNum].setChannelValue(it.channelNum, finiteTransformValue(val));
		}
		
		if (!mInheritsPlug.isNull())
		{
			candidateSample.setInheritsXforms(ddata_export_util::readExportValue<bool>(mInheritsPlug));
		}
		
		if (mFilterEulerRotations)
		{
			double xx(0), yy(0), zz(0);
			
			if (getSampledRotation(candidateSample, mJointOrientOpIndex, xx, yy, zz))
			{
				MEulerRotation euler(xx, yy, zz, candidateJoint.order);
				euler.setToClosestSolution(candidateJoint);
				
				// update sample with new solution
				setSampledRotation(candidateSample, mJointOrientOpIndex, euler.x, euler.y, euler.z);
				candidateJoint = euler;
			}
			
			if (getSampledRotation(candidateSample, mRotateOpIndex, xx, yy, zz))
			{
				MEulerRotation euler(xx, yy, zz, candidateRotate.order);
				euler.setToClosestSolution(candidateRotate);
				
				// update sample with new solution
				setSampledRotation(candidateSample, mRotateOpIndex, euler.x, euler.y, euler.z);
				candidateRotate = euler;
			}
			
			if (getSampledRotation(candidateSample, mRotateAxisOpIndex, xx, yy, zz))
			{
				MEulerRotation euler(xx, yy, zz, candidateAxis.order);
				euler.setToClosestSolution(candidateAxis);
				
				// update sample with new solution
				setSampledRotation(candidateSample, mRotateAxisOpIndex, euler.x, euler.y, euler.z);
				candidateAxis = euler;
			}
		}
		
		mSchema.set(candidateSample);
        mSample = candidateSample;
        mPrevJointOrientSolution = candidateJoint;
        mPrevRotateSolution = candidateRotate;
        mPrevRotateAxisSolution = candidateAxis;
	}
}

bool MayaTransformWriter::isAnimated() const
{
	return mAnimChanList.size() > 0 || !mInheritsPlug.isNull() || mStructuralControlsAnimated;
}

void MayaTransformWriter::trackStructuralControl(const MFnDependencyNode& node,
                                                const char* name, bool boolean,
                                                bool forceStatic)
{
    if (mStructuralControls.size() >= 3U * dommy::exporting::kMaxAbcDagDepth)
        throw std::length_error("Too many Alembic transform structural controls");
    auto& work = mInspectionBudget->inspectionWork();
    if (!work.consume(1U))
        throw std::length_error("Alembic transform inspection exceeds the work budget");
    mInspectionBudget->retainInspectionStorage(sizeof(StructuralControl));
    const MPlug plug = ddata_export_util::requiredExportPlug(node, name);
    const int value = boolean ? int(ddata_export_util::readExportValue<bool>(plug)) :
                               ddata_export_util::readExportValue<int>(plug);
    if (!boolean && (value < 0 || value > 5))
        throw std::invalid_argument("Invalid Alembic transform rotation-order control");
    if (!forceStatic && ddata_export_util::getSampledType(plug, work) != 0)
        mStructuralControlsAnimated = true;
    mStructuralControls.push_back(StructuralControl{plug, value, boolean});
}

void MayaTransformWriter::requireStructuralControls() const
{
    auto& work = mInspectionBudget->inspectionWork();
    for (const auto& control : mStructuralControls) {
        if (!work.consume(1U))
            throw std::length_error("Alembic transform inspection exceeds the work budget");
        const int value = control.boolean ?
            int(ddata_export_util::readExportValue<bool>(control.plug)) :
            ddata_export_util::readExportValue<int>(control.plug);
        if (value != control.value)
            throw std::invalid_argument("Alembic transform structure changed between frames");
    }
}

MMatrix MayaTransformWriter::getSampleMatrix() const
{
    const auto sample = mSample.getMatrix();
    MMatrix result;
    for (unsigned int row = 0U; row < 4U; ++row)
        for (unsigned int column = 0U; column < 4U; ++column)
            result[row][column] = finiteTransformValue(sample[row][column]);
    return result;
}

void MayaTransformWriter::pushTransformStack(const MFnTransform & iTrans,
											 bool iForceStatic)
{
    trackStructuralControl(iTrans, "rotateOrder", false, iForceStatic);
	
	// inspect the translate
	addTranslate(iTrans, "translate", "translateX", "translateY", "translateZ",
				 Alembic::AbcGeom::kTranslateHint, false, iForceStatic, false, mSample,
				 mAnimChanList);
	
	
	// inspect the rotate pivot translate
	addTranslate(iTrans, "rotatePivotTranslate", "rotatePivotTranslateX",
				 "rotatePivotTranslateY", "rotatePivotTranslateZ",
				 Alembic::AbcGeom::kRotatePivotTranslationHint, false,
				 iForceStatic, false, mSample, mAnimChanList);
	
	// inspect the rotate pivot
	addTranslate(iTrans, "rotatePivot", "rotatePivotX", "rotatePivotY",
				 "rotatePivotZ",  Alembic::AbcGeom::kRotatePivotPointHint,
				 false, iForceStatic, false, mSample, mAnimChanList);
	
	// inspect rotate names
	MString rotateNames[3];
	rotateNames[0] = "rotateX";
	rotateNames[1] = "rotateY";
	rotateNames[2] = "rotateZ";
	
	unsigned int rotOrder[3];
	
	// if this returns false then the rotation order was kInvalid or kLast
	MTransformationMatrix::RotationOrder eRotOrder(transformRotationOrder(iTrans));
	if (ddata_export_util::getRotOrder(eRotOrder, rotOrder[0], rotOrder[1],
        rotOrder[2]))
	{
		addRotate(iTrans, "rotate", rotateNames, rotOrder,
				  Alembic::AbcGeom::kRotateHint, iForceStatic, false,
				  mSample, mAnimChanList, mRotateOpIndex);
	}
	
	// now look at the rotation orientation, aka rotate axis
	rotateNames[0] = "rotateAxisX";
	rotateNames[1] = "rotateAxisY";
	rotateNames[2] = "rotateAxisZ";
	rotOrder[0] = 0;
	rotOrder[1] = 1;
	rotOrder[2] = 2;
	addRotate(iTrans, "rotateAxis", rotateNames, rotOrder,
			  Alembic::AbcGeom::kRotateOrientationHint, iForceStatic, false,
			  mSample, mAnimChanList, mRotateAxisOpIndex);
	
	// invert the rotate pivot if necessary
	addTranslate(iTrans, "rotatePivot", "rotatePivotX", "rotatePivotY",
				 "rotatePivotZ", Alembic::AbcGeom::kRotatePivotPointHint,
				 true, iForceStatic, false, mSample, mAnimChanList);
	
	// inspect the scale pivot translation
	addTranslate(iTrans, "scalePivotTranslate", "scalePivotTranslateX",
				 "scalePivotTranslateY", "scalePivotTranslateZ",
				 Alembic::AbcGeom::kScalePivotTranslationHint, false, iForceStatic,
				 false, mSample, mAnimChanList);
	
	// inspect the scale pivot point
	addTranslate(iTrans, "scalePivot", "scalePivotX", "scalePivotY",
				 "scalePivotZ", Alembic::AbcGeom::kScalePivotPointHint, false,
				 iForceStatic, false, mSample, mAnimChanList);
	
	// inspect the shear
	addShear(iTrans, iForceStatic, mSample, mAnimChanList);
	
	// add the scale
	addScale(iTrans, "scale", "scaleX", "scaleY", "scaleZ", false,
			 iForceStatic, false, mSample, mAnimChanList);
	
	// inverse the scale pivot point if necessary
	addTranslate(iTrans, "scalePivot", "scalePivotX", "scalePivotY",
				 "scalePivotZ", Alembic::AbcGeom::kScalePivotPointHint, true,
				 iForceStatic, false, mSample, mAnimChanList);
	
	// remember current rotation
	if (mFilterEulerRotations)
	{
		double xx(0), yy(0), zz(0);
		
		// there are 2 rotation order enum definitions:
		//     MEulerRotation::RotationOrder = MTransformationMatrix::RotationOrder-1
		if (getSampledRotation( mSample, mRotateOpIndex, xx, yy, zz ))
		{
			mPrevRotateSolution.setValue(xx, yy, zz, (MEulerRotation::RotationOrder)(eRotOrder-1));
		}
		
		if (getSampledRotation( mSample, mRotateAxisOpIndex, xx, yy, zz ))
		{
			mPrevRotateAxisSolution.setValue(xx, yy, zz, MEulerRotation::kXYZ);
		}
	}
	
}

void MayaTransformWriter::pushTransformStack(const MFnIkJoint & iJoint,
											 bool iForceStatic)
{
    trackStructuralControl(iJoint, "rotateOrder", false, iForceStatic);
    trackStructuralControl(iJoint, "segmentScaleCompensate", true, iForceStatic);
	// Some special cases that the joint is animated but has no input connections.
	bool forceAnimated = ddata_export_util::isDrivenByFBIK(iJoint) || ddata_export_util::isDrivenBySplineIK(iJoint);
	
	// inspect the translate
	addTranslate(iJoint, "translate", "translateX", "translateY", "translateZ",
				 Alembic::AbcGeom::kTranslateHint, false, iForceStatic, forceAnimated,
				 mSample, mAnimChanList);
	
	// inspect the inverseParent scale
	// [IS] is ignored when Segment Scale Compensate is false
	MPlug scaleCompensatePlug = ddata_export_util::requiredExportPlug(iJoint, "segmentScaleCompensate");
	if (ddata_export_util::readExportValue<bool>(scaleCompensatePlug))
	{
		addScale(iJoint, "inverseScale", "inverseScaleX", "inverseScaleY",
				 "inverseScaleZ", true, iForceStatic, forceAnimated, mSample, mAnimChanList);
	}
	
	MTransformationMatrix::RotationOrder eJointOrientOrder = MTransformationMatrix::kInvalid;
    MTransformationMatrix::RotationOrder eRotOrder = MTransformationMatrix::kInvalid;
    MTransformationMatrix::RotationOrder eRotateAxisOrder = MTransformationMatrix::kInvalid;
	double vals[3] = {0.0, 0.0, 0.0};
	
	// for reordering rotate names
	MString rotateNames[3];
	unsigned int rotOrder[3];
	
	// now look at the joint orientation
	rotateNames[0] = "jointOrientX";
	rotateNames[1] = "jointOrientY";
	rotateNames[2] = "jointOrientZ";
	
	ddata_export_util::requireExportQuery(iJoint.getOrientation(vals, eJointOrientOrder), "Unable to read Alembic joint orientation");
	if (eJointOrientOrder <= MTransformationMatrix::kInvalid || eJointOrientOrder >= MTransformationMatrix::kLast)
        throw std::invalid_argument("Invalid Alembic joint orientation order");
	if (ddata_export_util::getRotOrder(eJointOrientOrder, rotOrder[0], rotOrder[1], rotOrder[2]))
	{
		addRotate(iJoint, "jointOrient", rotateNames, rotOrder,
				  Alembic::AbcGeom::kRotateHint, iForceStatic, true,
				  mSample, mAnimChanList, mJointOrientOpIndex);
	}
	
	rotateNames[0] = "rotateX";
	rotateNames[1] = "rotateY";
	rotateNames[2] = "rotateZ";
	
	// if this returns false then the rotation order was kInvalid or kLast
	eRotOrder = transformRotationOrder(iJoint);
	if (ddata_export_util::getRotOrder(eRotOrder, rotOrder[0], rotOrder[1],
        rotOrder[2]))
	{
		addRotate(iJoint, "rotate", rotateNames, rotOrder,
				  Alembic::AbcGeom::kRotateHint, iForceStatic, true,
				  mSample, mAnimChanList, mRotateOpIndex);
	}
	
	// now look at the rotation orientation, aka rotate axis
	rotateNames[0] = "rotateAxisX";
	rotateNames[1] = "rotateAxisY";
	rotateNames[2] = "rotateAxisZ";
	
	ddata_export_util::requireExportQuery(iJoint.getScaleOrientation(vals, eRotateAxisOrder), "Unable to read Alembic joint orientation");
	if (eRotateAxisOrder <= MTransformationMatrix::kInvalid || eRotateAxisOrder >= MTransformationMatrix::kLast)
        throw std::invalid_argument("Invalid Alembic joint axis order");
	if (ddata_export_util::getRotOrder(eRotateAxisOrder, rotOrder[0], rotOrder[1], rotOrder[2]))
	{
		addRotate(iJoint, "rotateAxis", rotateNames, rotOrder,
				  Alembic::AbcGeom::kRotateOrientationHint, iForceStatic, true,
				  mSample, mAnimChanList, mRotateAxisOpIndex);
	}
	
	// inspect the scale
	addScale(iJoint, "scale", "scaleX", "scaleY", "scaleZ", false,
			 iForceStatic, forceAnimated, mSample, mAnimChanList);
	
	// remember current rotation
	if (mFilterEulerRotations)
	{
		double xx(0), yy(0), zz(0);
		
		// there are 2 rotation order enum definitions:
		//     MEulerRotation::RotationOrder = MTransformationMatrix::RotationOrder-1
		if (getSampledRotation( mSample, mJointOrientOpIndex, xx, yy, zz ))
		{
			mPrevJointOrientSolution.setValue(xx, yy, zz, (MEulerRotation::RotationOrder)(eJointOrientOrder-1));
		}
		
		if (getSampledRotation( mSample, mRotateOpIndex, xx, yy, zz ))
		{
			mPrevRotateSolution.setValue(xx, yy, zz, (MEulerRotation::RotationOrder)(eRotOrder-1));
		}
		
		if (getSampledRotation( mSample, mRotateAxisOpIndex, xx, yy, zz ))
		{
			mPrevRotateAxisSolution.setValue(xx, yy, zz, (MEulerRotation::RotationOrder)(eRotateAxisOrder-1));
		}
	}
}
