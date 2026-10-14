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
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/MayaMeshWriter.cpp

//
//  DMeshWriter.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/06.
//

#include "DMeshWriter.h"




#include "DMayaHeaders.h"

#include "DDataExportUtil.h"


#include "DMeshDataHeaders.h"
#include "DMeshDisplay.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMeshMergePlan.h"
#include <Alembic/AbcGeom/OPolyMesh.h>

#include <limits>
#include <stdexcept>

namespace {

bool hasExportableMeshTopology(const DMeshData& meshes, dommy::mesh::MeshMergePlan* output = nullptr) {
    if (meshes.numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount) return false;
    dommy::mesh::MeshMergePlan plan;
    for (const auto& mesh : *meshes.getMeshListConst()) {
        if (!mesh || !mesh->hasValidPolygonTopology()) {
            return false;
        }

        if (!dommy::mesh::appendMeshMergePlan(mesh->numberOfPoints(), mesh->numFace(),
                mesh->numberOfConnections(), 0U, plan) ||
            !dommy::mesh::validMeshOperationStorage(plan)) {
            return false;
        }
    }
    if (meshes.numberOfMesh() == 0 || plan.meshes != meshes.numberOfMesh()) return false;
    if (output) *output = plan;
    return true;
}

} // namespace




DMeshWriter::DMeshWriter(MDagPath & iDag,
                           Alembic::Abc::OObject & iParent, Alembic::Util::uint32_t iTimeIndex,
                           const JobArgs & iArgs) :
mIsAnimated(false), mRootDagPath(iDag), mAttributeState(iArgs.exportAttributeBudget)
{
    mMeshCount = 0;
	mPtCount = 0;
    mTimeIndex = iTimeIndex;
    mIsAnimated = false;
    
    MStatus stat;
    MFnDependencyNode fnDepNode(ddata_export_util::exportDagNode(iDag), &stat);
    if (!stat) {
        throw std::runtime_error(
            "DMeshWriter could not access the DMesh dependency node");
    }
    const MString name = ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace);
    
    
    MPlug dmPlug(ddata_export_util::exportDagNode(mRootDagPath), DMeshDisplay::inDM);
    
    MObject dataObject;
    const DMeshData* dmesh = nullptr;
    stat = dommy::maya::getPluginDataFromPlug<DMeshData>(
        dmPlug, DMeshData::id, dataObject, dmesh);
    if (!stat || !dmesh) {
        throw std::runtime_error(
            "DMeshWriter could not read valid DMesh input data");
    }
    
    
    if (dmesh->numberOfMesh() == 0U)
        throw std::invalid_argument("Selected Alembic mesh input is empty");
    if (dmesh && dmesh->numberOfMesh()) {
		if (!hasExportableMeshTopology(*dmesh)) {
			throw std::invalid_argument("Alembic export rejected invalid DMesh topology or its budget");
		}
        
        mMeshCount = dmesh->numberOfMesh();
        
        
        mIsAnimated = !ddata_export_util::readExportValue<bool>(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DMeshDisplay::aIsStaticObject));
        
        Alembic::AbcGeom::OPolyMesh obj(iParent, name.asChar(), iTimeIndex);
        mSchema = obj.getSchema();
        
        
        //for per dmesh Transform
        const std::string perMeshNameBase = name.asChar();
        for (size_t i=0;i<mMeshCount;++i) {
            const std::string perMeshName = perMeshNameBase+"_"+std::to_string(i);
            Alembic::AbcGeom::OXform transObj(iParent, perMeshName, iTimeIndex);
            mTransSchemaList.push_back(transObj.getSchema());
        }
        
        
        
        write();
        
    }
    
}

size_t DMeshWriter::getNumPoints()
{
    return mPtCount;
}

size_t DMeshWriter::getNumMeshs()
{
    return mMeshCount;
}

bool DMeshWriter::isAnimated() const
{
    return mIsAnimated;
}



#pragma mark -
#pragma mark write prim attribute

void DMeshWriter::getDMeshPrimAttribute(const DMeshData &dmesh, const std::list<std::string> &attributeList, bool isAnim) {
    

    
    Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
    
    Alembic::AbcGeom::GeometryScope scope = Alembic::AbcGeom::kUniformScope;
    
    
    for (auto attrName:attributeList) {
        
        
        
        if (dmesh.isExistAttr(attrName)) {
            const DPAttributeData::ConstPtr attribute = dmesh.getAttr(attrName);
            if (!attribute || attribute->attrSize() != dmesh.numberOfMesh()) {
                throw std::invalid_argument("Malformed Alembic mesh primitive attribute: " + attrName);
            }

            

            DP_ATTR_DATA_TYPE attrType = attribute->getAttrType();
            
            
            if (attrType == DP_ATTR_BOOL ) {
                std::vector<Alembic::Util::bool_t> val;
                DGetAbcDMPFaceAttribute<DA_BOOL>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OBoolGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::BoolArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OBoolGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_INT) {
                std::vector<DA_INT> val;
                DGetAbcDMPFaceAttribute<DA_INT>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OInt32GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::Int32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_UINT32) {
                std::vector<DA_UINT> val;
                DGetAbcDMPFaceAttribute<DA_UINT>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OUInt32GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::UInt32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OUInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT64) {
                std::vector<DA_INT64> val;
                DGetAbcDMPFaceAttribute<DA_INT64>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OInt64GeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::Int64ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt64GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_FLOAT) {
                
                std::vector<DA_FLOAT> val;
                DGetAbcDMPFaceAttribute<DA_FLOAT>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OFloatGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::FloatArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OFloatGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_DOUBLE) {
                std::vector<DA_DOUBLE> val;
                DGetAbcDMPFaceAttribute<DA_DOUBLE>(dmesh, attribute, val);
                
                Alembic::AbcGeom::ODoubleGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::DoubleArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::ODoubleGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC2I) {
                std::vector<DA_VEC2I> val;
                DGetAbcDMPFaceAttribute<DA_VEC2I>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV2iGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC2F) {
                std::vector<DA_VEC2F> val;
                DGetAbcDMPFaceAttribute<DA_VEC2F>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV2fGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2D) {
                std::vector<DA_VEC2D> val;
                DGetAbcDMPFaceAttribute<DA_VEC2D>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV2dGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V2dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3I) {
                std::vector<DA_VEC3I> val;
                DGetAbcDMPFaceAttribute<DA_VEC3I>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV3iGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC3F) {
                std::vector<DA_VEC3F> val;
                DGetAbcDMPFaceAttribute<DA_VEC3F>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV3fGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3D) {
                
                
                std::vector<DA_VEC3D> val;
                DGetAbcDMPFaceAttribute<DA_VEC3D>(dmesh, attribute, val);
                
                Alembic::AbcGeom::OV3dGeomParam::Sample newSamp;
                newSamp.setScope( scope );
                newSamp.setVals( Alembic::AbcGeom::V3dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, scope, attrType, mTimeIndex, isAnim);
            }
            
        }//check exist
        
        
    }//for attr name
    
}

#pragma mark -
#pragma mark write point attribute

void DMeshWriter::getDMeshPointAttribute(const DMeshData &dmesh, const std::list<std::string> &attributeList, bool isAnim) {
    

    
    Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
    
    Alembic::AbcGeom::GeometryScope ptScope = Alembic::AbcGeom::kVertexScope;

    for (const auto &attrName:attributeList) {
        
        
        
        if (dmesh.isExistPointAttr(attrName)) {
            
            DP_ATTR_DATA_TYPE attrType = dmesh.getPointAttributeType(attrName);
            
            
            if (attrType == DP_ATTR_BOOL ) {
                
                std::vector< Alembic::Util::bool_t > val;
                
                DGetAbcDMPScalarAttribute<DA_BOOL>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OBoolGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::BoolArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OBoolGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT) {
                
                std::vector< Alembic::Util::int32_t > val;
                
                DGetAbcDMPScalarAttribute<DA_INT>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_UINT32) {
                
                std::vector< Alembic::Util::uint32_t > val;
                
                DGetAbcDMPScalarAttribute<DA_UINT>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OUInt32GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::UInt32ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OUInt32GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_INT64) {
                
                std::vector< Alembic::Util::int64_t > val;
                
                DGetAbcDMPScalarAttribute<DA_INT64>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OInt64GeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::Int64ArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OInt64GeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_FLOAT) {
                
                std::vector< Alembic::Util::float32_t > val;
                
                DGetAbcDMPScalarAttribute<DA_FLOAT>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OFloatGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::FloatArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OFloatGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_DOUBLE) {
                
                
                std::vector< Alembic::Util::float64_t > val;
                
                DGetAbcDMPScalarAttribute<DA_DOUBLE>(dmesh, attrType, attrName, val);
                
                
                Alembic::AbcGeom::ODoubleGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::DoubleArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                
                publishShapeExportAttribute<Alembic::AbcGeom::ODoubleGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2I) {
                
                std::vector< Alembic::AbcGeom::V2i > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC2I>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            } else if (attrType == DP_ATTR_VEC2F) {
                
                std::vector< Alembic::AbcGeom::V2f > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC2F>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC2D) {
                
                std::vector< Alembic::AbcGeom::V2d > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC2D>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV2dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V2dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV2dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3I) {
                
                std::vector< Alembic::AbcGeom::V3i > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC3I>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3iGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3iArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3iGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3F) {
                
                std::vector< Alembic::AbcGeom::V3f > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC3F>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3fGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3fArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3fGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
            } else if (attrType == DP_ATTR_VEC3D) {
                
                
                std::vector< Alembic::AbcGeom::V3d > val;
                
                DGetAbcDMPScalarAttribute<DA_VEC3D>(dmesh, attrType, attrName, val);
                
                Alembic::AbcGeom::OV3dGeomParam::Sample newSamp;
                newSamp.setScope( ptScope );
                newSamp.setVals( Alembic::AbcGeom::V3dArraySample( dommy::memory::dataOrNull(val), val.size()) );
                
                publishShapeExportAttribute<Alembic::AbcGeom::OV3dGeomParam>(
                    mAttributeState, cp, pointAttributeList, attrName, newSamp, ptScope, attrType, mTimeIndex, isAnim);
                
                
            }//attribute type check
            
        }
        
    }//for point attr
    
}



#pragma mark -
#pragma mark write mesh


void DMeshWriter::write()
{

    
    MStatus stat;
    
    
    
#pragma mark get dcurve
    MFnDependencyNode dcNode(ddata_export_util::exportDagNode(mRootDagPath), &stat);
    if (!stat) {
        throw std::runtime_error(
            "DMeshWriter could not access the DMesh dependency node");
    }
    
    MPlug dmPlug(ddata_export_util::exportDagNode(mRootDagPath), DMeshDisplay::inDM);
    
    MObject dataObject;
    const DMeshData* dmesh = nullptr;
    stat = dommy::maya::getPluginDataFromPlug<DMeshData>(
        dmPlug, DMeshData::id, dataObject, dmesh);
    if (!stat || !dmesh) {
        throw std::runtime_error(
            "DMeshWriter could not read valid DMesh input data");
    }
    
    // The per-mesh schema forest is created once. Reject partition-count
    // changes before writing any child sample; do not index stale schemas.
    if (dmesh->numberOfMesh() != mMeshCount || mTransSchemaList.size() != mMeshCount) {
        throw std::invalid_argument("Alembic mesh partition count changed during export");
    }
    if (dmesh->numberOfMesh() == 0U)
        throw std::invalid_argument("Selected Alembic mesh input is empty");
    if (dmesh && dmesh->numberOfMesh()) {
		dommy::mesh::MeshMergePlan exportPlan;
        if (!hasExportableMeshTopology(*dmesh, &exportPlan)) {
			throw std::invalid_argument("Alembic export rejected invalid DMesh topology or its budget");
		}
        
        
//        std::size_t numMesh = dmesh->numberOfMesh();
        const std::size_t candidatePointCount = exportPlan.points;
        dommy::maya::NameListBudget nameBudget;
        const std::list<std::string> attrListArray = ddata_export_util::readExportNameList(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DMeshDisplay::aExportMeshAttribute),
            exportPlan.faces + exportPlan.meshes, nameBudget);
        const std::list<std::string> ptattrListArray = ddata_export_util::readExportNameList(
            MPlug(ddata_export_util::exportDagNode(mRootDagPath), DMeshDisplay::aExportPointAttribute), candidatePointCount, nameBudget);
        mAttributeState.prepare(ddata_export_util::planMeshExportAttributes(
            *dmesh, attrListArray, ptattrListArray, mAttributeState));
        
	        std::vector<Alembic::AbcGeom::V3f> points;
        std::vector<Alembic::Util::int32_t> facePoints;
        std::vector<Alembic::Util::int32_t> pointCounts;
        
//        points.resize(mPtCount*3);
        
        
#pragma mark generate mesh
        
        
        
        size_t ptOffset = 0;
        
        size_t meshIndex = 0;
        for (const auto &mIt:*dmesh->getMeshListConst())
        {
            
            //for Transform
            Alembic::AbcGeom::XformSample mTransSample;
            Alembic::AbcGeom::XformOp op(Alembic::AbcGeom::kMatrixOperation,
                                         Alembic::AbcGeom::kMatrixHint);
            
            MMatrix meshMat = mIt->getTransformMatrix();
            Alembic::Abc::M44d abcMat;
            abcMat.makeIdentity();
            
            for ( size_t i = 0 ; i < 4 ; ++i ) {
                for ( size_t j = 0 ; j < 4 ; ++j ) {
                    abcMat[i][j] = meshMat[i][j];
                }
            }
            
            op.setMatrix(abcMat);
            mTransSample.addOp(op);
            mTransSchemaList.at(meshIndex++).set(mTransSample);
            
            
            
            
            
            
            //Mesh data
            Alembic::Util::int32_t numPoint = mIt->numberOfPoints();
            
            //set points
            DPointList pts;
            if (!mIt->isIdentity()) {
                pts = mIt->getPointsInWorld();
            } else {
                pts = *mIt->getPositionPtrConst();
                
            }
            
            
	            size_t i=0;
	            
	            while (i<numPoint)
	            {
				points.push_back(Alembic::AbcGeom::V3f(
					ddata_export_util::checkedExportFloat(pts[i][0]),
					ddata_export_util::checkedExportFloat(pts[i][1]),
					ddata_export_util::checkedExportFloat(pts[i][2])));
	                i++;
	            }
            
            
            //set polygons
            MIntArray faceArray = mIt->getFaceList();
            if (faceArray.length() != static_cast<std::size_t>(mIt->numFace()))
                throw std::runtime_error("Unable to copy Alembic face counts");
            for (size_t f=0, N=faceArray.length(); f<N; ++f) {
                pointCounts.push_back(faceArray[f]);
            }
            
            MIntArray connectionArray = mIt->getConnectList();
            if (connectionArray.length() != mIt->numberOfConnections())
                throw std::runtime_error("Unable to copy Alembic face connections");
            for (size_t c=0, N=connectionArray.length(); c<N; ++c) {
                facePoints.push_back(static_cast<Alembic::Util::int32_t>(
                    static_cast<std::size_t>(connectionArray[c]) + ptOffset));
            }
            
            
            ptOffset += numPoint;
            
        }
        
        
        
        
        
        
        
        
        
        Alembic::AbcGeom::OPolyMeshSchema::Sample samp{
            Alembic::Abc::V3fArraySample(points),
            Alembic::Abc::Int32ArraySample(facePoints),
            Alembic::Abc::Int32ArraySample(pointCounts)};
        
        
        
        
        
        
#pragma mark check export attribute
        
//        Alembic::Abc::OCompoundProperty cp = mSchema.getArbGeomParams();
//        Alembic::Abc::OCompoundProperty up = mSchema.getUserProperties();
//
//        if(VERBOSE) cout << "Num Properties:" << cp.getNumProperties() << endl;
        
        

        

        getDMeshPrimAttribute(*dmesh, attrListArray, false);

        
#pragma mark check export point attribute
        
        
        
        
        getDMeshPointAttribute(*dmesh, ptattrListArray, true);
        
        
        
        
#pragma mark export
        const MBoundingBox candidateBounds = ddata_export_util::exportSampleBounds(points);
        mAttributeState.requireComplete(pointAttributeList.size());
        mSchema.set(samp);
        mAttributeState.commit();
        mSampleBounds = candidateBounds;
        mPtCount = candidatePointCount;
        
        
        
        
        
    }
    
}
