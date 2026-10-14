#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace dommy {
namespace numeric {
namespace detail {

// Up to four products and sixteen terms remain finite/normal in this band.
// Shared admission for legacy-rounding fast paths; zeros are admitted only
// after callers validate every input as finite.
template <typename Scalar>
inline bool hasModerateProductMagnitude(Scalar value)
{
    return value == 0 || (std::fabs(value) >= std::ldexp(Scalar(1), -100) &&
                          std::fabs(value) <= std::ldexp(Scalar(1), 100));
}

// Error-free TwoSum residual when the caller has admitted finite, normal-band
// arithmetic. Shared by certified dot products and exact-offset admission.
inline double additionResidual(double first, double second, double sum)
{
    const double virtualSecond = sum - first;
    return (first - (sum - virtualSecond)) + (second - virtualSecond);
}

// Fixed binary grids share carry, cancellation and rounding between arithmetic
// means and bounded sums of products. Size the grid for the caller's exponent
// range and count; no long-double width assumption or dynamic allocation.
template <std::size_t WordCount, int BaseExponent>
class BinaryAccumulator {
    static_assert(std::numeric_limits<double>::is_iec559 &&
        std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits == 53 &&
        std::numeric_limits<double>::max_exponent == 1024 &&
        std::numeric_limits<double>::min_exponent == -1021, "binary64 double required");
    static_assert(WordCount > 0U && BaseExponent <= -1074,
                  "binary grid must include every finite double");
    using Magnitude = std::array<std::uint32_t, WordCount>;
    Magnitude positive_ = {}, negative_ = {};

    template <std::size_t Size>
    static bool addWord(std::array<std::uint32_t, Size>& value, std::size_t index, std::uint32_t word)
    {
        std::uint64_t carry = word;
        while (carry != 0U) {
            if (index >= value.size()) return false;
            carry += value[index];
            value[index++] = static_cast<std::uint32_t>(carry);
            carry >>= 32U;
        }
        return true;
    }

    static bool addShiftedWord(Magnitude& value, std::size_t index,
                               unsigned int shift, std::uint32_t word)
    {
        const std::uint64_t shifted = static_cast<std::uint64_t>(word) << shift;
        return addWord(value, index, static_cast<std::uint32_t>(shifted)) &&
            addWord(value, index + 1U, static_cast<std::uint32_t>(shifted >> 32U));
    }

    static bool bit(const Magnitude& value, unsigned int index)
    {
        return ((value[index / 32U] >> (index % 32U)) & 1U) != 0U;
    }

    static int compareMagnitude(const Magnitude& first, const Magnitude& second)
    {
        for (std::size_t i = first.size(); i-- > 0U;)
            if (first[i] != second[i]) return first[i] < second[i] ? -1 : 1;
        return 0;
    }

    static void subtractMagnitude(Magnitude& first, const Magnitude& second)
    {
        std::uint64_t borrow = 0U;
        for (std::size_t i = 0U; i < first.size(); ++i) {
            const std::uint64_t subtrahend = static_cast<std::uint64_t>(second[i]) + borrow;
            const std::uint64_t original = first[i];
            first[i] = static_cast<std::uint32_t>(original - subtrahend);
            borrow = original < subtrahend ? 1U : 0U;
        }
    }

    static bool addMagnitude(Magnitude& first, const Magnitude& second)
    {
        for (std::size_t i = 0U; i < first.size(); ++i)
            if (!addWord(first, i, second[i])) return false;
        return true;
    }

    static void decompose(double value, std::uint64_t& significand, int& exponent)
    {
        int inputExponent = 0;
        const double fraction = std::frexp(std::fabs(value), &inputExponent);
        significand = static_cast<std::uint64_t>(std::ldexp(fraction, 53));
        exponent = inputExponent - 53;
        // Subnormals have trailing zero bits in the normalized significand.
        if (exponent < -1074) {
            significand >>= static_cast<unsigned int>(-1074 - exponent);
            exponent = -1074;
        }
    }

    int signedMagnitude(Magnitude& value) const
    {
        const int direction = sign();
        const Magnitude& larger = direction < 0 ? negative_ : positive_;
        const Magnitude& smaller = direction < 0 ? positive_ : negative_;
        value = larger;
        subtractMagnitude(value, smaller);
        return direction;
    }

    // Reuse the same unsigned carry operation for a bounded significand
    // product. Each factor has at most 53 bits, so 2*Count words suffice.
    template <std::size_t Count>
    bool addSignificandProduct(const std::array<std::uint64_t, Count>& factors,
                               int shift, bool negative)
    {
        static_assert(Count > 0U && Count <= 4U, "bounded significand product required");
        if (shift < 0) return false;
        std::array<std::uint32_t, 2U * Count> product = {};
        product[0] = 1U;
        for (std::uint64_t factor : factors) {
            std::array<std::uint32_t, 2U * Count> next = {};
            const std::uint32_t words[2] = {static_cast<std::uint32_t>(factor),
                                            static_cast<std::uint32_t>(factor >> 32U)};
            for (std::size_t i = 0U; i < product.size(); ++i)
                for (std::size_t j = 0U; j < 2U; ++j) {
                    const std::uint64_t term = static_cast<std::uint64_t>(product[i]) * words[j];
                    if (!addWord(next, i + j, static_cast<std::uint32_t>(term)) ||
                        !addWord(next, i + j + 1U, static_cast<std::uint32_t>(term >> 32U))) return false;
                }
            product = next;
        }
        Magnitude& magnitude = negative ? negative_ : positive_;
        const std::size_t index = static_cast<unsigned int>(shift) / 32U;
        const unsigned int offset = static_cast<unsigned int>(shift) % 32U;
        for (std::size_t i = 0U; i < product.size(); ++i)
            if (!addShiftedWord(magnitude, index + i, offset, product[i])) return false;
        return true;
    }

public:
    int sign() const
    {
        return compareMagnitude(positive_, negative_);
    }

    // Euclidean remainder of this exact offset, then add the exact origin
    // before rounding. Neither span nor remainder needs to fit binary64.
    // The fixed grid bounds division work and avoids integer quotient casts.
    bool periodicValue(const BinaryAccumulator& period,
                       const BinaryAccumulator& origin, double& output) const
    {
        Magnitude value = {}, divisor = {}, remainder = {};
        const int direction = signedMagnitude(value);
        if (period.signedMagnitude(divisor) <= 0) return false;
        if (compareMagnitude(value, divisor) < 0) remainder = value;
        else {
            int highest = static_cast<int>(value.size() * 32U) - 1, divisorHighest = highest;
            while (!bit(value, static_cast<unsigned int>(highest))) --highest;
            while (!bit(divisor, static_cast<unsigned int>(divisorHighest))) --divisorHighest;
            const unsigned int shift = static_cast<unsigned int>(highest - divisorHighest);
            Magnitude aligned = {};
            for (std::size_t i = 0U; i < divisor.size(); ++i)
                if (!addShiftedWord(aligned, i + shift / 32U, shift % 32U, divisor[i])) return false;
            remainder = value;
            // Align the divisor, rather than processing every low grid bit.
            // Ordinary small quotients now take only a few word-array passes.
            for (unsigned int index = shift;; --index) {
                if (compareMagnitude(remainder, aligned) >= 0) subtractMagnitude(remainder, aligned);
                if (index == 0U) break;
                std::uint32_t carry = 0U;
                for (std::size_t i = aligned.size(); i-- > 0U;) {
                    const std::uint32_t next = aligned[i] & 1U;
                    aligned[i] = (aligned[i] >> 1U) | (carry << 31U);
                    carry = next;
                }
            }
        }
        BinaryAccumulator candidate(origin);
        const Magnitude zero = {};
        if (direction < 0 && compareMagnitude(remainder, zero) != 0) {
            if (!addMagnitude(candidate.positive_, divisor) ||
                !addMagnitude(candidate.negative_, remainder)) return false;
        } else if (!addMagnitude(candidate.positive_, remainder)) return false;
        return candidate.mean(1U, output);
    }

    bool add(double value)
    {
        if (!std::isfinite(value)) return false;
        if (value == 0.0) return true;
        int exponent = 0;
        std::uint64_t significand = 0U;
        decompose(value, significand, exponent);
        const int shift = exponent - BaseExponent;
        Magnitude& magnitude = std::signbit(value) ? negative_ : positive_;
        const std::size_t index = static_cast<unsigned int>(shift) / 32U;
        const unsigned int offset = static_cast<unsigned int>(shift) % 32U;
        return addShiftedWord(magnitude, index, offset, static_cast<std::uint32_t>(significand)) &&
            addShiftedWord(magnitude, index + 1U, offset, static_cast<std::uint32_t>(significand >> 32U));
    }

    // Multiply significands as four 32x32 products, without first forming a
    // floating product that could overflow/underflow before cancellation.
    bool addProduct(double left, double right)
    {
        if (!std::isfinite(left) || !std::isfinite(right)) return false;
        if (left == 0.0 || right == 0.0) return true;
        std::uint64_t a = 0U, b = 0U;
        int aExponent = 0, bExponent = 0;
        decompose(left, a, aExponent);
        decompose(right, b, bExponent);
        const int shift = aExponent + bExponent - BaseExponent;
        if (shift < 0) return false;
        const std::size_t index = static_cast<unsigned int>(shift) / 32U;
        const unsigned int offset = static_cast<unsigned int>(shift) % 32U;
        Magnitude& magnitude = std::signbit(left) != std::signbit(right) ? negative_ : positive_;
        const std::uint32_t aWords[2] = {static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(a >> 32U)};
        const std::uint32_t bWords[2] = {static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(b >> 32U)};
        for (std::size_t i = 0U; i < 2U; ++i)
            for (std::size_t j = 0U; j < 2U; ++j) {
                const std::uint64_t product = static_cast<std::uint64_t>(aWords[i]) * bWords[j];
                if (!addShiftedWord(magnitude, index + i + j, offset, static_cast<std::uint32_t>(product)) ||
                    !addShiftedWord(magnitude, index + i + j + 1U, offset, static_cast<std::uint32_t>(product >> 32U)))
                    return false;
            }
        return true;
    }

    // Exact triple products are needed for determinant signs and normal
    // cofactors. Split each pair product before multiplying the third
    // significand, so every native multiplication remains 32 by 32 bits.
    bool addProduct3(double first, double second, double third)
    {
        if (!std::isfinite(first) || !std::isfinite(second) || !std::isfinite(third)) return false;
        if (first == 0.0 || second == 0.0 || third == 0.0) return true;
        std::uint64_t a = 0U, b = 0U, c = 0U;
        int ae = 0, be = 0, ce = 0;
        decompose(first, a, ae); decompose(second, b, be); decompose(third, c, ce);
        const int shift = ae + be + ce - BaseExponent;
        if (shift < 0) return false;
        const std::size_t index = static_cast<unsigned int>(shift) / 32U;
        const unsigned int offset = static_cast<unsigned int>(shift) % 32U;
        const bool negative = std::signbit(first) ^ std::signbit(second) ^ std::signbit(third);
        Magnitude& magnitude = negative ? negative_ : positive_;
        const std::uint32_t aw[2] = {static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(a >> 32U)};
        const std::uint32_t bw[2] = {static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(b >> 32U)};
        const std::uint32_t cw[2] = {static_cast<std::uint32_t>(c), static_cast<std::uint32_t>(c >> 32U)};
        for (std::size_t i = 0U; i < 2U; ++i) for (std::size_t j = 0U; j < 2U; ++j) {
            const std::uint64_t pair = static_cast<std::uint64_t>(aw[i]) * bw[j];
            const std::uint32_t pairWords[2] = {static_cast<std::uint32_t>(pair), static_cast<std::uint32_t>(pair >> 32U)};
            for (std::size_t k = 0U; k < 2U; ++k) for (std::size_t l = 0U; l < 2U; ++l) {
                const std::uint64_t product = static_cast<std::uint64_t>(pairWords[k]) * cw[l];
                if (!addShiftedWord(magnitude, index + i + j + k + l, offset, static_cast<std::uint32_t>(product)) ||
                    !addShiftedWord(magnitude, index + i + j + k + l + 1U, offset, static_cast<std::uint32_t>(product >> 32U)))
                    return false;
            }
        }
        return true;
    }

    // Spring/damping responses have four factors per expanded term. Admit
    // every input before honoring zero, so zero cannot conceal NaN/infinity.
    bool addProduct4(double first, double second, double third, double fourth)
    {
        if (!std::isfinite(first) || !std::isfinite(second) ||
            !std::isfinite(third) || !std::isfinite(fourth)) return false;
        if (first == 0.0 || second == 0.0 || third == 0.0 || fourth == 0.0) return true;
        std::array<std::uint64_t, 4> factors;
        int exponents[4];
        decompose(first, factors[0], exponents[0]); decompose(second, factors[1], exponents[1]);
        decompose(third, factors[2], exponents[2]); decompose(fourth, factors[3], exponents[3]);
        const int shift = exponents[0] + exponents[1] + exponents[2] + exponents[3] - BaseExponent;
        const bool negative = std::signbit(first) ^ std::signbit(second) ^
                              std::signbit(third) ^ std::signbit(fourth);
        return addSignificandProduct(factors, shift, negative);
    }

    // Return fraction * 2^exponent without requiring the unscaled value to
    // fit double. The fraction is rounded once; exact zero/sign is retained.
    void scaledValue(double& fraction, int& exponent) const
    {
        Magnitude value = {};
        const int direction = signedMagnitude(value);
        if (direction == 0) { fraction = 0.0; exponent = 0; return; }
        int highest = static_cast<int>(value.size() * 32U) - 1;
        while (highest >= 0 && !bit(value, static_cast<unsigned int>(highest))) --highest;
        const unsigned int shift = highest > 52 ? static_cast<unsigned int>(highest - 52) : 0U;
        std::uint64_t significand = 0U;
        for (int i = highest; i >= static_cast<int>(shift); --i)
            significand = (significand << 1U) | (bit(value, static_cast<unsigned int>(i)) ? 1U : 0U);
        if (shift != 0U) {
            bool sticky = false;
            for (unsigned int i = 0U; i + 1U < shift && !sticky; ++i) sticky = bit(value, i);
            if (bit(value, shift - 1U) && (sticky || (significand & 1U))) ++significand;
        }
        int normalizedExponent = 0;
        const double magnitude = std::frexp(static_cast<double>(significand), &normalizedExponent);
        fraction = direction < 0 ? -magnitude : magnitude;
        exponent = static_cast<int>(shift) + BaseExponent + normalizedExponent;
    }

    template <typename Scalar>
    bool mean(std::uint32_t count, Scalar& output) const
    {
        static_assert(std::is_floating_point<Scalar>::value &&
            std::numeric_limits<Scalar>::radix == 2 && std::numeric_limits<Scalar>::digits <= 53,
            "binary output with at most 53 significand bits required");
        if (count == 0U) return false;
        Magnitude quotient = {};
        const bool negative = signedMagnitude(quotient) < 0;
        std::uint64_t remainder = 0U;
        for (std::size_t i = quotient.size(); i-- > 0U;) {
            const std::uint64_t dividend = (remainder << 32U) | quotient[i];
            quotient[i] = static_cast<std::uint32_t>(dividend / count);
            remainder = dividend % count;
        }
        int highest = static_cast<int>(quotient.size() * 32U) - 1;
        while (highest >= 0 && !bit(quotient, static_cast<unsigned int>(highest))) --highest;
        const int lastBit = std::numeric_limits<Scalar>::digits - 1;
        const unsigned int precisionShift = highest > lastBit
            ? static_cast<unsigned int>(highest - lastBit) : 0U;
        const int smallestExponent = std::numeric_limits<Scalar>::min_exponent -
                                     std::numeric_limits<Scalar>::digits;
        static_assert(BaseExponent <= std::numeric_limits<Scalar>::min_exponent -
                                      std::numeric_limits<Scalar>::digits, "output grid too narrow");
        const unsigned int subnormalShift = static_cast<unsigned int>(smallestExponent - BaseExponent);
        const unsigned int shift = precisionShift > subnormalShift ? precisionShift : subnormalShift;
        std::uint64_t significand = 0U;
        for (int i = highest; i >= static_cast<int>(shift); --i)
            significand = (significand << 1U) | (bit(quotient, static_cast<unsigned int>(i)) ? 1U : 0U);

        // Round the exact rational only once, to nearest with ties to even.
        bool roundUp;
        if (shift == 0U) {
            roundUp = remainder * 2U > count ||
                (remainder * 2U == count && (significand & 1U));
        } else {
            bool sticky = remainder != 0U;
            for (unsigned int i = 0U; i + 1U < shift && !sticky; ++i) sticky = bit(quotient, i);
            roundUp = bit(quotient, shift - 1U) && (sticky || (significand & 1U));
        }
        if (roundUp) ++significand;
        int significandExponent = 0;
        std::frexp(static_cast<double>(significand), &significandExponent);
        const int outputExponent = static_cast<int>(shift) + BaseExponent;
        if (significand != 0U &&
            significandExponent + outputExponent > std::numeric_limits<Scalar>::max_exponent)
            return false;
        const Scalar magnitude = std::ldexp(static_cast<Scalar>(significand), outputExponent);
        if (!std::isfinite(magnitude)) return false;
        output = negative ? -magnitude : magnitude;
        return true;
    }
};

// Every binary64 is a multiple of 2^-1074. 68 words cover its 2098 input bits
// plus the count/carry bits for the shared 10-million-point admission.
using PointMeanAccumulator = BinaryAccumulator<68U, -1074>;

} // namespace detail
} // namespace numeric
} // namespace dommy
