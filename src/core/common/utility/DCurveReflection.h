#pragma once

#include "DCurveEvaluation.h"
#include "DDirectionMath.h"

namespace dommy {
namespace curve {

// Evaluate one collision before the node commits its continuation state.
// A terminal collision produces only the hit point. Zero incident distance
// uses the sampled incoming tangent (e.g. the first sample starts on a mesh).
template <typename Point, typename Vector>
bool evaluateReflectionHit(const Point& sample, const Point& previous,
                           const Vector& incoming, const Point& hit, Vector normal,
                           double step, double traveled, double length, double bias,
                           bool& finished, Point& next)
{
    direction::Delta nearDelta;
    if (!std::isfinite(step) || step < 0.0 || !std::isfinite(traveled) || traveled < 0.0 ||
        !std::isfinite(length) || length < traveled || !std::isfinite(bias) ||
        !direction::difference(sample, hit, nearDelta)) return false;
    if (nearDelta.length >= length - traveled) {
        next = hit;
        finished = true;
        return true;
    }
    if (!normalizeDirection(normal)) return false;
    direction::Delta incidentDelta;
    if (!direction::difference(previous, hit, incidentDelta)) return false;
    Vector incident(incoming);
    if (incidentDelta.length > 1.0e-12) {
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            incident[axis] = incidentDelta.values[axis] / incidentDelta.length;
    }
    if (!normalizeDirection(incident)) return false;
    double dot = 0.0;
    for (unsigned int axis = 0U; axis < 3U; ++axis) dot += incident[axis] * normal[axis];
    Vector reflected(incident);
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        reflected[axis] -= 2.0 * dot * normal[axis];
    if (!normalizeDirection(reflected)) return false;
    const double remainingStep = std::fabs(step - nearDelta.length);
    Point candidate(hit);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        const double value = hit[axis] + remainingStep *
            (normal[axis] * bias + reflected[axis] * (1.0 - bias));
        if (!numeric::canRepresentAsFloat(value)) return false;
        candidate[axis] = value;
    }
    next = candidate;
    finished = false;
    return true;
}

} // namespace curve
} // namespace dommy
