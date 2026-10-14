#pragma once

#include "DGenerationLimits.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace spatial {

const int kMaxSpatialSampling = 1000000;
const int kMaxSpatialConnections = 1000000;

struct SpatialSearchPlan {
    SpatialSearchPlan()
        : searchPointCount(0U),
          queryPointCount(0U),
          maximumNeighbors(0U),
          maximumConnections(0U),
          workItemBudget(0U),
          outputPairBudget(0U),
          radius(0.0) {}

    std::size_t searchPointCount;
    std::size_t queryPointCount;
    std::size_t maximumNeighbors;
    std::size_t maximumConnections;
    // Retained-neighbor budget; tree traversal/heap work is checked separately
    // by the index-owned RadiusSearchBudget during bounded radius searches.
    std::size_t workItemBudget;
    std::size_t outputPairBudget;
    double radius;
};

inline bool buildSpatialSearchPlan(std::size_t searchPointCount,
                                   std::size_t queryPointCount,
                                   int maximumSampling,
                                   int maximumConnections,
                                   double radius,
                                   SpatialSearchPlan& plan)
{
    if (searchPointCount > generation::kMaxGeneratedPointCount ||
        queryPointCount > generation::kMaxGeneratedPointCount ||
        maximumSampling <= 0 ||
        maximumSampling > kMaxSpatialSampling ||
        maximumConnections <= 0 ||
        maximumConnections > kMaxSpatialConnections ||
        !std::isfinite(radius) ||
        radius <= std::numeric_limits<double>::epsilon() ||
        !std::isfinite(radius * radius)) {
        return false;
    }

    SpatialSearchPlan candidate;
    candidate.searchPointCount = searchPointCount;
    candidate.queryPointCount = queryPointCount;
    candidate.maximumNeighbors = std::min(
        searchPointCount, static_cast<std::size_t>(maximumSampling));
    candidate.maximumConnections = std::min(
        candidate.maximumNeighbors,
        static_cast<std::size_t>(maximumConnections));
    if (!generation::checkedProduct(
            queryPointCount,
            candidate.maximumNeighbors,
            generation::kMaxWorkItemCount,
            candidate.workItemBudget) ||
        !generation::checkedProduct(
            queryPointCount,
            candidate.maximumConnections,
            generation::kMaxGeneratedCurveCount,
            candidate.outputPairBudget)) {
        return false;
    }

    candidate.radius = radius;
    plan = candidate;
    return true;
}

template <typename IdLists, typename DistanceLists>
inline bool validateSpatialNeighborLists(const IdLists& ids,
                                         const DistanceLists& distances,
                                         const SpatialSearchPlan& plan)
{
    if (ids.size() != plan.queryPointCount ||
        distances.size() != plan.queryPointCount) {
        return false;
    }

    std::size_t workItems = 0U;
    for (std::size_t query = 0U; query < ids.size(); ++query) {
        if (ids[query].size() != distances[query].size() ||
            ids[query].size() > plan.maximumNeighbors) {
            return false;
        }

        std::size_t updatedWorkItems = 0U;
        if (!generation::checkedSum(
                workItems,
                ids[query].size(),
                plan.workItemBudget,
                updatedWorkItems)) {
            return false;
        }
        workItems = updatedWorkItems;

        for (std::size_t neighbor = 0U;
             neighbor < ids[query].size(); ++neighbor) {
            if (static_cast<std::size_t>(ids[query][neighbor]) >=
                    plan.searchPointCount ||
                !std::isfinite(
                    static_cast<double>(distances[query][neighbor])) ||
                distances[query][neighbor] < 0.0) {
                return false;
            }
        }
    }
    return true;
}

} // namespace spatial
} // namespace dommy
