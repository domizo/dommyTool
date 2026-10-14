#pragma once

#include "DGenerationLimits.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dommy {
namespace spatial {

// One budget per locally owned index, shared by its parallel radius queries.
// Never return a truncated successful result when the budget is exhausted.
class RadiusSearchBudget {
public:
    explicit RadiusSearchBudget(
        std::size_t limit = generation::kMaxWorkItemCount) : remaining_(limit) {}

    void consume(std::size_t count) {
        std::size_t remaining = remaining_.load(std::memory_order_relaxed);
        do {
            if (count > remaining) {
                throw std::length_error("Radius search exceeded its work budget");
            }
        } while (!remaining_.compare_exchange_weak(
            remaining, remaining - count, std::memory_order_relaxed));
    }

private:
    std::atomic<std::size_t> remaining_;
};

// nanoflann ResultSet protocol. Retain at most capacity nearest points while
// traversing, not after collecting/sorting every point inside the radius.
// Equal distances are ordered by ID; the radius boundary remains exclusive.
class BoundedRadiusResultSet {
public:
    typedef std::pair<double, std::size_t> Match;

    BoundedRadiusResultSet(double squaredRadius, std::size_t capacity,
                           RadiusSearchBudget& budget, std::size_t leafSize,
                           std::size_t excludedId = std::numeric_limits<std::size_t>::max())
        : radius_(squaredRadius), capacity_(capacity), budget_(budget),
          leafSize_(std::max(std::size_t(1), leafSize)), excludedId_(excludedId),
          heapWork_(1U) {
        if (!std::isfinite(radius_) || radius_ <= 0.0 ||
            capacity_ > generation::kMaxGeneratedPointCount) {
            throw std::invalid_argument("Invalid bounded radius search parameters");
        }
        for (std::size_t count = capacity_; count > 1U; count /= 2U) ++heapWork_;
        // Sparse/unlimited queries need not allocate the worst-case result
        // size up front. Explicit growth also never reserves above the cap.
        matches_.reserve(std::min(capacity_, std::size_t(64U)));
    }

    std::size_t size() const { return matches_.size(); }
    bool full() const { return true; }

    void addPoint(double distance, std::size_t id) {
        if (!std::isfinite(distance) || distance < 0.0) {
            throw std::invalid_argument("Invalid radius search distance");
        }
        if (capacity_ == 0U || id == excludedId_ || distance >= radius_) return;
        budget_.consume(2U * heapWork_ + 1U);
        const Match match(distance, id);
        if (matches_.size() < capacity_) {
            if (matches_.size() == matches_.capacity()) {
                matches_.reserve(std::min(capacity_,
                    std::max(std::size_t(1U), matches_.capacity() * 2U)));
            }
            matches_.push_back(match);
            std::push_heap(matches_.begin(), matches_.end());
        } else if (match < matches_.front()) {
            std::pop_heap(matches_.begin(), matches_.end());
            matches_.back() = match;
            std::push_heap(matches_.begin(), matches_.end());
        }
    }

    double worstDist() const {
        // The pinned nanoflann checks worstDist before every leaf and branch.
        // Charging a full leaf here conservatively covers distance evaluation
        // even for candidates that never reach addPoint().
        budget_.consume(leafSize_);
        if (capacity_ == 0U) return 0.0;
        if (matches_.size() < capacity_) return radius_;
        return std::min(radius_, std::nextafter(matches_.front().first,
            std::numeric_limits<double>::infinity()));
    }

    std::vector<Match> takeSortedMatches() {
        std::size_t work = 0U;
        if (!generation::checkedProduct(matches_.size(), 2U * heapWork_ + 1U,
                generation::kMaxWorkItemCount, work)) {
            throw std::length_error("Radius result sorting exceeded its work budget");
        }
        budget_.consume(work);
        std::sort_heap(matches_.begin(), matches_.end());
        std::vector<Match> sorted;
        sorted.swap(matches_);
        return sorted;
    }

private:
    double radius_;
    std::size_t capacity_;
    RadiusSearchBudget& budget_;
    std::size_t leafSize_, excludedId_, heapWork_;
    std::vector<Match> matches_;
};

} // namespace spatial
} // namespace dommy
