#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace numeric {

// A later small/zero factor must not turn a representable product into an
// intermediate infinity. The bounded factor count keeps the exponent in int.
template <std::size_t Count>
bool scaledProduct(const std::array<double, Count>& factors, double& output) {
    static_assert(Count > 0U && Count <= 16U, "bounded scaled product required");
    bool zero = false;
    for (double factor : factors) {
        if (!std::isfinite(factor))
            return false;
        zero = zero || factor == 0.0;
    }
    if (zero) {
        output = 0.0;
        return true;
    }
    double fast = factors[0];
    bool rescale = false;
    for (std::size_t i = 1U; i < Count; ++i) {
        int leftExponent = 0, rightExponent = 0;
        std::frexp(fast, &leftExponent);
        std::frexp(factors[i], &rightExponent);
        const int productExponent = leftExponent + rightExponent;
        // Two normalized fractions multiply into [0.25,1). Admit only the
        // exponent band guaranteed to remain finite and normal, before doing
        // any risky multiplication. Boundary/subnormal cases use scaling.
        if (productExponent > std::numeric_limits<double>::max_exponent ||
            productExponent < std::numeric_limits<double>::min_exponent + 1) {
            rescale = true;
            break;
        }
        fast *= factors[i];
    }
    if (!rescale) {
        output = fast;
        return true;
    }
    double mantissa = 1.0;
    int exponent = 0;
    for (double factor : factors) {
        int factorExponent = 0;
        mantissa *= std::frexp(factor, &factorExponent);
        exponent += factorExponent;
    }
    int mantissaExponent = 0;
    mantissa = std::frexp(mantissa, &mantissaExponent);
    exponent += mantissaExponent;
    if (exponent > std::numeric_limits<double>::max_exponent) return false;
    const double candidate = exponent < -1074 ? std::copysign(0.0, mantissa)
                                            : std::scalbn(mantissa, exponent);
    if (!std::isfinite(candidate))
        return false;
    output = candidate;
    return true;
}

} // namespace numeric
} // namespace dommy
