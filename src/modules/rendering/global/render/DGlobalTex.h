//
//  DGlobalTex.h
//  dommyTool
//
//  Created by yuzo on 3/2/16.
//
//

#ifndef DGlobalTex_h
#define DGlobalTex_h

#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVectorArray.h>

MStatus sampleShadingNetworkInBatches(
    const MString& shadingNodeName,
    bool useShadowMaps,
    bool reuseMaps,
    const MFloatMatrix& cameraMatrix,
    const MFloatPointArray& points,
    const MFloatArray* uCoordinates,
    const MFloatArray* vCoordinates,
    const MFloatVectorArray* normals,
    const MFloatPointArray* referencePoints,
    const MFloatVectorArray* tangentUs,
    const MFloatVectorArray* tangentVs,
    const MFloatArray* filterSizes,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies);

MStatus evaluate2dTextureInBatches(
    const MObject& textureNode,
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies);

MStatus evaluateDynamics2dTextureInBatches(
    const MObject& node,
    const MObject& textureAttribute,
    const MDoubleArray& uCoordinates,
    const MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies);

MStatus getColorFromTex(
    const MObject& node,
    const MObject& plug,
    MDoubleArray& uCoordinates,
    MDoubleArray& vCoordinates,
    MVectorArray& colors,
    MDoubleArray& transparencies);

MStatus getColorFromShaderNetwork(
    const MObject& node,
    const MObject& plug,
    MFloatPointArray& points,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies);
MStatus getColorFromShaderNetwork(
    const MObject& node,
    const MObject& plug,
    MFloatPointArray& points,
    MDoubleArray& uCoordinates,
    MDoubleArray& vCoordinates,
    MFloatVectorArray& colors,
    MFloatVectorArray& transparencies);


#endif /* DGlobalTex_h */
