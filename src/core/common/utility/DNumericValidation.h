#pragma once

#include "math/DRemapMath.h"
#include "common/utility/DPointMeanAccumulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace numeric {

template <typename T>
inline bool checkedNumericCast(long double value, T& result)
{
    if (!std::isfinite(value)) return false;

    const long double minimum = std::numeric_limits<T>::is_integer
        ? (std::numeric_limits<T>::is_signed
            ? static_cast<long double>(std::numeric_limits<T>::min())
            : 0.0L)
        : -static_cast<long double>(std::numeric_limits<T>::max());
    const long double maximum =
        static_cast<long double>(std::numeric_limits<T>::max());
    if (value < minimum || value > maximum) return false;

    // With a 53-bit long double, INT64_MAX / UINT64_MAX round up to
    // 2^63 / 2^64. Those powers of two are exact but are not valid integer
    // values. Keep this exclusive bound in addition to the inclusive check
    // above (which preserves rejection of fractional values above small maxes).
    if (std::numeric_limits<T>::is_integer &&
        value >= std::ldexp(1.0L, std::numeric_limits<T>::digits)) {
        return false;
    }

    result = static_cast<T>(value);
    return true;
}

inline bool canRepresentAsFloat(double value)
{
    const double maximum =
        static_cast<double>(std::numeric_limits<float>::max());
    return std::isfinite(value) && value >= -maximum && value <= maximum;
}

// Spatial keys need exact integer headroom for neighboring-cell offsets.
// Floating LLONG bounds cannot express that margin on 53-bit long double.
inline bool checkedSpatialCellIndex(double value, double inverseCellSize,
                                    long long& result)
{
    if (!std::isfinite(value) || !std::isfinite(inverseCellSize) ||
        inverseCellSize <= 0.0) return false;
    const long double scaled = static_cast<long double>(value) *
                               static_cast<long double>(inverseCellSize);
    long long candidate = 0;
    if (!checkedNumericCast(std::floor(scaled), candidate) ||
        candidate < std::numeric_limits<long long>::min() + 2LL ||
        candidate > std::numeric_limits<long long>::max() - 2LL) return false;
    result = candidate;
    return true;
}

inline bool checkedFloatCast(double value, float& result)
{
    if (!canRepresentAsFloat(value)) return false;

    const float candidate = static_cast<float>(value);
    if (!std::isfinite(candidate)) return false;
    result = candidate;
    return true;
}

// Exact cardinality, checked conversion, and transactional output. A separate
// count avoids depending on Maya's array interface in the host-free tests.
template <typename Array, typename T>
bool checkedScalarArrayCopy(const Array& source, std::size_t sourceCount,
                            std::size_t expectedCount, std::vector<T>& result)
{
    if (sourceCount != expectedCount) return false;
    std::vector<T> candidate(sourceCount);
    for (std::size_t i = 0U; i < sourceCount; ++i) {
        if (!checkedNumericCast(source[i], candidate[i])) return false;
    }
    result.swap(candidate);
    return true;
}

template <typename Array, typename Vector>
bool checkedVectorArrayCopy(const Array& source, std::size_t sourceCount,
                            std::size_t expectedCount,
                            std::vector<Vector>& result)
{
    if (sourceCount != expectedCount) return false;
    std::vector<Vector> candidate(sourceCount);
    for (std::size_t i = 0U; i < sourceCount; ++i) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!checkedNumericCast(source[i][axis], candidate[i][axis]))
                return false;
        }
    }
    result.swap(candidate);
    return true;
}

inline bool clampUnitToFloat(double value, float& result)
{
    if (!std::isfinite(value)) return false;
    return checkedFloatCast(std::max(0.0, std::min(1.0, value)), result);
}

inline bool isWithinInclusiveBounds(double value,
                                    double firstBound,
                                    double secondBound)
{
    if (!std::isfinite(value) || !std::isfinite(firstBound) ||
        !std::isfinite(secondBound)) {
        return false;
    }

    const double minimum = std::min(firstBound, secondBound);
    const double maximum = std::max(firstBound, secondBound);
    return value >= minimum && value <= maximum;
}

inline bool validAscendingRange(double rangeStart, double rangeEnd)
{
    if (!std::isfinite(rangeStart) || !std::isfinite(rangeEnd) || rangeEnd <= rangeStart) return false;
    double spanFraction = 0.0;
    int spanExponent = 0, thresholdExponent = 0;
    math::detail::scaledRangeDifference(rangeEnd, rangeStart, spanFraction, spanExponent);
    const double thresholdFraction = std::frexp(1.0e-12, &thresholdExponent);
    return spanExponent > thresholdExponent ||
        (spanExponent == thresholdExponent && spanFraction > thresholdFraction);
}

inline bool normalizeRange(double value,
                           double rangeStart,
                           double rangeEnd,
                           double& result)
{
    if (!std::isfinite(value) || !validAscendingRange(rangeStart, rangeEnd)) return false;
    double candidate = 0.0;
    if (!math::normalizeRange(value, rangeStart, rangeEnd, candidate)) return false;
    result = candidate;
    return true;
}

inline bool wrapPeriodic(double value,
                         double rangeStart,
                         double rangeEnd,
                         double& result)
{
    if (!std::isfinite(value) || !validAscendingRange(rangeStart, rangeEnd)) return false;
    if (value >= rangeStart && value < rangeEnd) { result = value; return true; }
    if (value == rangeEnd) { result = rangeStart; return true; }
    double candidate = 0.0;
    const double moderate = std::ldexp(1.0, 100);
    bool fast = std::fabs(value) <= moderate && std::fabs(rangeStart) <= moderate &&
                std::fabs(rangeEnd) <= moderate;
    double range = 0.0, offset = 0.0;
    if (fast) {
        range = rangeEnd - rangeStart;
        offset = value - rangeStart;
        fast = detail::additionResidual(rangeEnd, -rangeStart, range) == 0.0 &&
               detail::additionResidual(value, -rangeStart, offset) == 0.0;
    }
    if (fast) {
        double wrappedOffset = std::fmod(offset, range);
        if (wrappedOffset < 0.0) wrappedOffset += range;
        candidate = rangeStart + wrappedOffset;
    } else {
        detail::PointMeanAccumulator offset, period, origin;
        if (!offset.add(value) || !offset.add(-rangeStart) ||
            !period.add(rangeEnd) || !period.add(-rangeStart) || !origin.add(rangeStart) ||
            !offset.periodicValue(period, origin, candidate)) return false;
    }
    result = candidate >= rangeEnd ? rangeStart : candidate;
    return true;
}

// Wrap a bounded sum of original products, before rounded multiplication or
// addition can erase phase. Callers retain their existing domain-size policy.
// Ordinary exact arithmetic uses fmod; ambiguous arithmetic reuses the fixed
// binary product grid. Rejection leaves output unchanged.
template <std::size_t Count>
bool wrapPeriodicProductSum(const std::array<double, Count>& left,
                            const std::array<double, Count>& right,
                            double rangeStart, double rangeEnd, double& result)
{
    static_assert(Count > 0U && Count <= 16U, "bounded periodic product sum required");
    if (!std::isfinite(rangeStart) || !std::isfinite(rangeEnd) || rangeEnd <= rangeStart)
        return false;
    const double moderate = std::ldexp(1.0, 100);
    bool fast = std::fabs(rangeStart) <= moderate && std::fabs(rangeEnd) <= moderate;
    for (std::size_t i = 0U; i < Count; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return false;
        fast = fast && detail::hasModerateProductMagnitude(left[i]) &&
            detail::hasModerateProductMagnitude(right[i]);
    }
    double candidate = 0.0;
    if (fast) {
        double value = 0.0;
        for (std::size_t i = 0U; i < Count; ++i) {
            const double product = left[i] * right[i];
            const double sum = value + product;
            fast = fast && std::fma(left[i], right[i], -product) == 0.0 &&
                detail::additionResidual(value, product, sum) == 0.0;
            value = sum;
        }
        const double span = rangeEnd - rangeStart, offset = value - rangeStart;
        fast = fast && detail::additionResidual(rangeEnd, -rangeStart, span) == 0.0 &&
            detail::additionResidual(value, -rangeStart, offset) == 0.0;
        if (fast) {
            double phase = std::fmod(offset, span);
            if (phase < 0.0) phase += span;
            candidate = rangeStart + phase;
        }
    }
    if (!fast) {
        detail::BinaryAccumulator<136U, -2148> value, span, origin;
        for (std::size_t i = 0U; i < Count; ++i)
            if (!value.addProduct(left[i], right[i])) return false;
        if (!value.add(-rangeStart) || !span.add(rangeEnd) || !span.add(-rangeStart) ||
            !origin.add(rangeStart) || !value.periodicValue(span, origin, candidate)) return false;
    }
    if (!std::isfinite(candidate)) return false;
    result = candidate >= rangeEnd ? rangeStart : candidate;
    return true;
}

} // namespace numeric
} // namespace dommy
