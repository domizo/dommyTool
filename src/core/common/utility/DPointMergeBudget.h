#pragma once

#include "common/utility/DAttributeValidation.h"
#include "common/utility/DPointPayloadBudget.h"
#include <array>
#include <map>
#include <set>
#include <vector>

namespace dommy {
namespace point {

inline std::size_t mergeAttributeComponents(DP_ATTR_DATA_TYPE type)
{
    switch (type) {
    case DP_ATTR_VEC2I: case DP_ATTR_VEC2F: case DP_ATTR_VEC2D: return 2U;
    case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 3U;
    case DP_ATTR_MATRIX: return 16U;
    default: return 1U;
    }
}

// Dynamic defaults may be repeated for all points missing the field. Numeric
// defaults have no extra payload beyond the component cost above.
inline bool mergeDefaultCost(const DPAttributeData::ConstPtr& value,
                             attribute::ValidationBudget& cost)
{
    if (value->getAttrType() == DP_ATTR_STRING) {
        const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<std::string>>(value);
        return typed && attribute::addDynamicValueBudget(typed->defaultValue(), cost);
    }
    if (value->getAttrType() == DP_ATTR_INT_ARRAY) {
        const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<DIntArray>>(value);
        return typed && attribute::addDynamicValueBudget(typed->defaultValue(), cost);
    }
    return true;
}

struct PointMergePlan {
    std::size_t points = 0U, workItems = 0U, stringBytes = 0U, estimatedBytes = 0U;
};

// Admission for a single unpublished batch, not cumulative-prefix copies.
// First occurrence determines each field's default. A later empty input is
// validated but contributes no schema, matching the historical pairwise merge.
// Repetition inspects one source only; empty copies retain one zero-row schema.
// The existing four-copy payload allowance is an estimate, not a process cap.
template <typename Inputs>
bool buildPointMergePlan(const Inputs& inputs, PointMergePlan& output,
                         std::size_t copies = 1U) try {
    using namespace generation;
    if (inputs.size() > kMaxConnectedGeometryCount || copies == 0U ||
        copies > kMaxGeneratedTransformCount || (copies != 1U && inputs.size() != 1U)) return false;
    struct Field {
        DPAttributeData::ConstPtr source;
        std::size_t present = 0U, rowWork = 0U, rowStrings = 0U;
    };
    std::map<std::string, Field> fields;
    std::set<std::string> groups;
    PointMergePlan plan;
    attribute::ValidationBudget reads;
    std::size_t batches = 0U;
    for (std::size_t index = 0U; index < inputs.size(); ++index) {
        const auto* input = inputs[index];
        if (!input || !checkedSum(reads.workItems, 1U, kMaxWorkItemCount, reads.workItems)) return false;
        const std::size_t count = input->numberOfPoints();
        std::size_t added = 0U;
        if (!checkedProduct(count, copies, kMaxGeneratedPointCount, added) ||
            !checkedSum(plan.points, added, kMaxGeneratedPointCount, plan.points) ||
            !checkedSum(reads.workItems, count, kMaxWorkItemCount, reads.workItems)) return false;
        const bool includeFields = index == 0U || count != 0U;
        if (includeFields && !checkedSum(batches, count == 0U ? 1U : copies,
                                         kMaxConnectedGeometryCount, batches)) return false;
        for (auto field = input->beginAttr(); field != input->endAttr(); ++field) {
            if (!checkedSum(reads.workItems, 1U, kMaxWorkItemCount, reads.workItems) ||
                !checkedSum(reads.stringBytes, field->first.size(), kMaxGeneratedStringByteCount, reads.stringBytes) ||
                !attribute::validAttributeName(field->first) ||
                !attribute::validateAttribute(field->second, count, reads)) return false;
            if (!includeFields) continue;
            auto found = fields.find(field->first);
            if (found == fields.end()) {
                if (fields.size() >= kMaxGeneratedStringCount) return false;
                Field item; item.source = field->second;
                found = fields.emplace(field->first, item).first;
            } else if (found->second.source->typeName() != field->second->typeName()) return false;
            attribute::ValidationBudget rows;
            std::size_t work = 0U, strings = 0U;
            if (!attribute::validateAttribute(field->second, count, rows, false) ||
                !checkedProduct(rows.workItems, copies, kMaxWorkItemCount, work) ||
                !checkedProduct(rows.stringBytes, copies, kMaxGeneratedStringByteCount, strings) ||
                !checkedSum(found->second.present, added, kMaxGeneratedPointCount, found->second.present) ||
                !checkedSum(found->second.rowWork, work, kMaxWorkItemCount, found->second.rowWork) ||
                !checkedSum(found->second.rowStrings, strings, kMaxGeneratedStringByteCount, found->second.rowStrings)) return false;
        }
        for (auto group = input->beginGrp(); group != input->endGrp(); ++group) {
            if (!group->second || group->second->getGroupIds().size() != count ||
                !checkedSum(reads.workItems, count, kMaxWorkItemCount, reads.workItems) ||
                !checkedSum(reads.workItems, 1U, kMaxWorkItemCount, reads.workItems) ||
                !checkedSum(reads.stringBytes, group->first.size(), kMaxGeneratedStringByteCount, reads.stringBytes)) return false;
            if (includeFields) {
                if (groups.size() >= kMaxGeneratedStringCount && groups.count(group->first) == 0U) return false;
                groups.insert(group->first);
            }
        }
    }
    plan.workItems = reads.workItems;
    std::size_t work = 0U, schemaCount = 0U;
    if (!checkedProduct(plan.points, 3U, kMaxWorkItemCount, work) ||
        !checkedSum(plan.workItems, work, kMaxWorkItemCount, plan.workItems) ||
        !checkedSum(fields.size(), groups.size(), kMaxWorkItemCount, schemaCount) ||
        !checkedProduct(batches, schemaCount, kMaxWorkItemCount, work) ||
        !checkedSum(plan.workItems, work, kMaxWorkItemCount, plan.workItems)) return false;
    PointPayloadBudget payload;
    payload.add(plan.points, sizeof(Imath::V3f));
    for (const auto& field : fields) {
        const Field& item = field.second;
        attribute::ValidationBudget defaults;
        const std::size_t missing = plan.points - item.present;
        std::size_t defaultWork = 0U, strings = 0U;
        if (!mergeDefaultCost(item.source, defaults) ||
            !checkedProduct(plan.points, mergeAttributeComponents(item.source->getAttrType()), kMaxWorkItemCount, work) ||
            !checkedSum(plan.workItems, work, kMaxWorkItemCount, plan.workItems) ||
            !checkedSum(plan.workItems, item.rowWork, kMaxWorkItemCount, plan.workItems) ||
            !checkedProduct(missing, defaults.workItems, kMaxWorkItemCount, defaultWork) ||
            !checkedSum(defaultWork, defaults.workItems, kMaxWorkItemCount, defaultWork) ||
            !checkedSum(plan.workItems, defaultWork, kMaxWorkItemCount, plan.workItems) ||
            !checkedProduct(missing, defaults.stringBytes, kMaxGeneratedStringByteCount, strings) ||
            !checkedSum(strings, defaults.stringBytes, kMaxGeneratedStringByteCount, strings) ||
            !checkedSum(strings, item.rowStrings, kMaxGeneratedStringByteCount, strings) ||
            !checkedSum(strings, field.first.size(), kMaxGeneratedStringByteCount, strings) ||
            !checkedSum(plan.stringBytes, strings, kMaxGeneratedStringByteCount, plan.stringBytes)) return false;
        const std::size_t width = pointAttributeStorageBytes(item.source->getAttrType());
        if (width == 0U) return false;
        payload.add(plan.points, width); payload.add(1U, width);
        payload.add(strings, sizeof(char));
        // validateAttribute charges one work item per row; the remainder is
        // actual integer-array payload, not numeric components charged above.
        payload.add(item.rowWork - item.present, sizeof(int));
        payload.add(defaultWork, sizeof(int));
    }
    for (const auto& group : groups) {
        if (!checkedSum(plan.workItems, plan.points, kMaxWorkItemCount, plan.workItems) ||
            !checkedSum(plan.stringBytes, group.size(), kMaxGeneratedStringByteCount, plan.stringBytes)) return false;
        payload.add(plan.points, sizeof(bool)); payload.add(group.size(), sizeof(char));
    }
    plan.estimatedBytes = payload.estimatedBytes();
    output = plan;
    return true;
} catch (...) { return false; }

template <typename Inputs>
bool buildPointMergeBudget(const Inputs& inputs, std::size_t& workItems) {
    PointMergePlan plan;
    if (!buildPointMergePlan(inputs, plan)) return false;
    workItems = plan.workItems;
    return true;
}

template <typename Owner>
bool buildPointCopyPlan(const Owner& input, std::size_t copies, PointMergePlan& output) {
    const std::array<const Owner*, 1U> source = {{&input}};
    return buildPointMergePlan(source, output, copies);
}

template <typename Owner>
bool buildPointCopyBudget(const Owner& input, std::size_t count, std::size_t& workItems) {
    // DPDuplicator's empty target historically constructs no source batch.
    if (count == 0U) { workItems = 0U; return true; }
    PointMergePlan plan;
    if (!buildPointCopyPlan(input, count, plan)) return false;
    workItems = plan.workItems;
    return true;
}

} // namespace point
} // namespace dommy
