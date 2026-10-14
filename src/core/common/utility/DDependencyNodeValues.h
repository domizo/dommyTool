#pragma once

#include "common/utility/DMayaValueValidation.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>

namespace dommy {
namespace maya {

inline MStatus readDependencyVector(
    const MObject& node,
    const char* plugName,
    MVector& value)
{
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) return status;

    MPlug plug = nodeFn.findPlug(plugName, true, &status);
    if (!status) return status;

    if (plug.numChildren(&status) != 3U || !status) {
        return MS::kFailure;
    }

    double components[3] = {0.0, 0.0, 0.0};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        MPlug child = plug.child(axis, &status);
        if (!status) return status;
        status = child.getValue(components[axis]);
        if (!status) return status;
    }

    const MVector candidate(components[0], components[1], components[2]);
    if (!isFiniteVector(candidate)) return MS::kFailure;

    value = candidate;
    return MS::kSuccess;
}

inline MStatus readDependencyShort(
    const MObject& node,
    const char* plugName,
    short& value)
{
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) return status;

    MPlug plug = nodeFn.findPlug(plugName, true, &status);
    if (!status) return status;
    return plug.getValue(value);
}

}  // namespace maya
}  // namespace dommy
