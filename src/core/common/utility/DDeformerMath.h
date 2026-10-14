#ifndef DOMMY_DDEFORMER_MATH_H
#define DOMMY_DDEFORMER_MATH_H

#include "DAttributeBlendMath.h"
#include "DPointMeanAccumulator.h"
#include "DGenerationLimits.h"
#include "DScaledProduct.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include <cmath>
#include <cstddef>
#include <vector>

namespace dommy {
namespace deformer {

template <typename Point>
bool blendPoint(const Point& source, const Point& target, double weight, Point& output) {
    Point candidate(source);
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!attribute::checkedLinearBlend(double(source[axis]), double(target[axis]), weight,
                                           candidate[axis]))
            return false;
    output = candidate;
    return true;
}

template <typename Point, typename Vector>
bool offsetPoint(const Point& source, const Vector& direction, double magnitude, double signal,
                 double weight, double envelope, double falloff, Point& output) {
    Point candidate(source);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        double displacement = 0.0;
        if (!std::isfinite(double(source[axis])) ||
            !numeric::scaledProduct(std::array<double, 6>{{double(direction[axis]), magnitude,
                                                           signal, weight, envelope, falloff}},
                                    displacement) ||
            !numeric::checkedNumericCast(double(source[axis]) + displacement, candidate[axis]))
            return false;
    }
    output = candidate;
    return true;
}

template <typename Vector> bool normalizeVector(Vector& vector) {
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(double(vector[axis])))
            return false;
    if (std::hypot(std::hypot(vector[0], vector[1]), vector[2]) <= 1.0e-12) {
        vector[0] = 0.0;
        vector[1] = 0.0;
        vector[2] = 0.0;
        return true;
    }
    double x = 0.0, y = 0.0, z = 0.0;
    if (!math::normalized3OrZero(vector[0], vector[1], vector[2], x, y, z))
        return false;
    vector[0] = x;
    vector[1] = y;
    vector[2] = z;
    return true;
}

// Collision displacement and smoothing are unweighted candidates. Apply the
// deformer weight only once when publishing; inactive vertices remain fixed
// and provide fixed boundary positions to their active neighbors.
template <typename Point>
bool smoothCollisionPoints(std::vector<Point>& points,
                           const std::vector<std::vector<unsigned int>>& neighbors,
                           const std::vector<bool>& active, const std::vector<bool>& contained,
                           int mode, unsigned int iterations) {
    const std::size_t count = points.size();
    if (count > generation::kMaxGeneratedPointCount || neighbors.size() != count ||
        active.size() != count || contained.size() != count || mode < 0 || mode > 2)
        return false;
    for (const Point& point : points)
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!std::isfinite(double(point[axis])))
                return false;
    std::size_t work = count;
    for (const auto& adjacent : neighbors) {
        if (!generation::checkedSum(work, adjacent.size(), generation::kMaxWorkItemCount, work))
            return false;
        for (unsigned int index : adjacent)
            if (index >= count)
                return false;
    }
    if (!generation::checkedProduct(work, iterations, generation::kMaxWorkItemCount, work))
        return false;
    if (count == 0U || iterations == 0U)
        return true;
    std::vector<Point> candidate(points), next(points);
    for (unsigned int iteration = 0U; iteration < iterations; ++iteration) {
        for (std::size_t index = 0U; index < count; ++index) {
            next[index] = candidate[index];
            if (!active[index] || (mode == 1 && !contained[index]) ||
                (mode == 2 && contained[index]) || neighbors[index].empty())
                continue;
            std::array<numeric::detail::PointMeanAccumulator, 3> sums;
            for (unsigned int neighbor : neighbors[index])
                for (unsigned int axis = 0U; axis < 3U; ++axis)
                    if (!sums[axis].add(candidate[neighbor][axis]))
                        return false;
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                if (!sums[axis].mean(static_cast<std::uint32_t>(neighbors[index].size()),
                                     next[index][axis]))
                    return false;
        }
        candidate.swap(next);
    }
    points.swap(candidate);
    return true;
}

const std::size_t kMaxMotionSampleCount = 4096U;
const std::size_t kMaxMotionPointSamples = 10000000U;

struct MotionSampleBlend {
    std::size_t lowerIndex;
    std::size_t upperIndex;
    double fraction;

    MotionSampleBlend() noexcept : lowerIndex(0U), upperIndex(0U), fraction(0.0) {}
};

inline bool motionSampleBlend(const double ramp, const std::size_t stateCount,
                              MotionSampleBlend& blend) noexcept {
    if (!std::isfinite(ramp) || ramp < 0.0 || ramp > 1.0 || stateCount == 0U) {
        return false;
    }

    MotionSampleBlend candidate;
    if (stateCount == 1U || ramp == 0.0) {
        blend = candidate;
        return true;
    }

    const std::size_t lastIndex = stateCount - 1U;
    if (ramp == 1.0) {
        candidate.lowerIndex = lastIndex;
        candidate.upperIndex = lastIndex;
        blend = candidate;
        return true;
    }

    const double position = ramp * static_cast<double>(lastIndex);
    if (!std::isfinite(position) || position < 0.0 || position >= static_cast<double>(lastIndex)) {
        return false;
    }

    candidate.lowerIndex = static_cast<std::size_t>(std::floor(position));
    candidate.upperIndex = candidate.lowerIndex + 1U;
    candidate.fraction = position - static_cast<double>(candidate.lowerIndex);
    if (candidate.upperIndex >= stateCount || candidate.fraction < 0.0 ||
        candidate.fraction >= 1.0) {
        return false;
    }

    blend = candidate;
    return true;
}

inline double linearFalloff(double distance, double radius) noexcept {
    if (!std::isfinite(distance) || !std::isfinite(radius) || distance < 0.0 || radius < 0.0) {
        return 0.0;
    }
    if (radius <= 1.0e-12) {
        return distance <= 1.0e-12 ? 1.0 : 0.0;
    }
    if (distance >= radius) {
        return 0.0;
    }
    return 1.0 - distance / radius;
}

inline double radialFalloff(double distance, double radius, int shape,
                            double attenuation) noexcept {
    if (!std::isfinite(distance) || !std::isfinite(radius) || !std::isfinite(attenuation) ||
        distance < 0.0 || radius < 0.0 || attenuation < 0.0 || shape < 0 || shape > 2) {
        return 0.0;
    }

    double weight = 0.0;
    if (shape == 0) {
        if (radius <= 1.0e-12) {
            weight = distance <= 1.0e-12 ? 1.0 : 0.0;
        } else {
            weight = distance < radius ? 1.0 : 0.0;
        }
    } else {
        weight = linearFalloff(distance, radius);
        if (shape == 2)
            weight *= weight;
    }

    if (weight <= 0.0)
        return 0.0;
    if (attenuation == 0.0)
        return 1.0;
    const double attenuated = std::pow(weight, attenuation);
    return std::isfinite(attenuated) ? attenuated : 0.0;
}

inline bool hasSampleForPoint(const std::size_t pointIndex, const std::size_t pointCount) noexcept {
    return pointIndex < pointCount;
}

inline bool validSamplingRequest(const long sampleCount, const double timeRange) noexcept {
    return sampleCount > 0 && std::isfinite(timeRange) && timeRange >= 0.0;
}

inline bool validSampleStorageRequest(const long sampleCount, const std::size_t pointCount,
                                      const std::size_t maximumSampleCount,
                                      const std::size_t maximumPointSamples) noexcept {
    if (sampleCount <= 0 || static_cast<unsigned long>(sampleCount) > maximumSampleCount) {
        return false;
    }
    const std::size_t samples = static_cast<std::size_t>(sampleCount);
    return pointCount == 0U || samples <= maximumPointSamples / pointCount;
}

inline bool validMotionSampleRequest(const long sampleCount, const double timeRange,
                                     const std::size_t pointCount) noexcept {
    return validSamplingRequest(sampleCount, timeRange) &&
           validSampleStorageRequest(sampleCount, pointCount, kMaxMotionSampleCount,
                                     kMaxMotionPointSamples);
}

inline bool hasMatchingSampleLayout(const std::vector<std::size_t>& expectedPointCounts,
                                    const std::vector<std::size_t>& sampledPointCounts) noexcept {
    return expectedPointCounts == sampledPointCounts;
}

} // namespace deformer
} // namespace dommy

#endif // DOMMY_DDEFORMER_MATH_H
