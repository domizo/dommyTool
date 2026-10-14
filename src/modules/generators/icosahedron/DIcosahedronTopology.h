#pragma once

#include "common/utility/DGenerationLimits.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

struct IcosahedronPoint {
    IcosahedronPoint() : x(0.0f), y(0.0f), z(0.0f) {}
    IcosahedronPoint(float xValue, float yValue, float zValue)
        : x(xValue), y(yValue), z(zValue) {}

    float x;
    float y;
    float z;
};

struct IcosahedronTriangle {
    IcosahedronTriangle() : first(0), second(0), third(0) {}
    IcosahedronTriangle(int firstValue, int secondValue, int thirdValue)
        : first(firstValue), second(secondValue), third(thirdValue) {}

    int first;
    int second;
    int third;
};

struct IcosahedronTopology {
    std::vector<IcosahedronPoint> points;
    std::vector<IcosahedronTriangle> triangles;

    void swap(IcosahedronTopology& other)
    {
        points.swap(other.points);
        triangles.swap(other.triangles);
    }
};

// Preserve the historical node contract: frequency 1 is the base icosahedron,
// while values above 1 run exactly that many subdivision passes.  Changing this
// mapping would alter geometry stored in existing scenes.
inline bool historicalSubdivisionCount(int frequency, unsigned int& subdivisions)
{
    if (frequency < 1 || frequency > 10) {
        return false;
    }

    subdivisions = frequency == 1 ? 0U : static_cast<unsigned int>(frequency);
    return true;
}

inline bool checkedIcosahedronCounts(unsigned int subdivisions,
                                     std::size_t& pointCount,
                                     std::size_t& faceCount)
{
    std::size_t powerOfFour = 1U;
    for (unsigned int level = 0; level < subdivisions; ++level) {
        if (!generation::checkedProduct(
                powerOfFour, 4U, generation::kMaxGeneratedFaceCount, powerOfFour)) {
            return false;
        }
    }

    if (!generation::checkedProduct(
            20U, powerOfFour, generation::kMaxGeneratedFaceCount, faceCount)) {
        return false;
    }

    std::size_t scaledPointCount = 0;
    if (!generation::checkedProduct(
            10U, powerOfFour, generation::kMaxGeneratedPointCount, scaledPointCount) ||
        !generation::checkedSum(
            scaledPointCount, 2U, generation::kMaxGeneratedPointCount, pointCount) ||
        pointCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        faceCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    return true;
}

namespace internal {

inline bool appendUnitPoint(const IcosahedronPoint& point,
                            std::vector<IcosahedronPoint>& points,
                            int& index)
{
    const double x = point.x;
    const double y = point.y;
    const double z = point.z;
    const double length = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(length) || length <= 0.0 ||
        points.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    index = static_cast<int>(points.size());
    points.push_back(IcosahedronPoint(
        static_cast<float>(x / length),
        static_cast<float>(y / length),
        static_cast<float>(z / length)));
    return true;
}

inline bool midpoint(int first,
                     int second,
                     std::vector<IcosahedronPoint>& points,
                     std::map<std::pair<int, int>, int>& cache,
                     int& result)
{
    if (first < 0 || second < 0 ||
        static_cast<std::size_t>(first) >= points.size() ||
        static_cast<std::size_t>(second) >= points.size()) {
        return false;
    }

    const std::pair<int, int> edge = first < second
        ? std::make_pair(first, second)
        : std::make_pair(second, first);
    const std::map<std::pair<int, int>, int>::const_iterator found = cache.find(edge);
    if (found != cache.end()) {
        result = found->second;
        return true;
    }

    const IcosahedronPoint& firstPoint = points[static_cast<std::size_t>(first)];
    const IcosahedronPoint& secondPoint = points[static_cast<std::size_t>(second)];
    const IcosahedronPoint middle(
        (firstPoint.x + secondPoint.x) * 0.5f,
        (firstPoint.y + secondPoint.y) * 0.5f,
        (firstPoint.z + secondPoint.z) * 0.5f);
    if (!appendUnitPoint(middle, points, result)) {
        return false;
    }

    cache.insert(std::make_pair(edge, result));
    return true;
}

} // namespace internal

inline bool buildHistoricalIcosahedron(int frequency, IcosahedronTopology& topology)
{
    unsigned int subdivisions = 0;
    std::size_t expectedPointCount = 0;
    std::size_t expectedFaceCount = 0;
    if (!historicalSubdivisionCount(frequency, subdivisions) ||
        !checkedIcosahedronCounts(subdivisions, expectedPointCount, expectedFaceCount)) {
        return false;
    }

    IcosahedronTopology generated;
    generated.points.reserve(expectedPointCount);
    generated.triangles.reserve(expectedFaceCount);

    const float goldenRatio = static_cast<float>((1.0 + std::sqrt(5.0)) * 0.5);
    const IcosahedronPoint basePoints[] = {
        IcosahedronPoint(-1.0f, goldenRatio, 0.0f),
        IcosahedronPoint(1.0f, goldenRatio, 0.0f),
        IcosahedronPoint(-1.0f, -goldenRatio, 0.0f),
        IcosahedronPoint(1.0f, -goldenRatio, 0.0f),
        IcosahedronPoint(0.0f, -1.0f, goldenRatio),
        IcosahedronPoint(0.0f, 1.0f, goldenRatio),
        IcosahedronPoint(0.0f, -1.0f, -goldenRatio),
        IcosahedronPoint(0.0f, 1.0f, -goldenRatio),
        IcosahedronPoint(goldenRatio, 0.0f, -1.0f),
        IcosahedronPoint(goldenRatio, 0.0f, 1.0f),
        IcosahedronPoint(-goldenRatio, 0.0f, -1.0f),
        IcosahedronPoint(-goldenRatio, 0.0f, 1.0f),
    };
    for (std::size_t index = 0; index < sizeof(basePoints) / sizeof(basePoints[0]); ++index) {
        int insertedIndex = 0;
        if (!internal::appendUnitPoint(basePoints[index], generated.points, insertedIndex)) {
            return false;
        }
    }

    const IcosahedronTriangle baseTriangles[] = {
        IcosahedronTriangle(0, 11, 5), IcosahedronTriangle(0, 5, 1),
        IcosahedronTriangle(0, 1, 7), IcosahedronTriangle(0, 7, 10),
        IcosahedronTriangle(0, 10, 11), IcosahedronTriangle(1, 5, 9),
        IcosahedronTriangle(5, 11, 4), IcosahedronTriangle(11, 10, 2),
        IcosahedronTriangle(10, 7, 6), IcosahedronTriangle(7, 1, 8),
        IcosahedronTriangle(3, 9, 4), IcosahedronTriangle(3, 4, 2),
        IcosahedronTriangle(3, 2, 6), IcosahedronTriangle(3, 6, 8),
        IcosahedronTriangle(3, 8, 9), IcosahedronTriangle(4, 9, 5),
        IcosahedronTriangle(2, 4, 11), IcosahedronTriangle(6, 2, 10),
        IcosahedronTriangle(8, 6, 7), IcosahedronTriangle(9, 8, 1),
    };
    generated.triangles.assign(
        baseTriangles, baseTriangles + sizeof(baseTriangles) / sizeof(baseTriangles[0]));

    for (unsigned int level = 0; level < subdivisions; ++level) {
        std::size_t nextFaceCount = 0;
        if (!generation::checkedProduct(
                generated.triangles.size(), 4U,
                generation::kMaxGeneratedFaceCount, nextFaceCount)) {
            return false;
        }

        std::vector<IcosahedronTriangle> nextTriangles;
        nextTriangles.reserve(nextFaceCount);
        std::map<std::pair<int, int>, int> midpointCache;
        for (std::vector<IcosahedronTriangle>::const_iterator it = generated.triangles.begin();
             it != generated.triangles.end(); ++it) {
            int firstMiddle = 0;
            int secondMiddle = 0;
            int thirdMiddle = 0;
            if (!internal::midpoint(
                    it->first, it->second, generated.points, midpointCache, firstMiddle) ||
                !internal::midpoint(
                    it->second, it->third, generated.points, midpointCache, secondMiddle) ||
                !internal::midpoint(
                    it->third, it->first, generated.points, midpointCache, thirdMiddle)) {
                return false;
            }

            nextTriangles.push_back(IcosahedronTriangle(it->first, firstMiddle, thirdMiddle));
            nextTriangles.push_back(IcosahedronTriangle(it->second, secondMiddle, firstMiddle));
            nextTriangles.push_back(IcosahedronTriangle(it->third, thirdMiddle, secondMiddle));
            nextTriangles.push_back(IcosahedronTriangle(firstMiddle, secondMiddle, thirdMiddle));
        }
        generated.triangles.swap(nextTriangles);
    }

    if (generated.points.size() != expectedPointCount ||
        generated.triangles.size() != expectedFaceCount) {
        return false;
    }

    topology.swap(generated);
    return true;
}

} // namespace geometry
} // namespace dommy
