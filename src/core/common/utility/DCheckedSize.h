#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace dommy {
namespace memory {

template<typename Unsigned>
inline bool checkedUnsignedAdd(Unsigned lhs, Unsigned rhs, Unsigned& result) {
    static_assert(std::is_integral<Unsigned>::value && std::is_unsigned<Unsigned>::value &&
                  !std::is_same<Unsigned, bool>::value, "An unsigned integer is required");
    if (rhs > std::numeric_limits<Unsigned>::max() - lhs) {
        return false;
    }

    result = lhs + rhs;
    return true;
}

inline bool checkedAdd(std::size_t lhs, std::size_t rhs, std::size_t& result) {
    return checkedUnsignedAdd(lhs, rhs, result);
}

inline bool checkedMultiply(std::size_t lhs, std::size_t rhs, std::size_t& result) {
    if (lhs != 0 && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
        return false;
    }

    result = lhs * rhs;
    return true;
}

inline bool checkedProduct(std::size_t first,
                           std::size_t second,
                           std::size_t third,
                           std::size_t& result) {
    std::size_t partial = 0;
    return checkedMultiply(first, second, partial) &&
           checkedMultiply(partial, third, result);
}

template<typename T>
inline const T* dataOrNull(const std::vector<T>& values) {
    return values.empty() ? nullptr : &values[0];
}

inline std::size_t boundedCopyCount(std::size_t sourceCount,
                                    std::size_t destinationCount) {
    return std::min(sourceCount, destinationCount);
}

template <typename Index>
inline bool indexInRange(Index index, std::size_t count) {
    using Integer = typename std::remove_cv<Index>::type;
    static_assert(std::is_integral<Integer>::value && !std::is_same<Integer, bool>::value &&
                  std::numeric_limits<Integer>::digits <= std::numeric_limits<std::uintmax_t>::digits,
                  "An index must be a non-bool integer representable in uintmax_t");
    // Reject signed negatives before conversion, even when count is SIZE_MAX.
    if (std::is_signed<Integer>::value && index < Integer(0)) return false;
    return static_cast<std::uintmax_t>(index) <
           static_cast<std::uintmax_t>(count);
}

// Copy only the common prefix. Callers may prefill the destination with
// semantic defaults; values beyond the source prefix are intentionally kept.
template <typename Source, typename Destination>
inline std::size_t copyPrefix(const Source& source,
                              std::size_t sourceCount,
                              Destination& destination,
                              std::size_t destinationCount) {
    const std::size_t count =
        boundedCopyCount(sourceCount, destinationCount);
    for (std::size_t index = 0U; index < count; ++index) {
        destination[index] = source[index];
    }
    return count;
}

} // namespace memory
} // namespace dommy
