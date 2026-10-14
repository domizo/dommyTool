#pragma once
#ifndef DUtility_h
#define DUtility_h

// Project-owned compatibility facade. Checked interpolation, conversions and
// accumulators reuse the shared project implementations; remaining helpers are
// independently defined from their public API and numerical contracts.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "DVec.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DIntegerBlend.h"
#include "common/utility/DProductSum.h"
#include "math/DLinearInterpolation.h"
#include "math/DRemapMath.h"

const double D_M_PI = 3.14159265358979323846;
const double D_PI = 3.14159265358979323846;
const double D_V_PI = 4.18879020478639098462;
const double D_ROOT2 = 1.41421356237309504880;
const double D_ROOT3 = 1.73205080756887729353;
const double D_ROOT5 = 2.23606797749978969641;
const double D_FEQ_EPS = 1.0e-10;
const double D_EPS = 1.0e-8;
const double D_FEQ_INF = 1.0e10;
const double D_FEQ_INFM = 0.999e10;
const float D_MAX_FLOAT = std::numeric_limits<float>::max();
const double D_INV_PI = 0.31830988618379067154;
const double D_SQRT3 = D_ROOT3;
const double D_DEGREES_TO_RADIANS = 0.0174532925199432957692369076848;
const double D_RADIANS_TO_DEGREES = 57.295779513082320876798154814114;

namespace dommy {
namespace utility {
namespace detail {

template <typename T> struct InterpolationVectorSize : std::integral_constant<unsigned int, 0U> {};
template <> struct InterpolationVectorSize<DTools::Vec2> : std::integral_constant<unsigned int, 2U> {};
template <> struct InterpolationVectorSize<DTools::Vec3> : std::integral_constant<unsigned int, 3U> {};
template <> struct InterpolationVectorSize<DTools::Vec4> : std::integral_constant<unsigned int, 4U> {};

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, bool>::type
approximatelyEqual(T a, T b, double epsilon)
{
    static_assert(std::numeric_limits<T>::digits <= std::numeric_limits<std::uintmax_t>::digits,
                  "Integer comparison requires a standard-width scalar");
    if (!(epsilon > 0.0)) return false;
    // Unsigned subtraction is exact even when signed endpoints span the full
    // integer domain. Do not round int64/uint64 values through a floating type.
    const std::uintmax_t difference = a >= b
        ? static_cast<std::uintmax_t>(a) - static_cast<std::uintmax_t>(b)
        : static_cast<std::uintmax_t>(b) - static_cast<std::uintmax_t>(a);
    if (!std::isfinite(epsilon)) return true; // Positive infinity only.
    std::uintmax_t threshold = 0U;
    if (!numeric::checkedNumericCast(std::ceil(epsilon), threshold)) return true;
    return difference < threshold;
}

template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, bool>::type
approximatelyEqual(T a, T b, double epsilon)
{
    // Preserve the historical floating subtraction/strict-epsilon behavior,
    // including signed zero, NaN and infinity handling.
    return std::fabs(a - b) < epsilon;
}

template <typename T>
inline T negateChecked(T value)
{
    static_assert(std::is_arithmetic<T>::value, "Scalar negation requires arithmetic scalars");
    if (std::is_integral<T>::value && std::is_signed<T>::value &&
        value == std::numeric_limits<T>::min())
        throw std::overflow_error("Scalar negation is not representable");
    return static_cast<T>(-value);
}

template <typename T>
inline T convertAngleChecked(T value, double factor)
{
    static_assert(std::is_arithmetic<T>::value && !std::is_same<T, bool>::value,
                  "Angle conversion requires non-bool arithmetic scalars");
    if (!std::isfinite(static_cast<long double>(value)))
        throw std::domain_error("Angle input must be finite");
    T result = T(0);
    // Keep the existing multiplication precision and integral truncation, but
    // reject nonrepresentable output before performing its narrowing cast.
    if (!numeric::checkedNumericCast(value * factor, result))
        throw std::overflow_error("Angle output is not representable");
    return result;
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, std::array<double, 2>>::type
interpolationParts(T value)
{
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "Interpolation requires standard-width integers");
    const std::uint64_t magnitude = attribute::detail::integerMagnitude(value);
    const double sign = std::is_signed<T>::value && value < 0 ? -1.0 : 1.0;
    return {{sign * std::ldexp(static_cast<double>(magnitude >> 32U), 32),
             sign * static_cast<double>(static_cast<std::uint32_t>(magnitude))}};
}

template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, std::array<double, 2>>::type
interpolationParts(T value)
{
    static_assert(std::numeric_limits<T>::digits <= 53, "Use extended interpolation for wider floating types");
    return {{static_cast<double>(value), 0.0}};
}

template <typename T, typename Weight, typename Result>
inline typename std::enable_if<std::is_integral<Result>::value, bool>::type
interpolateChecked(T source, T target, Weight weight, Result& result)
{
    return attribute::checkedIntegerBlend(source, target, weight, result);
}

template <typename T, typename Weight, typename Result>
inline typename std::enable_if<std::is_floating_point<Result>::value &&
    std::is_floating_point<T>::value && std::is_floating_point<Weight>::value, bool>::type
interpolateChecked(T source, T target, Weight weight, Result& result)
{
    return math::checkedLinearInterpolation(static_cast<Result>(source), static_cast<Result>(target), weight, result);
}

template <typename T, typename Weight, typename Result>
inline typename std::enable_if<std::is_floating_point<Result>::value &&
    (!std::is_floating_point<T>::value || !std::is_floating_point<Weight>::value) &&
    (std::numeric_limits<Result>::digits <= 53), bool>::type
interpolateChecked(T source, T target, Weight weight, Result& result)
{
    if (weight == 0) { result = static_cast<Result>(source); return true; }
    if (weight == 1) { result = static_cast<Result>(target); return true; }
    if (source == target) { result = static_cast<Result>(source); return true; }
    // Split integral operands into exact 32-bit pieces. Casting a 64-bit
    // endpoint/weight to double first loses low bits before cancellation.
    const auto first = interpolationParts(source), last = interpolationParts(target);
    const auto weights = interpolationParts(weight);
    numeric::detail::BinaryAccumulator<200U, -3222> sum;
    for (double part : first) if (!sum.add(part)) return false;
    for (unsigned int i = 0U; i < 2U; ++i)
        for (unsigned int j = 0U; j < 2U; ++j)
            if (!sum.addProduct(last[i], weights[j]) || !sum.addProduct(-first[i], weights[j])) return false;
    return sum.mean(1U, result);
}

template <typename T, typename Weight, typename Result>
inline typename std::enable_if<std::is_floating_point<Result>::value &&
    (!std::is_floating_point<T>::value || !std::is_floating_point<Weight>::value) &&
    (std::numeric_limits<Result>::digits > 53), bool>::type
interpolateChecked(T source, T target, Weight weight, Result& result)
{
    static_assert(!std::is_integral<T>::value || std::numeric_limits<T>::digits <= std::numeric_limits<Result>::digits,
                  "Extended interpolation must represent integral endpoints exactly");
    static_assert(!std::is_integral<Weight>::value || std::numeric_limits<Weight>::digits <= std::numeric_limits<Result>::digits,
                  "Extended interpolation must represent integral weights exactly");
    return math::checkedLinearInterpolation(static_cast<Result>(source), static_cast<Result>(target),
                                           static_cast<Result>(weight), result);
}

template <typename T, typename Weight>
inline typename std::enable_if<(InterpolationVectorSize<T>::value > 0U), bool>::type
interpolateChecked(const T& source, const T& target, Weight weight, T& result)
{
    T candidate;
    for (unsigned int axis = 0U; axis < InterpolationVectorSize<T>::value; ++axis)
        if (!interpolateChecked(source[axis], target[axis], weight, candidate[axis])) return false;
    result = candidate;
    return true;
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
finiteInterpolationInput(const T& value)
{
    return std::isfinite(static_cast<long double>(value));
}

template <typename T>
inline typename std::enable_if<(InterpolationVectorSize<T>::value > 0U), bool>::type
finiteInterpolationInput(const T& value)
{
    for (unsigned int axis = 0U; axis < InterpolationVectorSize<T>::value; ++axis)
        if (!std::isfinite(value[axis])) return false;
    return true;
}

template <typename T, typename Weight>
inline auto legacyInterpolation(const T& a, const T& b, const Weight& t, std::true_type)
-> decltype(a + t*(b-a))
{
    if (!finiteInterpolationInput(a) || !finiteInterpolationInput(b) || !finiteInterpolationInput(t))
        throw std::domain_error("Interpolation input must be finite");
    decltype(a + t*(b-a)) result{};
    if (!interpolateChecked(a, b, t, result))
        throw std::overflow_error("Interpolation output is not representable");
    return result;
}

template <typename T, typename Weight>
inline auto legacyInterpolation(const T& a, const T& b, const Weight& t, std::false_type)
-> decltype(a + t*(b-a))
{
    // Custom arithmetic types retain their own operator contract. Do not
    // reject historically valid non-scalar callers or assume their layout.
    return a + t*(b-a);
}

} // namespace detail
} // namespace utility
} // namespace dommy


template<class T>
inline bool D_IS_ZERO(const T &x)
{
    static_assert(std::is_arithmetic<T>::value, "Zero comparison requires arithmetic scalars");
    return dommy::utility::detail::approximatelyEqual(x, T(0), D_FEQ_EPS);
}

template<class T>
inline bool D_FEQ(const T &a, const T &b)
{
    static_assert(std::is_arithmetic<T>::value, "Equality comparison requires arithmetic scalars");
    return dommy::utility::detail::approximatelyEqual(a, b, D_FEQ_EPS);
}

template<class T>
inline bool D_VEC3_FEQ(const T &a, const T &b, double eps = D_FEQ_EPS)
{
    return dommy::utility::detail::approximatelyEqual(a[0], b[0], eps) &&
           dommy::utility::detail::approximatelyEqual(a[1], b[1], eps) &&
           dommy::utility::detail::approximatelyEqual(a[2], b[2], eps);
}

template<class T>
inline T D_TO_RADIANS(const T &x)
{
    return dommy::utility::detail::convertAngleChecked(x, D_DEGREES_TO_RADIANS);
}

template<class T>
inline T D_TO_DEGREES(const T &x)
{
    return dommy::utility::detail::convertAngleChecked(x, D_RADIANS_TO_DEGREES);
}

template<class T, class Weight>
inline auto D_LERP(const T &a, const T &b, const Weight &t)
-> decltype(a + t*(b-a))
{
    using Checked = std::integral_constant<bool, std::is_arithmetic<Weight>::value &&
        (std::is_arithmetic<T>::value || dommy::utility::detail::InterpolationVectorSize<T>::value > 0U)>;
    return dommy::utility::detail::legacyInterpolation(a, b, t, Checked{});
}

template<class T>
inline T D_SIGN2(const T &a, const T &b)
{
    static_assert(std::is_arithmetic<T>::value, "Sign selection requires arithmetic scalars");
    return b >= T(0)
        ? (a >= T(0) ? a : dommy::utility::detail::negateChecked(a))
        : (a >= T(0) ? dommy::utility::detail::negateChecked(a) : a);
}

template<class T>
inline T D_ABS(const T &x)
{
    static_assert(std::is_arithmetic<T>::value, "Absolute value requires arithmetic scalars");
    return x < T(0) ? dommy::utility::detail::negateChecked(x) : x;
}

inline std::mt19937& D_RANDOM_ENGINE() {
	static thread_local std::mt19937 engine(5489U);
	return engine;
}

inline double D_FRAND(){
	return std::uniform_real_distribution<double>(0.0, 1.0)(D_RANDOM_ENGINE());
}

template<typename T>
inline std::string D_TO_STRING(const T &x)
{
	std::stringstream ss;
	ss << x;
	return dommy::text::checkedStreamText(ss, "String conversion failed");
}

template<class T>
inline T D_MAX(const T& a, const T& b)
{
    if (a > b) return a;
    return b;
}
template<class T>
inline T D_MIN(const T& a, const T& b)
{
    if (a < b) return a;
    return b;
}
template<class T>
inline T D_MAX3(const T& a, const T& b, const T& c)
{
    if (a > b) return D_MAX(a, c);
    return D_MAX(b, c);
}
template<class T>
inline T D_MIN3(const T& a, const T& b, const T& c)
{
    if (a < b) return D_MIN(a, c);
    return D_MIN(b, c);
}
template<class T>
inline T D_CLAMP(const T& value, const T& lower, const T& upper)
{
    // Preserve strict comparisons, endpoint ties, NaNs and reversed bounds.
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}
template<class T>
inline void D_TOGGLE(T& flag, int requested = -1)
{
    const int next = requested == -1 ? int(!flag) : int(requested != 0);
    flag = next;
}
template<class T>
inline void D_SWAP(T& left, T& right)
{
    // Default construction and copy assignment are the legacy type contract.
    T saved;
    saved = left;
    left = right;
    right = saved;
}
template<class T>
inline T D_SIGN(const T& value)
{
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

// Preserve the loop-variable names, bounds and nesting exposed by these macros.
#ifndef DFOR3
#define DFOR3(i0, i1, j0, j1, k0, k1) for(int i = i0; i < i1; ++i) \
for(int j = j0; j < j1; ++j) \
for(int k = k0; k < k1; ++k)
#endif
#ifndef DFOR3E
#define DFOR3E(i0, i1, j0, j1, k0, k1) for(int i = i0; i <= i1; ++i) \
for(int j = j0; j < j1; ++j) \
for(int k = k0; k <= k1; ++k)
#endif
#ifndef DFOR2
#define DFOR2(i0, i1, j0, j1) for(int i = i0; i < i1; ++i) \
for(int j = j0; j < j1; ++j)
#endif
#ifndef DFOR2E
#define DFOR2E(i0, i1, j0, j1) for(int i = i0; i <= i1; ++i) \
for(int j = j0; j <= j1; ++j)
#endif

namespace DFunc {
inline std::string GenSpace(int count)
{
    return count > 0 ? std::string(static_cast<std::size_t>(count), ' ') : std::string();
}
inline bool IsPositive(const DTools::Vec3& value)
{
    if (!dommy::utility::detail::finiteInterpolationInput(value)) return false;
    for (int axis = 0; axis != 3; ++axis)
        if (value[axis] < -D_FEQ_EPS) return false;
    return true;
}
inline bool InRange(const DTools::Vec2& point, const DTools::Vec2& lower, const DTools::Vec2& upper)
{
    if (!dommy::utility::detail::finiteInterpolationInput(point) ||
        !dommy::utility::detail::finiteInterpolationInput(lower) ||
        !dommy::utility::detail::finiteInterpolationInput(upper)) return false;
    for (int axis = 0; axis != 2; ++axis)
        if (point[axis] < lower[axis] || point[axis] > upper[axis]) return false;
    return true;
}
inline double Min3(const DTools::Vec3& value) { return D_MIN3(value[0], value[1], value[2]); }
inline double Max3(const DTools::Vec3& value) { return D_MAX3(value[0], value[1], value[2]); }
inline double Min2(const DTools::Vec2& value) { return D_MIN(value[0], value[1]); }
inline double Max2(const DTools::Vec2& value) { return D_MAX(value[0], value[1]); }
inline double Frand() { return D_FRAND(); }
inline int Nrand(int count) { return static_cast<int>(Frand() * count); }
inline double SignedRand() { return 2 * Frand() - 1; }
inline char ByteRand()
{
    return static_cast<char>(std::uniform_int_distribution<unsigned int>(0U, 255U)(D_RANDOM_ENGINE()));
}

inline double Rand(const double &_max, const double &_min)
	{
        if (!std::isfinite(_max) || !std::isfinite(_min))
            throw std::domain_error("Random bounds must be finite");
        return D_LERP(_min, _max, Frand());
	}

inline DTools::Vec2 Rand(const DTools::Vec2 &_max, const DTools::Vec2 &_min)
	{
        if (!dommy::utility::detail::finiteInterpolationInput(_max) ||
            !dommy::utility::detail::finiteInterpolationInput(_min))
            throw std::domain_error("Random bounds must be finite");
        // Keep constructor argument evaluation structure and one draw per axis.
        // Axis draw order is historically unspecified in C++11.
        return DTools::Vec2(static_cast<DReal>(Rand(double(_max[0]), double(_min[0]))),
                           static_cast<DReal>(Rand(double(_max[1]), double(_min[1]))));
	}

inline DTools::Vec3 Rand(const DTools::Vec3 &_max, const DTools::Vec3 &_min)
	{
        if (!dommy::utility::detail::finiteInterpolationInput(_max) ||
            !dommy::utility::detail::finiteInterpolationInput(_min))
            throw std::domain_error("Random bounds must be finite");
        return DTools::Vec3(static_cast<DReal>(Rand(double(_max[0]), double(_min[0]))),
                           static_cast<DReal>(Rand(double(_max[1]), double(_min[1]))),
                           static_cast<DReal>(Rand(double(_max[2]), double(_min[2]))));
	}

namespace detail {
inline double polarNormalSample()
{
    // Preserve two draws per attempt, their order, and the rejection boundary.
    for (;;) {
        const double horizontal = 2.0 * Frand() - 1.0;
        const double vertical = 2.0 * Frand() - 1.0;
        const double radiusSquare = horizontal * horizontal + vertical * vertical;
        if (radiusSquare >= 1.0 || radiusSquare < 1E-30) continue;
        const double radialScale = std::sqrt((-2.0 * std::log(radiusSquare)) / radiusSquare);
        return horizontal * radialScale;
    }
}
} // namespace detail

inline double NormalRand(double m, double s)
{
        if (!std::isfinite(m) || !std::isfinite(s))
            throw std::domain_error("Normal random parameters must be finite");
        const double x1 = detail::polarNormalSample();
        const std::array<double, 2> factors{{x1, 1.0}}, values{{s, m}};
        double result = 0.0;
        if (!dommy::numeric::checkedProductSum(factors, values, result))
            throw std::overflow_error("Normal random output is not representable");
        return result;
}
inline double Sqrt(const double& value)
{
    return value > 0.0 ? std::sqrt(value) : 0.0;
}
inline DTools::Vec2 Fabs(const DTools::Vec2& value)
{
    return DTools::detail::mapComponents(value, [](DReal lane) {
        return static_cast<DReal>(std::fabs(static_cast<double>(lane)));
    });
}
inline DTools::Vec3 Fabs(const DTools::Vec3& value)
{
    return DTools::detail::mapComponents(value, [](DReal lane) {
        return static_cast<DReal>(std::fabs(static_cast<double>(lane)));
    });
}
inline double Max3Abs(const DTools::Vec3& value) { return Max3(Fabs(value)); }
inline double Max3Abs(const std::vector<DTools::Vec3>& values)
{
    double maximum = 0.0;
    for (const auto& value : values) {
        const double candidate = Max3Abs(value);
        if (candidate > maximum) maximum = candidate;
    }
    return maximum;
}
inline DTools::Vec3 Clamp(const DTools::Vec3& value, const DTools::Vec3& lower, const DTools::Vec3& upper)
{
    DTools::Vec3 result;
    for (int axis = 0; axis != 3; ++axis) result[axis] = D_CLAMP(value[axis], lower[axis], upper[axis]);
    return result;
}
inline bool IsZeroVec(const DTools::Vec3& value)
{
    for (int axis = 0; axis != 3; ++axis) if (!D_IS_ZERO(value[axis])) return false;
    return true;
}
inline bool NonZeroVec(const DTools::Vec3& value) { return !IsZeroVec(value); }

inline double AverageVec(DTools::Vec3 A)
	{
        if (!dommy::utility::detail::finiteInterpolationInput(A))
            throw std::domain_error("Average input must be finite");
        dommy::numeric::detail::PointMeanAccumulator sum;
        double result = 0.0;
        if (!sum.add(A[0]) || !sum.add(A[1]) || !sum.add(A[2]) ||
            !sum.mean(3U, result))
            throw std::overflow_error("Average output is not representable");
        return result;
	}

inline void Gradation(double col[3], const double col1[3], const double col2[3],
						  double x, const double xmin = 0.0, const double xmax = 1.0)
	{
        if (!col || !col1 || !col2) throw std::invalid_argument("Gradation requires three-component buffers");
        if (!std::isfinite(xmin) || !std::isfinite(xmax)) throw std::domain_error("Gradation range must be finite");
        double span = 0.0;
        int exponent = 0, thresholdExponent = 0;
        dommy::math::detail::scaledRangeDifference(xmax, xmin, span, exponent);
        const double threshold = std::frexp(1.0e-10, &thresholdExponent);
        // Preserve the legacy collapsed/near-zero-range no-op, including
        // reversed ranges, without forming an overflowing span.
        if (span == 0.0 || exponent < thresholdExponent ||
            (exponent == thresholdExponent && std::fabs(span) < threshold)) return;
        if (!std::isfinite(x)) throw std::domain_error("Gradation input must be finite");
        double dx = 0.0;
        if ((xmin < xmax && x <= xmin) || (xmin > xmax && x >= xmin)) dx = 0.0;
        else if ((xmin < xmax && x >= xmax) || (xmin > xmax && x <= xmax)) dx = 1.0;
        else if (!dommy::math::normalizeRange(x, xmin, xmax, dx))
            throw std::overflow_error("Gradation position is not representable");
        dx = D_CLAMP(dx, 0.0, 1.0);
        std::array<double, 3> candidate;
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            candidate[axis] = D_LERP(col1[axis], col2[axis], dx);
        std::copy(candidate.begin(), candidate.end(), col);
	}
} // namespace DFunc
#endif // DUtility_h
