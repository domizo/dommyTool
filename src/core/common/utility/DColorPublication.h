#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributePublication.h"
#include "DColorMode.h"

namespace dommy {
namespace color {

template <typename Owner, typename Value>
bool publishAttribute(Owner& owner, const std::string& name,
                      const TypedDPAttributeData<Value>& attribute, std::size_t count) {
    static_assert(numeric::ValueComponents<Value>::count == 3U,
                  "Color attributes require three components");
    if (!dommy::attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount || attribute.attrSize() != count ||
        !dommy::attribute::validArrayConversionBudget(count, sizeof(Value)))
        return false;
    const Value fallback = attribute.getDefaultValue();
    if (!isFiniteTriple(fallback[0], fallback[1], fallback[2]))
        return false;
    for (const Value& value : attribute.value())
        if (!isFiniteTriple(value[0], value[1], value[2]))
            return false;
    return dommy::attribute::commitStagedAttribute(owner, name, attribute.copy(), count);
}

template <typename Owner, typename Value>
bool publishValues(Owner& owner, const std::string& name, const std::vector<Value>& values,
                   std::size_t count) {
    if (!attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount || values.size() != count ||
        !attribute::validArrayConversionBudget(count, sizeof(Value)))
        return false;
    return publishAttribute(owner, name, TypedDPAttributeData<Value>(values), count);
}

} // namespace color
} // namespace dommy
