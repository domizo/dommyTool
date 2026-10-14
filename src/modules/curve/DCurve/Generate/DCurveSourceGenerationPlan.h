#pragma once

#include "common/utility/DCurveGenerationStorage.h"

namespace dommy {
namespace curve {

struct LinkGenerationPlan {
    std::size_t curveCount = 0U, segmentCount = 0U, pointsPerCurve = 0U;
    std::size_t outputPointCount = 0U, estimatedBytes = 0U;
};

inline bool checkedLinkGenerationPlan(std::size_t links, int rawCurves, int rawSegments,
                                      LinkGenerationPlan& result)
{
    LinkGenerationPlan candidate;
    std::size_t sampledPoints = 0U, segmentPoints = 0U, work = 0U;
    if (links < 2U || links > generation::kMaxConnectedGeometryCount ||
        !generation::checkedCount(rawCurves, generation::kMaxGeneratedCurveCount, candidate.curveCount) ||
        candidate.curveCount == 0U ||
        !generation::checkedCount(rawSegments, generation::kMaxCurveSampleCount, candidate.segmentCount) ||
        !generation::checkedProduct(links, candidate.curveCount, generation::kMaxWorkItemCount, sampledPoints) ||
        !generation::checkedProduct(links - 1U, candidate.segmentCount,
            generation::kMaxGeneratedPointCount, segmentPoints) ||
        !generation::checkedSum(links, segmentPoints, generation::kMaxGeneratedPointCount, candidate.pointsPerCurve) ||
        !generation::checkedProduct(candidate.curveCount, candidate.pointsPerCurve,
            generation::kMaxGeneratedPointCount, candidate.outputPointCount) ||
        !generation::checkedSum(sampledPoints, candidate.outputPointCount, generation::kMaxWorkItemCount, work) ||
        !generation::curveGenerationStorageBytes(candidate.curveCount, candidate.outputPointCount,
            candidate.pointsPerCurve, 0U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(sampledPoints, 64U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(candidate.curveCount, 64U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(links, 128U, candidate.estimatedBytes)) return false;
    // Retained source point arrays, per-object sampling/random arrays, array objects.
    result = candidate;
    return true;
}

inline bool checkedMeshCurveStorage(std::size_t points, std::size_t edges, std::size_t& result)
{
    std::size_t cvs = 0U, bytes = 0U;
    if (points > generation::kMaxGeneratedPointCount || edges > generation::kMaxGeneratedCurveCount ||
        !generation::checkedProduct(edges, 2U, generation::kMaxGeneratedPointCount, cvs) ||
        !generation::curveGenerationStorageBytes(edges, cvs, edges == 0U ? 0U : 2U, 0U, bytes) ||
        !generation::addCurveGenerationBytes(points, 64U, bytes)) return false;
    result = bytes;
    return true;
}

} // namespace curve
} // namespace dommy
