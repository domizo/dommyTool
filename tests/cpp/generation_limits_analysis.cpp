#include "core/common/utility/DGenerationLimits.h"
#include "core/common/utility/DRandomValueGeneration.h"
#include "core/geometry/DBlueNoiseSampler.h"
#include "core/math/DRandomGenerator.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"

namespace {

void analyzeGenerationBoundaries(int rawCount, int frequency)
{
    std::size_t count = 0;
    (void)dommy::generation::checkedCount(
        rawCount, dommy::generation::kMaxGeneratedPointCount, count);

    std::size_t product = 0;
    (void)dommy::generation::checkedProduct(
        count, 3U, dommy::generation::kMaxWorkItemCount, product);

    (void)dommy::generation::checkedGridBudget(
        10.0, 20.0, 30.0, 0.5, 6.0,
        dommy::generation::kMaxWorkItemCount, product);

    dommy::geometry::IcosahedronTopology topology;
    (void)dommy::geometry::buildHistoricalIcosahedron(frequency, topology);

    std::vector<int> values;
    dommy::random::generateIntValues(1, 0, 4, count, values);

    dommy::math::RandomGenerator random(1U);
    std::vector<dommy::geometry::Point2> points;
    (void)dommy::sampling::generateBlueNoise2D(count, 0.0, random, points);
    const std::vector<dommy::geometry::Point2> candidates = {{0.0, 0.0}, {1.0, 1.0}};
    (void)dommy::sampling::selectBlueNoise2D(candidates, count, 0.5, points);
}

}  // namespace
