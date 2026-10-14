#pragma once

#include "common/utility/DCurveGenerationStorage.h"
#include <algorithm>

namespace dommy {
namespace curve {

struct CurveConversionPlan {
    std::size_t curves = 0U, groups = 0U, inputSlots = 0U;
    std::size_t countedCurves = 0U, points = 0U, knots = 0U, largestCurve = 0U;
    std::size_t estimatedBytes = 0U;
};

inline bool curveConversionStorage(const CurveConversionPlan& plan, std::size_t& result)
{
    std::size_t bytes = 0U, masks = 0U, work = 0U;
    if (plan.curves > generation::kMaxGeneratedCurveCount || plan.groups > plan.curves ||
        plan.inputSlots < plan.curves || plan.inputSlots > generation::kMaxConnectedGeometryCount ||
        plan.points > generation::kMaxGeneratedPointCount ||
        plan.knots > 2U * generation::kMaxGeneratedPointCount ||
        !generation::checkedProduct(plan.groups, plan.curves, generation::kMaxWorkItemCount, masks) ||
        !generation::checkedSum(masks, plan.curves, generation::kMaxWorkItemCount, work) ||
        !generation::curveGenerationStorageBytes(plan.curves, plan.points, plan.largestCurve, 0U, bytes) ||
        !generation::addCurveGenerationBytes(plan.inputSlots, 128U, bytes) ||
        !generation::addCurveGenerationBytes(plan.knots, 32U, bytes) ||
        !generation::addCurveGenerationBytes(masks, 4U, bytes) ||
        !generation::addCurveGenerationBytes(plan.groups, 576U, bytes)) return false;
    // Native CV/knot scratch, staged + committed curves, DAG/array handles,
    // count/member vectors and group masks/copies/names. Excludes upstream DG caches.
    result = bytes;
    return true;
}

inline bool initializeCurveConversionPlan(std::size_t curves, std::size_t groups,
                                           std::size_t inputSlots, CurveConversionPlan& result)
{
    CurveConversionPlan candidate;
    candidate.curves = curves;
    candidate.groups = groups;
    candidate.inputSlots = inputSlots;
    if (!curveConversionStorage(candidate, candidate.estimatedBytes)) return false;
    result = candidate;
    return true;
}

inline bool admitCurveConversionCounts(int rawCvs, int rawKnots, CurveConversionPlan& plan)
{
    CurveConversionPlan candidate = plan;
    std::size_t cvs = 0U, knots = 0U;
    if (candidate.countedCurves >= candidate.curves ||
        !generation::checkedCount(rawCvs, generation::kMaxGeneratedPointCount, cvs) || cvs < 2U ||
        !generation::checkedCount(rawKnots, 2U * generation::kMaxGeneratedPointCount, knots) ||
        knots < cvs || knots >= 2U * cvs ||
        !generation::checkedSum(candidate.points, cvs, generation::kMaxGeneratedPointCount, candidate.points) ||
        !generation::checkedSum(candidate.knots, knots, 2U * generation::kMaxGeneratedPointCount, candidate.knots))
        return false;
    ++candidate.countedCurves;
    candidate.largestCurve = std::max(candidate.largestCurve, cvs);
    if (!curveConversionStorage(candidate, candidate.estimatedBytes)) return false;
    plan = candidate;
    return true;
}

} // namespace curve
} // namespace dommy
