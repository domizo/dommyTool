#pragma once

#include "common/utility/DGenerationLimits.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace curve {

struct WoolCurvePlan
{
    std::size_t generatedPointCount;
    std::size_t curveCvCount;
    std::size_t knotCount;
    unsigned int degree;
    double radiusStep;

    WoolCurvePlan()
        : generatedPointCount(0U),
          curveCvCount(0U),
          knotCount(0U),
          degree(3U),
          radiusStep(0.0)
    {
    }
};

inline bool buildWoolCurvePlan(double startRadius,
                               double endRadius,
                               int roundCount,
                               int sectionCount,
                               double zTranslation,
                               WoolCurvePlan& plan)
{
    if (!std::isfinite(startRadius) || !std::isfinite(endRadius) ||
        !std::isfinite(zTranslation) || startRadius < 0.0 ||
        endRadius <= startRadius || roundCount < 1 || sectionCount < 3) {
        return false;
    }

    std::size_t rounds = 0U;
    std::size_t sections = 0U;
    WoolCurvePlan candidate;
    if (!generation::checkedCount(
            roundCount, generation::kMaxCurveSampleCount, rounds) ||
        !generation::checkedCount(
            sectionCount, generation::kMaxCurveSampleCount, sections) ||
        !generation::checkedProduct(
            rounds, sections, generation::kMaxCurveSampleCount,
            candidate.generatedPointCount) ||
        !generation::checkedSum(
            candidate.generatedPointCount, 1U,
            generation::kMaxGeneratedPointCount, candidate.curveCvCount) ||
        candidate.curveCvCount <= candidate.degree ||
        !generation::checkedSum(
            candidate.curveCvCount,
            static_cast<std::size_t>(candidate.degree) - 1U,
            generation::kMaxGeneratedPointCount, candidate.knotCount)) {
        return false;
    }

    candidate.radiusStep =
        (endRadius - startRadius) /
        static_cast<double>(candidate.generatedPointCount);
    if (!std::isfinite(candidate.radiusStep) ||
        candidate.radiusStep <= 0.0) {
        return false;
    }

    plan = candidate;
    return true;
}

} // namespace curve
} // namespace dommy
