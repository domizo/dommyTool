#pragma once

#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DMayaAttributeTransfer.h"
#include "DPAttributeCommon.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace dommy {
namespace transfer {

enum AttributeApplyResult {
    kAttributeApplySuccess = 0,
    kAttributeApplyUnsupported,
    kAttributeApplyFailure
};

inline std::size_t attributeTransferComponents(DP_ATTR_DATA_TYPE type)
{
    switch (type) {
    case DP_ATTR_INT: case DP_ATTR_UINT32: case DP_ATTR_INT64:
    case DP_ATTR_FLOAT: case DP_ATTR_DOUBLE: return 1U;
    case DP_ATTR_VEC2I: case DP_ATTR_VEC2F: case DP_ATTR_VEC2D: return 2U;
    case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 3U;
    default: return 0U;
    }
}

// Preserve first-request order and the legacy missing/unsupported-name policy.
// The caller uses readCheckedNameList before this selection step.
template <typename NameArray>
inline bool selectTransferAttributes(const DPData& source, const NameArray& requested,
                                     std::vector<std::string>& names,
                                     std::size_t& componentCount)
{
    std::set<std::string> seen;
    std::vector<std::string> candidate;
    std::size_t components = 0U;
    for (unsigned int i = 0U; i < requested.length(); ++i) {
        const std::string name = requested[i].asChar();
        const DPAttributeData::ConstPtr attribute = source.getAttr(name);
        if (!attribute || !seen.insert(name).second) continue;
        if (!generation::checkedSum(components, attributeTransferComponents(attribute->getAttrType()),
                generation::kMaxWorkItemCount, components)) return false;
        candidate.push_back(name);
    }
    names.swap(candidate);
    componentCount = components;
    return true;
}

template <typename Value, typename Target, typename IdLists>
inline bool applyTypedPointAttribute(
    const DPData& source,
    Target& target,
    const std::string& name,
    const IdLists& neighborIds,
    const maya::AttributeTransferFactors& factors)
{
    const auto attribute =
        std::dynamic_pointer_cast<const TypedDPAttributeData<Value>>(source.getAttr(name));
    if (!attribute || attribute->value().size() != source.numberOfPoints()) return false;
    const std::vector<Value>& sourceValues = attribute->value();
    std::vector<Value> outputValues;
    if (!transferWeightedValues(
            sourceValues, source.numberOfPoints(),
            neighborIds, factors, outputValues) ||
        outputValues.size() != target.numberOfPoints()) {
        return false;
    }

    if (target.isExistAttr(name)) target.removeAttr(name);
    target.addAttr(
        name, TypedDPAttributeData<Value>(outputValues));
    return true;
}

template <typename Target, typename IdLists>
inline AttributeApplyResult applyPointAttribute(
    const DPData& source,
    Target& target,
    const std::string& name,
    const IdLists& neighborIds,
    const maya::AttributeTransferFactors& factors)
{
    const DPAttributeData::ConstPtr attribute = source.getAttr(name);
    if (!attribute) return kAttributeApplyFailure;

    bool success = false;
    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            success = applyTypedPointAttribute<int32_t>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_UINT32:
            success = applyTypedPointAttribute<uint32_t>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_INT64:
            success = applyTypedPointAttribute<int64_t>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_FLOAT:
            success = applyTypedPointAttribute<float>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_DOUBLE:
            success = applyTypedPointAttribute<double>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC2I:
            success = applyTypedPointAttribute<Imath::V2i>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC2F:
            success = applyTypedPointAttribute<Imath::V2f>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC2D:
            success = applyTypedPointAttribute<Imath::V2d>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC3I:
            success = applyTypedPointAttribute<Imath::V3i>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC3F:
            success = applyTypedPointAttribute<Imath::V3f>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_VEC3D:
            success = applyTypedPointAttribute<Imath::V3d>(
                source, target, name, neighborIds, factors);
            break;
        case DP_ATTR_BOOL:
        case DP_ATTR_STRING:
        case DP_ATTR_MATRIX:
        default:
            return kAttributeApplyUnsupported;
    }

    return success ? kAttributeApplySuccess : kAttributeApplyFailure;
}

} // namespace transfer
} // namespace dommy
