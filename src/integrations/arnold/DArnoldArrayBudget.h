#pragma once

#include "common/utility/DCheckedSize.h"
#include "common/utility/DWorkBudget.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace dommy {
namespace arnold {

const std::size_t kMaxArnoldArrayValues = 100000000U;
const std::size_t kMaxArnoldGeneratedNodes =
    dommy::generation::kMaxGeneratedTransformCount;

struct NodeGenerationLayout {
    std::size_t instanceCount = 0U;
    std::size_t nodeCount = 0U;
};

// Bound fanout and repeated field/replacement work, not native renderer bytes.
inline bool checkedNodeGenerationLayout(
    std::size_t pointCount, std::size_t referenceCount,
    std::size_t attributeCount, std::size_t replacementTargetCount,
    std::size_t auxiliaryNodeCount, NodeGenerationLayout& result)
{
    result = NodeGenerationLayout();
    if (pointCount != 0U && referenceCount == 0U) return false;
    NodeGenerationLayout candidate;
    if (!dommy::generation::checkedProduct(
            pointCount, referenceCount, kMaxArnoldGeneratedNodes,
            candidate.instanceCount) ||
        !dommy::generation::checkedSum(
            candidate.instanceCount, auxiliaryNodeCount,
            kMaxArnoldGeneratedNodes, candidate.nodeCount)) return false;
    dommy::generation::WorkBudget work;
    if (!work.consume(candidate.nodeCount) ||
        !work.consumeProduct(pointCount, attributeCount) ||
        !work.consumeProduct(candidate.instanceCount, attributeCount) ||
        !work.consumeProduct(candidate.instanceCount, replacementTargetCount))
        return false;
    result = candidate;
    return true;
}

struct ArrayLayout {
    ArrayLayout() : elementCount(0U), keyCount(0U), valueCount(0U) {}

    std::uint32_t elementCount;
    std::uint8_t keyCount;
    std::size_t valueCount;
};

inline bool checkedArrayLayout(
    std::size_t elementCount, std::size_t keyCount, ArrayLayout& result)
{
    result = ArrayLayout();
    if (elementCount > static_cast<std::size_t>(
                           std::numeric_limits<std::uint32_t>::max()) ||
        keyCount == 0U ||
        keyCount > static_cast<std::size_t>(
                       std::numeric_limits<std::uint8_t>::max())) {
        return false;
    }

    std::size_t valueCount = 0U;
    if (!dommy::memory::checkedMultiply(
            elementCount, keyCount, valueCount) ||
        valueCount > kMaxArnoldArrayValues) {
        return false;
    }

    result.elementCount = static_cast<std::uint32_t>(elementCount);
    result.keyCount = static_cast<std::uint8_t>(keyCount);
    result.valueCount = valueCount;
    return true;
}

inline bool checkedAdjustedAccumulate(
    std::size_t baseCount, int adjustment, std::size_t& total)
{
    std::size_t adjustedCount = baseCount;
    if (adjustment < 0) {
        const std::size_t magnitude = static_cast<std::size_t>(
            -(static_cast<long long>(adjustment)));
        if (adjustedCount < magnitude) return false;
        adjustedCount -= magnitude;
    } else {
        if (!dommy::memory::checkedAdd(
                adjustedCount, static_cast<std::size_t>(adjustment),
                adjustedCount)) {
            return false;
        }
    }
    return dommy::memory::checkedAdd(total, adjustedCount, total);
}

} // namespace arnold
} // namespace dommy
