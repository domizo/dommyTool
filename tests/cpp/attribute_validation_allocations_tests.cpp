// Real attribute templates and admission helpers, with tiny dynamic payloads.
// Allocation counters establish validation ordering, not a Maya memory profile.
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DAttributePartitionCopy.h"
#include "common/utility/DAttributeCreateNumeric.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>

static bool inspecting = false;
static std::size_t allocations = 0U;
void* operator new(std::size_t size) {
    if (inspecting) ++allocations;
    if (void* value = std::malloc(size ? size : 1U)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }

template <typename Action>
void inspect(Action action, bool expected) {
    allocations = 0U;
    inspecting = true;
    const bool accepted = action();
    inspecting = false;
    assert(accepted == expected && allocations == 0U);
}

template <typename T>
DPAttributeData::ConstPtr attribute(const std::vector<T>& values, const T& fallback) {
    return std::make_shared<TypedDPAttributeData<T> >(values, fallback);
}

struct PartitionOwner {
    std::size_t points;
    DPAttrList fields;
    explicit PartitionOwner(std::size_t count) : points(count) {}
    std::size_t numberOfPoints() const {return points;}
    DPAttrList::iterator beginAttr() {return fields.begin();}
    DPAttrList::iterator endAttr() {return fields.end();}
    bool isExistAttr(const std::string& name) const {return fields.count(name) != 0U;}
    void addAttr(const std::string& name, const DPAttributeData& value) {fields[name] = value.copy();}
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = fields.find(name);
        return found == fields.end() ? DPAttributeData::ConstPtr() : found->second;
    }
};

int main() {
    using namespace dommy::attribute;
    static_assert(std::is_same<decltype(std::declval<const TypedDPAttributeData<std::string>&>().defaultValue()),
        const std::string&>::value, "Default admission access must borrow storage");
    static_assert(std::is_same<decltype(std::declval<const TypedDPAttributeData<std::string>&>().getDefaultValue()),
        std::string>::value, "Keep the existing by-value API source compatible");
    const std::string text(64U, 'x'); // Above SSO, without a large allocation.
    auto rows = attribute<std::string>({text}, std::string());
    auto defaults = attribute<std::string>({std::string()}, text);
    ValidationBudget rowBudget, defaultBudget;
    rowBudget.stringBytes = defaultBudget.stringBytes = dommy::generation::kMaxGeneratedStringByteCount - 1U;
    inspect([&] {return validateTypedAttribute<std::string>(rows, 1U, rowBudget);}, false);
    inspect([&] {return validateTypedAttribute<std::string>(defaults, 1U, defaultBudget);}, false);
    assert(rowBudget.stringBytes == dommy::generation::kMaxGeneratedStringByteCount - 1U);
    assert(defaultBudget.stringBytes == rowBudget.stringBytes);
    ValidationBudget validStrings;
    inspect([&] {return validateTypedAttribute<std::string>(rows, 1U, validStrings);}, true);
    assert(validStrings.stringBytes == text.size());
    auto invalidText = attribute<std::string>({std::string("a\0b", 3U)}, std::string());
    ValidationBudget invalidStrings;
    inspect([&] {return validateTypedAttribute<std::string>(invalidText, 1U, invalidStrings);}, false);

    auto nested = attribute<DIntArray>({DIntArray{1,2,3}}, DIntArray());
    auto nestedDefaults = attribute<DIntArray>({DIntArray()}, DIntArray{4,5,6});
    ValidationBudget nestedBudget(dommy::generation::kMaxWorkItemCount - 1U);
    ValidationBudget nestedDefaultBudget(dommy::generation::kMaxWorkItemCount - 1U);
    inspect([&] {return validateTypedAttribute<DIntArray>(nested, 1U, nestedBudget);}, false);
    inspect([&] {return validateTypedAttribute<DIntArray>(nestedDefaults, 1U, nestedDefaultBudget);}, false);
    ValidationBudget validNested;
    inspect([&] {return validateTypedAttribute<DIntArray>(nested, 1U, validNested);}, true);
    assert(validNested.workItems == 3U);
    inspect([&] {return validateTypedAttribute<std::string>(nested, 1U, validNested);}, false);
    inspect([&] {return validateTypedAttribute<DIntArray>(nested, 2U, validNested);}, false);
    inspect([&] {return validateTypedAttribute<DIntArray>(DPAttributeData::ConstPtr(), 0U, validNested);}, false);

    auto bits = attribute<bool>({true,false,true}, false);
    ValidationBudget bitBudget;
    inspect([&] {return validateTypedAttribute<bool>(bits, 3U, bitBudget);}, true);
    // The proxy-like const vector<bool> access must keep its temporary alive.
    assert(std::dynamic_pointer_cast<const TypedDPAttributeData<bool> >(bits)->value()[0]);

    ValidationBudget expansion;
    inspect([&] {return budgetExpandedValues<std::string>(defaults, 2U, nullptr, expansion);}, true);
    assert(expansion.stringBytes == 128U);
    ValidationBudget excess;
    inspect([&] {return budgetExpandedValues<std::string>(defaults,
        dommy::generation::kMaxGeneratedStringByteCount, nullptr, excess);}, false);
    DPIndexList indices{0U,0U};
    ValidationBudget indexed;
    inspect([&] {return budgetExpandedValues<DIntArray>(nested, 2U, &indices, indexed);}, true);
    assert(indexed.workItems == 6U);
    ValidationBudget repeatedNested;
    inspect([&] {return budgetExpandedValues<DIntArray>(nestedDefaults, 2U, nullptr, repeatedNested);}, true);
    assert(repeatedNested.workItems == 6U);
    ValidationBudget mergeDefaults;
    inspect([&] {return dommy::point::mergeDefaultCost(defaults, mergeDefaults);}, true);
    inspect([&] {return dommy::point::mergeDefaultCost(nestedDefaults, mergeDefaults);}, true);
    assert(mergeDefaults.stringBytes == 64U && mergeDefaults.workItems == 3U);
    PartitionOwner empty(0U), nonempty(1U);
    std::vector<PartitionOwner*> owners{&empty, &nonempty};
    ValidationBudget partitionBudget;
    partitionBudget.stringBytes = dommy::generation::kMaxGeneratedStringByteCount - 66U;
    inspect([&] {return copyTypedAttributePartitions<std::string>(defaults, "v", owners.begin(),
        owners.end(), 1U, partitionBudget);}, false);
    assert(empty.fields.empty() && nonempty.fields.empty());
    assert(partitionBudget.workItems == 0U && partitionBudget.stringBytes ==
        dommy::generation::kMaxGeneratedStringByteCount - 66U);
    ValidationBudget acceptedPartition;
    assert(copyTypedAttributePartitions<std::string>(defaults, "v", owners.begin(), owners.end(), 1U,
        acceptedPartition)); // Owned output creation may allocate, after admission.
    const auto written = std::dynamic_pointer_cast<const TypedDPAttributeData<std::string> >(nonempty.getAttr("v"));
    assert(written && written->defaultValue() == text && written->value() == std::vector<std::string>{std::string()});
    PartitionOwner creationOwner(1U);
    const AttributeCreationPolicy creation(true);
    const std::string fieldName = "created";
    const MMatrix identity;
    inspect([&] { return bool(creation.create<DPMat4Attr>(creationOwner, fieldName,
        identity, identity, 1000000U)); }, false);
    inspect([&] { return bool(creation.create<DPStringAttr>(creationOwner, fieldName,
        text, std::string(), 2000000U)); }, false);
    MMatrix invalidMatrix; invalidMatrix[0][0] = NAN;
    inspect([&] { return bool(creation.create<DPMat4Attr>(creationOwner, fieldName,
        invalidMatrix, identity, 1U)); }, false);
    const std::vector<MMatrix> invalidMatrices{invalidMatrix};
    inspect([&] { return bool(creation.create<DPMat4Attr>(creationOwner, fieldName,
        invalidMatrices, identity)); }, false);
    const std::vector<std::string> invalidCreatedStrings{std::string("a\0b", 3U)};
    inspect([&] { return bool(creation.create<DPStringAttr>(creationOwner, fieldName,
        invalidCreatedStrings, std::string())); }, false);
    assert(creationOwner.fields.empty());
    std::cout << "Attribute validation/default admission: zero payload allocations, unchanged budgets and bool access pass\n";
}
