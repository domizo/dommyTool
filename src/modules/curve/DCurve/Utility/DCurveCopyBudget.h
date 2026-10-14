#pragma once

#include "common/utility/DGenerationLimits.h"

#include <cstddef>

namespace dommy {
namespace curve {

inline bool checkedCurveCopyCounts(int rawCountU,
                                   int rawCountV,
                                   std::size_t& countU,
                                   std::size_t& countV,
                                   std::size_t& gridCount)
{
    if (!generation::checkedCount(
            rawCountU, generation::kMaxGeneratedCurveCount, countU) ||
        !generation::checkedCount(
            rawCountV, generation::kMaxGeneratedCurveCount, countV) ||
        !generation::checkedProduct(
            countU, countV, generation::kMaxGeneratedCurveCount,
            gridCount)) {
        return false;
    }
    return true;
}

// Connected output can add up to two seam CVs per instance. Budget using that
// upper bound so both split and connected modes share one safe calculation.
inline bool checkedCurveCopyBudget(std::size_t instanceCount,
                                   std::size_t sourceCvCount,
                                   std::size_t& maximumOutputCvCount)
{
    if (instanceCount > generation::kMaxGeneratedCurveCount ||
        sourceCvCount == 0U) {
        return false;
    }

    std::size_t cvsPerInstance = 0U;
    return generation::checkedSum(
               sourceCvCount, 2U,
               generation::kMaxGeneratedPointCount, cvsPerInstance) &&
           generation::checkedProduct(
               instanceCount, cvsPerInstance,
               generation::kMaxGeneratedPointCount,
               maximumOutputCvCount) &&
           maximumOutputCvCount <= generation::kMaxWorkItemCount;
}

} // namespace curve
} // namespace dommy
