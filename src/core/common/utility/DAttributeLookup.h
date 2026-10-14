#pragma once

#include <string>
#include <memory>
#include <type_traits>

namespace dommy {
namespace attribute {

inline bool validAttributeName(const std::string& name)
{
    return !name.empty() && name.find('\0') == std::string::npos;
}

// Attribute names are stored keys, not tokens: whitespace is significant.
template <typename Map>
typename Map::mapped_type findAttribute(const Map& attributes,
                                       const std::string& name)
{
    if (!validAttributeName(name)) return typename Map::mapped_type();
    const auto found = attributes.find(name);
    return found == attributes.end() ? typename Map::mapped_type() : found->second;
}

// Named typed lookup is shared by point and primitive owners. RTTI verifies
// the object itself, not only a string advertised by a subclass.
template <typename Target, typename Map>
std::shared_ptr<Target> findTypedAttribute(const Map& attributes,
                                         const std::string& name)
{
    using DeclaredType = typename std::remove_const<Target>::type;
    const auto value = findAttribute(attributes, name);
    if (!value || value->typeName() != DeclaredType::staticTypeName()) return {};
    return std::dynamic_pointer_cast<Target>(value);
}

} // namespace attribute
} // namespace dommy
