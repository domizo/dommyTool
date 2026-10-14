#pragma once

#include "DCurveUtil.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MPoint.h>
#include <maya/MPointArray.h>

#include <cstdint>

namespace dommy {
namespace curve {

inline bool appendConnectionCurve(DCurveOutputBatch& curves,
                                  const MPoint& first,
                                  const MPoint& second,
                                  std::int32_t firstPrimitive,
                                  std::int32_t secondPrimitive)
{
    if (!maya::isFloatRepresentablePoint(first) ||
        !maya::isFloatRepresentablePoint(second)) {
        return false;
    }

    MPointArray cvs;
    if (!cvs.append(first) || !cvs.append(second)) return false;

    DCItem curve(new DCurveInternalData(cvs, 1));
    if (!curve || !curve->hasValidCurveTopology()) return false;

    DIntArray primitiveIndices(2U);
    primitiveIndices[0] = firstPrimitive;
    primitiveIndices[1] = secondPrimitive;
    curve->addAttr("prim_index", DPIntAttr(primitiveIndices, 0));
    return curves.append(curve);
}

} // namespace curve
} // namespace dommy
