#pragma once

#include "DGenerationLimits.h"
#include "DMayaValueValidation.h"
#include "DPointTransform.h"
#include "DMeshInputValidation.h"
#include "DSymmetryPointMatcher.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MColorArray.h>
#include <maya/MFnData.h>
#include <maya/MFnMesh.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace dommy {
namespace maya {

struct MeshPairInput {
    MObject firstObject;
    MObject secondObject;
    MPointArray firstPoints;
    MPointArray secondPoints;
    std::size_t pointCount;

    MeshPairInput()
        : firstObject(MObject::kNullObj),
          secondObject(MObject::kNullObj),
          pointCount(0U)
    {
    }
};

inline MStatus getMeshObject(const MObject& attribute,
                            MDataBlock& block,
                            MObject& meshObject)
{
    MStatus status;
    MDataHandle handle = block.inputValue(attribute, &status);
    if (!status) return status;
    if (handle.type() != MFnData::kMesh) return MS::kFailure;

    const MObject candidate = handle.data();
    if (candidate.isNull()) return MS::kFailure;
    MFnMesh mesh(candidate, &status);
    if (!status) return status;
    meshObject = candidate;
    return MS::kSuccess;
}

// Bake only transformation metadata carried by the input connection. The
// returned geometry must not be edited and is queried in kObject thereafter.
// An input with no transform metadata stays in its supplied coordinate frame.
inline MStatus getTransformedMeshObject(const MObject& attribute,
                                       MDataBlock& block,
                                       MObject& meshObject)
{
    MStatus status;
    const MDataHandle handle = block.inputValue(attribute, &status);
    if (!status) return status;
    if (handle.type() != MFnData::kMesh ||
        !numeric::isFiniteAffineTransform(handle.geometryTransformMatrix()))
        return MS::kInvalidParameter;
    const MObject candidate = handle.asMeshTransformed();
    if (candidate.isNull()) return MS::kInvalidParameter;
    MFnMesh mesh(candidate, &status);
    if (!status) return status;
    const int rawCount = mesh.numVertices(&status);
    if (!status) return status;
    std::size_t count = 0U;
    if (!generation::checkedCount(rawCount, generation::kMaxGeneratedPointCount, count) ||
        count == 0U) return MS::kInvalidParameter;
    MPointArray points;
    status = mesh.getPoints(points, MSpace::kObject);
    if (!status) return status;
    if (points.length() != count) return MS::kFailure;
    for (unsigned int i = 0U; i < points.length(); ++i)
        if (!isFloatRepresentablePoint(points[i])) return MS::kInvalidParameter;
    meshObject = candidate;
    return MS::kSuccess;
}

inline MStatus getConnectedMeshObject(const MObject& node,
                                      const MObject& attribute,
                                      MDataBlock& block,
                                      MObject& meshObject)
{
    MStatus status;
    const bool connected = MPlug(node, attribute).isConnected(&status);
    if (!status) return status;
    if (!connected) return MS::kFailure;
    return getMeshObject(attribute, block, meshObject);
}

inline bool areFinitePoints(const MPointArray& points)
{
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!isFinitePoint(points[index])) return false;
    }
    return true;
}

inline MStatus getPerVertexColors(MFnMesh& mesh,
                                  std::size_t pointCount,
                                  MColorArray& colors)
{
    MColorArray candidate;
    const MStatus status = mesh.getVertexColors(candidate);
    if (!status) return status;
    if (!mesh::hasPerPointValues(candidate.length(), pointCount)) {
        return MS::kInvalidParameter;
    }
    for (unsigned int index = 0U; index < candidate.length(); ++index) {
        const MColor& color = candidate[index];
        if (!std::isfinite(color.r) || !std::isfinite(color.g) ||
            !std::isfinite(color.b) || !std::isfinite(color.a)) {
            return MS::kInvalidParameter;
        }
    }
    return colors.copy(candidate);
}

inline MStatus getMatchingMeshPair(const MObject& node,
                                   const MObject& firstAttribute,
                                   const MObject& secondAttribute,
                                   MDataBlock& block,
                                   MeshPairInput& result)
{
    MeshPairInput candidate;
    MStatus status = getConnectedMeshObject(
        node, firstAttribute, block, candidate.firstObject);
    if (!status) return status;
    status = getConnectedMeshObject(
        node, secondAttribute, block, candidate.secondObject);
    if (!status) return status;

    MFnMesh firstMesh(candidate.firstObject, &status);
    if (!status) return status;
    MFnMesh secondMesh(candidate.secondObject, &status);
    if (!status) return status;

    const int firstCount = firstMesh.numVertices(&status);
    if (!status || firstCount < 0) return status ? MStatus(MS::kFailure) : status;
    const int secondCount = secondMesh.numVertices(&status);
    if (!status || secondCount < 0) return status ? MStatus(MS::kFailure) : status;

    std::size_t retainedPoints = 0U;
    if (!mesh::checkedMatchingPointCount(
            static_cast<std::size_t>(firstCount),
            static_cast<std::size_t>(secondCount),
            generation::kMaxGeneratedPointCount,
            candidate.pointCount) ||
        !generation::checkedSum(result.firstPoints.length(), result.secondPoints.length(),
                                generation::kMaxWorkItemCount, retainedPoints) ||
        !mesh::validMeshPairStorage(candidate.pointCount, retainedPoints)) {
        return MS::kInvalidParameter;
    }

    status = firstMesh.getPoints(candidate.firstPoints, MSpace::kObject);
    if (!status) return status;
    status = secondMesh.getPoints(candidate.secondPoints, MSpace::kObject);
    if (!status) return status;
    if (candidate.firstPoints.length() != candidate.pointCount ||
        candidate.secondPoints.length() != candidate.pointCount ||
        !areFinitePoints(candidate.firstPoints) ||
        !areFinitePoints(candidate.secondPoints)) {
        return MS::kInvalidParameter;
    }

    // Array assignment cannot report allocation failure. Callers may consume
    // this output only after a successful status, including both array copies.
    status = result.firstPoints.copy(candidate.firstPoints);
    if (!status) return status;
    status = result.secondPoints.copy(candidate.secondPoints);
    if (!status) return status;
    result.firstObject = candidate.firstObject;
    result.secondObject = candidate.secondObject;
    result.pointCount = candidate.pointCount;
    return MS::kSuccess;
}

inline MStatus getMirroredPointMatches(const MPointArray& points,
                                       double tolerance,
                                       std::vector<std::size_t>& matches)
{
    std::size_t resultSlots = 0U;
    if (!std::isfinite(tolerance) || tolerance < 0.0 ||
        !generation::checkedSum(points.length(), matches.size(),
                                generation::kMaxGeneratedPointCount, resultSlots) ||
        !mesh::validPointSearchStorage(points.length(), resultSlots))
        return MS::kInvalidParameter;
    std::vector<mesh::Point3d> searchPoints;
    searchPoints.reserve(points.length());
    for (unsigned int index = 0U; index < points.length(); ++index) {
        const MPoint& point = points[index];
        searchPoints.push_back(mesh::Point3d(point.x, point.y, point.z));
    }
    return mesh::findMirroredPointMatches(
               searchPoints, tolerance, generation::kMaxGeneratedPointCount,
               matches)
               ? MStatus(MS::kSuccess)
               : MStatus(MS::kInvalidParameter);
}

} // namespace maya
} // namespace dommy
