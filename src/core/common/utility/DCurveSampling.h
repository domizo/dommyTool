#pragma once

#include "DAttributeBlendMath.h"
#include "DDirectionMath.h"
#include "DGenerationLimits.h"
#include "DCurveGenerationStorage.h"

namespace dommy {
namespace curve {

struct StitchSamplingPlan {
    std::size_t samples = 0U;
    std::size_t curves = 0U;
    std::size_t points = 0U;
};

struct CurveExtensionPlan {
    std::size_t curves = 0U;
    std::size_t points = 0U;
    std::size_t addedCurves = 0U;
};

inline bool checkedCurveExtensionPlan(std::size_t inputCurves, std::size_t inputPoints,
                                      unsigned int ends, bool separate, bool deleteOriginal,
                                      CurveExtensionPlan& result)
{
    if (ends > 2U || inputCurves > generation::kMaxGeneratedCurveCount ||
        inputPoints > generation::kMaxGeneratedPointCount) return false;
    CurveExtensionPlan candidate;
    std::size_t added = 0U, addedPoints = 0U;
    if (!generation::checkedProduct(inputCurves, ends,
            generation::kMaxGeneratedPointCount, added)) return false;
    const bool keepOriginal = !separate || !deleteOriginal;
    if (!generation::checkedProduct(added, separate ? 2U : 1U,
            generation::kMaxGeneratedPointCount, addedPoints) ||
        !generation::checkedSum(keepOriginal ? inputPoints : 0U, addedPoints,
            generation::kMaxGeneratedPointCount, candidate.points) ||
        !generation::checkedSum(keepOriginal ? inputCurves : 0U, separate ? added : 0U,
            generation::kMaxGeneratedCurveCount, candidate.curves)) return false;
    candidate.addedCurves = separate ? added : 0U;
    result = candidate;
    return true;
}

inline bool checkedStitchSamplingPlan(std::size_t inputCurves, int rawSamples,
                                     StitchSamplingPlan& result)
{
    StitchSamplingPlan candidate;
    if (inputCurves > generation::kMaxGeneratedCurveCount ||
        !generation::checkedCount(rawSamples, generation::kMaxCurveSampleCount, candidate.samples))
        return false;
    if (inputCurves >= 2U && candidate.samples >= 2U) {
        std::size_t work = 0U;
        if (!generation::checkedProduct(inputCurves - 1U, candidate.samples,
                generation::kMaxGeneratedCurveCount, candidate.curves) ||
            !generation::checkedProduct(candidate.curves, 2U,
                generation::kMaxGeneratedPointCount, candidate.points) ||
            !generation::checkedProduct(inputCurves, candidate.samples,
                generation::kMaxWorkItemCount, work)) return false;
    }
    result = candidate;
    return true;
}

// Preserve DCOffset's historical n + (n-1)*subdivision span rule, but plan
// before Maya rebuild. Reserve one extra CV for re-closing clipped curves.
inline bool appendOffsetRebuildPlan(std::size_t sourcePoints, int degree, int subdivision,
                                    std::size_t& totalPoints, std::size_t& spans)
{
    std::size_t subdivisions = 0U, addedSpans = 0U, candidateSpans = 0U;
    std::size_t candidatePoints = 0U, candidateTotal = 0U;
    if (sourcePoints < 2U || degree < 1 || sourcePoints <= static_cast<std::size_t>(degree) ||
        sourcePoints > generation::kMaxGeneratedPointCount ||
        !generation::checkedCount(subdivision, generation::kMaxCurveSampleCount, subdivisions) ||
        !generation::checkedProduct(sourcePoints - 1U, subdivisions,
            generation::kMaxCurveSampleCount, addedSpans) ||
        !generation::checkedSum(sourcePoints, addedSpans,
            generation::kMaxCurveSampleCount, candidateSpans) ||
        !generation::checkedSum(candidateSpans, static_cast<std::size_t>(degree),
            generation::kMaxGeneratedPointCount, candidatePoints) ||
        !generation::checkedSum(candidatePoints, 1U,
            generation::kMaxGeneratedPointCount, candidatePoints) ||
        !generation::checkedSum(totalPoints, candidatePoints,
            generation::kMaxGeneratedPointCount, candidateTotal)) return false;
    totalPoints = candidateTotal;
    spans = candidateSpans;
    return true;
}

struct SurfaceSamplingPlan {
    std::size_t curvesU = 0U;
    std::size_t curvesV = 0U;
    std::size_t samples = 0U;
    std::size_t curveCount = 0U;
    std::size_t pointCount = 0U;
    std::size_t estimatedBytes = 0U;
};

inline bool checkedSurfaceSamplingPlan(int curvesU, int curvesV, int samples,
                                       SurfaceSamplingPlan& result)
{
    SurfaceSamplingPlan candidate;
    if (!generation::checkedCount(curvesU, generation::kMaxGeneratedCurveCount, candidate.curvesU) ||
        !generation::checkedCount(curvesV, generation::kMaxGeneratedCurveCount, candidate.curvesV) ||
        !generation::checkedCount(samples, generation::kMaxCurveSampleCount, candidate.samples) ||
        candidate.samples < 2U ||
        !generation::checkedSum(candidate.curvesU, candidate.curvesV,
            generation::kMaxGeneratedCurveCount, candidate.curveCount) ||
        !generation::checkedProduct(candidate.curveCount, candidate.samples,
            generation::kMaxGeneratedPointCount, candidate.pointCount) ||
        !generation::curveGenerationStorageBytes(candidate.curveCount, candidate.pointCount,
            candidate.curveCount == 0U ? 0U : candidate.samples, 0U,
            candidate.estimatedBytes)) return false;
    result = candidate;
    return true;
}

// Preserve inclusive sampling and signed scale/offset. A single curve is at
// the start of its fixed axis. Keep UV parameters in double, not point storage.
inline bool checkedSurfaceParameter(double start, double end, double scale,
                                     double offset, std::size_t index,
                                     std::size_t count, bool clamp, double& result)
{
    if (!std::isfinite(start) || !std::isfinite(end) || end < start ||
        !std::isfinite(scale) || !std::isfinite(offset) || count == 0U || index >= count) return false;
    const double fraction = count == 1U ? 0.0
        : static_cast<double>(index) / static_cast<double>(count - 1U);
    double parameter = 0.0;
    if (!attribute::checkedLinearBlend(start, end, scale * fraction, parameter) ||
        !numeric::checkedNumericCast(static_cast<long double>(parameter) + offset, parameter)) return false;
    result = clamp ? std::max(start, std::min(end, parameter)) : parameter;
    return true;
}

// Retain the historical round(distance / spacing) + 1 subdivision rule.
// Returning one means no interior samples; all conversions precede allocation.
inline bool checkedResampleDivisionCount(double distance, double spacing,
                                         std::size_t& result)
{
    if (!std::isfinite(distance) || distance < 0.0 ||
        !std::isfinite(spacing) || spacing <= 0.0) return false;
    std::size_t candidate = 1U;
    if (distance > spacing) {
        const double rawCount = std::round(distance / spacing) + 1.0;
        if (!std::isfinite(rawCount) || rawCount < 2.0 ||
            rawCount > static_cast<double>(generation::kMaxCurveSampleCount)) return false;
        candidate = static_cast<std::size_t>(rawCount);
    }
    result = candidate;
    return true;
}

// Stage both positions and source-attribute indices. Widen the distance before
// subtraction and blend components through the shared checked interpolation.
template <typename Point>
bool resampleCurvePolyline(const std::vector<Point>& source, double spacing,
                           std::size_t maximumPoints, std::vector<Point>& output,
                           std::vector<std::size_t>& sourceIndices)
{
    if (source.size() < 2U || source.size() > generation::kMaxGeneratedPointCount ||
        !std::isfinite(spacing) || spacing <= 0.0) return false;
    maximumPoints = std::min(maximumPoints, generation::kMaxGeneratedPointCount);
    if (source.size() > maximumPoints) return false;
    std::vector<std::size_t> divisions(source.size() - 1U);
    std::size_t totalPoints = 1U;
    for (std::size_t i = 0U; i < divisions.size(); ++i) {
        direction::Delta delta;
        if (!direction::difference(source[i], source[i + 1U], delta) ||
            !checkedResampleDivisionCount(delta.length, spacing, divisions[i]) ||
            !generation::checkedSum(totalPoints, divisions[i], maximumPoints, totalPoints)) return false;
    }
    std::vector<Point> candidate;
    std::vector<std::size_t> indices;
    candidate.reserve(totalPoints);
    indices.reserve(totalPoints);
    for (std::size_t i = 0U; i < divisions.size(); ++i) {
        candidate.push_back(source[i]);
        indices.push_back(i);
        for (std::size_t j = 1U; j < divisions[i]; ++j) {
            Point point;
            const double weight = static_cast<double>(j) / static_cast<double>(divisions[i]);
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!attribute::checkedLinearBlend(source[i][axis], source[i + 1U][axis],
                                                   weight, point[axis])) return false;
            }
            candidate.push_back(point);
            indices.push_back(i);
        }
    }
    candidate.push_back(source.back());
    indices.push_back(source.size() - 1U);
    output.swap(candidate);
    sourceIndices.swap(indices);
    return true;
}

} // namespace curve
} // namespace dommy
