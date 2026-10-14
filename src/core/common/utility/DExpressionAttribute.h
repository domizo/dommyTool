#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributePublication.h"

namespace dommy {
namespace expression {

// A conditional setDPAttr call must not seed untouched points with the first
// evaluated value. Build the zero-filled field and the indexed write together
// before making the new attribute visible to later expression calls.
template <typename Owner, typename Value>
bool createPointAttribute(Owner& owner, const std::string& name, std::size_t index,
                          const Value& value) {
    const std::size_t count = owner.numberOfPoints();
    if (!attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount || index >= count ||
        !attribute::validArrayConversionBudget(count, sizeof(Value)) || owner.getAttr(name))
        return false;
    Value checked;
    if (!numeric::checkedValueCast(value, checked))
        return false;
    Value zero;
    using Components = numeric::ValueComponents<Value>;
    for (unsigned int axis = 0; axis < Components::count; ++axis)
        Components::get(zero, axis) = 0;
    TypedDPAttributeData<Value> candidate(zero, zero, count);
    if (!candidate.setValueAt(index, checked))
        return false;
    return attribute::commitStagedAttribute(owner, name, candidate.copy(), count);
}

} // namespace expression
} // namespace dommy
