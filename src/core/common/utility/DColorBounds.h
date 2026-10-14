#pragma once

#include "DAttributeArrayConversion.h"
#include "DPointTransform.h"
#include "math/DBounds.h"
#include "math/DRemapMath.h"
#include <algorithm>

namespace dommy {
namespace color {

template <typename Points>
bool pointBounds(const Points& points, Imath::V3d& minimum, Imath::V3d& maximum) {
    Imath::V3d low(0, 0, 0), high(0, 0, 0);
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (unsigned int axis = 0; axis < 3U; ++axis) {
            const double value = points[i][axis];
            if (!std::isfinite(value))
                return false;
            low[axis] = i == 0 ? value : std::min(low[axis], value);
            high[axis] = i == 0 ? value : std::max(high[axis], value);
        }
    }
    minimum = low;
    maximum = high;
    return true;
}

// Promote before subtraction: finite float endpoints can have a span larger
// than FLT_MAX. Preserve zero-width axes as zero and do not clamp extrapolation.
template <typename Point>
bool normalizeBoundingColor(const Point& point, const Imath::V3d& minimum,
                            const Imath::V3d& maximum, Imath::V3f& result) {
    Imath::V3f candidate(0, 0, 0);
    for (unsigned int axis = 0; axis < 3U; ++axis) {
        const double value = point[axis];
        const double low = minimum[axis], high = maximum[axis];
        if (!std::isfinite(value) || !std::isfinite(low) || !std::isfinite(high) || low > high)
            return false;
        double normalized = 0.0;
        if (low != high && !math::normalizeRange(value, low, high, normalized))
            return false;
        if (!numeric::checkedFloatCast(normalized, candidate[axis]))
            return false;
    }
    result = candidate;
    return true;
}

template <typename Points>
bool boundingColors(const Points& points, const Imath::V3d& minimum,
                     const Imath::V3d& maximum, std::vector<Imath::V3f>& result) {
    if (!attribute::validArrayConversionBudget(points.size(), sizeof(Imath::V3f)))
        return false;
    std::vector<Imath::V3f> candidate(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
        if (!normalizeBoundingColor(points[i], minimum, maximum, candidate[i]))
            return false;
    result.swap(candidate);
    return true;
}

// The caller supplies primitives with validated topology and finite matrices.
// Both passes use checkedTransformPoint in double, never float-backed world
// point copies or a cached bounding box from another coordinate space.
template <typename Iterator>
bool transformedPointBounds(Iterator first, Iterator last, Imath::V3d& minimum,
                            Imath::V3d& maximum) {
    math::Bounds3d bounds;
    for (; first != last; ++first) {
        if (!*first)
            return false;
        const auto* points = (*first)->getPositionPtrConst();
        const auto matrix = (*first)->getTransformMatrix();
        if (!points || !numeric::appendTransformedBounds(*points, matrix, bounds))
            return false;
    }
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        minimum[axis] = bounds.empty() ? 0.0 : bounds.minimum(axis);
        maximum[axis] = bounds.empty() ? 0.0 : bounds.maximum(axis);
    }
    return true;
}

template <typename Points, typename Matrix>
bool transformedBoundingColors(const Points& points, const Matrix& matrix,
                                const Imath::V3d& minimum, const Imath::V3d& maximum,
                                std::vector<Imath::V3f>& result) {
    if (!attribute::validArrayConversionBudget(points.size(), sizeof(Imath::V3f)))
        return false;
    std::vector<Imath::V3f> candidate(points.size());
    for (std::size_t i = 0U; i < points.size(); ++i) {
        std::array<double, 3> transformed;
        if (!numeric::checkedTransformPoint(points[i], matrix, transformed) ||
            !normalizeBoundingColor(transformed, minimum, maximum, candidate[i]))
            return false;
    }
    result.swap(candidate);
    return true;
}

} // namespace color
} // namespace dommy
