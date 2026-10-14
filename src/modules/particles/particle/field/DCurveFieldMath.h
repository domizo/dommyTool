#pragma once

#include "common/utility/DNumericValidation.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace field {

inline double clampUnit(double value) {
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

inline bool normalizeRange(double value, double minimum, double maximum, float& normalized) {
    normalized = 0.0f;
    double candidate = 0.0;
    if (!numeric::normalizeRange(value, minimum, maximum, candidate))
        return false;
    return numeric::clampUnitToFloat(candidate, normalized);
}

inline bool validMassCount(std::size_t pointCount, std::size_t massCount) {
    return pointCount == 0 || massCount == 1 || massCount == pointCount;
}

} // namespace field
} // namespace dommy
