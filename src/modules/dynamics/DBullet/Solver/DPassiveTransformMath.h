#ifndef DOMMY_DPASSIVE_TRANSFORM_MATH_H
#define DOMMY_DPASSIVE_TRANSFORM_MATH_H

#include "modules/dynamics/DBullet/mvl/mathUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dommy {
namespace bullet {

inline bool normalizedSubstepFraction(
    int substepIndex, int substepCount, float& fraction) noexcept
{
    if (substepCount <= 0 || substepIndex < 0 ||
        substepIndex > substepCount) {
        return false;
    }
    fraction = static_cast<float>(substepIndex) /
               static_cast<float>(substepCount);
    return std::isfinite(fraction);
}

inline bool finiteVector(const vec3f& value) noexcept
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
           std::isfinite(value[2]);
}

inline bool finiteQuaternion(const quatf& value) noexcept
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
           std::isfinite(value[2]) && std::isfinite(value[3]);
}

inline bool normalizeQuaternion(
    const quatf& input, quatf& output) noexcept
{
    if (!finiteQuaternion(input)) {
        return false;
    }

    const double lengthSquared =
        static_cast<double>(input[0]) * input[0] +
        static_cast<double>(input[1]) * input[1] +
        static_cast<double>(input[2]) * input[2] +
        static_cast<double>(input[3]) * input[3];
    if (!std::isfinite(lengthSquared) ||
        lengthSquared <=
            static_cast<double>(std::numeric_limits<float>::epsilon())) {
        return false;
    }

    const float inverseLength =
        static_cast<float>(1.0 / std::sqrt(lengthSquared));
    output = inverseLength * input;
    return finiteQuaternion(output);
}

inline bool interpolateQuaternionShortest(
    const quatf& start,
    const quatf& end,
    float fraction,
    quatf& output) noexcept
{
    if (!std::isfinite(fraction) || fraction < 0.0F || fraction > 1.0F) {
        return false;
    }

    quatf normalizedStart;
    quatf normalizedEnd;
    if (!normalizeQuaternion(start, normalizedStart) ||
        !normalizeQuaternion(end, normalizedEnd)) {
        return false;
    }

    float cosine = mvl::dot(normalizedStart, normalizedEnd);
    if (!std::isfinite(cosine)) {
        return false;
    }
    if (cosine < 0.0F) {
        normalizedEnd = -normalizedEnd;
        cosine = -cosine;
    }
    cosine = std::max(0.0F, std::min(1.0F, cosine));

    quatf interpolated;
    if (cosine > 0.9995F) {
        interpolated =
            normalizedStart +
            fraction * (normalizedEnd - normalizedStart);
    } else {
        const float angle = std::acos(cosine);
        const float sine = std::sin(angle);
        if (!std::isfinite(angle) || !std::isfinite(sine) ||
            std::fabs(sine) <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        const float startScale =
            std::sin((1.0F - fraction) * angle) / sine;
        const float endScale = std::sin(fraction * angle) / sine;
        interpolated =
            startScale * normalizedStart + endScale * normalizedEnd;
    }

    return normalizeQuaternion(interpolated, output);
}

inline bool interpolatePassiveTransform(
    const vec3f& startPosition,
    const vec3f& endPosition,
    const quatf& startRotation,
    const quatf& endRotation,
    float fraction,
    vec3f& outputPosition,
    quatf& outputRotation) noexcept
{
    if (!std::isfinite(fraction) || fraction < 0.0F || fraction > 1.0F ||
        !finiteVector(startPosition) || !finiteVector(endPosition)) {
        return false;
    }

    outputPosition =
        startPosition + fraction * (endPosition - startPosition);
    return finiteVector(outputPosition) &&
           interpolateQuaternionShortest(
               startRotation, endRotation, fraction, outputRotation);
}

} // namespace bullet
} // namespace dommy

#endif
