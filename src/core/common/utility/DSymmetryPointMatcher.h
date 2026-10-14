#pragma once

#include "DBoundedRadiusSearch.h"
#include "DMeshInputValidation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace mesh {

struct Point3d {
    double x;
    double y;
    double z;

    Point3d() : x(0.0), y(0.0), z(0.0) {}
    Point3d(double xValue, double yValue, double zValue)
        : x(xValue), y(yValue), z(zValue) {}
};

const std::size_t kNoPointMatch = std::numeric_limits<std::size_t>::max();

namespace detail {

inline double coordinate(const Point3d& point, int axis)
{
    return axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
}

// Keep magnitude and exponent separate: neither squared DBL_MAX nor squared
// subnormals fit in double (and long double need not have a wider exponent).
struct SearchMagnitude {
    long double fraction;
    int exponent;

    bool operator<(const SearchMagnitude& rhs) const {
        if (fraction == 0.0L || rhs.fraction == 0.0L)
            return fraction < rhs.fraction;
        return exponent < rhs.exponent ||
               (exponent == rhs.exponent && fraction < rhs.fraction);
    }
    bool operator==(const SearchMagnitude& rhs) const {
        return fraction == rhs.fraction && exponent == rhs.exponent;
    }
    bool operator<=(const SearchMagnitude& rhs) const {
        return *this < rhs || *this == rhs;
    }
};

inline SearchMagnitude searchDifference(double first, double second)
{
    long double difference = static_cast<long double>(first) - second;
    int shift = 0;
    if (!std::isfinite(difference)) {
        // Opposite-sign extremes. Halving first makes subtraction safe even
        // on platforms where long double has double's exponent range.
        difference = static_cast<long double>(first) * 0.5L -
                     static_cast<long double>(second) * 0.5L;
        shift = 1;
    }
    int exponent = 0;
    const long double fraction = std::frexp(std::fabs(difference), &exponent);
    return SearchMagnitude{fraction, fraction == 0.0L ? 0 : exponent + shift};
}

inline SearchMagnitude searchSquaredDistance(
    const SearchMagnitude& x, const SearchMagnitude& y = SearchMagnitude{0.0L, 0},
    const SearchMagnitude& z = SearchMagnitude{0.0L, 0})
{
    const SearchMagnitude largest = x < y ? (y < z ? z : y) : (x < z ? z : x);
    if (largest.fraction == 0.0L) return SearchMagnitude{0.0L, 0};
    const long double dx = std::scalbn(x.fraction, x.exponent - largest.exponent);
    const long double dy = std::scalbn(y.fraction, y.exponent - largest.exponent);
    const long double dz = std::scalbn(z.fraction, z.exponent - largest.exponent);
    int exponent = 0;
    const long double fraction = std::frexp(dx * dx + dy * dy + dz * dz, &exponent);
    return SearchMagnitude{fraction, largest.exponent * 2 + exponent};
}

class SymmetryPointIndex {
public:
    SymmetryPointIndex(const std::vector<Point3d>& points,
                       spatial::RadiusSearchBudget& budget)
        : points_(&points), budget_(budget), root_(kNoPointMatch)
    {
        budget_.consume(points.size());
        order_.resize(points.size());
        for (std::size_t index = 0U; index < order_.size(); ++index) {
            order_[index] = index;
        }
        nodes_.reserve(points.size());
        root_ = build(0U, order_.size(), 0);
    }

    std::size_t nearestMirrored(std::size_t pointIndex,
                                double tolerance) const
    {
        const Point3d& point = (*points_)[pointIndex];
        const Point3d query(-point.x, point.y, point.z);
        SearchMagnitude bestDistance =
            searchSquaredDistance(searchDifference(tolerance, 0.0));
        std::size_t bestIndex = kNoPointMatch;
        queryNode(root_, query, bestDistance, bestIndex);
        return bestIndex;
    }

private:
    struct Node {
        std::size_t pointIndex;
        std::size_t left;
        std::size_t right;
        int axis;
    };

    std::size_t build(std::size_t begin, std::size_t end, int depth)
    {
        if (begin == end) return kNoPointMatch;
        budget_.consume(1U);

        const int axis = depth % 3;
        const std::size_t middle = begin + (end - begin) / 2U;
        const std::vector<Point3d>* points = points_;
        std::nth_element(
            order_.begin() + static_cast<std::ptrdiff_t>(begin),
            order_.begin() + static_cast<std::ptrdiff_t>(middle),
            order_.begin() + static_cast<std::ptrdiff_t>(end),
            [points, axis, this](std::size_t lhs, std::size_t rhs) {
                budget_.consume(1U);
                const double lhsValue = coordinate((*points)[lhs], axis);
                const double rhsValue = coordinate((*points)[rhs], axis);
                return lhsValue < rhsValue ||
                       (lhsValue == rhsValue && lhs < rhs);
            });

        const std::size_t nodeIndex = nodes_.size();
        nodes_.push_back(Node{order_[middle], kNoPointMatch, kNoPointMatch, axis});
        const std::size_t left = build(begin, middle, depth + 1);
        const std::size_t right = build(middle + 1U, end, depth + 1);
        nodes_[nodeIndex].left = left;
        nodes_[nodeIndex].right = right;
        return nodeIndex;
    }

    void queryNode(std::size_t nodeIndex,
                   const Point3d& query,
                   SearchMagnitude& bestDistance,
                   std::size_t& bestIndex) const
    {
        if (nodeIndex == kNoPointMatch) return;
        budget_.consume(1U);

        const Node& node = nodes_[nodeIndex];
        const Point3d& point = (*points_)[node.pointIndex];
        const SearchMagnitude distance = searchSquaredDistance(
            searchDifference(query.x, point.x),
            searchDifference(query.y, point.y),
            searchDifference(query.z, point.z));
        if (distance < bestDistance ||
            (distance == bestDistance &&
             (bestIndex == kNoPointMatch || node.pointIndex < bestIndex))) {
            bestDistance = distance;
            bestIndex = node.pointIndex;
        }

        const double queryAxis = coordinate(query, node.axis);
        const double pointAxis = coordinate(point, node.axis);
        const SearchMagnitude splitDistance =
            searchSquaredDistance(searchDifference(queryAxis, pointAxis));
        const std::size_t nearChild = queryAxis < pointAxis ? node.left : node.right;
        const std::size_t farChild = queryAxis < pointAxis ? node.right : node.left;
        queryNode(nearChild, query, bestDistance, bestIndex);
        if (splitDistance <= bestDistance) {
            queryNode(farChild, query, bestDistance, bestIndex);
        }
    }

    const std::vector<Point3d>* points_;
    spatial::RadiusSearchBudget& budget_;
    std::vector<std::size_t> order_;
    std::vector<Node> nodes_;
    std::size_t root_;
};

} // namespace detail

inline bool findMirroredPointMatches(const std::vector<Point3d>& points,
                                     double tolerance,
                                     std::size_t maximumPointCount,
                                     std::vector<std::size_t>& matches,
                                     std::size_t maximumWork = generation::kMaxWorkItemCount) try
{
    std::size_t resultSlots = 0U;
    if (!std::isfinite(tolerance) || tolerance < 0.0 ||
        points.size() > maximumPointCount ||
        !generation::checkedSum(points.size(), matches.size(),
                                generation::kMaxGeneratedPointCount, resultSlots) ||
        !validPointSearchStorage(points.size(), resultSlots)) {
        return false;
    }
    // One shared allowance covers validation, index construction/comparisons,
    // and all queries. Equal-distance tie breaking remains smallest input ID.
    spatial::RadiusSearchBudget budget(maximumWork);
    budget.consume(points.size());
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Point3d& point = points[index];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) {
            return false;
        }
    }

    const detail::SymmetryPointIndex index(points, budget);
    std::vector<std::size_t> candidate(points.size(), kNoPointMatch);
    for (std::size_t pointIndex = 0U; pointIndex < points.size(); ++pointIndex) {
        candidate[pointIndex] =
            index.nearestMirrored(pointIndex, tolerance);
    }
    matches.swap(candidate);
    return true;
}
catch (const std::length_error&) {
    // Never publish a partially searched result when the allowance is spent.
    return false;
}

} // namespace mesh
} // namespace dommy
