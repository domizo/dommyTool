#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dommy {
namespace random {

// Maya seed attributes use nonnegative signed ints. Preserve the historical
// sequence for in-range additions, then wrap modulo INT_MAX+1. The existing
// single-step API and larger stream offsets share that same contract.
inline bool offsetNonnegativeSeed(int seed, std::uint64_t offset, int& next)
{
    if (seed < 0) return false;
    const std::uint64_t modulus = static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + 1U;
    next = static_cast<int>((static_cast<std::uint64_t>(seed) + offset % modulus) % modulus);
    return true;
}

inline bool nextNonnegativeSeed(int seed, int& next)
{
    return offsetNonnegativeSeed(seed, 1U, next);
}

inline int requireOffsetNonnegativeSeed(int seed, std::uint64_t offset)
{
    int result = 0;
    if (!offsetNonnegativeSeed(seed, offset, result))
        throw std::invalid_argument("Random seed must be nonnegative");
    return result;
}

inline void requireRandomBudget(std::size_t count, std::size_t bytesPerValue)
{
    std::size_t bytes = 0U;
    if (count > generation::kMaxGeneratedPointCount ||
        !generation::checkedProduct(count, bytesPerValue, 256U * 1024U * 1024U, bytes))
        throw std::length_error("Random generation storage limit exceeded");
}

// Retain the historical distribution and sequence for supported ranges.
// Validate the *effective* upper bound, including the optional nextafter step,
// before constructing a distribution. long double need not be wider than double.
template <typename T>
T prepareRealBounds(T& minimum, T& maximum, bool includeMaximum)
{
    static_assert(std::is_floating_point<T>::value, "Real-valued distribution required");
    if (!std::isfinite(minimum) || !std::isfinite(maximum))
        throw std::invalid_argument("Random bounds must be finite");
    if (maximum < minimum) std::swap(minimum, maximum);
    const T upper = includeMaximum && minimum != maximum
        ? std::nextafter(maximum, std::numeric_limits<T>::max()) : maximum;
    const long double width = static_cast<long double>(upper) - minimum;
    if (!std::isfinite(width) || width > std::numeric_limits<T>::max())
        throw std::invalid_argument("Random range is not representable");
    return upper;
}

template <typename T>
void generateRealValues(std::uint32_t seed, T minimum, T maximum,
                        std::size_t count, std::vector<T>& values,
                        bool includeMaximum = false)
{
    requireRandomBudget(count, sizeof(T));
    const T upper = prepareRealBounds(minimum, maximum, includeMaximum);
    std::vector<T> candidate(count, minimum);
    if (minimum != maximum) {
        std::mt19937 engine(seed);
        std::uniform_real_distribution<T> distribution(minimum, upper);
        for (std::size_t i = 0U; i < count; ++i) {
            const T value = distribution(engine);
            if (!std::isfinite(value))
                throw std::runtime_error("Random generation produced a non-finite value");
            candidate[i] = value;
        }
    }
    values.swap(candidate);
}

inline void generateIntValues(std::uint32_t seed,
                              int minimum,
                              int maximum,
                              std::size_t count,
                              std::vector<int>& values)
{
    requireRandomBudget(count, sizeof(int));
    if (maximum < minimum) {
        std::swap(minimum, maximum);
    }

    std::mt19937 engine(seed);
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    std::vector<int> candidate(count);
    for (std::size_t index = 0; index < count; ++index) {
        candidate[index] = distribution(engine);
    }
    values.swap(candidate);
}

inline void generateFloatValues(std::uint32_t seed,
                                float minimum,
                                float maximum,
                                std::size_t count,
                                std::vector<float>& values)
{
    generateRealValues(seed, minimum, maximum, count, values);
}

inline void generateDoubleValues(std::uint32_t seed,
                                 double minimum,
                                 double maximum,
                                 std::size_t count,
                                 std::vector<double>& values)
{
    generateRealValues(seed, minimum, maximum, count, values);
}

template <typename T>
void generateValues(std::uint32_t seed, T minimum, T maximum, std::size_t count,
                    std::vector<T>& values, bool includeMaximum = false)
{
    generateRealValues(seed, minimum, maximum, count, values, includeMaximum);
}

inline void generateValues(std::uint32_t seed, int minimum, int maximum,
                           std::size_t count, std::vector<int>& values, bool = false)
{
    generateIntValues(seed, minimum, maximum, count, values);
}

// Shared by legacy scalar/vector adapters. Conversion is checked before any
// distribution sees a narrowed bound; all three axes are committed together.
template <typename Scalar, typename Bounds, typename Vector>
void generateRealTriples(int seed, const Bounds& minimum, const Bounds& maximum,
                         std::size_t count, std::vector<Vector>& values,
                         bool includeMaximum = true)
{
    requireRandomBudget(count, sizeof(Vector) + 2U * sizeof(Scalar));
    Scalar lower[3], upper[3];
    int seeds[3];
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        seeds[axis] = requireOffsetNonnegativeSeed(seed, axis);
        if (!numeric::checkedNumericCast(minimum[axis], lower[axis]) ||
            !numeric::checkedNumericCast(maximum[axis], upper[axis]))
            throw std::invalid_argument("Random vector bounds cannot be represented");
        (void)prepareRealBounds(lower[axis], upper[axis], includeMaximum);
    }
    std::vector<Vector> candidate(count);
    std::vector<Scalar> axisValues;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        generateRealValues(static_cast<std::uint32_t>(seeds[axis]), lower[axis], upper[axis],
                           count, axisValues, includeMaximum);
        for (std::size_t i = 0U; i < count; ++i) candidate[i][axis] = axisValues[i];
    }
    values.swap(candidate);
}

} // namespace random
} // namespace dommy
