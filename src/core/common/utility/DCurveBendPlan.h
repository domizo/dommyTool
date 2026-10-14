#pragma once

#include "DGenerationLimits.h"

namespace dommy {
namespace curve {

// Count point transformations and, in curve-normal mode, full CV refreshes.
// This is a bounded work estimate, not a Maya execution-time guarantee.
inline bool appendBendWork(std::size_t pointCount, bool refreshCurve,
                           std::size_t& totalWork)
{
    if (pointCount < 2U || pointCount > generation::kMaxGeneratedPointCount)
        return false;
    const std::size_t first = pointCount % 2U == 0U ? pointCount / 2U : pointCount;
    const std::size_t second = pointCount % 2U == 0U ? pointCount - 1U : (pointCount - 1U) / 2U;
    std::size_t work = 0U;
    if (!generation::checkedProduct(first, second, generation::kMaxWorkItemCount, work))
        return false;
    // Every pivot refresh writes all CVs: n*(n-1), twice the pair count.
    if (refreshCurve &&
        !generation::checkedProduct(work, 3U, generation::kMaxWorkItemCount, work))
        return false;
    return generation::checkedSum(totalWork, work, generation::kMaxWorkItemCount, totalWork);
}

} // namespace curve
} // namespace dommy
