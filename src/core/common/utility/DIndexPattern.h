#pragma once

#include "DGenerationLimits.h"
#include "common/utility/DUnsignedDecimal.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace dommy {
namespace component {

namespace detail {

inline bool parseIndexValue(const std::string& token, std::size_t& value)
{
    return numeric::parseUnsignedDecimal(token, value);
}

} // namespace detail

// indexLimit is exclusive. Ranges retain the archive's end-exclusive syntax:
// "2-5" expands to 2, 3, 4. The output is changed only on success.
inline bool parseIndexPattern(const std::string& pattern,
                              std::size_t indexLimit,
                              std::size_t maximumOutputCount,
                              bool& reverse,
                              std::vector<std::size_t>& indices)
{
    bool candidateReverse = false;
    std::vector<std::size_t> candidate;
    // Some archived nodes used semicolons while newer nodes use whitespace.
    // Normalize both spellings here so every caller shares the same bounded
    // parser and validation rules.
    std::string normalizedPattern(pattern);
    for (std::size_t index = 0U; index < normalizedPattern.size(); ++index) {
        if (normalizedPattern[index] == ';') {
            normalizedPattern[index] = ' ';
        }
    }
    std::istringstream stream(normalizedPattern);
    std::string token;
    while (stream >> token) {
        if (token == "!") {
            candidateReverse = true;
            continue;
        }

        const std::size_t separator = token.find('-');
        if (separator == std::string::npos) {
            std::size_t value = 0U;
            if (!detail::parseIndexValue(token, value)) return false;
            if (value < indexLimit) {
                std::size_t nextSize = 0U;
                if (!generation::checkedSum(
                        candidate.size(), 1U, maximumOutputCount, nextSize)) {
                    return false;
                }
                candidate.push_back(value);
            }
            continue;
        }

        if (separator == 0U || separator + 1U >= token.size() ||
            token.find('-', separator + 1U) != std::string::npos) {
            return false;
        }
        std::size_t first = 0U;
        std::size_t second = 0U;
        if (!detail::parseIndexValue(token.substr(0U, separator), first) ||
            !detail::parseIndexValue(token.substr(separator + 1U), second)) {
            return false;
        }
        if (first > second) {
            const std::size_t temporary = first;
            first = second;
            second = temporary;
        }
        if (first > indexLimit) first = indexLimit;
        if (second > indexLimit) second = indexLimit;

        const std::size_t rangeCount = second - first;
        std::size_t nextSize = 0U;
        if (!generation::checkedSum(
                candidate.size(), rangeCount, maximumOutputCount, nextSize)) {
            return false;
        }
        candidate.reserve(nextSize);
        for (std::size_t value = first; value < second; ++value) {
            candidate.push_back(value);
        }
    }

    // Formatted string extraction can absorb allocation failure into badbit.
    // Only normal EOF may publish the complete pattern and its reverse flag.
    if (!stream.eof() || stream.bad()) return false;

    reverse = candidateReverse;
    indices.swap(candidate);
    return true;
}

// Merges a parsed pattern into an existing selection. A reverse pattern
// selects the complement of its indices. All indices are validated before
// selection is modified, and the reverse path remains O(N + M).
inline bool mergeIndexPatternSelection(
    const std::vector<std::size_t>& indices,
    bool reverse,
    std::vector<bool>& selection)
{
    for (std::size_t index = 0U; index < indices.size(); ++index) {
        if (indices[index] >= selection.size()) return false;
    }

    if (!reverse) {
        for (std::size_t index = 0U; index < indices.size(); ++index) {
            selection[indices[index]] = true;
        }
        return true;
    }

    std::vector<bool> excluded(selection.size(), false);
    for (std::size_t index = 0U; index < indices.size(); ++index) {
        excluded[indices[index]] = true;
    }
    for (std::size_t index = 0U; index < selection.size(); ++index) {
        if (!excluded[index]) selection[index] = true;
    }
    return true;
}

} // namespace component
} // namespace dommy
