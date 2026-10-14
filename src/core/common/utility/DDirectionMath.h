#pragma once

#include "common/utility/DNumericValidation.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace direction {

struct Delta
{
    double values[3];
    double length;
};

template <typename FromVector, typename ToVector>
inline bool difference(
    const FromVector& from,
    const ToVector& to,
    Delta& result)
{
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const double fromValue = static_cast<double>(from[axis]);
        const double toValue = static_cast<double>(to[axis]);
        if (!std::isfinite(fromValue) || !std::isfinite(toValue)) return false;

        result.values[axis] = toValue - fromValue;
        if (!std::isfinite(result.values[axis])) return false;
    }

    result.length = std::hypot(
        std::hypot(result.values[0], result.values[1]),
        result.values[2]);
    return std::isfinite(result.length);
}

template <typename OutputVector>
inline bool assignDelta(const Delta& delta, OutputVector& output)
{
    float components[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (!dommy::numeric::checkedFloatCast(
                delta.values[axis], components[axis])) {
            return false;
        }
    }

    output = OutputVector(components[0], components[1], components[2]);
    return true;
}

template <typename InputVector, typename OutputVector>
inline bool assignFinite(const InputVector& input, OutputVector& output)
{
    float components[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (!dommy::numeric::checkedFloatCast(
                static_cast<double>(input[axis]), components[axis])) {
            return false;
        }
    }

    output = OutputVector(components[0], components[1], components[2]);
    return true;
}

template <typename InputVector, typename OutputVector>
inline bool adjust(
    const InputVector& input,
    bool invert,
    bool normalize,
    double scale,
    OutputVector& output)
{
    if (!std::isfinite(scale)) return false;

    double components[3] = {
        static_cast<double>(input[0]),
        static_cast<double>(input[1]),
        static_cast<double>(input[2])};
    for (double component : components) {
        if (!std::isfinite(component)) return false;
    }

    if (invert) {
        for (double& component : components) component = -component;
    }

    if (normalize) {
        const double length = std::hypot(
            std::hypot(components[0], components[1]), components[2]);
        if (!std::isfinite(length)) return false;

        if (length <= std::numeric_limits<double>::epsilon()) {
            components[0] = 0.0;
            components[1] = 0.0;
            components[2] = 0.0;
        } else {
            for (double& component : components) component /= length;
        }
    }

    float adjusted[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (!dommy::numeric::checkedFloatCast(
                components[axis] * scale, adjusted[axis])) {
            return false;
        }
    }

    output = OutputVector(adjusted[0], adjusted[1], adjusted[2]);
    return true;
}

}  // namespace direction
}  // namespace dommy
