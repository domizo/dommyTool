#pragma once

#include "geometry/DGeometryTypes.h"
#include "geometry/DTriangleComponents.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DProductSum.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

struct Plane3 {
    Plane3() : normal(), offset(0.0), origin(), hasOrigin(false) {}
    Plane3(const Point3& planeNormal, double planeOffset)
        : normal(planeNormal), offset(planeOffset), origin(), hasOrigin(false) {}
    Plane3(const Point3& planeNormal, double planeOffset, const Point3& planeOrigin)
        : normal(planeNormal), offset(planeOffset), origin(planeOrigin), hasOrigin(true) {}

    Point3 normal;
    double offset;
    // makePlane preserves its generating point so translated planes can use
    // dot(normal, point-origin) instead of cancelling two large scalars.
    Point3 origin;
    bool hasOrigin;
};

namespace slicer_detail {

inline double signedDistance(const Plane3& plane, const Point3& point)
{
    if (!plane.hasOrigin) {
        double value = 0.0;
        const std::array<double, 4> left{{plane.normal.x, plane.normal.y, plane.normal.z, plane.offset}};
        const std::array<double, 4> right{{point.x, point.y, point.z, 1.0}};
        const double magnitude = length(plane.normal);
        if (!std::isfinite(magnitude) || magnitude <= 1.0e-15 ||
            !numeric::checkedProductSum(left, right, value))
            return std::numeric_limits<double>::quiet_NaN();
        const double result = value / magnitude;
        return std::isfinite(result) ? result : std::numeric_limits<double>::quiet_NaN();
    }
    long double value = 0.0L;
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const long double coordinate = static_cast<long double>(point[axis]) -
            static_cast<long double>(plane.origin[axis]);
        value += static_cast<long double>(plane.normal[axis]) * coordinate;
    }
    double result = 0.0;
    return numeric::checkedNumericCast(value, result)
        ? result : std::numeric_limits<double>::quiet_NaN();
}

inline bool appendUniquePoint(std::vector<Point3>& points,
                              const Point3& point,
                              double squaredTolerance)
{
    if (!isFinite(point)) return false;
    for (std::vector<Point3>::const_iterator existing = points.begin();
         existing != points.end(); ++existing) {
        if (squaredDistance(*existing, point) <= squaredTolerance) {
            return true;
        }
    }
    points.push_back(point);
    return true;
}

inline bool intersectTriangle(
    const Point3& first,
    const Point3& second,
    const Point3& third,
    const Plane3& plane,
    double tolerance,
    Segment3& segment)
{
    const Point3 vertices[3] = {first, second, third};
    double distances[3] = {
        signedDistance(plane, first),
        signedDistance(plane, second),
        signedDistance(plane, third)
    };
    if (!std::isfinite(distances[0]) ||
        !std::isfinite(distances[1]) ||
        !std::isfinite(distances[2])) {
        return false;
    }

    const bool firstOn = std::fabs(distances[0]) <= tolerance;
    const bool secondOn = std::fabs(distances[1]) <= tolerance;
    const bool thirdOn = std::fabs(distances[2]) <= tolerance;
    if (firstOn && secondOn && thirdOn) {
        // Coplanar faces do not have one unambiguous slice contour. Adjacent
        // non-coplanar faces still contribute the boundary deterministically.
        return false;
    }

    std::vector<Point3> intersections;
    intersections.reserve(3U);
    const double squaredTolerance = tolerance * tolerance;
    for (std::size_t edge = 0U; edge < 3U; ++edge) {
        const std::size_t next = (edge + 1U) % 3U;
        const bool currentOn = std::fabs(distances[edge]) <= tolerance;
        const bool nextOn = std::fabs(distances[next]) <= tolerance;
        if (currentOn) {
            if (!appendUniquePoint(
                    intersections, vertices[edge], squaredTolerance)) {
                return false;
            }
        }
        if (currentOn || nextOn ||
            (distances[edge] < 0.0) == (distances[next] < 0.0)) {
            continue;
        }

        const double denominator = distances[edge] - distances[next];
        if (!std::isfinite(denominator) ||
            std::fabs(denominator) <= tolerance) {
            continue;
        }
        const double parameter = distances[edge] / denominator;
        if (!std::isfinite(parameter) ||
            parameter < -tolerance || parameter > 1.0 + tolerance) {
            continue;
        }
        const Point3 intersection =
            vertices[edge] +
            (vertices[next] - vertices[edge]) * parameter;
        if (!appendUniquePoint(
                intersections, intersection, squaredTolerance)) {
            return false;
        }
    }
    if (intersections.size() < 2U) return false;

    std::size_t firstIndex = 0U;
    std::size_t secondIndex = 1U;
    double farthest = squaredDistance(
        intersections[firstIndex], intersections[secondIndex]);
    for (std::size_t firstCandidate = 0U;
         firstCandidate < intersections.size(); ++firstCandidate) {
        for (std::size_t secondCandidate = firstCandidate + 1U;
             secondCandidate < intersections.size(); ++secondCandidate) {
            const double distance = squaredDistance(
                intersections[firstCandidate],
                intersections[secondCandidate]);
            if (distance > farthest) {
                farthest = distance;
                firstIndex = firstCandidate;
                secondIndex = secondCandidate;
            }
        }
    }
    if (!std::isfinite(farthest) || farthest <= squaredTolerance) {
        return false;
    }
    segment = Segment3(
        intersections[firstIndex], intersections[secondIndex]);
    return true;
}

struct CellKey {
    CellKey() : x(0), y(0), z(0) {}
    CellKey(long long xValue, long long yValue, long long zValue)
        : x(xValue), y(yValue), z(zValue) {}

    bool operator<(const CellKey& rhs) const
    {
        return x < rhs.x ||
               (x == rhs.x &&
                (y < rhs.y || (y == rhs.y && z < rhs.z)));
    }

    long long x;
    long long y;
    long long z;
};

inline bool quantize(double value,
                     double inverseTolerance,
                     long long& result)
{
    return numeric::checkedSpatialCellIndex(value, inverseTolerance, result);
}

inline bool cellKey(const Point3& point,
                    double inverseTolerance,
                    CellKey& key)
{
    return quantize(point.x, inverseTolerance, key.x) &&
           quantize(point.y, inverseTolerance, key.y) &&
           quantize(point.z, inverseTolerance, key.z);
}

class PointWelder {
public:
    PointWelder(double tolerance, std::size_t maximumPointCount,
                generation::WorkBudget& budget, const Point3& origin = Point3())
        : m_squaredTolerance(tolerance * tolerance),
          m_inverseTolerance(1.0 / tolerance),
          m_maximumPointCount(maximumPointCount), m_budget(budget), m_origin(origin) {}

    bool findOrInsert(const Point3& point, std::size_t& index)
    {
        CellKey key;
        if (!m_budget.consume(1U) || !isFinite(point) || !isFinite(m_origin) ||
            !std::isfinite(m_squaredTolerance) ||
            !std::isfinite(m_inverseTolerance) || m_inverseTolerance <= 0.0 ||
            !cellKey(point - m_origin, m_inverseTolerance, key)) {
            return false;
        }

        for (long long xOffset = -1; xOffset <= 1; ++xOffset) {
            for (long long yOffset = -1; yOffset <= 1; ++yOffset) {
                for (long long zOffset = -1; zOffset <= 1; ++zOffset) {
                    if (!m_budget.consume(
                            2U * generation::orderedContainerWork(m_buckets.size())))
                        return false;
                    const CellKey neighbor(
                        key.x + xOffset, key.y + yOffset, key.z + zOffset);
                    const std::map<CellKey, std::vector<std::size_t> >::
                        const_iterator bucket = m_buckets.find(neighbor);
                    if (bucket == m_buckets.end()) continue;
                    for (std::vector<std::size_t>::const_iterator existing =
                             bucket->second.begin();
                         existing != bucket->second.end(); ++existing) {
                        if (!m_budget.consume(1U)) return false;
                        if (squaredDistance(
                                m_points[*existing], point) <=
                            m_squaredTolerance) {
                            index = *existing;
                            return true;
                        }
                    }
                }
            }
        }

        if (m_points.size() >= m_maximumPointCount ||
            !m_budget.consume(2U * generation::orderedContainerWork(m_buckets.size()) + 2U))
            return false;
        index = m_points.size();
        m_points.push_back(point);
        m_buckets[key].push_back(index);
        return true;
    }

    const std::vector<Point3>& points() const { return m_points; }

private:
    double m_squaredTolerance;
    double m_inverseTolerance;
    std::size_t m_maximumPointCount;
    generation::WorkBudget& m_budget;
    Point3 m_origin;
    std::vector<Point3> m_points;
    std::map<CellKey, std::vector<std::size_t> > m_buckets;
};

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

inline bool takeEdge(
    std::size_t first,
    std::size_t second,
    std::set<EdgeKey>& unusedEdges,
    generation::WorkBudget& budget)
{
    if (!budget.consume(2U * generation::orderedContainerWork(unusedEdges.size()) + 1U))
        return false;
    const EdgeKey key(first, second);
    const std::set<EdgeKey>::iterator found = unusedEdges.find(key);
    if (found == unusedEdges.end()) return false;
    unusedEdges.erase(found);
    return true;
}

// Monotone per-vertex cursors visit at most two adjacency entries per edge
// over the complete contour batch, including arbitrary branching vertices.
// Edges consumed from the opposite end are skipped once, never rescanned.
inline bool nextUnusedNeighbor(
    std::size_t current,
    const std::vector<std::vector<std::size_t> >& adjacency,
    const std::set<EdgeKey>& unusedEdges,
    std::vector<std::size_t>& nextNeighbor,
    generation::WorkBudget& budget,
    std::size_t& result)
{
    const std::vector<std::size_t>& neighbors = adjacency[current];
    std::size_t& cursor = nextNeighbor[current];
    while (cursor < neighbors.size()) {
        if (!budget.consume(2U * generation::orderedContainerWork(unusedEdges.size()) + 1U))
            return false;
        const std::size_t neighbor = neighbors[cursor++];
        if (unusedEdges.find(EdgeKey(current, neighbor)) !=
            unusedEdges.end()) {
            result = neighbor;
            return true;
        }
    }
    result = std::numeric_limits<std::size_t>::max();
    return true;
}

inline bool tracePolyline(
    std::size_t start,
    std::size_t next,
    const std::vector<Point3>& points,
    const std::vector<std::vector<std::size_t> >& adjacency,
    std::set<EdgeKey>& unusedEdges,
    std::vector<std::size_t>& nextNeighbor,
    generation::WorkBudget& budget,
    std::size_t maximumOutputPointCount,
    Polyline3& polyline)
{
    polyline.clear();
    if (maximumOutputPointCount < 2U) return false;
    if (!budget.consume(2U) || !takeEdge(start, next, unusedEdges, budget)) return false;
    polyline.push_back(points[start]);
    polyline.push_back(points[next]);

    std::size_t current = next;
    while (current != start && adjacency[current].size() == 2U) {
        if (!budget.consume(1U)) return false;
        std::size_t following = 0U;
        if (!nextUnusedNeighbor(current, adjacency, unusedEdges,
                                nextNeighbor, budget, following)) return false;
        if (following == std::numeric_limits<std::size_t>::max()) break;
        if (!takeEdge(current, following, unusedEdges, budget)) return false;
        if (polyline.size() >= maximumOutputPointCount || !budget.consume(1U)) return false;
        polyline.push_back(points[following]);
        current = following;
    }
    return polyline.size() >= 2U;
}

} // namespace slicer_detail

inline bool makePlane(const Point3& point,
                      const Point3& normal,
                      Plane3& plane)
{
    Point3 unitNormal;
    if (!isFinite(point) || !normalized(normal, unitNormal)) return false;
    const double offset = -dot(unitNormal, point);
    if (!std::isfinite(offset)) return false;
    plane = Plane3(unitNormal, offset, point);
    return true;
}

inline bool sliceTriangleComponent(
    const std::vector<Point3>& points,
    const std::vector<Triangle>& triangles,
    const Plane3& plane,
    std::vector<Polyline3>& polylines,
    std::size_t maximumSegmentCount,
    std::size_t maximumOutputPointCount,
    generation::WorkBudget& budget,
    double requestedTolerance = 1.0e-9)
{
    if (points.empty() || triangles.empty() ||
        triangles.size() > maximumSegmentCount ||
        maximumSegmentCount == 0U || maximumOutputPointCount < 2U ||
        !isFinite(plane.normal) || !std::isfinite(plane.offset) ||
        (plane.hasOrigin && !isFinite(plane.origin)) || budget.exceeded()) {
        return false;
    }
    Point3 normalizedNormal;
    if (!normalized(plane.normal, normalizedNormal)) return false;
    Plane3 normalizedPlane = plane;
    // Keep equation coefficients together until after their product sum. Only
    // anchored planes may normalize their normal without shifting the plane.
    if (plane.hasOrigin) normalizedPlane.normal = normalizedNormal;

    Point3 origin;
    double extent = 0.0;
    if (!referencedTriangleFrame(points, triangles, origin, extent, budget) ||
        !budget.consumeProduct(triangles.size(), 64U)) return false;

    const double tolerance = scaledTolerance(
        extent, requestedTolerance);
    const double squaredTolerance = tolerance * tolerance;
    const double inverseTolerance = 1.0 / tolerance;
    if (!std::isfinite(tolerance) ||
        !std::isfinite(squaredTolerance) ||
        !std::isfinite(inverseTolerance) ||
        inverseTolerance <= 0.0) {
        return false;
    }

    std::vector<Segment3> segments;
    segments.reserve(std::min(triangles.size(), maximumSegmentCount));
    for (std::vector<Triangle>::const_iterator triangle =
             triangles.begin(); triangle != triangles.end(); ++triangle) {
        if (triangle->first >= points.size() ||
            triangle->second >= points.size() ||
            triangle->third >= points.size()) {
            return false;
        }
        Segment3 segment;
        if (!slicer_detail::intersectTriangle(
                points[triangle->first],
                points[triangle->second],
                points[triangle->third],
                normalizedPlane, tolerance, segment)) {
            continue;
        }
        if (segments.size() >= maximumSegmentCount) return false;
        segments.push_back(segment);
    }

    std::vector<Polyline3> candidate;
    if (segments.empty()) {
        polylines.swap(candidate);
        return true;
    }

    slicer_detail::PointWelder welder(
        tolerance, maximumOutputPointCount, budget, origin);
    std::set<slicer_detail::EdgeKey> edges;
    for (std::vector<Segment3>::const_iterator segment = segments.begin();
         segment != segments.end(); ++segment) {
        std::size_t first = 0U;
        std::size_t second = 0U;
        if (!welder.findOrInsert(segment->first, first) ||
            !welder.findOrInsert(segment->second, second)) {
            return false;
        }
        if (first != second) {
            if (!budget.consume(2U * generation::orderedContainerWork(edges.size()) + 1U))
                return false;
            edges.insert(slicer_detail::EdgeKey(first, second));
        }
    }

    const std::vector<Point3>& weldedPoints = welder.points();
    if (!budget.consumeProduct(weldedPoints.size(), 2U) ||
        !budget.consumeProduct(edges.size(), 4U)) return false;
    std::vector<std::vector<std::size_t> > adjacency(weldedPoints.size());
    std::vector<std::size_t> nextNeighbor(weldedPoints.size(), 0U);
    for (std::set<slicer_detail::EdgeKey>::const_iterator edge =
             edges.begin(); edge != edges.end(); ++edge) {
        adjacency[edge->first].push_back(edge->second);
        adjacency[edge->second].push_back(edge->first);
    }
    // EdgeKey's lexicographic set order already emits each vertex's unique
    // neighbors in ascending order: smaller endpoints first, then larger ones.

    std::set<slicer_detail::EdgeKey> unusedEdges = edges;
    std::size_t outputPointCount = 0U;
    for (std::size_t start = 0U; start < adjacency.size(); ++start) {
        if (!budget.consume(1U)) return false;
        if (adjacency[start].size() == 2U) continue;
        for (;;) {
            std::size_t next = 0U;
            if (!slicer_detail::nextUnusedNeighbor(
                    start, adjacency, unusedEdges, nextNeighbor, budget, next)) return false;
            if (next == std::numeric_limits<std::size_t>::max()) break;
            Polyline3 polyline;
            if (!slicer_detail::tracePolyline(
                    start, next, weldedPoints, adjacency, unusedEdges,
                    nextNeighbor, budget,
                    maximumOutputPointCount - outputPointCount,
                    polyline)) {
                return false;
            }
            if (polyline.size() >
                maximumOutputPointCount - outputPointCount) {
                return false;
            }
            outputPointCount += polyline.size();
            if (!budget.consume(1U)) return false;
            candidate.push_back(std::move(polyline));
        }
    }

    while (!unusedEdges.empty()) {
        if (!budget.consume(1U)) return false;
        const slicer_detail::EdgeKey firstEdge = *unusedEdges.begin();
        Polyline3 polyline;
        if (!slicer_detail::tracePolyline(
                firstEdge.first, firstEdge.second, weldedPoints,
                adjacency, unusedEdges,
                nextNeighbor, budget,
                maximumOutputPointCount - outputPointCount,
                polyline)) {
            return false;
        }
        if (polyline.size() >
            maximumOutputPointCount - outputPointCount) {
            return false;
        }
        outputPointCount += polyline.size();
        if (!budget.consume(1U)) return false;
        candidate.push_back(std::move(polyline));
    }

    polylines.swap(candidate);
    return true;
}

inline bool sliceTriangleMesh(
    const std::vector<Point3>& points, const std::vector<Triangle>& triangles,
    const Plane3& plane, std::vector<Polyline3>& polylines,
    std::size_t maximumSegmentCount, std::size_t maximumOutputPointCount,
    generation::WorkBudget& budget, double requestedTolerance = 1.0e-9)
{
    if (triangles.size() > maximumSegmentCount || maximumSegmentCount == 0U ||
        maximumOutputPointCount < 2U || !isFinite(plane.normal) ||
        !std::isfinite(plane.offset) || (plane.hasOrigin && !isFinite(plane.origin)) ||
        !std::isfinite(requestedTolerance) || requestedTolerance <= 0.0) return false;
    Point3 unitNormal;
    if (!normalized(plane.normal, unitNormal)) return false;
    std::vector<TriangleComponent> components;
    if (!triangleComponents(points, triangles, components, budget, true) ||
        !budget.consume(points.size())) return false;
    std::vector<std::size_t> remap(points.size());
    std::vector<Polyline3> candidate;
    std::size_t outputPointCount = 0U;
    for (const TriangleComponent& component : components) {
        std::vector<Point3> localPoints;
        std::vector<Triangle> localTriangles;
        if (!component_detail::extractTriangleComponent(points, triangles, component, remap,
                                       localPoints, localTriangles, budget)) return false;
        std::vector<Polyline3> result;
        if (!sliceTriangleComponent(localPoints, localTriangles, plane, result,
                                     maximumSegmentCount,
                                     std::max<std::size_t>(2U, maximumOutputPointCount - outputPointCount),
                                     budget, requestedTolerance)) return false;
        for (Polyline3& line : result) {
            if (!generation::checkedSum(outputPointCount, line.size(),
                                        maximumOutputPointCount, outputPointCount) ||
                !budget.consume(3U)) return false; // Move plus amortized handle-vector growth.
            candidate.push_back(std::move(line));
        }
    }
    polylines.swap(candidate);
    return true;
}

// Compatibility entry point for one independent operation. Maya adapters use
// the overload above to share a single budget across every mesh and cut.
inline bool sliceTriangleMesh(
    const std::vector<Point3>& points,
    const std::vector<Triangle>& triangles,
    const Plane3& plane,
    std::vector<Polyline3>& polylines,
    std::size_t maximumSegmentCount,
    std::size_t maximumOutputPointCount,
    double requestedTolerance = 1.0e-9)
{
    generation::WorkBudget budget;
    return sliceTriangleMesh(points, triangles, plane, polylines,
                             maximumSegmentCount, maximumOutputPointCount,
                             budget, requestedTolerance);
}

} // namespace geometry
} // namespace dommy
