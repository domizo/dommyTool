#pragma once

#include <array>
#include <cmath>

namespace dommy {
namespace geometry {

using FieldIconPoint = std::array<double, 3>;

// Three great circles and axis arrows: a bounded, project-owned field symbol.
// Every callback receives one independent line; no allocation or GL state is used.
template <typename LineVisitor>
inline void visitFieldIconLines(const LineVisitor& line)
{
    const unsigned int segments = 24U;
    const double radius = 1.25;
    const double step = 2.0 * std::acos(-1.0) / segments;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        const unsigned int first = (axis + 1U) % 3U;
        const unsigned int second = (axis + 2U) % 3U;
        for (unsigned int segment = 0U; segment < segments; ++segment) {
            const double angle = step * segment;
            FieldIconPoint a = {{0.0, 0.0, 0.0}}, b = a;
            a[first] = radius * std::cos(angle);
            a[second] = radius * std::sin(angle);
            b[first] = radius * std::cos(angle + step);
            b[second] = radius * std::sin(angle + step);
            line(a, b);
        }
        FieldIconPoint tail = {{0.0, 0.0, 0.0}}, tip = tail;
        tail[axis] = -radius;
        tip[axis] = radius;
        line(tail, tip);
        for (int side : {-1, 1}) {
            FieldIconPoint wing = tip;
            wing[axis] -= 0.3;
            wing[first] = 0.2 * side;
            line(tip, wing);
        }
    }
}

} // namespace geometry
} // namespace dommy
