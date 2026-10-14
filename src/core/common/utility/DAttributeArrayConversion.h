#pragma once

#include "DDataUtility/Data/DPAttributeData.h"
#include "DAttributeValueConversion.h"
#include "DAttributeValidation.h"
#include "common/utility/DGenerationLimits.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace dommy {
namespace attribute {

enum class ArrayKind { Int, Float, Double, Vector };

struct ConvertedArray {
    ArrayKind kind = ArrayKind::Int;
    std::vector<int> integers;
    std::vector<float> floats;
    std::vector<double> doubles;
    std::vector<Imath::V3d> vectors;
};

inline bool validArrayConversionBudget(std::size_t count, std::size_t elementBytes)
{
    std::size_t bytes = 0U;
    // Reserve room for staging, the previous result, and Maya array/data copies.
    const std::size_t maximum = 256U * 1024U * 1024U;
    return count <= generation::kMaxGeneratedPointCount &&
        count <= std::numeric_limits<unsigned int>::max() &&
        generation::checkedProduct(count, elementBytes, maximum, bytes) &&
        generation::checkedProduct(bytes, 4U, maximum, bytes);
}

template <typename Source, typename Destination>
bool convertTypedArray(const DPAttributeData::ConstPtr& input,
                       std::size_t count, std::vector<Destination>& output)
{
    const auto attribute = std::dynamic_pointer_cast<const TypedDPAttributeData<Source> >(input);
    if (!attribute || attribute->attrSize() != count || attribute->value().size() != count ||
        !validArrayConversionBudget(count, sizeof(Destination))) return false;
    using From = numeric::ValueComponents<Source>;
    using To = numeric::ValueComponents<Destination>;
    if (From::count != To::count && !(From::count == 2U && To::count == 3U)) return false;
    std::vector<Destination> candidate(count);
    for (std::size_t i = 0U; i < count; ++i) {
        for (unsigned int axis = 0U; axis < To::count; ++axis) {
            auto& component = To::get(candidate[i], axis);
            if (axis >= From::count) component = 0;
            else if (!numeric::checkedScalarCast(
                         From::get(attribute->value()[i], axis), component)) return false;
        }
    }
    output.swap(candidate);
    return true;
}

template <typename Value>
bool copyNonNumericArray(const DPAttributeData::ConstPtr& input,
                         std::size_t count, std::vector<Value>& output)
{
    ValidationBudget budget;
    std::size_t copiedStringBytes = 0U;
    if (!validArrayConversionBudget(count, sizeof(Value)) ||
        !validateTypedAttribute<Value>(input, count, budget) ||
        !generation::checkedProduct(budget.stringBytes, 4U,
            256U * 1024U * 1024U, copiedStringBytes)) return false;
    const auto attribute = std::dynamic_pointer_cast<const TypedDPAttributeData<Value> >(input);
    std::vector<Value> candidate(attribute->value());
    output.swap(candidate);
    return true;
}

template <>
inline bool convertTypedArray<std::string, std::string>(
    const DPAttributeData::ConstPtr& input, std::size_t count,
    std::vector<std::string>& output)
{
    return copyNonNumericArray(input, count, output);
}

template <>
inline bool convertTypedArray<MMatrix, MMatrix>(
    const DPAttributeData::ConstPtr& input, std::size_t count,
    std::vector<MMatrix>& output)
{
    return copyNonNumericArray(input, count, output);
}

// Preserve the scalar types accepted by the legacy weight readers, but reject
// present wrong-type or wrong-length fields instead of retaining defaults.
template <typename Destination>
inline bool convertScalarAttribute(const DPAttributeData::ConstPtr& input,
                                   std::size_t count, std::vector<Destination>& output)
{
    if (!input) return false;
    switch (input->getAttrType()) {
    case DP_ATTR_INT: return convertTypedArray<int32_t>(input, count, output);
    case DP_ATTR_UINT32: return convertTypedArray<uint32_t>(input, count, output);
    case DP_ATTR_INT64: return convertTypedArray<int64_t>(input, count, output);
    case DP_ATTR_FLOAT: return convertTypedArray<float>(input, count, output);
    case DP_ATTR_DOUBLE: return convertTypedArray<double>(input, count, output);
    default: return false;
    }
}

// Curve weights historically accept scalar/bool and the first component of
// Vec2/Vec3 values. Keep that contract without allocating unchecked Maya arrays.
template <typename Source>
bool convertTypedWeightArray(const DPAttributeData::ConstPtr& input,
                             std::size_t count, std::vector<double>& output)
{
    const auto attribute = std::dynamic_pointer_cast<const TypedDPAttributeData<Source> >(input);
    if (!attribute || attribute->attrSize() != count || attribute->value().size() != count ||
        !validArrayConversionBudget(count, sizeof(double))) return false;
    std::vector<double> candidate(count);
    for (std::size_t i = 0U; i < count; ++i) {
        const Source value = attribute->value()[i]; // Also materializes vector<bool> values.
        if (!numeric::checkedScalarCast(numeric::ValueComponents<Source>::get(value, 0U),
                                       candidate[i])) return false;
    }
    output.swap(candidate);
    return true;
}

inline bool convertWeightAttribute(const DPAttributeData::ConstPtr& input,
                                   std::size_t count, std::vector<double>& output)
{
    if (!input) return false;
    switch (input->getAttrType()) {
    case DP_ATTR_BOOL: return convertTypedWeightArray<bool>(input, count, output);
    case DP_ATTR_VEC2I: return convertTypedWeightArray<Imath::V2i>(input, count, output);
    case DP_ATTR_VEC2F: return convertTypedWeightArray<Imath::V2f>(input, count, output);
    case DP_ATTR_VEC2D: return convertTypedWeightArray<Imath::V2d>(input, count, output);
    case DP_ATTR_VEC3I: return convertTypedWeightArray<Imath::V3i>(input, count, output);
    case DP_ATTR_VEC3F: return convertTypedWeightArray<Imath::V3f>(input, count, output);
    case DP_ATTR_VEC3D: return convertTypedWeightArray<Imath::V3d>(input, count, output);
    default: return convertScalarAttribute(input, count, output);
    }
}

// Strict Vec3 readers reject present scalar/string/Vec2 fields. Missing
// optional attributes are handled by the caller before this boundary.
template <typename Destination>
bool convertVector3Attribute(const DPAttributeData::ConstPtr& input,
                             std::size_t count, std::vector<Destination>& output)
{
    if (!input) return false;
    switch (input->getAttrType()) {
    case DP_ATTR_VEC3I: return convertTypedArray<Imath::V3i>(input, count, output);
    case DP_ATTR_VEC3F: return convertTypedArray<Imath::V3f>(input, count, output);
    case DP_ATTR_VEC3D: return convertTypedArray<Imath::V3d>(input, count, output);
    default: return false;
    }
}

// The actual node dispatch is host-independent and tested with real typed
// attributes. Never route Int32/UInt32/Int64 or double through float storage.
inline bool convertAttributeToArray(const DPAttributeData::ConstPtr& input,
                                    std::size_t expectedCount, ConvertedArray& output)
{
    if (!input || input->attrSize() != expectedCount) return false;
    ConvertedArray candidate;
    bool converted = false;
    switch (input->getAttrType()) {
    case DP_ATTR_INT:
        converted = convertTypedArray<int32_t>(input, expectedCount, candidate.integers); break;
    case DP_ATTR_UINT32:
        converted = convertTypedArray<uint32_t>(input, expectedCount, candidate.integers); break;
    case DP_ATTR_INT64:
        converted = convertTypedArray<int64_t>(input, expectedCount, candidate.integers); break;
    case DP_ATTR_FLOAT:
        candidate.kind = ArrayKind::Float;
        converted = convertTypedArray<float>(input, expectedCount, candidate.floats); break;
    case DP_ATTR_DOUBLE:
        candidate.kind = ArrayKind::Double;
        converted = convertTypedArray<double>(input, expectedCount, candidate.doubles); break;
    case DP_ATTR_VEC2I:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V2i>(input, expectedCount, candidate.vectors); break;
    case DP_ATTR_VEC2F:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V2f>(input, expectedCount, candidate.vectors); break;
    case DP_ATTR_VEC2D:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V2d>(input, expectedCount, candidate.vectors); break;
    case DP_ATTR_VEC3I:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V3i>(input, expectedCount, candidate.vectors); break;
    case DP_ATTR_VEC3F:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V3f>(input, expectedCount, candidate.vectors); break;
    case DP_ATTR_VEC3D:
        candidate.kind = ArrayKind::Vector;
        converted = convertTypedArray<Imath::V3d>(input, expectedCount, candidate.vectors); break;
    default: return false; // Preserve the node's existing supported value types.
    }
    if (!converted) return false;
    output = std::move(candidate);
    return true;
}

} // namespace attribute
} // namespace dommy
