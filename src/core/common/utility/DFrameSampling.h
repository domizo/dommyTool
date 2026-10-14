#pragma once

#include "DCheckedSize.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace sampling {

const std::size_t kMaxExportFrameSamples = 1000000U;
const std::size_t kMaxExportSampleEvents = 10000000U;
const std::size_t kMaxExportShutterSamples = 4096U;
const std::size_t kMaxExportJobs = 1024U;
const std::size_t kMaxExportArgumentCharacters = 4U * 1024U * 1024U;

inline bool isFiniteFrameValue(double value)
{
    return std::isfinite(value);
}

inline bool checkedFrameCount(double start,
                              double end,
                              double stride,
                              std::size_t maximum,
                              std::size_t& count)
{
    count = 0;
    if (!isFiniteFrameValue(start) || !isFiniteFrameValue(end) ||
        !isFiniteFrameValue(stride) || stride <= 0.0 || end < start ||
        maximum == 0U) {
        return false;
    }

    const long double span =
        static_cast<long double>(end) - static_cast<long double>(start);
    const long double intervalCount =
        std::floor(span / static_cast<long double>(stride));
    if (!std::isfinite(intervalCount) || intervalCount < 0.0L ||
        intervalCount >= static_cast<long double>(maximum)) {
        return false;
    }

    const std::size_t result = static_cast<std::size_t>(intervalCount) + 1U;
    if (result > maximum) {
        return false;
    }
    count = result;
    return true;
}

inline bool frameAt(double start,
                    double stride,
                    std::size_t index,
                    double& frame)
{
    if (!isFiniteFrameValue(start) || !isFiniteFrameValue(stride) ||
        stride <= 0.0) {
        return false;
    }
    const long double value = static_cast<long double>(start) +
        static_cast<long double>(stride) * static_cast<long double>(index);
    if (!std::isfinite(value) ||
        value < -static_cast<long double>(std::numeric_limits<double>::max()) ||
        value > static_cast<long double>(std::numeric_limits<double>::max())) {
        return false;
    }
    frame = static_cast<double>(value);
    return isFiniteFrameValue(frame);
}

inline bool checkedSampleEventCount(std::size_t frameCount,
                                    std::size_t shutterCount,
                                    std::size_t& eventCount)
{
    return memory::checkedMultiply(frameCount, shutterCount, eventCount) &&
           eventCount <= kMaxExportSampleEvents;
}

} // namespace sampling
} // namespace dommy
