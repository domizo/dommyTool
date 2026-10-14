#pragma once

#include "common/utility/DGenerationLimits.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace curve {

struct DividePlan {
    DividePlan()
        : divideType(0), count(0U), sampling(0U), spaceLength(0.0),
          stepLength(0.0), offset(0.0), outputCurveCount(0U),
          outputPointCount(0U) {}

    short divideType;
    std::size_t count;
    std::size_t sampling;
    double spaceLength;
    double stepLength;
    double offset;
    std::size_t outputCurveCount;
    std::size_t outputPointCount;
};

inline bool makeDividePlan(short divideType,
                           int rawCount,
                           int rawSampling,
                           double spaceLength,
                           double stepLength,
                           double offset,
                           std::size_t inputCurveCount,
                           DividePlan& plan)
{
    if ((divideType != 0 && divideType != 1) || rawCount < 1 ||
        rawSampling < 2 || !std::isfinite(spaceLength) ||
        spaceLength < 0.0 || !std::isfinite(stepLength) ||
        !std::isfinite(offset) || (divideType == 1 && stepLength <= 0.0)) {
        return false;
    }

    const std::size_t count = static_cast<std::size_t>(rawCount);
    const std::size_t sampling = static_cast<std::size_t>(rawSampling);
    std::size_t outputCurveCount = 0U;
    std::size_t outputPointCount = 0U;
    if (!generation::checkedProduct(
            inputCurveCount, count,
            generation::kMaxGeneratedCurveCount, outputCurveCount) ||
        !generation::checkedProduct(
            outputCurveCount, sampling,
            generation::kMaxGeneratedPointCount, outputPointCount) ||
        outputPointCount > generation::kMaxWorkItemCount) {
        return false;
    }

    DividePlan candidate;
    candidate.divideType = divideType;
    candidate.count = count;
    candidate.sampling = sampling;
    candidate.spaceLength = spaceLength;
    candidate.stepLength = stepLength;
    candidate.offset = offset;
    candidate.outputCurveCount = outputCurveCount;
    candidate.outputPointCount = outputPointCount;
    plan = candidate;
    return true;
}

inline bool divideSegmentLengths(const DividePlan& plan,
                                 double curveLength,
                                 bool edgeSpace,
                                 double& segmentLength,
                                 double& gapLength,
                                 double& sampleStep)
{
    if (!std::isfinite(curveLength) || curveLength <= 0.0 ||
        plan.count == 0U || plan.sampling < 2U) {
        return false;
    }

    if (plan.divideType == 0) {
        const double count = static_cast<double>(plan.count);
        const double spaceCount = static_cast<double>(
            plan.count - 1U + (edgeSpace ? 2U : 0U));
        const double dividedLength = curveLength / count;
        gapLength = dividedLength * plan.spaceLength;
        segmentLength =
            (curveLength - gapLength * spaceCount) / count;
    } else if (plan.divideType == 1) {
        segmentLength = curveLength * plan.stepLength;
        gapLength = curveLength * plan.spaceLength;
    } else {
        return false;
    }

    sampleStep =
        segmentLength / static_cast<double>(plan.sampling - 1U);
    return std::isfinite(segmentLength) && segmentLength > 0.0 &&
           std::isfinite(gapLength) && gapLength >= 0.0 &&
           std::isfinite(sampleStep) && sampleStep > 0.0;
}

inline bool divideSampleLength(const DividePlan& plan,
                               std::size_t segmentIndex,
                               std::size_t sampleIndex,
                               bool edgeSpace,
                               double curveLength,
                               double segmentLength,
                               double gapLength,
                               double sampleStep,
                               double& length)
{
    if (segmentIndex >= plan.count || sampleIndex >= plan.sampling ||
        !std::isfinite(curveLength) || curveLength <= 0.0) {
        return false;
    }

    const double candidate = (edgeSpace ? gapLength : 0.0) +
        sampleStep * static_cast<double>(sampleIndex) +
        static_cast<double>(segmentIndex) * (segmentLength + gapLength);
    const double tolerance =
        1.0e-9 * (1.0 + std::fabs(curveLength));
    if (!std::isfinite(candidate) || candidate < -tolerance ||
        candidate > curveLength + tolerance) {
        return false;
    }

    length = candidate < 0.0
        ? 0.0
        : (candidate > curveLength ? curveLength : candidate);
    return true;
}

} // namespace curve
} // namespace dommy
