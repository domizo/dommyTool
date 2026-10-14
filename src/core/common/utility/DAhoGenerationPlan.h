#pragma once

#include "DCurveGenerationStorage.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

struct AhoGenerationPlan {
    std::size_t inputCurveCount;
    std::size_t copiesPerCurve;
    std::size_t pointsPerCurve;
    std::size_t outputCurveCount;
    std::size_t totalPointCount;
    std::size_t estimatedBytes;

    AhoGenerationPlan()
        : inputCurveCount(0U),
          copiesPerCurve(0U),
          pointsPerCurve(0U),
          outputCurveCount(0U),
          totalPointCount(0U),
          estimatedBytes(0U)
    {
    }
};

inline bool buildAhoGenerationPlan(
    std::size_t inputCurveCount,
    int rawCopiesPerCurve,
    int rawPointsPerCurve,
    const double* finiteInputs,
    std::size_t finiteInputCount,
    AhoGenerationPlan& result) noexcept
{
    if (finiteInputCount != 0U && !finiteInputs) return false;

    AhoGenerationPlan candidate;
    candidate.inputCurveCount = inputCurveCount;
    if (inputCurveCount > kMaxGeneratedCurveCount ||
        !checkedCount(
            rawCopiesPerCurve, kMaxGeneratedCurveCount,
            candidate.copiesPerCurve) ||
        !checkedCount(
            rawPointsPerCurve, kMaxGeneratedPointCount,
            candidate.pointsPerCurve) ||
        candidate.pointsPerCurve < 2U ||
        !checkedProduct(
            candidate.inputCurveCount, candidate.copiesPerCurve,
            kMaxGeneratedCurveCount, candidate.outputCurveCount) ||
        !checkedProduct(
            candidate.outputCurveCount, candidate.pointsPerCurve,
            kMaxGeneratedPointCount, candidate.totalPointCount)) {
        return false;
    }

    for (std::size_t index = 0U; index < finiteInputCount; ++index) {
        if (!std::isfinite(finiteInputs[index])) return false;
    }

    if (!curveGenerationStorageBytes(candidate.outputCurveCount, candidate.totalPointCount,
                                     candidate.pointsPerCurve, candidate.copiesPerCurve,
                                     candidate.estimatedBytes)) return false;
    result = candidate;
    return true;
}

} // namespace generation
} // namespace dommy
