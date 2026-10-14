#include "common/utility/DBoundedRadiusSearch.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DPointProjection.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DMeshInputValidation.h"
#include "modules/volume/VDB/Common/DVDBTimeMetadata.h"

#include <maya/MMatrix.h>

#include <array>
#include <cassert>
#include <cfenv>
#include <limits>
#include <new>

void runMeshBoundaryTests()
{
    using dommy::mesh::validEdgeVertexIndices;
    assert(validEdgeVertexIndices(0, 2, 3U));
    assert(!validEdgeVertexIndices(-1, 0, 3U));
    assert(!validEdgeVertexIndices(0, -1, 3U));
    assert(!validEdgeVertexIndices(0, 3, 3U));
    assert(!validEdgeVertexIndices(3, 0, 3U));
    assert(!validEdgeVertexIndices(0, 0, 0U));

    typedef std::array<float, 3> FloatPoint;
    typedef std::array<double, 3> ProjectionPoint;
    std::vector<FloatPoint> positions = {{{10.0f, 20.0f, 30.0f}},
                                         {{40.0f, 50.0f, 60.0f}}};
    const auto initialPositions = positions;
    using dommy::geometry::projectPointPositions;
    assert(projectPointPositions<ProjectionPoint>(positions,
        [&](std::vector<ProjectionPoint>& points) {
            // The isolated second point must not start at the origin.
            assert(points[0][0] == 10.0 && points[1][0] == 40.0);
            assert(points[1][1] == 50.0 && points[1][2] == 60.0);
            points[0][0] += 2.0;
            return true;
        }));
    assert(positions[0][0] == 12.0f && positions[1] == initialPositions[1]);
    assert(projectPointPositions<ProjectionPoint>(positions,
        [](std::vector<ProjectionPoint>& points) {
            assert(points[0][0] == 12.0); // Fresh inputs each iteration.
            points[1][2] += 1.0;
            return true;
        }));
    const auto projectedPositions = positions;
    assert(!projectPointPositions<ProjectionPoint>(positions,
        [](std::vector<ProjectionPoint>& points) {
            points[0][0] = 999.0;
            return false;
        }));
    assert(positions == projectedPositions);
    assert(!projectPointPositions<ProjectionPoint>(positions,
        [](std::vector<ProjectionPoint>& points) {
            points.pop_back();
            return true;
        }));
    assert(positions == projectedPositions);
    assert(!projectPointPositions<ProjectionPoint>(positions,
        [](std::vector<ProjectionPoint>& points) {
            points[1][2] = std::numeric_limits<double>::infinity();
            return true;
        }));
    assert(positions == projectedPositions);
    assert(!projectPointPositions<ProjectionPoint>(positions,
        [](std::vector<ProjectionPoint>& points) {
            points[1][2] = std::numeric_limits<double>::max();
            return true;
        }));
    assert(positions == projectedPositions);
    bool projectionThrew = false;
    try {
        projectPointPositions<ProjectionPoint>(positions,
            [](std::vector<ProjectionPoint>&) -> bool { throw std::bad_alloc(); });
    } catch (const std::bad_alloc&) { projectionThrew = true; }
    assert(projectionThrew && positions == projectedPositions);
    positions[0][0] = std::numeric_limits<float>::quiet_NaN();
    bool calledProject = false;
    assert(!projectPointPositions<ProjectionPoint>(positions,
        [&](std::vector<ProjectionPoint>&) { calledProject = true; return true; }));
    assert(!calledProject);
    positions.clear();
    assert(projectPointPositions<ProjectionPoint>(positions,
        [&](std::vector<ProjectionPoint>&) { calledProject = true; return true; }));
    assert(!calledProject && positions.empty());

    using dommy::attribute::checkedMaskedLinearBlend;
    double blended = -999.0;
    assert(checkedMaskedLinearBlend(2.0, 10.0, 0.0, 1.0, blended));
    assert(blended == 2.0); // No color mask must still honor Weight = 0.
    assert(checkedMaskedLinearBlend(2.0, 10.0, 0.25, 1.0, blended));
    assert(blended == 4.0);
    assert(checkedMaskedLinearBlend(2.0, 10.0, 1.0, 1.0, blended));
    assert(blended == 10.0);
    assert(checkedMaskedLinearBlend(2.0, 10.0, 0.5, 0.25, blended));
    assert(blended == 3.0);
    assert(checkedMaskedLinearBlend(2.0, 10.0, 1.0, 0.0, blended));
    assert(blended == 2.0);
    assert(checkedMaskedLinearBlend(2.0, 10.0, -1.0, 1.0, blended));
    assert(blended == -6.0); // Historical extrapolation is not clamped.
    assert(!checkedMaskedLinearBlend(2.0, 10.0,
        std::numeric_limits<double>::infinity(), 1.0, blended));
    assert(!checkedMaskedLinearBlend(2.0, 10.0, 1.0,
        std::numeric_limits<double>::quiet_NaN(), blended));
    assert(!checkedMaskedLinearBlend(2.0, 10.0,
        std::numeric_limits<double>::max(), 2.0, blended));
    assert(blended == -6.0);
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(checkedMaskedLinearBlend(1.0, 1.0, 1e308, 2.0, blended));
    assert(blended == 1.0);
    assert(checkedMaskedLinearBlend(0.0, 1e-308, 1e308, 2.0, blended));
    assert(std::fabs(blended - 2.0) <= 4 * std::numeric_limits<double>::epsilon());
    assert(checkedMaskedLinearBlend(0.0, 1e308, 1e-300, 1e-300, blended));
    assert(blended > 0.0 && std::fabs(blended / 1e-292 - 1.0) < 1e-14);
    const double previousBlend = blended;
    assert(!checkedMaskedLinearBlend(std::numeric_limits<double>::quiet_NaN(), 1.0, 0.0, 0.0, blended));
    assert(blended == previousBlend);
    assert(!checkedMaskedLinearBlend(1.0, 1.0, INFINITY, 0.0, blended));
    assert(blended == previousBlend);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    using dommy::vdb::checkedTimeMetadata;
    dommy::vdb::TimeMetadata metadata;
    assert(checkedTimeMetadata(1.0, 24.0, metadata));
    assert(metadata.frame == 1.0 && metadata.tick == 250);
    assert(metadata.framesPerSecond == 24 && metadata.ticksPerFrame == 250);
    assert(checkedTimeMetadata(1.0, 23.976, metadata));
    assert(metadata.framesPerSecond == 23 && metadata.ticksPerFrame == 250);
    assert(metadata.tick == 250);
    assert(checkedTimeMetadata(-0.5, 6000.0, metadata) && metadata.tick == 0);
    assert(checkedTimeMetadata(-1.5, 6000.0, metadata) && metadata.tick == -1);
    assert(checkedTimeMetadata(0.5, 6000.0, metadata) && metadata.tick == 1);
    const double maxTick = std::numeric_limits<std::int32_t>::max();
    const double minTick = std::numeric_limits<std::int32_t>::min();
    assert(checkedTimeMetadata(maxTick, 6000.0, metadata));
    assert(metadata.tick == std::numeric_limits<std::int32_t>::max());
    assert(checkedTimeMetadata(minTick, 6000.0, metadata));
    assert(metadata.tick == std::numeric_limits<std::int32_t>::min());
    const auto savedMetadata = metadata;
    assert(!checkedTimeMetadata(maxTick + 1.0, 6000.0, metadata));
    assert(!checkedTimeMetadata(minTick - 1.0, 6000.0, metadata));
    assert(!checkedTimeMetadata(10000000.0, 24.0, metadata));
    assert(!checkedTimeMetadata(-10000000.0, 24.0, metadata));
    assert(!checkedTimeMetadata(1.0, 0.0, metadata));
    assert(!checkedTimeMetadata(1.0, -24.0, metadata));
    assert(!checkedTimeMetadata(1.0,
        std::numeric_limits<double>::infinity(), metadata));
    assert(!checkedTimeMetadata(std::numeric_limits<double>::quiet_NaN(),
        24.0, metadata));
    assert(!checkedTimeMetadata(1.0,
        std::numeric_limits<double>::denorm_min(), metadata));
    assert(!checkedTimeMetadata(0.0, maxTick + 1.0, metadata));
    assert(!checkedTimeMetadata(0.0, 1.0e-6, metadata));
    assert(metadata.frame == savedMetadata.frame &&
        metadata.tick == savedMetadata.tick &&
        metadata.framesPerSecond == savedMetadata.framesPerSecond &&
        metadata.ticksPerFrame == savedMetadata.ticksPerFrame);

    using dommy::numeric::checkedTransformPoint;
    const std::array<float, 3> point = {{1.0f, 2.0f, 3.0f}};
    std::array<float, 3> output = {{0.0f, 0.0f, 0.0f}};
    MMatrix matrix;
    assert(checkedTransformPoint(point, matrix, output));
    assert(output == point);
    matrix[3][0] = 10.0;
    assert(checkedTransformPoint(point, matrix, output));
    assert(output[0] == 11.0f);
    matrix[3][0] = 20.0;
    assert(checkedTransformPoint(point, matrix, output));
    assert(output[0] == 21.0f);
    matrix[0][0] = 0.0; matrix[0][1] = 2.0;
    matrix[1][0] = -3.0; matrix[1][1] = 0.0;
    matrix[2][2] = 4.0;
    matrix[3][0] = 10.0; matrix[3][1] = 20.0; matrix[3][2] = 30.0;
    assert(checkedTransformPoint(point, matrix, output));
    assert((output == std::array<float, 3>{{4.0f, 22.0f, 42.0f}}));
    const auto unchanged = output;
    matrix[3][0] = 1.0e40;
    assert(!checkedTransformPoint(point, matrix, output));
    assert(output == unchanged);
    std::array<double, 3> precise;
    assert(checkedTransformPoint(point, matrix, precise));
    matrix[0][0] = std::numeric_limits<double>::infinity();
    assert(!checkedTransformPoint(point, matrix, precise));

    dommy::mesh::MeshMergePlan plan;
    for (std::size_t index = 0U; index < 100000U; ++index) {
        assert(plan.points == index * 3U);
        assert(plan.uvs == index * 3U);
        assert(dommy::mesh::appendMeshMergePlan(3U, 1U, 3U, 3U, plan));
    }
    assert(plan.meshes == 100000U && plan.points == 300000U);
    assert(plan.faces == 100000U && plan.connections == 300000U);
    assert(plan.workItems == 1700000U);
    const auto original = plan;
    assert(!dommy::mesh::appendMeshMergePlan(
        std::numeric_limits<std::size_t>::max(), 1U, 3U, 3U, plan));
    assert(plan.points == original.points && plan.meshes == original.meshes);
    assert(!dommy::mesh::appendMeshMergePlan(
        3U, 1U, 3U, dommy::generation::kMaxWorkItemCount, plan));

    int data = 7;
    assert(dommy::maya::invokeStatusOperation(
        [](int&) { return MStatus(MS::kFailure); }, data) == MS::kFailure);
    assert(data == 7);
    assert(dommy::maya::invokeStatusOperation(
        [](int& value) { value = 8; }, data) == MS::kSuccess);
    assert(data == 8);
    bool threw = false;
    try {
        dommy::maya::invokeStatusOperation(
            [](int&) -> MStatus { throw std::bad_alloc(); }, data);
    } catch (const std::bad_alloc&) { threw = true; }
    assert(threw && data == 8);
    // DPoint retains a throwing void copier; the adapter must not turn its
    // exception into success when used alongside status-returning DCurve copies.
    threw = false;
    try {
        dommy::maya::invokeStatusOperation(
            [](int&) { throw std::bad_alloc(); }, data);
    } catch (const std::bad_alloc&) { threw = true; }
    assert(threw && data == 8);

    using dommy::spatial::BoundedRadiusResultSet;
    using dommy::spatial::RadiusSearchBudget;
    RadiusSearchBudget budget;
    BoundedRadiusResultSet nearest(100.0, 3U, budget, 10U);
    for (std::size_t index = 20000U; index > 0U; --index) {
        nearest.addPoint(static_cast<double>(index) / 20000.0, index);
        assert(nearest.size() <= 3U);
    }
    auto matches = nearest.takeSortedMatches();
    assert(matches.size() == 3U && matches[0].second == 1U && matches[2].second == 3U);
    assert(nearest.takeSortedMatches().empty());

    BoundedRadiusResultSet ties(4.0, 2U, budget, 10U, 0U);
    ties.addPoint(0.0, 0U); // Excluded self does not consume a result slot.
    ties.addPoint(4.0, 1U); // Preserve the exclusive radius boundary.
    ties.addPoint(1.0, 9U);
    ties.addPoint(1.0, 8U);
    assert(ties.worstDist() > 1.0);
    ties.addPoint(1.0, 2U);
    matches = ties.takeSortedMatches();
    assert(matches.size() == 2U && matches[0].second == 2U && matches[1].second == 8U);
    BoundedRadiusResultSet zero(1.0, 0U, budget, 10U);
    zero.addPoint(0.0, 0U);
    assert(zero.size() == 0U && zero.worstDist() == 0.0);

    RadiusSearchBudget exhausted(9U);
    BoundedRadiusResultSet limited(1.0, 1U, exhausted, 10U);
    threw = false;
    try { limited.worstDist(); }
    catch (const std::length_error&) { threw = true; }
    assert(threw);
    threw = false;
    try { BoundedRadiusResultSet invalid(
        std::numeric_limits<double>::infinity(), 1U, budget, 10U); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}
