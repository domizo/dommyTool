#pragma once

#include "geometry/DGeometryTypes.h"
#include "geometry/DTriangleComponents.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DPointMean.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

// Keep the polygon-facing name while sharing the same sticky budget with
// other host-independent algorithms; callers retain the polygon-facing alias.
typedef generation::WorkBudget PolygonWorkBudget;

struct PolygonTriangulation {
    std::vector<Point2> points;
    std::vector<std::size_t> sourceIndices;
    std::vector<Triangle> triangles;

    void clear()
    {
        points.clear();
        sourceIndices.clear();
        triangles.clear();
    }

    void swap(PolygonTriangulation& other)
    {
        points.swap(other.points);
        sourceIndices.swap(other.sourceIndices);
        triangles.swap(other.triangles);
    }
};

struct PlaneFrame3 {
    Point3 origin;
    Point3 tangent;
    Point3 bitangent;
    Point3 normal;
};

inline bool projectPlanarPolygon(
    const std::vector<Point3>& points,
    PlaneFrame3& frame,
    std::vector<Point2>& projectedPoints,
    PolygonWorkBudget& budget,
    std::size_t extraStorageBytes = 0U)
{
    if (points.size() < 3U ||
        !validTriangleWorkspaceStorage(points.size(), points.size() - 2U, extraStorageBytes) ||
        !budget.consumeProduct(points.size(), 6U)) return false;
    PlaneFrame3 candidate;
    std::array<double, 3> origin;
    if (!numeric::checkedPointMean(points, origin)) return false;
    candidate.origin = Point3(origin[0], origin[1], origin[2]);
    Point3 minimum = points.front(), maximum = points.front();
    for (std::vector<Point3>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            minimum[axis] = std::min(minimum[axis], (*point)[axis]);
            maximum[axis] = std::max(maximum[axis], (*point)[axis]);
        }
    }
    const Point3 extent = maximum - minimum;
    const double scale = std::max(extent.x, std::max(extent.y, extent.z));
    if (!std::isfinite(scale) || scale <= 0.0) return false;

    // Newell's direction is unchanged by a uniform scale. Work on local,
    // unit-sized coordinates so an area is not tested as an absolute length.
    Point3 normal;
    for (std::size_t corner = 0U; corner < points.size(); ++corner) {
        const Point3 current = (points[corner] - candidate.origin) / scale;
        const Point3 next = (points[(corner + 1U) % points.size()] - candidate.origin) / scale;
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    if (!normalized(normal, candidate.normal)) return false;

    // The face consumers are planar algorithms, not surface flatteners.
    // Use a local, extent-relative residual; 1e-6 accommodates float-backed
    // input rounding without letting absolute position enlarge the tolerance.
    const double maximumRelativePlaneDistance = 1.0e-6;
    for (const Point3& point : points) {
        const double residual = dot((point - candidate.origin) / scale, candidate.normal);
        if (!std::isfinite(residual) ||
            std::fabs(residual) > maximumRelativePlaneDistance) return false;
    }

    const Point3 reference =
        std::fabs(candidate.normal.x) < 0.9
            ? Point3(1.0, 0.0, 0.0)
            : Point3(0.0, 1.0, 0.0);
    if (!normalized(
            cross(reference, candidate.normal), candidate.tangent)) {
        return false;
    }
    if (!normalized(
            cross(candidate.normal, candidate.tangent), candidate.bitangent)) {
        return false;
    }

    std::vector<Point2> projected;
    projected.reserve(points.size());
    for (std::vector<Point3>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        const Point3 offset = *point - candidate.origin;
        const Point2 value(dot(offset, candidate.tangent), dot(offset, candidate.bitangent));
        if (!isFinite(value)) return false;
        projected.push_back(value);
    }
    frame = candidate;
    projectedPoints.swap(projected);
    return true;
}

inline bool projectPlanarPolygon(
    const std::vector<Point3>& points,
    PlaneFrame3& frame,
    std::vector<Point2>& projectedPoints)
{
    PolygonWorkBudget budget;
    return projectPlanarPolygon(points, frame, projectedPoints, budget);
}

inline Point3 unprojectPoint(const PlaneFrame3& frame,
                             const Point2& point)
{
    return frame.origin +
           frame.tangent * point.x +
           frame.bitangent * point.y;
}

namespace detail {

struct PolygonTolerance {
    double distance;
    double area;
};

// requestedTolerance is a distance. Area predicates need distance*extent,
// not the same scalar, and translation must not enlarge either threshold.
inline bool polygonTolerance(const std::vector<Point2>& points,
                             double requestedTolerance, PolygonTolerance& result)
{
    if (points.empty() || !std::isfinite(requestedTolerance) || requestedTolerance <= 0.0)
        return false;
    Point2 minimum = points.front(), maximum = points.front();
    for (const Point2& point : points) {
        if (!isFinite(point)) return false;
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
    }
    const double extent = std::max(maximum.x - minimum.x, maximum.y - minimum.y);
    if (!std::isfinite(extent) || extent <= 0.0) return false;
    const double distance = std::max(requestedTolerance, extent * 1.0e-12);
    const double area = distance * extent;
    if (!std::isfinite(area) || area <= 0.0) return false;
    result = PolygonTolerance{distance, area};
    return true;
}

inline std::size_t polygonTreeWork(std::size_t entries)
{
    return generation::orderedContainerWork(entries);
}

inline bool samePoint(const Point2& lhs,
                      const Point2& rhs,
                      double squaredTolerance)
{
    return squaredDistance(lhs, rhs) <= squaredTolerance;
}

inline int orientation(const Point2& first,
                       const Point2& second,
                       const Point2& third,
                       double tolerance)
{
    const double value = cross2(first, second, third);
    if (!std::isfinite(value)) {
        return value > 0.0 ? 1 : -1;
    }
    if (value > tolerance) return 1;
    if (value < -tolerance) return -1;
    return 0;
}

inline bool pointOnSegment(const Point2& point,
                           const Point2& first,
                           const Point2& second,
                           const PolygonTolerance& tolerance)
{
    if (orientation(first, second, point, tolerance.area) != 0) return false;
    return point.x >= std::min(first.x, second.x) - tolerance.distance &&
           point.x <= std::max(first.x, second.x) + tolerance.distance &&
           point.y >= std::min(first.y, second.y) - tolerance.distance &&
           point.y <= std::max(first.y, second.y) + tolerance.distance;
}

inline bool segmentsIntersect(const Point2& firstA,
                              const Point2& secondA,
                              const Point2& firstB,
                              const Point2& secondB,
                              const PolygonTolerance& tolerance)
{
    const int firstOrientation =
        orientation(firstA, secondA, firstB, tolerance.area);
    const int secondOrientation =
        orientation(firstA, secondA, secondB, tolerance.area);
    const int thirdOrientation =
        orientation(firstB, secondB, firstA, tolerance.area);
    const int fourthOrientation =
        orientation(firstB, secondB, secondA, tolerance.area);

    if (firstOrientation != secondOrientation &&
        thirdOrientation != fourthOrientation) {
        return true;
    }
    return (firstOrientation == 0 &&
            pointOnSegment(firstB, firstA, secondA, tolerance)) ||
           (secondOrientation == 0 &&
            pointOnSegment(secondB, firstA, secondA, tolerance)) ||
           (thirdOrientation == 0 &&
            pointOnSegment(firstA, firstB, secondB, tolerance)) ||
           (fourthOrientation == 0 &&
            pointOnSegment(secondA, firstB, secondB, tolerance));
}

inline bool hasSelfIntersection(const std::vector<Point2>& points,
                                const PolygonTolerance& tolerance,
                                PolygonWorkBudget& budget)
{
    const std::size_t count = points.size();
    for (std::size_t first = 0U; first < count; ++first) {
        const std::size_t firstNext = (first + 1U) % count;
        for (std::size_t second = first + 1U; second < count; ++second) {
            if (!budget.consume(1U)) return true;
            const std::size_t secondNext = (second + 1U) % count;
            if (first == second || firstNext == second ||
                secondNext == first) {
                continue;
            }
            if (segmentsIntersect(
                    points[first], points[firstNext],
                    points[second], points[secondNext], tolerance)) {
                return true;
            }
        }
    }
    return false;
}

inline double signedAreaTwice(const std::vector<Point2>& points)
{
    long double area = 0.0L;
    const Point2& origin = points.front();
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Point2& first = points[index];
        const Point2& second = points[(index + 1U) % points.size()];
        const long double firstX = static_cast<long double>(first.x) - origin.x;
        const long double firstY = static_cast<long double>(first.y) - origin.y;
        const long double secondX = static_cast<long double>(second.x) - origin.x;
        const long double secondY = static_cast<long double>(second.y) - origin.y;
        area += firstX * secondY - secondX * firstY;
    }
    if (area > static_cast<long double>(
            std::numeric_limits<double>::max())) {
        return std::numeric_limits<double>::infinity();
    }
    if (area < -static_cast<long double>(
            std::numeric_limits<double>::max())) {
        return -std::numeric_limits<double>::infinity();
    }
    return static_cast<double>(area);
}

inline bool pointInTriangle(const Point2& point,
                            const Point2& first,
                            const Point2& second,
                            const Point2& third,
                            double tolerance)
{
    const double firstSide = cross2(first, second, point);
    const double secondSide = cross2(second, third, point);
    const double thirdSide = cross2(third, first, point);
    return firstSide >= -tolerance &&
           secondSide >= -tolerance &&
           thirdSide >= -tolerance;
}

inline bool cleanPolygon(const std::vector<Point2>& input,
                         const PolygonTolerance& tolerance,
                         std::vector<Point2>& points,
                         std::vector<std::size_t>& sourceIndices,
                         PolygonWorkBudget& budget)
{
    if (!budget.consumeProduct(input.size(), 4U)) return false;
    points.clear();
    sourceIndices.clear();
    const double squaredTolerance = tolerance.distance * tolerance.distance;
    points.reserve(input.size());
    sourceIndices.reserve(input.size());

    for (std::size_t index = 0U; index < input.size(); ++index) {
        if (!isFinite(input[index])) {
            points.clear();
            sourceIndices.clear();
            return false;
        }
        if (!points.empty() &&
            samePoint(points.back(), input[index], squaredTolerance)) {
            continue;
        }
        points.push_back(input[index]);
        sourceIndices.push_back(index);
    }
    if (points.size() > 1U &&
        samePoint(points.front(), points.back(), squaredTolerance)) {
        points.pop_back();
        sourceIndices.pop_back();
    }

    if (points.size() <= 3U) return true;
    const std::size_t count = points.size();
    // Circular links permit O(1) removal. Only the two changed neighbors need
    // another check, so there are at most 3*count queue visits and one compaction.
    if (!budget.consumeProduct(count, 4U)) return false;
    std::vector<std::size_t> previous(count), next(count);
    std::deque<std::size_t> pending;
    for (std::size_t index = 0U; index < count; ++index) {
        previous[index] = (index + count - 1U) % count;
        next[index] = (index + 1U) % count;
        pending.push_back(index);
    }
    std::size_t remaining = count;
    while (!pending.empty() && remaining > 3U) {
        if (!budget.consume(1U)) return false;
        const std::size_t index = pending.front();
        pending.pop_front();
        if (next[index] == index) continue;
        const std::size_t before = previous[index], after = next[index];
        if (std::fabs(cross2(points[before], points[index], points[after])) <= tolerance.area &&
            pointOnSegment(points[index], points[before], points[after], tolerance)) {
            if (!budget.consume(2U)) return false;
            next[before] = after;
            previous[after] = before;
            next[index] = index;
            --remaining;
            pending.push_back(before);
            pending.push_back(after);
        }
    }
    std::size_t used = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        if (next[index] == index) continue;
        points[used] = points[index];
        sourceIndices[used++] = sourceIndices[index];
    }
    points.resize(used);
    sourceIndices.resize(used);
    return true;
}

inline bool polygonIsConvex(const std::vector<std::size_t>& polygon,
                            const std::vector<Point2>& points,
                            double tolerance,
                            PolygonWorkBudget& budget)
{
    if (polygon.size() < 3U || !budget.consume(polygon.size())) return false;
    for (std::size_t index = 0U; index < polygon.size(); ++index) {
        const Point2& first = points[polygon[index]];
        const Point2& second =
            points[polygon[(index + 1U) % polygon.size()]];
        const Point2& third =
            points[polygon[(index + 2U) % polygon.size()]];
        if (cross2(first, second, third) < -tolerance) {
            return false;
        }
    }
    return true;
}

inline bool tryMergePolygons(const std::vector<std::size_t>& first,
                             const std::vector<std::size_t>& second,
                             const std::vector<Point2>& points,
                             double tolerance,
                             std::vector<std::size_t>& merged,
                             PolygonWorkBudget& budget)
{
    for (std::size_t firstEdge = 0U;
         firstEdge < first.size(); ++firstEdge) {
        const std::size_t firstStart = first[firstEdge];
        const std::size_t firstEnd =
            first[(firstEdge + 1U) % first.size()];
        for (std::size_t secondEdge = 0U;
             secondEdge < second.size(); ++secondEdge) {
            if (!budget.consume(1U)) return false;
            if (second[secondEdge] != firstEnd ||
                second[(secondEdge + 1U) % second.size()] != firstStart) {
                continue;
            }

            std::size_t count = 0U;
            if (!memory::checkedAdd(first.size(), second.size(), count) ||
                !budget.consumeProduct(count, 2U)) return false;
            std::vector<std::size_t> candidate;
            candidate.reserve(count - 2U);
            std::size_t index = (firstEdge + 1U) % first.size();
            do {
                candidate.push_back(first[index]);
                index = (index + 1U) % first.size();
            } while (index != (firstEdge + 1U) % first.size());

            index = (secondEdge + 2U) % second.size();
            while (index != secondEdge) {
                candidate.push_back(second[index]);
                index = (index + 1U) % second.size();
            }

            std::vector<std::size_t> cleaned;
            cleaned.reserve(candidate.size());
            for (std::size_t candidateIndex = 0U;
                 candidateIndex < candidate.size(); ++candidateIndex) {
                if (cleaned.empty() ||
                    cleaned.back() != candidate[candidateIndex]) {
                    cleaned.push_back(candidate[candidateIndex]);
                }
            }
            if (cleaned.size() > 1U &&
                cleaned.front() == cleaned.back()) {
                cleaned.pop_back();
            }
            if (cleaned.size() >= 3U &&
                polygonIsConvex(cleaned, points, tolerance, budget)) {
                merged.swap(cleaned);
                return true;
            }
        }
    }
    return false;
}

struct EdgeKey {
    EdgeKey() : first(0U), second(0U) {}
    EdgeKey(std::size_t firstIndex, std::size_t secondIndex)
        : first(std::min(firstIndex, secondIndex)),
          second(std::max(firstIndex, secondIndex)) {}

    bool operator<(const EdgeKey& rhs) const
    {
        return first < rhs.first ||
               (first == rhs.first && second < rhs.second);
    }

    std::size_t first;
    std::size_t second;
};

inline std::size_t midpointIndex(
    std::size_t first,
    std::size_t second,
    std::vector<Point2>& points,
    std::map<EdgeKey, std::size_t>& midpoints,
    std::size_t maximumPointCount)
{
    const EdgeKey key(first, second);
    const std::map<EdgeKey, std::size_t>::const_iterator existing =
        midpoints.find(key);
    if (existing != midpoints.end()) return existing->second;
    if (points.size() >= maximumPointCount) {
        return std::numeric_limits<std::size_t>::max();
    }
    const Point2 midpoint = (points[first] + points[second]) * 0.5;
    if (!isFinite(midpoint)) {
        return std::numeric_limits<std::size_t>::max();
    }
    const std::size_t index = points.size();
    points.push_back(midpoint);
    midpoints.insert(std::make_pair(key, index));
    return index;
}

inline bool hasValidTriangulation(
    const PolygonTriangulation& triangulation,
    std::size_t maximumPointCount,
    std::size_t maximumTriangleCount,
    PolygonWorkBudget& budget)
{
    if (triangulation.points.size() < 3U ||
        triangulation.points.size() > maximumPointCount ||
        triangulation.triangles.empty() ||
        triangulation.triangles.size() > maximumTriangleCount) {
        return false;
    }
    if (!budget.consume(triangulation.points.size()) ||
        !budget.consumeProduct(triangulation.triangles.size(), 4U)) return false;
    for (std::vector<Point2>::const_iterator point =
             triangulation.points.begin();
         point != triangulation.points.end(); ++point) {
        if (!isFinite(*point)) return false;
    }
    for (std::vector<Triangle>::const_iterator triangle =
             triangulation.triangles.begin();
         triangle != triangulation.triangles.end(); ++triangle) {
        if (triangle->first >= triangulation.points.size() ||
            triangle->second >= triangulation.points.size() ||
            triangle->third >= triangulation.points.size() ||
            triangle->first == triangle->second ||
            triangle->second == triangle->third ||
            triangle->third == triangle->first ||
            !std::isfinite(cross2(
                triangulation.points[triangle->first],
                triangulation.points[triangle->second],
                triangulation.points[triangle->third]))) {
            return false;
        }
    }
    return true;
}

} // namespace detail

inline bool triangulateSimplePolygon(
    const std::vector<Point2>& input,
    PolygonTriangulation& output,
    std::size_t maximumPointCount,
    std::size_t maximumTriangleCount,
    PolygonWorkBudget& budget,
    double requestedTolerance = 1.0e-10,
    std::size_t extraStorageBytes = 0U)
{
    PolygonTriangulation candidate;
    if (input.size() < 3U || input.size() > maximumPointCount ||
        maximumTriangleCount == 0U ||
        !validTriangleWorkspaceStorage(input.size(), input.size() - 2U, extraStorageBytes) ||
        !budget.consume(input.size())) {
        return false;
    }

    detail::PolygonTolerance tolerance;
    if (!detail::polygonTolerance(input, requestedTolerance, tolerance)) return false;
    if (!detail::cleanPolygon(
        input, tolerance, candidate.points, candidate.sourceIndices, budget)) return false;
    if (candidate.points.size() < 3U ||
        candidate.points.size() > maximumPointCount) {
        return false;
    }
    if (detail::hasSelfIntersection(
            candidate.points, tolerance, budget)) {
        return false;
    }

    if (!budget.consumeProduct(candidate.points.size(), 3U)) return false;
    double area = detail::signedAreaTwice(candidate.points);
    if (!std::isfinite(area) || std::fabs(area) <= tolerance.area) {
        return false;
    }
    if (area < 0.0) {
        std::reverse(candidate.points.begin(), candidate.points.end());
        std::reverse(
            candidate.sourceIndices.begin(), candidate.sourceIndices.end());
    }

    std::vector<std::size_t> polygon(candidate.points.size());
    for (std::size_t index = 0U; index < polygon.size(); ++index) {
        polygon[index] = index;
    }

    while (polygon.size() > 3U) {
        bool foundEar = false;
        for (std::size_t index = 0U; index < polygon.size(); ++index) {
            if (!budget.consume(1U)) return false;
            const std::size_t previous =
                polygon[(index + polygon.size() - 1U) % polygon.size()];
            const std::size_t current = polygon[index];
            const std::size_t next =
                polygon[(index + 1U) % polygon.size()];
            if (cross2(
                    candidate.points[previous],
                    candidate.points[current],
                    candidate.points[next]) <= tolerance.area) {
                continue;
            }

            bool containsPoint = false;
            for (std::size_t test = 0U; test < polygon.size(); ++test) {
                if (!budget.consume(1U)) return false;
                const std::size_t testIndex = polygon[test];
                if (testIndex == previous || testIndex == current ||
                    testIndex == next) {
                    continue;
                }
                if (detail::pointInTriangle(
                        candidate.points[testIndex],
                        candidate.points[previous],
                        candidate.points[current],
                        candidate.points[next], tolerance.area)) {
                    containsPoint = true;
                    break;
                }
            }
            if (containsPoint) continue;
            if (candidate.triangles.size() >= maximumTriangleCount) {
                return false;
            }
            if (!budget.consume(polygon.size() - index)) return false;
            candidate.triangles.push_back(
                Triangle(previous, current, next));
            polygon.erase(
                polygon.begin() + static_cast<std::ptrdiff_t>(index));
            foundEar = true;
            break;
        }
        if (!foundEar) return false;
    }

    if (candidate.triangles.size() >= maximumTriangleCount) {
        return false;
    }
    if (!budget.consume(1U)) return false;
    candidate.triangles.push_back(
        Triangle(polygon[0U], polygon[1U], polygon[2U]));
    output.swap(candidate);
    return true;
}

inline bool triangulateSimplePolygon(
    const std::vector<Point2>& input, PolygonTriangulation& output,
    std::size_t maximumPointCount, std::size_t maximumTriangleCount,
    std::size_t maximumWork, double requestedTolerance = 1.0e-10)
{
    PolygonWorkBudget budget(maximumWork);
    return triangulateSimplePolygon(input, output, maximumPointCount,
                                    maximumTriangleCount, budget, requestedTolerance);
}

inline bool convexPartition(
    const PolygonTriangulation& triangulation,
    std::vector<std::vector<std::size_t> >& polygons,
    PolygonWorkBudget& budget,
    double requestedTolerance = 1.0e-10)
{
    if (!detail::hasValidTriangulation(
            triangulation,
            std::numeric_limits<std::size_t>::max(),
            std::numeric_limits<std::size_t>::max(), budget)) {
        return false;
    }
    if (!budget.consume(triangulation.points.size()) ||
        !budget.consumeProduct(triangulation.triangles.size(), 4U)) return false;
    detail::PolygonTolerance tolerance;
    if (!detail::polygonTolerance(triangulation.points, requestedTolerance, tolerance)) return false;

    std::vector<std::vector<std::size_t> > candidate;
    candidate.reserve(triangulation.triangles.size());
    for (std::vector<Triangle>::const_iterator triangle =
             triangulation.triangles.begin();
         triangle != triangulation.triangles.end(); ++triangle) {
        std::vector<std::size_t> polygon;
        polygon.push_back(triangle->first);
        polygon.push_back(triangle->second);
        polygon.push_back(triangle->third);
        candidate.push_back(polygon);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t first = 0U;
             first < candidate.size() && !changed; ++first) {
            for (std::size_t second = first + 1U;
                 second < candidate.size(); ++second) {
                if (!budget.consume(1U)) return false;
                std::vector<std::size_t> merged;
                if (!detail::tryMergePolygons(
                        candidate[first], candidate[second],
                        triangulation.points, tolerance.area, merged, budget)) {
                    if (budget.exceeded()) return false;
                    continue;
                }
                if (!budget.consume(candidate.size() - second)) return false;
                candidate[first].swap(merged);
                candidate.erase(
                    candidate.begin() +
                    static_cast<std::ptrdiff_t>(second));
                changed = true;
                break;
            }
        }
    }

    polygons.swap(candidate);
    return true;
}

inline bool convexPartition(
    const PolygonTriangulation& triangulation,
    std::vector<std::vector<std::size_t> >& polygons,
    std::size_t maximumWork, double requestedTolerance = 1.0e-10)
{
    PolygonWorkBudget budget(maximumWork);
    return convexPartition(triangulation, polygons, budget, requestedTolerance);
}

inline bool refineTriangulation(
    PolygonTriangulation& triangulation,
    double maximumEdgeLength,
    unsigned int maximumIterations,
    std::size_t maximumPointCount,
    std::size_t maximumTriangleCount,
    PolygonWorkBudget& budget,
    std::size_t extraStorageBytes = 0U)
{
    if (!std::isfinite(maximumEdgeLength) ||
        maximumEdgeLength <= 0.0 ||
        maximumIterations == 0U ||
        !validTriangleWorkspaceStorage(triangulation.points.size(),
                                       triangulation.triangles.size(), extraStorageBytes) ||
        !detail::hasValidTriangulation(
            triangulation, maximumPointCount,
            maximumTriangleCount, budget)) {
        return false;
    }

    const double maximumSquared =
        maximumEdgeLength * maximumEdgeLength;
    if (!std::isfinite(maximumSquared)) return false;

    if (!budget.consume(triangulation.points.size()) ||
        !budget.consume(triangulation.sourceIndices.size()) ||
        !budget.consume(triangulation.triangles.size())) return false;
    PolygonTriangulation candidate = triangulation;
    for (unsigned int iteration = 0U;
         iteration < maximumIterations; ++iteration) {
        bool requiresSplit = false;
        if (!budget.consumeProduct(candidate.triangles.size(), 3U)) return false;
        for (std::vector<Triangle>::const_iterator triangle =
                 candidate.triangles.begin();
             triangle != candidate.triangles.end(); ++triangle) {
            if (squaredDistance(
                    candidate.points[triangle->first],
                    candidate.points[triangle->second]) >
                    maximumSquared ||
                squaredDistance(
                    candidate.points[triangle->second],
                    candidate.points[triangle->third]) >
                    maximumSquared ||
                squaredDistance(
                    candidate.points[triangle->third],
                    candidate.points[triangle->first]) >
                    maximumSquared) {
                requiresSplit = true;
                break;
            }
        }
        if (!requiresSplit) {
            triangulation.swap(candidate);
            return true;
        }
        if (candidate.triangles.size() >
            maximumTriangleCount / 4U) {
            return false;
        }
        // Admit the simultaneous old/new geometry and midpoint-map staging
        // before reserve or point growth. Three distinct edges/face is an
        // intentionally conservative upper bound for the new points.
        std::size_t addedPoints = 0U, plannedPoints = 0U, plannedTriangles = 0U;
        if (!memory::checkedMultiply(candidate.triangles.size(), 3U, addedPoints) ||
            !memory::checkedAdd(candidate.points.size(), addedPoints, plannedPoints) ||
            !memory::checkedMultiply(candidate.triangles.size(), 4U, plannedTriangles) ||
            !validTriangleWorkspaceStorage(std::min(plannedPoints, maximumPointCount),
                                           plannedTriangles, extraStorageBytes)) return false;
        // Charge bounded tree lookup/insert costs and all emitted entries
        // before allocating the refined triangle buffer.
        const std::size_t treeCost = detail::polygonTreeWork(maximumPointCount);
        if (!budget.consumeProduct(candidate.triangles.size(), 6U * treeCost + 7U))
            return false;

        std::map<detail::EdgeKey, std::size_t> midpoints;
        std::vector<Triangle> refined;
        refined.reserve(plannedTriangles);
        for (std::vector<Triangle>::const_iterator triangle =
                 candidate.triangles.begin();
             triangle != candidate.triangles.end(); ++triangle) {
            const std::size_t firstSecond = detail::midpointIndex(
                triangle->first, triangle->second,
                candidate.points, midpoints, maximumPointCount);
            const std::size_t secondThird = detail::midpointIndex(
                triangle->second, triangle->third,
                candidate.points, midpoints, maximumPointCount);
            const std::size_t thirdFirst = detail::midpointIndex(
                triangle->third, triangle->first,
                candidate.points, midpoints, maximumPointCount);
            if (firstSecond == std::numeric_limits<std::size_t>::max() ||
                secondThird == std::numeric_limits<std::size_t>::max() ||
                thirdFirst == std::numeric_limits<std::size_t>::max()) {
                return false;
            }
            refined.push_back(Triangle(
                triangle->first, firstSecond, thirdFirst));
            refined.push_back(Triangle(
                firstSecond, triangle->second, secondThird));
            refined.push_back(Triangle(
                thirdFirst, secondThird, triangle->third));
            refined.push_back(Triangle(
                firstSecond, secondThird, thirdFirst));
        }
        candidate.triangles.swap(refined);
    }
    if (!budget.consumeProduct(candidate.triangles.size(), 3U)) return false;
    for (std::vector<Triangle>::const_iterator triangle =
             candidate.triangles.begin();
         triangle != candidate.triangles.end(); ++triangle) {
        if (squaredDistance(
                candidate.points[triangle->first],
                candidate.points[triangle->second]) >
                maximumSquared ||
            squaredDistance(
                candidate.points[triangle->second],
                candidate.points[triangle->third]) >
                maximumSquared ||
            squaredDistance(
                candidate.points[triangle->third],
                candidate.points[triangle->first]) >
                maximumSquared) {
            return false;
        }
    }
    triangulation.swap(candidate);
    return true;
}

inline bool refineTriangulation(
    PolygonTriangulation& triangulation, double maximumEdgeLength,
    unsigned int maximumIterations, std::size_t maximumPointCount,
    std::size_t maximumTriangleCount)
{
    PolygonWorkBudget budget;
    return refineTriangulation(triangulation, maximumEdgeLength, maximumIterations,
                               maximumPointCount, maximumTriangleCount, budget);
}

inline bool relaxInteriorVertices(
    PolygonTriangulation& triangulation,
    unsigned int iterations,
    PolygonWorkBudget& budget,
    double relaxation = 0.5,
    std::size_t extraStorageBytes = 0U)
{
    if (iterations == 0U) return !budget.exceeded();
    if (triangulation.points.empty() ||
        triangulation.triangles.empty() ||
        !std::isfinite(relaxation) ||
        relaxation <= 0.0 || relaxation > 1.0 ||
        !validTriangleWorkspaceStorage(triangulation.points.size(),
                                       triangulation.triangles.size(), extraStorageBytes) ||
        !detail::hasValidTriangulation(
            triangulation,
            std::numeric_limits<std::size_t>::max(),
            std::numeric_limits<std::size_t>::max(), budget)) {
        return false;
    }

    if (!budget.consume(triangulation.points.size())) return false;
    std::vector<Point2> working = triangulation.points;
    for (unsigned int iteration = 0U; iteration < iterations; ++iteration) {
        // Every face contributes three edges/neighbors/incidences. Include
        // logarithmic ordered-container work, then the linear point passes.
        const std::size_t treeCost = detail::polygonTreeWork(triangulation.triangles.size()) + 2U;
        if (!budget.consumeProduct(triangulation.triangles.size(), 12U * treeCost + 18U) ||
            !budget.consumeProduct(working.size(), 6U)) return false;
        std::map<detail::EdgeKey, unsigned int> edgeCounts;
        std::vector<std::set<std::size_t> > neighbors(
            triangulation.points.size());
        std::vector<std::vector<std::size_t> > incident(
            triangulation.points.size());
        detail::PolygonTolerance tolerance;
        if (!detail::polygonTolerance(working, 1.0e-12, tolerance)) return false;
        if (!budget.consumeProduct(triangulation.triangles.size(), 4U)) return false;
        for (const Triangle& triangle : triangulation.triangles) {
            const double area = cross2(working[triangle.first], working[triangle.second],
                                       working[triangle.third]);
            if (!std::isfinite(area) || area <= tolerance.area) return false;
        }
        for (std::size_t triangleIndex = 0U;
             triangleIndex < triangulation.triangles.size();
             ++triangleIndex) {
            const Triangle& triangle =
                triangulation.triangles[triangleIndex];
            incident[triangle.first].push_back(triangleIndex);
            incident[triangle.second].push_back(triangleIndex);
            incident[triangle.third].push_back(triangleIndex);
        }
        for (std::vector<Triangle>::const_iterator triangle =
                 triangulation.triangles.begin();
             triangle != triangulation.triangles.end(); ++triangle) {
            for (std::size_t edge = 0U; edge < 3U; ++edge) {
                const std::size_t first = (*triangle)[edge];
                const std::size_t second = (*triangle)[(edge + 1U) % 3U];
                ++edgeCounts[detail::EdgeKey(first, second)];
                neighbors[first].insert(second);
                neighbors[second].insert(first);
            }
        }
        std::vector<bool> boundary(triangulation.points.size(), false);
        for (std::map<detail::EdgeKey, unsigned int>::const_iterator edge =
                 edgeCounts.begin(); edge != edgeCounts.end(); ++edge) {
            if (edge->second == 1U) {
                boundary[edge->first.first] = true;
                boundary[edge->first.second] = true;
            }
        }

        std::vector<Point2> next = working;
        for (std::size_t vertex = 0U;
             vertex < triangulation.points.size(); ++vertex) {
            if (boundary[vertex] || neighbors[vertex].empty()) continue;
            Point2 average;
            for (std::set<std::size_t>::const_iterator neighbor =
                     neighbors[vertex].begin();
                 neighbor != neighbors[vertex].end(); ++neighbor) {
                average = average + working[*neighbor];
            }
            average = average /
                static_cast<double>(neighbors[vertex].size());
            const Point2 candidate =
                working[vertex] * (1.0 - relaxation) +
                average * relaxation;
            if (!isFinite(candidate)) continue;

            bool valid = true;
            for (std::vector<std::size_t>::const_iterator triangleIndex =
                     incident[vertex].begin();
                 triangleIndex != incident[vertex].end();
                 ++triangleIndex) {
                const Triangle& triangle =
                    triangulation.triangles[*triangleIndex];
                const Point2 first =
                    triangle.first == vertex
                        ? candidate
                        : working[triangle.first];
                const Point2 second =
                    triangle.second == vertex
                        ? candidate
                        : working[triangle.second];
                const Point2 third =
                    triangle.third == vertex
                        ? candidate
                        : working[triangle.third];
                const double area = cross2(first, second, third);
                if (!std::isfinite(area) || area <= tolerance.area) {
                    valid = false;
                    break;
                }
            }
            if (valid) next[vertex] = candidate;
        }
        // Safe individual proposals can invert a face when applied together.
        // Match the 3D remesher's bounded whole-batch backtracking policy.
        const unsigned int maximumBacktrackingSteps = 32U;
        bool accepted = false;
        for (unsigned int step = 0U; step <= maximumBacktrackingSteps; ++step) {
            if (!budget.consumeProduct(triangulation.triangles.size(), 4U)) return false;
            bool valid = true;
            for (const Triangle& triangle : triangulation.triangles) {
                const double area = cross2(next[triangle.first], next[triangle.second],
                                           next[triangle.third]);
                if (!std::isfinite(area) || area <= tolerance.area) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                accepted = true;
                break;
            }
            if (step == maximumBacktrackingSteps) break;
            if (!budget.consume(next.size())) return false;
            for (std::size_t vertex = 0U; vertex < next.size(); ++vertex)
                next[vertex] = working[vertex] + (next[vertex] - working[vertex]) * 0.5;
        }
        // An unsafe quality proposal is a no-op, not a partial geometry commit.
        if (accepted) working.swap(next);
    }
    triangulation.points.swap(working);
    return true;
}

inline void relaxInteriorVertices(
    PolygonTriangulation& triangulation, unsigned int iterations, double relaxation = 0.5)
{
    // Compatibility entry point. Node consumers use the status-returning
    // shared-budget overload, and must not ignore a failed relaxation stage.
    PolygonWorkBudget budget;
    (void)relaxInteriorVertices(triangulation, iterations, budget, relaxation);
}

} // namespace geometry
} // namespace dommy
