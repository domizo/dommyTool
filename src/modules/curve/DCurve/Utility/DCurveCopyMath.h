#pragma once

#include "common/utility/DGenerationLimits.h"
#include "math/DBounds.h"
#include "math/DRemapMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dommy {
namespace curve {

// Triangles use the highest-V vertex; quads use the highest-V edge midpoint.
// UVs may be negative or tiled. Keep the first index on ties and leave output
// unchanged on invalid input. Widen before averaging finite float UVs.
template <typename UvArray>
bool checkedCopyUvTopIndex(const UvArray& u, const UvArray& v, unsigned int& output) {
    const unsigned int count = u.length();
    if ((count != 3U && count != 4U) || v.length() != count)
        return false;
    for (unsigned int i = 0U; i < count; ++i)
        if (!std::isfinite(double(u[i])) || !std::isfinite(double(v[i])))
            return false;
    const auto height = [&](unsigned int i) {
        return count == 3U ? double(v[i]) : double(v[i]) * 0.5 + double(v[(i + 1U) % count]) * 0.5;
    };
    unsigned int top = 0U;
    double maximum = height(0U);
    for (unsigned int i = 1U; i < count; ++i) {
        const double value = height(i);
        if (value > maximum) {
            maximum = value;
            top = i;
        }
    }
    output = top;
    return true;
}

inline bool validCopyParameterDomain(double start, double end) {
    return std::isfinite(start) && std::isfinite(end) && end > start && std::isfinite(end - start);
}

// Padding is a fraction of one cell; random padding remains in parameter units.
// All paths (curve, U-first surface, V-first surface) use the same clamping rule.
inline bool checkedCopyParameter(double start, double end, std::size_t count, std::size_t index,
                                 double padding, double randomPadding, double coordinate,
                                 double& result) {
    if (!validCopyParameterDomain(start, end) || count == 0U || index >= count ||
        count > generation::kMaxGeneratedCurveCount || !std::isfinite(padding) ||
        !std::isfinite(randomPadding) || !std::isfinite(coordinate) || coordinate < 0.0 ||
        coordinate > 1.0) {
        return false;
    }
    const double step = (end - start) / static_cast<double>(count);
    const double pad = step * padding + randomPadding;
    const double value =
        start + step * static_cast<double>(index) + pad * 0.5 + (step - pad) * coordinate;
    if (!std::isfinite(pad) || !std::isfinite(value))
        return false;
    result = std::max(start, std::min(end, value));
    return true;
}

// Keep bounds and subtraction in double: two finite float endpoints can have
// a span larger than FLT_MAX. Zero-width axes retain the historical zero value.
template <typename Points>
bool normalizedCopyCoordinates(const Points& points, std::vector<std::array<double, 2>>& result) {
    if (points.length() == 0U || points.length() > generation::kMaxGeneratedPointCount)
        return false;
    math::Bounds3d bounds;
    for (unsigned int i = 0U; i < points.length(); ++i) {
        if (!bounds.add(points[i].x, points[i].y, points[i].z))
            return false;
    }
    std::vector<std::array<double, 2>> candidate(points.length());
    for (unsigned int i = 0U; i < points.length(); ++i) {
        for (unsigned int uv = 0U; uv < 2U; ++uv) {
            const unsigned int axis = uv == 0U ? 0U : 2U;
            const double low = bounds.minimum(axis), high = bounds.maximum(axis);
            double value = 0.0;
            if (low != high &&
                !math::normalizeRange(uv == 0U ? points[i].x : points[i].z, low, high, value))
                return false;
            candidate[i][uv] = value;
        }
    }
    result.swap(candidate);
    return true;
}

} // namespace curve
} // namespace dommy
