#pragma once

#include "DGenerationLimits.h"
#include <stdexcept>

namespace dommy {
namespace generation {

const std::size_t kMaxCurveGenerationBytes = 256U * 1024U * 1024U;

inline bool addCurveGenerationBytes(std::size_t count, std::size_t width,
                                    std::size_t& bytes) noexcept
{
    std::size_t extra = 0U, total = 0U;
    if (!checkedProduct(count, width, kMaxCurveGenerationBytes, extra) ||
        !checkedSum(bytes, extra, kMaxCurveGenerationBytes, total)) return false;
    bytes = total;
    return true;
}

// Conservative admission, not a cap on Maya's allocator or upstream DG caches.
// Includes staged + committed custom curves, knot padding/objects/handles,
// CV/knot capacity, base/noise/ramp arrays and one curve's construction scratch.
// Root scratch covers Fur normal/length/index/random arrays or Aho frames.
inline bool curveGenerationStorageBytes(
    std::size_t curves, std::size_t totalPoints, std::size_t pointsPerCurve,
    std::size_t rootScratchCount, std::size_t& result) noexcept
{
    std::size_t bytes = 0U;
    if (!addCurveGenerationBytes(curves, 2048U, bytes) ||
        !addCurveGenerationBytes(totalPoints, 96U, bytes) ||
        !addCurveGenerationBytes(pointsPerCurve, 512U, bytes) ||
        !addCurveGenerationBytes(rootScratchCount, 256U, bytes)) return false;
    result = bytes;
    return true;
}

// Shared cumulative account for additional native/input and metadata scratch.
// A rejected charge leaves the account unchanged; compute guards catch errors.
class CurveGenerationStorage {
public:
    explicit CurveGenerationStorage(std::size_t initialBytes) { add(1U, initialBytes); }
    void add(std::size_t count, std::size_t width) {
        if (!addCurveGenerationBytes(count, width, bytes_))
            throw std::length_error("Curve generation exceeds its storage budget");
    }
    std::size_t estimatedBytes() const { return bytes_; }
private:
    std::size_t bytes_ = 0U;
};

} // namespace generation
} // namespace dommy
