#pragma once

#include "common/utility/DPointMeanAccumulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace dommy {
namespace math {
namespace detail {

template <typename Value>
bool storeInterpolationResult(long double candidate, Value& result)
{
    const long double limit = std::numeric_limits<Value>::max();
    if (!std::isfinite(candidate) || candidate < -limit || candidate > limit) return false;
    result = static_cast<Value>(candidate);
    return true;
}

template <typename Value, typename Weight>
typename std::enable_if<(std::numeric_limits<Value>::digits <= 53 &&
                         std::numeric_limits<Weight>::digits <= 53), bool>::type
extremeInterpolation(Value source, Value target, Weight weight, double mask, Value& result)
{
    // Expand source + (target-source)*weight without rounding the difference
    // or 1-weight first. Round directly to Value (including float), once.
    numeric::detail::BinaryAccumulator<200U, -3222> sum;
    return sum.add(source) && sum.addProduct3(target, weight, mask) &&
        sum.addProduct3(-static_cast<double>(source), weight, mask) && sum.mean(1U, result);
}

template <typename Value, typename Weight>
typename std::enable_if<(std::numeric_limits<Value>::digits > 53 ||
                         std::numeric_limits<Weight>::digits > 53), bool>::type
extremeInterpolation(Value source, Value target, Weight weight, double mask, Value& result)
{
    // Preserve extended precision rather than narrowing long double to the
    // binary64 grid. Scale both the difference and its weight before fma.
    int ae = 0, be = 0, te = 0, me = 0;
    std::frexp(static_cast<long double>(source), &ae);
    std::frexp(static_cast<long double>(target), &be);
    const long double tf = std::frexp(static_cast<long double>(weight), &te) *
                           std::frexp(static_cast<long double>(mask), &me);
    te += me;
    const int endpoints = source == 0 ? be : (target == 0 ? ae : std::max(ae, be));
    const long double delta = std::scalbn(static_cast<long double>(target), -endpoints) -
                              std::scalbn(static_cast<long double>(source), -endpoints);
    const int common = std::max(ae, endpoints + te);
    const long double scaled = std::fma(tf, std::scalbn(delta, endpoints + te - common),
                                       std::scalbn(static_cast<long double>(source), -common));
    if (scaled == 0.0L) { result = static_cast<Value>(scaled); return true; }
    int exponent = 0, limitExponent = 0;
    const long double fraction = std::frexp(scaled, &exponent);
    exponent += common;
    const long double limitFraction = std::frexp(
        static_cast<long double>(std::numeric_limits<Value>::max()), &limitExponent);
    if (exponent > limitExponent ||
        (exponent == limitExponent && std::fabs(fraction) > limitFraction)) return false;
    const int smallest = std::numeric_limits<Value>::min_exponent - std::numeric_limits<Value>::digits;
    const long double candidate = exponent < smallest ? std::copysign(0.0L, fraction)
                                                      : std::scalbn(fraction, exponent);
    return storeInterpolationResult(candidate, result);
}

} // namespace detail

// Shared checked floating interpolation, with no dependency on Maya or on
// numeric validation/casts (which themselves depend on range mathematics).
// Invalid input and unrepresentable final values leave result unchanged.
template <typename Value, typename Weight>
bool checkedLinearInterpolation(Value source, Value target, Weight weight, Value& result)
{
    static_assert(std::is_floating_point<Value>::value && std::is_floating_point<Weight>::value,
                  "floating endpoints and weight required");
    if (!std::isfinite(source) || !std::isfinite(target) || !std::isfinite(weight)) return false;
    if (weight == 0 || source == target) { result = source; return true; }
    if (weight == 1) { result = target; return true; }
    const long double a = source, b = target, t = weight;
    bool moderate = true;
    for (long double value : {a, b, t})
        moderate = moderate && numeric::detail::hasModerateProductMagnitude(value);
    if (moderate) {
        // Preserve the historical attribute-blend rounding in a band where
        // every intermediate is finite and normal, independent of FMA flags.
        const long double candidate = (a <= 0 && b >= 0) || (a >= 0 && b <= 0)
            ? (1.0L - t) * a + t * b : std::fma(t, b - a, a);
        return detail::storeInterpolationResult(candidate, result);
    }
    return detail::extremeInterpolation(source, target, weight, 1.0, result);
}

template <typename Value>
bool checkedMaskedLinearInterpolation(Value source, Value target,
                                       double weight, double mask, Value& result)
{
    static_assert(std::is_floating_point<Value>::value, "floating endpoints required");
    if (!std::isfinite(source) || !std::isfinite(target) ||
        !std::isfinite(weight) || !std::isfinite(mask)) return false;
    if (weight == 0.0 || mask == 0.0 || source == target) { result = source; return true; }
    if (numeric::detail::hasModerateProductMagnitude(weight) &&
        numeric::detail::hasModerateProductMagnitude(mask))
        return checkedLinearInterpolation(source, target, weight * mask, result);
    return detail::extremeInterpolation(source, target, weight, mask, result);
}

} // namespace math
} // namespace dommy
