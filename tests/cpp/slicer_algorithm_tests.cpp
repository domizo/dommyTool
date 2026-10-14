#include "geometry/DTrianglePlaneSlicer.h"
#include "geometry/DPointSetRegularizer.h"
#include "remesh_regression_fixture.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <vector>

using namespace dommy::geometry;
using dommy::generation::WorkBudget;

namespace {

struct Mesh {
    std::vector<Point3> points;
    std::vector<Triangle> triangles;
};

bool same(const std::vector<Polyline3>& first, const std::vector<Polyline3>& second)
{
    if (first.size() != second.size()) return false;
    for (std::size_t i = 0U; i < first.size(); ++i) {
        if (first[i].size() != second[i].size()) return false;
        for (std::size_t j = 0U; j < first[i].size(); ++j)
            for (std::size_t axis = 0U; axis < 3U; ++axis)
                if (first[i][j][axis] != second[i][j][axis]) return false;
    }
    return true;
}

Mesh star(std::size_t count)
{
    Mesh mesh;
    mesh.points.push_back(Point3());
    for (std::size_t i = 0U; i < count; ++i) {
        const double angle = double(i) * 6.28318530717958647692 / double(count);
        const double x = static_cast<float>(std::cos(angle));
        const double y = static_cast<float>(std::sin(angle));
        mesh.points.push_back(Point3(x, y, 1));
        mesh.points.push_back(Point3(x, y, -1));
        mesh.triangles.push_back(Triangle(0, 2U*i + 1U, 2U*i + 2U));
    }
    return mesh;
}

bool slice(const Mesh& mesh, std::vector<Polyline3>& result, WorkBudget& budget,
           std::size_t pointLimit = 10000000U)
{
    return sliceTriangleMesh(mesh.points, mesh.triangles, Plane3(Point3(0, 0, 1), 0),
                             result, 2000000U, pointLimit, budget);
}

void linearBranching()
{
    std::size_t previousWork = 0U;
    for (std::size_t count : {1000U, 2000U, 4000U, 8000U, 16000U}) {
        const Mesh mesh = star(count);
        WorkBudget budget;
        std::vector<Polyline3> result;
        assert(slice(mesh, result, budget));
        assert(result.size() == count && budget.used() < 2500U * count);
        if (previousWork != 0U) assert(double(budget.used()) / double(previousWork) < 2.5);
        previousWork = budget.used();
        for (std::size_t i = 0U; i < count; ++i) {
            assert(result[i].size() == 2U);
            assert(result[i][0].x == 0 && result[i][0].y == 0 && result[i][0].z == 0);
            assert(result[i][1].x == mesh.points[2U*i + 1U].x);
            assert(result[i][1].y == mesh.points[2U*i + 1U].y && result[i][1].z == 0);
        }
        std::cout << "Branching slice " << count << ": " << budget.used() << " bounded work units\n";
    }
}

void graphOrderAndCursors()
{
    using namespace slicer_detail;
    // Two branch points joined through degree-two vertices; a second path,
    // a branch-to-leaf path and a disconnected cycle. IDs deliberately make
    // incoming neighbors interleave with outgoing neighbors.
    const std::set<EdgeKey> edges = {{0, 2}, {2, 5}, {0, 3}, {3, 5},
                                    {0, 6}, {5, 7}, {1, 4}, {4, 8}, {1, 8}};
    std::vector<Point3> points;
    for (std::size_t i = 0U; i < 9U; ++i) points.push_back(Point3(double(i), 0, 0));
    std::vector<std::vector<std::size_t> > adjacency(points.size());
    for (const auto& edge : edges) {
        adjacency[edge.first].push_back(edge.second);
        adjacency[edge.second].push_back(edge.first);
    }
    for (const auto& neighbors : adjacency) assert(std::is_sorted(neighbors.begin(), neighbors.end()));
    auto unused = edges;
    std::vector<std::size_t> cursors(points.size(), 0U);
    WorkBudget budget;
    std::vector<Polyline3> result;
    for (std::size_t start = 0U; start < adjacency.size(); ++start) {
        if (adjacency[start].size() == 2U) continue;
        for (;;) {
            std::size_t next = 0U;
            assert(nextUnusedNeighbor(start, adjacency, unused, cursors, budget, next));
            if (next == std::numeric_limits<std::size_t>::max()) break;
            Polyline3 line;
            assert(tracePolyline(start, next, points, adjacency, unused, cursors, budget, 100U, line));
            result.push_back(line);
        }
    }
    while (!unused.empty()) {
        const auto edge = *unused.begin();
        Polyline3 line;
        assert(tracePolyline(edge.first, edge.second, points, adjacency, unused, cursors, budget, 100U, line));
        result.push_back(line);
    }
    const std::vector<std::vector<std::size_t> > expected{{0, 2, 5}, {0, 3, 5},
                                                        {0, 6}, {5, 7}, {1, 4, 8, 1}};
    assert(result.size() == expected.size());
    for (std::size_t i = 0U; i < result.size(); ++i) {
        assert(result[i].size() == expected[i].size());
        for (std::size_t j = 0U; j < result[i].size(); ++j) assert(result[i][j].x == expected[i][j]);
    }
    std::size_t visits = 0U;
    for (std::size_t i = 0U; i < cursors.size(); ++i) {
        assert(cursors[i] <= adjacency[i].size());
        visits += cursors[i];
    }
    assert(visits <= 2U * edges.size());
}

void exactBudgetsAndRollback()
{
    const Mesh mesh = star(1U);
    WorkBudget measured;
    std::vector<Polyline3> expected;
    assert(slice(mesh, expected, measured));
    const std::size_t work = measured.used();
    assert(work == 374U);
    const std::vector<Polyline3> before{{Point3(7, 8, 9), Point3(10, 11, 12)}};
    for (std::size_t limit = 0U; limit < work; ++limit) {
        auto result = before;
        WorkBudget budget(limit);
        assert(!slice(mesh, result, budget));
        assert(budget.exceeded() && budget.used() <= limit && same(result, before));
        assert(!budget.consume(0U));
    }
    WorkBudget exact(work);
    auto result = before;
    assert(slice(mesh, result, exact) && exact.remaining() == 0U && same(result, expected));
    assert(!slice(mesh, result, exact) && same(result, expected) && exact.exceeded());
    WorkBudget twoSlices(2U*work);
    assert(slice(mesh, result, twoSlices) && slice(mesh, result, twoSlices));
    assert(twoSlices.remaining() == 0U && !twoSlices.exceeded());
    assert(!slice(mesh, result, twoSlices) && same(result, expected));
    WorkBudget tooFewPoints;
    result = before;
    assert(!slice(star(3U), result, tooFewPoints, 5U) && same(result, before));
    WorkBudget invalidBudget;
    Mesh invalid = mesh;
    invalid.triangles[0].third = invalid.points.size();
    assert(!slice(invalid, result, invalidBudget) && same(result, before));
    WorkBudget missed;
    assert(sliceTriangleMesh(mesh.points, mesh.triangles, Plane3(Point3(0, 0, 1), 100),
                             result, 100U, 100U, missed) && result.empty());
    assert(missed.used() == 168U);
    WorkBudget overflow(10U);
    assert(!overflow.consumeProduct(std::numeric_limits<std::size_t>::max(), 2U));
    assert(overflow.exceeded() && !overflow.consume(0U));
    std::cout << "Single segment: " << work << " exact work units; all exhaustion boundaries preserve output\n";
}

void integerBoundaries()
{
    const double edge = std::ldexp(1.0, 63);
    for (double value : {edge, -edge, std::nextafter(edge, INFINITY),
                         std::nextafter(-edge, -INFINITY), double(NAN), double(INFINITY)}) {
        long long result = 42;
        assert(!slicer_detail::quantize(value, 1, result) && result == 42);
        assert(!regularizer_detail::quantize(value, 1, result) && result == 42);
        WorkBudget budget;
        slicer_detail::PointWelder welder(1, 10U, budget);
        std::size_t index = 77U;
        assert(!welder.findOrInsert(Point3(value, 0, 0), index) && index == 77U);
        assert(welder.points().empty());
    }
    for (double value : {std::nextafter(edge, 0.0), std::nextafter(-edge, 0.0)}) {
        long long result = 0;
        assert(slicer_detail::quantize(value, 1, result));
        assert(result >= std::numeric_limits<long long>::min() + 2LL &&
               result <= std::numeric_limits<long long>::max() - 2LL);
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            WorkBudget budget;
            slicer_detail::PointWelder welder(1, 10U, budget);
            Point3 point; point[axis] = value;
            std::size_t index = 77U;
            assert(welder.findOrInsert(point, index) && index == 0U);
            assert(welder.findOrInsert(point, index) && welder.points().size() == 1U);
        }
    }
    for (double inverse : {0.0, -1.0, double(NAN), double(INFINITY)}) {
        long long result = 42;
        assert(!slicer_detail::quantize(1, inverse, result) && result == 42);
    }
    long long result = 42;
    assert(slicer_detail::quantize(-0.25, 1, result) && result == -1);
    assert(slicer_detail::quantize(2.25, 2, result) && result == 4);
    assert(!slicer_detail::quantize(std::numeric_limits<double>::max(), 2, result) && result == 4);
}

void unusedVertices()
{
    TriangleMesh input;
    input.points = {{0,0,0}, {1,0,0}, {0,1,0}};
    input.triangles = {{0,1,2}};
    const Plane3 plane(Point3(0,1,0), -0.5);
    std::vector<Polyline3> expected;
    assert(sliceTriangleMesh(input.points, input.triangles, plane, expected, 100U, 100U));
    assert(expected.size() == 1U && expected[0].size() == 2U);
    for (double value : {double(1e12f), double(-1e30f),
                         std::numeric_limits<double>::max(),
                         -std::numeric_limits<double>::max()}) {
        for (std::size_t position : {0U, 1U, 3U}) {
            const auto padded = withUnusedVertex(input, position, Point3(value, value, value));
            std::vector<Polyline3> result;
            assert(sliceTriangleMesh(padded.points, padded.triangles, plane, result, 100U, 100U));
            assert(same(result, expected));
        }
    }
    auto invalid = withUnusedVertex(input, 0U, Point3(NAN, 0, 0));
    auto result = expected;
    assert(!sliceTriangleMesh(invalid.points, invalid.triangles, plane, result, 100U, 100U));
    assert(same(result, expected));
    invalid = input;
    invalid.triangles[0].first = invalid.points.size();
    assert(!sliceTriangleMesh(invalid.points, invalid.triangles, plane, result, 100U, 100U));
    assert(same(result, expected));
    std::cout << "Unused vertices do not alter slice tolerances, key origins or contour order\n";
}

void duplicatesAndClosure()
{
    Mesh mesh;
    mesh.points = {{0, 0, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1},
                   {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    mesh.triangles = {{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
                      {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    WorkBudget budget;
    std::vector<Polyline3> expected;
    assert(slice(mesh, expected, budget) && expected.size() == 1U && expected[0].size() == 9U);
    assert(squaredDistance(expected[0].front(), expected[0].back()) == 0);
    const auto original = mesh.triangles;
    mesh.triangles.insert(mesh.triangles.end(), original.begin(), original.end());
    WorkBudget duplicateBudget;
    std::vector<Polyline3> duplicates;
    assert(slice(mesh, duplicates, duplicateBudget) && same(expected, duplicates));
    std::vector<Polyline3> compatibility;
    assert(sliceTriangleMesh(mesh.points, mesh.triangles, Plane3(Point3(0, 0, 1), 0),
                             compatibility, 100U, 100U) && same(expected, compatibility));
}

void translatedContours()
{
    for (double x : {0.0, double(1.0e12f), double(-1.0e12f), double(1.0e30f)}) {
        const std::vector<Point3> points{{x, 0, 0}, {x, 1, 0}, {x, 0, 1}};
        const std::vector<Triangle> triangles{{0, 1, 2}};
        for (bool anchored : {false, true}) {
            Plane3 plane(Point3(0, 1, 0), -0.5);
            if (anchored) assert(makePlane(Point3(x, .25, .25), Point3(1, 1, 1), plane));
            std::vector<Polyline3> result;
            WorkBudget budget;
            assert(sliceTriangleMesh(points, triangles, plane, result, 10, 10, budget));
            assert(result.size() == 1U && result[0].size() == 2U);
            for (const Point3& point : result[0]) {
                assert(point.x == x);
                assert(std::fabs((anchored ? point.y + point.z : point.y) - .5) < 1e-12);
            }
            if (anchored) {
                assert(std::fabs(slicer_detail::signedDistance(plane, Point3(x, .25, .25))) < 1e-15);
                plane.origin.x = NAN;
                const auto previous = result;
                WorkBudget invalid;
                assert(!sliceTriangleMesh(points, triangles, plane, result, 10, 10, invalid));
                assert(same(result, previous));
            }
        }
    }
    // The spatial origin is only a key frame; stored points stay unchanged.
    WorkBudget budget;
    const Point3 origin(double(1e30f), 0, 0);
    slicer_detail::PointWelder welder(1e-9, 10, budget, origin);
    std::size_t first = 0U, second = 0U;
    assert(welder.findOrInsert(origin, first));
    assert(welder.findOrInsert(origin + Point3(0, 1, 0), second));
    assert(first != second && welder.points()[first].x == origin.x);
    std::cout << "Translated remesh/slice predicates and local welding retain small float-representable geometry\n";
}

void independentComponentsAndEquations()
{
    for (double separation : {double(1e12f), double(-1e12f), double(1e30f), 5e-10}) {
        const auto mesh = separatedTrianglesFixture(separation);
        const Plane3 plane({0,1,0}, -0.5);
        std::vector<Polyline3> result;
        WorkBudget budget;
        assert(sliceTriangleMesh(mesh.points, mesh.triangles, plane, result, 10U, 10U, budget));
        assert(result.size() == 2U && result[0].size() == 2U && result[1].size() == 2U);
        for (std::size_t i = 0U; i < 2U; ++i)
            for (const auto& point : result[i])
                assert(point.x == (i == 0U ? 0.0 : separation) && point.y == .5);
        const auto before = result;
        WorkBudget tooSmall(budget.used() - 1U);
        assert(!sliceTriangleMesh(mesh.points, mesh.triangles, plane, result, 10U, 10U, tooSmall));
        assert(tooSmall.exceeded() && same(result, before));
        assert(!sliceTriangleMesh(mesh.points, mesh.triangles, plane, result, 10U, 3U));
        assert(same(result, before));
    }
    for (double x : {0.0, double(1e12f), double(-1e12f), double(1e30f), double(-1e30f)}) {
        const std::vector<Point3> points{{x,0,0}, {x,1,0}, {x,0,1}};
        for (Point3 normal : {Point3(1,1,0), Point3(-1,2,0), Point3(2,2,0)}) {
            const Plane3 equation(normal, -normal.x * x);
            Plane3 anchored;
            assert(makePlane({x,0,0}, normal, anchored));
            std::vector<Polyline3> expected, result;
            assert(sliceTriangleMesh(points, {{0,1,2}}, anchored, expected, 10U, 10U));
            assert(expected.size() == 1U);
            assert(sliceTriangleMesh(points, {{0,1,2}}, equation, result, 10U, 10U));
            assert(same(result, expected));
            for (const auto& point : result[0]) assert(point.y == 0.0);
        }
    }
    // Exact-coordinate connectivity preserves contours for unindexed soups.
    Mesh soup;
    soup.points = {{0,0,-1}, {1,0,1}, {0,0,1}, {0,0,-1}, {1,0,-1}, {1,0,1}};
    soup.triangles = {{0,1,2}, {3,4,5}};
    WorkBudget budget;
    std::vector<Polyline3> result;
    assert(slice(soup, result, budget) && result.size() == 1U && result[0].size() == 3U);
    Mesh padded = soup;
    padded.points.insert(padded.points.end(), {{1e12,0,0}, {1e12,1,0}, {1e12,2,0}});
    padded.triangles.push_back({6,7,8});
    std::vector<Polyline3> withoutDegenerate;
    WorkBudget other;
    assert(slice(padded, withoutDegenerate, other) && same(result, withoutDegenerate));
    // A later missed component must not fail just because the first used all CVs.
    const auto mesh = separatedTrianglesFixture(10.0);
    assert(sliceTriangleMesh(mesh.points, mesh.triangles, Plane3({1,1,0}, -.5),
                             result, 10U, 2U) && result.size() == 1U);
    std::cout << "Component-local slice frames and original equation evaluation preserve distant geometry\n";
}

} // namespace

int main()
{
    linearBranching();
    graphOrderAndCursors();
    exactBudgetsAndRollback();
    integerBoundaries();
    duplicatesAndClosure();
    translatedContours();
    unusedVertices();
    independentComponentsAndEquations();
    std::cout << "Slicer contours, shared work and checked spatial keys passed\n";
}
