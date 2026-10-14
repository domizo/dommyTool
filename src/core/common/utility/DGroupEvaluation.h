#pragma once

#include "DGroupSelection.h"
#include "DIndexPattern.h"
#include "DCurveEvaluation.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include <maya/MStatus.h>

namespace dommy {
namespace group {

// Every condition stages its additions. A failed late query must not turn
// an incomplete mask into a successful result.
template <typename Predicate>
bool evaluateSelection(std::size_t count, std::vector<bool>& output, Predicate predicate) {
    if (output.size() != count || count > generation::kMaxGeneratedPointCount)
        return false;
    std::vector<bool> candidate(output);
    for (std::size_t i = 0U; i < count; ++i) {
        bool selected = false;
        if (!predicate(i, selected))
            return false;
        candidate[i] = candidate[i] || selected;
    }
    output.swap(candidate);
    return true;
}

inline bool selectPattern(const std::string& pattern, std::size_t count,
                          std::vector<bool>& output) {
    if (output.size() != count || count > generation::kMaxGeneratedPointCount ||
        pattern.size() > generation::kMaxGeneratedStringByteCount)
        return false;
    bool reverse = false;
    std::vector<std::size_t> indices;
    return component::parseIndexPattern(pattern, count, generation::kMaxGeneratedPointCount,
                                        reverse, indices) &&
           component::mergeIndexPatternSelection(indices, reverse, output);
}

template <typename Vector> bool finiteTriple(const Vector& value) {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

template <typename Vector, typename Direction>
bool normalSelected(const Vector& value, const Direction& direction, double spreadRadians,
                    bool& selected) {
    double x, y, z, nx, ny, nz;
    if (!finiteTriple(value) || !finiteTriple(direction) || !std::isfinite(spreadRadians) ||
        !math::normalized3OrZero(double(direction[0]), double(direction[1]), double(direction[2]),
                                 nx, ny, nz) ||
        (nx == 0.0 && ny == 0.0 && nz == 0.0))
        return false;
    selected = false;
    if (value[0] == 0.0 && value[1] == 0.0 && value[2] == 0.0)
        return true;
    if (!math::normalized3OrZero(double(value[0]), double(value[1]), double(value[2]), x, y, z))
        return false;
    if (x != 0.0 || y != 0.0 || z != 0.0) {
        const double dot = std::max(-1.0, std::min(1.0, x * nx + y * ny + z * nz));
        selected = std::acos(dot) < spreadRadians;
    }
    return true;
}

// CVs need not lie on the curve. Find their closest curve parameter, then
// evaluate the requested direction only after that query succeeds. The same
// adapter is exercised with a failure-injectable curve in host tests.
template <typename Curve, typename Point, typename Vector, typename Space>
bool curveDirectionAtClosestPoint(Curve& curve, const Point& point, bool normal, double tolerance,
                                  Space space, Vector& result) {
    return dommy::curve::directionAtClosestPoint(curve, point, normal, tolerance, space, result)
        == MS::kSuccess;
}

} // namespace group
} // namespace dommy
