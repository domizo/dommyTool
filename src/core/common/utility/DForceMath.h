#pragma once

#include "DPointMeanAccumulator.h"

#include <array>
#include <cmath>

namespace dommy {
namespace numeric {
inline bool checkedSpringDampingResponse(double current, double target, double velocity,
                                          double spring, double damping, double scale,
                                          double mass, double& output)
{
    const std::array<double, 7> inputs{{current, target, velocity, spring, damping, scale, mass}};
    bool moderate = true;
    for (double input : inputs) {
        if (!std::isfinite(input)) return false;
        moderate = moderate && detail::hasModerateProductMagnitude(input);
    }
    if (scale == 0.0 || mass == 0.0) { output = 0.0; return true; }
    if (moderate) {
        output = ((target - current) * spring - velocity * damping) * scale * mass;
        return true;
    }
    // Four binary64 factors occupy 8392 bits. 268 words include carry for
    // these three terms, even if their separate products cannot fit double.
    detail::BinaryAccumulator<268U, -4296> sum;
    if (!sum.addProduct4(target, spring, scale, mass) ||
        !sum.addProduct4(-current, spring, scale, mass) ||
        !sum.addProduct4(-velocity, damping, scale, mass)) return false;
    return sum.mean(1U, output);
}

inline bool checkedIntegratedVelocity(double velocity, double force, double inverseMass,
                                       double deltaTime, double& output)
{
    const std::array<double, 4> inputs{{velocity, force, inverseMass, deltaTime}};
    bool moderate = true;
    for (double input : inputs) {
        if (!std::isfinite(input)) return false;
        moderate = moderate && detail::hasModerateProductMagnitude(input);
    }
    if (moderate) {
        output = velocity + force * inverseMass * deltaTime;
        return true;
    }
    detail::BinaryAccumulator<200U, -3222> sum;
    if (!sum.add(velocity) || !sum.addProduct3(force, inverseMass, deltaTime)) return false;
    return sum.mean(1U, output);
}

} // namespace numeric
} // namespace dommy
