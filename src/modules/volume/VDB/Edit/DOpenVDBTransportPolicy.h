#pragma once

#include "DOpenVDBFilterPolicy.h"
#include "DOpenVDBValuePolicy.h"
#include <openvdb/math/FiniteDifference.h>

namespace dommy {
namespace vdb {

// Admission limits, not UI limits or native memory/time guarantees.
const int kMaxTransportSteps = 1024;
const int kMaxTransportDistance = 64;
const int kMaxTransportCflSteps = 4096;

struct TransportSettings {
    short spatial = 1, temporal = 1, renormSpatial = 1, renormTemporal = 1;
    int normCount = 3, substeps = 1;
    short integrator = 0, limiter = 0;
    float time = 0, maskMin = 0, maskMax = 1;
    bool respectClass = true, invertMask = false;
};

inline bool validTransportSettings(const TransportSettings& s) {
    return s.spatial >= 0 && s.spatial <= 1 && s.renormSpatial >= 0 && s.renormSpatial <= 1 &&
           s.temporal >= 0 && s.temporal <= 2 && s.renormTemporal >= 0 && s.renormTemporal <= 2 &&
           s.normCount >= 0 && s.normCount <= kMaxTransportSteps && s.substeps >= 1 &&
           s.substeps <= kMaxTransportSteps && s.integrator >= 0 && s.integrator <= 5 &&
           s.limiter >= 0 && s.limiter <= 2 && std::isfinite(s.time) && std::isfinite(s.maskMin) &&
           std::isfinite(s.maskMax);
}

inline bool validTransportMask(const TransportSettings& s, long double maxAbsValue) {
    float delta = 0, inverse = 0, difference = 0, normalized = 0;
    return s.maskMin < s.maskMax &&
           numeric::checkedNumericCast(static_cast<long double>(s.maskMax) - s.maskMin, delta) &&
           delta > 0 && numeric::checkedNumericCast(1.0L / delta, inverse) && inverse > 0 &&
           numeric::checkedNumericCast(maxAbsValue + std::fabs(s.maskMin), difference) &&
           numeric::checkedNumericCast(static_cast<long double>(difference) * inverse, normalized);
}

// A component-wise bound also covers staggered sampling, whose components can
// come from different stored vectors. Leave headroom for native lengthSqr().
template <typename T> struct TransportMagnitude {
    static long double get(T value) {
        return std::fabs(static_cast<long double>(value));
    }
    static long double limit() {
        return static_cast<long double>(std::numeric_limits<T>::max()) / 64.0L;
    }
};
template <typename T> struct TransportMagnitude<openvdb::math::Vec3<T>> {
    static long double get(const openvdb::math::Vec3<T>& value) {
        return std::max(std::fabs(static_cast<long double>(value[0])),
                        std::max(std::fabs(static_cast<long double>(value[1])),
                                 std::fabs(static_cast<long double>(value[2])))) *
               2;
    }
    static long double limit() {
        return std::sqrt(static_cast<long double>(std::numeric_limits<T>::max())) / 64;
    }
};

template <typename GridT>
bool transportValues(const GridT& grid, std::size_t& work, long double& magnitude) {
    typedef typename GridT::ValueType ValueT;
    ValuePreflight finite(work, 2); // finite scan and magnitude scan
    finite(grid);
    if (!finite.valid)
        return false;
    magnitude = TransportMagnitude<ValueT>::get(grid.background());
    for (auto it = grid.cbeginValueAll(); it; ++it)
        magnitude = std::max(magnitude, TransportMagnitude<ValueT>::get(*it));
    return magnitude <= TransportMagnitude<ValueT>::limit();
}

template <typename GridT> bool validTransportTransform(const GridT& grid) {
    if (!grid.transform().isLinear())
        return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(grid.voxelSize()[axis]) || grid.voxelSize()[axis] <= 0 ||
            !std::isfinite(1.0 / grid.voxelSize()[axis]))
            return false;
    return true;
}

struct TransportPlan {
    int padding = 16;
    std::size_t passes = 1;
    double worldMotion = 0;
};

inline bool makeTransportPlan(const TransportSettings& s, long double speed, double dx,
                              bool levelSet, TransportPlan& result) {
    if (!validTransportSettings(s) || !std::isfinite(speed) || speed < 0 || !std::isfinite(dx) ||
        dx <= 0)
        return false;
    const long double distance = speed * std::fabs(static_cast<long double>(s.time)) / dx;
    if (!std::isfinite(distance) || distance > kMaxTransportDistance)
        return false;
    TransportPlan plan;
    int layers = 0, steps = 0;
    if (levelSet) {
        // Smallest native CFL is 0.3/sqrt(3); factor 8 includes headroom.
        if (!numeric::checkedNumericCast(std::ceil(distance * 8), steps) ||
            steps > kMaxTransportCflSteps)
            return false;
        layers = steps;
        plan.passes = std::size_t(std::max(1, steps)) * (3U + 3U * s.normCount);
    } else {
        // Preflight the native ceil(maxVelocity*abs(dt)/dx) -> int conversion
        // before VolumeAdvection allocates/copies or dilates any sparse tree.
        if (!numeric::checkedNumericCast(std::ceil(distance / s.substeps), layers))
            return false;
        layers *= s.substeps; // bounded by 64 * 1024 above
        plan.passes = std::size_t(s.substeps) * 8U;
    }
    plan.padding = layers + 16;
    if (!numeric::checkedNumericCast(speed * std::fabs(static_cast<long double>(s.time)) * 4,
                                     plan.worldMotion))
        return false;
    result = plan;
    return true;
}

template <typename GridT>
bool transportWork(const GridT& grid, const TransportPlan& plan, std::size_t& work) {
    using namespace generation;
    const auto bounds = grid.evalActiveVoxelBoundingBox();
    std::size_t extentProduct = bounds.empty() ? 0U : 1U;
    if (!bounds.empty()) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!filterCoordinate(bounds.min()[axis], plan.padding) ||
                !filterCoordinate(bounds.max()[axis], plan.padding))
                return false;
            std::size_t extent = 0;
            if (!numeric::checkedNumericCast(static_cast<long double>(bounds.max()[axis]) -
                                                 bounds.min()[axis] + 2 * plan.padding + 1,
                                             extent) ||
                !checkedProduct(extentProduct, extent, kMaxWorkItemCount, extentProduct))
                return false;
        }
    }
    std::size_t cost = 0;
    const auto active = grid.activeVoxelCount();
    return active <= kMaxWorkItemCount &&
           checkedProduct(static_cast<std::size_t>(active), plan.passes, kMaxWorkItemCount, cost) &&
           checkedSum(work, cost, kMaxWorkItemCount, work) &&
           checkedSum(work, extentProduct, kMaxWorkItemCount, work);
}

template <typename GridT, typename SampleT>
bool transportDomain(const GridT& grid, const SampleT& sample, const TransportPlan& plan) {
    if (!validTransportTransform(grid) || !validTransportTransform(sample))
        return false;
    const auto bounds = grid.evalActiveVoxelBoundingBox();
    if (bounds.empty())
        return true;
    // For affine maps the extrema lie at corners. Also test a world-space
    // trajectory envelope for all RK stages and both correction directions.
    for (int corner = 0; corner < 8; ++corner) {
        auto position = bounds.min().asVec3d();
        for (int axis = 0; axis < 3; ++axis)
            position[axis] = (corner & (1 << axis)) ? double(bounds.max()[axis]) + plan.padding
                                                    : double(bounds.min()[axis]) - plan.padding;
        const auto world = grid.transform().indexToWorld(position);
        for (int motion = 0; motion < 8; ++motion) {
            auto moved = world;
            for (int axis = 0; axis < 3; ++axis) {
                moved[axis] += (motion & (1 << axis)) ? plan.worldMotion : -plan.worldMotion;
                if (!std::isfinite(moved[axis]))
                    return false;
            }
            const auto index = sample.transform().worldToIndex(moved);
            for (int axis = 0; axis < 3; ++axis)
                if (!filterCoordinate(index[axis], 4))
                    return false;
        }
    }
    return true;
}

template <typename Operation>
void configureTransport(Operation& operation, const TransportSettings& s) {
    operation.setSpatialScheme(s.spatial == 0 ? openvdb::math::FIRST_BIAS
                                              : openvdb::math::HJWENO5_BIAS);
    operation.setTemporalScheme(static_cast<openvdb::math::TemporalIntegrationScheme>(s.temporal));
    operation.setTrackerSpatialScheme(s.renormSpatial == 0 ? openvdb::math::FIRST_BIAS
                                                           : openvdb::math::HJWENO5_BIAS);
    operation.setTrackerTemporalScheme(
        static_cast<openvdb::math::TemporalIntegrationScheme>(s.renormTemporal));
    operation.setNormCount(s.normCount);
}
} // namespace vdb
} // namespace dommy
