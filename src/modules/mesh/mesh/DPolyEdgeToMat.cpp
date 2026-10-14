//
//  DPolyEdgeToMat.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolyEdgeToMat.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPolyComponentFrame.h"
#include "common/utility/DMeshInputValidation.h"

#include "general_ids.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshEdge.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>

namespace {

MStatus buildEdgeFrame(MObject meshObject,
                       int edgeIndex,
                       dommy::maya::PolyComponentFrame& frame) {
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (!status) return status;

    const int edgeCount = mesh.numEdges(&status);
    if (!status) return status;
    if (edgeIndex < 0 || edgeIndex >= edgeCount) {
        MGlobal::displayError(
            "DPolyEdgeToMat: EdgeNumber is outside the mesh edge range.");
        return MS::kInvalidParameter;
    }

    const int meshVertexCount = mesh.numVertices(&status);
    if (!status) return status;
    if (meshVertexCount <= 0) return MS::kInvalidParameter;

    MItMeshEdge edge(meshObject, &status);
    if (!status) return status;
    int previousIndex = 0;
    status = edge.setIndex(edgeIndex, previousIndex);
    if (!status) return status;

    const int vertex0 = edge.index(0, &status);
    if (!status) return status;
    const int vertex1 = edge.index(1, &status);
    if (!status) return status;
    if (!dommy::mesh::validEdgeVertexIndices(
            vertex0, vertex1, static_cast<std::size_t>(meshVertexCount))) {
        return MS::kFailure;
    }

    int rawConnectedFaceCount = 0;
    status = edge.numConnectedFaces(rawConnectedFaceCount);
    if (!status) return status;
    std::size_t connectedFaceCount = 0U;
    if (!dommy::generation::checkedCount(rawConnectedFaceCount,
            dommy::generation::kMaxGeneratedFaceCount, connectedFaceCount) ||
        !dommy::mesh::validComponentFrameStorage(connectedFaceCount, sizeof(int), 1U))
        return MS::kInvalidParameter;

    const int faceCount = mesh.numPolygons(&status);
    if (!status) return status;
    if (faceCount < 0 || connectedFaceCount > static_cast<std::size_t>(faceCount))
        return MS::kFailure;

    MPoint point0, point1;
    status = dommy::maya::getPolyComponentPoint(mesh, vertex0, point0);
    if (!status) return status;
    status = dommy::maya::getPolyComponentPoint(mesh, vertex1, point1);
    if (!status) return status;

    const MPoint center = point0 + (point1 - point0) * 0.5;
    const MVector tangent = point1 - point0;

    MIntArray faceIds;
    const int returnedFaceCount = edge.getConnectedFaces(faceIds, &status);
    if (!status) return status;
    if (returnedFaceCount != rawConnectedFaceCount || faceIds.length() != connectedFaceCount)
        return MS::kFailure;

    MVector normal(0.0, 0.0, 0.0);
    for (unsigned int index = 0U; index < faceIds.length(); ++index) {
        const int faceId = faceIds[index];
        if (faceId < 0 || faceId >= faceCount) return MS::kFailure;

        MVector faceNormal;
        status = mesh.getPolygonNormal(
            faceId, faceNormal, MSpace::kObject);
        if (!status) return status;
        if (!dommy::maya::isFiniteVector(faceNormal)) {
            return MS::kInvalidParameter;
        }
        normal += (faceNormal - normal) /
                  static_cast<double>(index + 1U);
        if (!dommy::maya::isFiniteVector(normal)) return MS::kFailure;
    }

    status = dommy::maya::buildPolyComponentFrame(
        center, normal, tangent, frame);
    if (status == MS::kInvalidParameter) {
        MGlobal::displayError(
            "DPolyEdgeToMat: the requested edge has no stable frame.");
    }
    return status;
}

} // namespace

const MTypeId DPolyEdgeToMat::typeId(ID_MESH_POLY_EDEG_TO_MAT);

MObject DPolyEdgeToMat::inMesh;
MObject DPolyEdgeToMat::aEdgeNumber;
MObject DPolyEdgeToMat::outMatrix;
MObject DPolyEdgeToMat::outTranslate;
MObject DPolyEdgeToMat::outRotate;

void* DPolyEdgeToMat::creator() {
    return ::dommy::maya::createObject("DPolyEdgeToMat", [&]() {
        return new DPolyEdgeToMat();
    });
}

MStatus DPolyEdgeToMat::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outMatrix && plug != outTranslate && plug != outRotate) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle edgeHandle = data.inputValue(aEdgeNumber, &status);
    if (!status) return status;
    const int edgeIndex = edgeHandle.asInt();

    const MDataHandle meshHandle = data.inputValue(inMesh, &status);
    if (!status) return status;
    if (meshHandle.type() != MFnData::kMesh) {
        MGlobal::displayError(
            "DPolyEdgeToMat: inMesh does not contain mesh data.");
        return MS::kInvalidParameter;
    }

    const MObject meshObject = meshHandle.asMesh();
    if (meshObject.isNull()) {
        MGlobal::displayError("DPolyEdgeToMat: inMesh is empty.");
        return MS::kInvalidParameter;
    }

    dommy::maya::PolyComponentFrame frame;
    status = buildEdgeFrame(meshObject, edgeIndex, frame);
    if (!status) return status;

    return dommy::maya::publishPolyComponentFrame(
        data, plug, outMatrix, outTranslate, outRotate, frame);
}
DOMMY_COMPUTE_CATCH("DPolyEdgeToMat")

MStatus DPolyEdgeToMat::initialize() try {
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

    aEdgeNumber = numericAttribute.create(
        "EdgeNumber", "enum", MFnNumericData::kInt, 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeNumber));

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
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeNumber, outMatrix));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeNumber, outTranslate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outRotate));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeNumber, outRotate));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyEdgeToMat", "initialize", std::current_exception());
}
