#pragma once

#include "DMathEvaluation.h"
#include <maya/MStatus.h>
#include <array>

namespace dommy {
namespace math {

// Check all conversions before touching a Maya array. Allocation failure must
// propagate; a shortened array is never a valid result of an exact-count map.
template <typename Number, typename Array, typename Read>
MStatus writeScalarMathArray(std::size_t count, Read read, Array& output)
{
    if (!validMathBudget(count, 0U)) return MS::kInvalidParameter;
    std::vector<Number> candidate(count);
    for (std::size_t i = 0U; i < count; ++i)
        if (!numeric::checkedNumericCast(read(i), candidate[i]))
            return MS::kInvalidParameter;
    const MStatus status = output.setLength(static_cast<unsigned int>(count));
    if (status != MS::kSuccess) return status;
    if (output.length() != count) return MS::kFailure;
    for (unsigned int i = 0U; i < output.length(); ++i) output[i] = candidate[i];
    return MS::kSuccess;
}

template <typename Element, typename Array, typename Read>
MStatus writeVectorMathArray(std::size_t count, Read read, Array& output)
{
    if (!validMathBudget(count, 0U)) return MS::kInvalidParameter;
    std::vector<std::array<double, 3> > candidate(count);
    for (std::size_t i = 0U; i < count; ++i) {
        const auto& value = read(i);
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!numeric::checkedNumericCast(value[axis], candidate[i][axis]))
                return MS::kInvalidParameter;
    }
    const MStatus status = output.setLength(static_cast<unsigned int>(count));
    if (status != MS::kSuccess) return status;
    if (output.length() != count) return MS::kFailure;
    for (unsigned int i = 0U; i < output.length(); ++i) {
        const auto& v = candidate[i];
        output[i] = Element(v[0], v[1], v[2]);
    }
    return MS::kSuccess;
}

} // namespace math
} // namespace dommy
