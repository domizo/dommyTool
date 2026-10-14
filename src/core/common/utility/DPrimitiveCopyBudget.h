#pragma once

#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DMeshMergePlan.h"

namespace dommy {
namespace geometry {

template <typename Iterator>
bool addCopyAttributeCost(Iterator begin, Iterator end, std::size_t count,
                          attribute::ValidationBudget& cost) {
    using namespace generation;
    for (auto field = begin; field != end; ++field) {
        std::size_t components = 0U;
        if (!checkedSum(cost.workItems, 1U, kMaxWorkItemCount, cost.workItems) ||
            !checkedSum(cost.stringBytes, field->first.size(), kMaxGeneratedStringByteCount, cost.stringBytes) ||
            !attribute::validAttributeName(field->first) ||
            !attribute::validateAttribute(field->second, count, cost) ||
            !checkedProduct(count, point::mergeAttributeComponents(field->second->getAttrType()),
                kMaxWorkItemCount, components) ||
            !checkedSum(cost.workItems, components, kMaxWorkItemCount, cost.workItems)) return false;
    }
    return true;
}

template <typename Iterator>
bool addCopyGroupCost(Iterator begin, Iterator end, attribute::ValidationBudget& cost) {
    using namespace generation;
    // Topology validation checks each mask's component count. Charge its actual
    // storage here, including edge masks, without rebuilding a mesh edge map.
    for (auto group = begin; group != end; ++group) {
        if (!group->second ||
            !checkedSum(cost.workItems, 1U, kMaxWorkItemCount, cost.workItems) ||
            !checkedSum(cost.workItems, group->second->getGroupIds().size(), kMaxWorkItemCount, cost.workItems) ||
            !checkedSum(cost.stringBytes, group->first.size(), kMaxGeneratedStringByteCount, cost.stringBytes))
            return false;
    }
    return true;
}

template <typename Owner>
bool addCopyMetadataCost(const Owner& owner, std::size_t count,
                         attribute::ValidationBudget& cost) {
    return addCopyAttributeCost(owner.beginAttr(), owner.endAttr(), count, cost) &&
           addCopyGroupCost(owner.beginGrp(), owner.endGrp(), cost);
}

// Collection schemas/defaults occur once; only rows and group masks repeat.
// Component metadata is charged separately by the mesh/curve planner.
template <typename Owner>
bool addRepeatedCollectionMetadataCost(const Owner& source, std::size_t sourceCount,
    std::size_t copies, std::size_t outputCount, attribute::ValidationBudget& budget) {
    using namespace generation;
    if (copies == 0U || copies > kMaxGeneratedTransformCount) return false;
    std::size_t repeated = 0U;
    if (!checkedProduct(sourceCount, copies, kMaxWorkItemCount, repeated) || repeated != outputCount) return false;
    for (auto field = source.beginAttr(); field != source.endAttr(); ++field) {
        attribute::ValidationBudget rows;
        if (!attribute::validAttributeName(field->first) ||
            !checkedSum(budget.workItems, 1U, kMaxWorkItemCount, budget.workItems) ||
            !checkedSum(budget.stringBytes, field->first.size(), kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !attribute::validateAttribute(field->second, sourceCount, budget) ||
            !attribute::validateAttribute(field->second, sourceCount, rows, false) ||
            !checkedProduct(rows.workItems, copies - 1U, kMaxWorkItemCount, repeated) ||
            !checkedSum(budget.workItems, repeated, kMaxWorkItemCount, budget.workItems) ||
            !checkedProduct(rows.stringBytes, copies - 1U, kMaxGeneratedStringByteCount, repeated) ||
            !checkedSum(budget.stringBytes, repeated, kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !checkedProduct(outputCount, point::mergeAttributeComponents(field->second->getAttrType()),
                kMaxWorkItemCount, repeated) ||
            !checkedSum(budget.workItems, repeated, kMaxWorkItemCount, budget.workItems)) return false;
    }
    for (auto group = source.beginGrp(); group != source.endGrp(); ++group) {
        if (!group->second || group->second->getGroupIds().size() != sourceCount ||
            !checkedSum(budget.stringBytes, group->first.size(), kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !checkedSum(budget.workItems, 1U, kMaxWorkItemCount, budget.workItems) ||
            !checkedSum(budget.workItems, outputCount, kMaxWorkItemCount, budget.workItems)) return false;
    }
    return true;
}

} // namespace geometry
} // namespace dommy
