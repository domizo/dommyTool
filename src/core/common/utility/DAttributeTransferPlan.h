#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

#include <ImathVec.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <vector>

namespace dommy {
namespace transfer {

const int kUnlimitedSampling = -1;
const int kMaxTransferSampling = 1000000;

struct AttributeTransferPlan {
    AttributeTransferPlan()
        : sourcePointCount(0U),
          targetPointCount(0U),
          maximumNeighbors(0U),
          workItemBudget(0U),
          distance(0.0),
          valueMinimum(0.0),
          valueMaximum(0.0),
          scale(0.0) {}

    std::size_t sourcePointCount;
    std::size_t targetPointCount;
    std::size_t maximumNeighbors;
    // Retained neighbors only. RadiusSearchBudget additionally bounds actual
    // traversal and heap work, including candidates that are not retained.
    std::size_t workItemBudget;
    double distance;
    double valueMinimum;
    double valueMaximum;
    double scale;
};

inline bool buildAttributeTransferPlan(std::size_t sourcePointCount,
                                       std::size_t targetPointCount,
                                       int maximumSampling,
                                       double distance,
                                       double valueMinimum,
                                       double valueMaximum,
                                       double scale,
                                       AttributeTransferPlan& plan)
{
    if (sourcePointCount > generation::kMaxGeneratedPointCount ||
        targetPointCount > generation::kMaxGeneratedPointCount ||
        (maximumSampling != kUnlimitedSampling &&
         (maximumSampling <= 0 ||
          maximumSampling > kMaxTransferSampling)) ||
        !std::isfinite(distance) ||
        distance <= std::numeric_limits<double>::epsilon() ||
        !std::isfinite(distance * distance) ||
        !std::isfinite(valueMinimum) ||
        !std::isfinite(valueMaximum) ||
        !std::isfinite(scale)) {
        return false;
    }

    AttributeTransferPlan candidate;
    candidate.sourcePointCount = sourcePointCount;
    candidate.targetPointCount = targetPointCount;
    candidate.maximumNeighbors =
        maximumSampling == kUnlimitedSampling
        ? sourcePointCount
        : std::min(
            sourcePointCount,
            static_cast<std::size_t>(maximumSampling));
    if (!generation::checkedProduct(
            candidate.targetPointCount,
            candidate.maximumNeighbors,
            generation::kMaxWorkItemCount,
            candidate.workItemBudget)) {
        return false;
    }

    candidate.distance = distance;
    candidate.valueMinimum =
        valueMinimum < valueMaximum ? valueMinimum : valueMaximum;
    candidate.valueMaximum =
        valueMinimum < valueMaximum ? valueMaximum : valueMinimum;
    candidate.scale = scale;
    plan = candidate;
    return true;
}

// Bound the complete field application, not only the retained neighbor lists.
// Components count supported scalar channels; names also count unsupported
// fields because each partition visits them. No output is allocated here.
inline bool buildAttributeApplicationBudget(const AttributeTransferPlan& plan,
                                            std::size_t nameCount,
                                            std::size_t componentCount,
                                            std::size_t partitionCount,
                                            std::size_t& workItems)
{
    std::size_t rowsAndNeighbors = 0U, numericWork = 0U, fieldVisits = 0U;
    if (nameCount > generation::kMaxGeneratedStringCount ||
        partitionCount > generation::kMaxConnectedGeometryCount ||
        !generation::checkedSum(plan.workItemBudget, plan.targetPointCount,
            generation::kMaxWorkItemCount, rowsAndNeighbors) ||
        !generation::checkedProduct(rowsAndNeighbors, componentCount,
            generation::kMaxWorkItemCount, numericWork) ||
        !generation::checkedProduct(partitionCount, nameCount,
            generation::kMaxWorkItemCount, fieldVisits) ||
        !generation::checkedSum(numericWork, fieldVisits,
            generation::kMaxWorkItemCount, numericWork))
        return false;
    workItems = numericWork;
    return true;
}

template <typename IdLists, typename DistanceLists>
inline bool validateNeighborLists(const IdLists& ids,
                                  const DistanceLists& distances,
                                  const AttributeTransferPlan& plan)
{
    if (ids.size() != plan.targetPointCount ||
        distances.size() != plan.targetPointCount) {
        return false;
    }

    std::size_t workItems = 0U;
    for (std::size_t target = 0U; target < ids.size(); ++target) {
        if (ids[target].size() != distances[target].size() ||
            ids[target].size() > plan.maximumNeighbors) {
            return false;
        }
        std::size_t updatedWorkItems = 0U;
        if (!generation::checkedSum(
                workItems, ids[target].size(),
                plan.workItemBudget, updatedWorkItems)) {
            return false;
        }
        workItems = updatedWorkItems;

        for (std::size_t neighbor = 0U;
             neighbor < ids[target].size(); ++neighbor) {
            if (static_cast<std::size_t>(ids[target][neighbor]) >=
                    plan.sourcePointCount ||
                !std::isfinite(
                    static_cast<double>(distances[target][neighbor])) ||
                distances[target][neighbor] < 0.0) {
                return false;
            }
        }
    }
    return true;
}

template <typename T>
inline bool checkedComponentCast(long double value, T& result)
{
    return numeric::checkedNumericCast(value, result);
}

template <typename T>
struct TransferValueTraits {
    static const std::size_t kComponentCount = 1U;

    static long double component(const T& value, std::size_t)
    {
        return static_cast<long double>(value);
    }

    static bool create(const long double* values, T& result)
    {
        return checkedComponentCast(values[0], result);
    }
};

template <typename T>
struct TransferValueTraits<Imath::Vec2<T> > {
    static const std::size_t kComponentCount = 2U;

    static long double component(const Imath::Vec2<T>& value,
                                 std::size_t componentIndex)
    {
        return static_cast<long double>(value[componentIndex]);
    }

    static bool create(const long double* values, Imath::Vec2<T>& result)
    {
        T x;
        T y;
        if (!checkedComponentCast(values[0], x) ||
            !checkedComponentCast(values[1], y)) {
            return false;
        }
        result = Imath::Vec2<T>(x, y);
        return true;
    }
};

template <typename T>
struct TransferValueTraits<Imath::Vec3<T> > {
    static const std::size_t kComponentCount = 3U;

    static long double component(const Imath::Vec3<T>& value,
                                 std::size_t componentIndex)
    {
        return static_cast<long double>(value[componentIndex]);
    }

    static bool create(const long double* values, Imath::Vec3<T>& result)
    {
        T x;
        T y;
        T z;
        if (!checkedComponentCast(values[0], x) ||
            !checkedComponentCast(values[1], y) ||
            !checkedComponentCast(values[2], z)) {
            return false;
        }
        result = Imath::Vec3<T>(x, y, z);
        return true;
    }
};

template <typename T, typename IdLists, typename FactorLists>
inline bool transferWeightedValues(const std::vector<T>& sourceValues,
                                   std::size_t expectedSourceCount,
                                   const IdLists& ids,
                                   const FactorLists& factors,
                                   std::vector<T>& outputValues)
{
    if (sourceValues.size() != expectedSourceCount ||
        ids.size() != factors.size()) {
        return false;
    }

    std::vector<T> candidate;
    candidate.reserve(ids.size());
    for (std::size_t target = 0U; target < ids.size(); ++target) {
        if (ids[target].size() != factors[target].size()) return false;

        long double accumulated[3] = {0.0L, 0.0L, 0.0L};
        for (std::size_t neighbor = 0U;
             neighbor < ids[target].size(); ++neighbor) {
            const std::size_t sourceIndex =
                static_cast<std::size_t>(ids[target][neighbor]);
            const long double factor =
                static_cast<long double>(factors[target][neighbor]);
            if (sourceIndex >= sourceValues.size() ||
                !std::isfinite(factor)) {
                return false;
            }
            for (std::size_t component = 0U;
                 component < TransferValueTraits<T>::kComponentCount;
                 ++component) {
                const long double sourceComponent =
                    TransferValueTraits<T>::component(
                        sourceValues[sourceIndex], component);
                if (!std::isfinite(sourceComponent)) return false;
                accumulated[component] += sourceComponent * factor;
                if (!std::isfinite(accumulated[component])) return false;
            }
        }

        if (!ids[target].empty()) {
            const long double divisor =
                static_cast<long double>(ids[target].size());
            for (std::size_t component = 0U;
                 component < TransferValueTraits<T>::kComponentCount;
                 ++component) {
                accumulated[component] /= divisor;
            }
        }

        T value;
        if (!TransferValueTraits<T>::create(accumulated, value)) {
            return false;
        }
        candidate.push_back(value);
    }

    outputValues.swap(candidate);
    return true;
}

} // namespace transfer
} // namespace dommy
