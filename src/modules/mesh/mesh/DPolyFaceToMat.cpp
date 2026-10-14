//
//  DPolyFaceToMat.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolyFaceToMat.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPolyComponentFrame.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DPointMean.h"

#include "general_ids.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>

namespace {

MStatus buildFaceFrame(const MObject& meshObject,
                       int faceIndex,
                       dommy::maya::PolyComponentFrame& frame) {
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (!status) return status;

    const int faceCount = mesh.numPolygons(&status);
    if (!status) return status;
    if (faceIndex < 0 || faceIndex >= faceCount) {
        MGlobal::displayError(
            "DPolyFaceToMat: FaceNumber is outside the mesh face range.");
        return MS::kInvalidParameter;
    }

    const int meshVertexCount = mesh.numVertices(&status);
    if (!status) return status;
    if (meshVertexCount <= 0) return MS::kInvalidParameter;

    const int rawVertexCount = mesh.polygonVertexCount(faceIndex, &status);
    if (!status) return status;
    std::size_t vertexCount = 0U;
    if (rawVertexCount < 3 ||
        !dommy::generation::checkedCount(rawVertexCount,
            dommy::generation::kMaxGeneratedPointCount, vertexCount) ||
        !dommy::mesh::validComponentFrameStorage(
            vertexCount, sizeof(MPoint) + sizeof(int), 4U)) {
        MGlobal::displayError(
            "DPolyFaceToMat: the requested face exceeds the component budget or is degenerate.");
        return MS::kInvalidParameter;
    }

    MIntArray vertexIds;
    status = mesh.getPolygonVertices(faceIndex, vertexIds);
    if (!status) return status;
    if (vertexIds.length() != vertexCount) return MS::kFailure;

    // Validate all IDs before issuing point queries or allocating point storage.
    for (unsigned int index = 0U; index < vertexIds.length(); ++index) {
        if (vertexIds[index] < 0 || vertexIds[index] >= meshVertexCount)
            return MS::kFailure;
    }

    MPointArray points;
    status = points.setLength(static_cast<unsigned int>(vertexCount));
    if (!status) return status;
    if (points.length() != vertexCount) return MS::kFailure;
    std::array<dommy::numeric::detail::PointMeanAccumulator, 3> sums;
    for (unsigned int index = 0U; index < vertexIds.length(); ++index) {
        status = dommy::maya::getPolyComponentPoint(mesh, vertexIds[index], points[index]);
        if (!status) return status;
        const MPoint& point = points[index];
        if (!sums[0].add(point.x) || !sums[1].add(point.y) || !sums[2].add(point.z))
            return MS::kInvalidParameter;
    }
    MPoint center(0.0, 0.0, 0.0, 1.0);
    const std::uint32_t divisor = static_cast<std::uint32_t>(vertexCount);
    if (!sums[0].mean(divisor, center.x) || !sums[1].mean(divisor, center.y) ||
        !sums[2].mean(divisor, center.z)) return MS::kFailure;

    MVector normal;
    status = mesh.getPolygonNormal(faceIndex, normal, MSpace::kObject);
    if (!status) return status;

    MVector tangent;
    bool foundTangent = false;
    if (vertexIds.length() >= 4U) {
        const MPoint& point0 = points[0];
        const MPoint& point1 = points[1];
        const MPoint& point2 = points[2];
        const MPoint& point3 = points[3];
        const MPoint midpoint01 = point1 + (point0 - point1) * 0.5;
        const MPoint midpoint23 = point2 + (point3 - point2) * 0.5;
        tangent = midpoint23 - midpoint01;
        MVector normalized;
        foundTangent = dommy::maya::normalizedVector(tangent, normalized);
    }

    if (!foundTangent) {
        for (unsigned int index = 0U; index < vertexIds.length(); ++index) {
            const MPoint& point = points[index];
            tangent = point - center;
            MVector normalized;
            if (dommy::maya::normalizedVector(tangent, normalized)) {
                foundTangent = true;
                break;
            }
        }
    }

    if (!foundTangent) {
        MGlobal::displayError(
            "DPolyFaceToMat: the requested face has no stable tangent.");
        return MS::kInvalidParameter;
    }

    status = dommy::maya::buildPolyComponentFrame(
        center, normal, tangent, frame);
    if (status == MS::kInvalidParameter) {
        MGlobal::displayError(
            "DPolyFaceToMat: the requested face has no stable frame.");
    }
    return status;
}

} // namespace

const MTypeId DPolyFaceToMat::typeId(ID_MESH_POLY_FACE_TO_MAT);

MObject DPolyFaceToMat::inMesh;
MObject DPolyFaceToMat::aFaceNumber;
MObject DPolyFaceToMat::outMatrix;
MObject DPolyFaceToMat::outTranslate;
MObject DPolyFaceToMat::outRotate;

void* DPolyFaceToMat::creator() {
    return ::dommy::maya::createObject("DPolyFaceToMat", [&]() {
        return new DPolyFaceToMat();
    });
}

MStatus DPolyFaceToMat::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outMatrix && plug != outTranslate && plug != outRotate) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle faceHandle = data.inputValue(aFaceNumber, &status);
    if (!status) return status;
    const int faceIndex = faceHandle.asInt();

    const MDataHandle meshHandle = data.inputValue(inMesh, &status);
    if (!status) return status;
    if (meshHandle.type() != MFnData::kMesh) {
        MGlobal::displayError(
            "DPolyFaceToMat: inMesh does not contain mesh data.");
        return MS::kInvalidParameter;
    }

    const MObject meshObject = meshHandle.asMesh();
    if (meshObject.isNull()) {
        MGlobal::displayError("DPolyFaceToMat: inMesh is empty.");
        return MS::kInvalidParameter;
    }

    dommy::maya::PolyComponentFrame frame;
    status = buildFaceFrame(meshObject, faceIndex, frame);
    if (!status) return status;

    return dommy::maya::publishPolyComponentFrame(
        data, plug, outMatrix, outTranslate, outRotate, frame);
}
DOMMY_COMPUTE_CATCH("DPolyFaceToMat")

MStatus DPolyFaceToMat::initialize() try {
    MStatus status;
    MFnGenericAttribute genericAttribute;
    MFnNumericAttribute numericAttribute;

    inMesh = genericAttribute.create("inMesh", "inm", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    aFaceNumber = numericAttribute.create(
        "FaceNumber", "fnum", MFnNumericData::kInt, 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceNumber));

    outMatrix = genericAttribute.create("outMatrix", "omat", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addDataAccept(MFnData::kMatrix));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMatrix));

    outTranslate =
        genericAttribute.create("outTranslate", "ot", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addNumericDataAccept(MFnNumericData::k3Double));
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addNumericDataAccept(MFnNumericData::k3Float));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outTranslate));

    outRotate = genericAttribute.create("outRotate", "or", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addNumericDataAccept(MFnNumericData::k3Double));
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addNumericDataAccept(MFnNumericData::k3Float));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outRotate));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outMatrix));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFaceNumber, outMatrix));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFaceNumber, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outRotate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFaceNumber, outRotate));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyFaceToMat", "initialize", std::current_exception());
}
