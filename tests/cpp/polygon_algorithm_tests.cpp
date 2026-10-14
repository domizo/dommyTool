#include "geometry/DPolygonTriangulator.h"
#include "remesh_regression_fixture.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace dommy::geometry;

namespace {

bool same(const PolygonTriangulation& first, const PolygonTriangulation& second)
{
    if (first.points.size() != second.points.size() ||
        first.sourceIndices != second.sourceIndices ||
        first.triangles.size() != second.triangles.size()) return false;
    for (std::size_t i = 0; i < first.points.size(); ++i)
        if (first.points[i].x != second.points[i].x || first.points[i].y != second.points[i].y)
            return false;
    for (std::size_t i = 0; i < first.triangles.size(); ++i)
        for (std::size_t corner = 0; corner < 3; ++corner)
            if (first.triangles[i][corner] != second.triangles[i][corner]) return false;
    return true;
}

std::vector<Point2> square(double edge = 1.0, double offset = 0.0)
{
    return {{offset, offset}, {offset + edge, offset},
            {offset + edge, offset + edge}, {offset, offset + edge}};
}

PolygonTriangulation seeded()
{
    PolygonTriangulation result;
    assert(triangulateSimplePolygon(square(), result, 100U, 100U, 10000U));
    return result;
}

void budgetBoundaries()
{
    PolygonWorkBudget overflow(100U);
    assert(overflow.consume(3U));
    assert(!overflow.consumeProduct(std::numeric_limits<std::size_t>::max(), 2U));
    assert(overflow.exceeded() && overflow.used() == 3U && !overflow.consume(0U));
    const std::vector<Point2> triangle = {{0, 0}, {1, 0}, {0, 1}};
    const PolygonTriangulation before = seeded();
    // Freeze the documented visit/copy model for this minimal fixture.
    for (std::size_t limit = 0U; limit < 28U; ++limit) {
        PolygonWorkBudget budget(limit);
        PolygonTriangulation output = before;
        assert(!triangulateSimplePolygon(triangle, output, 100U, 100U, budget));
        assert(budget.exceeded() && budget.used() <= limit && same(output, before));
    }
    PolygonWorkBudget exact(28U);
    PolygonTriangulation output;
    assert(triangulateSimplePolygon(triangle, output, 100U, 100U, exact));
    assert(exact.used() == 28U && exact.remaining() == 0U && !exact.exceeded());
    assert(!exact.consume(1U) && !exact.consume(0U));

    PolygonWorkBudget shared(118U);
    assert(triangulateSimplePolygon(square(), output, 100U, 100U, shared));
    assert(shared.used() == 68U);
    std::vector<std::vector<std::size_t> > partitions{{99U}};
    assert(!convexPartition(output, partitions, shared));
    assert(shared.exceeded() && partitions == std::vector<std::vector<std::size_t> >({{99U}}));
    PolygonWorkBudget enough(119U);
    assert(triangulateSimplePolygon(square(), output, 100U, 100U, enough));
    assert(convexPartition(output, partitions, enough));
    assert(enough.used() == 119U && partitions.size() == 1U && partitions[0].size() == 4U);
}

void linearCleanup()
{
    std::vector<Point2> polygon;
    const std::size_t count = 100000U;
    polygon.reserve(count + 1U);
    for (std::size_t i = 0U; i < count; ++i) polygon.push_back(Point2(double(i), 0));
    polygon.push_back(Point2(0, 1));
    PolygonTriangulation output = seeded(), before = output;
    PolygonWorkBudget tiny(polygon.size() + 1U);
    assert(!triangulateSimplePolygon(polygon, output, count + 1U, 100U, tiny));
    assert(tiny.exceeded() && same(output, before));
    PolygonWorkBudget budget(16U * polygon.size());
    assert(triangulateSimplePolygon(polygon, output, count + 1U, 100U, budget));
    assert(output.points.size() == 3U && output.triangles.size() == 1U);
    assert(output.sourceIndices == std::vector<std::size_t>({0U, count - 1U, count}));
    assert(budget.used() >= 11U * polygon.size() && budget.used() <= 13U * polygon.size() + 32U);
    std::cout << "100001-vertex cleanup: " << budget.used() << " bounded work units\n";

    std::vector<Point2> rectangle;
    const std::size_t side = 1000U;
    for (std::size_t i = 0U; i < side; ++i) rectangle.push_back(Point2(double(i), 0));
    for (std::size_t i = 0U; i < side; ++i) rectangle.push_back(Point2(double(side), double(i)));
    for (std::size_t i = 0U; i < side; ++i)
        rectangle.push_back(Point2(double(side - i), double(side)));
    for (std::size_t i = 0U; i < side; ++i)
        rectangle.push_back(Point2(0, double(side - i)));
    PolygonWorkBudget cyclic(16U * rectangle.size());
    assert(triangulateSimplePolygon(rectangle, output, rectangle.size(), 100U, cyclic));
    assert(output.points.size() == 4U && output.triangles.size() == 2U);
    assert(output.sourceIndices == std::vector<std::size_t>({0U, side, 2U*side, 3U*side}));
    assert(cyclic.used() <= 15U * rectangle.size() + 50U);
}

void scaleAndTranslation()
{
    for (double edge : {1.0e-9, 1.0e-6, 1.0, 1.0e6}) {
        for (double offset : {0.0, edge * 1000.0}) {
            for (bool reverse : {false, true}) {
                auto polygon = square(edge, offset);
                if (reverse) std::reverse(polygon.begin(), polygon.end());
                polygon.push_back(polygon.front());
                PolygonTriangulation result;
                assert(triangulateSimplePolygon(polygon, result, 100U, 100U, 10000U));
                assert(result.points.size() == 4U && result.triangles.size() == 2U);
                double area = 0;
                for (const Triangle& triangle : result.triangles) {
                    const double cross = cross2(result.points[triangle.first],
                                                result.points[triangle.second],
                                                result.points[triangle.third]);
                    assert(cross > 0);
                    area += cross * 0.5;
                }
                assert(std::fabs(area / (edge * edge) - 1.0) < 1.0e-9);
                for (std::size_t i = 0U; i < result.points.size(); ++i) {
                    assert(result.sourceIndices[i] < polygon.size());
                    const Point2& source = polygon[result.sourceIndices[i]];
                    assert(source.x == result.points[i].x && source.y == result.points[i].y);
                }
                std::vector<std::vector<std::size_t> > partitions;
                assert(convexPartition(result, partitions, 10000U));
                assert(partitions.size() == 1U && partitions.front().size() == 4U);
            }
        }
    }
    PolygonTriangulation translated;
    assert(triangulateSimplePolygon(square(1.0, 1.0e12), translated, 100U, 100U, 10000U));
    for (double edge : {1.0e-6, 1.0}) {
        const std::vector<Point2> polygon = {{0, 0}, {3*edge, 0}, {3*edge, edge},
                                           {edge, edge}, {edge, 3*edge}, {0, 3*edge}};
        PolygonTriangulation result;
        assert(triangulateSimplePolygon(polygon, result, 100U, 100U, 10000U));
        assert(result.triangles.size() == 4U);
    }
    const auto before = seeded();
    for (const auto& invalid : {
            square(1.0e-11),
            std::vector<Point2>{{0, 0}, {1, 1}, {0, 1}, {1, 0}},
            std::vector<Point2>{{0, 0}, {1, 0}, {2, 0}},
            std::vector<Point2>{{0, 0}, {1, 0}, {0, NAN}}}) {
        auto result = before;
        assert(!triangulateSimplePolygon(invalid, result, 100U, 100U, 10000U));
        assert(same(result, before));
    }
    PolygonTriangulation smaller;
    assert(triangulateSimplePolygon(square(1.0e-12), smaller, 100U, 100U, 10000U, 1.0e-14));
}

void projectionAndStages()
{
    for (double edge : {1.0e-9, 1.0e-6, 1.0, 1.0e6}) {
        std::vector<Point3> input{{0, 0, 2}, {edge, 0, 2}, {edge, edge, 2}, {0, edge, 2}};
        PlaneFrame3 frame;
        std::vector<Point2> projected;
        PolygonWorkBudget budget;
        assert(projectPlanarPolygon(input, frame, projected, budget));
        assert(frame.normal.z == 1.0);
        for (std::size_t i = 0; i < input.size(); ++i) {
            const Point3 point = unprojectPoint(frame, projected[i]);
            assert(std::fabs(point.x - input[i].x) <= edge * 1.0e-12);
            assert(std::fabs(point.y - input[i].y) <= edge * 1.0e-12 && point.z == 2);
        }
    }
    PlaneFrame3 frame{{4, 5, 6}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    std::vector<Point2> projected{{7, 8}};
    PolygonWorkBudget zero(0U);
    assert(!projectPlanarPolygon({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, frame, projected, zero));
    assert(frame.origin.x == 4 && projected[0].x == 7 && projected.size() == 1U);
    PolygonWorkBudget valid;
    assert(!projectPlanarPolygon({{0, 0, 0}, {1, 0, 0}, {0, NAN, 0}}, frame, projected, valid));
    assert(frame.origin.x == 4 && projected[0].x == 7);

    const auto original = seeded();
    PolygonWorkBudget exhausted(0U);
    assert(!exhausted.consume(1U));
    auto noIterations = original;
    assert(!relaxInteriorVertices(noIterations, 0U, exhausted) && same(noIterations, original));
    PolygonWorkBudget complete;
    auto refined = original;
    assert(refineTriangulation(refined, 0.8, 1U, 1000U, 1000U, complete));
    const std::size_t refinementWork = complete.used();
    for (std::size_t limit = 0U; limit < refinementWork; ++limit) {
        PolygonWorkBudget budget(limit);
        auto rejected = original;
        assert(!refineTriangulation(rejected, 0.8, 1U, 1000U, 1000U, budget));
        assert(budget.exceeded() && same(rejected, original));
    }
    PolygonWorkBudget relaxation;
    auto relaxed = refined;
    assert(relaxInteriorVertices(relaxed, 2U, relaxation));
    for (std::size_t limit = 0U; limit < relaxation.used(); ++limit) {
        PolygonWorkBudget budget(limit);
        auto rejected = refined;
        assert(!relaxInteriorVertices(rejected, 2U, budget));
        assert(budget.exceeded() && same(rejected, refined));
    }
}

void simultaneousFaceRelaxation()
{
    PolygonTriangulation original;
    PolygonWorkBudget preparation;
    assert(triangulateSimplePolygon(faceSmoothingPolygonFixture(), original,
                                    1000U, 1000U, preparation));
    assert(refineTriangulation(original, 0.4, 16U, 1000U, 1000U, preparation));
    assert(original.points.size() == 85U && original.triangles.size() == 128U);
    auto relaxed = original;
    PolygonWorkBudget budget;
    assert(relaxInteriorVertices(relaxed, 3U, budget, 0.75));
    for (const Triangle& triangle : relaxed.triangles)
        assert(cross2(relaxed.points[triangle.first], relaxed.points[triangle.second],
                      relaxed.points[triangle.third]) > 0.0);
    PolygonWorkBudget shortBudget(budget.used() - 1U);
    auto rejected = original;
    assert(!relaxInteriorVertices(rejected, 3U, shortBudget, 0.75));
    assert(shortBudget.exceeded() && same(rejected, original));
    auto invalid = original;
    std::swap(invalid.triangles[0].first, invalid.triangles[0].second);
    const auto before = invalid;
    PolygonWorkBudget valid;
    assert(!relaxInteriorVertices(invalid, 1U, valid, 0.75) && same(invalid, before));
    std::cout << "Face relaxation validates the complete batch, backtracks and preserves output on failure\n";
}

void polygonStorageAdmission()
{
    const std::size_t limit = dommy::mesh::kMaxMeshOperationBytes;
    std::size_t bytes = 0U;
    assert(triangleWorkspaceBytes(3U, 1U, bytes) && bytes == 1408U);
    const auto before = seeded();
    const std::vector<Point2> triangle{{0,0}, {1,0}, {0,1}};
    PolygonWorkBudget exact;
    auto output = before;
    assert(triangulateSimplePolygon(triangle, output, 100U, 100U, exact,
                                    1.0e-10, limit - bytes));
    PolygonWorkBudget rejectedBudget;
    output = before;
    assert(!triangulateSimplePolygon(triangle, output, 100U, 100U, rejectedBudget,
                                     1.0e-10, limit - bytes + 1U));
    assert(rejectedBudget.used() == 0U && same(output, before));

    assert(triangleWorkspaceBytes(10U, 8U, bytes) && bytes == 6784U);
    auto refined = before;
    PolygonWorkBudget enough;
    assert(refineTriangulation(refined, 0.8, 1U, 1000U, 1000U, enough, limit - bytes));
    refined = before;
    PolygonWorkBudget tooLarge;
    assert(!refineTriangulation(refined, 0.8, 1U, 1000U, 1000U, tooLarge, limit - bytes + 1U));
    assert(same(refined, before) && !tooLarge.exceeded());

    PolygonWorkBudget relaxBudget;
    output = before;
    assert(!relaxInteriorVertices(output, 1U, relaxBudget, 0.5, limit));
    assert(relaxBudget.used() == 0U && same(output, before));
    PlaneFrame3 frame{{4,5,6}, {1,0,0}, {0,1,0}, {0,0,1}};
    std::vector<Point2> projected{{7,8}};
    PolygonWorkBudget projectBudget;
    assert(!projectPlanarPolygon({{0,0,0}, {1,0,0}, {0,1,0}}, frame, projected,
                                 projectBudget, limit));
    assert(projectBudget.used() == 0U && frame.origin.x == 4 && projected.size() == 1U);

    dommy::mesh::MeshMergePlan plan;
    assert(dommy::mesh::appendMeshMergePlan(3U, 1U, 3U, 0U, plan));
    assert(dommy::mesh::meshOperationStorageBytes(plan, bytes) && bytes == 640U);
    bytes = 99U;
    assert(!dommy::mesh::meshOperationStorageBytes(plan, bytes, limit));
    assert(bytes == 99U);
    plan.points = std::numeric_limits<std::size_t>::max();
    assert(!dommy::mesh::meshOperationStorageBytes(plan, bytes) && bytes == 99U);
    std::cout << "Polygon projection/refinement/relaxation admit retained storage before geometry allocation\n";
}

void planarProjectionContract()
{
    for (double scale : {1.0e-9, 1.0, 1.0e9}) {
        for (double translation : {0.0, 1000.0 * scale}) {
            const std::vector<Point3> warped{{translation,0,0}, {translation+scale,0,0},
                {translation+scale,scale,scale}, {translation,scale,0}};
            PlaneFrame3 frame{{4,5,6}, {1,0,0}, {0,1,0}, {0,0,1}};
            std::vector<Point2> projected{{7,8}};
            PolygonWorkBudget budget;
            assert(!projectPlanarPolygon(warped, frame, projected, budget));
            assert(frame.origin.x == 4 && frame.normal.z == 1 &&
                   projected.size() == 1U && projected[0].x == 7);
            auto planar = warped;
            for (auto& p : planar) p.z = (p.x-translation) + p.y;
            assert(projectPlanarPolygon(planar, frame, projected, budget));
        }
    }
    // Float quantization can leave a tiny residual on an intended plane.
    std::vector<Point3> rounded{{0,0,0}, {double(.2f),0,double(.2f)},
        {double(.2f),double(.1f),double(.3f)}, {0,double(.1f),double(.1f)}};
    PlaneFrame3 frame; std::vector<Point2> projected;
    PolygonWorkBudget budget;
    assert(projectPlanarPolygon(rounded, frame, projected, budget));
    rounded[2].z += .001;
    assert(!projectPlanarPolygon(rounded, frame, projected, budget));
    std::cout << "Projection rejects warped faces transactionally using an extent-relative plane residual\n";
}

} // namespace

int main()
{
    budgetBoundaries();
    linearCleanup();
    scaleAndTranslation();
    projectionAndStages();
    simultaneousFaceRelaxation();
    polygonStorageAdmission();
    planarProjectionContract();
    std::cout << "Polygon budgets, dimensional tolerances and transactional stages passed\n";
}
