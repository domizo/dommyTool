#pragma once

#include <stdexcept>
#include <string>

namespace dommy {
namespace text {

// Formatted insertion can catch allocation failure and leave only a prefix.
// Preserve the caller's locale and exception mask; never publish that prefix.
template <typename Stream>
inline std::string checkedStreamText(Stream& stream, const char* context)
{
    if (!stream) throw std::runtime_error(context);
    return stream.str();
}

} // namespace text
} // namespace dommy
