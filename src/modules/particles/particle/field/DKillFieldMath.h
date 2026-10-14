#pragma once

#include "common/utility/DNumericValidation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dommy {
namespace kill_field {

enum Condition
{
    kEqual = 0,
    kNotEqual = 1,
    kGreaterThan = 2,
    kGreaterOrEqual = 3,
    kLessThan = 4,
    kLessOrEqual = 5
};

inline bool compareScalar(int condition, double lhs, double rhs)
{
    if (!std::isfinite(lhs) || !std::isfinite(rhs)) return false;
    switch (condition) {
        case kEqual: return lhs == rhs;
        case kNotEqual: return lhs != rhs;
        case kGreaterThan: return lhs > rhs;
        case kGreaterOrEqual: return lhs >= rhs;
        case kLessThan: return lhs < rhs;
        case kLessOrEqual: return lhs <= rhs;
        default: return false;
    }
}

struct Point3d
{
    Point3d() : x(0.0), y(0.0), z(0.0) {}
    Point3d(double xValue, double yValue, double zValue)
        : x(xValue), y(yValue), z(zValue) {}

    double x;
    double y;
    double z;
};

inline bool isFinite(const Point3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

inline bool compareComponents(int condition,
                              const Point3d& lhs,
                              const Point3d& rhs)
{
    if (!isFinite(lhs) || !isFinite(rhs)) return false;
    switch (condition) {
        case kEqual:
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        case kNotEqual:
            return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
        case kGreaterThan:
            return lhs.x > rhs.x && lhs.y > rhs.y && lhs.z > rhs.z;
        case kGreaterOrEqual:
            return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z;
        case kLessThan:
            return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z;
        case kLessOrEqual:
            return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z;
        default:
            return false;
    }
}

inline bool vectorLength(const Point3d& value, double& result)
{
    if (!isFinite(value)) return false;
    result = std::hypot(std::hypot(value.x, value.y), value.z);
    return std::isfinite(result);
}

inline bool compareMagnitude(int condition,
                             const Point3d& lhs,
                             const Point3d& rhs)
{
    double lhsLength = 0.0;
    double rhsLength = 0.0;
    return vectorLength(lhs, lhsLength) && vectorLength(rhs, rhsLength) &&
           compareScalar(condition, lhsLength, rhsLength);
}

inline bool vectorAngle(const Point3d& lhs,
                        const Point3d& rhs,
                        double& result)
{
    double lhsLength = 0.0;
    double rhsLength = 0.0;
    if (!vectorLength(lhs, lhsLength) || !vectorLength(rhs, rhsLength) ||
        lhsLength <= std::numeric_limits<double>::epsilon() ||
        rhsLength <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    const double normalizedDot =
        (lhs.x / lhsLength) * (rhs.x / rhsLength) +
        (lhs.y / lhsLength) * (rhs.y / rhsLength) +
        (lhs.z / lhsLength) * (rhs.z / rhsLength);
    if (!std::isfinite(normalizedDot)) return false;
    const double clampedDot = std::max(-1.0, std::min(1.0, normalizedDot));
    result = std::acos(clampedDot);
    return std::isfinite(result);
}

inline bool compareAngle(int condition,
                         const Point3d& lhs,
                         const Point3d& rhs,
                         double threshold)
{
    double angle = 0.0;
    return std::isfinite(threshold) && vectorAngle(lhs, rhs, angle) &&
           compareScalar(condition, angle, threshold);
}

struct IdInterval
{
    IdInterval(std::int64_t beginValue, std::int64_t endValue)
        : begin(beginValue), end(endValue) {}

    std::int64_t begin;
    std::int64_t end;
};

inline bool parseNonNegativeInteger(const std::string& text,
                                    std::size_t begin,
                                    std::size_t end,
                                    std::int64_t& value)
{
    while (begin < end && std::isspace(
               static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(
               static_cast<unsigned char>(text[end - 1U]))) {
        --end;
    }
    if (begin == end) return false;

    std::int64_t result = 0;
    for (std::size_t index = begin; index < end; ++index) {
        const unsigned char character =
            static_cast<unsigned char>(text[index]);
        if (character < '0' || character > '9') return false;
        const int digit = static_cast<int>(character - '0');
        if (result >
            (static_cast<std::int64_t>(std::numeric_limits<int>::max()) -
             digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

inline bool parseIdExpression(const std::string& expression,
                              std::size_t maximumBytes,
                              std::size_t maximumIntervals,
                              std::vector<IdInterval>& intervals)
{
    intervals.clear();
    if (expression.size() > maximumBytes) return false;

    std::size_t tokenBegin = 0U;
    while (tokenBegin < expression.size()) {
        const std::size_t tokenEnd = expression.find(';', tokenBegin);
        const std::size_t end = tokenEnd == std::string::npos
            ? expression.size() : tokenEnd;
        if (intervals.size() >= maximumIntervals) return false;

        const std::size_t dash = expression.find('-', tokenBegin);
        if (dash != std::string::npos && dash < end) {
            if (expression.find('-', dash + 1U) < end) return false;
            std::int64_t rangeBegin = 0;
            std::int64_t rangeEnd = 0;
            if (!parseNonNegativeInteger(
                    expression, tokenBegin, dash, rangeBegin) ||
                !parseNonNegativeInteger(
                    expression, dash + 1U, end, rangeEnd) ||
                rangeBegin > rangeEnd) {
                return false;
            }
            if (rangeBegin < rangeEnd) {
                intervals.push_back(IdInterval(rangeBegin, rangeEnd));
            }
        } else {
            std::int64_t id = 0;
            if (!parseNonNegativeInteger(
                    expression, tokenBegin, end, id)) {
                return false;
            }
            intervals.push_back(IdInterval(id, id + 1));
        }

        if (tokenEnd == std::string::npos) break;
        tokenBegin = tokenEnd + 1U;
        if (tokenBegin == expression.size()) return false;
    }

    if (expression.empty()) return true;
    std::sort(intervals.begin(), intervals.end(),
        [](const IdInterval& lhs, const IdInterval& rhs) {
            return lhs.begin < rhs.begin ||
                (lhs.begin == rhs.begin && lhs.end < rhs.end);
        });
    std::vector<IdInterval> merged;
    merged.reserve(intervals.size());
    for (std::size_t index = 0U; index < intervals.size(); ++index) {
        const IdInterval& interval = intervals[index];
        if (merged.empty() || interval.begin > merged.back().end) {
            merged.push_back(interval);
        } else if (interval.end > merged.back().end) {
            merged.back().end = interval.end;
        }
    }
    intervals.swap(merged);
    return true;
}

inline bool containsId(const std::vector<IdInterval>& intervals, int id)
{
    if (id < 0) return false;
    const std::int64_t value = id;
    std::size_t begin = 0U;
    std::size_t end = intervals.size();
    while (begin < end) {
        const std::size_t middle = begin + (end - begin) / 2U;
        if (intervals[middle].end <= value) {
            begin = middle + 1U;
        } else {
            end = middle;
        }
    }
    return begin < intervals.size() &&
           intervals[begin].begin <= value && value < intervals[begin].end;
}

struct CellKey
{
    CellKey() : x(0), y(0), z(0) {}
    CellKey(std::int64_t xValue,
            std::int64_t yValue,
            std::int64_t zValue)
        : x(xValue), y(yValue), z(zValue) {}

    bool operator==(const CellKey& rhs) const
    {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
};

struct CellKeyHash
{
    std::size_t operator()(const CellKey& key) const
    {
        std::size_t seed = static_cast<std::size_t>(key.x);
        seed ^= static_cast<std::size_t>(key.y) +
            static_cast<std::size_t>(0x9e3779b9U) +
            (seed << 6U) + (seed >> 2U);
        seed ^= static_cast<std::size_t>(key.z) +
            static_cast<std::size_t>(0x9e3779b9U) +
            (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

inline bool makeCellKey(const Point3d& point,
                        double cellSize,
                        CellKey& key)
{
    if (!isFinite(point) || !std::isfinite(cellSize) || cellSize <= 0.0) {
        return false;
    }
    const double values[3] = {point.x, point.y, point.z};
    std::int64_t coordinates[3] = {0, 0, 0};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        const long double coordinate = std::floor(
            static_cast<long double>(values[axis]) /
            static_cast<long double>(cellSize));
        // The shared conversion also rejects 2^63 when INT64_MAX rounds up
        // in a 53-bit long double. Publish the key only after all axes pass.
        if (!numeric::checkedNumericCast(coordinate, coordinates[axis])) {
            return false;
        }
    }
    key = CellKey(coordinates[0], coordinates[1], coordinates[2]);
    return true;
}

inline bool pointsWithinDistance(const Point3d& lhs,
                                 const Point3d& rhs,
                                 double distance)
{
    if (!isFinite(lhs) || !isFinite(rhs) ||
        !std::isfinite(distance) || distance < 0.0) {
        return false;
    }
    const long double dx =
        static_cast<long double>(lhs.x) - static_cast<long double>(rhs.x);
    const long double dy =
        static_cast<long double>(lhs.y) - static_cast<long double>(rhs.y);
    const long double dz =
        static_cast<long double>(lhs.z) - static_cast<long double>(rhs.z);
    const long double maximumDistance = distance;
    // An overflowing difference is necessarily outside any finite radius.
    // Reject by axis before scaling so each normalized component is in [-1, 1].
    if (std::fabs(dx) > maximumDistance ||
        std::fabs(dy) > maximumDistance ||
        std::fabs(dz) > maximumDistance) {
        return false;
    }
    if (distance == 0.0) return true; // Only coincident points reach this path.

    // Do not square raw coordinates/radii or form a subnormal world-space
    // length. Unit-radius comparison works even when long double == double.
    return std::hypot(std::hypot(dx / maximumDistance, dy / maximumDistance),
                      dz / maximumDistance) <= 1.0L;
}

inline bool findNeighborPresence(
    const std::vector<Point3d>& points,
    double maximumDistance,
    std::size_t maximumWork,
    std::vector<unsigned char>& hasNeighbor)
{
    hasNeighbor.assign(points.size(), 0U);
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0) {
        return false;
    }
    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (!isFinite(points[index])) return false;
    }
    if (points.size() < 2U) return true;

    if (maximumDistance == 0.0) {
        std::size_t sortLevels = 0U;
        for (std::size_t count = points.size(); count > 1U; count >>= 1U) {
            ++sortLevels;
        }
        if (sortLevels > 0U && points.size() > maximumWork / sortLevels) {
            return false;
        }
        std::vector<std::size_t> order(points.size());
        for (std::size_t index = 0U; index < order.size(); ++index) {
            order[index] = index;
        }
        std::sort(order.begin(), order.end(),
            [&points](std::size_t lhs, std::size_t rhs) {
                const Point3d& a = points[lhs];
                const Point3d& b = points[rhs];
                if (a.x != b.x) return a.x < b.x;
                if (a.y != b.y) return a.y < b.y;
                return a.z < b.z;
            });
        std::size_t groupBegin = 0U;
        while (groupBegin < order.size()) {
            std::size_t groupEnd = groupBegin + 1U;
            const Point3d& first = points[order[groupBegin]];
            while (groupEnd < order.size()) {
                const Point3d& candidate = points[order[groupEnd]];
                if (candidate.x != first.x || candidate.y != first.y ||
                    candidate.z != first.z) {
                    break;
                }
                ++groupEnd;
            }
            if (groupEnd - groupBegin > 1U) {
                for (std::size_t index = groupBegin;
                     index < groupEnd; ++index) {
                    hasNeighbor[order[index]] = 1U;
                }
            }
            groupBegin = groupEnd;
        }
        return true;
    }

    typedef std::unordered_map<
        CellKey, std::vector<std::size_t>, CellKeyHash> CellMap;
    CellMap cells;
    cells.reserve(points.size());
    std::vector<CellKey> pointCells(points.size());
    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (!makeCellKey(points[index], maximumDistance, pointCells[index])) {
            return false;
        }
        cells[pointCells[index]].push_back(index);
    }

    std::size_t work = 0U;
    for (std::size_t pointIndex = 0U;
         pointIndex < points.size(); ++pointIndex) {
        if (hasNeighbor[pointIndex] != 0U) continue;
        const CellKey& base = pointCells[pointIndex];
        bool found = false;
        for (int z = -1; z <= 1 && !found; ++z) {
            for (int y = -1; y <= 1 && !found; ++y) {
                for (int x = -1; x <= 1 && !found; ++x) {
                    if (++work > maximumWork) return false;
                    if ((x < 0 && base.x == std::numeric_limits<std::int64_t>::min()) ||
                        (x > 0 && base.x == std::numeric_limits<std::int64_t>::max()) ||
                        (y < 0 && base.y == std::numeric_limits<std::int64_t>::min()) ||
                        (y > 0 && base.y == std::numeric_limits<std::int64_t>::max()) ||
                        (z < 0 && base.z == std::numeric_limits<std::int64_t>::min()) ||
                        (z > 0 && base.z == std::numeric_limits<std::int64_t>::max())) {
                        continue;
                    }
                    const CellKey neighborKey(
                        base.x + x, base.y + y, base.z + z);
                    const typename CellMap::const_iterator cell =
                        cells.find(neighborKey);
                    if (cell == cells.end()) continue;
                    const std::vector<std::size_t>& candidates = cell->second;
                    for (std::size_t candidateOffset = 0U;
                         candidateOffset < candidates.size();
                         ++candidateOffset) {
                        if (++work > maximumWork) return false;
                        const std::size_t candidateIndex =
                            candidates[candidateOffset];
                        if (candidateIndex == pointIndex) continue;
                        if (pointsWithinDistance(
                                points[pointIndex], points[candidateIndex],
                                maximumDistance)) {
                            hasNeighbor[pointIndex] = 1U;
                            hasNeighbor[candidateIndex] = 1U;
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    return true;
}

} // namespace kill_field
} // namespace dommy
