#pragma once

#include "DAttributeArrayConversion.h"
#include <algorithm>
#include <numeric>

namespace dommy {
namespace attribute {

// Compare in the stored scalar type: int64 keys must never pass through double.
// Equal values retain their original index order before the node's final reverse.
template <typename Value>
bool sortValueIndices(const std::vector<Value>& values, int component, bool descending,
                      std::vector<std::size_t>& output) {
    using Components = numeric::ValueComponents<Value>;
    if (component < 0 || static_cast<unsigned int>(component) >= Components::count ||
        values.size() != output.size() ||
        !validArrayConversionBudget(values.size(), sizeof(std::size_t)))
        return false;
    for (const Value value : values)
        if (!std::isfinite(static_cast<long double>(Components::get(value, component))))
            return false;
    std::vector<std::size_t> candidate(values.size());
    std::iota(candidate.begin(), candidate.end(), 0U);
    std::sort(candidate.begin(), candidate.end(), [&](std::size_t a, std::size_t b) {
        const Value left = values[a], right = values[b];
        const auto x = Components::get(left, component), y = Components::get(right, component);
        return x == y ? a < b : descending ? x > y : x < y;
    });
    output.swap(candidate);
    return true;
}

template <typename Value>
bool sortTypedAttribute(const DPAttributeData::ConstPtr& input, int component, bool descending,
                        std::vector<std::size_t>& output) {
    const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<Value>>(input);
    if (!typed)
        return false;
    // Scalar fields historically ignore the vector component setting.
    if (numeric::ValueComponents<Value>::count == 1U)
        component = 0;
    return sortValueIndices(typed->value(), component, descending, output);
}

inline bool sortAttributeIndices(const DPAttributeData::ConstPtr& input, int component,
                                 bool descending, std::vector<std::size_t>& output) {
    if (!input || input->attrSize() != output.size())
        return false;
    switch (input->getAttrType()) {
    case DP_ATTR_BOOL:
        return sortTypedAttribute<bool>(input, component, descending, output);
    case DP_ATTR_INT:
        return sortTypedAttribute<int32_t>(input, component, descending, output);
    case DP_ATTR_UINT32:
        return sortTypedAttribute<uint32_t>(input, component, descending, output);
    case DP_ATTR_INT64:
        return sortTypedAttribute<int64_t>(input, component, descending, output);
    case DP_ATTR_FLOAT:
        return sortTypedAttribute<float>(input, component, descending, output);
    case DP_ATTR_DOUBLE:
        return sortTypedAttribute<double>(input, component, descending, output);
    case DP_ATTR_VEC2I:
        return sortTypedAttribute<Imath::V2i>(input, component, descending, output);
    case DP_ATTR_VEC2F:
        return sortTypedAttribute<Imath::V2f>(input, component, descending, output);
    case DP_ATTR_VEC2D:
        return sortTypedAttribute<Imath::V2d>(input, component, descending, output);
    case DP_ATTR_VEC3I:
        return sortTypedAttribute<Imath::V3i>(input, component, descending, output);
    case DP_ATTR_VEC3F:
        return sortTypedAttribute<Imath::V3f>(input, component, descending, output);
    case DP_ATTR_VEC3D:
        return sortTypedAttribute<Imath::V3d>(input, component, descending, output);
    default:
        return false;
    }
}

} // namespace attribute
} // namespace dommy
