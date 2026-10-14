#pragma once

#include "common/utility/DNumericValidation.h"

#include <cmath>
#include <cstdint>

namespace dommy {
namespace vdb {

struct TimeMetadata {
    double frame;
    std::int32_t tick;
    std::int32_t framesPerSecond;
    std::int32_t ticksPerFrame;

    TimeMetadata() : frame(0.0), tick(0), framesPerSecond(0), ticksPerFrame(0) {}
};

// Preserve the historical Int32 metadata and OpenVDB 5.2 Round convention
// (floor(x + 0.5), including negative ties). Reject overflow before any cast.
inline bool checkedTimeMetadata(double frame,
                                double framesPerSecond,
                                TimeMetadata& result)
{
    if (!std::isfinite(frame) || !std::isfinite(framesPerSecond) ||
        framesPerSecond <= 0.0) {
        return false;
    }
    const double ticksPerFrame = 6000.0 / framesPerSecond;
    const double tick = frame * ticksPerFrame;
    if (!std::isfinite(ticksPerFrame) || !std::isfinite(tick)) return false;

    TimeMetadata candidate;
    candidate.frame = frame;
    if (!numeric::checkedNumericCast(
            std::floor(tick + 0.5), candidate.tick) ||
        !numeric::checkedNumericCast(
            std::trunc(framesPerSecond), candidate.framesPerSecond) ||
        !numeric::checkedNumericCast(
            std::trunc(ticksPerFrame), candidate.ticksPerFrame)) {
        return false;
    }
    result = candidate;
    return true;
}

} // namespace vdb
} // namespace dommy
