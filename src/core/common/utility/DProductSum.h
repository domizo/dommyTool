#pragma once

#include "DPointMeanAccumulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace numeric {

// Short product chains, padded with unit factors. Unlike multiplying a chain
// first, the fallback admits finite cancellation between unrepresentable terms.
// Ordinary factors retain left-to-right product and sum rounding.
template <std::size_t Count>
bool checkedProductChainSum(const std::array<std::array<double, 4>, Count>& terms,
                            double& output)
{
    static_assert(Count > 0U && Count <= 16U, "bounded product-chain sum required");
    bool moderate = true;
    for (const auto& term : terms)
        for (double factor : term) {
            if (!std::isfinite(factor)) return false;
            moderate = moderate && detail::hasModerateProductMagnitude(factor);
        }
    if (moderate) {
        double value = 0.0;
        for (const auto& term : terms) {
            double product = term[0];
            for (unsigned int i = 1U; i < 4U; ++i) product *= term[i];
            value += product;
        }
        output = value;
        return true;
    }
    detail::BinaryAccumulator<268U, -4296> sum;
    for (const auto& term : terms)
        if (!sum.addProduct4(term[0], term[1], term[2], term[3])) return false;
    return sum.mean(1U, output);
}

// Bounded affine dot products. Ordinary factors retain a lightweight double
// path only when its rounding error cannot change the result; otherwise use
// exact binary products and
// a single final rounding. Rejected input leaves the previous result untouched.
template <std::size_t Count>
bool checkedProductSum(const std::array<double, Count>& left,
                       const std::array<double, Count>& right, double& output)
{
    static_assert(Count > 0U && Count <= 16U, "bounded product sum required");
    const double upper = std::ldexp(1.0, 500);
    const double lower = std::ldexp(1.0, -500);
    bool exact = false;
    for (std::size_t i = 0U; i < Count; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return false;
        if (left[i] == 0.0 || right[i] == 0.0) continue;
        const double a = std::fabs(left[i]), b = std::fabs(right[i]);
        exact = exact || a < lower || a > upper || b < lower || b > upper;
    }
    if (!exact) {
        // Products are in [2^-1000, 2^1000]; sixteen terms cannot overflow.
        double value = 0.0, errorBound = 0.0;
        for (std::size_t i = 0U; i < Count; ++i) {
            const double product = left[i] * right[i];
            // Keep the error-free FMA/TwoSum residuals in the normal band.
            // Smaller products are handled by the original-input binary grid.
            if (product != 0.0 && std::fabs(product) < std::ldexp(1.0, -900)) {
                exact = true;
                break;
            }
            const double productError = std::fma(left[i], right[i], -product);
            const double sum = value + product;
            const double sumError = detail::additionResidual(value, product, sum);
            // Round positive error-bound additions outward, never downward.
            for (double error : {productError, sumError}) {
                if (error != 0.0)
                    errorBound = std::nextafter(errorBound + std::fabs(error),
                                               std::numeric_limits<double>::infinity());
            }
            value = sum;
        }
        if (!exact && errorBound != 0.0) {
            const double magnitude = std::fabs(value);
            const double below = magnitude - std::nextafter(magnitude, 0.0);
            const double above = std::nextafter(magnitude,
                std::numeric_limits<double>::infinity()) - magnitude;
            // Strictly inside both rounding midpoints is sufficient. Ties and
            // ambiguous cancellation use exact products with ties-to-even.
            exact = errorBound >= 0.5 * std::min(below, above);
        }
        if (!exact) {
            output = value;
            return true;
        }
    }
    // Products are multiples of 2^-2148. 136 words cover all 4196 product bits
    // and the carry from at most sixteen terms, even before cancellation.
    detail::BinaryAccumulator<136U, -2148> sum;
    for (std::size_t i = 0U; i < Count; ++i)
        if (!sum.addProduct(left[i], right[i])) return false;
    return sum.mean(1U, output);
}

} // namespace numeric
} // namespace dommy
