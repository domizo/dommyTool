#pragma once

#include "DGenerationLimits.h"
#include "DRandomValueGeneration.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dommy {
namespace sampling {

const std::size_t kMaxTimeSamples = generation::kMaxCurveSampleCount;

// Conservative staging allowance for point arrays, flattened output, metadata,
// ordering indices and group objects. This is not Maya's upstream/cache budget.
inline bool validTimeStorage(std::size_t points, std::size_t groups)
{
    const std::size_t maximum = 256U * 1024U * 1024U;
    std::size_t pointBytes = 0U, groupBytes = 0U, bytes = 0U;
    return points <= generation::kMaxGeneratedPointCount &&
        groups <= generation::kMaxGeneratedPointCount &&
        generation::checkedProduct(points, 128U, maximum, pointBytes) &&
        generation::checkedProduct(groups, 128U, maximum, groupBytes) &&
        generation::checkedSum(pointBytes, groupBytes, maximum, bytes);
}

inline bool validTimePointProduct(std::size_t points, std::size_t samples)
{
    std::size_t total = 0U;
    return samples > 0U && samples <= kMaxTimeSamples &&
        generation::checkedProduct(points, samples, generation::kMaxGeneratedPointCount, total) &&
        validTimeStorage(total, points);
}

// Fixed-layout histories retain both the sample rows and their geometry groups.
// Count empty groups too: zero vertices must not permit an unbounded group list.
inline bool validTimeHistoryStorage(std::size_t pointsPerSample,
                                    std::size_t groupsPerSample,
                                    std::size_t samples)
{
    std::size_t points = 0U, groups = 0U;
    return samples > 0U && samples <= kMaxTimeSamples &&
        generation::checkedProduct(pointsPerSample, samples,
            generation::kMaxGeneratedPointCount, points) &&
        generation::checkedProduct(groupsPerSample, samples,
            generation::kMaxGeneratedPointCount, groups) &&
        generation::checkedSum(groups, samples,
            generation::kMaxGeneratedPointCount, groups) &&
        validTimeStorage(points, groups);
}

class TimeSamplingBudget {
public:
    bool addPoints(std::size_t count, std::size_t groups = 1U) {
        std::size_t points = 0U, batches = 0U;
        if (!generation::checkedSum(points_, count, generation::kMaxGeneratedPointCount, points) ||
            !generation::checkedSum(groups_, groups, generation::kMaxGeneratedPointCount, batches) ||
            !validTimeStorage(points, batches)) return false;
        points_ = points;
        groups_ = batches;
        return true;
    }
    bool readPoints(std::size_t count) {
        std::size_t next = 0U;
        if (!generation::checkedSum(work_, count, generation::kMaxWorkItemCount, next) ||
            !generation::checkedSum(next, 1U, generation::kMaxWorkItemCount, next)) return false;
        work_ = next;
        return true;
    }
private:
    std::size_t points_ = 0U, groups_ = 0U, work_ = 0U;
};

inline void requireTimeSamplingInputs(long count, float length, float offset,
                                      long seed, float random)
{
    if (count < 1 || static_cast<unsigned long>(count) > kMaxTimeSamples ||
        !std::isfinite(length) || !std::isfinite(offset) ||
        seed < 0 || static_cast<unsigned long>(seed) > std::numeric_limits<std::uint32_t>::max() ||
        !std::isfinite(random) || random < 0.0f)
        throw std::invalid_argument("Invalid or excessive time sampling input");
    float lower = -random, upper = random;
    (void)dommy::random::prepareRealBounds(lower, upper, false);
}

inline void requireFiniteTimes(const std::vector<float>& values)
{
    for (float value : values)
        if (!std::isfinite(value))
            throw std::invalid_argument("Time sampling produced a non-finite offset");
}

} // namespace sampling
} // namespace dommy
