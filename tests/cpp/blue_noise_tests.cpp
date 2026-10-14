#include "geometry/DBlueNoiseSampler.h"
#include "math/DRandomGenerator.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using dommy::geometry::Point2;
using dommy::sampling::BlueNoiseLimits;

bool samePoints(const std::vector<Point2>& first, const std::vector<Point2>& second)
{
    if (first.size() != second.size()) return false;
    for (std::size_t id = 0U; id < first.size(); ++id)
        if (first[id].x != second[id].x || first[id].y != second[id].y) return false;
    return true;
}

std::vector<Point2> uniformPoints(std::size_t count, std::uint32_t seed)
{
    dommy::math::RandomGenerator random(seed);
    std::vector<Point2> points;
    for (std::size_t id = 0U; id < count; ++id) {
        const double x = random.uniform(0.0, 1.0), y = random.uniform(0.0, 1.0);
        points.push_back(Point2(x, y));
    }
    return points;
}

// Deliberately quadratic oracle with no grid or priority structure. The dense
// test ranges avoid near-zero score ties between independent summation orders.
std::vector<Point2> bruteSelection(const std::vector<Point2>& points,
                                  std::size_t wanted, double radius)
{
    std::vector<unsigned char> active(points.size(), 1U);
    for (std::size_t left = points.size(); left > wanted; --left) {
        std::size_t remove = points.size();
        double worst = -1.0;
        for (std::size_t id = 0U; id < points.size(); ++id) {
            if (!active[id]) continue;
            double score = 0.0;
            for (std::size_t other = 0U; other < points.size(); ++other) {
                if (other == id || !active[other]) continue;
                const double distance = std::hypot(
                    points[id].x - points[other].x, points[id].y - points[other].y);
                if (distance < radius) {
                    const double proximity = 1.0 - distance / radius;
                    score += proximity * proximity;
                }
            }
            if (score > worst || (score == worst && id > remove)) {
                worst = score;
                remove = id;
            }
        }
        assert(remove < points.size());
        active[remove] = 0U;
    }
    std::vector<Point2> result;
    for (std::size_t id = 0U; id < points.size(); ++id)
        if (active[id]) result.push_back(points[id]);
    return result;
}

void gridAndSelection()
{
    std::vector<Point2> points = uniformPoints(37U, 17U);
    points.push_back(Point2(0.0, 0.0));
    points.push_back(Point2(1.0, 1.0));
    points.push_back(Point2(0.5, 0.5));
    points.push_back(Point2(0.5, 0.5));
    points.push_back(Point2(std::numeric_limits<double>::denorm_min(), 0.0));
    for (double radius : {std::numeric_limits<double>::denorm_min(),
                          0.00001, 0.08, std::nextafter(0.25, 0.0), 0.25,
                          std::nextafter(0.25, 1.0), 0.7, 2.0, 1e200}) {
        dommy::sampling::blue_noise_detail::Plan plan;
        assert(dommy::sampling::blue_noise_detail::makePlan(
            points.size(), 12U, radius, BlueNoiseLimits(), plan));
        dommy::sampling::blue_noise_detail::Grid grid(points, plan);
        dommy::generation::WorkBudget budget;
        for (std::size_t id = 0U; id < points.size(); ++id) {
            std::vector<std::size_t> actual, expected;
            assert(grid.visit(id, budget, [&](std::size_t other, double weight) {
                assert(std::isfinite(weight) && weight >= 0.0 && weight <= 1.0);
                actual.push_back(other);
                return true;
            }));
            for (std::size_t other = 0U; other < points.size(); ++other)
                if (other != id && std::hypot(points[id].x - points[other].x,
                        points[id].y - points[other].y) < radius) expected.push_back(other);
            std::sort(actual.begin(), actual.end());
            assert(actual == expected);
        }
    }
    for (std::uint32_t seed : {1U, 7U, 23U}) {
        const std::vector<Point2> candidates = uniformPoints(37U, seed);
        for (double radius : {0.8, 2.0}) {
            std::vector<Point2> actual;
            assert(dommy::sampling::selectBlueNoise2D(candidates, 16U, radius, actual));
            assert(samePoints(actual, bruteSelection(candidates, 16U, radius)));
        }
    }
    const std::vector<Point2> corners{
        Point2(0.0, 0.0), Point2(1.0, 0.0), Point2(0.0, 1.0), Point2(1.0, 1.0)};
    std::vector<Point2> selected;
    assert(dommy::sampling::selectBlueNoise2D(corners, 2U, 0.01, selected));
    assert(samePoints(selected, {corners[0], corners[1]}));
    std::vector<Point2> aliased = corners;
    assert(dommy::sampling::selectBlueNoise2D(aliased, 2U, 0.01, aliased));
    assert(samePoints(aliased, selected));

    // Fully coincident input exercises dense weights and exact priority ties.
    const std::vector<Point2> duplicates(64U, Point2(0.5, 0.5));
    assert(dommy::sampling::selectBlueNoise2D(duplicates, 9U, 0.01, selected));
    assert(selected.size() == 9U);
    assert(samePoints(selected, std::vector<Point2>(9U, Point2(0.5, 0.5))));
}

struct CountingRandom {
    explicit CountingRandom(std::uint32_t seed) : generator(seed), calls(0U) {}
    double uniform(double minimum, double maximum) {
        ++calls;
        return generator.uniform(minimum, maximum);
    }
    dommy::math::RandomGenerator generator;
    std::size_t calls;
};

struct InvalidRandom {
    double value;
    double uniform(double, double) { return value; }
};

struct ThrowingRandom {
    int kind;
    double uniform(double, double) {
        if (kind == 0) throw std::bad_alloc();
        if (kind == 1) throw std::length_error("injected sample allocation failure");
        throw std::runtime_error("injected random failure");
    }
};

void boundariesAndFailures()
{
    const std::vector<Point2> previous{Point2(9.0, -3.0)};
    std::vector<Point2> output = previous;
    const std::vector<Point2> candidates{Point2(0.0, 0.0), Point2(1.0, 1.0)};
    for (double radius : {-2.0, std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity(),
                          -std::numeric_limits<double>::infinity()}) {
        CountingRandom random(1U);
        assert(!dommy::sampling::generateBlueNoise2D(20U, radius, random, output));
        assert(random.calls == 0U && samePoints(output, previous));
        assert(!dommy::sampling::selectBlueNoise2D(candidates, 1U, radius, output));
        assert(samePoints(output, previous));
    }
    assert(!dommy::sampling::selectBlueNoise2D(candidates, 3U, 0.1, output));
    assert(samePoints(output, previous));
    for (Point2 bad : {Point2(-0.1, 0.0), Point2(1.1, 0.0),
                      Point2(std::numeric_limits<double>::quiet_NaN(), 0.0),
                      Point2(0.0, std::numeric_limits<double>::infinity())}) {
        assert(!dommy::sampling::selectBlueNoise2D({bad}, 1U, 0.1, output));
        assert(samePoints(output, previous));
    }
    for (std::size_t count : {dommy::generation::kMaxGeneratedPointCount,
                              std::numeric_limits<std::size_t>::max()}) {
        CountingRandom random(1U);
        assert(!dommy::sampling::generateBlueNoise2D(count, 0.1, random, output));
        assert(random.calls == 0U && samePoints(output, previous));
    }
    for (int failure = 0; failure < 4; ++failure) {
        BlueNoiseLimits limits;
        if (failure == 0) limits.maxCandidates = 5U;
        if (failure == 1) limits.maxBytes = 32U;
        if (failure == 2) limits.maxWork = 8U;
        if (failure == 3) limits.maxWork = 10000U;
        CountingRandom random(1U);
        assert(!dommy::sampling::generateBlueNoise2D(20U, 0.2, random, output, limits));
        assert((failure == 3 ? random.calls > 0U : random.calls == 0U));
        assert(samePoints(output, previous));
    }
    for (double bad : {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()}) {
        InvalidRandom random{bad};
        assert(!dommy::sampling::generateBlueNoise2D(20U, 0.2, random, output));
        assert(samePoints(output, previous));
    }
    for (int kind : {0, 1, 2}) {
        ThrowingRandom random{kind};
        bool threw = false;
        try {
            assert(!dommy::sampling::generateBlueNoise2D(20U, 0.2, random, output));
        } catch (const std::runtime_error&) { threw = true; }
        assert(threw == (kind == 2));
        assert(samePoints(output, previous));
    }
    BlueNoiseLimits zero;
    zero.maxWork = zero.maxBytes = zero.maxCandidates = 0U;
    CountingRandom random(1U);
    assert(dommy::sampling::generateBlueNoise2D(0U, -1.0, random, output, zero));
    assert(output.empty() && random.calls == 0U);
    assert(dommy::sampling::selectBlueNoise2D(candidates, candidates.size(), 0.1, output));
    assert(samePoints(output, candidates));

    // Large dense requests must stop at the work limit instead of completing
    // an unbounded all-pairs pass or changing the caller's existing output.
    output = previous;
    BlueNoiseLimits bounded;
    bounded.maxWork = 100000U;
    const std::vector<Point2> dense(4096U, Point2(0.5, 0.5));
    assert(!dommy::sampling::selectBlueNoise2D(dense, 2048U, 2.0, output, bounded));
    assert(samePoints(output, previous));
}

void reproducibility()
{
    for (std::size_t count : {std::size_t(0U), std::size_t(1U), std::size_t(20U),
                              std::size_t(128U)}) {
        for (double radius : {-1.0, -0.5, 0.0, 0.15, 2.0}) {
            for (std::uint32_t seed : {0U, 42U, std::numeric_limits<std::uint32_t>::max()}) {
                CountingRandom first(seed), second(seed);
                std::vector<Point2> a, b;
                assert(dommy::sampling::generateBlueNoise2D(count, radius, first, a));
                assert(dommy::sampling::generateBlueNoise2D(count, radius, second, b));
                assert(a.size() == count && samePoints(a, b));
                assert(first.calls == (count ? 2U * (count + 1000U) : 0U));
                for (const Point2& point : a)
                    assert(dommy::sampling::blue_noise_detail::validPoint(point));
            }
        }
    }
    CountingRandom a(1U), b(2U);
    std::vector<Point2> first, second;
    assert(dommy::sampling::generateBlueNoise2D(20U, 0.0, a, first));
    assert(dommy::sampling::generateBlueNoise2D(20U, 0.0, b, second));
    assert(!samePoints(first, second));

    // A more realistic output count exercises a many-cell grid and handoff.
    CountingRandom largeA(7U), largeB(7U);
    assert(dommy::sampling::generateBlueNoise2D(4096U, 0.0, largeA, first));
    assert(dommy::sampling::generateBlueNoise2D(4096U, 0.0, largeB, second));
    assert(first.size() == 4096U && samePoints(first, second));
}

double meanNearestDistance(const std::vector<Point2>& points)
{
    double total = 0.0;
    for (std::size_t id = 0U; id < points.size(); ++id) {
        double nearest = 2.0;
        for (std::size_t other = 0U; other < points.size(); ++other)
            if (other != id) nearest = std::min(nearest, std::hypot(
                points[id].x - points[other].x, points[id].y - points[other].y));
        total += nearest;
    }
    return total / points.size();
}

// A Hann window suppresses non-periodic domain-edge leakage. Wavevectors with
// a component >=2 have no mean-field leakage from this separable window.
double windowedPower(const std::vector<Point2>& points, int xFrequency, int yFrequency)
{
    const double tau = 6.28318530717958647692;
    double real = 0.0, imaginary = 0.0, normalizer = 0.0;
    for (const Point2& point : points) {
        const double weight = 0.25 * (1.0 - std::cos(tau * point.x)) *
                              (1.0 - std::cos(tau * point.y));
        const double angle = tau * (xFrequency * point.x + yFrequency * point.y);
        real += weight * std::cos(angle);
        imaginary += weight * std::sin(angle);
        normalizer += weight * weight;
    }
    return (real * real + imaginary * imaginary) / normalizer;
}

void distributionQuality()
{
    double randomDistance = 0.0, blueDistance = 0.0;
    double randomLow = 0.0, blueLow = 0.0, blueHigh = 0.0;
    for (std::uint32_t seed : {3U, 11U, 29U, 47U, 101U, 223U, 509U, 997U}) {
        const std::vector<Point2> baseline = uniformPoints(256U, seed);
        dommy::math::RandomGenerator random(seed);
        std::vector<Point2> points;
        assert(dommy::sampling::generateBlueNoise2D(256U, 0.0, random, points));
        randomDistance += meanNearestDistance(baseline);
        blueDistance += meanNearestDistance(points);
        for (int x = -3; x <= 3; ++x) for (int y = -3; y <= 3; ++y) {
            const int squaredFrequency = x * x + y * y;
            if (squaredFrequency < 4 || squaredFrequency > 9) continue;
            randomLow += windowedPower(baseline, x, y);
            blueLow += windowedPower(points, x, y);
            blueHigh += windowedPower(points, 10 * x, 10 * y);
        }
    }
    assert(blueDistance > randomDistance * 1.3);
    assert(blueLow < randomLow * 0.75);
    assert(blueHigh > blueLow * 1.5);
    std::cout << "Blue-noise quality: nearest-distance ratio " << blueDistance / randomDistance
              << ", low-frequency power ratio " << blueLow / randomLow << '\n';
}

} // namespace

void runBlueNoiseTests()
{
    gridAndSelection();
    boundariesAndFailures();
    reproducibility();
    distributionQuality();
}

#ifdef DOMMY_BLUE_NOISE_STANDALONE
int main() { runBlueNoiseTests(); }
#endif
