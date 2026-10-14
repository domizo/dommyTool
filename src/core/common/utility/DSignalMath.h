#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include "DNumericValidation.h"
#include "DScaledProduct.h"
#include "DProductSum.h"

namespace dommy {
namespace signal {

inline bool weightMapPosition(double base, double offset, int wrapMode, double& result)
{
    if (!std::isfinite(base) || !std::isfinite(offset) || wrapMode < 0 || wrapMode > 2) return false;
    const double moderate = std::ldexp(1.0, 100);
    if (std::fabs(base) <= moderate && std::fabs(offset) <= moderate) {
        const double position = base + offset;
        if (wrapMode != 2) {
            result = wrapMode == 1 ? std::max(0.0, std::min(1.0, position)) : position;
            return true;
        }
        if (numeric::detail::additionResidual(base, offset, position) == 0.0)
            return numeric::wrapPeriodic(position, 0.0, 1.0, result);
    }
    if (wrapMode == 0) return numeric::checkedProductSum(
        std::array<double, 2>{{base, offset}}, std::array<double, 2>{{1.0, 1.0}}, result);
    numeric::detail::PointMeanAccumulator position;
    if (!position.add(base) || !position.add(offset)) return false;
    if (wrapMode == 1) {
        if (position.sign() <= 0) { result = 0.0; return true; }
        numeric::detail::PointMeanAccumulator upper(position);
        if (!upper.add(-1.0)) return false;
        if (upper.sign() >= 0) { result = 1.0; return true; }
        return position.mean(1U, result);
    }
    numeric::detail::PointMeanAccumulator period, origin;
    double candidate = 0.0;
    if (!period.add(1.0) || !position.periodicValue(period, origin, candidate)) return false;
    result = candidate >= 1.0 ? 0.0 : candidate;
    return true;
}

// Return a ramp position in [0,1]. Non-repeating distances use the ramp's
// endpoint outside the influence radius; repeated distances form a triangle
// wave in physical distance units, before quotient rounding can lose phase.
inline bool weightMapDistance(double position, std::size_t index, std::size_t count,
                              double distance, bool repeat, double& result)
{
    if (!std::isfinite(position) || !std::isfinite(distance) ||
        distance <= std::numeric_limits<float>::epsilon() || count == 0U || index >= count)
        return false;
    const double part = count == 1U ? 0.0 : double(index) / double(count - 1U);
    const double moderate = std::ldexp(1.0, 100);
    double candidate = 0.0;
    bool fast = std::fabs(position) <= moderate;
    double delta = 0.0;
    if (fast) {
        delta = position - part;
        fast = numeric::detail::additionResidual(position, -part, delta) == 0.0;
    }
    if (fast) {
        const double magnitude = std::fabs(delta);
        if (magnitude <= distance) candidate = magnitude / distance;
        else if (!repeat) candidate = 1.0;
        else {
            // Here distance < magnitude <= 2^100+1, so doubling it is safe.
            const double period = distance * 2.0;
            double folded = std::fmod(magnitude, period);
            if (folded > distance) folded = period - folded;
            candidate = folded / distance;
        }
    } else {
        numeric::detail::PointMeanAccumulator exactDelta;
        if (!exactDelta.add(position) || !exactDelta.add(-part)) return false;
        if (!repeat) {
            numeric::detail::PointMeanAccumulator lower(exactDelta), upper(exactDelta);
            if (!lower.add(distance) || !upper.add(-distance)) return false;
            if (lower.sign() <= 0 || upper.sign() >= 0) candidate = 1.0;
            else {
                if (!exactDelta.mean(1U, delta)) return false;
                candidate = std::fabs(delta) / distance;
            }
        } else {
            // Center the exact remainder in [-distance,distance). The period
            // may exceed DBL_MAX, while this centered output never needs to.
            numeric::detail::PointMeanAccumulator period, origin;
            if (!exactDelta.add(distance) || !period.add(distance) ||
                !period.add(distance) || !origin.add(-distance) ||
                !exactDelta.periodicValue(period, origin, delta)) return false;
            candidate = std::fabs(delta) / distance;
        }
    }
    if (!std::isfinite(candidate)) return false;
    result = std::max(0.0, std::min(1.0, candidate));
    return true;
}

struct TimeSample {
    double position;
    double completedCycles;

    TimeSample() : position(0.0), completedCycles(0.0) {}
};

// Keep the three ordered terms of the legacy base/ramp/loop expression
// together until the extreme-path final rounding. Disabled loops use zero
// cycles; disabled strengths still cannot hide non-finite original inputs.
inline bool outputValue(double base, double mapped, double lastMapped, double cycles,
                        double strength, double uniformStrength, double& result)
{
    return numeric::checkedProductChainSum(std::array<std::array<double, 4>, 3>{{
        {{base, 1.0, 1.0, 1.0}}, {{mapped, strength, uniformStrength, 1.0}},
        {{lastMapped, cycles, strength, uniformStrength}}}}, result);
}

inline bool applyModulation(double current, double sample, double amplitude, double strength,
                            double uniformStrength, bool multiply, double& result)
{
    bool moderate = true;
    for (double input : {current, sample, amplitude, strength, uniformStrength}) {
        if (!std::isfinite(input)) return false;
        moderate = moderate && numeric::detail::hasModerateProductMagnitude(input);
    }
    if (moderate) {
        const double value = sample * amplitude;
        const double modulation = value * strength * uniformStrength;
        result = multiply ? current * modulation : current + modulation;
        return true;
    }
    if (multiply) return numeric::scaledProduct(
        std::array<double, 5>{{current, sample, amplitude, strength, uniformStrength}}, result);
    return numeric::checkedProductChainSum(std::array<std::array<double, 4>, 2>{{
        {{current, 1.0, 1.0, 1.0}}, {{sample, amplitude, strength, uniformStrength}}}}, result);
}

inline bool sampleTime(double currentTime,
                       double timeScale,
                       double rangeMinimum,
                       double rangeMaximum,
                       bool loop,
                       TimeSample& result) noexcept
{
    if (!std::isfinite(currentTime) || !std::isfinite(timeScale) ||
        !std::isfinite(rangeMinimum) || !std::isfinite(rangeMaximum)) {
        return false;
    }

    const double minimum = std::min(rangeMinimum, rangeMaximum);
    const double maximum = std::max(rangeMinimum, rangeMaximum);
    double scaledTime = 0.0;
    if (!numeric::validAscendingRange(minimum, maximum) ||
        !numeric::scaledProduct(std::array<double, 2>{{currentTime, timeScale}}, scaledTime)) return false;
    TimeSample candidate;
    if (!loop && (scaledTime <= minimum || scaledTime >= maximum)) {
        candidate.position = scaledTime <= minimum ? 0.0 : 1.0;
        result = candidate;
        return true;
    }
    double normalized = 0.0;
    if (!numeric::normalizeRange(scaledTime, minimum, maximum, normalized)) return false;
    if (loop) {
        candidate.completedCycles = std::floor(normalized);
        candidate.position = normalized - candidate.completedCycles;
        // A large rounded quotient loses fractional cycles. Compute the
        // periodic position from the original time instead, without a cast.
        if (std::fabs(normalized) >= std::ldexp(1.0, 52)) {
            double wrappedTime = 0.0;
            if (!numeric::wrapPeriodic(scaledTime, minimum, maximum, wrappedTime) ||
                !numeric::normalizeRange(wrappedTime, minimum, maximum, candidate.position)) return false;
        }
    } else {
        candidate.position = std::max(0.0, std::min(1.0, normalized));
    }
    if (!std::isfinite(candidate.position) ||
        !std::isfinite(candidate.completedCycles) ||
        candidate.position < 0.0 || candidate.position > 1.0) {
        return false;
    }

    result = candidate;
    return true;
}

inline bool mappedRampValue(double rangeMinimum,
                            double rangeMaximum,
                            float rampValue,
                            double& result) noexcept
{
    if (!std::isfinite(rangeMinimum) || !std::isfinite(rangeMaximum) ||
        !std::isfinite(rampValue)) {
        return false;
    }
    const double minimum = std::min(rangeMinimum, rangeMaximum);
    const double maximum = std::max(rangeMinimum, rangeMaximum);
    return math::checkedLinearInterpolation(minimum, maximum, double(rampValue), result);
}

} // namespace signal
} // namespace dommy
