#pragma once

#include "DGenerationLimits.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace generation {

const int kMaxArcDivisionCount = 1000000;
const long kMaxRigControllerDivisionCount = 100000;

struct ArcMeshPlan {
    ArcMeshPlan()
        : sampleCount(0U),
          vertexCount(0U),
          faceCount(0U),
          connectionCount(0U),
          startDegrees(0.0),
          stepDegrees(0.0),
          uvScale(0.0) {}

    std::size_t sampleCount;
    std::size_t vertexCount;
    std::size_t faceCount;
    std::size_t connectionCount;
    double startDegrees;
    double stepDegrees;
    double uvScale;
};

inline bool buildArcMeshPlan(int division,
                             double radius,
                             double width,
                             double startAngle,
                             double endAngle,
                             double offsetAngle,
                             ArcMeshPlan& plan)
{
    if (division < 0 || division > kMaxArcDivisionCount ||
        !std::isfinite(radius) || radius < 0.0 ||
        !std::isfinite(width) || width < 0.0 ||
        !std::isfinite(startAngle) || !std::isfinite(endAngle) ||
        !std::isfinite(offsetAngle)) {
        return false;
    }

    const double outerRadius = radius + width;
    const double minimumAngle = startAngle < endAngle ? startAngle : endAngle;
    const double maximumAngle = startAngle < endAngle ? endAngle : startAngle;
    const double angleRange = maximumAngle - minimumAngle;
    if (!std::isfinite(outerRadius) ||
        outerRadius <= std::numeric_limits<double>::epsilon() ||
        !std::isfinite(angleRange) ||
        angleRange <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    ArcMeshPlan candidate;
    if (!checkedSum(
            static_cast<std::size_t>(division), 2U,
            kMaxGeneratedPointCount, candidate.sampleCount) ||
        !checkedProduct(
            candidate.sampleCount, 2U,
            kMaxGeneratedPointCount, candidate.vertexCount)) {
        return false;
    }
    candidate.faceCount = candidate.sampleCount - 1U;
    if (candidate.faceCount > kMaxGeneratedFaceCount ||
        !checkedProduct(
            candidate.faceCount, 4U,
            kMaxWorkItemCount, candidate.connectionCount) ||
        candidate.vertexCount >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        candidate.faceCount >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    candidate.startDegrees = minimumAngle;
    candidate.stepDegrees =
        angleRange / static_cast<double>(candidate.faceCount);
    candidate.uvScale = outerRadius;
    if (!std::isfinite(candidate.stepDegrees)) return false;

    plan = candidate;
    return true;
}

struct RigControllerPlan {
    RigControllerPlan() : maximumPointCount(0U) {}

    std::size_t maximumPointCount;
};

inline bool buildRigControllerPlan(int shapeType,
                                   long division,
                                   const double* finiteValues,
                                   std::size_t finiteValueCount,
                                   RigControllerPlan& plan)
{
    if (shapeType < 0 || shapeType >= 9 ||
        division < 3 || division > kMaxRigControllerDivisionCount ||
        (!finiteValues && finiteValueCount != 0U)) {
        return false;
    }
    for (std::size_t index = 0; index < finiteValueCount; ++index) {
        if (!std::isfinite(finiteValues[index])) return false;
    }

    std::size_t scaledDivision = 0U;
    std::size_t maximumPointCount = 0U;
    if (!checkedProduct(
            static_cast<std::size_t>(division), 16U,
            kMaxGeneratedPointCount, scaledDivision) ||
        !checkedSum(
            scaledDivision, 64U,
            kMaxGeneratedPointCount, maximumPointCount)) {
        return false;
    }

    RigControllerPlan candidate;
    candidate.maximumPointCount = maximumPointCount;
    plan = candidate;
    return true;
}

struct LeafMeshPlan {
    LeafMeshPlan()
        : lengthSampleCount(0U), widthSampleCount(0U), pointsPerRow(0U),
          vertexCount(0U), faceCount(0U), connectionCount(0U),
          bendPointTransformCount(0U), workItemCount(0U) {}

    std::size_t lengthSampleCount;
    std::size_t widthSampleCount;
    std::size_t pointsPerRow;
    std::size_t vertexCount;
    std::size_t faceCount;
    std::size_t connectionCount;
    std::size_t bendPointTransformCount;
    std::size_t workItemCount;
};

inline bool buildLeafMeshPlan(int rawLengthSamples,
                              int rawWidthSamples,
                              LeafMeshPlan& plan)
{
    std::size_t lengthSamples = 0U;
    std::size_t widthSamples = 0U;
    std::size_t doubledWidth = 0U;
    if (!checkedCount(
            rawLengthSamples, kMaxGeneratedPointCount, lengthSamples) ||
        !checkedCount(
            rawWidthSamples, kMaxGeneratedPointCount, widthSamples) ||
        lengthSamples < 3U || widthSamples < 2U ||
        !checkedProduct(
            widthSamples, 2U, kMaxGeneratedPointCount, doubledWidth)) {
        return false;
    }

    LeafMeshPlan candidate;
    candidate.lengthSampleCount = lengthSamples;
    candidate.widthSampleCount = widthSamples;
    candidate.pointsPerRow = doubledWidth - 1U;
    const std::size_t faceRows = lengthSamples - 1U;
    const std::size_t faceColumns = (widthSamples - 1U) * 2U;
    if (!checkedProduct(
            lengthSamples, candidate.pointsPerRow,
            kMaxGeneratedPointCount, candidate.vertexCount) ||
        !checkedProduct(
            faceRows, faceColumns,
            kMaxGeneratedFaceCount, candidate.faceCount) ||
        !checkedProduct(
            candidate.faceCount, 4U,
            kMaxWorkItemCount, candidate.connectionCount) ||
        candidate.vertexCount >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        candidate.faceCount >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        candidate.connectionCount >
            static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return false;
    }

    candidate.bendPointTransformCount = candidate.vertexCount - candidate.pointsPerRow;
    std::size_t pointWork = 0U;
    std::size_t faceWork = 0U;
    std::size_t connectionWork = 0U;
    if (!checkedProduct(candidate.vertexCount, 3U, kMaxWorkItemCount, pointWork) ||
        !checkedProduct(candidate.faceCount, 2U, kMaxWorkItemCount, faceWork) ||
        !checkedProduct(candidate.connectionCount, 2U,
            kMaxWorkItemCount, connectionWork) ||
        !checkedSum(pointWork, faceWork, kMaxWorkItemCount, candidate.workItemCount) ||
        !checkedSum(candidate.workItemCount, connectionWork,
            kMaxWorkItemCount, candidate.workItemCount) ||
        !checkedSum(candidate.workItemCount, candidate.bendPointTransformCount,
            kMaxWorkItemCount, candidate.workItemCount) ||
        !checkedSum(candidate.workItemCount, lengthSamples - 1U,
            kMaxWorkItemCount, candidate.workItemCount)) return false;

    plan = candidate;
    return true;
}

struct EyePartPlan {
    EyePartPlan() : pointCount(0U), faceCount(0U), connectionCount(0U) {}

    std::size_t pointCount;
    std::size_t faceCount;
    std::size_t connectionCount;
};

struct EyeMeshPlan {
    EyeMeshPlan()
        : divideU(0U), divideV(0U), irisDivide(0U),
          lensTipPointCount(0U), workItemCount(0U) {}

    std::size_t divideU;
    std::size_t divideV;
    std::size_t irisDivide;
    std::size_t lensTipPointCount;
    std::size_t workItemCount;
    EyePartPlan lens;
    EyePartPlan iris;
};

inline bool buildEyePartPlan(std::size_t rings,
                             std::size_t segments,
                             std::size_t extraPoints,
                             std::size_t extraQuads,
                             bool cap,
                             EyePartPlan& part)
{
    if (rings == 0U || segments == 0U) return false;
    EyePartPlan candidate;
    std::size_t triangles = 0U;
    std::size_t quads = 0U;
    std::size_t quadConnections = 0U;
    if (!checkedProduct(rings, segments,
            kMaxGeneratedPointCount, candidate.pointCount) ||
        !checkedSum(candidate.pointCount, 1U + (cap ? 1U : 0U),
            kMaxGeneratedPointCount, candidate.pointCount) ||
        !checkedSum(candidate.pointCount, extraPoints,
            kMaxGeneratedPointCount, candidate.pointCount) ||
        !checkedProduct(segments, cap ? 2U : 1U,
            kMaxGeneratedFaceCount, triangles) ||
        !checkedProduct(rings - 1U, segments,
            kMaxGeneratedFaceCount, quads) ||
        !checkedSum(quads, extraQuads, kMaxGeneratedFaceCount, quads) ||
        !checkedSum(triangles, quads,
            kMaxGeneratedFaceCount, candidate.faceCount) ||
        !checkedProduct(triangles, 3U,
            kMaxWorkItemCount, candidate.connectionCount) ||
        !checkedProduct(quads, 4U, kMaxWorkItemCount, quadConnections) ||
        !checkedSum(candidate.connectionCount, quadConnections,
            kMaxWorkItemCount, candidate.connectionCount)) {
        return false;
    }
    part = candidate;
    return true;
}

inline bool buildEyeMeshPlan(int rawDivideU,
                             int rawDivideV,
                             int rawIrisDivide,
                             bool capPupil,
                             EyeMeshPlan& plan)
{
    std::size_t baseU = 0U;
    std::size_t baseV = 0U;
    std::size_t irisDivide = 0U;
    if (!checkedCount(rawDivideU, kMaxGeneratedPointCount, baseU) ||
        !checkedCount(rawDivideV, kMaxGeneratedPointCount, baseV) ||
        !checkedCount(
            rawIrisDivide, kMaxGeneratedPointCount, irisDivide) ||
        baseU == 0U || baseV == 0U) {
        return false;
    }

    EyeMeshPlan candidate;
    candidate.irisDivide = irisDivide;
    if (!checkedProduct(
            baseU, 4U, kMaxGeneratedPointCount, candidate.divideU) ||
        !checkedProduct(
            baseV, 4U, kMaxGeneratedPointCount, candidate.divideV)) {
        return false;
    }

    std::size_t irisRings = 0U;
    std::size_t tipSide = 0U;
    std::size_t tipQuads = 0U;
    if (!checkedSum(candidate.divideU, candidate.irisDivide,
            kMaxGeneratedPointCount, irisRings) ||
        !checkedSum(irisRings, 2U, kMaxGeneratedPointCount, irisRings) ||
        !checkedSum(baseV, 1U, kMaxGeneratedPointCount, tipSide) ||
        !checkedProduct(baseV, baseV, kMaxGeneratedFaceCount, tipQuads) ||
        (baseV > 1U && !checkedProduct(tipSide, tipSide,
            kMaxGeneratedPointCount, candidate.lensTipPointCount)) ||
        !buildEyePartPlan(candidate.divideU, candidate.divideV,
            candidate.lensTipPointCount, tipQuads, false, candidate.lens) ||
        !buildEyePartPlan(irisRings, candidate.divideV,
            0U, 0U, capPupil, candidate.iris)) {
        return false;
    }

    // Both meshes are built in one evaluation. Include their geometry and UV
    // storage/work, even when only one output plug is being requested.
    const EyePartPlan parts[] = {candidate.lens, candidate.iris};
    for (const EyePartPlan& part : parts) {
        std::size_t pointWork = 0U;
        std::size_t faceWork = 0U;
        std::size_t connectionWork = 0U;
        if (!checkedProduct(part.pointCount, 3U, kMaxWorkItemCount, pointWork) ||
            !checkedProduct(part.faceCount, 2U, kMaxWorkItemCount, faceWork) ||
            !checkedProduct(part.connectionCount, 2U,
                kMaxWorkItemCount, connectionWork) ||
            !checkedSum(candidate.workItemCount, pointWork,
                kMaxWorkItemCount, candidate.workItemCount) ||
            !checkedSum(candidate.workItemCount, faceWork,
                kMaxWorkItemCount, candidate.workItemCount) ||
            !checkedSum(candidate.workItemCount, connectionWork,
                kMaxWorkItemCount, candidate.workItemCount)) {
            return false;
        }
    }
    plan = candidate;
    return true;
}

inline bool matchesEyePartPlan(const EyePartPlan& plan,
                               std::size_t pointCount,
                               std::size_t faceCount,
                               std::size_t connectionCount)
{
    return pointCount == plan.pointCount &&
           faceCount == plan.faceCount &&
           connectionCount == plan.connectionCount;
}

} // namespace generation
} // namespace dommy
