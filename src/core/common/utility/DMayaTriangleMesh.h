#pragma once

#include "common/utility/DMeshMergePlan.h"
#include "geometry/DGeometryTypes.h"
#include <limits>
#include <type_traits>
#include <vector>

namespace dommy {
namespace maya {

// Append to unpublished staging arrays. On failure the caller must discard
// them; copying the growing mesh for every face would be quadratic in size.
// Admit counts and retained/workspace bytes before the first callback/append.
template <typename PointAt, typename PointArray, typename IntArray>
bool appendTriangleMeshChecked(std::size_t pointCount, PointAt pointAt,
                               const std::vector<geometry::Triangle>& triangles, PointArray& points,
                               IntArray& faces, IntArray& connections,
                               std::size_t extraStorageBytes = 0U) {
    const std::size_t offset = points.length();
    std::size_t nextPoints = 0U, nextFaces = 0U, nextConnections = 0U, addedConnections = 0U;
    if (!generation::checkedSum(offset, pointCount, generation::kMaxGeneratedPointCount,
                                nextPoints) ||
        nextPoints > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !generation::checkedSum(faces.length(), triangles.size(),
                                generation::kMaxGeneratedFaceCount, nextFaces) ||
        !generation::checkedProduct(triangles.size(), 3U, generation::kMaxWorkItemCount,
                                    addedConnections) ||
        !generation::checkedSum(connections.length(), addedConnections,
                                generation::kMaxWorkItemCount, nextConnections))
        return false;

    mesh::MeshMergePlan storage;
    if (!mesh::appendMeshMergePlan(nextPoints, nextFaces, nextConnections, 0U, storage) ||
        !mesh::validMeshOperationStorage(storage, extraStorageBytes)) return false;

    for (const geometry::Triangle& triangle : triangles)
        if (triangle.first >= pointCount || triangle.second >= pointCount ||
            triangle.third >= pointCount)
            return false;

    typedef typename std::remove_reference<decltype(points[0])>::type Point;
    for (std::size_t i = 0U; i < pointCount; ++i) {
        const geometry::Point3 point = pointAt(i);
        if (!geometry::isFinite(point) || !points.append(Point(point.x, point.y, point.z)))
            return false;
    }
    for (const geometry::Triangle& triangle : triangles) {
        if (!faces.append(3) || !connections.append(static_cast<int>(offset + triangle.first)) ||
            !connections.append(static_cast<int>(offset + triangle.second)) ||
            !connections.append(static_cast<int>(offset + triangle.third)))
            return false;
    }
    return points.length() == nextPoints && faces.length() == nextFaces &&
           connections.length() == nextConnections;
}

} // namespace maya
} // namespace dommy
