#pragma once

#include "common/utility/DNumericValidation.h"
#include <cmath>

namespace dommy {
namespace bullet {

const int kMaxSimulationSteps = 10000;
const int kMaxOuterSubsteps = 1024;

struct SimulationStepPlan {
    float seconds;
    float fixedSeconds;
    int substeps;
};

inline bool validSimulationSettings(int substeps, int hertz)
{
    return substeps > 0 && substeps <= kMaxOuterSubsteps &&
        hertz > 0 && hertz <= 1000000;
}

inline bool prepareSimulationStep(double duration, int substeps, int hertz,
                                  SimulationStepPlan& output)
{
    float seconds = 0;
    if (!validSimulationSettings(substeps, hertz) || duration <= 0 ||
        !numeric::checkedFloatCast(duration, seconds)) return false;
    const SimulationStepPlan candidate = {seconds / substeps, 1.f / hertz, substeps};
    if (!std::isnormal(candidate.seconds)) return false;
    output = candidate;
    return true;
}

// Mirror Bullet's scalar arithmetic, including its carried remainder. Check
// BEFORE its float-to-int conversion and before the clamp can discard time.
// The work budget covers the entire Maya update, not each outer substep alone.
template <typename Scalar>
inline bool simulationStepBudget(Scalar remainder, Scalar seconds,
                                 Scalar fixedSeconds, int substeps)
{
    if (substeps < 1 || substeps > kMaxOuterSubsteps ||
        !std::isfinite(remainder) ||
        !std::isfinite(seconds) || seconds < 0 ||
        !std::isnormal(fixedSeconds) || fixedSeconds <= 0 ||
        !std::isfinite(Scalar(1) / fixedSeconds)) return false;
    int available = kMaxSimulationSteps;
    for (int i = 0; i < substeps; ++i) {
        remainder += seconds;
        if (!std::isfinite(remainder)) return false;
        if (remainder >= fixedSeconds) {
            const Scalar quotient = remainder / fixedSeconds;
            if (!std::isfinite(quotient) || quotient >= Scalar(available + 1)) return false;
            const int count = static_cast<int>(quotient);
            available -= count;
            remainder -= count * fixedSeconds;
            // Bullet's subtraction may produce a tiny negative remainder
            // through rounding; preserve that debt instead of rejecting it.
            if (!std::isfinite(remainder)) return false;
        }
    }
    return true;
}

} // namespace bullet
} // namespace dommy
