//
//  DPointPackingCommon.h
//  dommyTool
//
//  Small shared types and host-independent helpers for packing nodes.
//

#pragma once

#include <maya/MPoint.h>
#include "common/utility/DGenerationLimits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

enum {
    D_PACK_RANDOM = 0,
    D_PACK_GRADATION,
    D_PACK_MAP,
    D_PACKING_TYPE_COUNT
};
typedef short D_PACKING_TYPE;

struct PackedCircle
{
    PackedCircle(const MPoint &pos, float radius)
        : m_pt(pos), m_radius(radius)
    {
    }

    MPoint m_pt;
    float m_radius;
};

namespace dommy {
namespace packing {

// Preserve PoissonGenerator's negative-distance auto mode, but bound its
// internal square grid and candidate loop before it allocates or casts to int.
inline bool isBoundedPoissonRequest(std::size_t count, float distance) {
    if (!std::isfinite(distance) || distance < -1.0f || distance == 0.0f ||
        count > generation::kMaxGeneratedPointCount)
        return false;
    if (count == 0U)
        return true;
    std::size_t maximumOutput = 0U, work = 0U;
    // The legacy generator can finish a batch beyond the requested count.
    if (!generation::checkedProduct(count, 2U, generation::kMaxGeneratedPointCount,
                                    maximumOutput) ||
        !generation::checkedProduct(count, maximumOutput, generation::kMaxWorkItemCount, work))
        return false;
    const float resolved = distance < 0.0f
                               ? std::sqrt(static_cast<float>(count)) / static_cast<float>(count)
                               : distance;
    const float cellSize = resolved / std::sqrt(2.0f);
    const double side = std::ceil(1.0f / cellSize);
    if (!std::isfinite(side) || side < 1.0 ||
        side > static_cast<double>(generation::kMaxGeneratedPointCount))
        return false;
    const std::size_t gridSide = static_cast<std::size_t>(side);
    return generation::checkedProduct(gridSide, gridSide, generation::kMaxGeneratedPointCount,
                                      work);
}

inline bool clampUnit(float value, float& result) {
    if (!std::isfinite(value)) {
        return false;
    }
    result = std::max(0.0f, std::min(1.0f, value));
    return true;
}

inline bool normalizedPosition(float value, float minimum, float extent, float& result) {
    if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(extent) ||
        extent < 0.0f) {
        return false;
    }
    if (extent <= std::numeric_limits<float>::epsilon()) {
        result = 0.5f;
        return true;
    }
    return clampUnit((value - minimum) / extent, result);
}

inline float uniform(std::mt19937& engine, float minimum, float maximum)
{
    if (maximum < minimum) {
        std::swap(minimum, maximum);
    }
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(engine);
}

inline bool isFinite(const MPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z) && std::isfinite(point.w);
}

inline bool isFinite(const PackedCircle& circle)
{
    return isFinite(circle.m_pt) && std::isfinite(circle.m_radius) &&
           circle.m_radius >= 0.0f;
}

} // namespace packing
} // namespace dommy
