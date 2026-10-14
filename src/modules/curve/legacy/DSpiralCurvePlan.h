#pragma once

#include "common/utility/DCurveGenerationStorage.h"

namespace dommy {
namespace curve {

struct SpiralCurvePlan {
    std::size_t samples = 0U, knots = 0U, estimatedBytes = 0U;
};

inline bool buildSpiralCurvePlan(int rawSampling, SpiralCurvePlan& result)
{
    SpiralCurvePlan candidate;
    std::size_t sampling = 0U;
    if (!generation::checkedCount(rawSampling, generation::kMaxGeneratedPointCount, sampling) ||
        sampling < 2U ||
        !generation::checkedSum(sampling, 2U, generation::kMaxGeneratedPointCount, candidate.samples) ||
        !generation::checkedSum(candidate.samples, 2U, generation::kMaxGeneratedPointCount, candidate.knots) ||
        !generation::addCurveGenerationBytes(1U, 4096U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(candidate.samples, 256U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(candidate.knots, 32U, candidate.estimatedBytes)) return false;
    // Two quaternion arrays, source/output points, native curve construction,
    // knot arrays and conservative capacity slack. Admission precedes reserve().
    result = candidate;
    return true;
}

} // namespace curve
} // namespace dommy
