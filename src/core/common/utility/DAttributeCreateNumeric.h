#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributePublication.h"
#include "DPointPayloadBudget.h"
#include <maya/MStatus.h>
#include <utility>

namespace dommy {
namespace attribute {

template <typename Attribute> struct CreatedAttributeValue;
template <typename Value> struct CreatedAttributeValue<TypedDPAttributeData<Value>> {
    using Type = Value;
};

// Borrow values/defaults while admitting a candidate. Do not build a repeated
// string or matrix vector merely to discover its size or invalid values.
template <typename Value>
bool validCreatedAttribute(const Value& value, const Value& defaultValue, std::size_t count)
{
    if (!validArrayConversionBudget(count, sizeof(Value)) ||
        !isValidValue(value) || !isValidValue(defaultValue)) return false;
    ValidationBudget rows, defaults;
    if (!addDynamicValueBudget(value, rows) ||
        !addDynamicValueBudget(defaultValue, defaults)) return false;
    std::size_t strings = 0U, work = 0U;
    if (!generation::checkedProduct(rows.stringBytes, count,
               generation::kMaxGeneratedStringByteCount, strings) ||
        !generation::checkedSum(strings, defaults.stringBytes,
               generation::kMaxGeneratedStringByteCount, strings) ||
        !generation::checkedProduct(rows.workItems, count,
               generation::kMaxWorkItemCount, work) ||
        !generation::checkedSum(work, defaults.workItems, generation::kMaxWorkItemCount, work))
        return false;
    try {
        point::PointPayloadBudget payload;
        payload.add(count, sizeof(Value)); payload.add(1U, sizeof(Value));
        payload.add(strings, sizeof(char)); payload.add(work, sizeof(int));
        return true;
    } catch (const std::length_error&) { return false; }
}

template <typename Value>
bool validCreatedAttribute(const std::vector<Value>& values, const Value& defaultValue)
{
    if (!validArrayConversionBudget(values.size(), sizeof(Value)) ||
        !isValidValue(defaultValue)) return false;
    ValidationBudget cost;
    if (!addDynamicValueBudget(defaultValue, cost)) return false;
    for (const auto& value : values)
        if (!isValidValue<Value>(value) || !addDynamicValueBudget<Value>(value, cost)) return false;
    try {
        point::PointPayloadBudget payload;
        payload.add(values.size(), sizeof(Value)); payload.add(1U, sizeof(Value));
        payload.add(cost.stringBytes, sizeof(char)); payload.add(cost.workItems, sizeof(int));
        return true;
    } catch (const std::length_error&) { return false; }
}

template <typename Source, typename Destination>
bool convertCreatedNumericValue(const Source& source, Destination& result)
{
    return numeric::checkedValueCast(source, result);
}

// AttributeCreate historically treats every finite nonzero scalar as true.
// Preserve that behavior instead of limiting bool inputs to the range [0, 1].
template <typename Source>
bool convertCreatedNumericValue(const Source& source, bool& result)
{
    if (!std::isfinite(static_cast<long double>(source))) return false;
    result = source != 0;
    return true;
}

// All owners are unpublished output copies. Conversion and construction must
// finish before replacement, including every vector component and its default.
template <typename Value, typename Owner, typename Source, typename Default>
MStatus createNumericAttribute(Owner& owner, const std::string& name,
                               const Source& source, const Default& defaultSource,
                               std::size_t count)
{
    Value value, defaultValue;
    if (!validArrayConversionBudget(count, sizeof(Value)) ||
        !convertCreatedNumericValue(source, value) ||
        !convertCreatedNumericValue(defaultSource, defaultValue))
        return MS::kInvalidParameter;
    DPAttributeData::Ptr candidate = std::make_shared<TypedDPAttributeData<Value>>(
        value, defaultValue, count);
    return commitStagedAttribute(owner, name, std::move(candidate), count)
        ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

template <typename Value, typename Owner, typename Source, typename Default>
MStatus createNumericAttribute(Owner& owner, const std::string& name,
                               const std::vector<Source>& source,
                               const Default& defaultSource)
{
    Value defaultValue;
    if (!validArrayConversionBudget(source.size(), sizeof(Value)) ||
        !convertCreatedNumericValue(defaultSource, defaultValue))
        return MS::kInvalidParameter;
    std::vector<Value> converted(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const Source input = source[i];
        Value value;
        if (!convertCreatedNumericValue(input, value)) return MS::kInvalidParameter;
        converted[i] = value;
    }
    DPAttributeData::Ptr candidate = std::make_shared<TypedDPAttributeData<Value>>(
        converted, defaultValue);
    return commitStagedAttribute(owner, name, std::move(candidate), source.size())
        ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

// Apply Replace/Not Add to the actual attribute owner, not its enclosing
// collection. Skip typed field conversion/construction for existing owners;
// raw point-array partitioning stays in the caller so skips do not shift offsets.
class AttributeCreationPolicy final {
public:
    explicit AttributeCreationPolicy(bool replaceExisting) : replaceExisting_(replaceExisting) {}

    template <typename Owner>
    bool shouldCreate(const Owner& owner, const std::string& name) const {
        return replaceExisting_ || !owner.isExistAttr(name);
    }

    template <typename Iterator>
    bool hasPointTargets(Iterator begin, Iterator end, const std::string& name) const {
        for (auto it = begin; it != end; ++it)
            if (shouldCreate(**it, name)) return true;
        return false;
    }

    template <typename Value, typename Owner, typename... Arguments>
    MStatus numeric(Owner& owner, const std::string& name, Arguments&&... arguments) const {
        if (!shouldCreate(owner, name)) return MS::kSuccess;
        return createNumericAttribute<Value>(owner, name, std::forward<Arguments>(arguments)...);
    }

    template <typename Attribute, typename Owner, typename Value, typename Default>
    MStatus create(Owner& owner, const std::string& name, const Value& value,
                   const Default& fallback, std::size_t count) const {
        if (!shouldCreate(owner, name)) return MS::kSuccess;
        using Stored = typename CreatedAttributeValue<Attribute>::Type;
        const Stored& stored = value;
        const Stored& defaultValue = fallback;
        if (!validAttributeName(name) || name.size() > generation::kMaxGeneratedStringByteCount ||
            !validCreatedAttribute(stored, defaultValue, count))
            return MS::kInvalidParameter;
        DPAttributeData::Ptr candidate = std::make_shared<Attribute>(stored, defaultValue, count);
        return commitStagedAttribute(owner, name, std::move(candidate), count)
            ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
    }

    template <typename Attribute, typename Owner, typename Value, typename Default>
    MStatus create(Owner& owner, const std::string& name, const std::vector<Value>& values,
                   const Default& fallback) const {
        if (!shouldCreate(owner, name)) return MS::kSuccess;
        using Stored = typename CreatedAttributeValue<Attribute>::Type;
        const Stored& defaultValue = fallback;
        if (!validAttributeName(name) || name.size() > generation::kMaxGeneratedStringByteCount ||
            !validCreatedAttribute(values, defaultValue)) return MS::kInvalidParameter;
        DPAttributeData::Ptr candidate = std::make_shared<Attribute>(values, defaultValue);
        const std::size_t count = values.size();
        return commitStagedAttribute(owner, name, std::move(candidate), count)
            ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
    }

private:
    const bool replaceExisting_;
};

} // namespace attribute
} // namespace dommy
