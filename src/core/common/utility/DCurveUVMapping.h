#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace curve {

struct UVMappingPlan {
    UVMappingPlan()
        : useRamp(false), useArcLength(false), minimum(0.0), maximum(0.0),
          span(0.0), scale(1.0), offset(0.0) {}

    bool useRamp;
    bool useArcLength;
    double minimum;
    double maximum;
    double span;
    double scale;
    double offset;
};

inline bool buildUVMappingPlan(short mappingType,
                               short calculationType,
                               double minimum,
                               double maximum,
                               double scale,
                               double offset,
                               UVMappingPlan& plan)
{
    if ((mappingType != 0 && mappingType != 1) ||
        (calculationType != 0 && calculationType != 1) ||
        !std::isfinite(minimum) || !std::isfinite(maximum) ||
        !std::isfinite(scale) || !std::isfinite(offset)) {
        return false;
    }

    const double span = maximum - minimum;
    const double mappedMinimum = minimum * scale + offset;
    const double mappedMaximum = maximum * scale + offset;
    if (!std::isfinite(span) || !std::isfinite(mappedMinimum) ||
        !std::isfinite(mappedMaximum)) {
        return false;
    }

    UVMappingPlan candidate;
    candidate.useRamp = mappingType == 1;
    candidate.useArcLength = calculationType == 1;
    candidate.minimum = minimum;
    candidate.maximum = maximum;
    candidate.span = span;
    candidate.scale = scale;
    candidate.offset = offset;
    plan = candidate;
    return true;
}

// A one-point curve is not valid DCurve topology, but defining the result as
// the start of the range keeps this helper safe for callers outside DCurve.
inline bool pointUVPosition(std::size_t index,
                            std::size_t pointCount,
                            double& position)
{
    if (pointCount == 0U || index >= pointCount) return false;
    position = pointCount == 1U
        ? 0.0
        : static_cast<double>(index) /
              static_cast<double>(pointCount - 1U);
    return std::isfinite(position);
}

inline bool arcLengthUVPosition(double partialLength,
                                double totalLength,
                                double& position)
{
    if (!std::isfinite(partialLength) || !std::isfinite(totalLength) ||
        partialLength < 0.0 || totalLength < 0.0) {
        return false;
    }
    if (totalLength <= std::numeric_limits<double>::epsilon()) {
        position = 0.0;
        return true;
    }

    const double normalized = partialLength / totalLength;
    if (!std::isfinite(normalized)) return false;
    position = std::max(0.0, std::min(1.0, normalized));
    return true;
}

inline bool mapUVValue(const UVMappingPlan& plan,
                       double normalizedValue,
                       double& mappedValue)
{
    if (!std::isfinite(normalizedValue)) return false;
    // Select by the sampled value, not curve position. Reconstructing the
    // maximum from minimum + span can lose a small endpoint to cancellation.
    const double value = normalizedValue == 0.0 ? plan.minimum :
        normalizedValue == 1.0 ? plan.maximum :
        plan.minimum + normalizedValue * plan.span;
    mappedValue = value * plan.scale + plan.offset;
    return std::isfinite(mappedValue);
}

} // namespace curve
} // namespace dommy
