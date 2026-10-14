#pragma once

#include <maya/MColor.h>
#include <maya/MRampAttribute.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>

#include <cmath>

namespace dommy {
namespace maya {

// Bind only ramps used by the selected mode. Keep the previous binding on a
// returned acquisition failure; exceptions belong to the owning entry point.
inline MStatus bindValueRamp(const MPlug& plug, MRampAttribute& result)
{
    MStatus status;
    const MRampAttribute candidate(plug, &status);
    if (!status) return status;
    result = candidate;
    return MS::kSuccess;
}

inline bool sampleFiniteRamp(
    MRampAttribute& ramp, const double position, float& value)
{
    if (!std::isfinite(position) || position < 0.0 || position > 1.0) {
        return false;
    }
    MStatus status;
    ramp.getValueAtPosition(
        static_cast<float>(position), value, &status);
    return status == MS::kSuccess && std::isfinite(value);
}

inline bool sampleFiniteColorRamp(
    MRampAttribute& ramp, const double position, MColor& color)
{
    if (!std::isfinite(position) || position < 0.0 || position > 1.0) {
        return false;
    }
    MStatus status;
    ramp.getColorAtPosition(
        static_cast<float>(position), color, &status);
    return status == MS::kSuccess &&
        std::isfinite(color.r) &&
        std::isfinite(color.g) &&
        std::isfinite(color.b) &&
        std::isfinite(color.a);
}

} // namespace maya
} // namespace dommy
