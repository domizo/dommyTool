#pragma once

#include "DMathEvaluation.h"
#include "common/utility/DAttributeValueConversion.h"

// Evaluation-only types. Serialized attributes and newly created attribute
// defaults retain their historical types; conversion happens at publication.
using DMathScalarData = double;
using DMathVec2Data = Imath::V2d;
using DMathVec3Data = Imath::V3d;
using DMathScalarDataList = std::vector<DMathScalarData>;
using DMathVec2DataList = std::vector<DMathVec2Data>;
using DMathVec3DataList = std::vector<DMathVec3Data>;

namespace dommy {
namespace math {

template <typename Result, typename Operand>
bool applyProceduralBinary(int operation, Result& result, const Operand& operand)
{
    using Lhs = numeric::ValueComponents<Result>;
    using Rhs = numeric::ValueComponents<Operand>;
    Result candidate = result;
    // Scalar operands broadcast. A vec2 operand leaves the vec3 Z component
    // unchanged, and scalar results use the operand's X component (legacy rules).
    const unsigned int count = Rhs::count == 1U
        ? Lhs::count : (Lhs::count < Rhs::count ? Lhs::count : Rhs::count);
    for (unsigned int axis = 0U; axis < count; ++axis)
        if (!checkedMathBinary(operation, Lhs::get(result, axis),
                Rhs::get(operand, Rhs::count == 1U ? 0U : axis),
                Lhs::get(candidate, axis))) return false;
    result = candidate;
    return true;
}

template <typename Result>
bool applyProceduralUnary(int operation, Result& result)
{
    using Components = numeric::ValueComponents<Result>;
    Result candidate = result;
    for (unsigned int axis = 0U; axis < Components::count; ++axis)
        if (!checkedMathUnary(operation, Components::get(result, axis),
                Components::get(candidate, axis))) return false;
    result = candidate;
    return true;
}

} // namespace math
} // namespace dommy
