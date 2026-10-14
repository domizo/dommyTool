#pragma once

#include "common/utility/DGenerationLimits.h"

#include <cstddef>
#include <vector>

namespace dommy {
namespace curve {

struct CurveReplacement {
    std::size_t source;
    std::size_t target;
};

struct CurveReplacementPlan {
    std::vector<CurveReplacement> curves;
    std::size_t points = 0U;
};

// Resolve references and bound the complete fan-out before cloning geometry.
// resolve returns a borrowed, stable list and accounts for its lookup/scanning
// work. Missing groups may resolve to an empty list, but failures are explicit.
template <typename Resolve, typename PointCount>
bool buildCurveReplacementPlan(std::size_t sourceCount, std::size_t targetCount,
                               Resolve resolve, PointCount pointCount,
                               CurveReplacementPlan& output)
{
    if (sourceCount > generation::kMaxGeneratedCurveCount ||
        targetCount > generation::kMaxGeneratedCurveCount) return false;
    CurveReplacementPlan candidate;
    std::size_t work = sourceCount;
    for (std::size_t source = 0U; source < sourceCount; ++source) {
        const std::vector<std::size_t>* targets = nullptr;
        if (!resolve(source, targets, work) || !targets ||
            work > generation::kMaxWorkItemCount) return false;
        std::size_t curveCount = 0U;
        if (!generation::checkedSum(candidate.curves.size(), targets->size(),
                generation::kMaxGeneratedCurveCount, curveCount) ||
            !generation::checkedSum(work, targets->size(),
                generation::kMaxWorkItemCount, work)) return false;
        for (std::size_t target : *targets) {
            if (target >= targetCount) return false;
            const std::size_t points = pointCount(target);
            if (points < 2U || !generation::checkedSum(candidate.points, points,
                    generation::kMaxGeneratedPointCount, candidate.points)) return false;
            candidate.curves.push_back(CurveReplacement{source, target});
        }
    }
    output.curves.swap(candidate.curves);
    output.points = candidate.points;
    return true;
}

} // namespace curve
} // namespace dommy
