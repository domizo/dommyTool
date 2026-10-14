#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <vector>
#include <utility>

namespace dommy {
namespace mesh {

struct WireTopologyPlan {
    WireTopologyPlan()
        : ringCount(0U),
          profilePointCount(0U),
          vertexCount(0U),
          faceCount(0U),
          faceVertexCount(0U),
          uvCoordinateCount(0U)
    {
    }

    std::size_t ringCount;
    std::size_t profilePointCount;
    std::size_t vertexCount;
    std::size_t faceCount;
    std::size_t faceVertexCount;
    std::size_t uvCoordinateCount;
    std::vector<int> faceCounts;
    std::vector<int> faceVertexIndices;
    std::vector<float> uCoordinates;
    std::vector<float> vCoordinates;
    std::vector<int> uvCounts;
    std::vector<int> uvIndices;
};

// Count-only preflight: safe to call before reading/building the profile.
inline bool checkedWireTopologyCounts(
    std::size_t ringCount, std::size_t profilePointCount,
    std::size_t maximumPointCount, std::size_t maximumFaceCount,
    std::size_t maximumWorkItems, WireTopologyPlan& plan)
{
    if (ringCount < 2U || profilePointCount < 3U) return false;
    WireTopologyPlan candidate;
    candidate.ringCount = ringCount;
    candidate.profilePointCount = profilePointCount;
    std::size_t columns = 0U;
    if (!generation::checkedProduct(ringCount, profilePointCount,
            maximumPointCount, candidate.vertexCount) ||
        !generation::checkedProduct(ringCount - 1U, profilePointCount,
            maximumFaceCount, candidate.faceCount) ||
        !generation::checkedProduct(candidate.faceCount, 4U,
            maximumWorkItems, candidate.faceVertexCount) ||
        !generation::checkedSum(profilePointCount, 1U, maximumWorkItems, columns) ||
        !generation::checkedProduct(ringCount, columns,
            maximumWorkItems, candidate.uvCoordinateCount) ||
        candidate.vertexCount > static_cast<std::size_t>(INT_MAX) ||
        candidate.uvCoordinateCount > static_cast<std::size_t>(INT_MAX)) return false;
    plan = std::move(candidate);
    return true;
}

inline bool validWireGenerationBudget(const WireTopologyPlan& plan, std::size_t meshCount)
{
    if (meshCount == 0U || meshCount > generation::kMaxGeneratedMeshCount) return false;
    std::size_t points = 0U, faces = 0U, corners = 0U, uvs = 0U;
    if (!generation::checkedProduct(plan.vertexCount, meshCount,
            generation::kMaxGeneratedPointCount, points) ||
        !generation::checkedProduct(plan.faceCount, meshCount,
            generation::kMaxGeneratedFaceCount, faces) ||
        !generation::checkedProduct(plan.faceVertexCount, meshCount,
            generation::kMaxWorkItemCount, corners) ||
        !generation::checkedProduct(plan.uvCoordinateCount, meshCount,
            generation::kMaxWorkItemCount, uvs)) return false;
    // Conservative allowance for the plan, Maya arrays, staged DMesh copies,
    // sample frames and group objects. Not Maya's upstream/cache memory limit.
    const std::size_t maximum = 256U * 1024U * 1024U;
    std::size_t bytes = 0U, part = 0U, work = points;
    const std::size_t counts[] = {points, faces, corners, uvs, meshCount};
    const std::size_t widths[] = {128U, 32U, 32U, 32U, 128U};
    for (std::size_t index = 0U; index < 5U; ++index)
        if (!generation::checkedProduct(counts[index], widths[index], maximum, part) ||
            !generation::checkedSum(bytes, part, maximum, bytes) ||
            !generation::checkedSum(work, counts[index], generation::kMaxWorkItemCount, work))
            return false;
    return true;
}

inline bool checkedWireTopologyPlan(
    std::size_t ringCount, std::size_t profilePointCount,
    double uvTranslateU, double uvTranslateV, double uvScaleU, double uvScaleV,
    std::size_t maximumPointCount, std::size_t maximumFaceCount,
    std::size_t maximumWorkItems, WireTopologyPlan& plan)
{
    WireTopologyPlan candidate;
    if (!checkedWireTopologyCounts(ringCount, profilePointCount, maximumPointCount,
            maximumFaceCount, maximumWorkItems, candidate) ||
        !validWireGenerationBudget(candidate, 1U) ||
        !std::isfinite(uvTranslateU) || !std::isfinite(uvTranslateV) ||
        !std::isfinite(uvScaleU) || !std::isfinite(uvScaleV)) return false;
    float endpoint = 0.0f;
    if (!numeric::checkedFloatCast(uvTranslateU, endpoint) ||
        !numeric::checkedFloatCast(uvTranslateV, endpoint) ||
        !numeric::checkedFloatCast(uvTranslateU + uvScaleU, endpoint) ||
        !numeric::checkedFloatCast(uvTranslateV + uvScaleV, endpoint)) return false;
    const std::size_t uvColumnCount = profilePointCount + 1U;

    candidate.faceCounts.assign(candidate.faceCount, 4);
    candidate.uvCounts.assign(candidate.faceCount, 4);
    candidate.faceVertexIndices.reserve(candidate.faceVertexCount);
    candidate.uvIndices.reserve(candidate.faceVertexCount);
    candidate.uCoordinates.reserve(candidate.uvCoordinateCount);
    candidate.vCoordinates.reserve(candidate.uvCoordinateCount);

    for (std::size_t ring = 0U; ring + 1U < ringCount; ++ring) {
        const std::size_t currentRing = ring * profilePointCount;
        const std::size_t nextRing = (ring + 1U) * profilePointCount;
        const std::size_t currentUvRing = ring * uvColumnCount;
        const std::size_t nextUvRing = (ring + 1U) * uvColumnCount;
        for (std::size_t profile = 0U;
             profile < profilePointCount;
             ++profile) {
            const std::size_t nextProfile =
                (profile + 1U) % profilePointCount;
            candidate.faceVertexIndices.push_back(
                static_cast<int>(currentRing + profile));
            candidate.faceVertexIndices.push_back(
                static_cast<int>(currentRing + nextProfile));
            candidate.faceVertexIndices.push_back(
                static_cast<int>(nextRing + nextProfile));
            candidate.faceVertexIndices.push_back(
                static_cast<int>(nextRing + profile));

            candidate.uvIndices.push_back(
                static_cast<int>(currentUvRing + profile));
            candidate.uvIndices.push_back(
                static_cast<int>(currentUvRing + profile + 1U));
            candidate.uvIndices.push_back(
                static_cast<int>(nextUvRing + profile + 1U));
            candidate.uvIndices.push_back(
                static_cast<int>(nextUvRing + profile));
        }
    }

    for (std::size_t ring = 0U; ring < ringCount; ++ring) {
        const double v = static_cast<double>(ring) /
            static_cast<double>(ringCount - 1U);
        for (std::size_t profile = 0U;
             profile <= profilePointCount;
             ++profile) {
            const double u = static_cast<double>(profile) /
                static_cast<double>(profilePointCount);
            float convertedU = 0.0f;
            float convertedV = 0.0f;
            if (!numeric::checkedFloatCast(
                    u * uvScaleU + uvTranslateU, convertedU) ||
                !numeric::checkedFloatCast(
                    v * uvScaleV + uvTranslateV, convertedV)) {
                return false;
            }
            candidate.uCoordinates.push_back(convertedU);
            candidate.vCoordinates.push_back(convertedV);
        }
    }

    if (candidate.faceVertexIndices.size() != candidate.faceVertexCount ||
        candidate.uvIndices.size() != candidate.faceVertexCount ||
        candidate.uCoordinates.size() != candidate.uvCoordinateCount ||
        candidate.vCoordinates.size() != candidate.uvCoordinateCount) {
        return false;
    }

    plan = std::move(candidate);
    return true;
}

inline bool hasValidWireTopology(const WireTopologyPlan& plan)
{
    if (plan.ringCount < 2U || plan.profilePointCount < 3U ||
        plan.faceCounts.size() != plan.faceCount ||
        plan.uvCounts.size() != plan.faceCount ||
        plan.faceVertexIndices.size() != plan.faceVertexCount ||
        plan.uvIndices.size() != plan.faceVertexCount ||
        plan.uCoordinates.size() != plan.uvCoordinateCount ||
        plan.vCoordinates.size() != plan.uvCoordinateCount) {
        return false;
    }
    for (std::size_t index = 0U;
         index < plan.faceVertexIndices.size(); ++index) {
        if (plan.faceVertexIndices[index] < 0 ||
            static_cast<std::size_t>(plan.faceVertexIndices[index]) >=
                plan.vertexCount ||
            plan.uvIndices[index] < 0 ||
            static_cast<std::size_t>(plan.uvIndices[index]) >=
                plan.uvCoordinateCount) {
            return false;
        }
    }
    return true;
}

} // namespace mesh
} // namespace dommy
