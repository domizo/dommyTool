#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

const std::size_t kMaxEyelashGenerationBytes = 256U * 1024U * 1024U;

// The legacy bend mapping multiplies in float, then subtracts in double.
// Check the narrowing without replacing that intermediate rounding or clamping
// valid ramp values outside [0, 1]. Leave result unchanged on failure.
inline bool remapEyelashBendWeight(float value, double& result) noexcept
{
    float doubled = 0.0F;
    if (!numeric::checkedFloatCast(static_cast<double>(value) * 2.0, doubled)) {
        return false;
    }
    result = static_cast<double>(doubled) - 1.0;
    return true;
}

// Preserve the historical single reflection, including its arithmetic order.
// Do not silently introduce periodic wrapping or clamp still-invalid samples.
inline bool reflectEyelashParameterOnce(double parameter, double start, double end,
                                        double& result) noexcept
{
    if (!std::isfinite(parameter) || !std::isfinite(start) ||
        !std::isfinite(end) || end <= start) return false;
    double candidate = parameter;
    if (candidate < start) {
        candidate = start + (start - candidate);
    } else if (candidate > end) {
        candidate = end - (candidate - end);
    }
    if (!std::isfinite(candidate) || candidate < start || candidate > end) return false;
    result = candidate;
    return true;
}

struct EyelashGenerationPlan {
    std::size_t curveCount;
    std::size_t segmentCount;
    std::size_t pointsPerCurve;
    std::size_t totalPointCount;
    std::size_t bendWorkItems;
    std::size_t estimatedBytes;

    EyelashGenerationPlan()
        : curveCount(0U),
          segmentCount(0U),
          pointsPerCurve(0U),
          totalPointCount(0U),
          bendWorkItems(0U),
          estimatedBytes(0U)
    {
    }
};

inline bool buildEyelashGenerationPlan(
    const int rawCurveCount,
    const int rawSegmentCount,
    const double* finiteInputs,
    const std::size_t finiteInputCount,
    EyelashGenerationPlan& result) noexcept
{
    if (finiteInputCount != 0U && !finiteInputs) return false;

    EyelashGenerationPlan candidate;
    if (!checkedCount(
            rawCurveCount, kMaxGeneratedCurveCount, candidate.curveCount) ||
        !checkedCount(
            rawSegmentCount, kMaxGeneratedPointCount,
            candidate.segmentCount) ||
        candidate.curveCount < 2U || candidate.segmentCount < 2U ||
        !checkedSum(
            candidate.segmentCount, 2U, kMaxGeneratedPointCount,
            candidate.pointsPerCurve) ||
        !checkedProduct(
            candidate.curveCount, candidate.pointsPerCurve,
            kMaxGeneratedPointCount, candidate.totalPointCount)) {
        return false;
    }

    for (std::size_t inputIndex = 0U; inputIndex < finiteInputCount;
         ++inputIndex) {
        if (!std::isfinite(finiteInputs[inputIndex])) return false;
    }

    // Each bend visits P*(P-1)/2 points. Both root and tip passes run for every
    // curve, including zero angles, so admit N*P*(P-1) operations in advance.
    // Keep the historical evaluation order instead of adding a zero-angle
    // shortcut with different rounding or validation behavior.
    std::size_t workPerCurve = 0U;
    if (!checkedProduct(candidate.pointsPerCurve, candidate.pointsPerCurve - 1U,
                        kMaxWorkItemCount, workPerCurve) ||
        !checkedProduct(candidate.curveCount, workPerCurve,
                        kMaxWorkItemCount, candidate.bendWorkItems)) {
        return false;
    }

    // Shared conservative admission for Maya curves and custom DCurve output.
    // Per curve: matrix capacity, root/vector/scalar arrays, random-generation
    // scratch, output handles and curve-object overhead (including both staged
    // and committed DCurve objects). Per retained point: CV/knot payload and
    // capacity, including both DCurve batch and commit copies. Per sample:
    // base CV/bend arrays plus one curve's deformation, copy and native-
    // construction workspace. This is not
    // a bound on Maya's allocator, upstream curve data or DG caches.
    const std::size_t counts[] = {
        candidate.curveCount, candidate.totalPointCount, candidate.pointsPerCurve};
    const std::size_t widths[] = {2048U, 96U, 512U};
    for (std::size_t i = 0U; i < 3U; ++i) {
        std::size_t bytes = 0U;
        if (!checkedProduct(counts[i], widths[i], kMaxEyelashGenerationBytes, bytes) ||
            !checkedSum(candidate.estimatedBytes, bytes, kMaxEyelashGenerationBytes,
                        candidate.estimatedBytes)) return false;
    }

    result = candidate;
    return true;
}

} // namespace generation
} // namespace dommy
