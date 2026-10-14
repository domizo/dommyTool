#pragma once

#include "common/utility/DCurveGenerationStorage.h"

#include <cmath>
#include <cstddef>
#include <algorithm>

namespace dommy {
namespace curve {

struct ClothSamplingPlan {
    ClothSamplingPlan()
        : samplingU(0U),
          samplingV(0U),
          curveCountU(0U),
          curveCountV(0U),
          pointsPerUCurve(0U),
          pointsPerVCurve(0U),
          totalCurveCount(0U),
          totalPointCount(0U),
          totalSurfaceQueries(0U),
          estimatedBytes(0U),
          startU(0.0),
          endU(0.0),
          startV(0.0),
          endV(0.0),
          stepU(0.0),
          stepV(0.0)
    {
    }

    std::size_t samplingU;
    std::size_t samplingV;
    std::size_t curveCountU;
    std::size_t curveCountV;
    std::size_t pointsPerUCurve;
    std::size_t pointsPerVCurve;
    std::size_t totalCurveCount;
    std::size_t totalPointCount;
    std::size_t totalSurfaceQueries;
    std::size_t estimatedBytes;
    double startU;
    double endU;
    double startV;
    double endV;
    double stepU;
    double stepV;
};

inline bool checkedClothSamplingPlan(
    int rawSamplingU,
    int rawSamplingV,
    bool addBiasPoints,
    double startU,
    double endU,
    double startV,
    double endV,
    std::size_t maximumPointCount,
    std::size_t maximumWorkItems,
    ClothSamplingPlan& plan)
{
    if (rawSamplingU < 1 || rawSamplingV < 1 ||
        !std::isfinite(startU) || !std::isfinite(endU) ||
        !std::isfinite(startV) || !std::isfinite(endV) ||
        endU <= startU || endV <= startV || maximumPointCount == 0U ||
        maximumWorkItems == 0U) {
        return false;
    }

    ClothSamplingPlan candidate;
    if (!generation::checkedCount(
            rawSamplingU, maximumPointCount, candidate.samplingU) ||
        !generation::checkedCount(
            rawSamplingV, maximumPointCount, candidate.samplingV) ||
        !generation::checkedSum(
            candidate.samplingU, 2U, maximumPointCount,
            candidate.curveCountU) ||
        !generation::checkedSum(
            candidate.samplingV, 2U, maximumPointCount,
            candidate.curveCountV) ||
        !generation::checkedSum(
            candidate.curveCountU, candidate.curveCountV,
            std::min(maximumPointCount, generation::kMaxGeneratedCurveCount),
            candidate.totalCurveCount)) {
        return false;
    }

    const std::size_t pointsPerInteriorSample = addBiasPoints ? 4U : 2U;
    std::size_t interiorUPoints = 0U;
    std::size_t interiorVPoints = 0U;
    if (!generation::checkedProduct(
            candidate.samplingV, pointsPerInteriorSample,
            maximumPointCount, interiorUPoints) ||
        !generation::checkedProduct(
            candidate.samplingU, pointsPerInteriorSample,
            maximumPointCount, interiorVPoints) ||
        !generation::checkedSum(
            interiorUPoints, 2U, maximumPointCount,
            candidate.pointsPerUCurve) ||
        !generation::checkedSum(
            interiorVPoints, 2U, maximumPointCount,
            candidate.pointsPerVCurve)) {
        return false;
    }

    std::size_t uPointCount = 0U;
    std::size_t vPointCount = 0U;
    if (!generation::checkedProduct(
            candidate.curveCountU, candidate.pointsPerUCurve,
            maximumPointCount, uPointCount) ||
        !generation::checkedProduct(
            candidate.curveCountV, candidate.pointsPerVCurve,
            maximumPointCount, vPointCount) ||
        !generation::checkedSum(
            uPointCount, vPointCount, maximumPointCount,
            candidate.totalPointCount)) {
        return false;
    }

    const std::size_t queriesPerInteriorSample = addBiasPoints ? 7U : 3U;
    std::size_t uQueriesPerCurve = 0U;
    std::size_t vQueriesPerCurve = 0U;
    std::size_t uQueryCount = 0U;
    std::size_t vQueryCount = 0U;
    if (!generation::checkedProduct(
            candidate.samplingV, queriesPerInteriorSample,
            maximumWorkItems, uQueriesPerCurve) ||
        !generation::checkedSum(
            uQueriesPerCurve, 2U, maximumWorkItems,
            uQueriesPerCurve) ||
        !generation::checkedProduct(
            candidate.samplingU, queriesPerInteriorSample,
            maximumWorkItems, vQueriesPerCurve) ||
        !generation::checkedSum(
            vQueriesPerCurve, 2U, maximumWorkItems,
            vQueriesPerCurve) ||
        !generation::checkedProduct(
            candidate.curveCountU, uQueriesPerCurve,
            maximumWorkItems, uQueryCount) ||
        !generation::checkedProduct(
            candidate.curveCountV, vQueriesPerCurve,
            maximumWorkItems, vQueryCount) ||
        !generation::checkedSum(
            uQueryCount, vQueryCount, maximumWorkItems,
            candidate.totalSurfaceQueries)) {
        return false;
    }

    candidate.startU = startU;
    candidate.endU = endU;
    candidate.startV = startV;
    candidate.endV = endV;
    candidate.stepU =
        (endU - startU) / static_cast<double>(candidate.curveCountU - 1U);
    candidate.stepV =
        (endV - startV) / static_cast<double>(candidate.curveCountV - 1U);
    if (!std::isfinite(candidate.stepU) || candidate.stepU <= 0.0 ||
        !std::isfinite(candidate.stepV) || candidate.stepV <= 0.0) {
        return false;
    }

    if (!generation::curveGenerationStorageBytes(
            candidate.totalCurveCount, candidate.totalPointCount,
            std::max(candidate.pointsPerUCurve, candidate.pointsPerVCurve), 0U,
            candidate.estimatedBytes)) return false;
    plan = candidate;
    return true;
}

// Optional per-CV float3 values/defaults, attribute objects and repeated names.
// Four-copy allowance covers construction and the transactional output clone.
inline bool checkedClothNormalStorage(const ClothSamplingPlan& plan,
                                      std::size_t nameBytes, std::size_t& result)
{
    std::size_t bytes = plan.estimatedBytes, names = 0U;
    if (!generation::checkedProduct(plan.totalCurveCount, nameBytes,
            generation::kMaxCurveGenerationBytes, names) ||
        !generation::addCurveGenerationBytes(plan.totalPointCount, 48U, bytes) ||
        !generation::addCurveGenerationBytes(plan.totalCurveCount, 512U, bytes) ||
        !generation::addCurveGenerationBytes(names, 4U, bytes)) return false;
    result = bytes;
    return true;
}

inline double clothParameterAt(double start,
                               double end,
                               double step,
                               std::size_t index,
                               std::size_t count)
{
    if (index == 0U) return start;
    if (index + 1U == count) return end;
    return start + step * static_cast<double>(index);
}

} // namespace curve
} // namespace dommy
