#pragma once

#include "DIntegerBlend.h"
#include "DNumericValidation.h"
#include "math/DLinearInterpolation.h"
#include "DScaledProduct.h"
#include <cmath>
#include <limits>
#include <type_traits>

namespace dommy {
namespace attribute {

template <typename Value>
typename std::enable_if<std::is_floating_point<Value>::value, bool>::type
checkedLinearBlend(Value source,
                   Value target,
                   double weight,
                   Value& result)
{
    return math::checkedLinearInterpolation(source, target, weight, result);
}

template <typename Value>
typename std::enable_if<std::is_integral<Value>::value, bool>::type
checkedLinearBlend(Value source,
                   Value target,
                   double weight,
                   Value& result)
{
    return checkedIntegerBlend(source, target, weight, result);
}

// Keep an absent mask equivalent to 1.0; do not clamp extrapolation weights.
template <typename Value>
typename std::enable_if<std::is_floating_point<Value>::value, bool>::type
checkedMaskedLinearBlend(Value source,
                              Value target,
                              double weight,
                              double mask,
                              Value& result)
{
    return math::checkedMaskedLinearInterpolation(source, target, weight, mask, result);
}

template <typename Value>
typename std::enable_if<std::is_integral<Value>::value, bool>::type
checkedMaskedLinearBlend(Value source, Value target, double weight, double mask, Value& result)
{
    if (!std::isfinite(weight) || !std::isfinite(mask)) return false;
    if (weight == 0.0 || mask == 0.0 || source == target) { result = source; return true; }
    double effectiveWeight = 0.0;
    if (!numeric::scaledProduct(std::array<double, 2>{{weight, mask}}, effectiveWeight)) return false;
    return checkedIntegerBlend(source, target, effectiveWeight, result);
}

} // namespace attribute
} // namespace dommy
