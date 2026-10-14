#pragma once

#include "DAttributeValidation.h"
#include <limits>
#include <string>
#include <vector>

namespace dommy {
namespace group {

// Count final metadata, including retained groups and variable-sized attribute
// payloads. Replacing a group must not be charged as an additional group.
template <typename Owner>
bool validGeneratedGroupBudget(const Owner& input, std::size_t primitiveCount,
                               const std::vector<std::string>& generatedNames)
{
    dommy::attribute::ValidationBudget budget(primitiveCount);
    for (auto attribute = input.beginAttr(); attribute != input.endAttr(); ++attribute) {
        if (!dommy::attribute::validAttributeName(attribute->first) ||
            !dommy::generation::checkedSum(budget.stringBytes, attribute->first.size(),
                dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !dommy::attribute::validateAttribute(attribute->second, primitiveCount, budget))
            return false;
    }

    std::size_t groupCount = 0U;
    for (auto group = input.beginGrp(); group != input.endGrp(); ++group) {
        if (!group->second || group->second->getGroupIds().size() != primitiveCount ||
            !dommy::attribute::validAttributeName(group->first) ||
            !dommy::generation::checkedSum(groupCount, 1U,
                dommy::generation::kMaxGeneratedStringCount, groupCount) ||
            !dommy::generation::checkedSum(budget.stringBytes, group->first.size(),
                dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes))
            return false;
    }
    for (const std::string& name : generatedNames) {
        if (!input.isExistGrp(name) &&
            (!dommy::generation::checkedSum(groupCount, 1U,
                dommy::generation::kMaxGeneratedStringCount, groupCount) ||
             !dommy::generation::checkedSum(budget.stringBytes, name.size(),
                dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes)))
            return false;
    }
    std::size_t maskItems = 0U;
    return dommy::generation::checkedProduct(primitiveCount, groupCount,
               dommy::generation::kMaxWorkItemCount, maskItems) &&
           dommy::generation::checkedSum(budget.workItems, maskItems,
               dommy::generation::kMaxWorkItemCount, budget.workItems);
}

// nameAt must produce the distinct legacy prefix + sequential integer names.
// Preflight the repeated prefix before constructing any generated name.
template <typename Owner, typename NameFactory>
bool buildGeneratedGroupNames(const Owner& input, std::size_t primitiveCount,
                              std::size_t generatedCount, std::size_t prefixBytes,
                              NameFactory nameAt, std::vector<std::string>& names)
{
    std::size_t generatedItems = 0U, nameBytes = 0U;
    if (generatedCount > generation::kMaxGeneratedStringCount ||
        generatedCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !generation::checkedProduct(primitiveCount, generatedCount,
            generation::kMaxWorkItemCount, generatedItems) ||
        !generation::checkedProduct(prefixBytes, generatedCount,
            generation::kMaxGeneratedStringByteCount, nameBytes))
        return false;
    std::vector<std::string> candidate;
    candidate.reserve(generatedCount);
    nameBytes = 0U;
    for (std::size_t i = 0U; i < generatedCount; ++i) {
        const std::string name = nameAt(static_cast<int>(i));
        if (!attribute::validAttributeName(name) ||
            !generation::checkedSum(nameBytes, name.size(),
                generation::kMaxGeneratedStringByteCount, nameBytes))
            return false;
        candidate.push_back(name);
    }
    if (!validGeneratedGroupBudget(input, primitiveCount, candidate)) return false;
    names.swap(candidate);
    return true;
}

} // namespace group
} // namespace dommy

