#pragma once

#include "common/utility/DPrimitiveCopyBudget.h"

namespace dommy {
namespace curve {

// Reuse the count-only geometry staging policy. Connections account for
// knots here (32 bytes each conservatively); points account for CV storage.
// This is a logical work/storage guard, not a Maya/process peak-memory limit.
// Topology must be validated by the caller before inspecting a range.
template <typename Iterator>
bool addCurveOperationCost(Iterator begin, Iterator end, mesh::MeshMergePlan& plan,
                           attribute::ValidationBudget& budget)
{
    for (; begin != end; ++begin) {
        const auto& curve = *begin;
        if (!curve || plan.meshes >= generation::kMaxGeneratedCurveCount ||
            !mesh::appendMeshMergePlan(curve->numberOfPoints(), 0U,
                curve->numberOfKnots(), 0U, plan) ||
            !geometry::addCopyMetadataCost(*curve, curve->numberOfPoints(), budget)) return false;
    }
    return true;
}

// Count-only admission for a single-pass repeated copy. Reuse the component
// accounting used by ordinary curve transactions and the collection-row policy
// used by mesh copies. Defaults are not multiplied at collection level.
template <typename Owner>
bool buildCurveCopyPlan(const Owner& source, std::size_t copies, mesh::MeshMergePlan& output)
{
    using namespace generation;
    if (copies == 0U || copies > kMaxGeneratedTransformCount) return false;
    mesh::MeshMergePlan input, repeated;
    attribute::ValidationBudget components;
    if (!addCurveOperationCost(source.beginCurves(), source.endCurves(), input, components) ||
        !mesh::repeatMeshMergePlan(input, copies, repeated) ||
        repeated.meshes > kMaxGeneratedCurveCount || !mesh::validMeshOperationStorage(repeated)) return false;
    attribute::ValidationBudget budget(repeated.workItems);
    std::size_t work = 0U;
    if (!checkedProduct(components.workItems, copies, kMaxWorkItemCount, work) ||
        !checkedSum(budget.workItems, work, kMaxWorkItemCount, budget.workItems) ||
        !checkedProduct(components.stringBytes, copies, kMaxGeneratedStringByteCount, budget.stringBytes) ||
        !geometry::addRepeatedCollectionMetadataCost(source, source.numberOfCurves(),
            copies, repeated.meshes, budget)) return false;
    output = repeated;
    return true;
}

// One accounting rule for copy, default expansion and indexed expansion.
// Numeric component widths, schemas, dynamic defaults/rows and group masks
// are charged before any payload is cloned or grown. Indices are borrowed
// only for the synchronous transaction and never stored in the result.
template <typename Groups>
inline bool validateCurveOperationBudget(const mesh::MeshMergePlan& plan,
    const DPAttrList& attributes, const Groups& groups,
    std::size_t sourceCount, std::size_t addedCount, const DPIndexList* indices,
    attribute::ValidationBudget& budget)
{
    using namespace generation;
    std::size_t count = 0U, components = 0U;
    if (!checkedSum(sourceCount, addedCount, kMaxGeneratedCurveCount, count) ||
        count != plan.meshes || !mesh::validMeshOperationStorage(plan) ||
        !checkedSum(budget.workItems, plan.workItems, kMaxWorkItemCount, budget.workItems) ||
        !attribute::validateAttributeExpansion(attributes, sourceCount, addedCount, indices, budget)) return false;
    for (const auto& field : attributes) {
        if (!checkedSum(budget.workItems, 1U, kMaxWorkItemCount, budget.workItems) ||
            !checkedProduct(count, point::mergeAttributeComponents(field.second->getAttrType()),
                kMaxWorkItemCount, components) ||
            !checkedSum(budget.workItems, components, kMaxWorkItemCount, budget.workItems)) return false;
    }
    for (const auto& group : groups) {
        if (!group.second || group.second->getGroupIds().size() != sourceCount ||
            !checkedSum(budget.stringBytes, group.first.size(), kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !checkedSum(budget.workItems, 1U, kMaxWorkItemCount, budget.workItems) ||
            !checkedSum(budget.workItems, count, kMaxWorkItemCount, budget.workItems)) return false;
    }
    return true;
}

} // namespace curve
} // namespace dommy
