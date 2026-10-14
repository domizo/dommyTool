#pragma once

#include "DOpenVDBLimits.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace vdb {

const std::size_t kNoCombineGrid = std::numeric_limits<std::size_t>::max();

enum CombinePlanStatus {
    kCombinePlanSuccess = 0,
    kCombinePlanInvalidOperation,
    kCombinePlanMissingA,
    kCombinePlanMissingB,
    kCombinePlanGridLimitExceeded
};

struct CombineStep {
    CombineStep()
        : aIndex(kNoCombineGrid),
          bIndex(kNoCombineGrid),
          usePreviousA(false),
          emitOutput(false)
    {
    }

    std::size_t aIndex;
    std::size_t bIndex;
    bool usePreviousA;
    bool emitOutput;
};

struct CombinePlan {
    CombinePlan() : unusedA(false), unusedB(false) {}

    std::vector<CombineStep> steps;
    bool unusedA;
    bool unusedB;
};

/// Build the historical OpenVDB Combine collation order without depending on
/// Maya or OpenVDB. Pair mode combines A/B by index, non-pair mode reuses the
/// first B for every A, and flatten mode folds every B into the first A.
inline CombinePlanStatus buildCombinePlan(
    std::size_t aCount,
    std::size_t bCount,
    bool needA,
    bool needB,
    bool flatten,
    bool pairs,
    CombinePlan& plan)
{
    if (!needA && !needB) return kCombinePlanInvalidOperation;
    if (aCount > kMaxVdbGridCount || bCount > kMaxVdbGridCount) {
        return kCombinePlanGridLimitExceeded;
    }
    if (needA && aCount == 0U) return kCombinePlanMissingA;
    if (needB && bCount == 0U) return kCombinePlanMissingB;

    CombinePlan candidate;

    if (flatten) {
        const std::size_t stepCount = needB ? bCount : 1U;
        candidate.steps.reserve(stepCount);
        for (std::size_t index = 0U; index < stepCount; ++index) {
            CombineStep step;
            if (needA && index == 0U) step.aIndex = 0U;
            if (needA && index > 0U) step.usePreviousA = true;
            if (needB) step.bIndex = index;
            step.emitOutput = (index + 1U == stepCount);
            candidate.steps.push_back(step);
        }
        candidate.unusedA = needA && aCount > 1U;
    } else if (pairs) {
        std::size_t stepCount = 0U;
        if (needA && needB) {
            stepCount = (aCount < bCount) ? aCount : bCount;
            candidate.unusedA = aCount > stepCount;
            candidate.unusedB = bCount > stepCount;
        } else {
            stepCount = needA ? aCount : bCount;
        }

        candidate.steps.reserve(stepCount);
        for (std::size_t index = 0U; index < stepCount; ++index) {
            CombineStep step;
            if (needA) step.aIndex = index;
            if (needB) step.bIndex = index;
            step.emitOutput = true;
            candidate.steps.push_back(step);
        }
    } else {
        // "Each A With First B". Unary B operations have no A sequence to
        // drive, so they evaluate the first selected B exactly once.
        const std::size_t stepCount = needA ? aCount : 1U;
        candidate.steps.reserve(stepCount);
        for (std::size_t index = 0U; index < stepCount; ++index) {
            CombineStep step;
            if (needA) step.aIndex = index;
            if (needB) step.bIndex = 0U;
            step.emitOutput = true;
            candidate.steps.push_back(step);
        }
        candidate.unusedB = needB && bCount > 1U;
    }

    plan.steps.swap(candidate.steps);
    plan.unusedA = candidate.unusedA;
    plan.unusedB = candidate.unusedB;
    return kCombinePlanSuccess;
}

inline const char* combinePlanStatusMessage(CombinePlanStatus status)
{
    switch (status) {
        case kCombinePlanSuccess:
            return "";
        case kCombinePlanInvalidOperation:
            return "OpenVDB Combine has no required input";
        case kCombinePlanMissingA:
            return "OpenVDB Combine requires at least one selected A grid";
        case kCombinePlanMissingB:
            return "OpenVDB Combine requires at least one selected B grid";
        case kCombinePlanGridLimitExceeded:
            return "OpenVDB Combine selection exceeds the grid safety limit";
    }
    return "OpenVDB Combine plan is invalid";
}

} // namespace vdb
} // namespace dommy
