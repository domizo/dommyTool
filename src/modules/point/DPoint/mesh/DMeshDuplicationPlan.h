#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace mesh {

struct DuplicationSourceSize {
    DuplicationSourceSize(std::size_t pointCountValue,
                          std::size_t faceCountValue,
                          std::size_t connectionCountValue,
                          std::size_t uvCountValue)
        : pointCount(pointCountValue),
          faceCount(faceCountValue),
          connectionCount(connectionCountValue),
          uvCount(uvCountValue)
    {
    }

    std::size_t pointCount;
    std::size_t faceCount;
    std::size_t connectionCount;
    std::size_t uvCount;
};

struct MeshDuplicationBudget {
    MeshDuplicationBudget()
        : meshCount(0U),
          pointCount(0U),
          faceCount(0U),
          connectionCount(0U),
          uvCount(0U),
          workItemCount(0U)
    {
    }

    std::size_t meshCount;
    std::size_t pointCount;
    std::size_t faceCount;
    std::size_t connectionCount;
    std::size_t uvCount;
    std::size_t workItemCount;
};

inline int clampedDuplicationSourceIndex(int index,
                                         std::size_t sourceCount)
{
    if (sourceCount == 0U || index < 0) return 0;
    if (static_cast<std::size_t>(index) >= sourceCount) {
        return static_cast<int>(sourceCount - 1U);
    }
    return index;
}

inline bool normalizeDuplicationSourceIndices(
    std::size_t sourceCount, std::vector<int>& indices)
{
    if (sourceCount == 0U ||
        sourceCount > static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return false;
    }

    std::vector<int> normalized(indices);
    for (std::size_t index = 0U; index < normalized.size(); ++index) {
        normalized[index] =
            clampedDuplicationSourceIndex(normalized[index], sourceCount);
    }
    indices.swap(normalized);
    return true;
}

inline bool buildMeshDuplicationBudget(
    const std::vector<DuplicationSourceSize>& sources,
    const std::vector<int>& sourceIndices,
    bool includeUVs,
    MeshDuplicationBudget& output,
    std::size_t extraBytes = 0U)
{
    if (sources.empty() ||
        sourceIndices.size() > generation::kMaxGeneratedTransformCount) {
        return false;
    }

    MeshDuplicationBudget candidate;
    MeshMergePlan geometry;

    for (std::size_t outputIndex = 0U;
         outputIndex < sourceIndices.size(); ++outputIndex) {
        const int sourceIndex = sourceIndices[outputIndex];
        if (sourceIndex < 0 ||
            static_cast<std::size_t>(sourceIndex) >= sources.size()) {
            return false;
        }

        const DuplicationSourceSize& source =
            sources[static_cast<std::size_t>(sourceIndex)];
        if (source.pointCount == 0U || source.faceCount == 0U ||
            source.connectionCount == 0U ||
            !appendMeshMergePlan(source.pointCount, source.faceCount,
                                 source.connectionCount, includeUVs ? source.uvCount : 0U,
                                 geometry)) {
            return false;
        }
    }

    // Match collection publication before any output geometry is constructed.
    // Extra retained arrays/attributes share that same staging allowance.
    if (!validMeshOperationStorage(geometry, extraBytes)) return false;
    candidate.meshCount = geometry.meshes;
    candidate.pointCount = geometry.points;
    candidate.faceCount = geometry.faces;
    candidate.connectionCount = geometry.connections;
    candidate.uvCount = geometry.uvs;

    if (!generation::checkedSum(
            candidate.pointCount, candidate.faceCount,
            generation::kMaxWorkItemCount, candidate.workItemCount) ||
        !generation::checkedSum(
            candidate.workItemCount, candidate.connectionCount,
            generation::kMaxWorkItemCount, candidate.workItemCount) ||
        !generation::checkedSum(
            candidate.workItemCount, candidate.uvCount,
            generation::kMaxWorkItemCount, candidate.workItemCount)) {
        return false;
    }

    output = candidate;
    return true;
}

} // namespace mesh
} // namespace dommy
