#ifndef DOMMY_D_COLOR_MODE_H
#define DOMMY_D_COLOR_MODE_H

#include <cmath>

namespace dommy {
namespace color {

enum Mode {
    kSolid = 0,
    kRandom = 1,
    kRamp = 2,
    kTexture = 3,
    kBoundingBox = 4,
    kExpression = 5,
    kPalette = 6
};

enum Assignment {
    kPrimitive = 0,
    kPoint = 1
};

inline bool isValidMode(const short value) {
    return value >= kSolid && value <= kPalette;
}

inline bool isValidAssignment(const short value) {
    return value == kPrimitive || value == kPoint;
}

inline bool requiresPointAssignment(const short mode) {
    return mode == kRamp || mode == kTexture || mode == kBoundingBox ||
           mode == kExpression;
}

inline bool isFiniteTriple(
    const double first,
    const double second,
    const double third) {
    return std::isfinite(first) && std::isfinite(second) &&
           std::isfinite(third);
}

inline bool isOrderedFiniteRange(
    const double minFirst,
    const double minSecond,
    const double minThird,
    const double maxFirst,
    const double maxSecond,
    const double maxThird) {
    return isFiniteTriple(minFirst, minSecond, minThird) &&
           isFiniteTriple(maxFirst, maxSecond, maxThird) &&
           minFirst <= maxFirst && minSecond <= maxSecond &&
           minThird <= maxThird;
}

} // namespace color
} // namespace dommy

#endif
