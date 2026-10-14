#pragma once

#include "DDataUtility/Data/DPAttributeData.h"
#include "DAttributeValueConversion.h"
#include "DAttributeLookup.h"

namespace dommy {
namespace attribute {

template <typename Source, typename Values>
bool readTypedNumericAttribute(const DPAttributeData::ConstPtr& input,
                               std::size_t count, Values& values)
{
    const auto attribute = std::dynamic_pointer_cast<const TypedDPAttributeData<Source> >(input);
    if (!attribute || attribute->value().size() != count) return false;
    Values candidate(count);
    for (std::size_t i = 0U; i < count; ++i)
        // In particular, reject Int64 values not exactly representable as double.
        if (!numeric::checkedValueCast(attribute->value()[i], candidate[i], true)) return false;
    values.swap(candidate);
    return true;
}

// The caller preflights its full work/copy budget before invoking this reader.
template <typename Owner, typename Values>
bool readNumericAttributeValues(const Owner& owner, const std::string& name,
                                std::size_t count, Values& values)
{
    if (!validAttributeName(name)) return false;
    const auto input = owner.getAttr(name);
    if (!input || input->attrSize() != count) return false;
    switch (input->getAttrType()) {
    case DP_ATTR_INT: return readTypedNumericAttribute<int32_t>(input, count, values);
    case DP_ATTR_UINT32: return readTypedNumericAttribute<uint32_t>(input, count, values);
    case DP_ATTR_INT64: return readTypedNumericAttribute<int64_t>(input, count, values);
    case DP_ATTR_FLOAT: return readTypedNumericAttribute<float>(input, count, values);
    case DP_ATTR_DOUBLE: return readTypedNumericAttribute<double>(input, count, values);
    case DP_ATTR_VEC2I: return readTypedNumericAttribute<Imath::V2i>(input, count, values);
    case DP_ATTR_VEC2F: return readTypedNumericAttribute<Imath::V2f>(input, count, values);
    case DP_ATTR_VEC2D: return readTypedNumericAttribute<Imath::V2d>(input, count, values);
    case DP_ATTR_VEC3I: return readTypedNumericAttribute<Imath::V3i>(input, count, values);
    case DP_ATTR_VEC3F: return readTypedNumericAttribute<Imath::V3f>(input, count, values);
    case DP_ATTR_VEC3D: return readTypedNumericAttribute<Imath::V3d>(input, count, values);
    default: return false;
    }
}

} // namespace attribute
} // namespace dommy
