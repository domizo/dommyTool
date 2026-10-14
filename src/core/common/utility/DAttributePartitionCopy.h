#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributePublication.h"

namespace dommy {
namespace attribute {

// The owners are an unpublished output copy. Build every partition before
// committing this field; a later commit failure requires discarding the output.
template <typename T, typename Iterator>
bool copyTypedAttributePartitions(const DPAttributeData::ConstPtr& untyped,
                                  const std::string& name, Iterator first, Iterator last,
                                  std::size_t totalCount, ValidationBudget& budget)
{
    const auto source = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(untyped);
    ValidationBudget candidateBudget = budget;
    if (!source || !validAttributeName(name) ||
        !validArrayConversionBudget(totalCount, sizeof(T)) ||
        !generation::checkedSum(candidateBudget.workItems, totalCount,
            generation::kMaxWorkItemCount, candidateBudget.workItems) ||
        !validateTypedAttribute<T>(source, totalCount, candidateBudget))
        return false;
    const T& defaultValue = source->defaultValue();
    std::size_t count = 0U, partitions = 0U;
    for (auto owner = first; owner != last; ++owner) {
        if (!*owner ||
            !generation::checkedSum(partitions, 1U,
                generation::kMaxConnectedGeometryCount, partitions) ||
            !generation::checkedSum(count, (*owner)->numberOfPoints(), totalCount, count) ||
            !generation::checkedSum(candidateBudget.stringBytes, name.size(),
                generation::kMaxGeneratedStringByteCount, candidateBudget.stringBytes) ||
            (partitions > 1U && !addDynamicValueBudget(defaultValue, candidateBudget)))
            return false;
    }
    if (count != totalCount) return false;

    std::vector<DPAttributeData::Ptr> candidates;
    candidates.reserve(partitions);
    const std::vector<T>& values = source->value();
    std::size_t offset = 0U;
    for (auto owner = first; owner != last; ++owner) {
        const std::size_t next = offset + (*owner)->numberOfPoints();
        const std::vector<T> partition(values.begin() + offset, values.begin() + next);
        candidates.push_back(
            std::make_shared<TypedDPAttributeData<T>>(partition, defaultValue));
        offset = next;
    }
    std::size_t index = 0U;
    for (auto owner = first; owner != last; ++owner, ++index)
        if (!commitStagedAttribute(**owner, name, candidates[index], (*owner)->numberOfPoints()))
            return false;
    budget = candidateBudget;
    return true;
}

template <typename Iterator>
bool copyAttributePartitions(const DPAttributeData::ConstPtr& source,
                             const std::string& name, Iterator first, Iterator last,
                             std::size_t totalCount, ValidationBudget& budget)
{
    if (!source) return false;
    switch (source->getAttrType()) {
    case DP_ATTR_BOOL: return copyTypedAttributePartitions<bool>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_INT: return copyTypedAttributePartitions<int32_t>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_UINT32: return copyTypedAttributePartitions<uint32_t>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_INT64: return copyTypedAttributePartitions<int64_t>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_FLOAT: return copyTypedAttributePartitions<float>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_DOUBLE: return copyTypedAttributePartitions<double>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC2I: return copyTypedAttributePartitions<Imath::V2i>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC2F: return copyTypedAttributePartitions<Imath::V2f>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC2D: return copyTypedAttributePartitions<Imath::V2d>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC3I: return copyTypedAttributePartitions<Imath::V3i>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC3F: return copyTypedAttributePartitions<Imath::V3f>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_VEC3D: return copyTypedAttributePartitions<Imath::V3d>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_STRING: return copyTypedAttributePartitions<std::string>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_MATRIX: return copyTypedAttributePartitions<MMatrix>(
        source, name, first, last, totalCount, budget);
    case DP_ATTR_INT_ARRAY: return copyTypedAttributePartitions<DIntArray>(
        source, name, first, last, totalCount, budget);
    default: return false;
    }
}

} // namespace attribute
} // namespace dommy
