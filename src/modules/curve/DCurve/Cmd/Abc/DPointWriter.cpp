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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaPointPrimitiveWriter.cpp

//
//  DPointWriter.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/31.
//

#include "DPointWriter.h"

#include "DMayaHeaders.h"

#include "DDataExportUtil.h"


#include "DPointDataHeaders.h"
#include "DPointDisplay.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMeshMergePlan.h"


#include <stdexcept>

namespace {
bool hasExportablePointTopology(const DPData& points)
{
    dommy::mesh::MeshMergePlan plan;
    return points.hasValidPointTopology() &&
        dommy::mesh::appendMeshMergePlan(points.numberOfPoints(), 0U, 0U, 0U, plan) &&
        dommy::mesh::validMeshOperationStorage(plan);
}
} // namespace


DPointWriter::DPointWriter(MDagPath & iDag,
                           Alembic::Abc::OObject & iParent, Alembic::Util::uint32_t iTimeIndex,
                           const JobArgs & iArgs) :
mIsAnimated(false), mRootDagPath(iDag), mAttributeState(iArgs.exportAttributeBudget)
{
    mPointCount = 0;
    mTimeIndex = iTimeIndex;
    mIsAnimated = false;
    
    MStatus stat;
    MFnDependencyNode fnDepNode(ddata_export_util::exportDagNode(iDag), &stat);
    if (!stat) {
        throw std::runtime_error(
            "DPointWriter could not access the DPoint dependency node");
    }
    const MString name = ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace);
    
    
    MPlug dpPlug = fnDepNode.findPlug(
        DPointDisplay::inDP, true, &stat);
    if (!stat) {
        throw std::runtime_error(
            "DPointWriter could not find the DPoint input plug");
    }

    MObject dataObject;
    const DPData* dpoint = nullptr;
    stat = dommy::maya::getPluginDataFromPlug<DPData>(
        dpPlug, DPData::id, dataObject, dpoint);
    if (!stat || !dpoint) {
        throw std::runtime_error(
            "DPointWriter could not read valid DPoint input data");
    }
    
    if (dpoint->numberOfPoints() == 0U)
        throw std::invalid_argument("Selected Alembic point input is empty");
    if (!hasExportablePointTopology(*dpoint))
        throw std::invalid_argument("Alembic export rejected invalid DPoint topology or its budget");
    if (dpoint && dpoint->numberOfPoints()) {
        
        //        cout << "constructor node has curve:" << iTimeIndex << endl;
        
        mPointCount = dpoint->numberOfPoints();
        
        //
        //        mIsAnimated = true;
        mIsAnimated = !ddata_export_util::readExportValue<bool>(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aIsStaticObject));
        
        Alembic::AbcGeom::OPoints obj(iParent, name.asChar(), iTimeIndex);
        mSchema = obj.getSchema();
        
        write();
        
    }
    
}

size_t DPointWriter::getNumPoints()
{
    return mPointCount;
}

bool DPointWriter::isAnimated() const
{
    return mIsAnimated;
}


void DPointWriter::getDPointPointAttribute(const DPData &dpoint, const std::list<std::string> &attributeList, bool isAnim) {
    

    
    Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
    
    Alembic::AbcGeom::GeometryScope ptScope = Alembic::AbcGeom::kVertexScope;

    for (auto attrName:attributeList) {
        
        
        
        if (dpoint.isExistAttr(attrName)) {
            
            DP_ATTR_DATA_TYPE attrType =  dpoint.getAttr(attrName)->getAttrType();
            
            
            if (attrType == DP_ATTR_BOOL ) {
                
                std::vector< Alembic::Util::bool_t > val;
                
                DGetAbcDPScalarAttribute<bool>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OBoolGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::BoolArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OBoolGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT) {
                
                std::vector< Alembic::Util::int32_t > val;
                
                DGetAbcDPScalarAttribute<int>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_UINT32) {
                
                std::vector< Alembic::Util::uint32_t > val;
                
                DGetAbcDPScalarAttribute<uint32_t>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OUInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::UInt32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OUInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT64) {
                
                std::vector< Alembic::Util::int64_t > val;
                
                DGetAbcDPScalarAttribute<int64_t>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt64GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int64ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt64GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_FLOAT) {
                
                std::vector< Alembic::Util::float32_t > val;
                
                DGetAbcDPScalarAttribute<float>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OFloatGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::FloatArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OFloatGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_DOUBLE) {
                
                
                std::vector< Alembic::Util::float64_t > val;
                
                DGetAbcDPScalarAttribute<double>(&dpoint, attrType, attrName, val);
                
                
                Alembic::AbcGeom::ODoubleGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::DoubleArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                
                publishShapeExportAttribute<Alembic::AbcGeom::ODoubleGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2I) {
                
                
                std::vector< Alembic::AbcGeom::V2i > val;
                
                DGetAbcDPScalarAttribute<Imath::V2i>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
                
                
            } else if (attrType == DP_ATTR_VEC2F) {
                
                std::vector< Alembic::AbcGeom::V2f > val;
                
                DGetAbcDPScalarAttribute<Imath::V2f>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2D) {
                
                std::vector< Alembic::AbcGeom::V2d > val;
                
                DGetAbcDPScalarAttribute<Imath::V2d>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3I) {
                
                std::vector< Alembic::AbcGeom::V3i > val;
                
                DGetAbcDPScalarAttribute<Imath::V3i>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3F) {
                
                std::vector< Alembic::AbcGeom::V3f > val;
                
                DGetAbcDPScalarAttribute<Imath::V3f>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3D) {
                
                
                std::vector< Alembic::AbcGeom::V3d > val;
                
                DGetAbcDPScalarAttribute<Imath::V3d>(&dpoint, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            }//attribute type check
            
        }
        
    }//for point attr
    
}


void DPointWriter::write()
{

    
    MStatus stat;
    
    
    
#pragma mark get dcurve
    MFnDependencyNode dcNode(ddata_export_util::exportDagNode(mRootDagPath), &stat);
    if (!stat) {
        throw std::runtime_error(
            "DPointWriter could not access the DPoint dependency node");
    }
    
    MPlug dpPlug = dcNode.findPlug(
        DPointDisplay::inDP, true, &stat);
    if (!stat) {
        throw std::runtime_error(
            "DPointWriter could not find the DPoint input plug");
    }

    MObject dataObject;
    const DPData* dpoint = nullptr;
    stat = dommy::maya::getPluginDataFromPlug<DPData>(
        dpPlug, DPData::id, dataObject, dpoint);
    if (!stat || !dpoint) {
        throw std::runtime_error(
            "DPointWriter could not read valid DPoint input data");
    }
    
    if (dpoint->numberOfPoints() == 0U)
        throw std::invalid_argument("Selected Alembic point input is empty");
    if (!hasExportablePointTopology(*dpoint))
        throw std::invalid_argument("Alembic export rejected invalid DPoint topology or its budget");
    if (dpoint && dpoint->numberOfPoints()) {
        
        Alembic::AbcGeom::OPointsSchema::Sample samp;
        
        
        const std::size_t size = dpoint->numberOfPoints();
        const DPointList *ptsPtr = dpoint->getPositionPtrConst();
        if (!ptsPtr || ptsPtr->size() != size) {
            throw std::invalid_argument("Alembic export rejected invalid DPoint storage");
        }
        dommy::maya::NameListBudget nameBudget;
        const std::list<std::string> ptattrListArray = ddata_export_util::readExportNameList(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aExportPointAttribute), size, nameBudget);
        mAttributeState.prepare(ddata_export_util::planPointExportAttributes(*dpoint, ptattrListArray, mAttributeState));
        
        std::vector<Alembic::AbcGeom::V3f> position;
        // Alembic samples borrow their arrays until mSchema.set() returns.
        std::vector<Alembic::AbcGeom::V3f> velocity;
        std::vector<float> width;
        std::vector< Alembic::Util::uint64_t > particleIds;
        
        
        position.reserve(size);
        
        particleIds.reserve(size);
        width.reserve(size);
        
        
#pragma mark set position
        for (const auto &pIt:*ptsPtr) {
            position.push_back(Alembic::AbcGeom::V3f(
                ddata_export_util::checkedExportFloat(pIt[0]),
                ddata_export_util::checkedExportFloat(pIt[1]),
                ddata_export_util::checkedExportFloat(pIt[2])));
        }
        
        samp.setPositions(Alembic::Abc::V3fArraySample(position));
        
        
#pragma mark set  particleIds
        
        size_t i=0;
        while (i<size) {
            particleIds.push_back(i++);
        }
        
        samp.setIds(Alembic::Abc::UInt64ArraySample(particleIds));
        
        
        
        
        
#pragma mark get velocity
        
        const std::string velAttribute = ddata_export_util::readExportValue<MString>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aVelocityAttributeName)).asChar();
        
        const bool velocitiesPresent = dpoint->isExistAttr(velAttribute);
        if (velocitiesPresent) {
            
            velocity.reserve(size);
            
            std::vector<DVec3Data> tmpVels;
            getMVectorArrayFromAttribute(*dpoint, velAttribute, tmpVels);
            
            if (tmpVels.size() == size) {
                for (size_t v=0, N=tmpVels.size(); v<N; ++v) {
                    velocity.push_back(Alembic::AbcGeom::V3f(
                        ddata_export_util::checkedExportFloat(tmpVels[v][0]),
                        ddata_export_util::checkedExportFloat(tmpVels[v][1]),
                        ddata_export_util::checkedExportFloat(tmpVels[v][2])));
                }
                
                
                samp.setVelocities(Alembic::Abc::V3fArraySample(velocity));
                
                
            } else {
                throw std::invalid_argument("Malformed Alembic point velocity attribute");
            }
            
        }
        
        
#pragma mark get width
        
        const short widthType = ddata_export_util::readExportValue<short>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aPointWidthType));
        if (widthType < 0 || widthType > 1) throw std::invalid_argument("Invalid Alembic width mode");
        const float constWidth = widthType == 0 ? ddata_export_util::checkedExportFloat(
            ddata_export_util::readExportValue<float>(MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aConstantWidth))) : 0.0f;
        const std::string widthAttribute = widthType == 1 ? ddata_export_util::readExportValue<MString>(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DPointDisplay::aWidthAttributeName)).asChar() : "";
        
        Alembic::AbcGeom::GeometryScope widthScope = Alembic::AbcGeom::kVertexScope;
        
        
        
        
        
        
        if (widthType==1) {//from attribute
            
            if (dpoint->isExistAttr(widthAttribute)) {
                
                getDScalarArrayFromAttribute(*dpoint, widthAttribute, width);
                
            }
            
        } else {//Contant Width
            
            width.push_back(constWidth);
            widthScope = Alembic::AbcGeom::kConstantScope;
            
        }
        
        
        for (float value : width) {
            if (!std::isfinite(value) || value < 0.0f)
                throw std::invalid_argument("Invalid Alembic width value");
        }
        if (widthType == 1 && dpoint->isExistAttr(widthAttribute) && width.size() != size)
            throw std::invalid_argument("Malformed Alembic point width attribute");
        const bool widthsPresent = !width.empty();
        mBuiltinState.require(widthsPresent, widthScope, velocitiesPresent);
        if (widthsPresent)
            samp.setWidths(Alembic::AbcGeom::OFloatGeomParam::Sample(
                Alembic::Abc::FloatArraySample(width), widthScope));
        
        
#pragma mark check export attribute
        
        Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
        Alembic::Abc::OCompoundProperty up = mSchema.getUserProperties();
//
        
        
        
        
        
#pragma mark check export point attribute
        
        
        getDPointPointAttribute(*dpoint, ptattrListArray, false);
        
        
        
#pragma mark export
        const MBoundingBox candidateBounds = ddata_export_util::exportSampleBounds(position);
        mAttributeState.requireComplete(pointAttributeList.size());
        mSchema.set(samp);
        mAttributeState.commit();
        mBuiltinState.commit(widthsPresent, widthScope, velocitiesPresent);
        mSampleBounds = candidateBounds;
        mPointCount = size;
        
        
        
        
        
    }
    
}
