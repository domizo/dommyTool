#pragma once

#include "common/utility/DCheckedSize.h"
#include "common/utility/DNumericValidation.h"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace dommy {
namespace file {

// String-output helpers publish only complete results. Rejection or allocation
// failure preserves the caller's output, including when source and output alias.
inline bool replaceAll(std::string& value,
                       const std::string& token,
                       const std::string& replacement,
                       std::size_t maximumBytes) {
    if (token.empty() || value.size() > maximumBytes) return false;
    std::size_t count = 0U, position = 0U;
    while ((position = value.find(token, position)) != std::string::npos) {
        ++count;
        position += token.size();
    }
    if (count == 0U) return true;
    std::size_t removed = 0U, inserted = 0U, size = 0U;
    if (!memory::checkedMultiply(count, token.size(), removed) ||
        !memory::checkedMultiply(count, replacement.size(), inserted) ||
        removed > value.size() ||
        !memory::checkedAdd(value.size() - removed, inserted, size) ||
        size > maximumBytes) return false;
    std::string candidate;
    candidate.reserve(size);
    position = 0U;
    for (std::size_t occurrence = 0U; occurrence < count; ++occurrence) {
        const std::size_t next = value.find(token, position);
        candidate.append(value, position, next - position);
        candidate += replacement;
        position = next + token.size();
    }
    candidate.append(value, position, value.size() - position);
    value.swap(candidate);
    return true;
}

inline bool replaceAll(std::string& value,
                       const std::string& token,
                       const std::string& replacement) {
    return replaceAll(value, token, replacement, value.max_size());
}

inline bool replaceFrameToken(const std::string& source,
                              long long frame,
                              unsigned int padding,
                              std::string& result) {
    if (padding > 32U) {
        return false;
    }

    const bool negative = frame < 0;
    unsigned long long magnitude = 0;
    if (negative) {
        magnitude = static_cast<unsigned long long>(-(frame + 1));
        ++magnitude;
    } else {
        magnitude = static_cast<unsigned long long>(frame);
    }

    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0);
    if (digits.size() < padding) {
        digits.insert(digits.begin(), padding - digits.size(), '0');
    }
    if (negative) {
        digits.insert(digits.begin(), '-');
    }

    std::string candidate(source);
    if (!replaceAll(candidate, "$F", digits)) return false;
    result.swap(candidate);
    return true;
}

inline bool formatSignedInteger(long long value,
                                unsigned int padding,
                                std::string& result) {
    if (padding > 32U) return false;

    const bool negative = value < 0;
    unsigned long long magnitude = 0U;
    if (negative) {
        magnitude = static_cast<unsigned long long>(-(value + 1));
        ++magnitude;
    } else {
        magnitude = static_cast<unsigned long long>(value);
    }

    std::ostringstream digits;
    digits.imbue(std::locale::classic());
    digits << magnitude;
    if (!digits) return false;
    std::string candidate = digits.str();
    if (candidate.size() < padding) {
        candidate.insert(candidate.begin(), padding - candidate.size(), '0');
    }
    if (negative) candidate.insert(candidate.begin(), '-');
    result.swap(candidate);
    return true;
}

inline bool checkedTruncatedFrame(double frame, long long& result) {
    if (!std::isfinite(frame)) return false;
    return numeric::checkedNumericCast(std::trunc(static_cast<long double>(frame)), result);
}

/// Replace legacy Maya sequence tokens: $F, $F<n>, and $FF.
/// $FF preserves the historical two-decimal fractional-frame form.
inline bool replaceLegacyFrameToken(const std::string& source,
                                    double frame,
                                    std::string& result) {
    if (!std::isfinite(frame)) return false;

    std::string candidate;
    candidate.reserve(source.size() + 16U);
    for (std::size_t index = 0U; index < source.size();) {
        if (source.compare(index, 3U, "$FF") == 0) {
            std::ostringstream value;
            value.imbue(std::locale::classic());
            value << std::fixed << std::setprecision(2) << frame;
            if (!value) return false;
            candidate += value.str();
            index += 3U;
            continue;
        }
        if (source.compare(index, 2U, "$F") != 0) {
            candidate += source[index++];
            continue;
        }

        std::size_t paddingEnd = index + 2U;
        unsigned int padding = 0U;
        while (paddingEnd < source.size() &&
               source[paddingEnd] >= '0' && source[paddingEnd] <= '9') {
            const unsigned int digit =
                static_cast<unsigned int>(source[paddingEnd] - '0');
            if (padding > 3U || (padding == 3U && digit > 2U)) return false;
            padding = padding * 10U + digit;
            ++paddingEnd;
        }

        long long wholeFrame = 0;
        std::string value;
        if (!checkedTruncatedFrame(frame, wholeFrame) ||
            !formatSignedInteger(wholeFrame, padding, value)) {
            return false;
        }
        candidate += value;
        index = paddingEnd;
    }
    result.swap(candidate);
    return true;
}

inline bool padSignedValue(const std::string& value,
                           unsigned int width,
                           std::string& result) {
    if (width > 32U) return false;
    std::string candidate(value);
    const bool negative = !candidate.empty() && candidate[0] == '-';
    const std::size_t digitOffset = negative ? 1U : 0U;
    const std::size_t digitCount = candidate.size() - digitOffset;
    if (digitCount < width) {
        candidate.insert(digitOffset, width - digitCount, '0');
    }
    result.swap(candidate);
    return true;
}

/// Replace each contiguous # sequence using the OpenVDB writer's three
/// historical numbering schemes.
inline bool replaceHashFrameToken(const std::string& source,
                                  double frame,
                                  double framesPerSecond,
                                  int numberingScheme,
                                  std::string& result) {
    if (!std::isfinite(frame) || !std::isfinite(framesPerSecond) ||
        framesPerSecond <= 0.0 || numberingScheme < 0 ||
        numberingScheme > 2) {
        return false;
    }

    const long double frameValue = static_cast<long double>(frame);
    long long wholeFrame = 0;
    if (!numeric::checkedNumericCast(frameValue, wholeFrame)) return false;

    const long double ticksPerFrame =
        6000.0L / static_cast<long double>(framesPerSecond);
    long long roundedTicksPerFrame = 0;
    // Admit both the original factor and its rounded value. A floating
    // LLONG_MAX comparison alone can round up to the invalid +2^63 boundary.
    if (ticksPerFrame <= 0.0L ||
        !numeric::checkedNumericCast(ticksPerFrame, roundedTicksPerFrame) ||
        !numeric::checkedNumericCast(std::round(ticksPerFrame), roundedTicksPerFrame) ||
        roundedTicksPerFrame <= 0) return false;

    std::string unpadded;
    if (numberingScheme == 1) {
        std::ostringstream value;
        value.imbue(std::locale::classic());
        value << std::setprecision(15) << frame;
        if (!value) return false;
        unpadded = value.str();
    } else if (numberingScheme == 2) {
        const long double globalTicks =
            std::round(static_cast<long double>(frame) * ticksPerFrame);
        long long tickInteger = 0;
        if (!numeric::checkedNumericCast(globalTicks, tickInteger) ||
            !formatSignedInteger(tickInteger, 0U, unpadded)) {
            return false;
        }
    } else {
        long double wholeValue = std::floor(frameValue);
        if (!numeric::checkedNumericCast(wholeValue, wholeFrame)) return false;
        long double tickValue =
            std::round((frameValue - wholeValue) * ticksPerFrame);
        if (tickValue >= roundedTicksPerFrame) {
            if (wholeFrame == std::numeric_limits<long long>::max()) return false;
            ++wholeFrame;
            tickValue = 0.0L;
        }

        if (!formatSignedInteger(wholeFrame, 0U, unpadded)) return false;
        if (tickValue > 0.0L) {
            std::string tickDigits;
            if (!formatSignedInteger(
                    roundedTicksPerFrame - 1, 0U, tickDigits)) {
                return false;
            }
            const unsigned int tickWidth =
                static_cast<unsigned int>(tickDigits.size());
            long long fractionalTicks = 0;
            if (!numeric::checkedNumericCast(tickValue, fractionalTicks) ||
                !formatSignedInteger(fractionalTicks, tickWidth, tickDigits)) {
                return false;
            }
            unpadded += '.';
            unpadded += tickDigits;
        }
    }

    std::string candidate;
    candidate.reserve(source.size() + unpadded.size());
    for (std::size_t index = 0U; index < source.size();) {
        if (source[index] != '#') {
            candidate += source[index++];
            continue;
        }
        std::size_t end = index;
        while (end < source.size() && source[end] == '#') ++end;
        const std::size_t width = end - index;
        if (width > 32U) return false;
        std::string padded;
        if (!padSignedValue(
                unpadded, static_cast<unsigned int>(width), padded)) {
            return false;
        }
        candidate += padded;
        index = end;
    }
    result.swap(candidate);
    return true;
}

} // namespace file
} // namespace dommy
