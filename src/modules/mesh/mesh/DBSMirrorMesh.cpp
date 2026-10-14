//
//  DBSMirrorMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/15.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DBSMirrorMesh.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaPointBlend.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"



#include "general_ids.h"

#include <algorithm>
#include <cmath>
#include <limits>



const MTypeId DBSMirrorMesh::typeId( ID_MESH_BLEND_SHAPE_MIRROR );

MObject DBSMirrorMesh::inBaseMesh;
MObject DBSMirrorMesh::inTargetMesh;

MObject DBSMirrorMesh::aDistance;
MObject DBSMirrorMesh::aBlendRamp;

MObject DBSMirrorMesh::outLeftMesh;
MObject DBSMirrorMesh::outRightMesh;



void *DBSMirrorMesh::creator() {
    return ::dommy::maya::createObject("DBSMirrorMesh", [&]() {
        return new DBSMirrorMesh();
    });
}



void DBSMirrorMesh::postConstructor() {
    dommy::maya::initializeNodeRamps("DBSMirrorMesh", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aBlendRamp,
                                                                    {0.0f, 1.0f}, {0.5f, 1.0f}));
        return MS::kSuccess;
    });
}



MStatus DBSMirrorMesh::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;
//    const bool VERBOSE = false;
    
    
    if ( plug == outLeftMesh || plug == outRightMesh )
    {
        
        
        //get attribute
        MPlug blendRampPlug( thisMObject(), aBlendRamp );
        MRampAttribute blendRampAttr(blendRampPlug, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        const MDataHandle distanceHandle = data.inputValue(aDistance, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const double distance = distanceHandle.asDouble();
        if (!std::isfinite(distance) || distance < 0.0) {
            return MS::kInvalidParameter;
        }
        const double affectDist = distance*2.0;
        if (!std::isfinite(affectDist)) return MS::kInvalidParameter;

        dommy::maya::MeshPairInput meshes;
        stat = dommy::maya::getMatchingMeshPair(
            thisMObject(), inBaseMesh, inTargetMesh, data, meshes);
        if (!stat) {
            EchoWarning(name().asChar(),
                        "Base and target meshes must contain matching finite point arrays.");
            return stat;
        }

        const auto sampleRamp = [&](double centerDistance, float& value) -> MStatus {
            if (!std::isfinite(centerDistance) || centerDistance < 0.0) {
                return MS::kInvalidParameter;
            }
            double position = 1.0;
            if (affectDist > std::numeric_limits<double>::epsilon()) {
                position = std::min(1.0, centerDistance / affectDist);
            }
            MStatus rampStatus;
            blendRampAttr.getValueAtPosition(
                static_cast<float>(position), value, &rampStatus);
            if (!rampStatus) return rampStatus;
            if (!std::isfinite(value)) return MS::kInvalidParameter;
            return MS::kSuccess;
        };
        
        
#pragma mark calc
        
        
        //outputData
        MFnMeshData leftDataCreator, rightDataCreator;
        
        MObject newLeftMeshData = leftDataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        MObject newRightMeshData = rightDataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (newLeftMeshData.isNull() || newRightMeshData.isNull()) return MS::kFailure;
        
        MFnMesh leftMeshFn, rightMeshFn;
        
        //copy base mesh
        MObject copiedMesh = leftMeshFn.copy(
            meshes.firstObject, newLeftMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (copiedMesh.isNull()) return MS::kFailure;
        copiedMesh = rightMeshFn.copy(
            meshes.firstObject, newRightMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (copiedMesh.isNull()) return MS::kFailure;
        
        
        //get mesh data
        const MPointArray& targetPointArray = meshes.secondPoints;
        
        
        
        //Left Mesh
        MItMeshVertex leftVtxIt(newLeftMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(
            leftVtxIt, meshes.pointCount, [&](MItMeshVertex& vertex) -> MStatus {
            
            const MPoint pt = vertex.position(MSpace::kObject, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            if (pt.x < -distance) {

                return MS::kSuccess;
            }
            
            const int rawIndex = vertex.index(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!dommy::memory::indexInRange(rawIndex, meshes.pointCount)) {
                return MS::kInvalidParameter;
            }
            const std::size_t index = static_cast<std::size_t>(rawIndex);
            
            MPoint targetPt = targetPointArray[index];
            double centerDist = pt.distanceTo(MPoint(-distance,pt.y,pt.z));
            
            float value = 0.0f;
            DOMMY_RETURN_IF_FAILURE(sampleRamp(centerDist, value));
            
            MPoint fintPt;
            if (!dommy::maya::checkedPointBlend(pt, targetPt, value, fintPt,
                    dommy::maya::PointBlendRounding::LegacyDisplacement)) {
                return MS::kInvalidParameter;
            }
            return vertex.setPosition(fintPt);
        }));
        
        DOMMY_RETURN_IF_FAILURE(leftMeshFn.updateSurface());
        
        
        
        //Right Mesh
        MItMeshVertex rightVtxIt(newRightMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(
            rightVtxIt, meshes.pointCount, [&](MItMeshVertex& vertex) -> MStatus {
            
            const MPoint pt = vertex.position(MSpace::kObject, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            if (pt.x > distance) {
                
                return MS::kSuccess;
            }
            
            const int rawIndex = vertex.index(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!dommy::memory::indexInRange(rawIndex, meshes.pointCount)) {
                return MS::kInvalidParameter;
            }
            const std::size_t index = static_cast<std::size_t>(rawIndex);
            
            MPoint targetPt = targetPointArray[index];
            double centerDist = pt.distanceTo(MPoint(distance,pt.y,pt.z));
            
            float value = 0.0f;
            DOMMY_RETURN_IF_FAILURE(sampleRamp(centerDist, value));
            
            MPoint fintPt;
            if (!dommy::maya::checkedPointBlend(pt, targetPt, value, fintPt,
                    dommy::maya::PointBlendRounding::LegacyDisplacement)) {
                return MS::kInvalidParameter;
            }
            return vertex.setPosition(fintPt);
        }));
        
        DOMMY_RETURN_IF_FAILURE(rightMeshFn.updateSurface());
        
        
        
        
        
        
        
        //Output
        MDataHandle leftOutHandle = data.outputValue(outLeftMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MDataHandle rightOutHandle = data.outputValue(outRightMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        // Acquire both handles before publishing either output. Maya setters
        // themselves do not provide an atomic two-output transaction.
        DOMMY_RETURN_IF_FAILURE(leftOutHandle.set(newLeftMeshData));
        DOMMY_RETURN_IF_FAILURE(rightOutHandle.set(newRightMeshData));
        
        
        leftOutHandle.setClean();
        rightOutHandle.setClean();
        
        return data.setClean(plug);
        
        
    }
    
    return MS::kUnknownParameter;
    
}
DOMMY_COMPUTE_CATCH("DBSMirrorMesh")



MStatus DBSMirrorMesh::initialize() try
{
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnEnumAttribute enumFn;
    
    
    
    inBaseMesh = gAttr.create( "inBaseMesh", "inbm",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inBaseMesh));
    
    
    inTargetMesh = gAttr.create( "inTargetMesh", "intm",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetMesh));
    
    
    
    
    aDistance = nAttr.create( "Distance", "dst", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 20.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));
    
    
    
    
    
    aBlendRamp = MRampAttribute::createCurveRamp("BlendRamp", "bldrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlendRamp));
    
    
    
    
    
    
    outLeftMesh = tAttr.create("outLeftMesh", "olm", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outLeftMesh));
    
    outRightMesh = tAttr.create("outRightMesh", "orm", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outRightMesh));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outLeftMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outLeftMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aDistance, outLeftMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBlendRamp, outLeftMesh ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outRightMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outRightMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aDistance, outRightMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aBlendRamp, outRightMesh ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSMirrorMesh", "initialize", std::current_exception());
}
