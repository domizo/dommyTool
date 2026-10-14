#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace dommy {
namespace attribute {
namespace detail {

// The exact product of a 64-bit integer distance and a double significand fits
// in 128 bits. Fixed unsigned limbs keep this portable to C++11/MSVC, with no
// __int128, heap allocation, or dependence on long-double precision.
struct BlendMagnitude {
    std::uint64_t high, low;
};

inline BlendMagnitude multiplyBlendMagnitude(std::uint64_t a, std::uint64_t b) {
    const std::uint64_t mask = UINT64_C(0xffffffff);
    const std::uint64_t a0 = a & mask, a1 = a >> 32;
    const std::uint64_t b0 = b & mask, b1 = b >> 32;
    const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    const std::uint64_t middle = (p00 >> 32) + (p01 & mask) + (p10 & mask);
    return {p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32),
            (middle << 32) | (p00 & mask)};
}

inline unsigned int blendBitCount(BlendMagnitude value) {
    unsigned int count = value.high ? 64U : 0U;
    std::uint64_t remaining = value.high ? value.high : value.low;
    while (remaining) { ++count; remaining >>= 1; }
    return count;
}

// Split product * 2^shift into its integer part and a nonzero-fraction flag.
// A term with more than 65 integer bits cannot yield a 64-bit result even after
// cancellation with either integer endpoint.
inline bool scaleBlendMagnitude(BlendMagnitude product, int shift,
                                 BlendMagnitude& integer, bool& fraction) {
    fraction = false;
    if (shift >= 0) {
        if (blendBitCount(product) + static_cast<unsigned int>(shift) > 65U) return false;
        if (shift == 0) integer = product;
        else if (shift < 64)
            integer = {(product.high << shift) | (product.low >> (64 - shift)), product.low << shift};
        else integer = {product.low << (shift - 64), 0U};
        return true;
    }
    const unsigned int right = static_cast<unsigned int>(-shift);
    if (right >= 128U) {
        integer = {0U, 0U};
        fraction = product.high != 0U || product.low != 0U;
    } else if (right >= 64U) {
        integer = {0U, product.high >> (right - 64U)};
        fraction = product.low != 0U ||
                   (right > 64U && (product.high << (128U - right)) != 0U);
    } else {
        integer = {product.high >> right, (product.low >> right) | (product.high << (64U - right))};
        fraction = (product.low << (64U - right)) != 0U;
    }
    return blendBitCount(integer) <= 65U;
}

template <typename Value> std::uint64_t integerMagnitude(Value value) {
    const std::uint64_t converted = static_cast<std::uint64_t>(value);
    return std::is_signed<Value>::value && value < 0 ? std::uint64_t(0) - converted : converted;
}

template <typename Value>
bool storeBlendInteger(BlendMagnitude magnitude, bool negative, Value& output) {
    if (magnitude.high) return false;
    if (!negative || magnitude.low == 0U) {
        if (magnitude.low > static_cast<std::uint64_t>(std::numeric_limits<Value>::max()))
            return false;
        output = static_cast<Value>(magnitude.low);
        return true;
    }
    if (!std::is_signed<Value>::value) return false;
    const std::uint64_t minimumMagnitude = integerMagnitude(std::numeric_limits<Value>::min());
    if (magnitude.low > minimumMagnitude) return false;
    if (magnitude.low == minimumMagnitude) output = std::numeric_limits<Value>::min();
    else output = static_cast<Value>(-static_cast<Value>(magnitude.low));
    return true;
}

// Shared signed-magnitude completion for floating/integer weights. No output
// is changed before the final range admission, including mixed signed types.
template <typename Value, typename Output>
bool finishIntegerBlend(Value source, BlendMagnitude term, bool termNegative,
                        bool hasFraction, Output& output) {
    const bool sourceNegative = std::is_signed<Value>::value && source < 0;
    const std::uint64_t start = integerMagnitude(source);
    bool negative = termNegative;
    if (sourceNegative == termNegative) {
        const std::uint64_t previous = term.low;
        term.low += start;
        term.high += term.low < previous ? 1U : 0U;
    } else if (term.high || term.low >= start) {
        const std::uint64_t previous = term.low;
        term.low -= start;
        term.high -= previous < start ? 1U : 0U;
    } else {
        term.low = start - term.low;
        negative = sourceNegative;
    }
    // A fractional term opposing the integer sum moves truncation one unit
    // toward zero (e.g. 5 - 0.25 -> 4, or -5 + 0.25 -> -4).
    if (hasFraction && negative != termNegative && (term.high || term.low)) {
        if (term.low == 0U) --term.high;
        --term.low;
    }
    return storeBlendInteger(term, negative, output);
}

} // namespace detail

// Compute trunc(source + (target-source)*weight) for the exact binary double
// weight. Keep the full integer endpoints, including adjacent values above 2^53.
template <typename Value>
bool checkedIntegerBlend(Value source, Value target, double weight, Value& output) {
    static_assert(std::is_integral<Value>::value && sizeof(Value) <= sizeof(std::uint64_t),
                  "Integer blending supports up to 64-bit integers");
    static_assert(std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits <= 63,
                  "Integer blending requires a binary double significand fitting uint64_t");
    if (!std::isfinite(weight)) return false;
    if (weight == 0.0 || source == target) { output = source; return true; }
    if (weight == 1.0) { output = target; return true; }
    const bool descending = target < source;
    const std::uint64_t distance = descending
        ? static_cast<std::uint64_t>(source) - static_cast<std::uint64_t>(target)
        : static_cast<std::uint64_t>(target) - static_cast<std::uint64_t>(source);
    int exponent = 0;
    const double fraction = std::frexp(std::fabs(weight), &exponent);
    const int digits = std::numeric_limits<double>::digits;
    const std::uint64_t significand = static_cast<std::uint64_t>(std::ldexp(fraction, digits));
    detail::BlendMagnitude term;
    bool hasFraction = false;
    if (!detail::scaleBlendMagnitude(detail::multiplyBlendMagnitude(distance, significand),
                                      exponent - digits, term, hasFraction)) return false;
    return detail::finishIntegerBlend(source, term, descending != std::signbit(weight), hasFraction, output);
}

// Integral weights remain exact, even above 2^53. Output may be the usual
// arithmetic result type rather than the endpoint type; reject wraparound.
template <typename Value, typename Weight, typename Output>
typename std::enable_if<std::is_integral<Weight>::value && std::is_integral<Output>::value, bool>::type
checkedIntegerBlend(Value source, Value target, Weight weight, Output& output) {
    static_assert(std::is_integral<Value>::value && sizeof(Value) <= sizeof(std::uint64_t) &&
                  sizeof(Weight) <= sizeof(std::uint64_t) && sizeof(Output) <= sizeof(std::uint64_t),
                  "Integer blending supports up to 64-bit scalars");
    const bool descending = target < source;
    const std::uint64_t distance = descending
        ? static_cast<std::uint64_t>(source) - static_cast<std::uint64_t>(target)
        : static_cast<std::uint64_t>(target) - static_cast<std::uint64_t>(source);
    const detail::BlendMagnitude term = detail::multiplyBlendMagnitude(distance, detail::integerMagnitude(weight));
    const bool weightNegative = std::is_signed<Weight>::value && weight < 0;
    return detail::finishIntegerBlend(source, term, descending != weightNegative, false, output);
}

} // namespace attribute
} // namespace dommy
