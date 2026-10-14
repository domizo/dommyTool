#pragma once

#include "DAttributeArrayConversion.h"
#include "DGenerationLimits.h"

namespace dommy {
namespace point {

// One budget for all source partitions. Work counts point/target visits and
// closest-point calls, not the internal cost of Maya's native query algorithms.
inline bool validPointForceBudget(std::size_t points, std::size_t partitions,
                                  std::size_t targets, std::size_t iterations,
                                  std::size_t targetConstructionWork = 0U)
{
    using namespace generation;
    std::size_t work = 0U;
    return iterations != 0U && targets <= kMaxGeneratedPointCount &&
        partitions <= kMaxConnectedGeometryCount &&
        attribute::validArrayConversionBudget(points, 4U * sizeof(float)) &&
        attribute::validArrayConversionBudget(partitions,
            2U * sizeof(std::vector<float>) + sizeof(void*)) &&
        attribute::validArrayConversionBudget(targets, sizeof(void*) + sizeof(float)) &&
        checkedProduct(points, targets, kMaxWorkItemCount, work) &&
        checkedProduct(work, iterations, kMaxWorkItemCount, work) &&
        checkedSum(work, points, kMaxWorkItemCount, work) &&
        checkedSum(work, partitions, kMaxWorkItemCount, work) &&
        checkedSum(work, targets, kMaxWorkItemCount, work) &&
        checkedSum(work, targetConstructionWork, kMaxWorkItemCount, work);
}

} // namespace point
} // namespace dommy
