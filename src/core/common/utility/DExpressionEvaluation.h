#pragma once

#include <cmath>
#include <type_traits>

namespace dommy {
namespace expression {

// Keep the destination unchanged on either custom-function or numeric failure.
// Callers prepare the context (including the point ID) before each evaluation.
template <typename Expression, typename Vector>
bool evaluateChecked(Expression& expression, Vector& result) {
    static_assert(std::is_same<typename std::decay<decltype(result[0])>::type, double>::value,
                  "Expression results must be staged in double components");
    const auto value = expression.evaluate();
    if (!expression.evaluationSucceeded())
        return false;
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(value[axis]))
            return false;
    result = Vector(value[0], value[1], value[2]);
    return true;
}

// SeExpr's scalar locals only define val[0], unlike its promoted final result.
// isVec() is non-const in the legacy API, hence the small local copy.
template <typename LocalVariable, typename Vector>
bool readScale(LocalVariable local, Vector& result) {
    static_assert(std::is_same<typename std::decay<decltype(result[0])>::type, double>::value,
                  "Expression scales must be staged in double components");
    const bool vector = local.isVec();
    const Vector value(local.val[0], local.val[vector ? 1 : 0], local.val[vector ? 2 : 0]);
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(value[axis]))
            return false;
    result = value;
    return true;
}

// Unit conversion belongs to the time object; never cache the UI frame rate.
template <typename Expression, typename Time, typename Unit>
bool setTime(Expression& expression, const Time& value, Unit frameUnit, Unit secondsUnit) {
    const double frame = value.as(frameUnit);
    const double seconds = value.as(secondsUnit);
    if (!std::isfinite(frame) || !std::isfinite(seconds))
        return false;
    expression.setFrame(frame);
    expression.setTime(seconds);
    return true;
}

} // namespace expression
} // namespace dommy
