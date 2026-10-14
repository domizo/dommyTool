#pragma once

#include "common/utility/DUnsignedDecimal.h"

#include <cmath>
#include <istream>
#include <locale>

namespace dommy {
namespace file {

enum class NumericTokenRead { Value, End, Invalid };

// A count is a complete nonnegative decimal token, never an unsigned numeric
// prefix or a wrapped negative number. Preserve the legacy leading-plus and
// zero-padding spellings. Stream allocation failures follow the caller's mask.
inline bool readNonnegativeSizeToken(std::istream& input, std::size_t& result)
{
    std::string token;
    if (!(input >> token)) return false;
    return numeric::parseUnsignedDecimal(token, result, true);
}

// Read one complete, finite, whitespace-delimited double. Machine-format
// callers use a private classic-locale stream; this does not alter its locale.
// End is normal EOF before a token, never failed extraction of a partial token.
// The optional '#' boundary preserves ASS's trailing-comment behavior without
// consuming the comment or permitting it in legacy point/color records.
inline NumericTokenRead readFiniteDoubleToken(std::istream& input,
                                              double& result,
                                              bool allowHashBoundary = false)
{
    input >> std::ws;
    if (input.bad()) return NumericTokenRead::Invalid;
    if (input.eof()) return NumericTokenRead::End;
    if (!input) return NumericTokenRead::Invalid;

    double parsed = 0.0;
    if (!(input >> parsed) || !std::isfinite(parsed)) return NumericTokenRead::Invalid;

    if (!input.eof()) {
        const std::istream::int_type next = input.peek();
        if (input.bad() || (!input && !input.eof())) return NumericTokenRead::Invalid;
        const bool delimiter = std::char_traits<char>::eq_int_type(
            next, std::char_traits<char>::eof()) ||
            std::use_facet<std::ctype<char>>(input.getloc()).is(
                std::ctype_base::space, static_cast<char>(next)) ||
            (allowHashBoundary && next == '#');
        if (!delimiter) return NumericTokenRead::Invalid;
    }
    result = parsed;
    return NumericTokenRead::Value;
}

} // namespace file
} // namespace dommy
