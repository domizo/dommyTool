#pragma once

#include "DOpenVDBTransportPolicy.h"
#include "math/DBounds.h"
#include <openvdb/math/Transform.h>

namespace dommy {
namespace vdb {
// Shared affine-coordinate admission rules for legacy VDB tree operations.
// Leave headroom for tree origins, inclusive loops and sampler neighborhoods.
const int kGridCoordinatePadding = 4096;

inline bool validDerivativeGridMap(const openvdb::math::Transform& transform) {
    const auto origin = transform.indexToWorld(openvdb::Vec3d(0));
    const auto inverseOrigin = transform.worldToIndex(openvdb::Vec3d(0));
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || !std::isfinite(inverseOrigin[axis]))
            return false;
        openvdb::Vec3d unit(0);
        unit[axis] = 1;
        const auto a = transform.indexToWorld(unit), b = transform.worldToIndex(unit);
        const double forward =
            std::hypot(std::hypot(a[0] - origin[0], a[1] - origin[1]), a[2] - origin[2]);
        const double inverse = std::hypot(
            std::hypot(b[0] - inverseOrigin[0], b[1] - inverseOrigin[1]), b[2] - inverseOrigin[2]);
        // Bounded affine coefficients for legacy derivative intermediate values.
        if (!std::isfinite(forward) || !std::isfinite(inverse) || forward < 1.0e-6 ||
            forward > 1.0e6 || inverse < 1.0e-6 || inverse > 1.0e6)
            return false;
    }
    return true;
}

inline bool validGridBox(const openvdb::BBoxd& box) {
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(box.min()[axis]) || !std::isfinite(box.max()[axis]) ||
            box.min()[axis] > box.max()[axis])
            return false;
    return true;
}

inline bool indexGridBox(const openvdb::BBoxd& box, openvdb::CoordBBox& result,
                         bool roundUp = false) {
    if (!validGridBox(box))
        return false;
    openvdb::Int32 low[3], high[3];
    for (int axis = 0; axis < 3; ++axis) {
        const double first = std::floor(box.min()[axis]);
        const double last = roundUp ? std::ceil(box.max()[axis]) : std::floor(box.max()[axis]);
        if (!dommy::vdb::filterCoordinate(first, kGridCoordinatePadding) ||
            !dommy::vdb::filterCoordinate(last, kGridCoordinatePadding) ||
            !dommy::numeric::checkedNumericCast(first, low[axis]) ||
            !dommy::numeric::checkedNumericCast(last, high[axis]))
            return false;
    }
    result.reset(openvdb::Coord(low[0], low[1], low[2]), openvdb::Coord(high[0], high[1], high[2]));
    return true;
}

inline openvdb::BBoxd gridBounds(const dommy::math::Bounds3d& bounds) {
    return openvdb::BBoxd(openvdb::Vec3d(bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)),
                          openvdb::Vec3d(bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)));
}

// Native 5.2 initializes mip-level shifts even for non-mipmapped samplers.
// This relative scale envelope bounds those shifts in both directions.
inline bool validRelativeGridMap(const openvdb::math::Transform& from,
                                 const openvdb::math::Transform& to) {
    const auto origin = to.worldToIndex(from.indexToWorld(openvdb::Vec3d(0)));
    for (int axis = 0; axis < 3; ++axis)
        if (!dommy::vdb::filterCoordinate(origin[axis], kGridCoordinatePadding))
            return false;
    for (int axis = 0; axis < 3; ++axis) {
        openvdb::Vec3d unit(0);
        unit[axis] = 1;
        const auto mapped = to.worldToIndex(from.indexToWorld(unit));
        const double length = std::hypot(std::hypot(mapped[0] - origin[0], mapped[1] - origin[1]),
                                         mapped[2] - origin[2]);
        if (!std::isfinite(length) || length < 1.0 / 64 || length > 64)
            return false;
    }
    return true;
}

inline bool mapGridBox(const openvdb::BBoxd& input, const openvdb::math::Transform& from,
                       const openvdb::math::Transform& to, openvdb::BBoxd& output) {
    if (!validGridBox(input))
        return false;
    dommy::math::Bounds3d bounds;
    for (int corner = 0; corner < 8; ++corner) {
        openvdb::Vec3d p;
        for (int axis = 0; axis < 3; ++axis)
            p[axis] = (corner & (1 << axis)) ? input.max()[axis] : input.min()[axis];
        p = to.worldToIndex(from.indexToWorld(p));
        if (!bounds.add(p[0], p[1], p[2]))
            return false;
    }
    output = gridBounds(bounds);
    return true;
}
} // namespace vdb
} // namespace dommy
