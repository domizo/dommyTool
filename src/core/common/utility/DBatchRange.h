#pragma once

#include <algorithm>
#include <cstddef>

namespace dommy {
namespace batch {

// Keep host-renderer calls bounded even when procedural nodes hold millions of
// points. The value is intentionally independent of scene attribute limits.
const std::size_t kMaximumShadingSampleCount = 4096U;

struct Range {
    Range(std::size_t beginValue = 0U, std::size_t countValue = 0U)
        : begin(beginValue), count(countValue) {}

    std::size_t end() const { return begin + count; }
    bool empty() const { return count == 0U; }

    std::size_t begin;
    std::size_t count;
};

inline Range rangeAt(std::size_t totalCount,
                     std::size_t maximumBatchSize,
                     std::size_t begin)
{
    if (maximumBatchSize == 0U || begin >= totalCount) {
        return Range(begin, 0U);
    }

    return Range(
        begin, std::min(maximumBatchSize, totalCount - begin));
}

} // namespace batch
} // namespace dommy
