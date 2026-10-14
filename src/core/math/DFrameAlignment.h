#pragma once

#include "modules/point/DPoint/math/DVectorMath.h"

#include <array>

namespace dommy {
namespace math {

using FrameDirection = std::array<double, 3>;
using FrameRotation = std::array<FrameDirection, 3>;

namespace frame_detail {

inline bool normalize(FrameDirection& value, double minimumLength = 0.0) {
    FrameDirection candidate;
    if (!normalized3OrZero(value[0], value[1], value[2], candidate[0], candidate[1], candidate[2],
                           minimumLength)) {
        return false;
    }
    value = candidate;
    return true;
}

inline FrameDirection cross(const FrameDirection& a, const FrameDirection& b) {
    return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

inline bool makeFrame(FrameDirection forward, FrameDirection up, FrameRotation& frame) {
    if (!normalize(forward) || !normalize(up))
        return false;

    // The threshold is an angular test on unit vectors, not a restriction on
    // input magnitude. Parallel defining axes do not determine a unique frame.
    FrameDirection side = cross(forward, up);
    if (!normalize(side, 1.0e-12))
        return false;
    up = cross(side, forward);
    if (!normalize(up))
        return false;
    frame = {{forward, up, side}};
    return true;
}

} // namespace frame_detail

// Row-vector convention: sourceForward * R = normalized(targetForward).
// Forward is preserved; each up axis is orthogonalized against its forward.
// Failure leaves output unchanged. Euler order is irrelevant to this matrix.
inline bool makeFrameAlignment(const FrameDirection& sourceForward, const FrameDirection& sourceUp,
                               const FrameDirection& targetForward, const FrameDirection& targetUp,
                               FrameRotation& output) {
    FrameRotation source, target;
    if (!frame_detail::makeFrame(sourceForward, sourceUp, source) ||
        !frame_detail::makeFrame(targetForward, targetUp, target)) {
        return false;
    }
    FrameRotation candidate = {};
    // source and target have orthonormal rows: R = transpose(source) * target.
    for (unsigned row = 0U; row < 3U; ++row) {
        for (unsigned column = 0U; column < 3U; ++column) {
            for (unsigned axis = 0U; axis < 3U; ++axis) {
                candidate[row][column] += source[axis][row] * target[axis][column];
            }
        }
    }
    output = candidate;
    return true;
}

} // namespace math
} // namespace dommy
