#pragma once

#include "DDataUtility/Data/DPAttributeData.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DAttributeLookup.h"

#include <cmath>
#include <cstddef>
#include <string>

namespace dommy {
namespace attribute {

struct ValidationBudget
{
	explicit ValidationBudget(std::size_t initialWorkItems = 0U)
		: workItems(initialWorkItems), stringBytes(0U)
	{
	}

	std::size_t workItems;
	std::size_t stringBytes;
};

inline bool isFiniteMatrix(const MMatrix& matrix)
{
	for (unsigned int row = 0U; row < 4U; ++row) {
		for (unsigned int column = 0U; column < 4U; ++column) {
			if (!std::isfinite(matrix[row][column])) {
				return false;
			}
		}
	}
	return true;
}

template <typename T>
inline bool isValidValue(const T&)
{
	return true;
}

template <>
inline bool isValidValue<float>(const float& value)
{
	return std::isfinite(value);
}

template <>
inline bool isValidValue<double>(const double& value)
{
	return std::isfinite(value);
}

template <>
inline bool isValidValue<Imath::V2f>(const Imath::V2f& value)
{
	return std::isfinite(value[0]) && std::isfinite(value[1]);
}

template <>
inline bool isValidValue<Imath::V2d>(const Imath::V2d& value)
{
	return std::isfinite(value[0]) && std::isfinite(value[1]);
}

template <>
inline bool isValidValue<Imath::V3f>(const Imath::V3f& value)
{
	return std::isfinite(value[0]) &&
		   std::isfinite(value[1]) &&
		   std::isfinite(value[2]);
}

template <>
inline bool isValidValue<Imath::V3d>(const Imath::V3d& value)
{
	return std::isfinite(value[0]) &&
		   std::isfinite(value[1]) &&
		   std::isfinite(value[2]);
}

template <>
inline bool isValidValue<std::string>(const std::string& value)
{
	return value.find('\0') == std::string::npos;
}

template <>
inline bool isValidValue<MMatrix>(const MMatrix& value)
{
	return isFiniteMatrix(value);
}

template <typename T>
inline bool addDynamicValueBudget(
	const T&,
	ValidationBudget&)
{
	return true;
}

template <>
inline bool addDynamicValueBudget<std::string>(
	const std::string& value,
	ValidationBudget& budget)
{
	return generation::checkedSum(
		budget.stringBytes,
		value.size(),
		generation::kMaxGeneratedStringByteCount,
		budget.stringBytes);
}

template <>
inline bool addDynamicValueBudget<DIntArray>(
	const DIntArray& value,
	ValidationBudget& budget)
{
	return generation::checkedSum(
		budget.workItems,
		value.size(),
		generation::kMaxWorkItemCount,
		budget.workItems);
}

template <typename T>
inline bool validateTypedAttribute(
	const DPAttributeData::ConstPtr& untypedAttribute,
	std::size_t expectedCount,
	ValidationBudget& budget,
	bool budgetDefaultValue = true)
{
	const typename TypedDPAttributeData<T>::ConstPtr attribute =
		std::dynamic_pointer_cast<
			const TypedDPAttributeData<T> >(untypedAttribute);
	if (!attribute || attribute->attrSize() != expectedCount) {
		return false;
	}

	const T& defaultValue = attribute->defaultValue();
	if (!isValidValue<T>(defaultValue) ||
		(budgetDefaultValue && !addDynamicValueBudget<T>(defaultValue, budget))) {
		return false;
	}

	const std::vector<T>& values = attribute->value();
	if (values.size() != expectedCount) {
		return false;
	}
	for (std::size_t index = 0U; index < values.size(); ++index) {
		// vector<bool>'s const access yields a bool temporary; this local
		// reference extends its lifetime through the checks below.
		const T& value = values[index];
		if (!isValidValue<T>(value) ||
			!addDynamicValueBudget<T>(value, budget)) {
			return false;
		}
	}
	return true;
}

inline bool validateAttribute(
	const DPAttributeData::ConstPtr& attribute,
	std::size_t expectedCount,
	ValidationBudget& budget,
	bool budgetDefaultValue = true)
{
	if (!attribute ||
		!generation::checkedSum(
			budget.workItems,
			expectedCount,
			generation::kMaxWorkItemCount,
			budget.workItems)) {
		return false;
	}

	switch (attribute->getAttrType()) {
		case DP_ATTR_BOOL:
			return validateTypedAttribute<bool>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_INT:
			return validateTypedAttribute<int32_t>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_UINT32:
			return validateTypedAttribute<uint32_t>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_INT64:
			return validateTypedAttribute<int64_t>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_FLOAT:
			return validateTypedAttribute<float>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_DOUBLE:
			return validateTypedAttribute<double>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC2I:
			return validateTypedAttribute<Imath::V2i>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC2F:
			return validateTypedAttribute<Imath::V2f>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC2D:
			return validateTypedAttribute<Imath::V2d>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC3I:
			return validateTypedAttribute<Imath::V3i>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC3F:
			return validateTypedAttribute<Imath::V3f>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_VEC3D:
			return validateTypedAttribute<Imath::V3d>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_STRING:
			return validateTypedAttribute<std::string>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_MATRIX:
			return validateTypedAttribute<MMatrix>(
				attribute, expectedCount, budget, budgetDefaultValue);
		case DP_ATTR_INT_ARRAY:
			return validateTypedAttribute<DIntArray>(
				attribute, expectedCount, budget, budgetDefaultValue);
		default:
			return false;
	}
}

inline bool validateAttributeList(
	const DPAttrList& attributes,
	std::size_t expectedCount,
	ValidationBudget& budget)
{
	for (ConstDPAttrIter attribute = attributes.begin();
		 attribute != attributes.end(); ++attribute) {
		if (!validAttributeName(attribute->first) ||
			!generation::checkedSum(
				budget.stringBytes,
				attribute->first.size(),
				generation::kMaxGeneratedStringByteCount,
				budget.stringBytes) ||
			!validateAttribute(
				attribute->second, expectedCount, budget)) {
			return false;
		}
	}
	return true;
}

// A mesh-collection merge retains common fields and the left field's default.
// Inspect both inputs without constructing concatenated arrays. The right
// default is still validated, but it is not part of the merged payload budget.
inline bool validateAttributeMerge(const DPAttrList& base, std::size_t baseCount,
                                   const DPAttrList& target, std::size_t targetCount,
                                   ValidationBudget& budget)
{
    for (const auto& field : base) {
        const auto other = target.find(field.first);
        if (other == target.end()) continue;
        if (!field.second || !other->second ||
            field.second->getAttrType() != other->second->getAttrType() ||
            !validAttributeName(field.first) ||
            !generation::checkedSum(budget.stringBytes, field.first.size(),
                generation::kMaxGeneratedStringByteCount, budget.stringBytes) ||
            !validateAttribute(field.second, baseCount, budget) ||
            !validateAttribute(other->second, targetCount, budget, false)) return false;
    }
    return true;
}

// Budget variable-sized values before growing primitive metadata. Numeric
// attributes need only the row budget; strings and nested arrays need payloads.
template <typename T>
bool budgetExpandedValues(const DPAttributeData::ConstPtr& attribute,
                          std::size_t addedCount, const DPIndexList* sourceIndices,
                          ValidationBudget& budget)
{
    const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(attribute);
    if (!typed) return false;
    if (sourceIndices) {
        for (std::size_t index : *sourceIndices)
            if (index >= typed->value().size() ||
                !addDynamicValueBudget<T>(typed->value()[index], budget)) return false;
        return true;
    }
    ValidationBudget valueBudget;
    if (!addDynamicValueBudget<T>(typed->defaultValue(), valueBudget)) return false;
    std::size_t work = 0U, bytes = 0U;
    return generation::checkedProduct(valueBudget.workItems, addedCount,
               generation::kMaxWorkItemCount, work) &&
        generation::checkedProduct(valueBudget.stringBytes, addedCount,
               generation::kMaxGeneratedStringByteCount, bytes) &&
        generation::checkedSum(budget.workItems, work,
               generation::kMaxWorkItemCount, budget.workItems) &&
        generation::checkedSum(budget.stringBytes, bytes,
               generation::kMaxGeneratedStringByteCount, budget.stringBytes);
}

inline bool validateAttributeExpansion(const DPAttrList& attributes,
                                      std::size_t sourceCount, std::size_t addedCount,
                                      const DPIndexList* sourceIndices,
                                      ValidationBudget& budget)
{
    if ((sourceIndices && sourceIndices->size() != addedCount) ||
        !validateAttributeList(attributes, sourceCount, budget)) return false;
    if (sourceIndices)
        for (std::size_t index : *sourceIndices)
            if (index >= sourceCount) return false;
    std::size_t rows = 0U;
    if (!generation::checkedProduct(attributes.size(), addedCount,
            generation::kMaxWorkItemCount, rows) ||
        !generation::checkedSum(budget.workItems, rows,
            generation::kMaxWorkItemCount, budget.workItems)) return false;
    for (const auto& entry : attributes) {
        if (entry.second->getAttrType() == DP_ATTR_STRING &&
            !budgetExpandedValues<std::string>(entry.second, addedCount, sourceIndices, budget))
            return false;
        if (entry.second->getAttrType() == DP_ATTR_INT_ARRAY &&
            !budgetExpandedValues<DIntArray>(entry.second, addedCount, sourceIndices, budget))
            return false;
    }
    return true;
}

} // namespace attribute
} // namespace dommy
