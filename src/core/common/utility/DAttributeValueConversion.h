#pragma once

#include "DNumericValidation.h"
#include <ImathVec.h>
#include <cstdint>
#include <string>
#include <type_traits>

namespace dommy {
namespace numeric {

// Do not route integer-to-integer conversions through a floating workspace:
// long double has only 53 significand bits on some supported hosts.
template <typename Destination, typename Source>
typename std::enable_if<std::is_integral<Source>::value &&
                        std::is_integral<Destination>::value, bool>::type
checkedScalarCast(Source value, Destination& result, bool = false)
{
    if (std::is_signed<Source>::value && value < 0) {
        if (!std::is_signed<Destination>::value ||
            static_cast<std::intmax_t>(value) <
                static_cast<std::intmax_t>(std::numeric_limits<Destination>::min()))
            return false;
    } else if (static_cast<std::uintmax_t>(value) >
               static_cast<std::uintmax_t>(std::numeric_limits<Destination>::max())) {
        return false;
    }
    result = static_cast<Destination>(value);
    return true;
}

template <typename Destination, typename Source>
typename std::enable_if<!std::is_integral<Source>::value ||
                        !std::is_integral<Destination>::value, bool>::type
checkedScalarCast(Source value, Destination& result, bool exact = false)
{
    Destination candidate;
    if (!checkedNumericCast(static_cast<long double>(value), candidate)) return false;
    if (exact) {
        Source restored;
        if (!checkedNumericCast(static_cast<long double>(candidate), restored) ||
            restored != value) return false;
    }
    result = candidate;
    return true;
}

template <typename T> struct ValueComponents {
    using Scalar = T;
    static const unsigned int count = 1U;
    static const T& get(const T& value, unsigned int) { return value; }
    static T& get(T& value, unsigned int) { return value; }
};

template <typename T> struct ValueComponents<Imath::Vec2<T> > {
    using Scalar = T;
    static const unsigned int count = 2U;
    static const T& get(const Imath::Vec2<T>& value, unsigned int axis) { return value[axis]; }
    static T& get(Imath::Vec2<T>& value, unsigned int axis) { return value[axis]; }
};

template <typename T> struct ValueComponents<Imath::Vec3<T> > {
    using Scalar = T;
    static const unsigned int count = 3U;
    static const T& get(const Imath::Vec3<T>& value, unsigned int axis) { return value[axis]; }
    static T& get(Imath::Vec3<T>& value, unsigned int axis) { return value[axis]; }
};

template <typename Destination, typename Source>
bool checkedValueCast(const Source& value, Destination& result, bool exact = false)
{
    using From = ValueComponents<Source>;
    using To = ValueComponents<Destination>;
    if (From::count != To::count) return false;
    Destination candidate;
    for (unsigned int axis = 0U; axis < To::count; ++axis)
        if (!checkedScalarCast(From::get(value, axis), To::get(candidate, axis), exact))
            return false;
    result = candidate;
    return true;
}

} // namespace numeric
} // namespace dommy

// Shared by all procedural attribute owners. Conversion is staged before the
// existing transactional storage replacement, including every vector component.
template <typename Destination, typename Source, typename Owner>
inline bool replaceConvertedAttributeValues(
    Owner& owner, const std::string& name, const std::vector<Source>& source)
{
    try {
        const auto attribute = owner.template getValidTypedAttributedata<Destination>(name);
        if (!attribute || attribute->value().size() != source.size()) return false;
        std::vector<Destination> converted;
        converted.reserve(source.size());
        for (const auto& value : source) {
            Destination candidate;
            if (!dommy::numeric::checkedValueCast(value, candidate)) return false;
            converted.push_back(candidate);
        }
        return attribute->replaceValues(converted, source.size());
    } catch (...) {
        return false;
    }
}
