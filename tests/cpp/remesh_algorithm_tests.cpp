#include "remesh_regression_fixture.h"

#include <cassert>
#include <iostream>

using namespace dommy::geometry;
using dommy::generation::WorkBudget;

namespace {

bool same(const TriangleMesh& a, const TriangleMesh& b)
{
    if (a.points.size() != b.points.size() || a.triangles.size() != b.triangles.size()) return false;
    for (std::size_t i = 0U; i < a.points.size(); ++i)
        for (std::size_t axis = 0U; axis < 3U; ++axis)
            if (a.points[i][axis] != b.points[i][axis]) return false;
    for (std::size_t i = 0U; i < a.triangles.size(); ++i)
        for (std::size_t corner = 0U; corner < 3U; ++corner)
            if (a.triangles[i][corner] != b.triangles[i][corner]) return false;
    return true;
}

TriangleMesh triangle()
{
    TriangleMesh mesh;
    mesh.points = {{0,0,0}, {1,0,0}, {0,1,0}};
    mesh.triangles = {{0,1,2}};
    return mesh;
}

bool remesh(const TriangleMesh& input, TriangleMesh& output, WorkBudget& budget,
            double target = 2.0, unsigned int iterations = 1U)
{
    return isotropicRemesh(input, target, iterations, output, 300000U, 100000U, budget);
}

void exactBudgets()
{
    const auto input = triangle();
    TriangleMesh expected;
    WorkBudget measured;
    assert(remesh(input, expected, measured));
    const std::size_t work = measured.used();
    std::cout << "Single triangle remesh uses " << work << " logical work units" << std::endl;
    // Frozen after inspection, not a dynamically accepted accounting oracle.
    assert(work == 387U);
    TriangleMesh before;
    before.points = {{7,8,9}};
    for (std::size_t limit = 0U; limit < work; ++limit) {
        WorkBudget budget(limit);
        auto output = before;
        assert(!remesh(input, output, budget));
        assert(budget.exceeded() && budget.used() <= limit && same(output, before));
        assert(!budget.consume(0U));
    }
    WorkBudget exact(work);
    auto output = before;
    assert(remesh(input, output, exact) && exact.remaining() == 0U && same(output, expected));
    assert(!remesh(input, output, exact) && same(output, expected) && exact.exceeded());
    WorkBudget shared(2U * work);
    assert(remesh(input, output, shared) && remesh(input, output, shared));
    assert(shared.remaining() == 0U && !shared.exceeded());
    assert(!remesh(input, output, shared) && same(output, expected));

    auto padded = input;
    padded.points.resize(250000U);
    WorkBudget tiny(16U);
    output = before;
    assert(!remesh(padded, output, tiny) && tiny.exceeded() && same(output, before));
    WorkBudget sufficient;
    assert(remesh(padded, output, sufficient) && same(output, expected));
    assert(sufficient.used() > 250000U); // No uncharged copy/compaction scan.
}

void smoothingRollback()
{
    const auto input = simultaneousSmoothingFixture();
    TriangleMesh output;
    WorkBudget measured;
    assert(remesh(input, output, measured, 0.5));
    assert(output.triangles.size() == 108U);
    for (const auto& face : output.triangles)
        assert(remesh_detail::triangleNormal(face, output.points).z > 0.0);
    for (unsigned int iterations : {1U, 2U, 3U}) {
        WorkBudget ample;
        assert(remesh(input, output, ample, 0.5, iterations));
        for (const auto& face : output.triangles)
            assert(remesh_detail::triangleNormal(face, output.points).z > 0.0);
    }
    // Exhaustion at each pipeline checkpoint, including final restore/copy,
    // must leave the public destination intact.
    const std::size_t limits[] = {1U, 100U, 1000U, 5000U, measured.used() - 1U};
    for (std::size_t limit : limits) {
        WorkBudget budget(limit);
        output = triangle();
        const auto before = output;
        assert(!remesh(input, output, budget, 0.5));
        assert(budget.exceeded() && same(output, before));
    }

    TriangleMesh staged = input;
    const double tolerance = std::pow(4e-12, 4.0);
    WorkBudget preparation;
    assert(remesh_detail::normalizeMesh(staged, 10000U, 10000U, tolerance, preparation));
    assert(remesh_detail::splitLongEdges(staged, 0.5 * (4.0 / 3.0), 10000U, 10000U, preparation));
    assert(remesh_detail::collapseShortEdges(staged, 0.4, 10000U, 10000U, tolerance, preparation));
    assert(remesh_detail::flipEdges(staged, tolerance, preparation));
    const auto before = staged;
    WorkBudget smoothing;
    assert(remesh_detail::smoothTangentially(staged, 0.35, tolerance, smoothing));
    assert(!same(staged, before));
    for (std::size_t limit : {smoothing.used() / 2U, smoothing.used() - 1U}) {
        WorkBudget insufficient(limit);
        staged = before;
        assert(!remesh_detail::smoothTangentially(staged, 0.35, tolerance, insufficient));
        assert(insufficient.exceeded() && same(staged, before));
    }

    // A safe full proposal keeps the ordinary, unhalved smoothing result.
    TriangleMesh ordinary;
    for (std::size_t y = 0U; y < 3U; ++y)
        for (std::size_t x = 0U; x < 3U; ++x)
            ordinary.points.push_back(Point3(double(x), double(y), 0));
    ordinary.points[4].x = 1.25;
    for (std::size_t y = 0U; y < 2U; ++y)
        for (std::size_t x = 0U; x < 2U; ++x) {
            const std::size_t a = y * 3U + x;
            ordinary.triangles.push_back({a, a + 1U, a + 4U});
            ordinary.triangles.push_back({a, a + 4U, a + 3U});
        }
    const auto original = ordinary;
    WorkBudget fullStep;
    assert(remesh_detail::smoothTangentially(ordinary, 0.35, tolerance, fullStep));
    assert(ordinary.points[4].x == 1.1625 && ordinary.points[4].y == 1.0 && ordinary.points[4].z == 0.0);
    for (std::size_t i = 0U; i < ordinary.points.size(); ++i) {
        if (i == 4U) continue;
        for (std::size_t axis = 0U; axis < 3U; ++axis)
            assert(ordinary.points[i][axis] == original.points[i][axis]);
    }
}

void geometricFrame()
{
    const auto input = triangle();
    for (std::size_t position : {0U, 1U, 3U}) {
        const auto padded = withUnusedVertex(input, position, {1e30, -1e30, 1e30});
        WorkBudget budget;
        Point3 origin(7,8,9);
        double extent = 42.0;
        assert(referencedTriangleFrame(padded.points, padded.triangles, origin, extent, budget));
        assert(origin.x == 0.0 && origin.y == 0.0 && origin.z == 0.0 && extent == 1.0);
        TriangleMesh output;
        assert(remesh(padded, output, budget) && same(output, input));
    }
    auto bad = input;
    bad.points.push_back({NAN,0,0});
    WorkBudget budget;
    Point3 origin(7,8,9);
    double extent = 42.0;
    assert(!referencedTriangleFrame(bad.points, bad.triangles, origin, extent, budget));
    assert(origin.x == 7.0 && extent == 42.0);
}

void independentComponents()
{
    for (double separation : {double(1e12f), double(-1e12f), double(1e30f), 5e-10}) {
        for (double scale : {0.01, 1.0, 10.0}) {
            const auto input = separatedTrianglesFixture(separation, scale);
            TriangleMesh expected;
            for (std::size_t component = 0U; component < 2U; ++component) {
                TriangleMesh part, result;
                part.points.assign(input.points.begin() + 3U * component,
                                   input.points.begin() + 3U * component + 3U);
                part.triangles = {{0,1,2}};
                WorkBudget budget;
                assert(remesh(part, result, budget));
                const std::size_t offset = expected.points.size();
                expected.points.insert(expected.points.end(), result.points.begin(), result.points.end());
                for (const auto& face : result.triangles)
                    expected.triangles.push_back({offset+face.first, offset+face.second, offset+face.third});
            }
            TriangleMesh result;
            WorkBudget budget;
            assert(remesh(input, result, budget) && same(result, expected));
            const auto before = result;
            WorkBudget shortBudget(budget.used() - 1U);
            assert(!remesh(input, result, shortBudget) && same(result, before));
            assert(shortBudget.exceeded());
            assert(!isotropicRemesh(input, 2, 1, result, expected.points.size() - 1U,
                                     100000U) && same(result, before));
        }
    }
    auto padded = triangle();
    padded.points.insert(padded.points.end(), {{1e12,0,0}, {1e12,1,0}, {1e12,2,0}});
    padded.triangles.push_back({3,4,5});
    TriangleMesh output;
    WorkBudget budget;
    assert(remesh(padded, output, budget) && same(output, triangle()));
    padded.triangles.back() = {3,3,4};
    const auto before = output;
    WorkBudget invalid;
    assert(!remesh(padded, output, invalid) && same(output, before));
    std::cout << "Disconnected and degenerate components cannot change another remesh frame\n";
}

void storageAdmission()
{
    std::size_t bytes = 77U;
    assert(triangleWorkspaceBytes(3U, 1U, bytes) && bytes == 1408U);
    bytes = 77U;
    assert(!triangleWorkspaceBytes(9000000U, 1U, bytes) && bytes == 77U);
    assert(!validTriangleWorkspaceStorage(std::numeric_limits<std::size_t>::max(), 1U));
    assert(!validTriangleWorkspaceStorage(3U, std::numeric_limits<std::size_t>::max()));
    const auto limit = dommy::mesh::kMaxMeshOperationBytes;
    const std::size_t points = (limit - 640U) / 256U;
    assert(validTriangleWorkspaceStorage(points, 1U));
    assert(!validTriangleWorkspaceStorage(points + 1U, 1U));
    assert(!validTriangleWorkspaceStorage(3U, 1U, limit));
    auto mesh = triangle();
    mesh.points.resize(850000U); // Admitted point count, rejected peak-staging estimate.
    TriangleMesh output = triangle();
    WorkBudget budget;
    assert(!isotropicRemesh(mesh, 2, 1, output, 10000000U, 2000000U, budget));
    assert(budget.used() == 0U && same(output, triangle()));
    std::vector<TriangleComponent> components(1U);
    components[0].points = {42U};
    WorkBudget tiny(1U);
    assert(!triangleComponents(triangle().points, triangle().triangles, components, tiny));
    assert(components.size() == 1U && components[0].points[0] == 42U && tiny.exceeded());
    auto compact = triangle();
    compact.points.resize(10000U);
    WorkBudget ample;
    assert(remesh_detail::compactReferencedPoints(compact, ample));
    assert(compact.points.size() == 3U && compact.points.capacity() < 10000U);
    std::cout << "Remesh storage estimates reject overflow/oversize before copying; compaction reserves used points\n";
}

} // namespace

int main()
{
    exactBudgets();
    smoothingRollback();
    geometricFrame();
    independentComponents();
    storageAdmission();
    std::cout << "Isotropic remesh budgets, batch orientation and referenced frames passed\n";
}
