#pragma once

#include "DCheckedSize.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace dommy {
namespace viewport {

// Persistent position, color, normal and index storage uses 40 bytes per point
// before allocator overhead (roughly 80 MiB per generated buffer set). During
// generation, a bounded temporary Coord array preserves source voxel identity
// and adds about 24 MiB at the two-million-point limit; native work is separate.
const std::size_t kMaxVdbViewportPointCount = 2000000U;

struct PointSamplePlan {
    std::vector<std::size_t> offsets;
    std::vector<std::size_t> sampleCounts;
    std::size_t totalSampleCount;

    PointSamplePlan() : totalSampleCount(0) {}
};

/// Build a deterministic, strict sample plan for a sequence of work items.
///
/// Every non-empty item receives one sample before additional capacity is
/// distributed. If the budget cannot cover every item, samples are spread
/// evenly across the non-empty items. The output is left unchanged on size
/// arithmetic failure.
inline bool buildPointSamplePlan(const std::vector<std::size_t>& availableCounts,
                                 std::size_t maximumPointCount,
                                 bool onePointPerItem,
                                 PointSamplePlan& output)
{
    std::size_t desiredTotal = 0;
    std::size_t nonEmptyCount = 0;

    for (std::size_t index = 0; index < availableCounts.size(); ++index) {
        const std::size_t desired = onePointPerItem
            ? std::min<std::size_t>(availableCounts[index], 1U)
            : availableCounts[index];
        if (desired != 0) {
            ++nonEmptyCount;
        }
        if (!memory::checkedAdd(desiredTotal, desired, desiredTotal)) {
            return false;
        }
    }

    PointSamplePlan plan;
    plan.offsets.assign(availableCounts.size(), 0);
    plan.sampleCounts.assign(availableCounts.size(), 0);
    const std::size_t targetCount = std::min(desiredTotal, maximumPointCount);

    if (targetCount != 0 && nonEmptyCount > targetCount) {
        // Bresenham-style distribution without multiplying two potentially
        // large size_t values.
        std::size_t accumulator = 0;
        for (std::size_t index = 0; index < availableCounts.size(); ++index) {
            if (availableCounts[index] == 0) {
                continue;
            }
            if (accumulator >= nonEmptyCount - targetCount) {
                plan.sampleCounts[index] = 1;
                accumulator -= nonEmptyCount - targetCount;
            } else {
                accumulator += targetCount;
            }
        }
    } else if (targetCount != 0) {
        for (std::size_t index = 0; index < availableCounts.size(); ++index) {
            if (availableCounts[index] != 0) {
                plan.sampleCounts[index] = 1;
            }
        }

        std::size_t remaining = targetCount - nonEmptyCount;
        while (remaining != 0) {
            std::size_t eligibleCount = 0;
            for (std::size_t index = 0; index < availableCounts.size(); ++index) {
                const std::size_t desired = onePointPerItem
                    ? std::min<std::size_t>(availableCounts[index], 1U)
                    : availableCounts[index];
                if (plan.sampleCounts[index] < desired) {
                    ++eligibleCount;
                }
            }
            if (eligibleCount == 0) {
                return false;
            }

            const std::size_t share = std::max<std::size_t>(remaining / eligibleCount, 1U);
            for (std::size_t index = 0;
                 index < availableCounts.size() && remaining != 0;
                 ++index) {
                const std::size_t desired = onePointPerItem
                    ? std::min<std::size_t>(availableCounts[index], 1U)
                    : availableCounts[index];
                const std::size_t capacity =
                    desired - plan.sampleCounts[index];
                const std::size_t addition = std::min(capacity, std::min(share, remaining));
                plan.sampleCounts[index] += addition;
                remaining -= addition;
            }
        }
    }

    std::size_t offset = 0;
    for (std::size_t index = 0; index < plan.sampleCounts.size(); ++index) {
        plan.offsets[index] = offset;
        if (!memory::checkedAdd(offset, plan.sampleCounts[index], offset)) {
            return false;
        }
    }
    plan.totalSampleCount = offset;
    output = plan;
    return true;
}

} // namespace viewport
} // namespace dommy
