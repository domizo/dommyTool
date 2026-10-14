#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "math/DRemapMath.h"
#include <openvdb/math/Vec3.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace dommy {
namespace vdb {

inline bool validPruneSettings(short mode, double tolerance) {
    return mode >= 0 && mode <= 2 && std::isfinite(tolerance) && tolerance >= 0 && tolerance <= 1;
}

template <typename T> struct ValueComponents {
    static const std::size_t count = 1;
    static bool finite(T value) {
        return std::isfinite(static_cast<long double>(value));
    }
    static bool negatable(T value) {
        return !std::is_integral<T>::value || !std::is_signed<T>::value ||
               value != std::numeric_limits<T>::min();
    }
    static bool tolerance(double value, T& result) {
        return numeric::checkedNumericCast(value, result);
    }
    static bool matchesBackground(T a, T b, T delta) {
        // Native 5.2 isApproxEqual subtracts signed integers before abs().
        return std::is_integral<T>::value ? a == b
                                          : std::fabs(static_cast<long double>(a) - b) <= delta;
    }
};
template <typename T> struct ValueComponents<openvdb::math::Vec3<T>> {
    static const std::size_t count = 3;
    static bool finite(const openvdb::math::Vec3<T>& value) {
        return ValueComponents<T>::finite(value[0]) && ValueComponents<T>::finite(value[1]) &&
               ValueComponents<T>::finite(value[2]);
    }
    static bool negatable(const openvdb::math::Vec3<T>& value) {
        return ValueComponents<T>::negatable(value[0]) && ValueComponents<T>::negatable(value[1]) &&
               ValueComponents<T>::negatable(value[2]);
    }
    static bool tolerance(double value, openvdb::math::Vec3<T>& result) {
        T scalar;
        if (!ValueComponents<T>::tolerance(value, scalar))
            return false;
        result = openvdb::math::Vec3<T>(scalar);
        return true;
    }
    static bool matchesBackground(const openvdb::math::Vec3<T>& a, const openvdb::math::Vec3<T>& b,
                                  const openvdb::math::Vec3<T>& delta) {
        return ValueComponents<T>::matchesBackground(a[0], b[0], delta[0]) &&
               ValueComponents<T>::matchesBackground(a[1], b[1], delta[1]) &&
               ValueComponents<T>::matchesBackground(a[2], b[2], delta[2]);
    }
};

template <typename T, bool Integral = std::is_integral<T>::value && std::is_signed<T>::value>
struct PruneRange {
    bool add(const T&) {
        return true;
    }
};
template <typename T> struct PruneRange<T, true> {
    T low = std::numeric_limits<T>::max(), high = std::numeric_limits<T>::min();
    bool add(T value) {
        if (value < low)
            low = value;
        if (value > high)
            high = value;
        // Native isConstant computes max-min in ValueType. No signed
        // subtraction or floating-point rounding is used by this guard.
        return low >= 0 || high < 0 || high <= std::numeric_limits<T>::max() + low;
    }
};
template <typename T> struct PruneRange<openvdb::math::Vec3<T>, false> {
    PruneRange<T> components[3];
    bool add(const openvdb::math::Vec3<T>& value) {
        return components[0].add(value[0]) && components[1].add(value[1]) &&
               components[2].add(value[2]);
    }
};

// Charge stored values (including inactive voxels and tiles), not expanded
// active voxel counts. This admission budget is not a native memory/time cap.
struct ValuePreflight {
    std::size_t& work;
    std::size_t passes;
    bool valid = true;
    ValuePreflight(std::size_t& total, std::size_t passCount) : work(total), passes(passCount) {}
    template <typename GridT> void operator()(const GridT& grid) {
        using Components = ValueComponents<typename GridT::ValueType>;
        const std::size_t cost = Components::count * passes;
        valid = Components::finite(grid.background()) &&
                generation::checkedSum(work, cost, generation::kMaxWorkItemCount, work);
        for (auto it = grid.cbeginValueAll(); valid && it; ++it) {
            valid = Components::finite(*it) &&
                    generation::checkedSum(work, cost, generation::kMaxWorkItemCount, work);
        }
    }
};

// sdfInteriorMask expects a finite, positive exterior for a level set.
// Dispatch only scalar grids to this predicate.
struct PositiveMaskBackground {
    bool valid = false;
    template <typename GridT> void operator()(const GridT& grid) {
        valid = ValueComponents<typename GridT::ValueType>::finite(grid.background()) &&
                grid.background() > 0;
    }
};

inline bool validRemapSettings(short below, short above, float inMin, float inMax, float outMin,
                               float outMax) {
    return below >= 0 && below <= 2 && above >= 0 && above <= 2 && std::isfinite(inMin) &&
           std::isfinite(inMax) && inMin != inMax && std::isfinite(outMin) && std::isfinite(outMax);
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, int>::type compareRemapBound(T value,
                                                                                  float bound) {
    return value < bound ? -1 : (value > bound ? 1 : 0);
}
template <typename T>
typename std::enable_if<std::is_integral<T>::value, int>::type compareRemapBound(T value,
                                                                                 float bound) {
    // Avoid rounding INT64_MAX to 2^63 on platforms where long double == double.
    T integer;
    if (!numeric::checkedNumericCast(bound, integer))
        return bound > 0 ? -1 : 1;
    if (value != integer)
        return value < integer ? -1 : 1;
    const long double fraction = static_cast<long double>(bound) - integer;
    return fraction > 0 ? -1 : (fraction < 0 ? 1 : 0);
}

template <typename T, typename Lookup>
T remapValue(T source, const Lookup& ramp, short belowMode, short aboveMode, float inMin,
             float inMax, float outMin, float outMax) {
    if (!ValueComponents<T>::finite(source))
        throw std::domain_error("VDB remap received a non-finite value");
    const bool reversed = inMax < inMin;
    const bool below =
        reversed ? compareRemapBound(source, inMin) > 0 : compareRemapBound(source, inMin) < 0;
    const bool above =
        reversed ? compareRemapBound(source, inMax) < 0 : compareRemapBound(source, inMax) > 0;
    if ((below && belowMode == 1) || (above && aboveMode == 1))
        return source;

    long double position = 0;
    if (below && belowMode == 0)
        position = 0;
    else if (above && aboveMode == 0)
        position = 1;
    else if (!math::normalizeRange(static_cast<long double>(source),
                                   static_cast<long double>(inMin), static_cast<long double>(inMax),
                                   position))
        throw std::overflow_error("VDB remap normalization exceeded the numeric range");
    const long double value = ramp.sample(position, below || above);
    const long double result = std::fma(value, static_cast<long double>(outMax) - outMin,
                                        static_cast<long double>(outMin));
    T converted;
    if (!numeric::checkedNumericCast(result, converted))
        throw std::overflow_error("VDB remap result cannot be represented by the grid type");
    return converted;
}
} // namespace vdb
} // namespace dommy
