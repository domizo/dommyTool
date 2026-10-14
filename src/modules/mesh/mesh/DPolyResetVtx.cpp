//
//  DPolyResetVtx.cpp
//  dommyTool
//
//  Created by yuzo on 2018/05/25.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolyResetVtx.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DMayaMeshInput.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "general_ids.h"

#include <cmath>
#include <cstddef>




const MTypeId DPolyResetVtx::typeId( ID_MESH_RESET_VTX );

MObject DPolyResetVtx::inBaseMesh;
MObject DPolyResetVtx::inTargetMesh;

MObject DPolyResetVtx::inMaskMesh;
MObject DPolyResetVtx::aWeight;

MObject DPolyResetVtx::outMesh;



void *DPolyResetVtx::creator() {
    return ::dommy::maya::createObject("DPolyResetVtx", [&]() {
        return new DPolyResetVtx();
    });
}




MStatus DPolyResetVtx::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle weightHandle = data.inputValue(aWeight, &status);
    if (!status) return status;
    const double weight = weightHandle.asDouble();
    if (!std::isfinite(weight)) return MS::kInvalidParameter;

    dommy::maya::MeshPairInput meshes;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMatchingMeshPair(
        thisMObject(), inBaseMesh, inTargetMesh, data, meshes));

    // Keep the historical required mask connection. A mask without colors
    // has unit influence, so the global Weight still controls the blend.
    MObject maskObject;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getConnectedMeshObject(
        thisMObject(), inMaskMesh, data, maskObject));
    MFnMesh maskMesh(maskObject, &status);
    if (!status) return status;
    const int colorCount = maskMesh.numColors(&status);
    if (!status) return status;
    if (colorCount < 0) return MS::kInvalidParameter;
    const bool hasMaskColors = colorCount != 0;
    MColorArray maskColors;
    if (hasMaskColors) {
        const int maskPointCount = maskMesh.numVertices(&status);
        if (!status) return status;
        if (maskPointCount < 0 ||
            static_cast<std::size_t>(maskPointCount) != meshes.pointCount) {
            return MS::kInvalidParameter;
        }
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPerVertexColors(
            maskMesh, meshes.pointCount, maskColors));
    }

    MPointArray resultPoints;
    DOMMY_RETURN_IF_FAILURE(resultPoints.setLength(
        static_cast<unsigned int>(meshes.pointCount)));
    for (unsigned int index = 0U; index < resultPoints.length(); ++index) {
        const double maskWeight = hasMaskColors
            ? static_cast<double>(maskColors[index].r) : 1.0;
        MPoint result;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::attribute::checkedMaskedLinearBlend(
                    meshes.firstPoints[index][axis],
                    meshes.secondPoints[index][axis],
                    weight, maskWeight, result[axis])) {
                return MS::kInvalidParameter;
            }
        }
        resultPoints[index] = result;
    }

    MFnMeshData outputCreator;
    MObject outputObject = outputCreator.create(&status);
    if (!status) return status;
    if (outputObject.isNull()) return MS::kFailure;
    MFnMesh outputMesh;
    const MObject copiedMesh = outputMesh.copy(
        meshes.firstObject, outputObject, &status);
    if (!status) return status;
    if (copiedMesh.isNull()) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(
        outputMesh.setPoints(resultPoints, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(outputMesh.updateSurface());

    MDataHandle output = data.outputValue(outMesh, &status);
    if (!status) return status;
    DOMMY_RETURN_IF_FAILURE(output.set(outputObject));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPolyResetVtx")
        
        
        






MStatus DPolyResetVtx::initialize() try
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
    
    inMaskMesh = gAttr.create( "inMaskMesh", "inmm",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMaskMesh));
    
    
    
    aWeight = nAttr.create( "Weight", "w", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));
    
    
    
    outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inBaseMesh, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inTargetMesh, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inMaskMesh, outMesh ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aWeight, outMesh ) );
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyResetVtx", "initialize", std::current_exception());
}
