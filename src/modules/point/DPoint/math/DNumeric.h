#pragma once

#include <algorithm>
#include <cmath>

namespace dommy {
namespace math {

template <typename T>
inline bool isFinite(const T value)
{
    return std::isfinite(value);
}

template <typename T>
inline T divideOrZero(const T numerator, const T denominator)
{
    if (!isFinite(numerator) || !isFinite(denominator) ||
        denominator == T(0)) {
        return T(0);
    }

    const T result = numerator/denominator;
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T moduloOrZero(const T numerator, const T denominator)
{
    if (!isFinite(numerator) || !isFinite(denominator) ||
        denominator == T(0)) {
        return T(0);
    }

    const T result = static_cast<T>(std::fmod(numerator, denominator));
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T sqrtOrZero(const T value)
{
    if (!isFinite(value) || value <= T(0)) return T(0);

    const T result = static_cast<T>(std::sqrt(value));
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T powerOrZero(const T base, const T exponent)
{
    if (!isFinite(base) || !isFinite(exponent)) return T(0);

    const T result = static_cast<T>(std::pow(base, exponent));
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T exponentialOrZero(const T value)
{
    if (!isFinite(value)) return T(0);

    const T result = static_cast<T>(std::exp(value));
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T acosClamped(const T value)
{
    if (!isFinite(value)) return T(0);

    const T clamped = std::max(T(-1), std::min(T(1), value));
    const T result = static_cast<T>(std::acos(clamped));
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T angleRadians(const T dotProduct, const T firstLength, const T secondLength)
{
    if (!isFinite(dotProduct) || !isFinite(firstLength) ||
        !isFinite(secondLength) || firstLength <= T(0) ||
        secondLength <= T(0)) {
        return T(0);
    }

    // Divide sequentially to avoid overflowing the product of two valid,
    // large vector lengths.
    return acosClamped(
        divideOrZero(divideOrZero(dotProduct, firstLength), secondLength));
}

template <typename T>
inline T fitRange(const T sourceMin,
                  const T sourceMax,
                  const T destinationMin,
                  const T destinationMax,
                  const T value)
{
    if (!isFinite(sourceMin) || !isFinite(sourceMax) ||
        !isFinite(destinationMin) || !isFinite(destinationMax) ||
        !isFinite(value)) {
        return T(0);
    }

    const T sourceWidth = sourceMax-sourceMin;
    if (!isFinite(sourceWidth) || sourceWidth == T(0)) {
        return destinationMin;
    }

    const T result =
        ((value-sourceMin)*(destinationMax-destinationMin)/sourceWidth) +
        destinationMin;
    return isFinite(result) ? result : T(0);
}

template <typename T>
inline T fitRangeClamped(const T sourceMin,
                         const T sourceMax,
                         const T destinationMin,
                         const T destinationMax,
                         const T value)
{
    const T mapped = fitRange(sourceMin, sourceMax, destinationMin, destinationMax, value);
    if (!isFinite(mapped) || !isFinite(destinationMin) ||
        !isFinite(destinationMax)) {
        return T(0);
    }

    const T lower = std::min(destinationMin, destinationMax);
    const T upper = std::max(destinationMin, destinationMax);
    return std::max(lower, std::min(upper, mapped));
}

} // namespace math
} // namespace dommy
