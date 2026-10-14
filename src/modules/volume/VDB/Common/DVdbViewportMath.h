#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "common/utility/DNumericValidation.h"

namespace dommy {
namespace vdb {

// Keep the legacy slider's float widths and fixed padding, but validate every
// narrowing and derived value before handing a world-space box to OpenVDB.
// All outputs are staged: a bad later axis must leave the caller unchanged.
inline bool viewportClipBounds(const std::array<double, 3>& minimum,
                               const std::array<double, 3>& maximum,
                               int axis, float position, float width,
                               std::array<double, 3>& outputMinimum,
                               std::array<double, 3>& outputMaximum)
{
    if (axis < 0 || axis > 2 || !std::isfinite(position) ||
        position < 0.0f || position > 1.0f || !std::isfinite(width) || width < 0.01f)
        return false;

    std::array<double, 3> center;
    std::array<float, 3> extent;
    float band = 0.0f;
    for (int component = 0; component < 3; ++component) {
        if (!numeric::canRepresentAsFloat(minimum[component]) ||
            !numeric::canRepresentAsFloat(maximum[component]) ||
            minimum[component] > maximum[component] ||
            !numeric::checkedFloatCast(maximum[component] - minimum[component],
                                       extent[component]))
            return false;
        extent[component] += 0.8f;
        center[component] = minimum[component] * 0.5 + maximum[component] * 0.5;
        float distance = 0.0f;
        if (!std::isfinite(extent[component]) ||
            !numeric::checkedFloatCast(std::max(std::abs(minimum[component]),
                                                std::abs(maximum[component])), distance))
            return false;
        band = std::max(band, distance);
    }

    const float largeSize = band + 2.0f;
    const float halfWidth = width * 0.5f;
    if (!std::isfinite(largeSize) || !std::isfinite(halfWidth)) return false;
    std::array<double, 3> low, high;
    for (int component = 0; component < 3; ++component) {
        const double offset = component == axis
            ? -double(extent[component]) * 0.5 + extent[component] * position
            : 0.0;
        const double radius = component == axis ? double(halfWidth) : double(largeSize);
        low[component] = center[component] + offset - radius;
        high[component] = center[component] + offset + radius;
        if (!std::isfinite(low[component]) || !std::isfinite(high[component]) ||
            low[component] > high[component]) return false;
    }
    outputMinimum = low;
    outputMaximum = high;
    return true;
}

inline bool normalizedSignedWeight(double value,
                                   double minimum,
                                   double maximum,
                                   double zero,
                                   bool& isNegative,
                                   float& weight)
{
    if (!std::isfinite(value) || !std::isfinite(minimum) ||
        !std::isfinite(maximum) || !std::isfinite(zero) ||
        minimum > maximum) {
        return false;
    }

    isNegative = value < zero;
    const double extent = isNegative ? zero - minimum : maximum - zero;
    if (!std::isfinite(extent) || extent <= 0.0) {
        weight = 0.0f;
        return true;
    }

    const double rawWeight =
        isNegative ? (zero - value) / extent : (value - zero) / extent;
    if (!std::isfinite(rawWeight)) return false;

    weight = static_cast<float>(
        std::max(0.0, std::min(1.0, rawWeight)));
    return true;
}

} // namespace vdb
} // namespace dommy
