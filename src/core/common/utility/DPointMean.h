#pragma once

#include "DPointMeanAccumulator.h"
#include "DGenerationLimits.h"

namespace dommy {
namespace numeric {

static_assert(generation::kMaxGeneratedPointCount <= UINT32_MAX, "mean divisor must fit uint32");

template <typename Values>
bool checkedScalarMean(const Values& values, double& result)
{
    if (values.empty() || values.size() > generation::kMaxGeneratedPointCount) return false;
    detail::PointMeanAccumulator sum;
    for (const auto& value : values)
        if (!sum.add(static_cast<double>(value))) return false;
    return sum.mean(static_cast<std::uint32_t>(values.size()), result);
}

// Preserve the arithmetic vertex mean, not a box/area-weighted center. Exact
// accumulation preserves tiny residuals even when intermediate sums exceed
// DBL_MAX. Failed input validation leaves the previous result untouched.
template <typename PointList>
bool checkedPointMean(const PointList& points, std::array<double, 3>& result)
{
    if (points.empty() || points.size() > generation::kMaxGeneratedPointCount) return false;
    if (points.size() == 1U) {
        const auto& point = *points.begin();
        std::array<double, 3> single;
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            single[axis] = static_cast<double>(point[axis]);
            if (!std::isfinite(single[axis])) return false;
        }
        result = single;
        return true;
    }
    std::array<detail::PointMeanAccumulator, 3> sums;
    for (const auto& point : points)
        for (std::size_t axis = 0U; axis < 3U; ++axis)
            if (!sums[axis].add(static_cast<double>(point[axis]))) return false;
    std::array<double, 3> candidate;
    for (std::size_t axis = 0U; axis < 3U; ++axis)
        if (!sums[axis].mean(static_cast<std::uint32_t>(points.size()), candidate[axis])) return false;
    result = candidate;
    return true;
}

} // namespace numeric
} // namespace dommy
