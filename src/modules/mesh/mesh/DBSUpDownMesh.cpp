//
//  DBSUpDownMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/21.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DBSUpDownMesh.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaPointBlend.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"



#include "general_ids.h"



const MTypeId DBSUpDownMesh::typeId( ID_MESH_BLEND_SHAPE_UP_DOWN );

MObject DBSUpDownMesh::inBaseMesh;
MObject DBSUpDownMesh::inTargetMesh;

MObject DBSUpDownMesh::aSpace;
MObject DBSUpDownMesh::aInvert;

MObject DBSUpDownMesh::outUpMesh;
MObject DBSUpDownMesh::outDownMesh;



void *DBSUpDownMesh::creator() {
    return ::dommy::maya::createObject("DBSUpDownMesh", [&]() {
        return new DBSUpDownMesh();
    });
}



MStatus DBSUpDownMesh::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;
    //    const bool VERBOSE = false;
    
    if ( plug == outUpMesh || plug == outDownMesh )
    {
        
        
        //get attribute
        
        MDataHandle control = data.inputValue(aInvert, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const bool invert = control.asBool();

        control = data.inputValue(aSpace, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const short space = control.asShort();
        if (space < 0 || space > 1) return MS::kInvalidParameter;
        
        const MSpace::Space spaceType = space==0?MSpace::kObject:MSpace::kWorld;
        
//        cout << "space type:" << spaceType << endl;
        
        
        dommy::maya::MeshPairInput meshes;
        stat = dommy::maya::getMatchingMeshPair(
            thisMObject(), inBaseMesh, inTargetMesh, data, meshes);
        if (!stat) {
            EchoWarning(name().asChar(),
                        "Base and target meshes must contain matching finite point arrays.");
            return stat;
        }

        MFnMesh baseMeshFn(meshes.firstObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        MFnMesh targetMeshFn(meshes.secondObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
        
#pragma mark get Color Map
//        cout << "vertext color num:" << targetMeshFn.numColors() <<endl;
        const int colorCount = targetMeshFn.numColors(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (colorCount < 0) return MS::kInvalidParameter;
        if (colorCount == 0) {
            EchoWarning(name().asChar(), "Not color set in target mesh.");
            return MS::kFailure;
        }
        
        MColorArray weightArray;
        stat = dommy::maya::getPerVertexColors(
            targetMeshFn, meshes.pointCount, weightArray);
        if (!stat) {
            EchoWarning(name().asChar(),
                        "Target mesh colors must contain one finite value per vertex.");
            return stat;
        }
        
//        cout << "weight num:" << weightArray.length() <<endl;
        
        
        
#pragma mark calc
        
        
        //outputData
        MFnMeshData upDataCreator, downDataCreator;
        
        MObject newUpMeshData = upDataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        MObject newDownMeshData = downDataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (newUpMeshData.isNull() || newDownMeshData.isNull()) return MS::kFailure;
        
        MFnMesh upMeshFn, downMeshFn;
        
        //copy base mesh
        MObject copiedMesh = upMeshFn.copy(
            meshes.firstObject, newUpMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (copiedMesh.isNull()) return MS::kFailure;
        copiedMesh = downMeshFn.copy(
            meshes.firstObject, newDownMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (copiedMesh.isNull()) return MS::kFailure;
        
        
        //get mesh data
        MPointArray basePointArray, targetPointArray;
        DOMMY_RETURN_IF_FAILURE(basePointArray.copy(meshes.firstPoints));
        DOMMY_RETURN_IF_FAILURE(targetPointArray.copy(meshes.secondPoints));
        if (spaceType == MSpace::kWorld) {
            stat = baseMeshFn.getPoints(basePointArray, spaceType);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            stat = targetMeshFn.getPoints(targetPointArray, spaceType);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (basePointArray.length() != meshes.pointCount ||
                targetPointArray.length() != meshes.pointCount ||
                !dommy::maya::areFinitePoints(basePointArray) ||
                !dommy::maya::areFinitePoints(targetPointArray)) {
                return MS::kInvalidParameter;
            }
        }
        
        
//        cout << "base point num:" << basePointArray.length() <<endl;
//        cout << "target point num:" << targetPointArray.length() <<endl;
        
        
        //Upper Mesh
        MItMeshVertex upVtxIt(newUpMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(
            upVtxIt, meshes.pointCount, [&](MItMeshVertex& vertex) -> MStatus {
            
            const MPoint pt = vertex.position(spaceType, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);

            const int rawIndex = vertex.index(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!dommy::memory::indexInRange(rawIndex, meshes.pointCount)) {
                return MS::kInvalidParameter;
            }
            const std::size_t index = static_cast<std::size_t>(rawIndex);
            
            double weight = invert?1.0-weightArray[index][0]:weightArray[index][0];
            
            MPoint targetPt = targetPointArray[index];
            
            MPoint fintPt;
            if (!dommy::maya::checkedPointBlend(pt, targetPt, weight, fintPt,
                    dommy::maya::PointBlendRounding::LegacyDisplacement)) {
                return MS::kInvalidParameter;
            }
            return vertex.setPosition(fintPt, spaceType);
        }));
        
        DOMMY_RETURN_IF_FAILURE(upMeshFn.updateSurface());
        
        
        //Right Mesh
        MItMeshVertex downVtxIt(newDownMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(
            downVtxIt, meshes.pointCount, [&](MItMeshVertex& vertex) -> MStatus {
            
            const MPoint pt = vertex.position(spaceType, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);

            const int rawIndex = vertex.index(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (!dommy::memory::indexInRange(rawIndex, meshes.pointCount)) {
                return MS::kInvalidParameter;
            }
            const std::size_t index = static_cast<std::size_t>(rawIndex);
            
            double weight = invert?weightArray[index][0]:1.0-weightArray[index][0];
            
            MPoint targetPt = targetPointArray[index];
            
            MPoint fintPt;
            if (!dommy::maya::checkedPointBlend(pt, targetPt, weight, fintPt,
                    dommy::maya::PointBlendRounding::LegacyDisplacement)) {
                return MS::kInvalidParameter;
            }
            return vertex.setPosition(fintPt, spaceType);
        }));
        
        DOMMY_RETURN_IF_FAILURE(downMeshFn.updateSurface());
        
        
        
        
        
#pragma mark Output
        MDataHandle upOutHandle = data.outputValue(outUpMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MDataHandle downOutHandle = data.outputValue(outDownMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(upOutHandle.set(newUpMeshData));
        DOMMY_RETURN_IF_FAILURE(downOutHandle.set(newDownMeshData));
        
        upOutHandle.setClean();
        downOutHandle.setClean();
        
        return data.setClean(plug);
        
        
        
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBSUpDownMesh")




MStatus DBSUpDownMesh::initialize() try
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
    
    
    
    
    MFnEnumAttribute spaceEnumFn;
    aSpace = spaceEnumFn.create( "Space", "spc", 0,&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(spaceEnumFn.addField( "Object", 0 ));
    DOMMY_RETURN_IF_FAILURE(spaceEnumFn.addField( "World", 1 ));
    DOMMY_RETURN_IF_FAILURE(spaceEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(spaceEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpace));
    
    
    aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));
    
    
    
    outUpMesh = tAttr.create("outUpMesh", "oum", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outUpMesh));
    
    outDownMesh = tAttr.create("outDownMesh", "odm", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDownMesh));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outUpMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outUpMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSpace, outUpMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aInvert, outUpMesh ) );
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outDownMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outDownMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSpace, outDownMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aInvert, outDownMesh ) );
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSUpDownMesh", "initialize", std::current_exception());
}
