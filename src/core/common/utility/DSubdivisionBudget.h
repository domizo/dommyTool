#pragma once

#include "DGenerationLimits.h"

#include <algorithm>
#include <cstddef>

namespace dommy {
namespace mesh {

const int kMaxSubdivisionLevel = 8;

struct SubdivisionBudget {
    SubdivisionBudget()
        : level(0),
          estimatedFinalVertexCount(0U),
          estimatedFinalFaceCount(0U),
          estimatedFinalFaceVertexCount(0U),
          estimatedTotalWorkItems(0U)
    {
    }

    int level;
    std::size_t estimatedFinalVertexCount;
    std::size_t estimatedFinalFaceCount;
    std::size_t estimatedFinalFaceVertexCount;
    std::size_t estimatedTotalWorkItems;
};

inline bool checkedSubdivisionBudget(
    int level,
    std::size_t vertexCount,
    std::size_t faceCount,
    std::size_t faceVertexCount,
    std::size_t maximumVertexCount,
    std::size_t maximumFaceCount,
    std::size_t maximumWorkItems,
    SubdivisionBudget& budget)
{
    if (level < 0 || level > kMaxSubdivisionLevel || vertexCount == 0U ||
        faceCount == 0U || faceVertexCount < faceCount ||
        vertexCount > maximumVertexCount || faceCount > maximumFaceCount) {
        return false;
    }

    SubdivisionBudget candidate;
    candidate.level = level;
    std::size_t currentVertices = vertexCount;
    std::size_t currentFaces = faceCount;
    std::size_t currentFaceVertices = faceVertexCount;
    std::size_t initialWork = 0U;
    if (!generation::checkedSum(
            currentVertices, currentFaces, maximumWorkItems, initialWork) ||
        !generation::checkedSum(
            initialWork, currentFaceVertices, maximumWorkItems,
            candidate.estimatedTotalWorkItems)) {
        return false;
    }

    for (int currentLevel = 0; currentLevel < level; ++currentLevel) {
        std::size_t nextVertices = 0U;
        std::size_t fourCurrentFaces = 0U;
        std::size_t nextFaces = 0U;
        std::size_t nextFaceVertices = 0U;
        std::size_t levelWork = 0U;
        if (!generation::checkedSum(
                currentVertices, currentFaceVertices,
                maximumVertexCount, nextVertices) ||
            !generation::checkedSum(
                nextVertices, currentFaces,
                maximumVertexCount, nextVertices) ||
            !generation::checkedProduct(
                currentFaces, 4U, maximumFaceCount,
                fourCurrentFaces)) {
            return false;
        }
        nextFaces = std::max(currentFaceVertices, fourCurrentFaces);
        if (nextFaces > maximumFaceCount ||
            !generation::checkedProduct(
                nextFaces, 4U, maximumWorkItems,
                nextFaceVertices) ||
            !generation::checkedSum(
                nextVertices, nextFaces, maximumWorkItems, levelWork) ||
            !generation::checkedSum(
                levelWork, nextFaceVertices, maximumWorkItems, levelWork) ||
            !generation::checkedSum(
                candidate.estimatedTotalWorkItems, levelWork,
                maximumWorkItems, candidate.estimatedTotalWorkItems)) {
            return false;
        }

        currentVertices = nextVertices;
        currentFaces = nextFaces;
        currentFaceVertices = nextFaceVertices;
    }

    candidate.estimatedFinalVertexCount = currentVertices;
    candidate.estimatedFinalFaceCount = currentFaces;
    candidate.estimatedFinalFaceVertexCount = currentFaceVertices;
    budget = candidate;
    return true;
}

// Accumulate all primitives before native refinement. The byte estimate covers
// staged geometry/level buffers, not native allocator overhead, DG caches or
// arbitrary attribute payloads. A rejected addition leaves totals unchanged.
inline bool appendSubdivisionBudget(const SubdivisionBudget& input, SubdivisionBudget& total)
{
    SubdivisionBudget candidate = total;
    std::size_t bytes = 0U;
    if (!generation::checkedSum(total.estimatedFinalVertexCount, input.estimatedFinalVertexCount,
            generation::kMaxGeneratedPointCount, candidate.estimatedFinalVertexCount) ||
        !generation::checkedSum(total.estimatedFinalFaceCount, input.estimatedFinalFaceCount,
            generation::kMaxGeneratedFaceCount, candidate.estimatedFinalFaceCount) ||
        !generation::checkedSum(total.estimatedFinalFaceVertexCount, input.estimatedFinalFaceVertexCount,
            generation::kMaxWorkItemCount, candidate.estimatedFinalFaceVertexCount) ||
        !generation::checkedSum(total.estimatedTotalWorkItems, input.estimatedTotalWorkItems,
            generation::kMaxWorkItemCount, candidate.estimatedTotalWorkItems) ||
        !generation::checkedProduct(candidate.estimatedTotalWorkItems, 32U,
            256U * 1024U * 1024U, bytes)) return false;
    candidate.level = std::max(total.level, input.level);
    total = candidate;
    return true;
}

} // namespace mesh
} // namespace dommy
