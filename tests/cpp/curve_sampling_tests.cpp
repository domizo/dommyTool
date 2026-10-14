#include "common/utility/DCurveSampling.h"
#include "common/utility/DSignalMath.h"

#include <array>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <limits>
#include <vector>

namespace {
using Point = std::array<float, 3>;
using Points = std::vector<Point>;

void testWeightMap()
{
    using dommy::signal::weightMapPosition;
    using dommy::signal::weightMapDistance;
    double value = 17.0;
    assert(weightMapPosition(-0.25, 0, 2, value) && value == 0.75);
    assert(weightMapPosition(2, 0, 1, value) && value == 1);
    assert(weightMapPosition(-2, 0, 0, value) && value == -2);
    assert(weightMapPosition(16777217.25, 0.25, 0, value) && value == 16777217.5);
    for (int mode : {-1, 3}) assert(!weightMapPosition(0, 0, mode, value));
    assert(!weightMapPosition(std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 0, value));
    assert(value == 16777217.5);
    for (int i = 0; i <= 400; ++i) {
        const double distance = double(i) / 16.0;
        const double expected = (int(distance) % 2) == 0
            ? std::fmod(distance, 1.0) : 1.0 - std::fmod(distance, 1.0);
        assert(weightMapDistance(distance, 0, 2, 1, true, value));
        assert(value == expected);
    }
    assert(weightMapDistance(1e10, 0, 2, 1, true, value) && value == 0);
    assert(weightMapDistance(2147483649.25, 0, 2, 1, true, value) && value == 0.75);
    assert(weightMapDistance(-2.25, 0, 2, 1, true, value) && value == 0.25);
    assert(weightMapDistance(0.25, 1, 3, 1, false, value) && value == 0.25);
    assert(weightMapDistance(2.5, 0, 1, 1, false, value) && value == 1.0);
    assert(weightMapDistance(0.5, 0, 5, double(0.2f), false, value) && value == 1.0);
    const double huge = std::ldexp(1.0, 60);
    assert(weightMapPosition(0.25, huge, 2, value) && value == 0.25);
    assert(weightMapPosition(0.25, -huge, 2, value) && value == 0.25);
    for (double position : {huge, -huge})
        assert(weightMapDistance(position, 1, 5, 1.0, true, value) && value == 0.25);
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(weightMapDistance(std::numeric_limits<double>::max(), 0, 2, 0.5, true, value) && value == 0.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
    assert(weightMapPosition(std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 1, value) && value == 1.0);
    assert(weightMapPosition(std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 2, value) && value == 0.0);
    assert(weightMapDistance(0.25, 0, 2, std::numeric_limits<double>::max(), true, value));
    assert(value > 0.0 && value <= 1.0);
    const double previous = value;
    assert(!weightMapDistance(0, 0, 0, 1, true, value));
    assert(!weightMapDistance(0, 2, 2, 1, true, value));
    assert(!weightMapDistance(0, 0, 2, 0, true, value));
    assert(!weightMapDistance(std::numeric_limits<double>::quiet_NaN(), 0, 2, 1, true, value));
    assert(value == previous);
}

void testStitchAndExtensionPlans()
{
    using namespace dommy::curve;
    StitchSamplingPlan stitch;
    assert(checkedStitchSamplingPlan(3, 4, stitch));
    assert(stitch.samples == 4 && stitch.curves == 8 && stitch.points == 16);
    assert(!checkedStitchSamplingPlan(2, -1, stitch));
    assert(!checkedStitchSamplingPlan(2, std::numeric_limits<int>::max(), stitch));
    assert(!checkedStitchSamplingPlan(3, 500001, stitch));
    assert(stitch.curves == 8 && stitch.points == 16);
    assert(checkedStitchSamplingPlan(3, 500000, stitch) && stitch.curves == 1000000);
    for (int samples : {0, 1}) assert(checkedStitchSamplingPlan(3, samples, stitch) && stitch.curves == 0);
    assert(checkedStitchSamplingPlan(1, 5, stitch) && stitch.curves == 0);
    assert(!checkedStitchSamplingPlan(std::numeric_limits<std::size_t>::max(), 5, stitch));
    CurveExtensionPlan extend;
    assert(checkedCurveExtensionPlan(3, 12, 2, false, false, extend));
    assert(extend.curves == 3 && extend.points == 18 && extend.addedCurves == 0);
    assert(checkedCurveExtensionPlan(3, 12, 2, true, false, extend));
    assert(extend.curves == 9 && extend.points == 24 && extend.addedCurves == 6);
    assert(checkedCurveExtensionPlan(3, 12, 2, true, true, extend));
    assert(extend.curves == 6 && extend.points == 12);
    assert(!checkedCurveExtensionPlan(3, 12, 3, true, false, extend));
    assert(!checkedCurveExtensionPlan(500001, 1000002, 2, true, true, extend));
    assert(!checkedCurveExtensionPlan(1, 10000000, 1, false, false, extend));
    assert(extend.curves == 6 && extend.points == 12);
    assert(checkedCurveExtensionPlan(1, 9999999, 1, false, false, extend) && extend.points == 10000000);
    assert(checkedCurveExtensionPlan(3, 12, 0, true, true, extend) && extend.curves == 0 && extend.points == 0);
}

void testOffsetPlan()
{
    using dommy::curve::appendOffsetRebuildPlan;
    std::size_t total = 0U, spans = 0U;
    assert(appendOffsetRebuildPlan(4U, 3, 5, total, spans));
    assert(spans == 19U && total == 23U); // spans + degree + closure headroom
    assert(appendOffsetRebuildPlan(2U, 1, 0, total, spans));
    assert(spans == 2U && total == 27U);
    for (const auto& input : std::vector<std::array<int, 3>>{
            {{0, 1, 0}}, {{1, 1, 0}}, {{4, 0, 0}}, {{4, 4, 0}}, {{4, 3, -1}},
            {{4, 3, 4000000}}, {{4, 3, std::numeric_limits<int>::max()}},
            {{1000001, 1, 0}}, {{10000000, 1, 1}}}) {
        assert(!appendOffsetRebuildPlan(input[0], input[1], input[2], total, spans));
        assert(total == 27U && spans == 2U);
    }
    assert(!appendOffsetRebuildPlan(std::numeric_limits<std::size_t>::max(), 1, 0, total, spans));
    total = dommy::generation::kMaxGeneratedPointCount - 23U;
    assert(appendOffsetRebuildPlan(4U, 3, 5, total, spans));
    assert(total == dommy::generation::kMaxGeneratedPointCount);
    assert(!appendOffsetRebuildPlan(2U, 1, 0, total, spans) && spans == 19U);
}

void testSurfacePlan()
{
    dommy::curve::SurfaceSamplingPlan plan;
    assert(dommy::curve::checkedSurfaceSamplingPlan(4, 4, 8, plan));
    assert(plan.curveCount == 8U && plan.pointCount == 64U);
    for (const auto& raw : std::vector<std::array<int, 3>>{
             {{-1, 0, 2}}, {{0, -1, 2}}, {{1, 1, 1}},
             {{1000001, 0, 2}}, {{500000, 500001, 2}},
             {{1000000, 0, 11}}, {{0, 0, 1000001}}}) {
        assert(!dommy::curve::checkedSurfaceSamplingPlan(raw[0], raw[1], raw[2], plan));
        assert(plan.curveCount == 8U && plan.pointCount == 64U);
    }
    assert(!dommy::curve::checkedSurfaceSamplingPlan(1000000, 0, 10, plan));
    assert(plan.curveCount == 8U && plan.pointCount == 64U);
    assert(dommy::curve::checkedSurfaceSamplingPlan(0, 0, 2, plan));
    assert(plan.curveCount == 0U && plan.pointCount == 0U);
}

void testSurfaceParameters()
{
    using dommy::curve::checkedSurfaceParameter;
    double value = 17.0;
    for (std::size_t i = 0U; i < 3U; ++i) {
        assert(checkedSurfaceParameter(16777216.0, 16777217.0, 1.0, 0.0, i, 3U, true, value));
        assert(value == 16777216.0 + 0.5 * i);
    }
    assert(checkedSurfaceParameter(0, 8, 0.5, 1, 2, 3, false, value) && value == 5);
    assert(checkedSurfaceParameter(0, 8, -1, 0, 2, 3, false, value) && value == -8);
    assert(checkedSurfaceParameter(0, 8, -1, 0, 2, 3, true, value) && value == 0);
    assert(checkedSurfaceParameter(2, 8, 3, 1, 0, 1, false, value) && value == 3);
    const double largest = std::numeric_limits<double>::max();
    assert(checkedSurfaceParameter(-largest, largest, 1, 0, 1, 3, false, value) && value == 0);
    value = 17;
    assert(!checkedSurfaceParameter(0, 1, 1, 0, 0, 0, true, value));
    assert(!checkedSurfaceParameter(0, 1, 1, 0, 2, 2, true, value));
    assert(!checkedSurfaceParameter(1, 0, 1, 0, 0, 2, true, value));
    assert(!checkedSurfaceParameter(0, largest, 1, largest, 1, 2, true, value));
    assert(!checkedSurfaceParameter(0, 1, std::numeric_limits<double>::infinity(), 0, 0, 2, true, value));
    assert(!checkedSurfaceParameter(0, 1, 1, std::numeric_limits<double>::quiet_NaN(), 0, 2, true, value));
    assert(value == 17);
}

void testDivisionCounts()
{
    using dommy::curve::checkedResampleDivisionCount;
    std::size_t count = 17U;
    assert(checkedResampleDivisionCount(0, 1, count) && count == 1U);
    assert(checkedResampleDivisionCount(1, 1, count) && count == 1U);
    assert(checkedResampleDivisionCount(1.01, 1, count) && count == 2U);
    assert(checkedResampleDivisionCount(1.5, 1, count) && count == 3U);
    assert(checkedResampleDivisionCount(999999, 1, count) && count == 1000000U);
    assert(!checkedResampleDivisionCount(1000000, 1, count) && count == 1000000U);
    assert(!checkedResampleDivisionCount(-1, 1, count));
    assert(!checkedResampleDivisionCount(1, 0, count));
    assert(!checkedResampleDivisionCount(std::numeric_limits<double>::max(),
                                        std::numeric_limits<double>::min(), count));
}

void testPolyline()
{
    using dommy::curve::resampleCurvePolyline;
    const Points source = {{{0, 0, 0}}, {{3, 0, 0}}, {{3, 0, 0}}, {{4, 0, 0}}};
    Points output;
    std::vector<std::size_t> indices;
    assert(resampleCurvePolyline(source, 1, 100, output, indices));
    assert(output.size() == 7U);
    assert(output[1][0] == 0.75f && output[2][0] == 1.5f && output[3][0] == 2.25f);
    assert(output.front() == source.front() && output.back() == source.back());
    assert(indices == std::vector<std::size_t>({0, 0, 0, 0, 1, 2, 3}));
    const Points previous = output;
    const auto previousIndices = indices;
    assert(!resampleCurvePolyline(source, 1, 6, output, indices));
    assert(!resampleCurvePolyline(source, 0, 100, output, indices));
    assert(!resampleCurvePolyline(Points(), 1, 100, output, indices));
    Points invalid = source;
    invalid[2][1] = std::numeric_limits<float>::quiet_NaN();
    assert(!resampleCurvePolyline(invalid, 1, 100, output, indices));
    invalid[2][1] = std::numeric_limits<float>::infinity();
    assert(!resampleCurvePolyline(invalid, 1, 100, output, indices));
    assert(output == previous && indices == previousIndices);

    const Points large = {{{0, 0, 0}}, {{1.0e20f, 0, 0}}};
    assert(resampleCurvePolyline(large, 1.0e19, 100, output, indices));
    assert(output.size() == 12U && output.back() == large.back());
    for (std::size_t i = 1; i < output.size(); ++i) {
        assert(std::isfinite(output[i][0]) && output[i][0] > output[i - 1U][0]);
    }
    const float maximum = std::numeric_limits<float>::max();
    const Points extremes = {{{-maximum, 0, 0}}, {{maximum, 0, 0}}};
    assert(resampleCurvePolyline(extremes, 2.0e38, 100, output, indices));
    assert(output.size() == 5U && output[2][0] == 0.0f);
    for (const auto& p : output) assert(std::isfinite(p[0]));

    const float tiny = std::numeric_limits<float>::denorm_min();
    const Points small = {{{0, 0, 0}}, {{tiny, 0, 0}}};
    assert(resampleCurvePolyline(small, double(tiny) / 4.0, 100, output, indices));
    assert(output.size() == 6U && output.back() == small.back());

    Points aliased = source;
    assert(resampleCurvePolyline(aliased, 1, 100, aliased, indices));
    assert(aliased == previous && indices == previousIndices);
}
} // namespace

void runCurveSamplingTests()
{
    testWeightMap();
    testStitchAndExtensionPlans();
    testOffsetPlan();
    testSurfacePlan();
    testSurfaceParameters();
    testDivisionCounts();
    testPolyline();
}
