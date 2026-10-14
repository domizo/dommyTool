#pragma once

#include "DLinearInterpolation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace dommy {
namespace math {

namespace detail {

// Represent a difference as fraction*2^exponent rather than forming a span
// that may exceed the scalar type. Independent numerator/denominator scaling
// also retains a narrow interval when the queried value is very far away.
// Call only after finite input admission; retain T's precision (including
// extended long double), rather than silently narrowing the differences.
template <typename T>
inline void scaledRangeDifference(T first, T second, T& fraction, int& exponent)
{
    if (first == second) { fraction = T(0); exponent = 0; return; }
    int firstExponent = 0, secondExponent = 0;
    std::frexp(first, &firstExponent); std::frexp(second, &secondExponent);
    const int common = first == T(0) ? secondExponent :
                       (second == T(0) ? firstExponent : std::max(firstExponent, secondExponent));
    const T difference = std::scalbn(first, -common) - std::scalbn(second, -common);
    int differenceExponent = 0;
    fraction = std::frexp(difference, &differenceExponent);
    exponent = common + differenceExponent;
}

} // namespace detail

template <typename T>
inline bool normalizeRange(const T value, const T minimum, const T maximum, T& normalized) {
    static_assert(std::is_floating_point<T>::value,
                  "normalizeRange requires a floating-point type");
    static_assert(std::numeric_limits<T>::radix == 2,
                  "normalizeRange requires binary floating point");
    if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) ||
        maximum == minimum) {
        normalized = T(0);
        return false;
    }

    T numerator, denominator;
    int numeratorExponent = 0, denominatorExponent = 0;
    detail::scaledRangeDifference(value, minimum, numerator, numeratorExponent);
    detail::scaledRangeDifference(maximum, minimum, denominator, denominatorExponent);
    if (numerator == T(0)) { normalized = T(0); return true; }
    int quotientExponent = 0;
    const T fraction = std::frexp(numerator / denominator, &quotientExponent);
    const int exponent = numeratorExponent - denominatorExponent + quotientExponent;
    int maximumExponent = 0;
    const T maximumFraction = std::frexp(std::numeric_limits<T>::max(), &maximumExponent);
    // Reject before scalbn, so true final overflow raises no FE_OVERFLOW.
    if (exponent > maximumExponent ||
        (exponent == maximumExponent && std::fabs(fraction) > maximumFraction)) {
        normalized = T(0);
        return false;
    }
    const int minimumExponent = std::numeric_limits<T>::min_exponent - std::numeric_limits<T>::digits;
    normalized = exponent < minimumExponent ? std::copysign(T(0), fraction) : std::scalbn(fraction, exponent);
    return true;
}

template <typename T>
inline T denormalizeRange(const T normalized, const T minimum, const T maximum) {
    static_assert(std::is_floating_point<T>::value,
                  "denormalizeRange requires a floating-point type");
    T result = T(0);
    return checkedLinearInterpolation(minimum, maximum, normalized, result)
        ? result : std::numeric_limits<T>::quiet_NaN();
}

} // namespace math
} // namespace dommy
