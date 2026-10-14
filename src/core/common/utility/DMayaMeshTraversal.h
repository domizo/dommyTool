#pragma once

#include "DGenerationLimits.h"

#include <maya/MFnMesh.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MStatus.h>

namespace dommy {
namespace maya {

// The caller supplies a fresh, whole-mesh iterator. Bound even a successful
// next() that fails to advance, and reject both early and late termination.
template <typename Iterator, typename Visitor>
MStatus visitMeshElements(Iterator& iterator, std::size_t expectedCount, Visitor visit)
{
    if (expectedCount > generation::kMaxWorkItemCount)
        return MS::kInvalidParameter;
    MStatus status;
    std::size_t visited = 0U;
    for (;;) {
        const bool done = iterator.isDone(&status);
        if (!status) return status;
        if (done) return visited == expectedCount ? MS::kSuccess : MS::kFailure;
        if (visited >= expectedCount) return MS::kFailure;
        status = visit(iterator);
        if (!status) return status;
        ++visited;
        status = iterator.next();
        if (!status) return status;
    }
}

template <typename Visitor>
MStatus visitMeshPolygons(MItMeshPolygon& iterator, std::size_t expectedCount, Visitor visit)
{
    if (expectedCount > generation::kMaxGeneratedFaceCount) return MS::kInvalidParameter;
    return visitMeshElements(iterator, expectedCount, visit);
}

// Query scalar counts before getTriangles creates its arrays. Do not estimate
// n-gon triangulation using n-2: holes and host triangulation need real counts.
// This limits plug-in-owned arrays, not Maya's internal triangulation cache.
inline MStatus preflightMeshTriangles(MFnMesh& mesh, const MObject& object,
                                     std::size_t& faceCount, std::size_t& triangleCount)
{
    MStatus status;
    const int rawCount = mesh.numPolygons(&status);
    if (!status) return status;
    std::size_t faces = 0U, triangles = 0U;
    if (!generation::checkedCount(rawCount, generation::kMaxGeneratedFaceCount, faces))
        return MS::kInvalidParameter;
    MItMeshPolygon iterator(object, &status);
    if (!status) return status;
    status = visitMeshPolygons(iterator, faces, [&](MItMeshPolygon& polygon) -> MStatus {
        int count = 0;
        const MStatus queried = polygon.numTriangles(count);
        if (!queried) return queried;
        if (count < 0 || !generation::checkedSum(
                triangles, static_cast<std::size_t>(count),
                generation::kMaxGeneratedFaceCount, triangles))
            return MS::kInvalidParameter;
        return MS::kSuccess;
    });
    if (!status) return status;
    faceCount = faces;
    triangleCount = triangles;
    return MS::kSuccess;
}

// Match bulk reads against scalar preflight, including whole missing triangles
// (length % 3 alone cannot detect them). No geometry is copied or allocated.
template <typename PointArray, typename IntArray>
bool validMeshTriangleArrays(const PointArray& points, const IntArray& faceTriangles,
                             const IntArray& vertices, std::size_t expectedPoints,
                             std::size_t expectedFaces, std::size_t expectedTriangles)
{
    std::size_t expectedVertices = 0U;
    if (expectedPoints > generation::kMaxGeneratedPointCount ||
        expectedFaces > generation::kMaxGeneratedFaceCount ||
        expectedTriangles > generation::kMaxGeneratedFaceCount ||
        !generation::checkedProduct(expectedTriangles, 3U, generation::kMaxWorkItemCount,
                                     expectedVertices) ||
        points.length() != expectedPoints || faceTriangles.length() != expectedFaces ||
        vertices.length() != expectedVertices) return false;
    std::size_t triangles = 0U;
    for (unsigned int face = 0U; face < faceTriangles.length(); ++face)
        if (faceTriangles[face] < 0 || !generation::checkedSum(
                triangles, static_cast<std::size_t>(faceTriangles[face]),
                expectedTriangles, triangles)) return false;
    if (triangles != expectedTriangles) return false;
    for (unsigned int i = 0U; i < vertices.length(); ++i)
        if (vertices[i] < 0 || static_cast<std::size_t>(vertices[i]) >= expectedPoints)
            return false;
    return true;
}

} // namespace maya
} // namespace dommy
