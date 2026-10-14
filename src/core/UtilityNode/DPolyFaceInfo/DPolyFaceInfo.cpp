//
//  DPolyFaceInfo.cpp
//  dommyTool
//
//  Created by yuzo on 6/1/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolyFaceInfo.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DPointMean.h"

#include "DMayaHeaders.h"
#include "dommyMath.h"

#include <cmath>
#include <cstddef>
#include <array>
#include <vector>

const MTypeId DPolyFaceInfo::typeId(0x4003A);

MObject DPolyFaceInfo::inMesh;
MObject DPolyFaceInfo::faceIndex;
MObject DPolyFaceInfo::outTranslate;
MObject DPolyFaceInfo::outRotate;

void* DPolyFaceInfo::creator() {
    return ::dommy::maya::createObject("DPolyFaceInfo", [&]() {
        return new DPolyFaceInfo();
    });
}

MStatus DPolyFaceInfo::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outTranslate && plug != outRotate) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle inputMeshHandle = data.inputValue(inMesh, &status);
    if (!status)
        return status;

    const MObject inputMeshObject = inputMeshHandle.asMesh();
    if (inputMeshObject.isNull())
        return MS::kInvalidParameter;

    MFnMesh mesh(inputMeshObject, &status);
    if (!status)
        return status;

    const MDataHandle faceIndexHandle = data.inputValue(faceIndex, &status);
    if (!status)
        return status;
    const int requestedFaceIndex = faceIndexHandle.asInt();

    const int faceCount = mesh.numPolygons(&status);
    if (!status)
        return status;
    if (requestedFaceIndex < 0 || requestedFaceIndex >= faceCount) {
        return MS::kInvalidParameter;
    }

    const int vertexCount = mesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int cornerCount = mesh.polygonVertexCount(requestedFaceIndex, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::mesh::MeshMergePlan plan;
    if (vertexCount < 0 || cornerCount < 3 ||
        static_cast<std::size_t>(cornerCount) > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::mesh::appendMeshMergePlan(static_cast<std::size_t>(vertexCount), 1U,
            static_cast<std::size_t>(cornerCount), 0U, plan) ||
        !dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;

    MPointArray points;
    status = mesh.getPoints(points, MSpace::kObject);
    if (!status)
        return status;
    if (points.length() != static_cast<unsigned int>(vertexCount)) return MS::kFailure;

    MIntArray vertexIds;
    status = mesh.getPolygonVertices(requestedFaceIndex, vertexIds);
    if (!status)
        return status;
    if (vertexIds.length() != static_cast<unsigned int>(cornerCount)) {
        MGlobal::displayError("DPolyFaceInfo: incomplete polygon vertex data.");
        return MS::kFailure;
    }

    std::vector<std::array<double, 3>> facePoints;
    facePoints.reserve(vertexIds.length());
    for (unsigned int index = 0U; index < vertexIds.length(); ++index) {
        const int vertexId = vertexIds[index];
        if (vertexId < 0 || static_cast<unsigned int>(vertexId) >= points.length()) {
            return MS::kFailure;
        }

        const MPoint& point = points[static_cast<unsigned int>(vertexId)];
        if (!dommy::maya::isFinitePoint(point))
            return MS::kFailure;

        facePoints.push_back({{point.x, point.y, point.z}});
    }
    // Preserve the arithmetic vertex mean, including cancellation residuals.
    // Admission above bounds both the bulk Maya arrays and this face scratch.
    std::array<double, 3> mean;
    if (!dommy::numeric::checkedPointMean(facePoints, mean)) return MS::kFailure;
    const MPoint center(mean[0], mean[1], mean[2], 1.0);

    MVector faceNormal;
    status = mesh.getPolygonNormal(requestedFaceIndex, faceNormal, MSpace::kObject);
    if (!status)
        return status;

    MVector normalizedNormal;
    if (!dommy::maya::normalizedVector(faceNormal, normalizedNormal)) {
        MGlobal::displayError("DPolyFaceInfo: the requested face has no stable normal.");
        return MS::kFailure;
    }

    MVector normalizedTangent;
    bool foundTangent = false;
    for (unsigned int index = 0U; index < vertexIds.length(); ++index) {
        const MPoint& point = points[static_cast<unsigned int>(vertexIds[index])];
        MVector tangent = point - center;
        tangent -= normalizedNormal * (tangent * normalizedNormal);
        if (dommy::maya::normalizedVector(tangent, normalizedTangent)) {
            foundTangent = true;
            break;
        }
    }
    if (!foundTangent) {
        MGlobal::displayError("DPolyFaceInfo: the requested face has no stable tangent.");
        return MS::kFailure;
    }

    const MVector xAxis(1.0, 0.0, 0.0);
    const MVector yAxis(0.0, 1.0, 0.0);
    const MQuaternion alignNormal = yAxis.rotateTo(normalizedNormal);
    const MVector rotatedXAxis = xAxis.rotateBy(alignNormal);
    if (!dommy::maya::isFiniteVector(rotatedXAxis))
        return MS::kFailure;

    const MQuaternion alignTangent = rotatedXAxis.rotateTo(normalizedTangent);
    MTransformationMatrix transform;
    transform.rotateBy(alignNormal, MSpace::kWorld, &status);
    if (!status)
        return status;
    transform.rotateBy(alignTangent, MSpace::kWorld, &status);
    if (!status)
        return status;

    double rotation[3] = {0.0, 0.0, 0.0};
    MTransformationMatrix::RotationOrder rotationOrder = MTransformationMatrix::kXYZ;
    status = transform.getRotation(rotation, rotationOrder);
    if (!status || !std::isfinite(rotation[0]) || !std::isfinite(rotation[1]) ||
        !std::isfinite(rotation[2])) {
        return status ? MStatus(MS::kFailure) : status;
    }

    const double rotationDegrees[3] = {
        dommy::math::radiansToDegrees(rotation[0]),
        dommy::math::radiansToDegrees(rotation[1]),
        dommy::math::radiansToDegrees(rotation[2])};
    for (const double value : rotationDegrees)
        if (!std::isfinite(value)) return MS::kInvalidParameter;

    // Acquire every fallible output handle before changing either output.
    // Maya's void setters/final clean are not an atomic rollback transaction.
    MDataHandle translateOutput = data.outputValue(outTranslate, &status);
    if (!status)
        return status;
    MDataHandle rotateOutput = data.outputValue(outRotate, &status);
    if (!status)
        return status;
    translateOutput.set3Double(center.x, center.y, center.z);
    rotateOutput.set3Double(rotationDegrees[0], rotationDegrees[1], rotationDegrees[2]);
    translateOutput.setClean();
    rotateOutput.setClean();

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPolyFaceInfo")

MStatus DPolyFaceInfo::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inMesh = typedAttribute.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    faceIndex = numericAttribute.create("faceIndex", "fidx", MFnNumericData::kLong, 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(faceIndex));

    outTranslate =
        numericAttribute.create("outTranslate", "otrans", MFnNumericData::k3Double, 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outTranslate));

    outRotate = numericAttribute.create("outRotate", "orot", MFnNumericData::k3Double, 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outRotate));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(faceIndex, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outRotate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(faceIndex, outRotate));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyFaceInfo", "initialize", std::current_exception());
}
