#pragma once

#include "DMayaValueValidation.h"
#include "DMayaPointBlend.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnMesh.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>

#include <cmath>

namespace dommy {
namespace maya {

struct PolyComponentFrame {
    MMatrix matrix;
    MVector translation;
    MVector rotation;
};

// Read only a previously range-checked vertex. Normalize homogeneous input
// through the established checked point math before any frame arithmetic.
inline MStatus getPolyComponentPoint(MFnMesh& mesh, int vertexId, MPoint& result) {
    MPoint point;
    const MStatus status = mesh.getPoint(vertexId, point, MSpace::kObject);
    if (!status) return status;
    return checkedPointBlend(point, point, 0.0, result)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

inline MStatus buildPolyComponentFrame(const MPoint& center,
                                       const MVector& sourceNormal,
                                       const MVector& sourceTangent,
                                       PolyComponentFrame& result) {
    if (!isFinitePoint(center)) return MS::kInvalidParameter;

    MVector normal;
    if (!normalizedVector(sourceNormal, normal)) return MS::kInvalidParameter;

    const MVector projectedTangent =
        sourceTangent - normal * (sourceTangent * normal);
    MVector tangent;
    if (!normalizedVector(projectedTangent, tangent)) {
        return MS::kInvalidParameter;
    }

    const MVector xAxis(1.0, 0.0, 0.0);
    const MVector yAxis(0.0, 1.0, 0.0);
    const MQuaternion alignNormal = yAxis.rotateTo(normal);
    if (!isFiniteQuaternion(alignNormal)) return MS::kFailure;

    const MVector rotatedXAxis = xAxis.rotateBy(alignNormal);
    if (!isFiniteVector(rotatedXAxis)) return MS::kFailure;

    const MQuaternion alignTangent = rotatedXAxis.rotateTo(tangent);
    if (!isFiniteQuaternion(alignTangent)) return MS::kFailure;

    MStatus status;
    MTransformationMatrix transform;
    transform.rotateBy(alignNormal, MSpace::kWorld, &status);
    if (!status) return status;
    transform.rotateBy(alignTangent, MSpace::kWorld, &status);
    if (!status) return status;
    status = transform.addTranslation(
        MVector(center.x, center.y, center.z), MSpace::kWorld);
    if (!status) return status;

    double rotation[3] = {0.0, 0.0, 0.0};
    MTransformationMatrix::RotationOrder rotationOrder =
        MTransformationMatrix::kXYZ;
    status = transform.getRotation(rotation, rotationOrder);
    if (!status) return status;
    if (!std::isfinite(rotation[0]) || !std::isfinite(rotation[1]) ||
        !std::isfinite(rotation[2])) {
        return MS::kFailure;
    }

    const MMatrix matrix = transform.asMatrix();
    if (!isFiniteMatrix(matrix)) return MS::kFailure;

    result.matrix = matrix;
    result.translation = MVector(center.x, center.y, center.z);
    result.rotation = MVector(rotation[0], rotation[1], rotation[2]);
    return MS::kSuccess;
}

inline MStatus publishPolyComponentFrame(MDataBlock& data,
                                         const MPlug& requestedPlug,
                                         const MObject& matrixAttribute,
                                         const MObject& translateAttribute,
                                         const MObject& rotateAttribute,
                                         const PolyComponentFrame& frame) {
    if (!isFiniteMatrix(frame.matrix) ||
        !isFiniteVector(frame.translation) ||
        !isFiniteVector(frame.rotation)) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnMatrixData matrixData;
    const MObject matrixObject = matrixData.create(frame.matrix, &status);
    if (!status) return status;

    MFnNumericData translateData;
    const MObject translateObject =
        translateData.create(MFnNumericData::k3Double, &status);
    if (!status) return status;
    status = translateData.setData(
        frame.translation.x, frame.translation.y, frame.translation.z);
    if (!status) return status;

    MFnNumericData rotateData;
    const MObject rotateObject =
        rotateData.create(MFnNumericData::k3Double, &status);
    if (!status) return status;
    status = rotateData.setData(
        frame.rotation.x, frame.rotation.y, frame.rotation.z);
    if (!status) return status;

    MDataHandle matrixOutput = data.outputValue(matrixAttribute, &status);
    if (!status) return status;
    MDataHandle translateOutput = data.outputValue(translateAttribute, &status);
    if (!status) return status;
    MDataHandle rotateOutput = data.outputValue(rotateAttribute, &status);
    if (!status) return status;

    status = matrixOutput.set(matrixObject);
    if (!status) return status;
    status = translateOutput.set(translateObject);
    if (!status) return status;
    status = rotateOutput.set(rotateObject);
    if (!status) return status;

    matrixOutput.setClean();
    translateOutput.setClean();
    rotateOutput.setClean();
    return data.setClean(requestedPlug);
}

} // namespace maya
} // namespace dommy
