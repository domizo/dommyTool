#pragma once

#include "common/utility/DCheckedSize.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

// These limits are runtime safety budgets, not UI ranges.  Attribute metadata
// can be bypassed by old or malformed scene data, so compute entry points must
// validate raw values again before allocating or iterating.
const std::size_t kMaxGeneratedPointCount = 10000000U;
const std::size_t kMaxGeneratedCurveCount = 1000000U;
const std::size_t kMaxGeneratedMeshCount = 1000000U;
const std::size_t kMaxCurveSampleCount = 1000000U;
const std::size_t kMaxGeneratedFaceCount = 2000000U;
const std::size_t kMaxGeneratedStringCount = 1000000U;
const std::size_t kMaxGeneratedStringByteCount = 100000000U;
const std::size_t kMaxGeneratedTransformCount = 1000000U;
const std::size_t kMaxConnectedGeometryCount = 1000000U;
const std::size_t kMaxWorkItemCount = 100000000U;

inline bool checkedCount(int rawValue,
                         std::size_t maximum,
                         std::size_t& result)
{
    if (rawValue < 0) {
        return false;
    }

    const std::size_t value = static_cast<std::size_t>(rawValue);
    if (value > maximum) {
        return false;
    }

    result = value;
    return true;
}

inline bool checkedProduct(std::size_t lhs,
                           std::size_t rhs,
                           std::size_t maximum,
                           std::size_t& result)
{
    std::size_t product = 0;
    if (!memory::checkedMultiply(lhs, rhs, product) || product > maximum) {
        return false;
    }

    result = product;
    return true;
}

inline bool checkedSum(std::size_t lhs,
                       std::size_t rhs,
                       std::size_t maximum,
                       std::size_t& result)
{
    std::size_t sum = 0;
    if (!memory::checkedAdd(lhs, rhs, sum) || sum > maximum) {
        return false;
    }

    result = sum;
    return true;
}

inline bool unitCircleOffset(double fraction, double& x, double& y)
{
    if (!std::isfinite(fraction) || fraction < 0.0 || fraction >= 1.0) {
        return false;
    }

    const double angle = fraction * 6.28318530717958647692;
    x = std::cos(angle);
    y = std::sin(angle);
    return std::isfinite(x) && std::isfinite(y);
}

inline bool orderedUnitCircleOffset(std::size_t index,
                                    std::size_t count,
                                    double& x,
                                    double& y)
{
    if (count == 0U || index >= count) {
        return false;
    }
    return unitCircleOffset(
        static_cast<double>(index) / static_cast<double>(count), x, y);
}

inline bool checkedGridBudget(double extentX,
                              double extentY,
                              double extentZ,
                              double cellSize,
                              double padding,
                              std::size_t maximum,
                              std::size_t& result)
{
    const double extents[3] = {extentX, extentY, extentZ};
    if (!std::isfinite(cellSize) || cellSize <= 0.0 ||
        !std::isfinite(padding) || padding < 0.0) {
        return false;
    }

    std::size_t budget = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(extents[axis]) || extents[axis] < 0.0) {
            return false;
        }
        const double dimension =
            std::ceil(extents[axis] / cellSize) + padding;
        if (!std::isfinite(dimension) || dimension < 1.0 ||
            dimension > static_cast<double>(maximum)) {
            return false;
        }

        std::size_t nextBudget = 0;
        if (!checkedProduct(
                budget, static_cast<std::size_t>(dimension), maximum,
                nextBudget)) {
            return false;
        }
        budget = nextBudget;
    }

    result = budget;
    return true;
}

} // namespace generation
} // namespace dommy
