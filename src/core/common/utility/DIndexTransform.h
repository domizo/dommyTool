#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace dommy {
namespace container {

inline bool isValidIndexSelection(const std::vector<std::size_t>& indices,
                                  std::size_t sourceSize)
{
    for (std::size_t index = 0U; index < indices.size(); ++index) {
        if (indices[index] >= sourceSize) return false;
    }
    return true;
}

inline bool isValidPermutation(const std::vector<std::size_t>& indices,
                               std::size_t sourceSize)
{
    if (indices.size() != sourceSize) return false;

    std::vector<unsigned char> seen(sourceSize, 0U);
    for (std::size_t index = 0U; index < indices.size(); ++index) {
        const std::size_t sourceIndex = indices[index];
        if (sourceIndex >= sourceSize || seen[sourceIndex] != 0U) {
            return false;
        }
        seen[sourceIndex] = 1U;
    }
    return true;
}

template <typename Value>
bool reorderInPlace(std::vector<Value>& values,
                    const std::vector<std::size_t>& order)
{
    if (!isValidPermutation(order, values.size())) return false;

    std::vector<Value> reordered;
    reordered.reserve(order.size());
    for (std::size_t index = 0U; index < order.size(); ++index) {
        reordered.push_back(values[order[index]]);
    }
    values.swap(reordered);
    return true;
}

template <typename Value>
bool remapInPlace(std::vector<Value>& values,
                  const std::vector<std::size_t>& selection)
{
    if (!isValidIndexSelection(selection, values.size())) return false;

    std::vector<Value> remapped;
    remapped.reserve(selection.size());
    for (std::size_t index = 0U; index < selection.size(); ++index) {
        remapped.push_back(values[selection[index]]);
    }
    values.swap(remapped);
    return true;
}

template <typename Value>
bool insertFill(std::vector<Value>& values,
                std::size_t index,
                std::size_t count,
                const Value& value)
{
    if (index > values.size() ||
        count > values.max_size() - values.size()) {
        return false;
    }
    values.insert(values.begin() + index, count, value);
    return true;
}

inline bool normalizeCyclicOffset(std::int64_t offset,
                                  std::size_t valueCount,
                                  std::size_t& normalizedOffset)
{
    if (valueCount == 0U ||
        valueCount > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return false;
    }

    const std::int64_t signedCount =
        static_cast<std::int64_t>(valueCount);
    std::int64_t remainder = offset % signedCount;
    if (remainder < 0) remainder += signedCount;
    normalizedOffset = static_cast<std::size_t>(remainder);
    return true;
}

} // namespace container
} // namespace dommy
