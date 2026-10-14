#pragma once

#include "DAttributeArrayConversion.h"
#include "DMayaCheckedArray.h"

#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dommy {
namespace attribute {

template <typename Array>
MStatus copyStringMayaArray(const DPAttributeData::ConstPtr& input,
                           std::size_t count, Array& output, bool prefixOnly = false)
{
    using Element = typename std::remove_reference<decltype(output[0U])>::type;
    const std::size_t outputCount = prefixOnly && output.length() != 0U ? output.length() : count;
    if (!validArrayConversionBudget(outputCount, sizeof(std::string) + sizeof(Element)))
        return MS::kInvalidParameter;
    std::vector<std::string> values;
    if (!convertTypedArray<std::string>(input, count, values)) return MS::kInvalidParameter;
    return maya::buildCheckedArray(outputCount,
        [&](unsigned int i, Element& value) -> MStatus {
            return i < values.size() ? value.set(values[i].c_str()) : value.set(output[i].asChar());
        }, output);
}

// Convert all values before touching the Maya destination. Array::copy can
// itself fail; report that failure without promising rollback inside Maya.
template <typename Source, typename Value, typename Array, typename Convert>
MStatus copyTypedMayaArray(const DPAttributeData::ConstPtr& input,
                          std::size_t count, Convert convert, Array& output)
{
    using Element = typename std::remove_reference<decltype(output[0U])>::type;
    if (!validArrayConversionBudget(count, sizeof(Value) + sizeof(Element)))
        return MS::kInvalidParameter;
    std::vector<Value> values;
    if (!convertTypedArray<Source>(input, count, values)) return MS::kInvalidParameter;
    return maya::buildCheckedArray(count,
        [&](unsigned int i, Element& value) -> MStatus {
            value = convert(values[i]);
            return MS::kSuccess;
        }, output);
}

template <typename Source, typename Value, typename Array>
MStatus copyTypedMayaArray(const DPAttributeData::ConstPtr& input,
                          std::size_t count, Array& output)
{
    return copyTypedMayaArray<Source, Value>(input, count,
        [](const Value& value) { return value; }, output);
}

// Dispatch only Vec3I/F/D attributes, with checked conversion and exact counts.
// The caller supplies the host element construction (including point/vector w).
template <typename Value, typename Array, typename Convert>
MStatus copyVector3MayaArray(const DPAttributeData::ConstPtr& input,
                            std::size_t count, Convert convert, Array& output)
{
    using Element = typename std::remove_reference<decltype(output[0U])>::type;
    if (!validArrayConversionBudget(count, sizeof(Value) + sizeof(Element)))
        return MS::kInvalidParameter;
    std::vector<Value> values;
    if (!convertVector3Attribute(input, count, values)) return MS::kInvalidParameter;
    return maya::buildCheckedArray(count,
        [&](unsigned int i, Element& value) -> MStatus {
            value = convert(values[i]);
            return MS::kSuccess;
        }, output);
}

template <typename Source, typename Value>
void requireTypedArray(const DPAttributeData::ConstPtr& input,
                       std::size_t count, std::vector<Value>& output)
{
    if (!convertTypedArray<Source>(input, count, output))
        throw std::invalid_argument("Invalid or out-of-range attribute array");
}

inline void requireArrayStatus(const MStatus& status)
{
    if (status != MS::kSuccess)
        throw std::runtime_error("Attribute array conversion or Maya array copy failed");
}

// UV readers accept Vec2 or Vec3 and intentionally discard Z. Validate all
// components first, then stage both arrays. A final Maya copy failure is
// propagated; callers must discard both destinations in that case.
template <typename Source, typename Array>
MStatus copyTypedMayaUvArrays(const DPAttributeData::ConstPtr& input,
                             std::size_t count, Array& first, Array& second)
{
    if (&first == &second || !validArrayConversionBudget(count,
            sizeof(Imath::V3d) + 2U * sizeof(double))) return MS::kInvalidParameter;
    std::vector<Imath::V3d> values;
    if (!convertTypedArray<Source>(input, count, values)) return MS::kInvalidParameter;
    Array stagedFirst, stagedSecond;
    MStatus status = maya::buildCheckedArray(count,
        [&](unsigned int i, double& value) { value = values[i][0]; return MS::kSuccess; }, stagedFirst);
    if (status != MS::kSuccess) return status;
    status = maya::buildCheckedArray(count,
        [&](unsigned int i, double& value) { value = values[i][1]; return MS::kSuccess; }, stagedSecond);
    if (status != MS::kSuccess) return status;
    status = maya::copyCheckedArray(stagedFirst, first);
    if (status != MS::kSuccess) return status;
    return maya::copyCheckedArray(stagedSecond, second);
}

// These legacy readers update only a prefix of an existing array, leaving
// its length and remaining values unchanged. Empty destinations grow to fit.
template <typename Value, typename Array, typename Convert>
MStatus copyMayaArrayPrefix(const std::vector<Value>& values,
                           Convert convert, Array& output)
{
    using Element = typename std::remove_reference<decltype(output[0U])>::type;
    const std::size_t count = output.length() == 0U ? values.size() : output.length();
    if (!validArrayConversionBudget(count, sizeof(Element))) return MS::kInvalidParameter;
    return maya::buildCheckedArray(count,
        [&](unsigned int i, Element& value) -> MStatus {
            value = i < values.size() ? convert(values[i]) : output[i];
            return MS::kSuccess;
        }, output);
}

} // namespace attribute
} // namespace dommy
