#pragma once

#include "DNumericValidation.h"
#include "DForceMath.h"

#include <cmath>

namespace dommy {
namespace point {

inline bool validateGoalParameters(double springConstant,
                                   double damping,
                                   double magnitude)
{
    return std::isfinite(springConstant) &&
           std::isfinite(damping) &&
           std::isfinite(magnitude);
}

inline bool computeGoalVelocityComponent(double currentPosition,
                                         double targetPosition,
                                         double currentVelocity,
                                         double springConstant,
                                         double damping,
                                         double magnitude,
                                         double mass,
                                         float& result)
{
    if (!std::isfinite(currentPosition) ||
        !std::isfinite(targetPosition) ||
        !std::isfinite(currentVelocity) ||
        !validateGoalParameters(springConstant, damping, magnitude) ||
        !std::isfinite(mass)) {
        return false;
    }

    double candidate = 0.0;
    return numeric::checkedSpringDampingResponse(currentPosition, targetPosition,
        currentVelocity, springConstant, damping, magnitude, mass, candidate) &&
        numeric::checkedFloatCast(candidate, result);
}

} // namespace point
} // namespace dommy
