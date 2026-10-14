#pragma once

#include "DGenerationLimits.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

enum FillSpaceSamplingMode {
    kFillSpaceByCount = 0,
    kFillSpaceByDistance = 1
};

struct FillSpacePlan {
    int samplingMode;
    std::size_t fixedSampleCount;
    double sampleSpacing;
    bool fillByIdStep;
    std::size_t neighborStepCount;
    bool fillByDistance;
    double distanceLimit;
    std::size_t pairCount;
    std::size_t outputPointCount;
    std::size_t minimumOutputPointCount;
    std::size_t workItemCount;

    FillSpacePlan()
        : samplingMode(kFillSpaceByCount),
          fixedSampleCount(0U),
          sampleSpacing(0.0),
          fillByIdStep(false),
          neighborStepCount(0U),
          fillByDistance(false),
          distanceLimit(0.0),
          pairCount(0U),
          outputPointCount(0U),
          minimumOutputPointCount(0U),
          workItemCount(0U)
    {
    }
};

inline bool checkedTriangularCount(std::size_t value,
                                   std::size_t& result)
{
    if (value < 2U) {
        result = 0U;
        return true;
    }

    std::size_t first = value;
    std::size_t second = value - 1U;
    if ((first & 1U) == 0U) {
        first /= 2U;
    } else {
        second /= 2U;
    }
    return memory::checkedMultiply(first, second, result);
}

inline bool checkedNeighborPairCount(std::size_t pointCount,
                                     std::size_t neighborStepCount,
                                     std::size_t& result)
{
    if (pointCount < 2U || neighborStepCount == 0U) {
        result = 0U;
        return true;
    }

    const std::size_t steps =
        std::min(neighborStepCount, pointCount - 1U);
    std::size_t scaledCount = 0U;
    std::size_t triangular = 0U;
    if (!memory::checkedMultiply(steps, pointCount, scaledCount) ||
        !checkedTriangularCount(steps + 1U, triangular) ||
        triangular > scaledCount) {
        return false;
    }
    result = scaledCount - triangular;
    return true;
}

inline bool buildFillSpacePlan(std::size_t inputPointCount,
                               int samplingMode,
                               int rawSamplingCount,
                               double sampleSpacing,
                               bool fillByIdStep,
                               int rawNeighborStepCount,
                               bool fillByDistance,
                               double distanceLimit,
                               FillSpacePlan& result)
{
    FillSpacePlan candidate;
    if (inputPointCount > kMaxGeneratedPointCount ||
        (samplingMode != kFillSpaceByCount &&
         samplingMode != kFillSpaceByDistance) ||
        !checkedCount(
            rawSamplingCount, kMaxGeneratedPointCount,
            candidate.fixedSampleCount) ||
        candidate.fixedSampleCount == 0U ||
        !std::isfinite(sampleSpacing) || sampleSpacing < 0.0 ||
        !checkedCount(
            rawNeighborStepCount, kMaxWorkItemCount,
            candidate.neighborStepCount) ||
        !std::isfinite(distanceLimit) || distanceLimit < 0.0) {
        return false;
    }
    if (samplingMode == kFillSpaceByDistance &&
        sampleSpacing <= 0.0) {
        return false;
    }

    candidate.samplingMode = samplingMode;
    candidate.sampleSpacing = sampleSpacing;
    candidate.fillByIdStep = fillByIdStep;
    candidate.fillByDistance = fillByDistance;
    candidate.distanceLimit = distanceLimit;
    candidate.outputPointCount = inputPointCount;
    candidate.minimumOutputPointCount = inputPointCount;
    candidate.workItemCount = inputPointCount;

    std::size_t idPairCount = 0U;
    if (fillByIdStep &&
        !checkedNeighborPairCount(
            inputPointCount, candidate.neighborStepCount, idPairCount)) {
        return false;
    }

    std::size_t distancePairCount = 0U;
    if (fillByDistance &&
        !checkedTriangularCount(inputPointCount, distancePairCount)) {
        return false;
    }
    if (!checkedSum(
            idPairCount, distancePairCount,
            kMaxWorkItemCount, candidate.pairCount)) {
        return false;
    }

    std::size_t plannedWork = 0U;
    if (!checkedSum(
            candidate.workItemCount, candidate.pairCount,
            kMaxWorkItemCount, plannedWork)) {
        return false;
    }

    // ID-step pairs are always sampled. Reject impossible fixed-count output
    // before entering Maya-dependent loops. Distance-filtered output remains
    // budgeted incrementally because many candidate pairs may be discarded.
    if (samplingMode == kFillSpaceByCount && idPairCount != 0U) {
        std::size_t idSampleCount = 0U;
        std::size_t minimumOutputCount = 0U;
        if (!checkedProduct(
                idPairCount, candidate.fixedSampleCount,
                kMaxGeneratedPointCount, idSampleCount) ||
            !checkedSum(
                candidate.outputPointCount, idSampleCount,
                kMaxGeneratedPointCount, minimumOutputCount)) {
            return false;
        }
        candidate.minimumOutputPointCount = minimumOutputCount;
    }

    result = candidate;
    return true;
}

inline bool reserveFillSpacePairWork(FillSpacePlan& plan)
{
    return checkedSum(
        plan.workItemCount, 1U, kMaxWorkItemCount, plan.workItemCount);
}

inline bool reserveFillSpaceSamples(FillSpacePlan& plan,
                                    double pairDistance,
                                    std::size_t& sampleCount)
{
    if (!std::isfinite(pairDistance) || pairDistance < 0.0) return false;

    std::size_t count = plan.fixedSampleCount;
    if (plan.samplingMode == kFillSpaceByDistance) {
        const double ratio = pairDistance / plan.sampleSpacing;
        if (!std::isfinite(ratio)) return false;
        if (ratio <= 2.0) {
            count = 0U;
        } else {
            const double rawCount = std::floor(ratio) - 2.0;
            if (!std::isfinite(rawCount) || rawCount < 0.0 ||
                rawCount >
                    static_cast<double>(kMaxGeneratedPointCount)) {
                return false;
            }
            count = static_cast<std::size_t>(rawCount);
        }
    }

    std::size_t nextOutputCount = 0U;
    std::size_t nextWorkItemCount = 0U;
    if (!checkedSum(
            plan.outputPointCount, count,
            kMaxGeneratedPointCount, nextOutputCount) ||
        !checkedSum(
            plan.workItemCount, count,
            kMaxWorkItemCount, nextWorkItemCount)) {
        return false;
    }

    plan.outputPointCount = nextOutputCount;
    plan.workItemCount = nextWorkItemCount;
    sampleCount = count;
    return true;
}

} // namespace generation
} // namespace dommy
