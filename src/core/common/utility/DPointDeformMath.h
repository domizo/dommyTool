#pragma once

#include "common/utility/DDirectionMath.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DScaledProduct.h"
#include "modules/point/DPoint/math/DVectorMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace dommy {
namespace point {

// Keep the product's exponent separate so a later small/zero weight cannot
// turn a representable displacement into an intermediate infinity or NaN.
inline bool scaledDisplacement(double component, double magnitude,
                               double localWeight, double weight, double& output)
{
    return numeric::scaledProduct(
        std::array<double, 4>{{component, magnitude, localWeight, weight}}, output);
}

// Clamp without squaring large components or first forming an overflowing
// length. Zero vectors are valid; negative and non-finite limits are not.
template <typename Vector>
bool clampVectorMagnitude(const Vector& input, double maximum,
                          std::array<double, 3>& output)
{
    if (!std::isfinite(maximum) || maximum < 0.0) return false;
    std::array<double, 3> candidate{{double(input[0]), double(input[1]), double(input[2])}};
    double scale = 0.0;
    for (double component : candidate) {
        if (!std::isfinite(component)) return false;
        scale = std::max(scale, std::fabs(component));
    }
    if (maximum == 0.0) candidate = {{0.0, 0.0, 0.0}};
    else if (scale != 0.0) {
        const double relativeLength = std::hypot(std::hypot(candidate[0] / scale,
            candidate[1] / scale), candidate[2] / scale);
        if (scale > maximum / relativeLength) {
            double x = 0.0, y = 0.0, z = 0.0;
            if (!math::normalized3OrZero(candidate[0], candidate[1], candidate[2], x, y, z))
                return false;
            candidate = {{x * maximum, y * maximum, z * maximum}};
        }
    }
    output = candidate;
    return true;
}

// All geometry families store float points. Keep them unchanged on failure,
// and defer narrowing until after the complete double-precision displacement.
template <typename Point, typename Vector>
bool offsetAlongVector(const Point& source, const Vector& vector,
                       double magnitude, double localWeight, double weight,
                       Point& output)
{
    Point candidate(source);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        double displacement = 0.0;
        if (!std::isfinite(double(source[axis])) ||
            !scaledDisplacement(double(vector[axis]), magnitude, localWeight, weight, displacement) ||
            !numeric::checkedNumericCast(double(source[axis]) + displacement, candidate[axis]))
            return false;
    }
    output = candidate;
    return true;
}

inline bool radialFalloffWeight(double distance,
                                double radius,
                                double& weight)
{
    if (!std::isfinite(distance) || distance < 0.0 ||
        !std::isfinite(radius) || radius <= 0.0) {
        return false;
    }

    const double normalizedDistance =
        std::min(1.0, distance / radius);
    const double attenuation =
        1.0 - normalizedDistance * normalizedDistance;
    const double candidate = attenuation * attenuation;
    if (!std::isfinite(candidate)) return false;

    weight = candidate;
    return true;
}

inline bool canNormalizeWeight(double totalWeight)
{
    return std::isfinite(totalWeight) &&
           totalWeight > 1.0e-12;
}

template <typename Point>
inline bool moveToward(const Point& source,
                       const Point& target,
                       double weight,
                       Point& result)
{
    if (!std::isfinite(weight)) return false;

    direction::Delta delta;
    if (!direction::difference(source, target, delta)) return false;

    float components[3] = {0.0f, 0.0f, 0.0f};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        // Keep validated endpoints exact without changing ordinary or
        // extrapolated displacement rounding. Neither endpoint bypasses the
        // finite source/target checks above or checked output conversion.
        const double candidate =
            weight == 0.0 ? static_cast<double>(source[axis]) :
            weight == 1.0 ? static_cast<double>(target[axis]) :
            static_cast<double>(source[axis]) + delta.values[axis] * weight;
        if (!numeric::checkedFloatCast(candidate, components[axis])) {
            return false;
        }
    }

    result = Point(components[0], components[1], components[2]);
    return true;
}

} // namespace point
} // namespace dommy
