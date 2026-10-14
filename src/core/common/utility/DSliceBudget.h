#pragma once

#include "common/utility/DMeshMergePlan.h"

namespace dommy {
namespace mesh {

struct SliceBudget {
    SliceBudget() : workItems(0U) {}
    MeshMergePlan storage;
    std::size_t workItems;
};

// Minimum traversal admission and staging estimate, not the contour algorithm's
// full work cost. DMSlicer additionally shares generation::WorkBudget across all
// meshes/cuts, including welding, graph traversal and curve construction.
// Every slice scans the points and triangles even when the plane misses.
inline bool appendSliceBudget(std::size_t points, std::size_t faces, std::size_t triangles,
                              std::size_t cuts, SliceBudget& budget) {
    SliceBudget next = budget;
    std::size_t perCut = 0U, work = 0U, connections = 0U;
    if (cuts == 0U || cuts > generation::kMaxGeneratedCurveCount ||
        triangles > generation::kMaxGeneratedFaceCount ||
        !generation::checkedSum(points, triangles, generation::kMaxWorkItemCount, perCut) ||
        !generation::checkedSum(perCut, 1U, generation::kMaxWorkItemCount, perCut) ||
        !generation::checkedProduct(perCut, cuts, generation::kMaxWorkItemCount, work) ||
        !generation::checkedSum(next.workItems, work, generation::kMaxWorkItemCount,
                                next.workItems) ||
        !generation::checkedProduct(triangles, 3U, generation::kMaxWorkItemCount, connections) ||
        !appendMeshMergePlan(points, faces, connections, 0U, next.storage) ||
        !validMeshOperationStorage(next.storage))
        return false;
    budget = next;
    return true;
}

} // namespace mesh
} // namespace dommy
