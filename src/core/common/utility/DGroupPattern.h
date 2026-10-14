#pragma once

#include "DGenerationLimits.h"

#include <regex>
#include <string>
#include <vector>

namespace dommy {
namespace group {

struct PatternLimits {
    std::size_t patternBytes = 4096U;
    std::size_t nameBytes = 4096U;
    std::size_t groups = generation::kMaxGeneratedStringCount;
    std::size_t inputWork = generation::kMaxWorkItemCount;
};

inline bool validPatternInput(const std::string& pattern,
                               const PatternLimits& limits = PatternLimits())
{
    return pattern.size() <= limits.patternBytes && pattern.find('\0') == std::string::npos;
}

// Preserve ECMAScript full-match syntax, including captures/backreferences.
// Bound the input surface before compiling once. inputWork measures input
// size products, NOT regex-engine steps: std::regex provides no portable match
// timeout. A hard execution bound requires a separate engine/syntax decision.
// Callbacks must stage changes if failure must leave their output untouched.
template <typename Iterator, typename Match>
bool forEachMatchingGroup(Iterator begin, Iterator end, const std::string& pattern,
                           Match match, const PatternLimits& limits = PatternLimits())
try {
    if (!validPatternInput(pattern, limits)) return false;
    std::size_t count = 0U, work = 0U, patternWork = 0U;
    if (!generation::checkedSum(pattern.size(), 1U, limits.inputWork, patternWork)) return false;
    for (auto it = begin; it != end; ++it) {
        const std::string& name = it->first;
        std::size_t nameWork = 0U;
        if (name.size() > limits.nameBytes || name.find('\0') != std::string::npos ||
            !generation::checkedSum(count, 1U, limits.groups, count) ||
            !generation::checkedSum(name.size(), 1U, limits.inputWork, nameWork) ||
            !generation::checkedProduct(patternWork, nameWork, limits.inputWork, nameWork) ||
            !generation::checkedSum(work, nameWork, limits.inputWork, work)) return false;
    }
    const std::regex expression(pattern);
    for (auto it = begin; it != end; ++it)
        if (std::regex_match(it->first, expression) && !match(it)) return false;
    return true;
}
catch (const std::regex_error&) { return false; }

// Keep the legacy append-on-success API, but do not expose a partial match list
// when a later match, allocation, or input check fails.
template <typename Owner>
bool appendMatchedGroupNames(const Owner& owner, const std::string& pattern,
                              std::vector<std::string>& output,
                              const PatternLimits& limits = PatternLimits())
{
    if (output.size() > generation::kMaxGeneratedStringCount) return false;
    std::size_t bytes = 0U;
    for (const auto& name : output)
        if (!generation::checkedSum(bytes, name.size(),
                generation::kMaxGeneratedStringByteCount, bytes)) return false;
    std::vector<std::string> candidate(output);
    if (!forEachMatchingGroup(owner.beginGrp(), owner.endGrp(), pattern,
            [&](decltype(owner.beginGrp()) it) -> bool {
                if (candidate.size() >= generation::kMaxGeneratedStringCount ||
                    !generation::checkedSum(bytes, it->first.size(),
                        generation::kMaxGeneratedStringByteCount, bytes)) return false;
                candidate.push_back(it->first);
                return true;
            }, limits)) return false;
    output.swap(candidate);
    return true;
}

} // namespace group
} // namespace dommy
