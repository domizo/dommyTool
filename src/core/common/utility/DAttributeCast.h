#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributeLookup.h"

namespace dommy {
namespace attribute {

// Values match DPAttributeCast's serialized enum. Preserve component count.
enum class CastTarget { Int = 0, Float = 1, Double = 2 };

inline bool validCastTarget(int target)
{
    return target >= static_cast<int>(CastTarget::Int) &&
        target <= static_cast<int>(CastTarget::Double);
}

template <typename Source, typename Scalar> struct CastValue { using Type = Scalar; };
template <typename T, typename Scalar>
struct CastValue<Imath::Vec2<T>, Scalar> { using Type = Imath::Vec2<Scalar>; };
template <typename T, typename Scalar>
struct CastValue<Imath::Vec3<T>, Scalar> { using Type = Imath::Vec3<Scalar>; };

template <typename Source, typename Scalar>
bool buildTypedAttributeCast(const DPAttributeData::ConstPtr& input,
                            std::size_t count, DPAttributeData::Ptr& output)
{
    using Destination = typename CastValue<Source, Scalar>::Type;
    const auto source = std::dynamic_pointer_cast<const TypedDPAttributeData<Source> >(input);
    if (!source || !validArrayConversionBudget(count, sizeof(Source))) return false;
    Destination defaultValue;
    if (!numeric::checkedValueCast(source->getDefaultValue(), defaultValue)) return false;
    std::vector<Destination> values;
    if (!convertTypedArray<Source>(input, count, values)) return false;
    DPAttributeData::Ptr candidate =
        std::make_shared<TypedDPAttributeData<Destination> >(values, defaultValue);
    output.swap(candidate);
    return true;
}

template <typename Source>
bool dispatchAttributeCast(const DPAttributeData::ConstPtr& input,
                           std::size_t count, int target, DPAttributeData::Ptr& output)
{
    switch (static_cast<CastTarget>(target)) {
    case CastTarget::Int: return buildTypedAttributeCast<Source, int32_t>(input, count, output);
    case CastTarget::Float: return buildTypedAttributeCast<Source, float>(input, count, output);
    case CastTarget::Double: return buildTypedAttributeCast<Source, double>(input, count, output);
    }
    return false;
}

// No float workspace and no implicit Imath narrowing. Failed conversion leaves
// output untouched, including when only a later value or the default is invalid.
inline bool buildAttributeCast(const DPAttributeData::ConstPtr& input,
                               std::size_t count, int target, DPAttributeData::Ptr& output)
{
    try {
        if (!validCastTarget(target) || !input || input->attrSize() != count) return false;
        switch (input->getAttrType()) {
        case DP_ATTR_INT: return dispatchAttributeCast<int32_t>(input, count, target, output);
        case DP_ATTR_FLOAT: return dispatchAttributeCast<float>(input, count, target, output);
        case DP_ATTR_DOUBLE: return dispatchAttributeCast<double>(input, count, target, output);
        case DP_ATTR_VEC2I: return dispatchAttributeCast<Imath::V2i>(input, count, target, output);
        case DP_ATTR_VEC2F: return dispatchAttributeCast<Imath::V2f>(input, count, target, output);
        case DP_ATTR_VEC2D: return dispatchAttributeCast<Imath::V2d>(input, count, target, output);
        case DP_ATTR_VEC3I: return dispatchAttributeCast<Imath::V3i>(input, count, target, output);
        case DP_ATTR_VEC3F: return dispatchAttributeCast<Imath::V3f>(input, count, target, output);
        case DP_ATTR_VEC3D: return dispatchAttributeCast<Imath::V3d>(input, count, target, output);
        default: return false; // Keep the node's nine supported storage types.
        }
    } catch (...) {
        return false;
    }
}

// Explicit type-changing operation on one existing point attribute. Ordinary
// arithmetic publication intentionally rejects type changes. Use the owner's
// existing mutable attribute iterator to swap only this field, without deleting
// it first or modifying any shared source attribute object.
template <typename Owner>
bool castPointAttribute(Owner& owner, const std::string& name, int target)
{
    if (!validAttributeName(name) || !validCastTarget(target)) return false;
    for (auto it = owner.beginAttr(); it != owner.endAttr(); ++it) {
        if (it->first != name) continue;
        DPAttributeData::Ptr candidate;
        if (!buildAttributeCast(it->second, owner.numberOfPoints(), target, candidate)) return false;
        it->second.swap(candidate); // shared_ptr::swap is the non-throwing commit.
        return true;
    }
    return false;
}

} // namespace attribute
} // namespace dommy
