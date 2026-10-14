#pragma once

#include "DDataUtility/Data/DPAttributeData.h"
#include "DAttributePublication.h"
#include "DAttributeValueConversion.h"
#include <maya/MStatus.h>

namespace dommy {
namespace attribute {

template <typename T> struct MathOutputType { using Type = float; };
template <typename T> struct MathOutputType<Imath::Vec2<T> > { using Type = Imath::V2f; };
template <typename T> struct MathOutputType<Imath::Vec3<T> > { using Type = Imath::V3f; };

template <typename Owner, typename Values>
bool replaceNumericAttributeValues(Owner& owner, const std::string& name,
                                   const Values& values)
{
    const auto attribute = owner.getAttr(name);
    if (!validAttributeName(name) || !attribute) return false;
    switch (attribute->getAttrType()) {
    case DP_ATTR_INT: return replaceConvertedAttributeValues<int32_t>(owner, name, values);
    case DP_ATTR_UINT32: return replaceConvertedAttributeValues<uint32_t>(owner, name, values);
    case DP_ATTR_INT64: return replaceConvertedAttributeValues<int64_t>(owner, name, values);
    case DP_ATTR_FLOAT: return replaceConvertedAttributeValues<float>(owner, name, values);
    case DP_ATTR_DOUBLE: return replaceConvertedAttributeValues<double>(owner, name, values);
    case DP_ATTR_VEC2I: return replaceConvertedAttributeValues<Imath::V2i>(owner, name, values);
    case DP_ATTR_VEC2F: return replaceConvertedAttributeValues<Imath::V2f>(owner, name, values);
    case DP_ATTR_VEC2D: return replaceConvertedAttributeValues<Imath::V2d>(owner, name, values);
    case DP_ATTR_VEC3I: return replaceConvertedAttributeValues<Imath::V3i>(owner, name, values);
    case DP_ATTR_VEC3F: return replaceConvertedAttributeValues<Imath::V3f>(owner, name, values);
    case DP_ATTR_VEC3D: return replaceConvertedAttributeValues<Imath::V3d>(owner, name, values);
    default: return false;
    }
}

// Owners must be staged output payloads. Preserve in-place storage types and
// convert workspaces back to historical float/vec2f/vec3f for a new field.
template <typename Owner, typename Values>
bool writeNumericAttributeValues(Owner& owner, bool createOutput, bool deleteOriginal,
                                 const std::string& outputName,
                                 const std::string& sourceName, const Values& values)
{
    try {
        if (!createOutput) return replaceNumericAttributeValues(owner, sourceName, values);
        using Stored = typename MathOutputType<typename Values::value_type>::Type;
        std::vector<Stored> converted(values.size());
        for (std::size_t i = 0U; i < values.size(); ++i)
            if (!numeric::checkedValueCast(values[i], converted[i])) return false;
        const TypedDPAttributeData<Stored> replacement(converted);
        return publishAttributeReplacement(owner, sourceName, outputName, replacement, deleteOriginal);
    } catch (...) {
        return false;
    }
}

} // namespace attribute
} // namespace dommy

// Thin Maya status adapter used by the legacy named publication entry points.
template <typename Owner, typename Values>
inline MStatus DPublishAttributeValues(
    Owner& owner, bool createOutput, bool deleteOriginal,
    const std::string& outputName, const std::string& sourceName, const Values& values)
{
    return dommy::attribute::writeNumericAttributeValues(
        owner, createOutput, deleteOriginal, outputName, sourceName, values)
        ? MS::kSuccess : MS::kInvalidParameter;
}
