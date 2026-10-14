#pragma once

#include "geometry/DGeometryTypes.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DWorkBudget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

struct RegularizedPointSet {
    std::vector<Point3> points;
    std::vector<std::size_t> sourceIndices;

    void clear()
    {
        points.clear();
        sourceIndices.clear();
    }

    void swap(RegularizedPointSet& other)
    {
        points.swap(other.points);
        sourceIndices.swap(other.sourceIndices);
    }
};

namespace regularizer_detail {

struct CellKey {
    CellKey() : x(0), y(0), z(0) {}
    CellKey(long long xValue, long long yValue, long long zValue)
        : x(xValue), y(yValue), z(zValue) {}

    bool operator<(const CellKey& rhs) const
    {
        return x < rhs.x ||
               (x == rhs.x &&
                (y < rhs.y || (y == rhs.y && z < rhs.z)));
    }

    long long x;
    long long y;
    long long z;
};

inline bool quantize(double value,
                     double inverseCellSize,
                     long long& result)
{
    return numeric::checkedSpatialCellIndex(value, inverseCellSize, result);
}

inline bool cellKey(const Point3& point,
                    double inverseCellSize,
                    CellKey& key)
{
    return quantize(point.x, inverseCellSize, key.x) &&
           quantize(point.y, inverseCellSize, key.y) &&
           quantize(point.z, inverseCellSize, key.z);
}

class SpatialGrid {
public:
    SpatialGrid() : m_inverseCellSize(0.0), m_valid(false) {}

    bool build(const std::vector<Point3>& points, double cellSize,
               generation::WorkBudget& budget)
    {
        m_buckets.clear();
        m_inverseCellSize = 0.0;
        m_valid = false;
        if (!std::isfinite(cellSize) || cellSize <= 0.0) return false;
        m_inverseCellSize = 1.0 / cellSize;
        if (!std::isfinite(m_inverseCellSize) ||
            m_inverseCellSize <= 0.0) {
            return false;
        }
        for (std::size_t index = 0U; index < points.size(); ++index) {
            if (!budget.consume(2U * generation::orderedContainerWork(m_buckets.size()) + 4U))
                return false;
            CellKey key;
            if (!isFinite(points[index]) ||
                !cellKey(points[index], m_inverseCellSize, key)) {
                m_buckets.clear();
                return false;
            }
            m_buckets[key].push_back(index);
        }
        m_valid = true;
        return true;
    }

    template <typename Visitor>
    bool visitNeighbors(const Point3& point,
                        generation::WorkBudget& budget,
                        Visitor visitor) const
    {
        if (!m_valid || !isFinite(point)) return false;
        CellKey center;
        if (!cellKey(point, m_inverseCellSize, center)) return false;
        for (long long xOffset = -1; xOffset <= 1; ++xOffset) {
            for (long long yOffset = -1; yOffset <= 1; ++yOffset) {
                for (long long zOffset = -1; zOffset <= 1; ++zOffset) {
                    if (!budget.consume(2U * generation::orderedContainerWork(m_buckets.size())))
                        return false;
                    const CellKey key(
                        center.x + xOffset,
                        center.y + yOffset,
                        center.z + zOffset);
                    const std::map<CellKey, std::vector<std::size_t> >::
                        const_iterator bucket = m_buckets.find(key);
                    if (bucket == m_buckets.end()) continue;
                    for (std::vector<std::size_t>::const_iterator index =
                             bucket->second.begin();
                         index != bucket->second.end(); ++index) {
                        if (!budget.consume(1U) || !visitor(*index)) return false;
                    }
                }
            }
        }
        return true;
    }

private:
    double m_inverseCellSize;
    bool m_valid;
    std::map<CellKey, std::vector<std::size_t> > m_buckets;
};

inline bool selectPoints(const std::vector<Point3>& input,
                         std::size_t targetCount,
                         double radius,
                         bool uniform,
                         std::vector<std::size_t>& output,
                         generation::WorkBudget& budget)
{
    if (targetCount == 0U || targetCount > input.size() || budget.exceeded()) return false;
    std::vector<std::size_t> selected;
    if (targetCount == input.size()) {
        if (!budget.consumeProduct(input.size(), 2U)) return false;
        selected.resize(input.size());
        for (std::size_t index = 0U; index < input.size(); ++index) {
            selected[index] = index;
        }
        output.swap(selected);
        return true;
    }

    if (!uniform) {
        if (!budget.consumeProduct(targetCount, 2U)) return false;
        selected.reserve(targetCount);
        for (std::size_t selection = 0U;
             selection < targetCount; ++selection) {
            const long double numerator =
                static_cast<long double>(selection) *
                static_cast<long double>(input.size());
            std::size_t index = static_cast<std::size_t>(
                numerator / static_cast<long double>(targetCount));
            if (index >= input.size()) index = input.size() - 1U;
            selected.push_back(index);
        }
        output.swap(selected);
        return true;
    }

    const double inverseRadius = 1.0 / radius;
    if (!std::isfinite(inverseRadius) || inverseRadius <= 0.0) {
        return false;
    }
    std::map<CellKey, std::vector<std::size_t> > cells;
    for (std::size_t index = 0U; index < input.size(); ++index) {
        if (!budget.consume(2U * generation::orderedContainerWork(cells.size()) + 4U))
            return false;
        CellKey key;
        if (!cellKey(input[index], inverseRadius, key)) return false;
        cells[key].push_back(index);
    }

    // Stable active-cell compaction preserves the historical layer order.
    // Each source entry is visited at most once, including exhausted cells.
    std::vector<const std::vector<std::size_t>* > groups;
    if (!budget.consume(cells.size())) return false;
    groups.reserve(cells.size());
    for (std::map<CellKey, std::vector<std::size_t> >::const_iterator cell =
             cells.begin(); cell != cells.end(); ++cell) {
        groups.push_back(&cell->second);
    }

    selected.reserve(targetCount);
    std::size_t layer = 0U;
    while (selected.size() < targetCount) {
        std::vector<std::size_t> layerCandidates;
        std::size_t activeCount = 0U;
        for (const std::vector<std::size_t>* group : groups) {
            if (!budget.consume(3U) || layer >= group->size()) return false;
            layerCandidates.push_back((*group)[layer]);
            if (layer + 1U < group->size()) groups[activeCount++] = group;
        }
        groups.resize(activeCount);
        if (layerCandidates.empty()) return false;
        const std::size_t remaining = targetCount - selected.size();
        if (!budget.consume(std::min(remaining, layerCandidates.size()))) return false;
        if (layerCandidates.size() <= remaining) {
            selected.insert(
                selected.end(),
                layerCandidates.begin(), layerCandidates.end());
        } else {
            for (std::size_t choice = 0U; choice < remaining; ++choice) {
                const long double numerator =
                    static_cast<long double>(choice) *
                    static_cast<long double>(layerCandidates.size());
                std::size_t candidate = static_cast<std::size_t>(
                    numerator / static_cast<long double>(remaining));
                if (candidate >= layerCandidates.size()) {
                    candidate = layerCandidates.size() - 1U;
                }
                selected.push_back(layerCandidates[candidate]);
            }
        }
        ++layer;
    }
    if (!budget.consumeProduct(selected.size(),
            2U * generation::orderedContainerWork(selected.size()) + 1U)) return false;
    std::sort(selected.begin(), selected.end());
    if (selected.size() != targetCount ||
        std::adjacent_find(selected.begin(), selected.end()) != selected.end()) return false;
    output.swap(selected);
    return true;
}

struct WeightedNeighbor {
    WeightedNeighbor() : index(0U), squaredDistance(0.0) {}
    WeightedNeighbor(std::size_t neighborIndex, double distance)
        : index(neighborIndex), squaredDistance(distance) {}

    bool operator<(const WeightedNeighbor& rhs) const
    {
        return squaredDistance < rhs.squaredDistance ||
               (squaredDistance == rhs.squaredDistance &&
                index < rhs.index);
    }

    std::size_t index;
    double squaredDistance;
};

inline bool keepNearest(std::vector<WeightedNeighbor>& neighbors,
                        std::size_t maximumNeighborCount,
                        generation::WorkBudget& budget)
{
    if (!budget.consumeProduct(neighbors.size(),
            2U * generation::orderedContainerWork(neighbors.size()) + 1U)) return false;
    std::sort(neighbors.begin(), neighbors.end());
    if (neighbors.size() > maximumNeighborCount) {
        neighbors.resize(maximumNeighborCount);
    }
    return true;
}

inline bool considerNearest(
    std::vector<WeightedNeighbor>& neighbors,
    const WeightedNeighbor& candidate,
    std::size_t maximumNeighborCount,
    generation::WorkBudget& budget)
{
    if (maximumNeighborCount == 0U) return false;
    if (neighbors.size() < maximumNeighborCount) {
        if (!budget.consume(1U)) return false;
        neighbors.push_back(candidate);
        return true;
    }
    if (!budget.consume(neighbors.size()) || !budget.consume(1U)) return false;
    std::vector<WeightedNeighbor>::iterator farthest =
        std::max_element(neighbors.begin(), neighbors.end());
    if (candidate < *farthest) {
        *farthest = candidate;
    }
    return true;
}

} // namespace regularizer_detail

inline bool regularizePointSet(
    const std::vector<Point3>& input,
    double retainPercentage,
    double neighborRadius,
    unsigned int iterations,
    bool uniform,
    RegularizedPointSet& output,
    std::size_t maximumPointCount,
    generation::WorkBudget& budget,
    std::size_t maximumNeighborCount = 128U)
{
    if (input.empty() || input.size() > maximumPointCount ||
        !std::isfinite(retainPercentage) ||
        retainPercentage < 0.0 || retainPercentage > 100.0 ||
        !std::isfinite(neighborRadius) || neighborRadius <= 0.0 ||
        iterations == 0U || budget.exceeded() ||
        maximumNeighborCount == 0U) {
        return false;
    }
    const double squaredRadius = neighborRadius * neighborRadius;
    if (!std::isfinite(squaredRadius) || !budget.consume(input.size())) return false;
    for (std::vector<Point3>::const_iterator point = input.begin();
         point != input.end(); ++point) {
        if (!isFinite(*point)) return false;
    }

    const long double retained =
        static_cast<long double>(input.size()) *
        static_cast<long double>(retainPercentage) / 100.0L;
    if (!std::isfinite(retained)) return false;
    std::size_t targetCount =
        static_cast<std::size_t>(std::floor(retained + 0.5L));
    targetCount = std::min<std::size_t>(input.size(), targetCount);

    RegularizedPointSet candidate;
    if (targetCount == 0U) {
        output.swap(candidate);
        return true;
    }
    if (!regularizer_detail::selectPoints(
            input, targetCount, neighborRadius, uniform,
            candidate.sourceIndices, budget)) {
        return false;
    }
    if (!budget.consume(targetCount)) return false;
    candidate.points.reserve(targetCount);
    for (std::vector<std::size_t>::const_iterator source =
             candidate.sourceIndices.begin();
         source != candidate.sourceIndices.end(); ++source) {
        candidate.points.push_back(input[*source]);
    }

    regularizer_detail::SpatialGrid inputGrid;
    if (!inputGrid.build(input, neighborRadius, budget)) return false;
    const double attractionScale = uniform ? 0.25 : 0.35;
    const double repulsionScale =
        neighborRadius * (uniform ? 0.08 : 0.035);
    for (unsigned int iteration = 0U; iteration < iterations; ++iteration) {
        if (!budget.consume(1U)) return false;
        regularizer_detail::SpatialGrid outputGrid;
        if (!outputGrid.build(candidate.points, neighborRadius, budget)) {
            return false;
        }
        if (!budget.consume(candidate.points.size())) return false;
        std::vector<Point3> next = candidate.points;
        for (std::size_t pointIndex = 0U;
             pointIndex < candidate.points.size(); ++pointIndex) {
            if (!budget.consume(2U)) return false;
            const Point3 current = candidate.points[pointIndex];
            std::vector<regularizer_detail::WeightedNeighbor>
                inputNeighbors;
            if (!inputGrid.visitNeighbors(
                    current, budget,
                    [&](std::size_t index) {
                        const double distance =
                            squaredDistance(current, input[index]);
                        if (distance <= squaredRadius) {
                            return regularizer_detail::considerNearest(
                                inputNeighbors,
                                regularizer_detail::WeightedNeighbor(
                                    index, distance),
                                maximumNeighborCount, budget);
                        }
                        return true;
                    })) {
                return false;
            }
            if (!regularizer_detail::keepNearest(
                    inputNeighbors, maximumNeighborCount, budget) ||
                !budget.consumeProduct(inputNeighbors.size(), 8U)) return false;

            Point3 attraction;
            double attractionWeight = 0.0;
            for (std::vector<regularizer_detail::WeightedNeighbor>::
                     const_iterator neighbor = inputNeighbors.begin();
                 neighbor != inputNeighbors.end(); ++neighbor) {
                const double distance =
                    std::sqrt(std::max(0.0, neighbor->squaredDistance));
                const double normalizedDistance =
                    std::max(0.0, 1.0 - distance / neighborRadius);
                const double weight =
                    normalizedDistance * normalizedDistance;
                attraction += input[neighbor->index] * weight;
                attractionWeight += weight;
            }
            if (attractionWeight > 0.0) {
                attraction =
                    attraction / attractionWeight - current;
            } else {
                attraction = Point3();
            }

            std::vector<regularizer_detail::WeightedNeighbor>
                outputNeighbors;
            if (!outputGrid.visitNeighbors(
                    current, budget,
                    [&](std::size_t index) {
                        if (index == pointIndex) return true;
                        const double distance = squaredDistance(
                            current, candidate.points[index]);
                        if (distance > 0.0 && distance <= squaredRadius) {
                            return regularizer_detail::considerNearest(
                                outputNeighbors,
                                regularizer_detail::WeightedNeighbor(
                                    index, distance),
                                maximumNeighborCount, budget);
                        }
                        return true;
                    })) {
                return false;
            }
            if (!regularizer_detail::keepNearest(
                    outputNeighbors, maximumNeighborCount, budget) ||
                !budget.consumeProduct(outputNeighbors.size(), 8U)) return false;

            Point3 repulsion;
            double repulsionWeight = 0.0;
            for (std::vector<regularizer_detail::WeightedNeighbor>::
                     const_iterator neighbor = outputNeighbors.begin();
                 neighbor != outputNeighbors.end(); ++neighbor) {
                const double distance =
                    std::sqrt(neighbor->squaredDistance);
                if (distance <= 1.0e-15) continue;
                const double normalizedDistance =
                    std::max(0.0, 1.0 - distance / neighborRadius);
                const double weight =
                    normalizedDistance * normalizedDistance;
                repulsion +=
                    (current - candidate.points[neighbor->index]) /
                    distance * weight;
                repulsionWeight += weight;
            }
            if (repulsionWeight > 0.0) {
                repulsion *= repulsionScale / repulsionWeight;
            }

            Point3 displacement =
                attraction * attractionScale + repulsion;
            const double displacementLength = length(displacement);
            const double maximumDisplacement = neighborRadius * 0.25;
            if (displacementLength > maximumDisplacement &&
                displacementLength > 0.0) {
                displacement *= maximumDisplacement / displacementLength;
            }
            const Point3 updated = current + displacement;
            if (!isFinite(updated)) return false;
            next[pointIndex] = updated;
        }
        candidate.points.swap(next);
    }

    if (!budget.consume(1U)) return false;
    output.swap(candidate);
    return true;
}

// Source-compatible entry point; all stages share this single bounded budget.
inline bool regularizePointSet(
    const std::vector<Point3>& input,
    double retainPercentage, double neighborRadius, unsigned int iterations,
    bool uniform, RegularizedPointSet& output, std::size_t maximumPointCount,
    std::size_t maximumWork, std::size_t maximumNeighborCount = 128U)
{
    generation::WorkBudget budget(maximumWork);
    return regularizePointSet(input, retainPercentage, neighborRadius, iterations,
                              uniform, output, maximumPointCount, budget, maximumNeighborCount);
}

} // namespace geometry
} // namespace dommy
