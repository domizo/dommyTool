// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2014,
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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaNurbsCurveWriter.cpp

//
//  DCurveWriter.cpp
//  dommyTool
//
//  Created by yuzo on 3/16/17.
//
//

#include "DCurveWriter.h"

#include "DMayaHeaders.h"

#include "DDataExportUtil.h"


#include "DCurveDataHeaders.h"
#include "DCurveDisplay.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMeshMergePlan.h"


#include <limits>
#include <stdexcept>

namespace {

bool hasExportableCurveTopology(const DCurveData& curves, dommy::mesh::MeshMergePlan* output = nullptr) {
    if (curves.numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount) return false;
    dommy::mesh::MeshMergePlan plan;
    bool isOpen = true;
    for (const auto& curve : *curves.getCurveListConst()) {
        if (!curve || !curve->hasValidCurveTopology() ||
            curve->getDegree() >= std::numeric_limits<Alembic::Util::uint8_t>::max()) {
            return false;
        }
        // One Alembic Curves sample has one wrap mode for every curve.
        if (plan.meshes == 0U) isOpen = curve->isCurveOpen();
        else if (curve->isCurveOpen() != isOpen) return false;

        std::size_t knots = 0U;
        if (!dommy::generation::checkedSum(curve->numberOfKnots(), 2U,
                dommy::generation::kMaxWorkItemCount, knots) ||
            !dommy::mesh::appendMeshMergePlan(curve->numberOfPoints(), 0U, knots, 0U, plan) ||
            !dommy::mesh::validMeshOperationStorage(plan)) {
            return false;
        }
    }
    if (curves.numberOfCurves() == 0 || plan.meshes != curves.numberOfCurves()) return false;
    if (output) *output = plan;
    return true;
}

} // namespace


DCurveWriter::DCurveWriter(MDagPath & iDag,
						   Alembic::Abc::OObject & iParent, Alembic::Util::uint32_t iTimeIndex,
						   const JobArgs & iArgs) :
mIsAnimated(false), mRootDagPath(iDag), mAttributeState(iArgs.exportAttributeBudget)
{
	mCurveCount = 0;
	mCVCount = 0;
	mTimeIndex = iTimeIndex;
    mIsAnimated = false;
    
		MStatus stat;
		MFnDependencyNode fnDepNode(ddata_export_util::exportDagNode(iDag), &stat);
		if (!stat) {
			throw std::runtime_error(
				"DCurveWriter could not access the DCurve dependency node");
		}
		const MString name = ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace);
		
		
		MPlug dcPlug = fnDepNode.findPlug(
			DCurveDisplay::inDC, true, &stat);
		if (!stat) {
			throw std::runtime_error(
				"DCurveWriter could not find the DCurve input plug");
		}

		MObject dataObject;
		const DCurveData* dCurves = nullptr;
		stat = dommy::maya::getPluginDataFromPlug<DCurveData>(
			dcPlug, DCurveData::id, dataObject, dCurves);
		if (!stat || !dCurves) {
			throw std::runtime_error(
				"DCurveWriter could not read valid DCurve input data");
		}
	
    if (dCurves->numberOfCurves() == 0U) {
        throw std::invalid_argument("Selected Alembic curve input is empty");
    }
    if (dCurves && dCurves->numberOfCurves()) {
		if (!hasExportableCurveTopology(*dCurves)) {
        throw std::invalid_argument("Alembic export rejected invalid DCurve topology, mixed periodicity or its budget");
		}
        
//        cout << "constructor node has curve:" << iTimeIndex << endl;
		
		mCurveCount = dCurves->numberOfCurves();
		
//
//        mIsAnimated = true;
        mIsAnimated = !ddata_export_util::readExportValue<bool>(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aIsStaticObject));
        
		Alembic::AbcGeom::OCurves obj(iParent, name.asChar(), iTimeIndex);
		mSchema = obj.getSchema();
		
		write();
		
	}
	
}

size_t DCurveWriter::getNumCVs()
{
	return mCVCount;
}

size_t DCurveWriter::getNumCurves()
{
	 return mCurveCount;
}

bool DCurveWriter::isAnimated() const
{
	return mIsAnimated;
}

void DCurveWriter::getDCurvePrimAttribute(const DCurveData &dCurves, const std::list<std::string> &attributeList, bool isAnim) {
    

    
    Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
    
    Alembic::AbcGeom::GeometryScope scope = Alembic::AbcGeom::kUniformScope;
    
    
    for (auto attrName:attributeList) {
        
        
        
        if (dCurves.isExistAttr(attrName)) {
            const DPAttributeData::ConstPtr attribute = dCurves.getAttr(attrName);
            if (!attribute || attribute->attrSize() != dCurves.numberOfCurves()) {
                throw std::invalid_argument("Malformed Alembic curve primitive attribute: " + attrName);
            }

            DP_ATTR_DATA_TYPE attrType = attribute->getAttrType();
            
            
            if (attrType == DP_ATTR_BOOL ) {
                std::vector<bool> tmpval = dCurves.attrListValue<bool>(attrName);
                
                std::vector<Alembic::AbcGeom::bool_t> val(tmpval.begin(), tmpval.end());
                
                Alembic::AbcGeom::OBoolGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::BoolArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OBoolGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_INT) {
                std::vector<int32_t> val = dCurves.attrListValue<int32_t>(attrName);
                
                Alembic::AbcGeom::OInt32GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::Int32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_UINT32) {
                std::vector<uint32_t> val = dCurves.attrListValue<uint32_t>(attrName);
                
                Alembic::AbcGeom::OUInt32GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::UInt32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OUInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT64) {
                std::vector<int64_t> val = dCurves.attrListValue<int64_t>(attrName);
                
                Alembic::AbcGeom::OInt64GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::Int64ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt64GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_FLOAT) {
                
                std::vector<float> val = dCurves.attrListValue<float>(attrName);
                
                Alembic::AbcGeom::OFloatGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::FloatArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OFloatGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_DOUBLE) {
                std::vector<double> val = dCurves.attrListValue<double>(attrName);
                
                Alembic::AbcGeom::ODoubleGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::DoubleArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::ODoubleGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC2I) {
                std::vector<Imath::V2i> val = dCurves.attrListValue<Imath::V2i>(attrName);
                
                Alembic::AbcGeom::OV2iGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC2F) {
                std::vector<Imath::V2f> val = dCurves.attrListValue<Imath::V2f>(attrName);
                
                Alembic::AbcGeom::OV2fGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2D) {
                std::vector<Imath::V2d> val = dCurves.attrListValue<Imath::V2d>(attrName);
                
                Alembic::AbcGeom::OV2dGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3I) {
                std::vector<Imath::V3i> val = dCurves.attrListValue<Imath::V3i>(attrName);
                
                Alembic::AbcGeom::OV3iGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC3F) {
                std::vector<Imath::V3f> val = dCurves.attrListValue<Imath::V3f>(attrName);
                
                Alembic::AbcGeom::OV3fGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3D) {
                
                
                std::vector<Imath::V3d> val = dCurves.attrListValue<Imath::V3d>(attrName);
                
                Alembic::AbcGeom::OV3dGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
            }
            
        }//check exist
        
        
    }//for attr name
    
}

void DCurveWriter::getDCurvePointAttribute(const DCurveData &dCurves, const std::list<std::string> &attributeList, bool isAnim) {
    

    
    Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
    
    Alembic::AbcGeom::GeometryScope ptScope = Alembic::AbcGeom::kVertexScope;
    
    for (auto attrName:attributeList) {
        
        
        
        if (dCurves.isExistPointAttr(attrName)) {
            
            DP_ATTR_DATA_TYPE attrType =  dCurves.getPointAttributeType(attrName);
            
            
            if (attrType == DP_ATTR_BOOL ) {
                
                std::vector< Alembic::Util::bool_t > val;
                
                DGetAbcDCPScalarAttribute<bool>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OBoolGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::BoolArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OBoolGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT) {
                
                std::vector< Alembic::Util::int32_t > val;
                
                DGetAbcDCPScalarAttribute<int>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_UINT32) {
                
                std::vector< Alembic::Util::uint32_t > val;
                
                DGetAbcDCPScalarAttribute<uint32_t>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OUInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::UInt32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OUInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT64) {
                
                std::vector< Alembic::Util::int64_t > val;
                
                DGetAbcDCPScalarAttribute<int64_t>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt64GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int64ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt64GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_FLOAT) {
                
                std::vector< Alembic::Util::float32_t > val;
                
                DGetAbcDCPScalarAttribute<float>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OFloatGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::FloatArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OFloatGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_DOUBLE) {
                
                
                std::vector< Alembic::Util::float64_t > val;
                
                DGetAbcDCPScalarAttribute<double>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::ODoubleGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::DoubleArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                
                publishShapeExportAttribute<Alembic::AbcGeom::ODoubleGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2I) {
                
                
                std::vector< Alembic::AbcGeom::V2i > val;
                
                DGetAbcDCPScalarAttribute<Imath::V2i>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
                
                
            } else if (attrType == DP_ATTR_VEC2F) {
                
                std::vector< Alembic::AbcGeom::V2f > val;
                
                DGetAbcDCPScalarAttribute<Imath::V2f>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2D) {
                
                std::vector< Alembic::AbcGeom::V2d > val;
                
                DGetAbcDCPScalarAttribute<Imath::V2d>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3I) {
                
                std::vector< Alembic::AbcGeom::V3i > val;
                
                DGetAbcDCPScalarAttribute<Imath::V3i>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3F) {
                
                std::vector< Alembic::AbcGeom::V3f > val;
                
                DGetAbcDCPScalarAttribute<Imath::V3f>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3D) {
                
                
                std::vector< Alembic::AbcGeom::V3d > val;
                
                DGetAbcDCPScalarAttribute<Imath::V3d>(&dCurves, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            }//attribute type check
            
        }
        
    }//for point attr
    
}


void DCurveWriter::write()
{

	
	MStatus stat;
	
	
	
#pragma mark get dcurve
		MFnDependencyNode dcNode(ddata_export_util::exportDagNode(mRootDagPath), &stat);
		if (!stat) {
			throw std::runtime_error(
				"DCurveWriter could not access the DCurve dependency node");
		}
		
		MPlug dcPlug = dcNode.findPlug(
			DCurveDisplay::inDC, true, &stat);
		if (!stat) {
			throw std::runtime_error(
				"DCurveWriter could not find the DCurve input plug");
		}

		MObject dataObject;
		const DCurveData* dCurves = nullptr;
		stat = dommy::maya::getPluginDataFromPlug<DCurveData>(
			dcPlug, DCurveData::id, dataObject, dCurves);
		if (!stat || !dCurves) {
			throw std::runtime_error(
				"DCurveWriter could not read valid DCurve input data");
		}
	
    if (dCurves->numberOfCurves() == 0U) {
        throw std::invalid_argument("Selected Alembic curve input is empty");
    }
    if (dCurves && dCurves->numberOfCurves()) {
		dommy::mesh::MeshMergePlan exportPlan;
        if (!hasExportableCurveTopology(*dCurves, &exportPlan)) {
			throw std::invalid_argument("Alembic export rejected invalid DCurve topology, mixed periodicity or its budget");
		}
        dommy::maya::NameListBudget nameBudget;
        const std::list<std::string> attrListArray = ddata_export_util::readExportNameList(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aExportCurveAttribute), exportPlan.meshes, nameBudget);
        const std::list<std::string> ptattrListArray = ddata_export_util::readExportNameList(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aExportPointAttribute), exportPlan.points, nameBudget);
        mAttributeState.prepare(ddata_export_util::planCurveExportAttributes(
            *dCurves, attrListArray, ptattrListArray, mAttributeState));
		
		std::size_t candidateCVCount = 0U;
        Alembic::AbcGeom::OCurvesSchema::Sample samp;
		samp.setBasis(Alembic::AbcGeom::kBsplineBasis);
		
		
		
		
        
		std::size_t numCurves = dCurves->numberOfCurves();
		
        
        
		std::vector<Alembic::Util::int32_t> nVertices(numCurves);
			std::vector<Alembic::AbcGeom::V3f> points;
		std::vector<float> width;
		std::vector<float> knots;
		std::vector<Alembic::Util::uint8_t> orders(numCurves);
		
//        MMatrix transformMatrix;
		
		
		
		
#pragma mark get width
		
		const short widthType = ddata_export_util::readExportValue<short>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aCurveWidthType));
		if (widthType < 0 || widthType > 1) throw std::invalid_argument("Invalid Alembic width mode");
        const float constWidth = widthType == 0 ? ddata_export_util::checkedExportFloat(
            ddata_export_util::readExportValue<float>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aConstantWidth))) : 0.0f;
		const std::string widthCurveAttribute = widthType == 1 ? ddata_export_util::readExportValue<MString>(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aWidthAttributeName)).asChar() : "";
		
		Alembic::AbcGeom::GeometryScope widthScope = Alembic::AbcGeom::kVertexScope;
		
		
		
		
		
		
		if (widthType==1) {//from attribute
			
			if (dCurves->isExistAttr(widthCurveAttribute)) {
                const DPAttributeData::ConstPtr attribute = dCurves->getAttr(widthCurveAttribute);
                if (!attribute || attribute->attrSize() != numCurves)
                    throw std::invalid_argument("Malformed Alembic curve width attribute");
				
				//curve attribute
				widthScope = Alembic::AbcGeom::kUniformScope;
				
//                DScalarDataList tmpWidth;
				getDCAttributeMDoubleArrayFromScalar(*dCurves, widthCurveAttribute, width);
                if (width.size() != numCurves)
                    throw std::invalid_argument("Unsupported Alembic curve width type");
				
//                width = DScalarDataList(tmpWidth.begin(), tmpWidth.end());
				
			} else if (dCurves->isExistPointAttr(widthCurveAttribute)) {
				
				//point attribute;
				widthScope = Alembic::AbcGeom::kVertexScope;
				
                const DP_ATTR_DATA_TYPE type = dCurves->getPointAttributeType(widthCurveAttribute);
                if (type == DP_ATTR_FLOAT)
                    DGetAbcDCPScalarAttribute<float>(dCurves, type, widthCurveAttribute, width);
                else if (type == DP_ATTR_DOUBLE)
                    DGetAbcDCPScalarAttribute<double>(dCurves, type, widthCurveAttribute, width);
                else throw std::invalid_argument("Unsupported Alembic curve point-width type");

			}
			
			
		} else {//Contant Width
			
			width.push_back(constWidth);
			widthScope = Alembic::AbcGeom::kConstantScope;
			
		}
		

		
        for (float value : width) {
            if (!std::isfinite(value) || value < 0.0f)
                throw std::invalid_argument("Invalid Alembic width value");
        }
#pragma mark generate curve
		
		const bool isMultiEndKnot = ddata_export_util::readExportValue<bool>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aIsMultiEndKnot));
		
		
		size_t curveOffset = 0;
        for (const auto &cIt:*dCurves->getCurveListConst())
		{
			
			if (curveOffset == 0)
			{
                if (cIt->isCurveOpen())
				{
					samp.setWrap(Alembic::AbcGeom::kNonPeriodic);
				}
				else
				{
					samp.setWrap(Alembic::AbcGeom::kPeriodic);
				}
				
				
				if (cIt->getDegree() == 3)
				{
					samp.setType(Alembic::AbcGeom::kCubic);
				}
				else if (cIt->getDegree() == 1)
				{
					samp.setType(Alembic::AbcGeom::kLinear);
				}
				else
				{
					samp.setType(Alembic::AbcGeom::kVariableOrder);
				}
			}
			else
			{
				if ((samp.getType() == Alembic::AbcGeom::kCubic &&
					 cIt->getDegree() != 3) ||
					(samp.getType() == Alembic::AbcGeom::kLinear &&
					 cIt->getDegree() != 1))
				{
					samp.setType(Alembic::AbcGeom::kVariableOrder);
				}
			}
			
			
			orders[curveOffset] = static_cast<Alembic::Util::uint8_t>(cIt->getDegree() + 1);
			
			
			Alembic::Util::int32_t numCVs = cIt->numberOfPoints();
			
            
			candidateCVCount += numCVs;
			nVertices[curveOffset] = numCVs;
			
            DPointList pts;
            if (!cIt->isIdentity()) {
                pts = cIt->getPointsInWorld();
            } else {
                pts = *cIt->getPositionPtrConst();
                
            }
            
            
	            size_t i=0,N=pts.size();
	            while (i<N)
				{
					points.push_back(Alembic::AbcGeom::V3f(
						ddata_export_util::checkedExportFloat(pts[i][0]),
						ddata_export_util::checkedExportFloat(pts[i][1]),
						ddata_export_util::checkedExportFloat(pts[i][2])));
//                points.push_back(static_cast<float>(transformdPt.x));
//                points.push_back(static_cast<float>(transformdPt.y));
//                points.push_back(static_cast<float>(transformdPt.z));
                i++;
			}
			
			
            MDoubleArray knotsArray=cIt->getKnots();
            if (knotsArray.length() != cIt->numberOfKnots())
                throw std::runtime_error("Unable to copy Alembic knots");
			
			// need to add a knot to the start and end (M + 2N + 1)
			if (knotsArray.length() > 1)
			{
				if(isMultiEndKnot) {
					knots.push_back(ddata_export_util::checkedExportFloat(knotsArray[0]));
//					knots.push_back(ddata_export_util::checkedExportFloat(knotsArray[0]));
				}
				
				for (unsigned int j = 0; j < knotsArray.length(); ++j)
				{
					knots.push_back(ddata_export_util::checkedExportFloat(knotsArray[j]));
				}
				if(isMultiEndKnot) {
					knots.push_back(ddata_export_util::checkedExportFloat(knotsArray[knotsArray.length()-1]));
//					knots.push_back(ddata_export_util::checkedExportFloat(knotsArray[knotsArray.length()-1]));
				}
				
			}
			
            
			curveOffset++;
            
		}
		
		
		
		
		
		
		
		
		
			samp.setCurvesNumVertices(Alembic::Abc::Int32ArraySample(nVertices));
			samp.setPositions(Alembic::Abc::V3fArraySample(points));
		
		const bool widthsPresent = !width.empty();
        mBuiltinState.require(widthsPresent, widthScope, false);
        if (widthsPresent)
            samp.setWidths(Alembic::AbcGeom::OFloatGeomParam::Sample(
                Alembic::Abc::FloatArraySample(width), widthScope));
		
		
		
		
		if (!width.empty() && widthScope == Alembic::AbcGeom::kUniformScope && width.size() != numCurves)
            throw std::invalid_argument("Malformed Alembic curve width attribute");
        if (!width.empty() && widthScope == Alembic::AbcGeom::kVertexScope && width.size() != candidateCVCount)
            throw std::invalid_argument("Malformed Alembic curve point-width attribute");
        if (samp.getType() == Alembic::AbcGeom::kVariableOrder)
		{
			samp.setOrders(Alembic::Abc::UcharArraySample(orders));
		}
		
		if (!knots.empty())
		{
			samp.setKnots(Alembic::Abc::FloatArraySample(knots));
		}
		
		
		
#pragma mark check export attribute
		
        Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
        Alembic::Abc::OCompoundProperty up = mSchema.getUserProperties();
        
        
        
        
		//const MString exportCurveAttributeNames = MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aExportCurveAttribute).asString();
		
		
		
		//MStringArray curveAttrNameArray;
		
		//exportCurveAttributeNames.split(*MString(" ").asChar(), curveAttrNameArray);
        
//        std::list<std::string> attrAnimListArray;
//        boost::split(attrAnimListArray, _attrAnimList, boost::is_space(), boost::algorithm::token_compress_on );
		
        
		
		//if(VERBOSE)cout << "curveAttrNameArray count:" << curveAttrNameArray.length() << endl;
		
        
//		if(VERBOSE)cout << "attrAnimListArray count:" << attrAnimListArray.size() << endl;
		
        
        getDCurvePrimAttribute(*dCurves, attrListArray, false);
//        getDCurvePrimAttribute(dCurves, attrAnimListArray, true);
        
        
        
        
#pragma mark check export point attribute
		
//		const MString exportPointAttribute = MPlug(ddata_export_util::exportDagNode(mRootDagPath), DCurveDisplay::aExportPointAttribute).asString();
		
		
//		MStringArray pointAttrNameArray;
//		
//		exportPointAttribute.split(*MString(" ").asChar(), pointAttrNameArray);
        
		
		
        
        
        getDCurvePointAttribute(*dCurves, ptattrListArray, false);

#pragma mark export
        const MBoundingBox candidateBounds = ddata_export_util::exportSampleBounds(points);
        if (candidateCVCount != exportPlan.points)
            throw std::logic_error("Alembic curve sample count changed after admission");
        mAttributeState.requireComplete(pointAttributeList.size());
		mSchema.set(samp);
        mAttributeState.commit();
        mBuiltinState.commit(widthsPresent, widthScope, false);
        mSampleBounds = candidateBounds;
        mCVCount = candidateCVCount;
        mCurveCount = numCurves;
		
        
		
		
	}
	
}
