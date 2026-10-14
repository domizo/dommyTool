#pragma once

#include <cctype>
#include <cstddef>
#include <string>

namespace dommy {
namespace file {

const std::size_t kMaxExportPathBytes = 32768U;

inline bool hasPathTraversalComponent(const std::string& path)
{
    std::size_t componentStart = 0U;
    for (std::size_t index = 0U; index <= path.size(); ++index) {
        if (index != path.size() &&
            path[index] != '/' && path[index] != '\\') {
            continue;
        }
        if (index - componentStart == 2U &&
            path[componentStart] == '.' &&
            path[componentStart + 1U] == '.') {
            return true;
        }
        componentStart = index + 1U;
    }
    return false;
}

inline bool hasCaseInsensitiveExtension(
    const std::string& path, const std::string& extension)
{
    if (extension.empty() || path.size() < extension.size()) {
        return false;
    }
    const std::size_t offset = path.size() - extension.size();
    for (std::size_t index = 0U; index < extension.size(); ++index) {
        const unsigned char pathCharacter =
            static_cast<unsigned char>(path[offset + index]);
        const unsigned char extensionCharacter =
            static_cast<unsigned char>(extension[index]);
        if (std::tolower(pathCharacter) !=
            std::tolower(extensionCharacter)) {
            return false;
        }
    }
    return true;
}

inline bool isSafePathSyntax(const std::string& path)
{
    if (path.empty() || path.size() > kMaxExportPathBytes ||
        path[path.size() - 1U] == '/' ||
        path[path.size() - 1U] == '\\' ||
        hasPathTraversalComponent(path)) {
        return false;
    }

    for (std::size_t index = 0U; index < path.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(path[index]);
        if (character == 0U || character < 0x20U ||
            character == 0x7fU) {
            return false;
        }
    }
    return true;
}

inline bool isSafeExportPath(const std::string& path,
                             const std::string& requiredExtension)
{
    return isSafePathSyntax(path) &&
        hasCaseInsensitiveExtension(path, requiredExtension);
}

} // namespace file
} // namespace dommy
