#pragma once

#include "DPointMeanAccumulator.h"
#include "DGenerationLimits.h"
#include "DNumericValidation.h"
#include <ImathVec.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace dommy {
namespace attribute {

// Keep integer means exact even for int64 endpoints: no floating conversion,
// wide-integer extension or overflowing sum. Divide each magnitude first and
// carry the remainder; subtract the two signs only after all terms are read.
template <typename T, bool Integral = std::is_integral<T>::value> class SmoothingMean;

template <typename T> class SmoothingMean<T, true> {
  public:
    explicit SmoothingMean(std::uint32_t count) : count_(count) {}
    bool add(T value) {
        if (count_ == 0U || added_ >= count_)
            return false;
        const bool negative = std::is_signed<T>::value && value < T(0);
        const std::uint64_t magnitude = negative
                                            ? std::uint64_t(0) - static_cast<std::uint64_t>(value)
                                            : static_cast<std::uint64_t>(value);
        std::uint64_t& quotient = negative ? negativeQuotient_ : positiveQuotient_;
        std::uint64_t& remainder = negative ? negativeRemainder_ : positiveRemainder_;
        const std::uint64_t part = magnitude / count_;
        if (part > std::numeric_limits<std::uint64_t>::max() - quotient)
            return false;
        quotient += part;
        remainder += magnitude % count_; // Less than twice the uint32 divisor.
        if (remainder >= count_) {
            if (quotient == std::numeric_limits<std::uint64_t>::max())
                return false;
            ++quotient;
            remainder -= count_;
        }
        ++added_;
        return true;
    }
    bool result(T& output) const {
        if (count_ == 0U || added_ != count_)
            return false;
        const bool negative =
            negativeQuotient_ > positiveQuotient_ ||
            (negativeQuotient_ == positiveQuotient_ && negativeRemainder_ > positiveRemainder_);
        const std::uint64_t larger = negative ? negativeQuotient_ : positiveQuotient_;
        const std::uint64_t smaller = negative ? positiveQuotient_ : negativeQuotient_;
        const std::uint64_t largerRemainder = negative ? negativeRemainder_ : positiveRemainder_;
        const std::uint64_t smallerRemainder = negative ? positiveRemainder_ : negativeRemainder_;
        std::uint64_t magnitude = larger - smaller;
        if (largerRemainder < smallerRemainder)
            --magnitude;
        // Fractional remainder is discarded toward zero, as in integer division.
        const std::uint64_t maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
        if (negative) {
            if (!std::is_signed<T>::value || magnitude > maximum + 1U)
                return false;
            output = magnitude == maximum + 1U ? std::numeric_limits<T>::min()
                                               : static_cast<T>(-static_cast<T>(magnitude));
        } else {
            if (magnitude > maximum)
                return false;
            output = static_cast<T>(magnitude);
        }
        return true;
    }

  private:
    std::uint32_t count_, added_ = 0U;
    std::uint64_t positiveQuotient_ = 0U, positiveRemainder_ = 0U;
    std::uint64_t negativeQuotient_ = 0U, negativeRemainder_ = 0U;
};

template <typename T> class SmoothingMean<T, false> {
  public:
    explicit SmoothingMean(std::uint32_t count) : count_(count) {}
    bool add(T value) {
        if (added_ >= count_ || !sum_.add(static_cast<double>(value)))
            return false;
        ++added_;
        return true;
    }
    bool result(T& output) const {
        double result = 0.0;
        return count_ != 0U && added_ == count_ && sum_.mean(count_, result) &&
               numeric::checkedNumericCast(result, output);
    }

  private:
    numeric::detail::PointMeanAccumulator sum_;
    std::uint32_t count_, added_ = 0U;
};

template <typename T> struct SmoothingComponents {
    using Scalar = T;
    static const std::size_t count = 1U;
    static T zero() {
        return T(0);
    }
    static T& at(T& value, std::size_t) {
        return value;
    }
    static const T& at(const T& value, std::size_t) {
        return value;
    }
};
template <typename T> struct SmoothingComponents<Imath::Vec2<T>> {
    using Scalar = T;
    static const std::size_t count = 2U;
    static Imath::Vec2<T> zero() {
        return Imath::Vec2<T>(T(0), T(0));
    }
    static T& at(Imath::Vec2<T>& value, std::size_t axis) {
        return value[axis];
    }
    static const T& at(const Imath::Vec2<T>& value, std::size_t axis) {
        return value[axis];
    }
};
template <typename T> struct SmoothingComponents<Imath::Vec3<T>> {
    using Scalar = T;
    static const std::size_t count = 3U;
    static Imath::Vec3<T> zero() {
        return Imath::Vec3<T>(T(0), T(0), T(0));
    }
    static T& at(Imath::Vec3<T>& value, std::size_t axis) {
        return value[axis];
    }
    static const T& at(const Imath::Vec3<T>& value, std::size_t axis) {
        return value[axis];
    }
};

struct SerialSmoothingPass {
    template <typename Row> void operator()(std::size_t count, Row row) const {
        for (std::size_t i = 0U; i < count; ++i)
            row(i);
    }
};

// Transactional Jacobi pass: every row reads the same immutable snapshot.
// A caller may supply a parallel runner; rows write disjoint, non-bool values.
template <typename T, typename Neighbors, typename Run>
bool smoothNeighborValues(const std::vector<T>& values, const Neighbors& neighbors,
                          std::size_t maximumSamples, std::vector<T>& output, Run run) {
    using Components = SmoothingComponents<T>;
    if (values.size() != neighbors.size() || maximumSamples == 0U ||
        values.size() > generation::kMaxGeneratedPointCount)
        return false;
    std::vector<T> candidate(values.size(), Components::zero());
    std::atomic<bool> valid(true);
    run(values.size(), [&](std::size_t row) {
        const auto& ids = neighbors[row];
        const std::size_t count = std::min(ids.size(), maximumSamples);
        if (count > generation::kMaxGeneratedPointCount) {
            valid.store(false);
            return;
        }
        for (std::size_t i = 0U; i < count; ++i)
            if (static_cast<std::size_t>(ids[i]) >= values.size()) {
                valid.store(false);
                return;
            }
        for (std::size_t axis = 0U; axis < Components::count; ++axis) {
            SmoothingMean<typename Components::Scalar> sum(
                static_cast<std::uint32_t>(count ? count : 1U));
            if (count == 0U) {
                if (!sum.add(Components::at(values[row], axis))) {
                    valid.store(false);
                    return;
                }
            } else {
                for (std::size_t i = 0U; i < count; ++i)
                    if (!sum.add(Components::at(values[ids[i]], axis))) {
                        valid.store(false);
                        return;
                    }
            }
            if (!sum.result(Components::at(candidate[row], axis))) {
                valid.store(false);
                return;
            }
        }
    });
    if (!valid.load())
        return false;
    output.swap(candidate);
    return true;
}

template <typename T, typename Neighbors>
bool smoothNeighborValues(const std::vector<T>& values, const Neighbors& neighbors,
                          std::size_t maximumSamples, std::vector<T>& output) {
    return smoothNeighborValues(values, neighbors, maximumSamples, output, SerialSmoothingPass());
}

inline bool addSmoothingWork(std::size_t count, std::size_t samples, std::size_t iterations,
                             std::size_t components, std::size_t& total,
                             std::size_t& retainedNeighbors) {
    using namespace generation;
    std::size_t search = 0U, retained = 0U, rows = 0U, passes = 0U, next = 0U;
    if (count > kMaxGeneratedPointCount || samples == 0U || components > 3U ||
        !checkedProduct(count, count, kMaxWorkItemCount, search) ||
        !checkedProduct(count, std::min(samples, count ? count - 1U : 0U), kMaxWorkItemCount,
                        retained) ||
        !checkedSum(retained, count, kMaxWorkItemCount, rows) ||
        !checkedProduct(rows, components, kMaxWorkItemCount, passes) ||
        !checkedProduct(passes, iterations, kMaxWorkItemCount, passes) ||
        !checkedSum(search, passes, kMaxWorkItemCount, next) ||
        !checkedSum(total, next, kMaxWorkItemCount, next))
        return false;
    total = next;
    retainedNeighbors = retained;
    return true;
}

} // namespace attribute
} // namespace dommy
