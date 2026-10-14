#pragma once

#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DPointMeanAccumulator.h"

#include <array>
#include <cmath>

namespace dommy {
namespace particle {

// Mesh samples use Cartesian coordinates. Stage the complete point so failure
// cannot expose a partially updated sample. Point's default w remains one.
template <typename Point>
bool blendMeshSample(const Point& first, const Point& second,
                     double weight, Point& output)
{
    Point candidate;
    if (!attribute::checkedLinearBlend(first.x, second.x, weight, candidate.x) ||
        !attribute::checkedLinearBlend(first.y, second.y, weight, candidate.y) ||
        !attribute::checkedLinearBlend(first.z, second.z, weight, candidate.z)) return false;
    output = candidate;
    return true;
}

inline bool quadSampleCoordinate(double origin, double first, double third,
                                 double u, double v, double& output)
{
    if (!std::isfinite(origin) || !std::isfinite(first) || !std::isfinite(third) ||
        !std::isfinite(u) || !std::isfinite(v) || u < 0.0 || u > 1.0 ||
        v < 0.0 || v > 1.0) return false;

    // Preserve ordinary arithmetic and the archival parallelogram rule, not
    // bilinear interpolation through all four vertices.
    const double candidate = origin + (first - origin) * u + (third - origin) * v;
    if (std::isfinite(candidate)) { output = candidate; return true; }

    // Expanded products stay finite because each weight is in [0,1]. Reuse
    // the exact accumulator to avoid overflowing their sum before cancellation.
    // fma retains each product's representable rounding residual. No wider
    // long double or new arbitrary-precision implementation is required.
    numeric::detail::PointMeanAccumulator sum;
    if (!sum.add(origin)) return false;
    const std::array<double, 4> values = {{first, origin, third, origin}};
    const std::array<double, 4> weights = {{u, -u, v, -v}};
    for (unsigned int i = 0U; i < values.size(); ++i) {
        const double product = values[i] * weights[i];
        if (!sum.add(product) ||
            !sum.add(std::fma(values[i], weights[i], -product))) return false;
    }
    return sum.mean(1U, output);
}

template <typename Point>
bool quadMeshSample(const Point& origin, const Point& first, const Point& third,
                    double u, double v, Point& output)
{
    Point candidate;
    if (!quadSampleCoordinate(origin.x, first.x, third.x, u, v, candidate.x) ||
        !quadSampleCoordinate(origin.y, first.y, third.y, u, v, candidate.y) ||
        !quadSampleCoordinate(origin.z, first.z, third.z, u, v, candidate.z)) return false;
    output = candidate;
    return true;
}

} // namespace particle
} // namespace dommy
