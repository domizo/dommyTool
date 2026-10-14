#pragma once

#include "common/utility/DCurveGenerationStorage.h"

#include <cmath>
#include <cstddef>
#include <vector>
#include <algorithm>

namespace dommy {
namespace curve {

struct CurveFromCurvePlan {
    CurveFromCurvePlan()
        : samplingType(0), requestedSampling(0U), curveDivide(0U),
          degreeType(0), curveLengthScale(0.0), outputCurveCount(0U),
          outputPointCount(0U), estimatedBytes(0U) {}

    short samplingType;
    std::size_t requestedSampling;
    std::size_t curveDivide;
    short degreeType;
    double curveLengthScale;
    std::vector<std::size_t> samplingPerSourceCurve;
    std::size_t outputCurveCount;
    std::size_t outputPointCount;
    std::size_t estimatedBytes;
};

inline bool makeCurveFromCurvePlan(
    short samplingType,
    int rawSampling,
    int rawCurveDivide,
    short degreeType,
    double curveLengthScale,
    std::size_t profilePointCount,
    const std::vector<std::size_t>& sourceCvCounts,
    CurveFromCurvePlan& plan)
{
    if ((samplingType != 0 && samplingType != 1) || rawSampling < 2 ||
        rawCurveDivide < 0 || (degreeType != 0 && degreeType != 1) ||
        !std::isfinite(curveLengthScale) || curveLengthScale <= 0.0 ||
        curveLengthScale > 1.0 || profilePointCount == 0U ||
        sourceCvCounts.empty() ||
        sourceCvCounts.size() > generation::kMaxGeneratedCurveCount) {
        return false;
    }

    const std::size_t requestedSampling =
        static_cast<std::size_t>(rawSampling);
    const std::size_t curveDivide =
        static_cast<std::size_t>(rawCurveDivide);
    std::vector<std::size_t> samplingCounts;
    samplingCounts.reserve(sourceCvCounts.size());
    std::size_t samplesAcrossSources = 0U;
    std::size_t sourcePoints = 0U, largestSampling = 0U;

    for (std::size_t sourceIndex = 0U;
         sourceIndex < sourceCvCounts.size(); ++sourceIndex) {
        const std::size_t cvCount = sourceCvCounts[sourceIndex];
        if (cvCount < 2U || !generation::checkedSum(sourcePoints, cvCount,
                generation::kMaxGeneratedPointCount, sourcePoints)) return false;

        std::size_t totalSampling = requestedSampling;
        if (samplingType == 1) {
            std::size_t insertedCount = 0U;
            if (!generation::checkedProduct(
                    cvCount - 1U, curveDivide,
                    generation::kMaxGeneratedPointCount,
                    insertedCount) ||
                !generation::checkedSum(
                    cvCount, insertedCount,
                    generation::kMaxGeneratedPointCount,
                    totalSampling)) {
                return false;
            }
        }
        if (totalSampling < 2U) return false;

        if (!generation::checkedSum(
                samplesAcrossSources, totalSampling,
                generation::kMaxGeneratedPointCount,
                samplesAcrossSources)) {
            return false;
        }
        samplingCounts.push_back(totalSampling);
        largestSampling = std::max(largestSampling, totalSampling);
    }

    std::size_t outputCurveCount = 0U;
    std::size_t outputPointCount = 0U;
    if (!generation::checkedProduct(
            sourceCvCounts.size(), profilePointCount,
            generation::kMaxGeneratedCurveCount, outputCurveCount) ||
        !generation::checkedProduct(
            samplesAcrossSources, profilePointCount,
            generation::kMaxGeneratedPointCount, outputPointCount) ||
        outputPointCount > generation::kMaxWorkItemCount) {
        return false;
    }

    CurveFromCurvePlan candidate;
    // The shared single-curve allowance includes matrices, ramps, transformed
    // points and CV/knots. All converted source objects stay alive together;
    // account for their payload, closure padding and native conversion scratch.
    if (!generation::curveGenerationStorageBytes(outputCurveCount, outputPointCount,
            largestSampling, 0U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(sourcePoints, 256U, candidate.estimatedBytes) ||
        !generation::addCurveGenerationBytes(sourceCvCounts.size(), 2112U, candidate.estimatedBytes))
        return false;
    candidate.samplingType = samplingType;
    candidate.requestedSampling = requestedSampling;
    candidate.curveDivide = curveDivide;
    candidate.degreeType = degreeType;
    candidate.curveLengthScale = curveLengthScale;
    candidate.samplingPerSourceCurve.swap(samplingCounts);
    candidate.outputCurveCount = outputCurveCount;
    candidate.outputPointCount = outputPointCount;
    plan = candidate;
    return true;
}

inline short curveFromCurveDegree(short degreeType,
                                  std::size_t samplingCount)
{
    return degreeType == 1 && samplingCount >= 4U ? 3 : 1;
}

} // namespace curve
} // namespace dommy
