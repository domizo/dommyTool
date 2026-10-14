#pragma once

#include "DGenerationLimits.h"
#include <cstddef>

namespace dommy {
namespace mesh {

// Shared non-owning polygon admission for DMesh storage and renderer imports.
// Check every face corner before converting signed Maya indices to UINT.
template <typename Counts, typename Indices>
bool validPolygonArrays(std::size_t points, const Counts& faces,
                        const Indices& indices)
{
    if (points == 0U || points > generation::kMaxGeneratedPointCount ||
        faces.length() == 0U || faces.length() > generation::kMaxGeneratedFaceCount ||
        indices.length() > generation::kMaxWorkItemCount) return false;
    std::size_t connections = 0U;
    for (unsigned int face = 0U; face < faces.length(); ++face) {
        const auto corners = faces[face];
        if (corners < 3 ||
            !generation::checkedSum(connections, static_cast<std::size_t>(corners),
                generation::kMaxWorkItemCount, connections)) return false;
    }
    if (connections != indices.length()) return false;
    for (unsigned int corner = 0U; corner < indices.length(); ++corner) {
        const auto pointIndex = indices[corner];
        if (pointIndex < 0 || static_cast<std::size_t>(pointIndex) >= points)
            return false;
    }
    return true;
}

// Conservative local array admission for the legacy mesh-pair consumers:
// four staged/returned MPoint arrays (128 bytes/point), plus up to 128 bytes
// for color/weight/result arrays or mirrored-search points/index/results.
// Previously retained destination points are additional live storage.
// Not a bound on Maya-owned input caches, copied mesh topology, UV/color sets,
// or undocumented allocator overhead. Do not use this as a process-wide cap.
inline bool validMeshPairStorage(std::size_t points, std::size_t retainedPoints = 0U)
{
    const std::size_t maximum = 256U * 1024U * 1024U;
    std::size_t bytes = 0U, retainedBytes = 0U, total = 0U;
    return points <= generation::kMaxGeneratedPointCount &&
        generation::checkedProduct(points, 256U, maximum, bytes) &&
        generation::checkedProduct(retainedPoints, 32U, maximum, retainedBytes) &&
        generation::checkedSum(bytes, retainedBytes, maximum, total);
}

// Reference points, search-index storage and staged per-query result arrays.
// Geometry-only allowance; Maya-owned input/cache storage is outside this cap.
inline bool validPointSearchStorage(std::size_t referencePoints, std::size_t queryPoints)
{
    const std::size_t maximum = 256U * 1024U * 1024U;
    std::size_t referenceBytes = 0U, queryBytes = 0U, total = 0U;
    return referencePoints <= generation::kMaxGeneratedPointCount &&
        queryPoints <= generation::kMaxGeneratedPointCount &&
        generation::checkedProduct(referencePoints, 128U, maximum, referenceBytes) &&
        generation::checkedProduct(queryPoints, 96U, maximum, queryBytes) &&
        generation::checkedSum(referenceBytes, queryBytes, maximum, total);
}

inline bool validEdgeVertexIndices(int first, int second,
                                  std::size_t pointCount)
{
    return first >= 0 && second >= 0 &&
        static_cast<std::size_t>(first) < pointCount &&
        static_cast<std::size_t>(second) < pointCount;
}

// Component-frame nodes retain only selected points/IDs, not the whole mesh.
// This admission bounds their local arrays and traversal work; Maya-owned
// topology/caches and allocator overhead are outside this local byte budget.
inline bool validComponentFrameStorage(std::size_t count,
                                      std::size_t bytesPerItem,
                                      std::size_t workPerItem)
{
    std::size_t bytes = 0U, work = 0U;
    return count > 0U && count <= generation::kMaxGeneratedPointCount &&
        generation::checkedProduct(count, bytesPerItem,
                                   256U * 1024U * 1024U, bytes) &&
        generation::checkedProduct(count, workPerItem,
                                   generation::kMaxWorkItemCount, work);
}

inline bool checkedMatchingPointCount(std::size_t firstCount,
                                      std::size_t secondCount,
                                      std::size_t maximum,
                                      std::size_t& result)
{
    if (firstCount != secondCount || firstCount > maximum) {
        return false;
    }

    result = firstCount;
    return true;
}

inline bool hasPerPointValues(std::size_t valueCount,
                              std::size_t pointCount)
{
    return valueCount == pointCount;
}

} // namespace mesh
} // namespace dommy
