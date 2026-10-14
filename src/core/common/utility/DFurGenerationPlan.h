#pragma once

#include "DCurveGenerationStorage.h"

namespace dommy {
namespace generation {

struct FurGenerationPlan {
    std::size_t curveCount = 0U;
    std::size_t pointsPerCurve = 0U;
    std::size_t totalPointCount = 0U;
    std::size_t estimatedBytes = 0U;
};

// Before thinning/length filtering: do not admit a request based on random
// outcomes that would require allocations merely to discover its actual size.
inline bool buildFurGenerationPlan(std::size_t pointCount, int segments,
                                    FurGenerationPlan& result) noexcept
{
    FurGenerationPlan candidate;
    candidate.curveCount = pointCount;
    if (segments < 2 ||
        static_cast<std::size_t>(segments) >= kMaxCurveSampleCount ||
        pointCount > kMaxGeneratedCurveCount ||
        !checkedSum(static_cast<std::size_t>(segments), 1U, kMaxGeneratedPointCount,
                    candidate.pointsPerCurve) ||
        !checkedProduct(pointCount, candidate.pointsPerCurve, kMaxGeneratedPointCount,
                        candidate.totalPointCount) ||
        !curveGenerationStorageBytes(pointCount, candidate.totalPointCount,
                                     candidate.pointsPerCurve, pointCount,
                                     candidate.estimatedBytes)) return false;
    result = candidate;
    return true;
}

} // namespace generation
} // namespace dommy
