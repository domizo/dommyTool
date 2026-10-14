#pragma once

#include "DGenerationLimits.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace fluid {

struct GridShape {
    GridShape() : voxelCount(0U)
    {
        dimensions[0] = dimensions[1] = dimensions[2] = 0U;
        origin[0] = origin[1] = origin[2] = 0.0;
    }

    std::size_t dimensions[3];
    std::size_t voxelCount;
    double origin[3];
};

struct GridPlan {
    GridPlan() : totalWorkItems(0U)
    {
        cellSize[0] = cellSize[1] = cellSize[2] = 0.0;
    }

    double cellSize[3];
    GridShape scalar;
    GridShape velocity[3];
    std::size_t totalWorkItems;
};

namespace detail {

inline bool checkedVoxelCount(const std::size_t dimensions[3],
                              std::size_t maximum,
                              std::size_t& voxelCount)
{
    std::size_t xyCount = 0U;
    return generation::checkedProduct(
               dimensions[0], dimensions[1], maximum, xyCount) &&
           generation::checkedProduct(
               xyCount, dimensions[2], maximum, voxelCount);
}

} // namespace detail

// Build all scalar-cell and staggered velocity-grid geometry before a consumer
// exposes a raw Maya array. Scalar values are cell-centered. Velocity component
// values are face-centered and have one extra sample on their respective axis.
inline bool buildGridPlan(unsigned int xResolution,
                          unsigned int yResolution,
                          unsigned int zResolution,
                          double xDimension,
                          double yDimension,
                          double zDimension,
                          double minimumX,
                          double minimumY,
                          double minimumZ,
                          std::size_t scalarGridCount,
                          bool includeVelocity,
                          std::size_t maximumWorkItems,
                          GridPlan& plan)
{
    const unsigned int resolutions[3] = {
        xResolution, yResolution, zResolution};
    const double dimensions[3] = {xDimension, yDimension, zDimension};
    const double minimums[3] = {minimumX, minimumY, minimumZ};

    if (maximumWorkItems == 0U) return false;

    GridPlan candidate;
    for (int axis = 0; axis < 3; ++axis) {
        if (resolutions[axis] == 0U ||
            resolutions[axis] >
                static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
            !std::isfinite(dimensions[axis]) || dimensions[axis] <= 0.0 ||
            !std::isfinite(minimums[axis])) {
            return false;
        }

        candidate.scalar.dimensions[axis] =
            static_cast<std::size_t>(resolutions[axis]);
        candidate.cellSize[axis] =
            dimensions[axis] / static_cast<double>(resolutions[axis]);
        candidate.scalar.origin[axis] =
            minimums[axis] + 0.5 * candidate.cellSize[axis];
        if (!std::isfinite(candidate.cellSize[axis]) ||
            candidate.cellSize[axis] <= 0.0 ||
            !std::isfinite(candidate.scalar.origin[axis])) {
            return false;
        }
    }

    if (!detail::checkedVoxelCount(
            candidate.scalar.dimensions, maximumWorkItems,
            candidate.scalar.voxelCount)) {
        return false;
    }

    std::size_t scalarWorkItems = 0U;
    if (!generation::checkedProduct(
            candidate.scalar.voxelCount, scalarGridCount, maximumWorkItems,
            scalarWorkItems)) {
        return false;
    }
    candidate.totalWorkItems = scalarWorkItems;

    for (int component = 0; component < 3; ++component) {
        for (int axis = 0; axis < 3; ++axis) {
            candidate.velocity[component].dimensions[axis] =
                candidate.scalar.dimensions[axis];
            candidate.velocity[component].origin[axis] =
                candidate.scalar.origin[axis];
        }
        candidate.velocity[component].dimensions[component] += 1U;
        candidate.velocity[component].origin[component] = minimums[component];

        if (includeVelocity) {
            if (!detail::checkedVoxelCount(
                    candidate.velocity[component].dimensions,
                    maximumWorkItems,
                    candidate.velocity[component].voxelCount) ||
                !generation::checkedSum(
                    candidate.totalWorkItems,
                    candidate.velocity[component].voxelCount,
                    maximumWorkItems, candidate.totalWorkItems)) {
                return false;
            }
        }
    }

    plan = candidate;
    return true;
}

} // namespace fluid
} // namespace dommy
