#pragma once

#include <string>

namespace dommy {
namespace script {

inline std::string escapeMelStringLiteral(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        switch (*it) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += *it; break;
        }
    }
    return escaped;
}

} // namespace script
} // namespace dommy
