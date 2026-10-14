#pragma once

#include "DProceduralMath.h"
#include "DVectorMath.h"
#include "math/DRemapMath.h"

namespace dommy {
namespace math {

// Callers preserve the float Maya range attributes, but promote them before
// arithmetic. Collapsed ranges retain the historical destination-minimum rule.
inline bool fitFiniteValue(double value, double sourceMin, double sourceMax,
                           double destinationMin, double destinationMax, double& result)
{
    if (!std::isfinite(value) || !std::isfinite(sourceMin) || !std::isfinite(sourceMax) ||
        !std::isfinite(destinationMin) || !std::isfinite(destinationMax)) return false;
    double normalized = 0.0;
    if (sourceMin != sourceMax &&
        !normalizeRange(value, sourceMin, sourceMax, normalized)) return false;
    if (sourceMin == sourceMax) { result = destinationMin; return true; }
    return checkedLinearInterpolation(destinationMin, destinationMax, normalized, result);
}

template <typename Values, typename Operation>
bool mapUnaryValues(Values& values, Operation operation)
{
    if (!validMathBudget(values.size(), 0U)) return false;
    Values candidate(values);
    for (auto& value : candidate) if (!operation(value)) return false;
    values.swap(candidate);
    return true;
}

template <typename Values, typename Range>
bool fitValues(Values& values, const Range& sourceMin, const Range& sourceMax,
               const Range& destinationMin, const Range& destinationMax)
{
    using Components = numeric::ValueComponents<typename Values::value_type>;
    return mapUnaryValues(values, [&](typename Values::value_type& value) {
        for (unsigned int axis = 0U; axis < Components::count; ++axis) {
            double result;
            if (!fitFiniteValue(Components::get(value, axis), sourceMin[axis], sourceMax[axis],
                    destinationMin[axis], destinationMax[axis], result) ||
                !numeric::checkedScalarCast(result, Components::get(value, axis))) return false;
        }
        return true;
    });
}

// The sampler is the only float boundary (MRampAttribute). A failed sample or
// unrepresentable position is an error, never an implicit identity-ramp result.
template <typename Sample>
bool remapValue(double value, double minimum, double maximum, bool keepRange,
                Sample sample, double& result)
{
    if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum)) return false;
    double normalized = 0.0;
    if (minimum != maximum && !normalizeRange(value, minimum, maximum, normalized)) return false;
    float position = 0.0f, sampled = 0.0f;
    if (!numeric::checkedFloatCast(normalized, position) ||
        !sample(position, sampled) || !std::isfinite(sampled)) return false;
    if (keepRange) return checkedLinearInterpolation(minimum, maximum, double(sampled), result);
    result = double(sampled);
    return true;
}

template <typename Values, typename Settings, typename Sample>
bool remapValues(Values& values, const Settings& settings, Sample sample)
{
    using Components = numeric::ValueComponents<typename Values::value_type>;
    return mapUnaryValues(values, [&](typename Values::value_type& value) {
        for (unsigned int axis = 0U; axis < Components::count; ++axis) {
            double result;
            if (!remapValue(Components::get(value, axis), settings.minimum[axis],
                    settings.maximum[axis], settings.keepRange,
                    [&](float position, float& sampled) { return sample(axis, position, sampled); },
                    result) || !numeric::checkedScalarCast(result, Components::get(value, axis))) return false;
        }
        return true;
    });
}

template <typename Values>
bool normalizeValues(Values& values)
{
    using Components = numeric::ValueComponents<typename Values::value_type>;
    if (Components::count != 2U && Components::count != 3U) return false;
    return mapUnaryValues(values, [](typename Values::value_type& value) {
        for (unsigned int axis = 0U; axis < Components::count; ++axis)
            if (!std::isfinite(Components::get(value, axis))) return false;
        double x = Components::get(value, 0U), y = Components::get(value, 1U), z = 0.0;
        if (Components::count == 3U) z = Components::get(value, 2U);
        double nx = 0.0, ny = 0.0, nz = 0.0;
        if (x != 0.0 || y != 0.0 || z != 0.0) {
            const bool ok = Components::count == 2U ? normalized2OrZero(x, y, nx, ny)
                                                   : normalized3OrZero(x, y, z, nx, ny, nz);
            if (!ok) return false;
        }
        if (!numeric::checkedScalarCast(nx, Components::get(value, 0U)) ||
            !numeric::checkedScalarCast(ny, Components::get(value, 1U))) return false;
        return Components::count == 2U || numeric::checkedScalarCast(nz, Components::get(value, 2U));
    });
}

struct FitOperation {
    double sourceMin[3], sourceMax[3], destinationMin[3], destinationMax[3];
    template <typename Values> bool operator()(Values& values) const {
        return fitValues(values, sourceMin, sourceMax, destinationMin, destinationMax);
    }
};

struct NormalizeOperation {
    template <typename Values> bool operator()(Values& values) const {
        return normalizeValues(values);
    }
};

} // namespace math
} // namespace dommy
