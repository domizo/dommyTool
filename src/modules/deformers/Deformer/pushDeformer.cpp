#include "pushDeformer.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DMayaDeformerGeometry.h"
#include "DGlobalTex.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MColor.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnData.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>
#include <maya/MMatrix.h>
#include <maya/MPlugArray.h>
#include <maya/MString.h>

#include <cmath>

MTypeId pushDeformer::typeId(0x0033A);
MString pushDeformer::typeName("push");

MObject pushDeformer::amplitude;
MObject pushDeformer::ampColor;
MObject pushDeformer::cameraMatrix;

void* pushDeformer::creator() {
    return ::dommy::maya::createObject("pushDeformer", [&]() {
        return new pushDeformer();
    });
}

MStatus pushDeformer::deform(MDataBlock& block, MItGeometry& iter, const MMatrix& localToWorld,
                             unsigned int geomIndex) try {
    (void)localToWorld;
    MStatus status;

    MDataHandle envHandle = block.inputValue(envelope, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0f) {
        return MS::kSuccess;
    }

    MDataHandle amplitudeValueHandle = block.inputValue(amplitude, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double amplitudeValue = amplitudeValueHandle.asDouble();
    if (!std::isfinite(amplitudeValue)) {
        return MS::kFailure;
    }

    MDataHandle cameraValueHandle = block.inputValue(cameraMatrix, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MMatrix cameraValue = cameraValueHandle.asMatrix();
    for (unsigned int row = 0; row < 4; ++row) {
        for (unsigned int column = 0; column < 4; ++column) {
            if (!std::isfinite(cameraValue[row][column])) {
                return MS::kFailure;
            }
        }
    }

    MString sourcePlugName;
    MPlug colorPlug(thisMObject(), ampColor);
    const bool textureConnected = colorPlug.isConnected(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (textureConnected) {
        MPlugArray sourcePlugs;
        colorPlug.connectedTo(sourcePlugs, true, false, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (sourcePlugs.length() != 1U)
            return MS::kInvalidParameter;
        sourcePlugName = sourcePlugs[0].name(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (sourcePlugName.length() == 0U)
            return MS::kInvalidParameter;
    }

    MArrayDataHandle inputHandle = block.inputArrayValue(input, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(inputHandle.jumpToElement(geomIndex));
    MDataHandle geometryHandleElement = inputHandle.inputValue(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle geometryHandle = geometryHandleElement.child(inputGeom);
    if (geometryHandle.type() != MFnData::kMesh) {
        return MS::kSuccess;
    }

    MFnMesh meshFn(geometryHandle.data(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int vertexCount = meshFn.numVertices(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (vertexCount < 0 ||
        static_cast<std::size_t>(vertexCount) > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    MFloatPointArray points;
    MFloatVectorArray normals;
    DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(points, MSpace::kObject));
    DOMMY_RETURN_IF_FAILURE(meshFn.getVertexNormals(true, normals, MSpace::kObject));
    if (points.length() != static_cast<unsigned int>(vertexCount) ||
        normals.length() != points.length()) {
        return MS::kFailure;
    }

    MFloatArray uCoordinates;
    MFloatArray vCoordinates;
    DOMMY_RETURN_IF_FAILURE(uCoordinates.setLength(points.length()));
    DOMMY_RETURN_IF_FAILURE(vCoordinates.setLength(points.length()));
    for (unsigned int index = 0; index < points.length(); ++index) {
        DOMMY_RETURN_IF_FAILURE(uCoordinates.set(0.0f, index));
        DOMMY_RETURN_IF_FAILURE(vCoordinates.set(0.0f, index));
    }

    MObject meshObject = geometryHandle.data();
    MItMeshVertex vertexIt(meshObject, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    for (;;) {
        const bool done = vertexIt.isDone(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (done)
            break;
        float2 uv = {0.0f, 0.0f};
        const MStatus uvStatus = vertexIt.getUV(uv);
        // Missing UVs retain the historical (0,0) fallback. Iterator failures
        // are not equivalent to missing UVs and must stop evaluation.
        const int vertexIndex = vertexIt.index(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (vertexIndex < 0 || static_cast<unsigned int>(vertexIndex) >= points.length())
            return MS::kInvalidParameter;
        if (uvStatus && vertexIndex >= 0 &&
            static_cast<unsigned int>(vertexIndex) < points.length()) {
            DOMMY_RETURN_IF_FAILURE(
                uCoordinates.set(uv[0], static_cast<unsigned int>(vertexIndex)));
            DOMMY_RETURN_IF_FAILURE(
                vCoordinates.set(uv[1], static_cast<unsigned int>(vertexIndex)));
        }
        DOMMY_RETURN_IF_FAILURE(vertexIt.next());
    }

    MFloatVectorArray colors;
    MFloatVectorArray transparencies;
    if (sourcePlugName.length() > 0) {
        const MFloatMatrix camera(cameraValue.matrix);
        status = sampleShadingNetworkInBatches(sourcePlugName, false, false, camera, points,
                                               &uCoordinates, &vCoordinates, &normals, &points,
                                               nullptr, nullptr, nullptr, colors, transparencies);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int pointIndex, const MPoint& point, MPoint& result) -> MStatus {
            const float weight = weightValue(block, geomIndex, pointIndex);
            if (!std::isfinite(weight)) {
                return MS::kFailure;
            }
            if (weight == 0.0f) {
                return MS::kSuccess;
            }

            if (pointIndex >= normals.length()) {
                return MS::kFailure;
            }

            MVector normal(normals[pointIndex]);
            if (!dommy::deformer::normalizeVector(normal))
                return MS::kInvalidParameter;

            double textureWeight = 1.0;
            if (pointIndex < colors.length()) {
                MColor color(colors[pointIndex].x, colors[pointIndex].y, colors[pointIndex].z);
                float hue = 0.0f;
                float saturation = 0.0f;
                float value = 0.0f;
                color.get(MColor::kHSV, hue, saturation, value);
                if (!std::isfinite(value)) {
                    return MS::kFailure;
                }
                textureWeight = value;
            }

            return dommy::deformer::offsetPoint(point, normal, amplitudeValue, textureWeight,
                                                weight, env, 1.0, result)
                       ? MS::kSuccess
                       : MS::kInvalidParameter;
        });
}
DOMMY_DEFORMER_CATCH("pushDeformer")

MStatus pushDeformer::initialize()
try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnMatrixAttribute matrixAttribute;

    amplitude = numericAttribute.create(
        "amplitude", "am", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));

    ampColor = numericAttribute.createColor("ampColor", "ac", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setWritable(true));

    cameraMatrix = matrixAttribute.create(
        "cameraMatrix", "cmat", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setDefault(MMatrix::identity));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setConnectable(true));

    DOMMY_RETURN_IF_FAILURE(addAttribute(amplitude));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ampColor));
    DOMMY_RETURN_IF_FAILURE(addAttribute(cameraMatrix));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(amplitude, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ampColor, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(cameraMatrix, outputGeom));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "pushDeformer", "initialize", std::current_exception());
}
