#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

namespace dommy {
namespace container {

template <typename T>
inline bool findIndex(const std::vector<T>& values,
                      const T& value,
                      std::size_t& index)
{
    const typename std::vector<T>::const_iterator found =
        std::find(values.begin(), values.end(), value);
    if (found == values.end()) return false;

    index = static_cast<std::size_t>(
        std::distance(values.begin(), found));
    return true;
}

} // namespace container
} // namespace dommy
