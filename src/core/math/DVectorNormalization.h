#pragma once

#include <algorithm>
#include <cmath>

namespace dommy {
namespace math {

template <typename T>
inline bool normalized2OrZero(T x, T y, T& outputX, T& outputY)
{
    outputX = T(0);
    outputY = T(0);
    if (!std::isfinite(static_cast<double>(x)) ||
        !std::isfinite(static_cast<double>(y))) {
        return false;
    }

    const T scale = std::max(std::abs(x), std::abs(y));
    if (!(scale > T(0))) return false;

    const T scaledX = x / scale;
    const T scaledY = y / scale;
    const T length = static_cast<T>(
        std::sqrt(scaledX * scaledX + scaledY * scaledY));
    if (!std::isfinite(static_cast<double>(length)) ||
        !(length > T(0))) {
        return false;
    }

    const T candidateX = scaledX / length;
    const T candidateY = scaledY / length;
    if (!std::isfinite(static_cast<double>(candidateX)) ||
        !std::isfinite(static_cast<double>(candidateY))) {
        return false;
    }
    outputX = candidateX;
    outputY = candidateY;
    return true;
}

template <typename T>
inline bool normalized3OrZero(T x,
                             T y,
                             T z,
                             T& outputX,
                             T& outputY,
                             T& outputZ,
                             T minimumLength = T(0))
{
    outputX = T(0);
    outputY = T(0);
    outputZ = T(0);
    if (!std::isfinite(static_cast<double>(x)) ||
        !std::isfinite(static_cast<double>(y)) ||
        !std::isfinite(static_cast<double>(z)) ||
        !std::isfinite(static_cast<double>(minimumLength)) || minimumLength < T(0)) {
        return false;
    }

    const T scale =
        std::max(std::max(std::abs(x), std::abs(y)), std::abs(z));
    if (!(scale > T(0))) return false;

    const T scaledX = x / scale;
    const T scaledY = y / scale;
    const T scaledZ = z / scale;
    const T length = static_cast<T>(std::sqrt(
        scaledX * scaledX + scaledY * scaledY + scaledZ * scaledZ));
    if (!std::isfinite(static_cast<double>(length)) ||
        !(length > T(0)) || scale <= minimumLength / length) {
        return false;
    }

    const T candidateX = scaledX / length;
    const T candidateY = scaledY / length;
    const T candidateZ = scaledZ / length;
    if (!std::isfinite(static_cast<double>(candidateX)) ||
        !std::isfinite(static_cast<double>(candidateY)) ||
        !std::isfinite(static_cast<double>(candidateZ))) {
        return false;
    }
    outputX = candidateX;
    outputY = candidateY;
    outputZ = candidateZ;
    return true;
}

} // namespace math
} // namespace dommy
