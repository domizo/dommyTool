#pragma once

#include "common/utility/DCurveGenerationStorage.h"

#include <cmath>
#include <cstddef>
#include <algorithm>

namespace dommy {
namespace curve {

const std::size_t kMaxTextInputBytes = 1000000U;
const std::size_t kMaxFontPathBytes = 32768U;
const std::size_t kMaxTextGroupNameBytes = 4096U;
const int kMaxTextSampling = 4096;
const double kMaxTextSize = 10000.0;

struct TextGenerationPlan {
    TextGenerationPlan()
        : sampling(0U),
          fontSize(0.0),
          trackingX(0.0),
          trackingY(0.0) {}

    std::size_t sampling;
    double fontSize;
    double trackingX;
    double trackingY;
};

inline bool buildTextGenerationPlan(std::size_t textBytes,
                                    std::size_t fontPathBytes,
                                    std::size_t groupNameBytes,
                                    double fontSize,
                                    int sampling,
                                    double trackingX,
                                    double trackingY,
                                    TextGenerationPlan& plan)
{
    if (textBytes > kMaxTextInputBytes ||
        fontPathBytes > kMaxFontPathBytes ||
        groupNameBytes > kMaxTextGroupNameBytes ||
        !std::isfinite(fontSize) ||
        fontSize <= 0.0 || fontSize > kMaxTextSize ||
        sampling <= 0 || sampling > kMaxTextSampling ||
        !std::isfinite(trackingX) ||
        !std::isfinite(trackingY)) {
        return false;
    }

    TextGenerationPlan candidate;
    candidate.sampling = static_cast<std::size_t>(sampling);
    candidate.fontSize = fontSize;
    candidate.trackingX = trackingX;
    candidate.trackingY = trackingY;
    plan = candidate;
    return true;
}

inline bool reserveTextOutput(std::size_t currentCurves,
                              std::size_t currentPoints,
                              std::size_t additionalCurves,
                              std::size_t additionalPoints,
                              std::size_t& nextCurves,
                              std::size_t& nextPoints)
{
    return generation::checkedSum(
               currentCurves, additionalCurves,
               generation::kMaxGeneratedCurveCount, nextCurves) &&
           generation::checkedSum(
               currentPoints, additionalPoints,
               generation::kMaxGeneratedPointCount, nextPoints);
}

struct TextOutputBudget {
    std::size_t curves = 0U, points = 0U, largestCurve = 0U, groups = 0U;
    std::size_t baseBytes = 0U, groupNameBytes = 0U, estimatedBytes = 0U;
    bool createGroups = false;
};

inline bool initializeTextOutputBudget(std::size_t characters, std::size_t textBytes,
                                       std::size_t groupNameBytes, bool createGroups,
                                       TextOutputBudget& result)
{
    TextOutputBudget candidate;
    if (characters > generation::kMaxGeneratedStringCount || textBytes > kMaxTextInputBytes ||
        groupNameBytes > kMaxTextGroupNameBytes ||
        !generation::addCurveGenerationBytes(textBytes, 4U, candidate.baseBytes) ||
        !generation::addCurveGenerationBytes(characters, createGroups ? 96U : 32U, candidate.baseBytes))
        return false;
    candidate.groupNameBytes = groupNameBytes;
    candidate.createGroups = createGroups;
    candidate.estimatedBytes = candidate.baseBytes;
    result = candidate;
    return true;
}

inline bool textOutputStorage(const TextOutputBudget& budget, std::size_t curves,
                              std::size_t points, std::size_t largestCurve,
                              std::size_t groups, std::size_t& result)
{
    std::size_t bytes = 0U, masks = 0U, work = 0U;
    if (budget.groupNameBytes > kMaxTextGroupNameBytes ||
        curves > generation::kMaxGeneratedCurveCount || points > generation::kMaxGeneratedPointCount ||
        groups > generation::kMaxGeneratedStringCount ||
        !generation::curveGenerationStorageBytes(curves, points, largestCurve, 0U, bytes) ||
        !generation::addCurveGenerationBytes(1U, budget.baseBytes, bytes) ||
        !generation::checkedProduct(groups, curves, generation::kMaxWorkItemCount, masks) ||
        !generation::checkedSum(masks, curves, generation::kMaxWorkItemCount, work) ||
        !generation::addCurveGenerationBytes(masks, 4U, bytes) ||
        !generation::addCurveGenerationBytes(groups, 512U + 4U * (budget.groupNameBytes + 10U), bytes) ||
        !generation::addCurveGenerationBytes(budget.createGroups ? curves : 0U, 32U, bytes)) return false;
    // Group masks/defaults/copies, generated name suffixes and membership lists.
    result = bytes;
    return true;
}

inline bool reserveTextGroup(TextOutputBudget& budget)
{
    if (!budget.createGroups) return true;
    std::size_t groups = 0U, bytes = 0U;
    if (!generation::checkedSum(budget.groups, 1U, generation::kMaxGeneratedStringCount, groups) ||
        !textOutputStorage(budget, budget.curves, budget.points, budget.largestCurve, groups, bytes)) return false;
    budget.groups = groups;
    budget.estimatedBytes = bytes;
    return true;
}

inline bool admitTextCurvePoints(const TextOutputBudget& budget, std::size_t localPoints,
                                 std::size_t& bytes)
{
    std::size_t curves = 0U, points = 0U;
    return reserveTextOutput(budget.curves, budget.points, 1U, localPoints, curves, points) &&
        textOutputStorage(budget, curves, points, std::max(budget.largestCurve, localPoints), budget.groups, bytes);
}

inline bool commitTextCurve(TextOutputBudget& budget, std::size_t localPoints)
{
    std::size_t bytes = 0U;
    if (localPoints < 2U || !admitTextCurvePoints(budget, localPoints, bytes)) return false;
    ++budget.curves;
    budget.points += localPoints;
    budget.largestCurve = std::max(budget.largestCurve, localPoints);
    budget.estimatedBytes = bytes;
    return true;
}

} // namespace curve
} // namespace dommy
