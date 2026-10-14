#pragma once

#include "common/utility/DNumericValidation.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DAttributeValueConversion.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dommy {
namespace arnold {

// These value adapters are used inside guarded Maya/renderer callbacks. Reject
// invalid staging before handing ownership or numeric values to Arnold; do not
// silently clamp HDR colors, negative attributes or other historical values.
inline float checkedFloat(double value)
{
    float result = 0.0f;
    if (!numeric::checkedFloatCast(value, result)) {
        throw std::invalid_argument(
            "Arnold export value is not representable as a finite float.");
    }
    return result;
}

inline double checkedScaleProduct(double first, double second)
{
    double result = 0.0;
    if (!numeric::scaledProduct(std::array<double, 2>{{first, second}}, result)) {
        throw std::invalid_argument("Arnold light scale product overflows.");
    }
    return result;
}

inline float checkedFloatProduct(double first, double second)
{
    return checkedFloat(checkedScaleProduct(first, second));
}

inline float checkedTextureReciprocal(double repeat)
{
    if (!std::isfinite(repeat)) {
        throw std::invalid_argument("Arnold texture repeat is not finite.");
    }
    if (repeat == 0.0) return 0.0f; // Historical zero-repeat behavior.
    const double smallestRepeat =
        1.0 / static_cast<double>(std::numeric_limits<float>::max());
    if (std::fabs(repeat) < smallestRepeat) {
        throw std::invalid_argument(
            "Arnold texture repeat reciprocal exceeds the float range.");
    }
    return checkedFloat(1.0 / repeat);
}

template <typename Matrix>
void checkedFloatMatrix(const Matrix& matrix, float (&result)[4][4])
{
    float candidate[4][4];
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            candidate[row][column] = checkedFloat(matrix[row][column]);
        }
    }
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            result[row][column] = candidate[row][column];
        }
    }
}

template <typename Array>
std::vector<float> checkedFloatValues(const Array& source)
{
    std::vector<float> result;
    if (!numeric::checkedScalarArrayCopy(
            source, source.size(), source.size(), result)) {
        throw std::invalid_argument(
            "Arnold export array contains a value outside the float range.");
    }
    return result;
}

template <typename Vector, typename Array>
std::vector<Vector> checkedFloatVectors(const Array& source)
{
    std::vector<Vector> result(source.size());
    for (std::size_t index = 0U; index < source.size(); ++index) {
        if (!numeric::checkedValueCast(source[index], result[index])) {
            throw std::invalid_argument(
                "Arnold export vector array contains a value outside the float range.");
        }
    }
    return result;
}

} // namespace arnold
} // namespace dommy
