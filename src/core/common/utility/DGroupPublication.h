#pragma once

#include "DDataUtility/Data/DPGroupData.h"
#include "DAttributeLookup.h"
#include "DGenerationLimits.h"
#include <utility>

namespace dommy {
namespace group {

// Serialized values match DGroupExistActionType. Subtract intentionally keeps
// DPGroup's historical direction: new selection minus existing selection.
inline bool combineGroupSelection(const DPIdList* previous, const DPIdList& generated, int action,
                                  bool invert, DPIdList& output) {
    if (action < 0 || action > 3 || generated.size() > generation::kMaxGeneratedPointCount ||
        (previous && previous->size() != generated.size()))
        return false;
    DPIdList candidate(generated);
    if (invert)
        candidate.flip();
    if (previous) {
        for (std::size_t i = 0U; i < candidate.size(); ++i) {
            if (action == 1)
                candidate[i] = candidate[i] || (*previous)[i];
            else if (action == 2)
                candidate[i] = candidate[i] && !(*previous)[i];
            else if (action == 3)
                candidate[i] = candidate[i] && (*previous)[i];
        }
    }
    output.swap(candidate);
    return true;
}

// On a staged point/primitive owner, replace one group without deleting it or
// modifying a group object that may still be shared by upstream data.
template <typename Owner>
bool publishCombinedGroup(Owner& owner, const std::string& name, std::size_t count,
                          const DPIdList& generated, int action, bool invert) {
    if (!attribute::validAttributeName(name) || generated.size() != count ||
        name.size() > generation::kMaxGeneratedStringByteCount)
        return false;
    const auto previous = owner.getGrp(name);
    DPIdList combined;
    if (!combineGroupSelection(previous ? &previous->getGroupIds() : nullptr, generated, action,
                               invert, combined))
        return false;
    DPGroupData::Ptr candidate = std::make_shared<DPGroupData>(combined);
    for (auto it = owner.beginGrp(); it != owner.endGrp(); ++it) {
        if (it->first == name) {
            it->second.swap(candidate);
            return true;
        }
    }
    owner.addGrp(name, *candidate);
    const auto written = owner.getGrp(name);
    return written && written->getGroupIds() == combined;
}

// Replace only the group namespace. Keep addGrp's insert-only legacy contract.
// Allocate first; neither an existing group nor a shared input mask is mutated.
inline bool replaceGroupValues(DPGrpList& groups, const std::string& name, const DPIdList& values,
                               std::size_t expectedCount) {
    if (!attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount || values.size() != expectedCount ||
        expectedCount > generation::kMaxGeneratedPointCount)
        return false;
    try {
        DPGroupData::Ptr candidate = std::make_shared<DPGroupData>(values);
        const auto found = groups.find(name);
        if (found == groups.end())
            groups.emplace(name, std::move(candidate));
        else
            found->second.swap(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool publishCombinedComponentGroup(DPGrpList* groups, const std::string& name,
                                          std::size_t count, const DPIdList& generated, int action,
                                          bool invert) {
    if (!groups || generated.size() != count)
        return false;
    const auto previous = attribute::findAttribute(*groups, name);
    DPIdList combined;
    return combineGroupSelection(previous ? &previous->getGroupIds() : nullptr, generated, action,
                                 invert, combined) &&
           replaceGroupValues(*groups, name, combined, count);
}

// Call on an unpublished output copy. Preflight every partition before any
// writes. On allocation failure the caller must discard that staged output.
template <typename Iterator>
bool replacePointGroupPartitions(Iterator first, Iterator last, const std::string& name,
                                 const std::vector<DPIdList>& masks) {
    if (!attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount ||
        masks.size() > generation::kMaxConnectedGeometryCount)
        return false;
    std::size_t index = 0U, total = 0U;
    for (auto it = first; it != last; ++it, ++index) {
        if (!*it || index >= masks.size() || masks[index].size() != (*it)->numberOfPoints() ||
            masks[index].size() > generation::kMaxGeneratedPointCount ||
            !generation::checkedSum(total, masks[index].size(), generation::kMaxWorkItemCount,
                                    total))
            return false;
    }
    if (index != masks.size())
        return false;
    index = 0U;
    for (auto it = first; it != last; ++it, ++index)
        if (!(*it)->replaceGrp(name, masks[index]))
            return false;
    return true;
}

} // namespace group
} // namespace dommy
