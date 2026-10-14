#pragma once

#include "DDeformerMath.h"
#include "DGenerationLimits.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace generation {

const std::size_t kMaxMotionTrailBytes = 256U * 1024U * 1024U;

struct MotionTrailBudget {
    std::size_t curveCount;
    std::size_t sampleCount;
    std::size_t pointCount;
    std::size_t estimatedBytes;

    MotionTrailBudget() noexcept
        : curveCount(0U), sampleCount(0U), pointCount(0U), estimatedBytes(0U)
    {
    }
};

inline bool validMotionTrailControls(const int rawSampleCount,
                                     const double evaluationTime,
                                     const double trailLength,
                                     const double trailLengthRandom) noexcept
{
    if (!std::isfinite(evaluationTime) || !std::isfinite(trailLength) ||
        !std::isfinite(trailLengthRandom) || trailLength < 0.0 ||
        trailLengthRandom < 0.0 ||
        trailLengthRandom * 0.5 > trailLength ||
        rawSampleCount < 4 ||
        static_cast<std::size_t>(rawSampleCount) > deformer::kMaxMotionSampleCount) {
        return false;
    }
    const double halfRandomLength = trailLengthRandom * 0.5;
    return std::isfinite(trailLength - halfRandomLength) &&
           std::isfinite(trailLength + halfRandomLength);
}

inline bool buildMotionTrailBudget(const int rawCurveCount,
                                   const int rawSampleCount,
                                   const double evaluationTime,
                                   const double trailLength,
                                   const double trailLengthRandom,
                                   MotionTrailBudget& budget,
                                   const std::size_t sourcePointCount = 0U,
                                   const bool randomSelection = false) noexcept
{
    if (!validMotionTrailControls(rawSampleCount, evaluationTime, trailLength,
                                  trailLengthRandom) ||
        sourcePointCount > kMaxGeneratedPointCount) return false;

    MotionTrailBudget candidate;
    if (!checkedCount(
            rawCurveCount, kMaxGeneratedCurveCount,
            candidate.curveCount) ||
        !checkedCount(
            rawSampleCount, deformer::kMaxMotionSampleCount,
            candidate.sampleCount) ||
        !checkedProduct(
            candidate.curveCount, candidate.sampleCount,
            kMaxGeneratedPointCount, candidate.pointCount)) {
        return false;
    }

    std::size_t workCount = 0U;
    if (!checkedProduct(
            candidate.curveCount, candidate.sampleCount,
            kMaxWorkItemCount, workCount)) {
        return false;
    }

    // Conservative payload/workspace estimate, not a native Maya memory cap.
    // Include source array capacity, selection points and native curve/handle
    // overhead, retained CV/knot data, one matrix/edit-point/construction workspace,
    // and the full shuffle-index array only when random selection is nonempty.
    const std::size_t counts[] = {
        sourcePointCount, candidate.curveCount, candidate.pointCount,
        candidate.curveCount == 0U ? 0U : candidate.sampleCount,
        randomSelection && candidate.curveCount != 0U ? sourcePointCount : 0U};
    const std::size_t widths[] = {64U, 1088U, 96U, 512U, sizeof(unsigned int)};
    for (std::size_t i = 0U; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        std::size_t bytes = 0U;
        if (!checkedProduct(counts[i], widths[i], kMaxMotionTrailBytes, bytes) ||
            !checkedSum(candidate.estimatedBytes, bytes, kMaxMotionTrailBytes,
                        candidate.estimatedBytes)) return false;
    }

    budget = candidate;
    return true;
}

inline bool motionTrailStep(const double trailLength,
                            const std::size_t sampleCount,
                            double& step) noexcept
{
    if (!std::isfinite(trailLength) || trailLength < 0.0 ||
        sampleCount < 2U) {
        return false;
    }

    const double candidate =
        -trailLength / static_cast<double>(sampleCount - 1U);
    if (!std::isfinite(candidate)) return false;
    step = candidate;
    return true;
}

// Publicly queryable geometry payload copied by the motion duplicator. Native
// trim/blind data and host caches are not a measurable allocation contract.
struct MotionGeometrySize {
    std::size_t points = 0U, faces = 0U, connections = 0U;
    std::size_t uvs = 0U, normals = 0U, colors = 0U, knots = 0U;
    std::size_t assignments = 0U, sets = 0U, nameCharacters = 0U;
};

const std::size_t kMaxMotionGeometrySets = 1024U;
const std::size_t kMaxMotionDuplicationBytes = 256U * 1024U * 1024U;

struct MotionDuplicationBudget {
    std::size_t objectCount;
    std::size_t pointCount;
    std::size_t faceCount;
    std::size_t connectionCount;
    std::size_t estimatedBytes;

    MotionDuplicationBudget() noexcept
        : objectCount(0U),
          pointCount(0U),
          faceCount(0U),
          connectionCount(0U),
          estimatedBytes(0U)
    {
    }
};

inline bool buildMotionDuplicationBudget(
    const int rawObjectCount,
    const MotionGeometrySize& source,
    MotionDuplicationBudget& budget) noexcept
{
    MotionDuplicationBudget candidate;
    if (!checkedCount(
            rawObjectCount, kMaxGeneratedTransformCount,
            candidate.objectCount)) {
        return false;
    }
    if (candidate.objectCount == 0U) {
        budget = candidate;
        return true;
    }
    if (source.points == 0U || source.sets > kMaxMotionGeometrySets) return false;

    if (!checkedProduct(
            candidate.objectCount, source.points,
            kMaxGeneratedPointCount, candidate.pointCount) ||
        !checkedProduct(
            candidate.objectCount, source.faces,
            kMaxGeneratedFaceCount, candidate.faceCount) ||
        !checkedProduct(
            candidate.objectCount, source.connections,
            kMaxWorkItemCount, candidate.connectionCount)) {
        return false;
    }

    // Include retained output plus source/transformed point workspace. The
    // per-object allowance covers matrix/scalar histories, four random-offset
    // arrays, staging copies and output handles even when every input is animated.
    std::size_t copies = 0U;
    if (!checkedSum(candidate.objectCount, 1U, kMaxWorkItemCount, copies) ||
        !checkedProduct(candidate.objectCount, 1024U, kMaxMotionDuplicationBytes,
                        candidate.estimatedBytes)) return false;
    const std::size_t counts[] = {source.points, source.faces, source.connections,
        source.uvs, source.normals, source.colors, source.knots, source.assignments,
        source.sets, source.nameCharacters};
    const std::size_t widths[] = {128U, 32U, 32U, 16U, 24U, 32U, 16U, 8U, 512U, 4U};
    std::size_t workItems = 0U;
    for (std::size_t i = 0U; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        std::size_t total = 0U, bytes = 0U;
        if (!checkedProduct(counts[i], copies, kMaxWorkItemCount, total) ||
            !checkedSum(workItems, total, kMaxWorkItemCount, workItems) ||
            !checkedProduct(total, widths[i], kMaxMotionDuplicationBytes, bytes) ||
            !checkedSum(candidate.estimatedBytes, bytes, kMaxMotionDuplicationBytes,
                        candidate.estimatedBytes)) return false;
    }

    budget = candidate;
    return true;
}

// Count-only callers still receive the base geometry/workspace estimate.
inline bool buildMotionDuplicationBudget(
    int count, std::size_t points, std::size_t faces, std::size_t connections,
    MotionDuplicationBudget& budget) noexcept
{
    MotionGeometrySize source;
    source.points = points;
    source.faces = faces;
    source.connections = connections;
    return buildMotionDuplicationBudget(count, source, budget);
}

} // namespace generation
} // namespace dommy
