#include "geometry/DMeshRemesher.h"
#include "geometry/DPointSetRegularizer.h"
#include "geometry/DPolygonTriangulator.h"
#include "geometry/DTrianglePlaneSlicer.h"
#include "remesh_regression_fixture.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace {

bool geometryNear(double lhs, double rhs, double tolerance = 1.0e-8)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

double polygonArea(const std::vector<dommy::geometry::Point2>& points)
{
    double area = 0.0;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const dommy::geometry::Point2& first = points[index];
        const dommy::geometry::Point2& second =
            points[(index + 1U) % points.size()];
        area += first.x * second.y - second.x * first.y;
    }
    return std::fabs(area) * 0.5;
}

double triangleArea(
    const dommy::geometry::Point2& first,
    const dommy::geometry::Point2& second,
    const dommy::geometry::Point2& third)
{
    return std::fabs(dommy::geometry::cross2(first, second, third)) * 0.5;
}

void testPolygonTriangulation()
{
    using namespace dommy::geometry;
    const std::vector<Point2> concave = {
        Point2(0.0, 0.0),
        Point2(3.0, 0.0),
        Point2(3.0, 1.0),
        Point2(1.0, 1.0),
        Point2(1.0, 3.0),
        Point2(0.0, 3.0),
        Point2(0.0, 0.0)
    };
    PolygonTriangulation triangulation;
    assert(triangulateSimplePolygon(
        concave, triangulation, 100U, 100U, 10000U));
    assert(triangulation.points.size() == 6U);
    assert(triangulation.triangles.size() == 4U);
    double area = 0.0;
    for (std::vector<Triangle>::const_iterator triangle =
             triangulation.triangles.begin();
         triangle != triangulation.triangles.end(); ++triangle) {
        area += triangleArea(
            triangulation.points[triangle->first],
            triangulation.points[triangle->second],
            triangulation.points[triangle->third]);
    }
    std::vector<Point2> openConcave(concave.begin(), concave.end() - 1);
    assert(geometryNear(area, polygonArea(openConcave)));

    std::vector<std::vector<std::size_t> > partitions;
    assert(convexPartition(triangulation, partitions, 10000U));
    assert(!partitions.empty());
    for (std::vector<std::vector<std::size_t> >::const_iterator polygon =
             partitions.begin(); polygon != partitions.end(); ++polygon) {
        assert(polygon->size() >= 3U);
        for (std::size_t index = 0U; index < polygon->size(); ++index) {
            const Point2& first =
                triangulation.points[(*polygon)[index]];
            const Point2& second =
                triangulation.points[
                    (*polygon)[(index + 1U) % polygon->size()]];
            const Point2& third =
                triangulation.points[
                    (*polygon)[(index + 2U) % polygon->size()]];
            assert(cross2(first, second, third) >= -1.0e-8);
        }
    }

    const std::vector<Point2> selfIntersecting = {
        Point2(0.0, 0.0), Point2(2.0, 2.0),
        Point2(0.0, 2.0), Point2(2.0, 0.0)
    };
    PolygonTriangulation rejected;
    assert(!triangulateSimplePolygon(
        selfIntersecting, rejected, 100U, 100U, 10000U));

    PolygonTriangulation refined = triangulation;
    assert(refineTriangulation(
        refined, 0.8, 4U, 10000U, 10000U));
    assert(refined.points.size() > triangulation.points.size());
    assert(refined.triangles.size() > triangulation.triangles.size());
    relaxInteriorVertices(refined, 2U);
    for (std::vector<Point2>::const_iterator point = refined.points.begin();
         point != refined.points.end(); ++point) {
        assert(isFinite(*point));
    }

    PolygonTriangulation transactional = triangulation;
    const std::size_t originalPointCount = transactional.points.size();
    const std::size_t originalTriangleCount =
        transactional.triangles.size();
    assert(!refineTriangulation(
        transactional, 0.01, 2U,
        originalPointCount + 1U, 10000U));
    assert(transactional.points.size() == originalPointCount);
    assert(transactional.triangles.size() == originalTriangleCount);

    PolygonTriangulation malformed = triangulation;
    malformed.triangles.front().first = malformed.points.size();
    std::vector<std::vector<std::size_t> > malformedPartitions;
    assert(!convexPartition(
        malformed, malformedPartitions, 10000U));
    assert(!refineTriangulation(
        malformed, 0.8, 2U, 100U, 100U));
}

void testTrianglePlaneSlicer()
{
    using namespace dommy::geometry;
    const std::vector<Point3> points = {
        Point3(0.0, 0.0, 0.0), Point3(1.0, 0.0, 0.0),
        Point3(1.0, 1.0, 0.0), Point3(0.0, 1.0, 0.0),
        Point3(0.0, 0.0, 1.0), Point3(1.0, 0.0, 1.0),
        Point3(1.0, 1.0, 1.0), Point3(0.0, 1.0, 1.0)
    };
    const std::vector<Triangle> triangles = {
        Triangle(0U, 2U, 1U), Triangle(0U, 3U, 2U),
        Triangle(4U, 5U, 6U), Triangle(4U, 6U, 7U),
        Triangle(0U, 1U, 5U), Triangle(0U, 5U, 4U),
        Triangle(1U, 2U, 6U), Triangle(1U, 6U, 5U),
        Triangle(2U, 3U, 7U), Triangle(2U, 7U, 6U),
        Triangle(3U, 0U, 4U), Triangle(3U, 4U, 7U)
    };
    Plane3 plane;
    assert(makePlane(
        Point3(0.0, 0.0, 0.5), Point3(0.0, 0.0, 1.0), plane));
    std::vector<Polyline3> polylines;
    assert(sliceTriangleMesh(
        points, triangles, plane, polylines, 100U, 100U));
    assert(polylines.size() == 1U);
    assert(polylines.front().size() >= 5U);
    assert(squaredDistance(
        polylines.front().front(), polylines.front().back()) <= 1.0e-16);
    for (Polyline3::const_iterator point = polylines.front().begin();
         point != polylines.front().end(); ++point) {
        assert(geometryNear(point->z, 0.5));
    }

    std::vector<Polyline3> boundedPolylines;
    assert(!sliceTriangleMesh(
        points, triangles, plane, boundedPolylines,
        triangles.size() - 1U, 100U));
}

void testPointSetRegularizer()
{
    using namespace dommy::geometry;
    std::vector<Point3> input;
    for (std::size_t index = 0U; index < 20U; ++index) {
        input.push_back(Point3(
            static_cast<double>(index % 5U),
            static_cast<double>((index / 5U) % 2U),
            static_cast<double>(index / 10U)));
    }
    RegularizedPointSet first;
    RegularizedPointSet second;
    assert(regularizePointSet(
        input, 50.0, 2.0, 3U, true,
        first, 100U, 1000000U));
    assert(regularizePointSet(
        input, 50.0, 2.0, 3U, true,
        second, 100U, 1000000U));
    assert(first.points.size() == 10U);
    assert(first.points.size() == first.sourceIndices.size());
    assert(first.points.size() == second.points.size());
    for (std::size_t index = 0U; index < first.points.size(); ++index) {
        assert(isFinite(first.points[index]));
        assert(first.sourceIndices[index] == second.sourceIndices[index]);
        assert(squaredDistance(
            first.points[index], second.points[index]) <= 1.0e-20);
    }

    RegularizedPointSet empty;
    assert(regularizePointSet(
        input, 0.0, 2.0, 1U, true,
        empty, 100U, 1000000U));
    assert(empty.points.empty());
    assert(empty.sourceIndices.empty());

    RegularizedPointSet nearestOnly;
    assert(regularizePointSet(
        input, 50.0, 2.0, 1U, true,
        nearestOnly, 100U, 1000000U, 1U));
    assert(nearestOnly.points.size() == 10U);
}

void testIsotropicRemesher()
{
    using namespace dommy::geometry;
    TriangleMesh input;
    input.points.push_back(Point3(0.0, 0.0, 0.0));
    input.points.push_back(Point3(2.0, 0.0, 0.0));
    input.points.push_back(Point3(2.0, 2.0, 0.0));
    input.points.push_back(Point3(0.0, 2.0, 0.0));
    input.triangles.push_back(Triangle(0U, 1U, 2U));
    input.triangles.push_back(Triangle(0U, 2U, 3U));

    TriangleMesh output;
    assert(isotropicRemesh(
        input, 0.75, 3U, output, 10000U, 10000U));
    assert(output.points.size() > input.points.size());
    assert(output.triangles.size() > input.triangles.size());
    for (std::vector<Point3>::const_iterator point = output.points.begin();
         point != output.points.end(); ++point) {
        assert(isFinite(*point));
        assert(geometryNear(point->z, 0.0));
    }
    for (std::vector<Triangle>::const_iterator triangle =
             output.triangles.begin();
         triangle != output.triangles.end(); ++triangle) {
        assert(triangle->first < output.points.size());
        assert(triangle->second < output.points.size());
        assert(triangle->third < output.points.size());
        assert(triangleAreaSquared(
            output.points[triangle->first],
            output.points[triangle->second],
            output.points[triangle->third]) > 0.0);
    }

    TriangleMesh unchanged;
    unchanged.points.push_back(Point3(9.0, 9.0, 9.0));
    assert(!isotropicRemesh(
        input, 0.75, 1U, unchanged,
        input.points.size() - 1U, 100U));
    assert(unchanged.points.size() == 1U);
    assert(geometryNear(unchanged.points.front().x, 9.0));
}

void testTranslatedIsotropicRemesher()
{
    using namespace dommy::geometry;
    for (double x : {0.0, double(1.0e12f), double(-1.0e12f), double(1.0e30f)}) {
        for (double edge : {1.0e-6, 1.0, 2.0}) {
            TriangleMesh input, output;
            input.points = {{x, 0, 0}, {x, edge, 0}, {x, 0, edge}};
            input.triangles = {{0, 1, 2}};
            assert(pointSetExtent(input.points) == edge);
            assert(isotropicRemesh(input, 2.0 * edge, 1U, output, 100U, 100U));
            assert(output.points.size() == 3U && output.triangles.size() == 1U);
            for (std::size_t i = 0; i < 3U; ++i)
                for (std::size_t axis = 0; axis < 3U; ++axis)
                    assert(output.points[i][axis] == input.points[i][axis]);
        }
    }
    const double maximum = std::numeric_limits<double>::max();
    assert(std::isnan(pointSetExtent({Point3(-maximum, 0, 0), Point3(maximum, 1, 0)})));
    assert(std::isnan(pointSetExtent({Point3(NAN, 0, 0)})));
    assert(std::isnan(pointSetExtent({})));
    TriangleMesh invalid, unchanged;
    invalid.points = {{-maximum, 0, 0}, {maximum, 1, 0}, {0, 0, 1}};
    invalid.triangles = {{0, 1, 2}};
    unchanged.points = {{7, 8, 9}};
    assert(!isotropicRemesh(invalid, 2, 1, unchanged, 10, 10));
    assert(unchanged.points.size() == 1U && unchanged.points[0].x == 7);
}

bool sameTriangleMesh(const dommy::geometry::TriangleMesh& a,
                      const dommy::geometry::TriangleMesh& b)
{
    if (a.points.size() != b.points.size() || a.triangles.size() != b.triangles.size()) return false;
    for (std::size_t i = 0U; i < a.points.size(); ++i)
        for (std::size_t c = 0U; c < 3U; ++c)
            if (a.points[i][c] != b.points[i][c]) return false;
    for (std::size_t i = 0U; i < a.triangles.size(); ++i)
        for (std::size_t c = 0U; c < 3U; ++c)
            if (a.triangles[i][c] != b.triangles[i][c]) return false;
    return true;
}

void testUnusedRemeshVertices()
{
    using namespace dommy::geometry;
    TriangleMesh input, expected;
    input.points = {{0,0,0}, {1,0,0}, {0,1,0}};
    input.triangles = {{0,1,2}};
    assert(isotropicRemesh(input, 2.0, 1U, expected, 100U, 100U));
    for (double value : {double(1e12f), double(-1e30f),
                         std::numeric_limits<double>::max(),
                         -std::numeric_limits<double>::max()}) {
        for (std::size_t position : {0U, 1U, 3U}) {
            const auto padded = withUnusedVertex(input, position, Point3(value, value, value));
            TriangleMesh output;
            assert(isotropicRemesh(padded, 2.0, 1U, output, 100U, 100U));
            assert(sameTriangleMesh(output, expected));
        }
    }
    // Ignoring a finite unused coordinate must not hide malformed payloads.
    TriangleMesh invalid = withUnusedVertex(input, 0U, Point3(NAN, 0, 0));
    TriangleMesh unchanged = expected;
    assert(!isotropicRemesh(invalid, 2, 1U, unchanged, 100U, 100U));
    assert(sameTriangleMesh(unchanged, expected));
    invalid = input;
    invalid.triangles[0].third = invalid.points.size();
    assert(!isotropicRemesh(invalid, 2, 1U, unchanged, 100U, 100U));
    assert(sameTriangleMesh(unchanged, expected));
}

void testShortEdgeCollapse()
{
    using namespace dommy::geometry;
    TriangleMesh grid;
    const double coordinates[] = {0.0, 0.9, 1.0, 2.0};
    for (unsigned int y = 0U; y < 4U; ++y)
        for (unsigned int x = 0U; x < 4U; ++x)
            grid.points.push_back(Point3(coordinates[x], coordinates[y], 0.0));
    for (unsigned int y = 0U; y < 3U; ++y)
        for (unsigned int x = 0U; x < 3U; ++x) {
            const unsigned int a = y * 4U + x, b = a + 1U, c = a + 4U, d = c + 1U;
            grid.triangles.push_back(Triangle(a, b, d));
            grid.triangles.push_back(Triangle(a, d, c));
        }
    const TriangleMesh original = grid;
    assert(remesh_detail::collapseShortEdges(grid, 0.2, 1000U, 1000U, 1.0e-40));
    assert(grid.points.size() == 15U && grid.triangles.size() == 16U);
    double area = 0.0;
    for (const Triangle& triangle : grid.triangles) {
        assert(remesh_detail::validTriangle(triangle, grid.points.size()));
        const Point3 normal = remesh_detail::triangleNormal(triangle, grid.points);
        assert(normal.z > 0.0);
        area += normal.z * 0.5;
    }
    assert(geometryNear(area, 4.0));
    std::map<remesh_detail::EdgeKey, unsigned int> edges;
    assert(remesh_detail::buildEdgeCounts(grid.triangles, edges));
    unsigned int boundaryEdges = 0U;
    for (const auto& edge : edges) {
        assert(edge.second <= 2U);
        if (edge.second == 1U) ++boundaryEdges;
    }
    assert(boundaryEdges == 12U);
    assert(grid.points.size() + grid.triangles.size() == edges.size() + 1U);
    for (const Point3& point : original.points) {
        if (point.x == 0.0 || point.x == 2.0 || point.y == 0.0 || point.y == 2.0) {
            bool found = false;
            for (const Point3& result : grid.points)
                if (squaredDistance(point, result) == 0.0) found = true;
            assert(found);
        }
    }
    for (double target : {0.15, 0.2, 0.3, 0.4, 0.5, 0.75, 1.0}) {
        TriangleMesh output;
        assert(isotropicRemesh(original, target, 1U, output, 10000U, 10000U, 1000000U));
        for (const Triangle& triangle : output.triangles) {
            assert(remesh_detail::validTriangle(triangle, output.points.size()));
            assert(remesh_detail::triangleNormal(triangle, output.points).z > 0.0);
        }
    }
    // Two disjoint short edges can be collapsed in the same pass.
    TriangleMesh two = original;
    for (const Point3& point : original.points) two.points.push_back(point + Point3(5.0, 0.0, 0.0));
    for (const Triangle& face : original.triangles)
        two.triangles.push_back(Triangle(face.first + 16U, face.second + 16U, face.third + 16U));
    assert(remesh_detail::collapseShortEdges(two, 0.2, 1000U, 1000U, 1.0e-40));
    assert(two.points.size() == 30U && two.triangles.size() == 32U);

    TriangleMesh tetrahedron;
    tetrahedron.points = {Point3(0,0,0), Point3(.1,0,0), Point3(0,2,0), Point3(0,0,2)};
    tetrahedron.triangles = {Triangle(0,2,1), Triangle(0,1,3), Triangle(1,2,3), Triangle(2,0,3)};
    const TriangleMesh tetraBefore = tetrahedron;
    assert(remesh_detail::collapseShortEdges(tetrahedron, 0.2, 1000U, 1000U, 1.0e-40));
    assert(sameTriangleMesh(tetrahedron, tetraBefore));

    TriangleMesh malformed = original;
    malformed.triangles[0].second = malformed.triangles[0].first;
    const TriangleMesh badBefore = malformed;
    assert(!remesh_detail::collapseShortEdges(malformed, 0.2, 1000U, 1000U, 1.0e-40));
    assert(sameTriangleMesh(malformed, badBefore));
    assert(!remesh_detail::normalizeMesh(malformed, 1000U, 1000U, 1.0e-40));
    malformed = original;
    malformed.triangles[0].first = malformed.points.size();
    assert(!remesh_detail::collapseShortEdges(malformed, 0.2, 1000U, 1000U, 1.0e-40));
    TriangleMesh unchanged = original;
    assert(!remesh_detail::collapseShortEdges(unchanged, 0.2, 3U, 1000U, 1.0e-40));
    assert(sameTriangleMesh(unchanged, original));
    // Invalid tolerances must also leave the original geometry unchanged.
    assert(!remesh_detail::collapseShortEdges(unchanged, 0.2, 1000U, 1000U,
                                             std::numeric_limits<double>::infinity()));
    assert(sameTriangleMesh(unchanged, original));

    // A valid contraction followed by failed candidate normalization must not
    // commit. This disconnected, finite-coordinate face overflows its area.
    TriangleMesh failedCandidate = original;
    failedCandidate.points.push_back(Point3(10.0, 0.0, 0.0));
    failedCandidate.points.push_back(Point3(1.0e200, 0.0, 0.0));
    failedCandidate.points.push_back(Point3(10.0, 1.0e200, 0.0));
    failedCandidate.triangles.push_back(Triangle(16U, 17U, 18U));
    const TriangleMesh candidateBefore = failedCandidate;
    assert(!remesh_detail::collapseShortEdges(failedCandidate, 0.2, 1000U, 1000U, 1.0e-40));
    assert(sameTriangleMesh(failedCandidate, candidateBefore));
}

void testSimultaneousSmoothing()
{
    using namespace dommy::geometry;
    const TriangleMesh input = simultaneousSmoothingFixture();
    for (const Triangle& triangle : input.triangles)
        assert(remesh_detail::triangleNormal(triangle, input.points).z > 0.0);
    TriangleMesh output;
    assert(isotropicRemesh(input, 0.5, 1U, output, 10000U, 10000U));
    assert(output.triangles.size() == 108U);
    for (const Triangle& triangle : output.triangles)
        assert(remesh_detail::triangleNormal(triangle, output.points).z > 0.0);

    TriangleMesh staged = input;
    const double areaTolerance = std::pow(4.0e-12, 4.0);
    assert(remesh_detail::normalizeMesh(staged, 10000U, 10000U, areaTolerance));
    assert(remesh_detail::splitLongEdges(staged, 0.5 * (4.0 / 3.0), 10000U, 10000U));
    assert(remesh_detail::collapseShortEdges(staged, 0.4, 10000U, 10000U, areaTolerance));
    assert(remesh_detail::flipEdges(staged, areaTolerance));
    const TriangleMesh before = staged;
    assert(remesh_detail::smoothTangentially(staged, 0.35, areaTolerance));
    for (std::size_t i = 0U; i < staged.triangles.size(); ++i)
        assert(remesh_detail::preservesTriangleOrientation(
            remesh_detail::triangleNormal(before.triangles[i], before.points),
            remesh_detail::triangleNormal(staged.triangles[i], staged.points), areaTolerance));
    assert(!sameTriangleMesh(staged, before)); // Backtracking still smooths.
}

void testEdgeFlipTopology()
{
    using namespace dommy::geometry;
    TriangleMesh tetrahedron;
    tetrahedron.points = {Point3(1, 2, 0), Point3(-1, -1, -1),
                          Point3(2, 0, 2), Point3(0, 2, -2)};
    tetrahedron.triangles = {Triangle(0, 2, 1), Triangle(0, 1, 3),
                             Triangle(1, 2, 3), Triangle(2, 0, 3)};
    const TriangleMesh before = tetrahedron;
    assert(remesh_detail::flipEdges(tetrahedron, 1.0e-40));
    assert(sameTriangleMesh(tetrahedron, before));
    for (double target : {1.0, 2.0, 3.0, 4.0, 5.0}) {
        for (unsigned int iterations : {1U, 2U, 3U}) {
            TriangleMesh output;
            assert(isotropicRemesh(before, target, iterations, output, 10000U, 10000U, 1000000U));
            std::map<remesh_detail::EdgeKey, unsigned int> edges;
            assert(remesh_detail::buildEdgeCounts(output.triangles, edges));
            for (const auto& edge : edges) assert(edge.second == 2U);
            assert(output.points.size() + output.triangles.size() == edges.size() + 2U);
            std::set<std::vector<std::size_t>> faces;
            for (const Triangle& triangle : output.triangles) {
                assert(remesh_detail::validTriangle(triangle, output.points.size()));
                std::vector<std::size_t> key = {triangle.first, triangle.second, triangle.third};
                std::sort(key.begin(), key.end());
                assert(faces.insert(key).second);
            }
        }
    }

    // A valid quality-improving diagonal change on an open quad still occurs.
    TriangleMesh quad;
    quad.points = {Point3(0, 0, 0), Point3(3, 0, 0), Point3(2, 1, 0), Point3(0, 3, 0)};
    quad.triangles = {Triangle(0, 1, 3), Triangle(1, 2, 3)};
    const double quality = std::min(remesh_detail::triangleQuality(quad.triangles[0], quad.points),
                                    remesh_detail::triangleQuality(quad.triangles[1], quad.points));
    assert(remesh_detail::flipEdges(quad, 1.0e-40));
    std::map<remesh_detail::EdgeKey, unsigned int> edges;
    assert(remesh_detail::buildEdgeCounts(quad.triangles, edges));
    assert(edges.count(remesh_detail::EdgeKey(1U, 3U)) == 0U);
    assert(edges[remesh_detail::EdgeKey(0U, 2U)] == 2U);
    for (const Triangle& triangle : quad.triangles) {
        assert(remesh_detail::triangleNormal(triangle, quad.points).z > 0.0);
        assert(remesh_detail::triangleQuality(triangle, quad.points) > quality);
    }
}

} // namespace

void runGeometryTests()
{
    testPolygonTriangulation();
    testTrianglePlaneSlicer();
    testPointSetRegularizer();
    testIsotropicRemesher();
    testTranslatedIsotropicRemesher();
    testUnusedRemeshVertices();
    testShortEdgeCollapse();
    testSimultaneousSmoothing();
    testEdgeFlipTopology();
}
