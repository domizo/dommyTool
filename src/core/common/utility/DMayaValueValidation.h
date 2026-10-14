#pragma once

#include "DNumericValidation.h"

#include <maya/MBoundingBox.h>
#include <maya/MColor.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

#include <cmath>
#include <limits>

namespace dommy {
namespace maya {

inline bool isFiniteMatrix(const MMatrix& matrix) {
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            if (!std::isfinite(matrix[row][column]))
                return false;
        }
    }
    return true;
}

inline bool isFinitePoint(const MPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
           std::isfinite(point.w);
}

inline bool isFiniteBoundingBox(const MBoundingBox& bounds) {
    const MPoint minimum = bounds.min();
    const MPoint maximum = bounds.max();
    return isFinitePoint(minimum) && isFinitePoint(maximum) && minimum.x <= maximum.x &&
           minimum.y <= maximum.y && minimum.z <= maximum.z;
}

inline bool isFiniteColor(const MColor& color) {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
           std::isfinite(color.a);
}

inline bool isFiniteQuaternion(const MQuaternion& quaternion) {
    return std::isfinite(quaternion.x) && std::isfinite(quaternion.y) &&
           std::isfinite(quaternion.z) && std::isfinite(quaternion.w);
}

inline bool isFiniteVector(const MVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

inline bool isFloatRepresentablePoint(const MPoint& point) {
    return isFinitePoint(point) && numeric::canRepresentAsFloat(point.x) &&
           numeric::canRepresentAsFloat(point.y) && numeric::canRepresentAsFloat(point.z);
}

inline bool isFloatRepresentableVector(const MVector& vector) {
    return isFiniteVector(vector) && numeric::canRepresentAsFloat(vector.x) &&
           numeric::canRepresentAsFloat(vector.y) && numeric::canRepresentAsFloat(vector.z);
}

inline bool normalizedVector(const MVector& vector, MVector& result) {
    if (!isFloatRepresentableVector(vector))
        return false;

    const double length = vector.length();
    if (!std::isfinite(length) || length <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    const MVector candidate = vector / length;
    if (!isFloatRepresentableVector(candidate))
        return false;
    result = candidate;
    return true;
}

inline bool normalizedVectorOrZero(const MVector& vector, MVector& result) {
    if (!isFloatRepresentableVector(vector))
        return false;

    const double length = vector.length();
    if (!std::isfinite(length))
        return false;
    if (length <= std::numeric_limits<double>::epsilon()) {
        result = MVector::zero;
        return true;
    }

    const MVector candidate = vector / length;
    if (!isFloatRepresentableVector(candidate))
        return false;
    result = candidate;
    return true;
}

} // namespace maya
} // namespace dommy
