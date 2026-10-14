#pragma once

#include "DAttributeBlendMath.h"

#include <maya/MPoint.h>

#include <cmath>

namespace dommy {
namespace maya {

enum class PointBlendRounding {
    CheckedInterpolation,
    LegacyDisplacement
};

// MPoint +/- MVector operates on Cartesian coordinates and returns w == 1.
// Keep the existing curve interpolation policy; mesh callers can explicitly
// retain their historical double displacement rounding for finite interiors.
// Invalid inputs/final coordinates leave output unchanged, including aliases.
inline bool checkedPointBlend(const MPoint& source, const MPoint& target,
                              double weight, MPoint& output,
                              PointBlendRounding rounding = PointBlendRounding::CheckedInterpolation)
{
    if (!std::isfinite(weight) || !std::isfinite(source.w) ||
        !std::isfinite(target.w) || source.w == 0.0 || target.w == 0.0) {
        return false;
    }

    const double sources[3] = {source.x, source.y, source.z};
    const double targets[3] = {target.x, target.y, target.z};
    MPoint candidate;
    double* components[3] = {&candidate.x, &candidate.y, &candidate.z};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!std::isfinite(sources[axis]) || !std::isfinite(targets[axis])) return false;
        // Divide directly rather than multiplying by 1/w, which can overflow
        // even when a tiny homogeneous point has finite Cartesian coordinates.
        const double first = sources[axis] / source.w;
        const double second = targets[axis] / target.w;
        if (!std::isfinite(first) || !std::isfinite(second)) return false;

        if (rounding == PointBlendRounding::LegacyDisplacement) {
            // Select the actual endpoint, including its signed zero. The
            // scalar helper's equal-endpoint shortcut intentionally retains
            // the source, which remains the curve/default policy below.
            if (weight == 0.0) { *components[axis] = first; continue; }
            if (weight == 1.0) { *components[axis] = second; continue; }
            const double legacy = first + (second - first) * weight;
            if (std::isfinite(legacy)) {
                *components[axis] = legacy;
                continue;
            }
        }
        // Use the established curve policy, or recover non-finite mesh
        // intermediates through checked scalar interpolation, without clamping.
        if (!attribute::checkedLinearBlend(first, second, weight, *components[axis])) return false;
    }
    output = candidate;
    return true;
}

} // namespace maya
} // namespace dommy
