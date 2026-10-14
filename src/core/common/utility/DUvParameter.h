#pragma once

#include "DNumericValidation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace uv {

// Closed-domain surface parameters retain an already-in-range end point.
// Outside the domain, wrap relative to its origin without repeated subtraction
// or prematurely rounding away the phase of large offsets. Negative exact
// periods retain the historical upper-seam choice of the legacy duplicater.
inline bool wrapOffsetParameter(double base, double offset, double step,
                                double randomOffset, double domainStart,
                                double domainEnd, double& result)
{
    if (!std::isfinite(base) || !std::isfinite(offset) ||
        !std::isfinite(step) || !std::isfinite(randomOffset) ||
        !std::isfinite(domainStart) || !std::isfinite(domainEnd) ||
        domainEnd <= domainStart) return false;

    const double unwrapped = base + offset + step + randomOffset;
    double candidate = 0.0;
    if (!numeric::wrapPeriodicProductSum(
            std::array<double, 4>{{base, offset, step, randomOffset}},
            std::array<double, 4>{{1.0, 1.0, 1.0, 1.0}},
            domainStart, domainEnd, candidate)) return false;
    result = candidate == domainStart &&
        (unwrapped == domainEnd || unwrapped < domainStart)
        ? domainEnd : candidate;
    return true;
}

inline bool mapTileUV(double u, double v,
                       std::size_t tileU, std::size_t tileV,
                       std::size_t countU, std::size_t countV,
                       float& outputU, float& outputV)
{
    if (countU == 0U || countV == 0U || tileU >= countU || tileV >= countV ||
        !std::isfinite(u) || !std::isfinite(v)) return false;
    float resultU = 0.0f;
    float resultV = 0.0f;
    if (!numeric::checkedFloatCast(
            (u + static_cast<double>(tileU)) / static_cast<double>(countU), resultU) ||
        !numeric::checkedFloatCast(
            (v + static_cast<double>(tileV)) / static_cast<double>(countV), resultV)) {
        return false;
    }
    outputU = resultU;
    outputV = resultV;
    return true;
}

inline bool normalizeParameter(
    double value,
    double domainStart,
    double domainEnd,
    float& result)
{
    if (!std::isfinite(value) ||
        !std::isfinite(domainStart) ||
        !std::isfinite(domainEnd)) {
        return false;
    }

    const double domainSize = domainEnd - domainStart;
    if (!std::isfinite(domainSize) ||
        std::fabs(domainSize) <=
            std::numeric_limits<double>::epsilon()) {
        return false;
    }

    return numeric::checkedFloatCast(
        (value - domainStart) / domainSize, result);
}

} // namespace uv
} // namespace dommy
