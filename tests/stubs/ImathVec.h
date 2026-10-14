#pragma once

#include <cmath>
#include <cstddef>

namespace Imath {

template <typename T>
class Vec2
{
public:
    using BaseType = T;
#ifdef DOMMY_POISON_IMATH_DEFAULT
    // Optional test sentinel: real legacy Imath leaves default components
    // uninitialized. Do not let the normal zero-filled stub mask that mistake.
    Vec2() { values_[0] = values_[1] = T(37); }
    Vec2(T x, T y = T()) {
#else
    Vec2(T x = T(), T y = T()) {
#endif
        values_[0] = x;
        values_[1] = y;
    }

    template <typename U>
    explicit Vec2(const Vec2<U>& other) {
        values_[0] = static_cast<T>(other[0]);
        values_[1] = static_cast<T>(other[1]);
    }

    T& operator[](std::size_t index) { return values_[index]; }
    const T& operator[](std::size_t index) const { return values_[index]; }

private:
    T values_[2];
};

template <typename T>
class Vec3
{
public:
    using BaseType = T;
#ifdef DOMMY_POISON_IMATH_DEFAULT
    Vec3() : x(T(37)), y(T(37)), z(T(37)) {}
    Vec3(T xValue, T yValue = T(), T zValue = T())
#else
    Vec3(T xValue = T(), T yValue = T(), T zValue = T())
#endif
        : x(xValue), y(yValue), z(zValue) {}

    template <typename U>
    explicit Vec3(const Vec3<U>& other)
        : x(static_cast<T>(other[0]))
        , y(static_cast<T>(other[1]))
        , z(static_cast<T>(other[2])) {}

    T& operator[](std::size_t index) { return (&x)[index]; }
    const T& operator[](std::size_t index) const { return (&x)[index]; }

    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator*(T scale) const {
        return Vec3(x * scale, y * scale, z * scale);
    }

    Vec3 operator/(T scale) const {
        return Vec3(x / scale, y / scale, z / scale);
    }

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vec3& operator*=(T scale) {
        x *= scale;
        y *= scale;
        z *= scale;
        return *this;
    }

    T dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x);
    }

    T length() const {
        return static_cast<T>(std::sqrt(dot(*this)));
    }

    const Vec3& normalize() {
        const T vectorLength = length();
        if (vectorLength != T()) {
            const T inverseLength = T(1) / vectorLength;
            *this *= inverseLength;
        }
        return *this;
    }

    Vec3 normalized() const {
        Vec3 result(*this);
        result.normalize();
        return result;
    }

    T x;
    T y;
    T z;
};

template <typename T>
class Vec4
{
public:
    Vec4(
        T xValue = T(),
        T yValue = T(),
        T zValue = T(),
        T wValue = T())
        : x(xValue), y(yValue), z(zValue), w(wValue) {}

    T& operator[](std::size_t index) { return (&x)[index]; }
    const T& operator[](std::size_t index) const { return (&x)[index]; }

    T x;
    T y;
    T z;
    T w;
};

typedef Vec2<int> V2i;
typedef Vec2<float> V2f;
typedef Vec2<double> V2d;
typedef Vec3<int> V3i;
typedef Vec3<float> V3f;
typedef Vec3<double> V3d;
typedef Vec4<int> V4i;
typedef Vec4<float> V4f;
typedef Vec4<double> V4d;

}  // namespace Imath
