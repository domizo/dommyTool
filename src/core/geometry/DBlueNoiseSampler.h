#pragma once

#include "DGeometryTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace dommy {
namespace sampling {

// A project-owned 2D implementation of greedy crowding elimination.
// Algorithm reference: Cem Yuksel, "Sample Elimination for Generating Poisson
// Disk Sample Sets", Computer Graphics Forum 34(2), 2015.
// https://www.cemyuksel.com/research/sampleelimination/
// The compact quadratic weight, uniform-grid index and indexed priority array
// below are implemented here, not adapted from the reference code release.
// The radius is an interaction range, not a promised minimum separation.
struct BlueNoiseLimits {
    std::size_t maxCandidates = generation::kMaxGeneratedPointCount;
    std::size_t maxBytes = 256U * 1024U * 1024U;
    std::size_t maxWork = generation::kMaxWorkItemCount;
};

namespace blue_noise_detail {

const std::size_t absent = std::numeric_limits<std::size_t>::max();

struct Plan {
    std::size_t dimension = 1U;
    std::size_t cells = 1U;
    double radius = 1.0;
};

inline bool addStorage(std::size_t count, std::size_t width,
                       std::size_t limit, std::size_t& total)
{
    std::size_t bytes = 0U;
    return generation::checkedProduct(count, width, limit, bytes) &&
           generation::checkedSum(total, bytes, limit, total);
}

inline bool makePlan(std::size_t candidates, std::size_t wanted,
                     double radius, const BlueNoiseLimits& limits, Plan& result)
{
    if (!std::isfinite(radius) || radius < -1.0 || wanted > candidates ||
        candidates > limits.maxCandidates ||
        candidates > generation::kMaxGeneratedPointCount) return false;
    if (wanted == 0U) {
        result = Plan();
        result.cells = 0U;
        return true;
    }

    Plan plan;
    // The unit-square hexagonal packing estimate controls the default range.
    plan.radius = radius > 0.0 ? radius :
        std::sqrt(2.0 / std::sqrt(3.0)) / std::sqrt(static_cast<double>(wanted));
    const std::size_t maximumDimension = static_cast<std::size_t>(
        std::ceil(std::sqrt(static_cast<double>(candidates))));
    if (plan.radius < 1.0) {
        const double minimumCellWidth = 1.0 / static_cast<double>(maximumDimension);
        plan.dimension = plan.radius < minimumCellWidth ? maximumDimension :
            std::max(std::size_t(1U), static_cast<std::size_t>(1.0 / plan.radius));
    }
    if (!memory::checkedMultiply(plan.dimension, plan.dimension, plan.cells)) return false;

    // Input points, scores, cell links, priority IDs/positions, cell heads,
    // staged output and conservative space for the caller's point handoff.
    // This is algorithm admission, not a cap on Maya's native allocator.
    std::size_t bytes = 0U;
    if (!addStorage(candidates, sizeof(geometry::Point2), limits.maxBytes, bytes) ||
        !addStorage(candidates, sizeof(double), limits.maxBytes, bytes) ||
        !addStorage(candidates, 3U * sizeof(std::size_t), limits.maxBytes, bytes) ||
        !addStorage(plan.cells, sizeof(std::size_t), limits.maxBytes, bytes) ||
        !addStorage(wanted, sizeof(geometry::Point2) + 64U, limits.maxBytes, bytes)) return false;
    result = plan;
    return true;
}

inline bool validPoint(const geometry::Point2& point)
{
    return geometry::isFinite(point) && point.x >= 0.0 && point.x <= 1.0 &&
           point.y >= 0.0 && point.y <= 1.0;
}

// Static cell membership remains valid as points are removed. Inactive IDs are
// skipped by the consumer; no adjacency matrix or growing queue is stored.
class Grid {
public:
    Grid(const std::vector<geometry::Point2>& points, const Plan& plan)
        : points_(points), plan_(plan), heads_(plan.cells, absent),
          next_(points.size(), absent)
    {
        for (std::size_t id = 0U; id < points.size(); ++id) {
            const std::size_t cell = coordinate(points[id].y) * plan.dimension +
                                     coordinate(points[id].x);
            next_[id] = heads_[cell];
            heads_[cell] = id;
        }
    }

    template <typename Visitor>
    bool visit(std::size_t id, generation::WorkBudget& budget, Visitor visitor) const
    {
        const geometry::Point2& point = points_[id];
        const std::size_t x = coordinate(point.x), y = coordinate(point.y);
        // Usually one cell in either direction. Ceil also covers roundoff at a
        // cell-width boundary. Large ranges use the single-cell grid.
        const std::size_t span = plan_.dimension == 1U ? 0U :
            std::min(plan_.dimension - 1U, static_cast<std::size_t>(
                std::ceil(plan_.radius * static_cast<double>(plan_.dimension))));
        const std::size_t firstX = x > span ? x - span : 0U;
        const std::size_t firstY = y > span ? y - span : 0U;
        const std::size_t lastX = std::min(plan_.dimension - 1U, x + span);
        const std::size_t lastY = std::min(plan_.dimension - 1U, y + span);
        for (std::size_t row = firstY; row <= lastY; ++row) {
            for (std::size_t column = firstX; column <= lastX; ++column) {
                if (!budget.consume(1U)) return false;
                for (std::size_t other = heads_[row * plan_.dimension + column];
                     other != absent; other = next_[other]) {
                    if (!budget.consume(8U)) return false;
                    if (other == id) continue;
                    const double distance = std::hypot(
                        point.x - points_[other].x, point.y - points_[other].y);
                    if (distance >= plan_.radius) continue;
                    const double proximity = 1.0 - distance / plan_.radius;
                    if (!visitor(other, proximity * proximity)) return false;
                }
            }
        }
        return true;
    }

private:
    std::size_t coordinate(double value) const
    {
        return std::min(plan_.dimension - 1U,
            static_cast<std::size_t>(value * static_cast<double>(plan_.dimension)));
    }

    const std::vector<geometry::Point2>& points_;
    Plan plan_;
    std::vector<std::size_t> heads_, next_;
};

// One slot per candidate. Scores only decrease after a removal, so updating an
// affected point requires lowering its priority in place, not adding stale
// entries to an unbounded lazy queue. Equal scores are resolved by input ID.
class Priority {
public:
    explicit Priority(const std::vector<double>& scores)
        : scores_(scores), ids_(scores.size()), positions_(scores.size())
    {
        for (std::size_t id = 0U; id < scores.size(); ++id) ids_[id] = positions_[id] = id;
    }

    bool build(generation::WorkBudget& budget)
    {
        for (std::size_t parent = ids_.size() / 2U; parent > 0U; --parent)
            if (!lower(parent - 1U, budget)) return false;
        return true;
    }

    bool active(std::size_t id) const { return positions_[id] != absent; }

    bool remove(std::size_t& id, generation::WorkBudget& budget)
    {
        if (ids_.empty() || !budget.consume(1U)) return false;
        id = ids_.front();
        const std::size_t replacement = ids_.back();
        ids_.pop_back();
        positions_[id] = absent;
        if (ids_.empty()) return true;
        ids_.front() = replacement;
        positions_[replacement] = 0U;
        return lower(0U, budget);
    }

    bool changed(std::size_t id, generation::WorkBudget& budget)
    {
        return lower(positions_[id], budget);
    }

private:
    bool higher(std::size_t first, std::size_t second) const
    {
        return scores_[first] > scores_[second] ||
               (scores_[first] == scores_[second] && first > second);
    }

    bool lower(std::size_t slot, generation::WorkBudget& budget)
    {
        while (slot < ids_.size() / 2U) {
            if (!budget.consume(4U)) return false;
            std::size_t child = slot * 2U + 1U;
            if (child + 1U < ids_.size() && higher(ids_[child + 1U], ids_[child])) ++child;
            if (!higher(ids_[child], ids_[slot])) break;
            std::swap(ids_[slot], ids_[child]);
            positions_[ids_[slot]] = slot;
            positions_[ids_[child]] = child;
            slot = child;
        }
        return true;
    }

    const std::vector<double>& scores_;
    std::vector<std::size_t> ids_, positions_;
};

inline bool select(const std::vector<geometry::Point2>& candidates,
                   std::size_t wanted, const Plan& plan,
                   generation::WorkBudget& budget,
                   std::vector<geometry::Point2>& result)
{
    if (!budget.consumeProduct(candidates.size(), 6U) ||
        !budget.consume(plan.cells) || !budget.consume(wanted)) return false;
    for (std::size_t id = 0U; id < candidates.size(); ++id)
        if (!validPoint(candidates[id])) return false;
    if (wanted == 0U) {
        std::vector<geometry::Point2>().swap(result);
        return true;
    }
    if (wanted == candidates.size()) {
        std::vector<geometry::Point2> selected(candidates);
        result.swap(selected);
        return true;
    }

    Grid grid(candidates, plan);
    std::vector<double> scores(candidates.size(), 0.0);
    for (std::size_t id = 0U; id < candidates.size(); ++id) {
        if (!grid.visit(id, budget, [&](std::size_t other, double contribution) {
                if (other > id) {
                    scores[id] += contribution;
                    scores[other] += contribution;
                }
                return true;
            })) return false;
    }

    Priority priority(scores);
    if (!priority.build(budget)) return false;
    for (std::size_t removed = candidates.size() - wanted; removed > 0U; --removed) {
        std::size_t id = absent;
        if (!priority.remove(id, budget) ||
            !grid.visit(id, budget, [&](std::size_t other, double contribution) {
                if (!priority.active(other)) return true;
                scores[other] = std::max(0.0, scores[other] - contribution);
                return priority.changed(other, budget);
            })) return false;
    }

    std::vector<geometry::Point2> selected;
    selected.reserve(wanted);
    for (std::size_t id = 0U; id < candidates.size(); ++id)
        if (priority.active(id)) selected.push_back(candidates[id]);
    if (selected.size() != wanted) return false;
    result.swap(selected);
    return true;
}

} // namespace blue_noise_detail

// All candidates must lie in the unit square. Failure/exception leaves the
// caller's output unchanged. Successful output follows original candidate IDs;
// it does not reproduce a historical sampler's heap order or point choices.
inline bool selectBlueNoise2D(const std::vector<geometry::Point2>& candidates,
                             std::size_t wanted, double interactionRadius,
                             std::vector<geometry::Point2>& result,
                             const BlueNoiseLimits& limits = BlueNoiseLimits())
{
    try {
        blue_noise_detail::Plan plan;
        if (!blue_noise_detail::makePlan(candidates.size(), wanted,
                interactionRadius, limits, plan)) return false;
        generation::WorkBudget budget(limits.maxWork);
        return blue_noise_detail::select(candidates, wanted, plan, budget, result);
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }
}

// Keep the emitter's historical N+1000 candidate policy and default-radius
// convention (finite values in [-1,0] select the automatic radius).
// The supplied random generator is a trusted internal/test boundary. It may
// throw; staging still preserves result, and the owning Maya compute catches it.
template <typename RandomGenerator>
bool generateBlueNoise2D(std::size_t count, double interactionRadius,
                         RandomGenerator& random,
                         std::vector<geometry::Point2>& result,
                         const BlueNoiseLimits& limits = BlueNoiseLimits())
{
    try {
        std::size_t candidateCount = 0U;
        if (count && !generation::checkedSum(count, 1000U,
                generation::kMaxGeneratedPointCount, candidateCount)) return false;
        blue_noise_detail::Plan plan;
        if (!blue_noise_detail::makePlan(candidateCount, count,
                interactionRadius, limits, plan)) return false;
        generation::WorkBudget budget(limits.maxWork);
        if (!budget.consumeProduct(candidateCount, 2U)) return false;
        std::vector<geometry::Point2> candidates(candidateCount);
        for (std::size_t id = 0U; id < candidateCount; ++id) {
            const double x = random.uniform(0.0, 1.0);
            const double y = random.uniform(0.0, 1.0);
            candidates[id] = geometry::Point2(x, y);
            if (!blue_noise_detail::validPoint(candidates[id])) return false;
        }
        return blue_noise_detail::select(candidates, count, plan, budget, result);
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }
}

} // namespace sampling
} // namespace dommy
