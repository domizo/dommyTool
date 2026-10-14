#include "modules/particles/particle/field/DKillFieldMath.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using dommy::kill_field::CellKey;
using dommy::kill_field::findNeighborPresence;
using dommy::kill_field::makeCellKey;
using dommy::kill_field::Point3d;
using dommy::kill_field::pointsWithinDistance;

void testCellBoundaries() {
    const double upper = std::ldexp(1.0, 63);
    const double belowUpper = std::nextafter(upper, 0.0);
    const double belowLower = std::nextafter(-upper, -std::numeric_limits<double>::infinity());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const CellKey original(7, 8, 9);
    CellKey key = original;
    for (double invalid : {upper, belowLower, infinity, -infinity, nan}) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            double coordinates[] = {1.0, 2.0, 3.0};
            coordinates[axis] = invalid;
            assert(!makeCellKey(Point3d(coordinates[0], coordinates[1], coordinates[2]), 1.0, key));
            assert(key == original);
        }
    }
    assert(makeCellKey(Point3d(belowUpper, -upper, -0.25), 1.0, key));
    assert(key.x == static_cast<std::int64_t>(belowUpper));
    assert(key.y == std::numeric_limits<std::int64_t>::min());
    assert(key.z == -1); // Floor, not truncation toward zero.
    assert(makeCellKey(Point3d(-0.0, 1.25, -1.25), 0.5, key));
    assert(key == CellKey(0, 2, -3));
    key = original;
    for (double width : {0.0, -0.0, -1.0, infinity, nan}) {
        assert(!makeCellKey(Point3d(), width, key));
        assert(key == original);
    }
    // A normal-sized point can reach the integer boundary with a tiny cell.
    assert(!makeCellKey(Point3d(1.0, 0.0, 0.0), std::ldexp(1.0, -63), key));
    assert(key == original);
    assert(!makeCellKey(Point3d(std::numeric_limits<double>::max(), 0, 0),
                        std::numeric_limits<double>::denorm_min(), key));
    assert(key == original);

    std::vector<unsigned char> neighbors;
    assert(
        !findNeighborPresence({Point3d(upper, 0, 0), Point3d(upper, 0, 0)}, 1.0, 1000U, neighbors));
    assert(findNeighborPresence({Point3d(-upper, 0, 0), Point3d(-upper, 0, 0)}, 1.0, 1000U,
                                neighbors));
    assert(neighbors == std::vector<unsigned char>({1U, 1U}));
}

void testDistanceBoundaries() {
    const Point3d origin;
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double invalid : {-1.0, infinity, -infinity, nan})
        assert(!pointsWithinDistance(origin, origin, invalid));
    for (double invalid : {infinity, -infinity, nan}) {
        assert(!pointsWithinDistance(Point3d(invalid, 0, 0), origin, 1.0));
        assert(!pointsWithinDistance(origin, Point3d(0, invalid, 0), 1.0));
    }
    assert(pointsWithinDistance(origin, Point3d(-0.0, 0.0, 0.0), 0.0));
    assert(pointsWithinDistance(origin, origin, -0.0));
    assert(!pointsWithinDistance(origin, Point3d(std::numeric_limits<double>::denorm_min(), 0, 0),
                                 0.0));

    // Binary scaling keeps the axis and 3-4-5 boundary cases representable.
    for (int exponent : {-1074, -1000, -665, -30, 0, 30, 665, 1000}) {
        const double scale = std::ldexp(1.0, exponent);
        const double radius = 5.0 * scale;
        const Point3d axis(radius, 0, 0);
        assert(pointsWithinDistance(origin, axis, radius));
        assert(pointsWithinDistance(axis, origin, radius));
        assert(pointsWithinDistance(origin, Point3d(3.0 * scale, 4.0 * scale, 0), radius));
        assert(!pointsWithinDistance(origin, axis, std::nextafter(radius, 0.0)));
        assert(!pointsWithinDistance(origin, Point3d(scale, scale, 0), scale));
        assert(!pointsWithinDistance(origin, Point3d(radius, radius, radius), radius));
    }
    const double maximum = std::numeric_limits<double>::max();
    assert(pointsWithinDistance(origin, Point3d(maximum, 0, 0), maximum));
    assert(!pointsWithinDistance(origin, Point3d(maximum, maximum, 0), maximum));
    assert(!pointsWithinDistance(Point3d(-maximum, 0, 0), Point3d(maximum, 0, 0), maximum));
}

void testScaledNeighborSearch() {
    const std::vector<Point3d> reference = {
        Point3d(-2.0, 0, 0), Point3d(-1.5, 0, 0), Point3d(0, 0, 0),   Point3d(3, 0, 0),
        Point3d(3.75, 0, 0), Point3d(0, 5, 0),    Point3d(0, 5.5, 0), Point3d(0, 0, 9)};
    const std::vector<unsigned char> expected = {1U, 1U, 0U, 1U, 1U, 1U, 1U, 0U};
    for (double scale : {1.0e-200, 1.0e-100, 1.0, 1.0e100, 1.0e200}) {
        std::vector<Point3d> points;
        for (const Point3d& point : reference)
            points.push_back(Point3d(point.x * scale, point.y * scale, point.z * scale));
        std::vector<unsigned char> result;
        assert(findNeighborPresence(points, scale, 10000U, result));
        assert(result == expected);
        // The original false-positive example must remain outside in every scale.
        assert(findNeighborPresence({Point3d(), Point3d(1.5 * scale, 0, 0)}, scale, 1000U, result));
        assert(result == std::vector<unsigned char>({0U, 0U}));
        assert(!findNeighborPresence(points, scale, 1U, result));
    }
    const double tiny = std::numeric_limits<double>::denorm_min();
    std::vector<unsigned char> result;
    assert(findNeighborPresence({Point3d(), Point3d(tiny, tiny, 0)}, tiny, 1000U, result));
    assert(result == std::vector<unsigned char>({0U, 0U}));
    assert(findNeighborPresence({Point3d(), Point3d(), Point3d(tiny, 0, 0)}, 0.0, 1000U, result));
    assert(result == std::vector<unsigned char>({1U, 1U, 0U}));
    assert(findNeighborPresence({}, 1.0, 1000U, result) && result.empty());
    assert(findNeighborPresence({Point3d()}, 1.0, 1000U, result));
    assert(result == std::vector<unsigned char>({0U}));
}
} // namespace

void runKillFieldNumericTests() {
    testCellBoundaries();
    testDistanceBoundaries();
    testScaledNeighborSearch();
    std::cout << "Kill field integer, distance, and scaled neighbor boundaries: OK\n";
}
