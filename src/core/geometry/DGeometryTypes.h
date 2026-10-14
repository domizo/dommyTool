#pragma once

#include "math/DBounds.h"
#include "common/utility/DWorkBudget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace geometry {

struct Point2 {
    Point2() : x(0.0), y(0.0) {}
    Point2(double xValue, double yValue) : x(xValue), y(yValue) {}

    double x;
    double y;
};

struct Point3 {
    Point3() : x(0.0), y(0.0), z(0.0) {}
    Point3(double xValue, double yValue, double zValue)
        : x(xValue), y(yValue), z(zValue) {}

    double& operator[](std::size_t axis)
    {
        return axis == 0U ? x : (axis == 1U ? y : z);
    }

    double operator[](std::size_t axis) const
    {
        return axis == 0U ? x : (axis == 1U ? y : z);
    }

    double x;
    double y;
    double z;
};

struct Triangle {
    Triangle() : first(0U), second(0U), third(0U) {}
    Triangle(std::size_t firstIndex,
             std::size_t secondIndex,
             std::size_t thirdIndex)
        : first(firstIndex), second(secondIndex), third(thirdIndex) {}

    std::size_t& operator[](std::size_t corner)
    {
        return corner == 0U ? first : (corner == 1U ? second : third);
    }

    std::size_t operator[](std::size_t corner) const
    {
        return corner == 0U ? first : (corner == 1U ? second : third);
    }

    std::size_t first;
    std::size_t second;
    std::size_t third;
};

struct Segment3 {
    Segment3() {}
    Segment3(const Point3& firstPoint, const Point3& secondPoint)
        : first(firstPoint), second(secondPoint) {}

    Point3 first;
    Point3 second;
};

typedef std::vector<Point3> Polyline3;

inline bool isFinite(const Point2& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline bool isFinite(const Point3& point)
{
    return std::isfinite(point.x) &&
           std::isfinite(point.y) &&
           std::isfinite(point.z);
}

inline Point2 operator+(const Point2& lhs, const Point2& rhs)
{
    return Point2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline Point2 operator-(const Point2& lhs, const Point2& rhs)
{
    return Point2(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline Point2 operator*(const Point2& point, double scalar)
{
    return Point2(point.x * scalar, point.y * scalar);
}

inline Point2 operator/(const Point2& point, double scalar)
{
    return Point2(point.x / scalar, point.y / scalar);
}

inline Point3 operator+(const Point3& lhs, const Point3& rhs)
{
    return Point3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

inline Point3 operator-(const Point3& lhs, const Point3& rhs)
{
    return Point3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

inline Point3 operator*(const Point3& point, double scalar)
{
    return Point3(point.x * scalar, point.y * scalar, point.z * scalar);
}

inline Point3 operator/(const Point3& point, double scalar)
{
    return Point3(point.x / scalar, point.y / scalar, point.z / scalar);
}

inline Point3& operator+=(Point3& lhs, const Point3& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

inline Point3& operator-=(Point3& lhs, const Point3& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    return lhs;
}

inline Point3& operator*=(Point3& point, double scalar)
{
    point.x *= scalar;
    point.y *= scalar;
    point.z *= scalar;
    return point;
}

inline double dot(const Point3& lhs, const Point3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Point3 cross(const Point3& lhs, const Point3& rhs)
{
    return Point3(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

inline double squaredLength(const Point2& value)
{
    return value.x * value.x + value.y * value.y;
}

inline double squaredLength(const Point3& value)
{
    return dot(value, value);
}

inline double length(const Point3& value)
{
    return std::sqrt(std::max(0.0, squaredLength(value)));
}

inline double squaredDistance(const Point2& lhs, const Point2& rhs)
{
    return squaredLength(lhs - rhs);
}

inline double squaredDistance(const Point3& lhs, const Point3& rhs)
{
    return squaredLength(lhs - rhs);
}

inline bool normalized(const Point3& input,
                       Point3& output,
                       double minimumLength = 1.0e-15)
{
    if (!isFinite(input) || !std::isfinite(minimumLength) ||
        minimumLength <= 0.0) {
        return false;
    }

    const double magnitude = length(input);
    if (!std::isfinite(magnitude) || magnitude <= minimumLength) {
        return false;
    }

    output = input / magnitude;
    return isFinite(output);
}

inline double cross2(const Point2& first,
                     const Point2& second,
                     const Point2& third)
{
    const long double firstX =
        static_cast<long double>(second.x) - static_cast<long double>(first.x);
    const long double firstY =
        static_cast<long double>(second.y) - static_cast<long double>(first.y);
    const long double secondX =
        static_cast<long double>(third.x) - static_cast<long double>(first.x);
    const long double secondY =
        static_cast<long double>(third.y) - static_cast<long double>(first.y);
    const long double value = firstX * secondY - firstY * secondX;
    if (value > static_cast<long double>(
            std::numeric_limits<double>::max())) {
        return std::numeric_limits<double>::infinity();
    }
    if (value < -static_cast<long double>(
            std::numeric_limits<double>::max())) {
        return -std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(value);
}

inline double triangleAreaSquared(const Point3& first,
                                  const Point3& second,
                                  const Point3& third)
{
    return squaredLength(cross(second - first, third - first)) * 0.25;
}

// Shape dimensions, not distance from the coordinate origin. Reuse the common
// bounds reducer; reject a span that cannot be represented in double.
inline double boundsExtent(const math::Bounds3d& bounds)
{
    if (bounds.empty()) return std::numeric_limits<double>::quiet_NaN();
    double extent = 0.0;
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const double span = bounds.maximum(axis) - bounds.minimum(axis);
        if (!std::isfinite(span)) return std::numeric_limits<double>::quiet_NaN();
        extent = std::max(extent, span);
    }
    return extent;
}

inline double pointSetExtent(const std::vector<Point3>& points)
{
    math::Bounds3d bounds;
    for (const Point3& point : points) {
        if (!bounds.add(point.x, point.y, point.z))
            return std::numeric_limits<double>::quiet_NaN();
    }
    return boundsExtent(bounds);
}

// Validate the entire payload, but derive the geometric frame only from
// vertices used by faces. Finite unused points must not change the tolerance
// or welding/rebasing origin. Output arguments are unchanged on failure.
inline bool referencedTriangleFrame(const std::vector<Point3>& points,
                                    const std::vector<Triangle>& triangles,
                                    Point3& origin,
                                    double& extent,
                                    generation::WorkBudget& budget)
{
    if (points.empty() || triangles.empty() ||
        !budget.consume(points.size()) ||
        !budget.consumeProduct(triangles.size(), 3U) ||
        !budget.consume(3U)) return false;
    for (const Point3& point : points)
        if (!isFinite(point)) return false;
    math::Bounds3d bounds;
    std::size_t originIndex = std::numeric_limits<std::size_t>::max();
    for (const Triangle& triangle : triangles) {
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            if (triangle[corner] >= points.size()) return false;
            const Point3& point = points[triangle[corner]];
            if (!bounds.add(point.x, point.y, point.z)) return false;
            originIndex = std::min(originIndex, triangle[corner]);
        }
    }
    const double candidateExtent = boundsExtent(bounds);
    if (!std::isfinite(candidateExtent)) return false;
    origin = points[originIndex];
    extent = candidateExtent;
    return true;
}

inline double coordinateScale(const std::vector<Point2>& points)
{
    double scale = 1.0;
    for (std::vector<Point2>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        if (!isFinite(*point)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        scale = std::max(scale, std::fabs(point->x));
        scale = std::max(scale, std::fabs(point->y));
    }
    return scale;
}

inline double coordinateScale(const std::vector<Point3>& points)
{
    double scale = 1.0;
    for (std::vector<Point3>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        if (!isFinite(*point)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        scale = std::max(scale, std::fabs(point->x));
        scale = std::max(scale, std::fabs(point->y));
        scale = std::max(scale, std::fabs(point->z));
    }
    return scale;
}

inline double scaledTolerance(double scale, double requestedTolerance)
{
    if (!std::isfinite(scale) || scale < 0.0 ||
        !std::isfinite(requestedTolerance) || requestedTolerance <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::max(requestedTolerance, scale * 1.0e-12);
}

} // namespace geometry
} // namespace dommy
