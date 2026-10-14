//
//  DSkinInfo.cpp
//  dommyTool
//
//  Created by yuzo on 2018/04/17.
//

#include "common/utility/DMayaMeshColorOutput.h"
#include "DSkinInfo.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaMeshInput.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "general_ids.h"

#include <cmath>



const MTypeId DSkinInfo::typeId( ID_MESH_SKIN_INFO );

MObject DSkinInfo::inBaseMesh;
MObject DSkinInfo::inTargetMesh;

MObject DSkinInfo::outMesh;


void *DSkinInfo::creator() {
    return ::dommy::maya::createObject("DSkinInfo", [&]() {
        return new DSkinInfo();
    });
}




MStatus DSkinInfo::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;

    
    
    
    if ( plug == outMesh )
    {
        
        dommy::maya::MeshPairInput meshes;
        stat = dommy::maya::getMatchingMeshPair(
            thisMObject(), inBaseMesh, inTargetMesh, data, meshes);
        if (!stat) {
            EchoWarning(name().asChar(),
                        "Base and target meshes must contain matching finite point arrays.");
            return stat;
        }

        MFnMesh targetMeshFn(meshes.secondObject, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
        
        
#pragma mark calc
        
        
        //outputData
        MFnMeshData dataCreator;
        
        MObject newMeshObj = dataCreator.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (newMeshObj.isNull()) return MS::kFailure;

        MFnMesh newMeshFn;
        const MObject copiedMesh =
            newMeshFn.copy(meshes.secondObject, newMeshObj, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (copiedMesh.isNull()) return MS::kFailure;

        const MPointArray& basePoints = meshes.firstPoints;
        const MPointArray& deformPoints = meshes.secondPoints;
        
        
        
        
        
        MDoubleArray distArray;
        DOMMY_RETURN_IF_FAILURE(distArray.setLength(basePoints.length()));
        if (distArray.length() != basePoints.length()) return MS::kFailure;
        double maxDist = 0.0;
        
        for( size_t i=0, N=basePoints.length(); i<N; ++i) {
            
            const double dist = deformPoints[i].distanceTo(basePoints[i]);
            if (!std::isfinite(dist)) return MS::kInvalidParameter;
            distArray[i] = dist;
//            if(VERBOSE) cout << "dist:" << dist << endl;
            
            
            if (maxDist < dist) {
                maxDist = dist;
            }
        }
        
        
        
        
        
        
        //create color
        MColorArray colorDataArray;
        DOMMY_RETURN_IF_FAILURE(colorDataArray.setLength(basePoints.length()));
        if (colorDataArray.length() != basePoints.length()) return MS::kFailure;
        
        for (size_t i=0, N=basePoints.length(); i<N; ++i) {
            colorDataArray[i] = MColor(0, 0, 0, 1);
            if (distArray[i]>0.0) {
                const float colRatio = static_cast<float>(distArray[i] / maxDist);
                colorDataArray[i] = MColor(colRatio,colRatio,colRatio,1.0f);
            }
        }
        
        
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setMeshDataVertexColors(
            newMeshFn, colorDataArray, MString("skinInfo")));

#pragma mark Output
        MDataHandle outHandle = data.outputValue(outMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(outHandle.set(newMeshObj));
        
        outHandle.setClean();
        return data.setClean(plug);
        
        
        
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DSkinInfo")

        




MStatus DSkinInfo::initialize() try
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
    
    
    
    
    
    
    outMesh = tAttr.create("outMesh", "om", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outMesh ) );
    
    return MS::kSuccess;
}


catch (...) {
    return dommy::maya::reportEntryPointException(
        "DSkinInfo", "initialize", std::current_exception());
}
