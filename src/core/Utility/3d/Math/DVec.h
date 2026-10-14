#pragma once
#ifndef DVec_h
#define DVec_h

// Project-owned fixed-width vectors. Storage, overloads and the historical
// floating-point policies are compatibility contracts, not a third-party import.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "DType.h"
#include "math/DVectorNormalization.h"
#include "common/utility/DNumericValidation.h"

// These two public macros are retained solely as compatibility options.
// In particular, callers may override the homogeneous-projection tolerance.
#ifndef VEC_RX_FEQ_INF
#define VEC_RX_FEQ_INF 1.0e10
#endif
#ifndef VEC_RX_FEQ_EPS
#define VEC_RX_FEQ_EPS 1e-10
#endif

namespace DTools {
namespace detail {
template<std::size_t N>
void copyComponents(DReal (&destination)[N], const DReal* source)
{
    for (std::size_t axis = 0; axis != N; ++axis) destination[axis] = source[axis];
}
template<std::size_t N>
void fillComponents(DReal (&destination)[N], const DReal& value)
{
    for (std::size_t axis = 0; axis != N; ++axis) destination[axis] = value;
}
template<std::size_t N, class Operation>
void combineComponents(DReal (&destination)[N], const DReal (&other)[N], Operation operation)
{
    for (std::size_t axis = 0; axis != N; ++axis)
        destination[axis] = operation(destination[axis], other[axis]);
}
template<std::size_t N>
void multiplyComponents(DReal (&destination)[N], const DReal& factor)
{
    // Keep reference aliasing and serial updates for the Vec2/Vec3 API.
    for (std::size_t axis = 0; axis != N; ++axis) destination[axis] *= factor;
}
template<std::size_t N>
void reciprocalDivide(DReal (&destination)[N], const DReal& divisor)
{
    const DReal reciprocal = 1.0 / divisor;
    multiplyComponents(destination, reciprocal);
}
template<std::size_t N>
bool equalComponents(const DReal (&left)[N], const DReal (&right)[N])
{
    for (std::size_t axis = 0; axis != N; ++axis)
        if (!(left[axis] == right[axis])) return false;
    return true;
}
template<class Vector, class Operation>
Vector mapComponents(const Vector& source, Operation operation)
{
    Vector result;
    for (std::size_t axis = 0; axis != sizeof(result.data) / sizeof(DReal); ++axis)
        result.data[axis] = operation(source.data[axis]);
    return result;
}
template<class Vector, class Operation>
Vector zipComponents(const Vector& left, const Vector& right, Operation operation)
{
    Vector result;
    for (std::size_t axis = 0; axis != sizeof(result.data) / sizeof(DReal); ++axis)
        result.data[axis] = operation(left.data[axis], right.data[axis]);
    return result;
}
template<class Vector, class Scalar>
Vector scaledVector(const Vector& value, const Scalar& factor)
{
    return mapComponents(value, [&](DReal lane) { return lane * factor; });
}
template<class Vector, class Scalar>
Vector dividedVector(const Vector& value, const Scalar& divisor)
{
    const DReal reciprocal = 1.0 / divisor;
    return scaledVector(value, reciprocal);
}
template<std::size_t N>
DReal componentDot(const DReal (&left)[N], const DReal (&right)[N])
{
    // Do not start with +0 or widen: both would alter the existing arithmetic.
    DReal sum = left[0] * right[0];
    for (std::size_t axis = 1; axis != N; ++axis) sum = sum + left[axis] * right[axis];
    return sum;
}
template<class Vector>
std::ostream& writeComponents(std::ostream& stream, const Vector& value)
{
    stream << "(";
    for (std::size_t axis = 0; axis != sizeof(value.data) / sizeof(DReal); ++axis) {
        if (axis) stream << ", ";
        stream << value.data[axis];
    }
    return stream << ")";
}
template<class Vector>
std::istream& readComponents(std::istream& stream, Vector& value)
{
    for (std::size_t axis = 0; axis != sizeof(value.data) / sizeof(DReal); ++axis)
        stream >> value.data[axis];
    return stream;
}
} // namespace detail

class Vec3 {
public:
    DReal data[3];

    Vec3() : data{} {}
    Vec3(const DReal& value) { detail::fillComponents(data, value); }
    Vec3(const Vec3& value) { detail::copyComponents(data, value.data); }
    Vec3(const DReal* value) { std::memcpy(data, value, sizeof(data)); }
    Vec3(DReal x, DReal y, DReal z) : data{x, y, z} {}
    Vec3(DReal x, DReal y) : data{x, y, 0.0f} {}
    void SetValue(const DReal* value) { detail::copyComponents(data, value); }
    void SetValue(const DReal& x, const DReal& y, const DReal& z) {
        data[0] = x; data[1] = y; data[2] = z;
    }
    void GetValue(DReal* value) const {
        for (std::size_t axis = 0; axis != 3; ++axis) value[axis] = data[axis];
    }

    explicit operator DReal*() { return data; }
    explicit operator const DReal*() const { return data; }
    DReal& operator[](int axis) { return data[axis]; }
    DReal operator[](int axis) const { return data[axis]; }
    Vec3& operator=(const Vec3& value) {
        detail::copyComponents(data, value.data); return *this;
    }
    Vec3& operator=(const DReal& value) {
        detail::fillComponents(data, value); return *this;
    }
    Vec3& operator+=(const Vec3& value) {
        detail::combineComponents(data, value.data, std::plus<DReal>()); return *this;
    }
    Vec3& operator-=(const Vec3& value) {
        detail::combineComponents(data, value.data, std::minus<DReal>()); return *this;
    }
    Vec3& operator*=(const DReal& factor) {
        detail::multiplyComponents(data, factor); return *this;
    }
    Vec3& operator*=(const Vec3& value) {
        detail::combineComponents(data, value.data, std::multiplies<DReal>()); return *this;
    }
    Vec3& operator/=(const DReal& divisor) {
        detail::reciprocalDivide(data, divisor); return *this;
    }
    bool operator==(const Vec3& value) const { return detail::equalComponents(data, value.data); }
    bool operator!=(const Vec3& value) const { return !(*this == value); }
    bool operator<(const Vec3& value) const;
    bool operator>(const Vec3& value) const;
    bool operator<=(const Vec3& value) const;
    bool operator>=(const Vec3& value) const;
};

class Vec2 {
public:
    DReal data[2];

    Vec2() : data{} {}
    Vec2(const DReal& value) { detail::fillComponents(data, value); }
    Vec2(const Vec2& value) { detail::copyComponents(data, value.data); }
    Vec2(const DReal* value) { std::memcpy(data, value, sizeof(data)); }
    Vec2(DReal x, DReal y) : data{x, y} {}

    explicit operator DReal*() { return data; }
    explicit operator const DReal*() const { return data; }
    DReal& operator[](int axis) { return data[axis]; }
    DReal operator[](int axis) const { return data[axis]; }
    Vec2& operator=(const Vec2& value) {
        detail::copyComponents(data, value.data); return *this;
    }
    Vec2& operator=(const DReal& value) {
        detail::fillComponents(data, value); return *this;
    }
    Vec2& operator+=(const Vec2& value) {
        detail::combineComponents(data, value.data, std::plus<DReal>()); return *this;
    }
    Vec2& operator-=(const Vec2& value) {
        detail::combineComponents(data, value.data, std::minus<DReal>()); return *this;
    }
    Vec2& operator*=(const DReal& factor) {
        detail::multiplyComponents(data, factor); return *this;
    }
    Vec2& operator*=(const Vec2& value) {
        detail::combineComponents(data, value.data, std::multiplies<DReal>()); return *this;
    }
    Vec2& operator/=(const DReal& divisor) {
        detail::reciprocalDivide(data, divisor); return *this;
    }
    bool operator==(const Vec2& value) const { return detail::equalComponents(data, value.data); }
    bool operator!=(const Vec2& value) const { return !(*this == value); }
    bool operator<(const Vec2& value) const;
    bool operator>(const Vec2& value) const;
    bool operator<=(const Vec2& value) const;
    bool operator>=(const Vec2& value) const;
};

class Vec4 {
public:
    DReal data[4];

    Vec4() : data{} {}
    Vec4(DReal value) { detail::fillComponents(data, value); }
    Vec4(const Vec4& value) { detail::copyComponents(data, value.data); }
    Vec4(const DReal* value) { std::memcpy(data, value, sizeof(data)); }
    Vec4(DReal x, DReal y, DReal z, DReal w) : data{x, y, z, w} {}
    Vec4(const Vec3& value) : data{value[0], value[1], value[2], 0.0f} {}
    Vec4(const Vec3& value, const DReal& w) : data{value[0], value[1], value[2], w} {}
    Vec4& operator=(const Vec3& value) {
        for (std::size_t axis = 0; axis != 3; ++axis) data[axis] = value[static_cast<int>(axis)];
        data[3] = 0.0f;
        return *this;
    }

    explicit operator DReal*() { return data; }
    explicit operator const DReal*() const { return data; }
    DReal& operator[](int axis) { return data[axis]; }
    DReal operator[](int axis) const { return data[axis]; }
    Vec4& operator=(const Vec4& value) {
        detail::copyComponents(data, value.data); return *this;
    }
    Vec4& operator=(DReal value) {
        detail::fillComponents(data, value); return *this;
    }
    Vec4& operator+=(const Vec4& value) {
        detail::combineComponents(data, value.data, std::plus<DReal>()); return *this;
    }
    Vec4& operator-=(const Vec4& value) {
        detail::combineComponents(data, value.data, std::minus<DReal>()); return *this;
    }
    Vec4& operator*=(DReal factor) {
        detail::multiplyComponents(data, factor); return *this;
    }
    Vec4& operator/=(DReal divisor) {
        // Vec4 compound division has always divided each lane directly.
        for (std::size_t axis = 0; axis != 4; ++axis) data[axis] /= divisor;
        return *this;
    }
    bool operator==(const Vec4& value) const { return detail::equalComponents(data, value.data); }
    bool operator!=(const Vec4& value) const { return !(*this == value); }
};

// Vector comparison, magnitude and projection helpers.
namespace detail {

inline int compareScalar(double left, double right)
{
    // NaNs sort after numeric values. All NaNs are equivalent for ordering,
    // while == retains ordinary component-wise floating-point equality.
    if (std::isnan(left)) return std::isnan(right) ? 0 : 1;
    if (std::isnan(right)) return -1;
    return left < right ? -1 : left > right ? 1 : 0;
}

inline double squaredLengthWide(const Vec2& value)
{
    // DReal is float; promote before multiplication to avoid float overflow.
    const double x = value[0], y = value[1];
    return x * x + y * y;
}

template <std::size_t N>
inline double vectorLengthWide(const DReal (&values)[N])
{
    double result = 0.0;
    for (std::size_t i = 0U; i < N; ++i) {
        if (std::isnan(values[i]))
            return std::numeric_limits<double>::quiet_NaN();
        result = std::hypot(result, static_cast<double>(values[i]));
    }
    return result;
}

inline DReal narrowLength(double length)
{
    // A finite vector can have a magnitude outside DReal's range. Keep its
    // normalized direction valid and represent that magnitude as +infinity.
    return length > std::numeric_limits<DReal>::max()
        ? std::numeric_limits<DReal>::infinity() : static_cast<DReal>(length);
}

// Scale only the direction: float products fit exactly in double, and the
// scaled denominator stays in [1,N] even for subnormal/maximum directions.
// Stage the result so rejection also preserves aliased/in-place operands.
template <std::size_t N>
inline bool removeParallelComponent(const DReal (&a)[N], const DReal (&b)[N],
                                    DReal (&result)[N])
{
    std::size_t pivot = 0U;
    double scale = 0.0;
    for (std::size_t axis = 0U; axis < N; ++axis) {
        if (!std::isfinite(a[axis]) || !std::isfinite(b[axis])) return false;
        const double magnitude = std::fabs(static_cast<double>(b[axis]));
        if (magnitude > scale) { scale = magnitude; pivot = axis; }
    }
    std::array<DReal, N> candidate = {};
    if (scale == 0.0) {
        std::copy(a, a + N, candidate.begin());
    } else {
        bool parallel = true;
        for (std::size_t axis = 0U; axis < N; ++axis) {
            if (static_cast<double>(a[axis]) * b[pivot] !=
                static_cast<double>(a[pivot]) * b[axis]) parallel = false;
        }
        // Exact collinearity avoids a spurious residual for very large inputs.
        if (!parallel) {
            std::array<double, N> direction = {};
            double denominator = 0.0, numerator = 0.0;
            for (std::size_t axis = 0U; axis < N; ++axis) {
                direction[axis] = static_cast<double>(b[axis]) / scale;
                denominator = std::fma(direction[axis], direction[axis], denominator);
                numerator = std::fma(static_cast<double>(a[axis]), direction[axis], numerator);
            }
            const double factor = numerator / denominator;
            for (std::size_t axis = 0U; axis < N; ++axis) {
                if (!dommy::numeric::checkedFloatCast(
                        std::fma(-direction[axis], factor, static_cast<double>(a[axis])),
                        candidate[axis])) return false;
            }
        }
    }
    std::copy(candidate.begin(), candidate.end(), result);
    return true;
}

} // namespace detail


inline bool Vec3::operator<(const Vec3& value) const
{
    for (int axis = 0; axis != 3; ++axis) {
        const int order = detail::compareScalar(data[axis], value[axis]);
        if (order) return order < 0;
    }
    return false;
}
inline bool Vec3::operator>(const Vec3& value) const { return value < *this; }
inline bool Vec3::operator<=(const Vec3& value) const { return !(value < *this); }
inline bool Vec3::operator>=(const Vec3& value) const { return !(*this < value); }
inline bool Vec2::operator<(const Vec2& value) const
{
    return detail::compareScalar(detail::squaredLengthWide(*this),
                                 detail::squaredLengthWide(value)) < 0;
}
inline bool Vec2::operator>(const Vec2& value) const { return value < *this; }
inline bool Vec2::operator<=(const Vec2& value) const { return !(value < *this); }
inline bool Vec2::operator>=(const Vec2& value) const { return !(*this < value); }

inline Vec2 operator*(const Vec2& value, DReal factor) { return detail::scaledVector(value, factor); }
inline Vec2 operator*(DReal factor, const Vec2& value) { return detail::scaledVector(value, factor); }
inline Vec2 operator/(const Vec2& value, DReal divisor) { return detail::dividedVector(value, divisor); }
inline Vec2 operator+(const Vec2& left, const Vec2& right) { return detail::zipComponents(left, right, std::plus<DReal>()); }
inline Vec2 operator-(const Vec2& left, const Vec2& right) { return detail::zipComponents(left, right, std::minus<DReal>()); }
inline Vec2 operator*(const Vec2& left, const Vec2& right) { return detail::zipComponents(left, right, std::multiplies<DReal>()); }
inline Vec2 operator/(const Vec2& left, const Vec2& right) { return detail::zipComponents(left, right, std::divides<DReal>()); }
inline Vec2 operator-(const Vec2& value) { return detail::mapComponents(value, [](DReal lane) { return -lane; }); }
inline DReal dot(const Vec2& left, const Vec2& right) { return detail::componentDot(left.data, right.data); }
inline DReal norm2(const Vec2& value) { return dot(value, value); }
inline DReal norm(const Vec2& value) { return detail::narrowLength(detail::vectorLengthWide(value.data)); }
inline std::ostream& operator<<(std::ostream& stream, const Vec2& value) { return detail::writeComponents(stream, value); }
inline std::istream& operator>>(std::istream& stream, Vec2& value) { return detail::readComponents(stream, value); }

inline Vec3 operator*(const Vec3& value, const DReal& factor) { return detail::scaledVector(value, factor); }
inline Vec3 operator*(const DReal& factor, const Vec3& value) { return detail::scaledVector(value, factor); }
inline Vec3 operator/(const Vec3& value, const DReal& divisor) { return detail::dividedVector(value, divisor); }
inline Vec3 operator*(const Vec3& value, const int& factor) { return detail::scaledVector(value, factor); }
inline Vec3 operator*(const int& factor, const Vec3& value) { return detail::scaledVector(value, factor); }
inline Vec3 operator+(const Vec3& left, const Vec3& right) { return detail::zipComponents(left, right, std::plus<DReal>()); }
inline Vec3 operator-(const Vec3& left, const Vec3& right) { return detail::zipComponents(left, right, std::minus<DReal>()); }
inline Vec3 operator*(const Vec3& left, const Vec3& right) { return detail::zipComponents(left, right, std::multiplies<DReal>()); }
inline Vec3 operator/(const Vec3& left, const Vec3& right) { return detail::zipComponents(left, right, std::divides<DReal>()); }
inline Vec3 operator-(const Vec3& value) { return detail::mapComponents(value, [](DReal lane) { return -lane; }); }
inline DReal dot(const Vec3& left, const Vec3& right) { return detail::componentDot(left.data, right.data); }
inline DReal norm2(const Vec3& value) { return dot(value, value); }
inline DReal norm(const Vec3& value) { return detail::narrowLength(detail::vectorLengthWide(value.data)); }
inline std::ostream& operator<<(std::ostream& stream, const Vec3& value) { return detail::writeComponents(stream, value); }
inline std::istream& operator>>(std::istream& stream, Vec3& value) { return detail::readComponents(stream, value); }

inline Vec4 operator*(const Vec4& value, DReal factor) { return detail::scaledVector(value, factor); }
inline Vec4 operator*(DReal factor, const Vec4& value) { return detail::scaledVector(value, factor); }
inline Vec4 operator/(const Vec4& value, DReal divisor) { return detail::dividedVector(value, divisor); }
inline Vec4 operator*(const Vec4& value, int factor) { return detail::scaledVector(value, factor); }
inline Vec4 operator*(int factor, const Vec4& value) { return detail::scaledVector(value, factor); }
inline Vec4 operator/(const Vec4& value, int divisor) { return detail::dividedVector(value, divisor); }
inline Vec4 operator+(const Vec4& left, const Vec4& right) { return detail::zipComponents(left, right, std::plus<DReal>()); }
inline Vec4 operator-(const Vec4& left, const Vec4& right) { return detail::zipComponents(left, right, std::minus<DReal>()); }
inline Vec4 operator*(const Vec4& left, const Vec4& right) { return detail::zipComponents(left, right, std::multiplies<DReal>()); }
inline Vec4 operator/(const Vec4& left, const Vec4& right) { return detail::zipComponents(left, right, std::divides<DReal>()); }
inline Vec4 operator-(const Vec4& value) { return detail::mapComponents(value, [](DReal lane) { return -lane; }); }
inline DReal dot(const Vec4& left, const Vec4& right) { return detail::componentDot(left.data, right.data); }
inline DReal norm2(const Vec4& value) { return dot(value, value); }
inline DReal norm(const Vec4& value) { return detail::narrowLength(detail::vectorLengthWide(value.data)); }
inline std::ostream& operator<<(std::ostream& stream, const Vec4& value) { return detail::writeComponents(stream, value); }
inline std::istream& operator>>(std::istream& stream, Vec4& value) { return detail::readComponents(stream, value); }

inline Vec3 cross(const Vec3& left, const Vec3& right)
{
    return Vec3(left[1] * right[2] - left[2] * right[1],
                left[2] * right[0] - left[0] * right[2],
                left[0] * right[1] - left[1] * right[0]);
}
inline DReal cross(const Vec2& left, const Vec2& right)
{
    return left[0] * right[1] - left[1] * right[0];
}
inline DReal length(const Vec3& value) { return norm(value); }

inline DReal unitize(Vec2& value)
{
    const DReal magnitude = norm(value);
    return dommy::math::normalized2OrZero(value[0], value[1], value[0], value[1]) ? magnitude : 0.0f;
}
inline DReal unitize(Vec3& value)
{
    const DReal magnitude = norm(value);
    return dommy::math::normalized3OrZero(value[0], value[1], value[2],
        value[0], value[1], value[2]) ? magnitude : 0.0f;
}
inline DReal unitize(Vec4& value)
{
    const double magnitude = detail::vectorLengthWide(value.data);
    if (!std::isfinite(magnitude) || magnitude == 0.0) {
        value = 0.0f;
        return 0.0f;
    }
    for (int axis = 0; axis != 4; ++axis)
        value[axis] = static_cast<DReal>(static_cast<double>(value[axis]) / magnitude);
    return detail::narrowLength(magnitude);
}
inline DReal normalize(Vec2& value) { return unitize(value); }
inline DReal normalize(Vec3& value) { return unitize(value); }
inline DReal normalize(Vec4& value) { return unitize(value); }

inline Vec2 Unit(const Vec2& value)
{
    Vec2 result;
    dommy::math::normalized2OrZero(value[0], value[1], result[0], result[1]);
    return result;
}
inline Vec3 Unit(const Vec3& value)
{
    Vec3 result;
    dommy::math::normalized3OrZero(value[0], value[1], value[2], result[0], result[1], result[2]);
    return result;
}
inline Vec2 Unit(const DReal& x, const DReal& y) { return Unit(Vec2(x, y)); }
inline Vec3 Unit(const DReal& x, const DReal& y, const DReal& z) { return Unit(Vec3(x, y, z)); }

inline Vec2 remCompB(const Vec2& value, const Vec2& direction)
{
    Vec2 result;
    if (!detail::removeParallelComponent(value.data, direction.data, result.data))
        throw std::domain_error("Vector projection requires finite, representable components");
    return result;
}
inline Vec3 remCompB(const Vec3& value, const Vec3& direction)
{
    Vec3 result;
    if (!detail::removeParallelComponent(value.data, direction.data, result.data))
        throw std::domain_error("Vector projection requires finite, representable components");
    return result;
}
inline void remCompB2(Vec2& value, const Vec2& direction) { value = remCompB(value, direction); }
inline void remCompB2(Vec3& value, const Vec3& direction) { value = remCompB(value, direction); }

inline Vec3 proj(const Vec4& value)
{
    Vec3 result(value[0], value[1], value[2]);
    const DReal w = value[3];
    if (w != 1.0 && !(std::fabs(static_cast<double>(w)) < VEC_RX_FEQ_EPS)) result /= w;
    return result;
}
inline void vec4_to_array(const Vec4& value, DReal* destination)
{
    for (int axis = 0; axis != 4; ++axis) destination[axis] = value[axis];
}

} // namespace DTools
#endif // DVec_h
