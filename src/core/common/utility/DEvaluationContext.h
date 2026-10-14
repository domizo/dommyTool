#pragma once

#include "DExpressionEvaluation.h"

#include <maya/MDataBlock.h>
#include <maya/MDGContext.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

#include <cmath>

namespace dommy {
namespace maya {

inline MStatus getEvaluationTime(MDataBlock& data, MTime& time) {
    MStatus status;
    const MDGContext context = data.context(&status);
    if (status != MS::kSuccess)
        return status;
    return context.getTime(time);
}

template <typename Expression>
MStatus setExpressionTime(Expression& expression, const MTime& time) {
    return dommy::expression::setTime(expression, time, MTime::uiUnit(), MTime::kSeconds)
               ? MS::kSuccess
               : MS::kInvalidParameter;
}

// Returns the duration of one unit in the time carried by the current DG
// context. This avoids coupling node evaluation to interactive playback
// preferences such as MAnimControl::playbackBy().
inline MStatus getEvaluationStepSeconds(MDataBlock& data, double& seconds) {
    MTime evaluationTime;
    const MStatus status = getEvaluationTime(data, evaluationTime);
    if (status != MS::kSuccess)
        return status;

    seconds = MTime(1.0, evaluationTime.unit()).as(MTime::kSeconds);
    return std::isfinite(seconds) && seconds > 1.0e-12 ? MS::kSuccess : MS::kFailure;
}

} // namespace maya
} // namespace dommy
