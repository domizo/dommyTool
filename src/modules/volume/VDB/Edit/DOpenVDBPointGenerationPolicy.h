#pragma once

#include "DOpenVDBSpatialPolicy.h"
#include "DOpenVDBTreeBudget.h"

namespace dommy {
namespace vdb {

// Leave room for legacy integer neighborhoods and float squared distances.
inline bool pointGenerationBox(const openvdb::BBoxd& box,
                               const openvdb::math::Transform& transform,
                               openvdb::CoordBBox& result) {
    if (!indexGridBox(box, result, true)) return false;
    for (int axis = 0; axis < 3; ++axis)
        if (std::fabs(box.min()[axis]) > 1e6 || std::fabs(box.max()[axis]) > 1e6)
            return false;
    for (int corner = 0; corner < 8; ++corner) {
        openvdb::Vec3d point;
        for (int axis = 0; axis < 3; ++axis)
            point[axis] = corner & (1 << axis) ? box.max()[axis] : box.min()[axis];
        const auto world = transform.indexToWorld(point);
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(world[axis]) || std::fabs(world[axis]) > 1e12) return false;
    }
    return true;
}

template<typename GridT>
bool pointGenerationDomain(const GridT& grid, bool includeInactive,
                           TreeEditBudget& budget, math::Bounds3d& bounds) {
    if (!grid.transform().isLinear() || !validDerivativeGridMap(grid.transform()) ||
        !budget.addBytes(1, grid.memUsage())) return false;
    // Values, bounds, mode-specific counts and the native source traversal.
    ValuePreflight values(budget.work, 4);
    values(grid);
    if (!values.valid) return false;
    for (auto it = grid.cbeginValueAll(); it; ++it) {
        if (!it.isValueOn() && (!includeInactive ||
            (it.isTileValue() && *it == grid.background()))) continue;
        openvdb::CoordBBox box;
        it.getBoundingBox(box);
        if (!bounds.add(box.min()[0], box.min()[1], box.min()[2]) ||
            !bounds.add(box.max()[0], box.max()[1], box.max()[2])) return false;
    }
    if (bounds.empty()) return true;
    auto padded = gridBounds(bounds);
    padded.expand(1); // Complete voxel cells and immediate neighbors.
    openvdb::CoordBBox checked;
    return pointGenerationBox(padded, grid.transform(), checked);
}

inline std::size_t pointSortPasses(std::size_t count) {
    std::size_t passes = 1;
    while (count > 1) { count /= 2; ++passes; }
    return passes;
}

} // namespace vdb
} // namespace dommy
