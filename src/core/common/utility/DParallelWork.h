#pragma once

#include <algorithm>
#include <cstddef>

namespace dommy {
namespace parallel {

// Select a stable TBB grain size even when the standard library cannot report
// the number of hardware threads (hardware_concurrency() is allowed to return
// zero).  Keeping this calculation Maya-independent also makes its boundary
// behavior directly testable.
inline std::size_t grainSize(std::size_t itemCount,
                             std::size_t hardwareThreadCount,
                             std::size_t minimumGrainSize = 20000U)
{
    const std::size_t threadCount =
        std::max<std::size_t>(1U, hardwareThreadCount);
    const std::size_t itemsPerThread =
        itemCount / threadCount + (itemCount % threadCount == 0U ? 0U : 1U);
    return std::max<std::size_t>(
        1U, std::max(minimumGrainSize, itemsPerThread));
}

} // namespace parallel
} // namespace dommy
