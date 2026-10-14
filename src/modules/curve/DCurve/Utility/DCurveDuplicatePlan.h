#pragma once

#include "common/utility/DCheckedSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointTransform.h"

#include <cstddef>
#include <vector>

namespace dommy {
namespace curve {

inline bool validDuplicateComparisonBudget(std::size_t curveCount)
{
    std::size_t pairs = 0U;
    return curveCount <= generation::kMaxGeneratedCurveCount &&
        (curveCount < 2U || (memory::checkedMultiply(curveCount, curveCount - 1U, pairs) &&
                            pairs / 2U <= generation::kMaxWorkItemCount));
}

template <typename DuplicatePredicate>
inline bool buildDuplicateDeletionPlan(
    const std::size_t curveCount,
    DuplicatePredicate isDuplicate,
    std::vector<bool>& deleteFlags)
{
    if (!validDuplicateComparisonBudget(curveCount)) return false;

    std::vector<bool> candidate(curveCount, false);
    for (std::size_t first = 0U; first < curveCount; ++first) {
        for (std::size_t second = first + 1U; second < curveCount; ++second) {
            if (isDuplicate(first, second)) {
                // Preserve the historical behavior: delete the lower index and
                // retain the last curve in an otherwise identical group.
                candidate[first] = true;
            }
        }
    }
    deleteFlags.swap(candidate);
    return true;
}

// Compare actual world-space line endpoints in double precision, preserving
// the exact-match and last-duplicate-survives policy. Keep source geometry and
// metadata untouched; a failed transform cannot publish a partial deletion mask.
template <typename GetPoints, typename GetMatrix>
bool buildWorldLineDeletionPlan(std::size_t curveCount, GetPoints getPoints,
                                GetMatrix getMatrix, std::vector<bool>& output)
{
    if (!validDuplicateComparisonBudget(curveCount)) return false;
    typedef std::array<std::array<double, 3>, 2> Endpoints;
    std::vector<Endpoints> endpoints(curveCount);
    std::vector<bool> lines(curveCount, false);
    for (std::size_t i = 0U; i < curveCount; ++i) {
        const auto& points = getPoints(i);
        if (points.size() != 2U) continue;
        const auto matrix = getMatrix(i);
        if (!numeric::isFiniteAffineTransform(matrix) ||
            !numeric::checkedTransformPoint(points[0], matrix, endpoints[i][0]) ||
            !numeric::checkedTransformPoint(points[1], matrix, endpoints[i][1])) return false;
        lines[i] = true;
    }
    return buildDuplicateDeletionPlan(curveCount,
        [&](std::size_t first, std::size_t second) {
            return lines[first] && lines[second] &&
                ((endpoints[first][0] == endpoints[second][0] &&
                  endpoints[first][1] == endpoints[second][1]) ||
                 (endpoints[first][0] == endpoints[second][1] &&
                  endpoints[first][1] == endpoints[second][0]));
        }, output);
}

} // namespace curve
} // namespace dommy
