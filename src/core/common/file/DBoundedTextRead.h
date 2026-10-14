#pragma once

#include <cstddef>
#include <istream>
#include <string>

namespace dommy {
namespace file {

enum class UnterminatedLinePolicy { PhysicalBytes, CountTerminator };

// Bound storage while consuming, not after getline has allocated a hostile
// line. A rejected line is never a successful record. At most one lookahead
// byte is consumed beyond a limit; it is never appended to the line.
inline bool readBoundedLine(std::istream& input, std::string& line,
                            std::size_t& totalBytes, std::size_t maxTotalBytes,
                            std::size_t maxLineBytes,
                            UnterminatedLinePolicy eofPolicy)
{
    line.clear();
    char value = 0;
    while (input.get(value)) {
        if (totalBytes >= maxTotalBytes) {
            input.setstate(std::ios::failbit);
            return false;
        }
        ++totalBytes;
        if (value == '\n') return true;
        if (line.size() >= maxLineBytes) {
            input.setstate(std::ios::failbit);
            return false;
        }
        line.push_back(value);
    }
    if (!input.eof() || input.bad() || line.empty()) return false;
    if (eofPolicy == UnterminatedLinePolicy::CountTerminator) {
        if (totalBytes >= maxTotalBytes) {
            input.setstate(std::ios::badbit);
            return false;
        }
        ++totalBytes;
    }
    return true;
}

} // namespace file
} // namespace dommy
