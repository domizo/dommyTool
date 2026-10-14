#pragma once

#include "DCheckedSize.h"

#include <algorithm>
#include <cstddef>

namespace dommy {
namespace viewport {

// These limits apply to CPU staging buffers as well as Viewport 2.0 buffers.
// Surface extraction must also check its input work estimate before invoking
// OpenVDB, because trimming the result afterwards cannot prevent the mesher's
// temporary allocations.
const std::size_t kMaxVdbViewportSurfaceWorkItemCount = 4000000U;
const std::size_t kMaxVdbViewportSurfaceVertexCount = 2000000U;
const std::size_t kMaxVdbViewportSurfaceIndexCount = 6000000U;
const std::size_t kMaxVdbViewportWireBoxCount = 100000U;
const std::size_t kMaxViewportVertexCount = 2000000U;
const std::size_t kMaxViewportIndexCount = 6000000U;
const std::size_t kMaxViewportScalarCount = 8000000U;

class GeometryBudget {
public:
    explicit GeometryBudget(std::size_t maximum) : mRemaining(maximum) {}

    std::size_t remaining() const { return mRemaining; }

    std::size_t take(std::size_t requested) {
        const std::size_t granted = std::min(requested, mRemaining);
        mRemaining -= granted;
        return granted;
    }

private:
    std::size_t mRemaining;
};

inline bool checkedInstanceLayout(std::size_t basePoints, std::size_t baseIndices,
                                   std::size_t instances, std::size_t& pointCount,
                                   std::size_t& scalarCount, std::size_t& indexCount)
{
    std::size_t points = 0U, scalars = 0U, indices = 0U;
    if (instances > kMaxViewportVertexCount ||
        !memory::checkedMultiply(basePoints, instances, points) ||
        points > kMaxViewportVertexCount ||
        !memory::checkedMultiply(points, 3U, scalars) ||
        scalars > kMaxViewportScalarCount ||
        !memory::checkedMultiply(baseIndices, instances, indices) ||
        indices > kMaxViewportIndexCount) return false;
    pointCount = points;
    scalarCount = scalars;
    indexCount = indices;
    return true;
}

inline bool checkedWireBoxLayout(std::size_t boxCount,
                                 std::size_t& vertexCount,
                                 std::size_t& componentCount,
                                 std::size_t& indexCount)
{
    if (boxCount > kMaxVdbViewportWireBoxCount ||
        !memory::checkedMultiply(boxCount, 8U, vertexCount) ||
        !memory::checkedMultiply(vertexCount, 3U, componentCount) ||
        !memory::checkedMultiply(boxCount, 24U, indexCount)) {
        return false;
    }
    return true;
}

inline bool checkedSurfaceIndexCount(std::size_t quadCount,
                                     std::size_t triangleCount,
                                     std::size_t maximumIndexCount,
                                     std::size_t& indexCount)
{
    std::size_t quadIndices = 0;
    std::size_t triangleIndices = 0;
    std::size_t total = 0;
    if (!memory::checkedMultiply(quadCount, 6U, quadIndices) ||
        !memory::checkedMultiply(triangleCount, 3U, triangleIndices) ||
        !memory::checkedAdd(quadIndices, triangleIndices, total) ||
        total > maximumIndexCount) {
        return false;
    }
    indexCount = total;
    return true;
}

} // namespace viewport
} // namespace dommy
