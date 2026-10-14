#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace dommy {
namespace vdb {

// Runtime limits, deliberately separate from the archived Maya UI ranges.
const int kMaxFilterWidth = 64;
const int kMaxFilterIterations = 1024;
const int kMaxFilterOffsetSteps = 4096;

struct FilterSettings {
    int operation = 0, radius = 1, iterations = 1;
    float offset = 0;
};
struct LevelSetFilterSettings {
    int operation = 0, accuracy = 0, iterations = 1;
    int radius = 0, halfWidth = 3;
    float worldRadius = 0.1f, worldHalfWidth = 0.3f, offset = 0;
    float maskMin = 0, maskMax = 1;
    bool worldUnits = false, invertMask = false;
};
struct FilterPlan {
    int radius = 1, halfWidth = 3, padding = 1;
    float distance = 0;
    std::size_t workPerVoxel = 0;
};

inline bool validFilterSettings(const FilterSettings& s) {
    return s.operation >= 0 && s.operation <= 3 && s.radius >= 1 && s.radius <= kMaxFilterWidth &&
           s.iterations >= 1 && s.iterations <= kMaxFilterIterations && std::isfinite(s.offset);
}
inline bool validLevelSetFilterSettings(const LevelSetFilterSettings& s, bool masked) {
    return s.operation >= -1 && s.operation <= 11 && s.accuracy >= 0 && s.accuracy <= 4 &&
           s.iterations >= 0 && s.iterations <= kMaxFilterIterations && s.radius >= 0 &&
           s.radius <= kMaxFilterWidth && s.halfWidth >= 1 && s.halfWidth <= kMaxFilterWidth &&
           std::isfinite(s.worldRadius) && s.worldRadius > 0 && std::isfinite(s.worldHalfWidth) &&
           s.worldHalfWidth > 0 && std::isfinite(s.offset) && s.offset >= 0 &&
           (!masked || (std::isfinite(s.maskMin) && std::isfinite(s.maskMax) && s.maskMin >= 0 &&
                        s.maskMin < s.maskMax && std::isfinite(1.0f / (s.maskMax - s.maskMin))));
}
inline bool filterWidth(double worldWidth, double voxelSize, int& result) {
    if (!std::isfinite(worldWidth) || worldWidth <= 0 || !std::isfinite(voxelSize) ||
        voxelSize <= 0)
        return false;
    const double width = std::max(1.0, std::floor(worldWidth / voxelSize));
    int candidate = 0;
    if (!numeric::checkedNumericCast(width, candidate) || candidate > kMaxFilterWidth)
        return false;
    result = candidate;
    return true;
}
inline std::size_t filterNeighborhoodWork(int radius, bool median, bool gaussian) {
    const std::size_t width = 2U * static_cast<std::size_t>(radius) + 1U;
    return median ? width * width * width : width * (gaussian ? 12U : 3U);
}
inline bool makeFilterPlan(const FilterSettings& s, FilterPlan& result) {
    if (!validFilterSettings(s))
        return false;
    FilterPlan plan;
    plan.radius = s.radius;
    plan.padding = s.operation == 3 ? 0 : s.radius + 1;
    plan.workPerVoxel =
        s.operation == 3
            ? 1U
            : filterNeighborhoodWork(s.radius, s.operation == 2, s.operation == 1) * s.iterations;
    result = plan;
    return true;
}
inline bool makeLevelSetFilterPlan(const LevelSetFilterSettings& s, double voxelSize,
                                   FilterPlan& result) {
    if (!validLevelSetFilterSettings(s, false) || !std::isfinite(voxelSize) || voxelSize <= 0)
        return false;
    FilterPlan plan;
    plan.radius = std::max(1, s.radius);
    plan.halfWidth = s.halfWidth;
    const bool neighborhood = s.operation == 1 || s.operation == 2 || s.operation == 10;
    if (s.worldUnits && neighborhood && !filterWidth(s.worldRadius, voxelSize, plan.radius))
        return false;
    if (s.worldUnits && s.operation == 11 &&
        !filterWidth(s.worldHalfWidth, voxelSize, plan.halfWidth))
        return false;
    std::size_t passes = static_cast<std::size_t>(s.iterations);
    if (s.operation >= 5 && s.operation <= 8) {
        // The legacy API accepts a float distance even for a DoubleGrid. Check
        // after narrowing, and bound the native half-voxel CFL stepping loop.
        if (!numeric::checkedNumericCast(static_cast<long double>(s.offset) *
                                             (s.worldUnits ? 1.0L : voxelSize),
                                         plan.distance))
            return false;
        const double steps = std::ceil(2.0 * double(plan.distance) / voxelSize);
        int count = 0;
        if (!numeric::checkedNumericCast(steps, count) || count > kMaxFilterOffsetSteps)
            return false;
        passes = static_cast<std::size_t>(count) * (s.operation >= 7 ? 2U : 1U);
    } else if (s.operation == 11) {
        // Includes erosion from the largest accepted existing narrow band.
        passes = kMaxFilterWidth;
    } else if (s.operation == -1) {
        passes = 0;
    } else if (s.operation == 10) {
        passes *= 4U; // four box passes, each followed by tracking
    }
    // Each track grows the active topology by at most one voxel layer. Extra
    // padding covers the largest finite-difference stencil and leaf boundaries.
    plan.padding = static_cast<int>(passes) + std::max(plan.radius, plan.halfWidth) + 16;
    const std::size_t neighborhoodWork =
        neighborhood ? filterNeighborhoodWork(plan.radius, s.operation == 2, false) : 1U;
    // Admission estimate only: tracking can grow sparse topology. This is not
    // a hard native memory/CPU bound or a wall-clock timeout.
    plan.workPerVoxel = passes * (neighborhoodWork + 128U);
    result = plan;
    return true;
}
inline bool addFilterWork(std::uint64_t activeVoxels, const FilterPlan& plan, std::size_t& total) {
    std::size_t work = 0;
    return activeVoxels <= generation::kMaxWorkItemCount &&
           generation::checkedProduct(static_cast<std::size_t>(activeVoxels),
                                      std::max(std::size_t(1), plan.workPerVoxel),
                                      generation::kMaxWorkItemCount, work) &&
           generation::checkedSum(total, work, generation::kMaxWorkItemCount, total);
}
inline bool filterCoordinate(double value, int padding) {
    return padding >= 0 && std::isfinite(value) &&
           value >= double(std::numeric_limits<int>::min()) + padding &&
           value <= double(std::numeric_limits<int>::max()) - padding;
}

// Dependent templates allow the policy and the actual traversal to be tested
// without pretending that the host-free doubles implement sparse OpenVDB.
template <typename GridT> bool finiteFilterValues(const GridT& grid) {
    if (!std::isfinite(grid.background()))
        return false;
    std::size_t count = 0;
    for (auto it = grid.cbeginValueAll(); it; ++it) {
        if (++count > generation::kMaxWorkItemCount || !std::isfinite(*it))
            return false;
    }
    return true;
}
template <typename GridT> bool validFilterGrid(const GridT& grid, const FilterPlan& plan) {
    if (!finiteFilterValues(grid))
        return false;
    const auto bounds = grid.evalActiveVoxelBoundingBox();
    if (!bounds.empty()) {
        for (int axis = 0; axis < 3; ++axis)
            if (!filterCoordinate(bounds.min()[axis], plan.padding) ||
                !filterCoordinate(bounds.max()[axis], plan.padding))
                return false;
    }
    return true;
}
template <typename GridT> bool validLevelSetScale(const GridT& grid) {
    typedef typename GridT::ValueType ValueT;
    ValueT dx = 0, inverse = 0, expandedBand = 0;
    if (!grid.hasUniformVoxels() || !grid.transform().isLinear() ||
        !numeric::checkedNumericCast(grid.voxelSize()[0], dx) ||
        dx < std::numeric_limits<ValueT>::min() * ValueT(16) ||
        !numeric::checkedNumericCast(1.0L / dx, inverse) ||
        !numeric::checkedNumericCast(static_cast<long double>(dx) * kMaxFilterWidth, expandedBand))
        return false;
    const long double halfWidth = static_cast<long double>(grid.background()) / dx;
    // resize() internally floors this ratio into an int in OpenVDB 5.2.
    return std::isfinite(halfWidth) && halfWidth > 0 && halfWidth <= kMaxFilterWidth;
}
template <typename GridT, typename MaskT>
bool validFilterMaskDomain(const GridT& grid, const MaskT& mask, const FilterPlan& plan) {
    if (!grid.transform().isLinear() || !mask.transform().isLinear())
        return false;
    const auto bounds = grid.evalActiveVoxelBoundingBox();
    if (bounds.empty())
        return true;
    // Affine maps attain coordinate extrema at corners. Include all possible
    // tracking growth before testing the mask's trilinear sampler integer range.
    for (int corner = 0; corner < 8; ++corner) {
        auto position = bounds.min().asVec3d();
        for (int axis = 0; axis < 3; ++axis)
            position[axis] = (corner & (1 << axis)) ? double(bounds.max()[axis]) + plan.padding
                                                    : double(bounds.min()[axis]) - plan.padding;
        const auto world = grid.transform().indexToWorld(position);
        const auto index = mask.transform().worldToIndex(world);
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(world[axis]) || !filterCoordinate(index[axis], 2))
                return false;
    }
    return true;
}
} // namespace vdb
} // namespace dommy
