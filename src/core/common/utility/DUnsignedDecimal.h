#pragma once

#include <cstddef>
#include <limits>
#include <string>

namespace dommy {
namespace numeric {

// ASCII decimal sizes/indices only. Signed unsigned-stream extraction can
// wrap a negative token into a small positive value. Accumulate with integer
// overflow admission and publish only a complete token. Index syntax remains
// digits-only; legacy count headers explicitly permit a leading '+'.
inline bool parseUnsignedDecimal(const std::string& token, std::size_t& value,
                                 bool allowLeadingPlus = false)
{
    if (token.empty()) return false;
    const std::size_t first = allowLeadingPlus && token[0] == '+' ? 1U : 0U;
    if (first == token.size()) return false;
    std::size_t candidate = 0U;
    for (std::size_t index = first; index < token.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(token[index]);
        if (character < '0' || character > '9') return false;
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (candidate > (std::numeric_limits<std::size_t>::max() - digit) / 10U)
            return false;
        candidate = candidate * 10U + digit;
    }
    value = candidate;
    return true;
}

} // namespace numeric
} // namespace dommy
