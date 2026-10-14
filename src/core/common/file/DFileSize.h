#pragma once

#include "DAtomicFile.h"

#include <fstream>
#include <string>

namespace dommy {
namespace file {

inline bool fileSizeWithinLimit(
    const std::string& path, unsigned long long maximumBytes)
{
    if (!isRegularFile(path)) return false;
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streampos position = input.tellg();
    if (position == std::streampos(-1)) return false;
    const std::streamoff size = position;
    return size >= 0 && static_cast<unsigned long long>(size) <= maximumBytes;
}

} // namespace file
} // namespace dommy
