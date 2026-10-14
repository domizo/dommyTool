#pragma once

#include "DBoundedTextRead.h"
#include "common/file/DNumericTextRead.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <istream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace dommy {
namespace file {

typedef std::array<double, 3> Triple;

const std::size_t kMaxLegacyTextBytes = 512U * 1024U * 1024U;
const std::size_t kMaxLegacyTextLineBytes = 1024U * 1024U;
const std::size_t kMaxLegacyTextRecords = 10000000U;

namespace detail {

inline bool appendTriples(std::istringstream& values, std::vector<Triple>& output) {
    std::vector<double> components;
    double value = 0.0;
    for (;;) {
        const NumericTokenRead status = readFiniteDoubleToken(values, value);
        if (status == NumericTokenRead::End) break;
        if (status != NumericTokenRead::Value) return false;
        components.push_back(value);
    }
    if (components.empty() || components.size() % 3 != 0) {
        return false;
    }

    const std::size_t newRecords = components.size() / 3;
    if (output.size() > kMaxLegacyTextRecords ||
        newRecords > kMaxLegacyTextRecords - output.size()) {
        return false;
    }
    const std::size_t required = output.size() + newRecords;
    if (required > output.capacity()) {
        // Geometric growth avoids copying the entire array on every VC/P line.
        // Never eagerly allocate from an untrusted declared record count.
        const std::size_t grown = output.capacity() > kMaxLegacyTextRecords / 2
            ? kMaxLegacyTextRecords : output.capacity() * 2;
        output.reserve(required > grown ? required : grown);
    }
    for (std::size_t index = 0; index < components.size(); index += 3) {
        output.push_back(Triple{{components[index], components[index + 1], components[index + 2]}});
    }
    return true;
}

// Enforce limits while consuming the stream, before an oversized line can be
// allocated. As before, the last line is charged one terminator even at EOF.
inline bool readLegacyLine(std::istream& input, std::string& line,
                           std::size_t& totalBytes) {
    return readBoundedLine(input, line, totalBytes, kMaxLegacyTextBytes,
                           kMaxLegacyTextLineBytes,
                           UnterminatedLinePolicy::CountTerminator);
}

inline bool isPointDataHeader(const std::string& tag) {
    return tag == "POINT_DATA_FROM_MESH" ||
           tag == "POINT_DATA_FROM_CURVE" ||
           tag == "POINT_DATA_FROM_SURFACE" ||
           tag == "POINT_DATA_FROM_PARTICLE" ||
           tag == "POINT_DATA_FROM_DPOINT";
}

inline bool pointDataSectionCountMatches(bool hasActiveSection,
                                         std::size_t sectionStart,
                                         std::size_t declaredCount,
                                         std::size_t totalCount) {
    return !hasActiveSection ||
           (totalCount >= sectionStart &&
            totalCount - sectionStart == declaredCount);
}

} // namespace detail

inline bool readVertexColors(std::istream& input, std::vector<Triple>& colors) {
    colors.clear();
    bool hasDeclaredCount = false;
    std::size_t declaredCount = 0;
    std::size_t totalBytes = 0;
    std::string line;

    try {
        while (detail::readLegacyLine(input, line, totalBytes)) {
            std::istringstream values(line);
            // This persisted format uses '.' decimals and ungrouped counts.
            // Do not inherit the host application's numeric locale.
            values.imbue(std::locale::classic());
            // String extraction can absorb allocation failure into badbit.
            // Never reinterpret such a failure as an unknown/absent tag.
            values.exceptions(std::ios::badbit);
            std::string tag;
            values >> tag;
            if (tag.empty()) {
                continue;
            }
            if (tag == "VTXCOLOR") {
                std::size_t count = 0U;
                std::string trailing;
                if (hasDeclaredCount || !readNonnegativeSizeToken(values, count) || (values >> trailing) ||
                    count > kMaxLegacyTextRecords) {
                    colors.clear();
                    return false;
                }
                declaredCount = count;
                hasDeclaredCount = true;
            } else if (tag == "VC" && !detail::appendTriples(values, colors)) {
                colors.clear();
                return false;
            }
        }
    } catch (...) {
        colors.clear();
        return false;
    }

    if (input.bad() || !input.eof() ||
        (hasDeclaredCount && declaredCount != colors.size())) {
        colors.clear();
        return false;
    }
    return true;
}

inline bool readPointData(std::istream& input, std::vector<Triple>& points) {
    points.clear();
    bool hasActiveSection = false;
    std::size_t sectionStart = 0;
    std::size_t declaredCount = 0;
    std::size_t totalBytes = 0;
    std::string line;

    try {
        while (detail::readLegacyLine(input, line, totalBytes)) {
            std::istringstream values(line);
            values.imbue(std::locale::classic());
            values.exceptions(std::ios::badbit);
            std::string tag;
            values >> tag;
            if (tag.empty()) {
                continue;
            }

            if (detail::isPointDataHeader(tag)) {
                std::string objectName;
                std::size_t count = 0U;
                std::string trailing;
                if (!detail::pointDataSectionCountMatches(
                        hasActiveSection, sectionStart, declaredCount,
                        points.size()) ||
                    (!hasActiveSection && !points.empty()) ||
                    !(values >> objectName) || !readNonnegativeSizeToken(values, count) || (values >> trailing) ||
                    count > kMaxLegacyTextRecords ||
                    count >
                        kMaxLegacyTextRecords - points.size()) {
                    points.clear();
                    return false;
                }
                declaredCount = count;
                sectionStart = points.size();
                hasActiveSection = true;
            } else if (tag == "P") {
                if (!detail::appendTriples(values, points) ||
                    (hasActiveSection &&
                     points.size() - sectionStart > declaredCount)) {
                    points.clear();
                    return false;
                }
            }
        }
    } catch (...) {
        points.clear();
        return false;
    }

    if (input.bad() || !input.eof() ||
        !detail::pointDataSectionCountMatches(
            hasActiveSection, sectionStart, declaredCount, points.size())) {
        points.clear();
        return false;
    }
    return true;
}

} // namespace file
} // namespace dommy
