#include <nanoflann.hpp>
#include "common/utility/DBoundedRadiusSearch.h"
#include "common/utility/DPointTransform.h"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

namespace {

struct Point {
    double x;
    double y;
    double z;
};

struct PointCloud {
    std::vector<Point> points;

    std::size_t kdtree_get_point_count() const
    {
        return points.size();
    }

    double kdtree_distance(
        const double* query, std::size_t index, std::size_t) const
    {
        const double dx = query[0] - points[index].x;
        const double dy = query[1] - points[index].y;
        const double dz = query[2] - points[index].z;
        return dx * dx + dy * dy + dz * dz;
    }

    double kdtree_get_pt(std::size_t index, int dimension) const
    {
        if (dimension == 0) return points[index].x;
        if (dimension == 1) return points[index].y;
        return points[index].z;
    }

    template <typename BoundingBox>
    bool kdtree_get_bbox(BoundingBox&) const
    {
        return false;
    }
};

typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<double, PointCloud>,
    PointCloud,
    3> Tree;

void testWorldSpaceQueries()
{
    using dommy::spatial::BoundedRadiusResultSet;
    using dommy::spatial::RadiusSearchBudget;
    const nanoflann::SearchParams searchParams(32, 0.0f, true);
    const auto search = [&](const PointCloud& cloud, const std::array<double, 3>& query,
                            double squaredRadius) {
        Tree tree(3, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        tree.buildIndex();
        RadiusSearchBudget budget;
        BoundedRadiusResultSet result(squaredRadius, 4U, budget, 10U);
        tree.findNeighbors(result, query.data(), searchParams);
        return result.takeSortedMatches();
    };
    double matrix[4][4] = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {10,0,0,1}};
    const std::array<double, 3> local = {{0,0,0}};
    std::array<double, 3> world;
    assert(dommy::numeric::checkedTransformPoint(local, matrix, world));
    PointCloud cloud;
    cloud.points = {Point{0.5,0,0}};
    assert(search(cloud, local, 1.0).size() == 1U);
    assert(search(cloud, world, 1.0).empty());

    // A float query would round 16777217 to 16777216 and miss this neighbor.
    matrix[3][0] = 16777217.0;
    assert(dommy::numeric::checkedTransformPoint(local, matrix, world));
    cloud.points = {Point{16777217.25,0,0}, Point{16777218,0,0}};
    const auto precise = search(cloud, world, 0.25);
    assert(precise.size() == 1U && precise[0].second == 0U && precise[0].first == 0.0625);

    // Baked points and retained affine matrices must feed identical queries.
    // Cover rotation, shear, nonuniform/reflected scale, and a collapsed axis.
    for (double scale : {0.0, -2.0, 3.0}) {
        matrix[0][0] = 0.0; matrix[0][1] = scale;
        matrix[1][0] = -1.0; matrix[1][1] = 0.0; matrix[1][2] = 0.5;
        const std::array<double, 3> source = {{2,3,4}};
        assert(dommy::numeric::checkedTransformPoint(source, matrix, world));
        cloud.points = {Point{world[0], world[1], world[2]}};
        for (double dx : {0.25, 0.5, 2.0})
            cloud.points.push_back(Point{world[0] + dx, world[1], world[2]});
        const std::array<double, 3> baked = {{16777214.0, 2.0 * scale, 5.5}};
        assert(world == baked);
        assert(search(cloud, world, 1.0) == search(cloud, baked, 1.0));
        assert(search(cloud, world, 1.0).size() == 3U);
    }
}

} // namespace

int main()
{
    testWorldSpaceQueries();
    PointCloud cloud;
    cloud.points.push_back(Point{0.0, 0.0, 0.0});
    cloud.points.push_back(Point{1.0, 0.0, 0.0});

    Tree tree(
        3, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    tree.buildIndex();

    const double query[3] = {0.25, 0.0, 0.0};
    std::vector<std::pair<std::size_t, double> > radiusMatches;
    const nanoflann::SearchParams searchParams(32, 0.0f, true);
    tree.radiusSearch(query, 1.0, radiusMatches, searchParams);

    std::size_t nearestIndex = 0U;
    double squaredDistance = 0.0;
    nanoflann::KNNResultSet<double> resultSet(1U);
    resultSet.init(&nearestIndex, &squaredDistance);
    tree.findNeighbors(resultSet, query, searchParams);

    assert(resultSet.size() == 1U);

    using dommy::spatial::BoundedRadiusResultSet;
    using dommy::spatial::RadiusSearchBudget;
    PointCloud dense;
    dense.points.assign(20000U, Point{0.0, 0.0, 0.0});
    Tree denseTree(3, dense, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    denseTree.buildIndex();
    const double origin[3] = {0.0, 0.0, 0.0};
    RadiusSearchBudget budget;
    BoundedRadiusResultSet bounded(100.0, 3U, budget, 10U);
    denseTree.findNeighbors(bounded, origin, searchParams);
    const auto matches = bounded.takeSortedMatches();
    assert(matches.size() == 3U);
    assert(matches[0].second == 0U && matches[1].second == 1U && matches[2].second == 2U);

    RadiusSearchBudget smallBudget(25U);
    BoundedRadiusResultSet limited(100.0, 3U, smallBudget, 10U);
    bool exhausted = false;
    try { denseTree.findNeighbors(limited, origin, searchParams); }
    catch (const std::length_error&) { exhausted = true; }
    assert(exhausted);

    // Compare actual legacy traversal against exhaustive radius collection,
    // including tied distances, self exclusion, and varying result limits.
    PointCloud varied;
    for (std::size_t index = 0U; index < 2000U; ++index) {
        varied.points.push_back(Point{
            static_cast<double>((index * 17U) % 97U),
            static_cast<double>((index * 13U) % 53U),
            static_cast<double>((index * 7U) % 29U)});
    }
    Tree variedTree(3, varied, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    variedTree.buildIndex();
    for (std::size_t capacity : {1U, 3U, 10U, 2000U}) {
        const double testQuery[3] = {40.0, 20.0, 10.0};
        std::vector<std::pair<std::size_t, double>> all;
        variedTree.radiusSearch(testQuery, 400.0, all, searchParams);
        std::vector<BoundedRadiusResultSet::Match> expected;
        for (const auto& match : all) {
            if (match.first != 7U) expected.emplace_back(match.second, match.first);
        }
        std::sort(expected.begin(), expected.end());
        if (expected.size() > capacity) expected.resize(capacity);
        BoundedRadiusResultSet result(400.0, capacity, budget, 10U, 7U);
        variedTree.findNeighbors(result, testQuery, searchParams);
        assert(result.takeSortedMatches() == expected);
    }
    std::cout << "Legacy nanoflann bounded-radius runtime tests: OK\n";
    return 0;
}
