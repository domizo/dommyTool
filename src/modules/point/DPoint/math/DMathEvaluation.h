#pragma once

#include "DNumeric.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include <type_traits>

// Float-backed procedural attributes retain their representation. Wider
// floating inputs stay wide; integer arguments are promoted before arithmetic.
template <typename T>
using DMathScalar = typename std::conditional<std::is_floating_point<T>::value, T, double>::type;
template <typename T>
DMathScalar<T> DAdd(const T& a, const T& b) { return DMathScalar<T>(a) + DMathScalar<T>(b); }
template <typename T>
DMathScalar<T> DSub(const T& a, const T& b) { return DMathScalar<T>(a) - DMathScalar<T>(b); }
template <typename T>
DMathScalar<T> DMult(const T& a, const T& b) { return DMathScalar<T>(a) * DMathScalar<T>(b); }
template <typename T>
DMathScalar<T> DDiv(const T& a, const T& b) { return dommy::math::divideOrZero(DMathScalar<T>(a), DMathScalar<T>(b)); }
template <typename T>
DMathScalar<T> DMod(const T& a, const T& b) { return dommy::math::moduloOrZero(DMathScalar<T>(a), DMathScalar<T>(b)); }
template <typename T>
DMathScalar<T> DPow(const T& a, const T& b) { return dommy::math::powerOrZero(DMathScalar<T>(a), DMathScalar<T>(b)); }
template <typename T>
DMathScalar<T> DMin(const T& a, const T& b) { return std::fmin(DMathScalar<T>(a), DMathScalar<T>(b)); }
template <typename T>
DMathScalar<T> DMax(const T& a, const T& b) { return std::fmax(DMathScalar<T>(a), DMathScalar<T>(b)); }
template <typename T>
DMathScalar<T> DNegative(T a) { return -DMathScalar<T>(a); }
template <typename T>
DMathScalar<T> DSqrt(T a) { return dommy::math::sqrtOrZero(DMathScalar<T>(a)); }
template <typename T>
DMathScalar<T> DFloor(T a) { return std::floor(DMathScalar<T>(a)); }
template <typename T>
DMathScalar<T> DRound(T a) { return std::round(DMathScalar<T>(a)); }
template <typename T>
DMathScalar<T> DCeil(T a) { return std::ceil(DMathScalar<T>(a)); }
template <typename T>
DMathScalar<T> DExponential(T a) { return dommy::math::exponentialOrZero(DMathScalar<T>(a)); }
template <typename T>
DMathScalar<T> DAbsolute(T a) { return std::fabs(DMathScalar<T>(a)); }

// Values are part of existing node operation contracts; never reorder.
enum {
    D_SRC_NEGATIVE = 0, D_SRC_SQRT, D_SRC_FLOOR, D_SRC_ROUND,
    D_SRC_CEIL, D_SRC_EXPONENTIAL, D_SRC_ABS, D_SRC_COUNT
};
typedef int D_SRC_SINGLE_MATH_ENUM;
enum {
    D_CAL_ADD = 0, D_CAL_SUB, D_CAL_MULT, D_CAL_DIV, D_CAL_MOD,
    D_CAL_POW, D_CAL_MIN, D_CAL_MAX, D_CAL_COUNT
};
typedef int D_CALC_MATH_ENUM;

namespace dommy {
namespace math {

// Calculation and operand staging budget, excluding Maya-owned input objects
// and the separately validated procedural-data copy. Account conservatively
// for three double components, simultaneous flat/per-primitive staging,
// and the conversion/copy buffers used when publishing typed attributes.
inline bool validMathBudget(std::size_t points, std::size_t operands,
                            std::size_t partitions = 1U)
{
    const std::size_t maxBytes = 256U * 1024U * 1024U;
    std::size_t count = 0U, work = 0U, bytes = 0U, containers = 0U;
    return points <= generation::kMaxGeneratedPointCount &&
        operands <= generation::kMaxGeneratedPointCount &&
        generation::checkedSum(operands, 1U, generation::kMaxWorkItemCount, count) &&
        generation::checkedProduct(std::max(points, std::size_t(1)), count,
                                    generation::kMaxWorkItemCount / 3U, work) &&
        generation::checkedProduct(work, 96U, maxBytes, bytes) &&
        generation::checkedProduct(std::max(partitions, std::size_t(1)), count,
                                    generation::kMaxWorkItemCount, containers) &&
        generation::checkedProduct(containers, 96U, maxBytes, containers) &&
        generation::checkedSum(bytes, containers, maxBytes, bytes);
}

inline bool checkedMathBinary(int operation, double a, double b, double& result)
{
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    double candidate = 0.0;
    switch (operation) {
    case D_CAL_ADD: candidate = a + b; break;
    case D_CAL_SUB: candidate = a - b; break;
    case D_CAL_MULT: candidate = a * b; break;
    // Retain the established zero fallback for undefined divide/mod/pow.
    case D_CAL_DIV: candidate = divideOrZero(a, b); break;
    case D_CAL_MOD: candidate = moduloOrZero(a, b); break;
    case D_CAL_POW: candidate = powerOrZero(a, b); break;
    case D_CAL_MIN: candidate = std::fmin(a, b); break;
    case D_CAL_MAX: candidate = std::fmax(a, b); break;
    default: return false;
    }
    return numeric::checkedNumericCast(candidate, result);
}

inline bool checkedMathUnary(int operation, double value, double& result)
{
    if (!std::isfinite(value)) return false;
    double candidate = 0.0;
    switch (operation) {
    case D_SRC_NEGATIVE: candidate = -value; break;
    case D_SRC_SQRT: candidate = sqrtOrZero(value); break;
    case D_SRC_FLOOR: candidate = std::floor(value); break;
    case D_SRC_ROUND: candidate = std::round(value); break;
    case D_SRC_CEIL: candidate = std::ceil(value); break;
    case D_SRC_EXPONENTIAL: candidate = exponentialOrZero(value); break;
    case D_SRC_ABS: candidate = std::fabs(value); break;
    default: return false;
    }
    return numeric::checkedNumericCast(candidate, result);
}

// Internally, a one-element operand represents a broadcast constant. Maya
// array inputs are required to match the source count before reaching here.
template <typename Operand>
const typename Operand::value_type& mathOperandAt(const Operand& values,
                                                 std::size_t index)
{
    return values[values.size() == 1U ? 0U : index];
}

// Result is a caller-owned staging buffer: failures must not publish it.
template <typename Result, typename Operand, typename Operation>
bool applyMathOperand(Result& result, const Operand& operand, Operation operation)
{
    if (operand.size() != 1U && operand.size() != result.size()) return false;
    for (std::size_t i = 0U; i < result.size(); ++i) {
        if (!operation(result[i], mathOperandAt(operand, i))) return false;
    }
    return true;
}

} // namespace math
} // namespace dommy
