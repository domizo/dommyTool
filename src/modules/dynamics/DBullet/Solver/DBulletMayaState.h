#pragma once

#include "common/utility/DStatusOperation.h"
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MVector.h>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace dommy {
namespace bullet {

// Append only to evaluation-local storage. No native world state is changed.
inline MStatus appendRestartScalars(MDataBlock& data,
                                    std::initializer_list<MObject> attributes,
                                    std::vector<double>& settings)
{
    for (const MObject& attribute : attributes) {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        const double value = handle.asDouble();
        if (!std::isfinite(value)) return MS::kInvalidParameter;
        settings.push_back(value);
    }
    return MS::kSuccess;
}

inline MStatus appendRestartVectors(MDataBlock& data,
                                    std::initializer_list<MObject> attributes,
                                    std::vector<double>& settings)
{
    for (const MObject& attribute : attributes) {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) return status;
        const MVector value = handle.asVector();
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(value[axis])) return MS::kInvalidParameter;
            settings.push_back(value[axis]);
        }
    }
    return MS::kSuccess;
}

struct PlaneAttributes {
    MObject enabled, position, axis, restitution, friction;
};

// Compare values in compute, not only dirty callbacks (which are not a
// reliable state-invalidation boundary under Maya's Evaluation Manager).
inline MStatus readPlaneRestartSettings(MDataBlock& data, double startSeconds,
                                        const PlaneAttributes& plane,
                                        std::vector<double>& settings)
{
    if (!std::isfinite(startSeconds)) return MS::kInvalidParameter;
    MStatus status;
    const MDataHandle enableHandle = data.inputValue(plane.enabled, &status);
    if (!status) return status;
    const bool enabled = enableHandle.asBool();
    std::vector<double> candidate{startSeconds, enabled ? 1.0 : 0.0};
    if (enabled) {
        DOMMY_RETURN_IF_FAILURE(appendRestartVectors(
            data, {plane.position, plane.axis}, candidate));
        DOMMY_RETURN_IF_FAILURE(appendRestartScalars(
            data, {plane.restitution, plane.friction}, candidate));
    }
    settings.swap(candidate);
    return MS::kSuccess;
}

} // namespace bullet
} // namespace dommy
