#pragma once

#include "DMayaCheckedArray.h"
#include "DMayaEntryPointGuard.h"
#include "DStatusOperation.h"

#include <maya/MColorArray.h>
#include <maya/MCurveAttribute.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MRampAttribute.h>
#include <initializer_list>

namespace dommy {
namespace maya {
namespace detail {
template <class Value, class Array>
MStatus makeDefaultRampArray(std::initializer_list<Value> values, Array& result) {
    return buildCheckedArray(
        values.size(),
        [&](unsigned int i, Value& value) -> MStatus {
            value = *(values.begin() + i);
            return MS::kSuccess;
        },
        result);
}

template <class Ramp, class Fill>
MStatus initializeEmptyRamp(const MObject& node, const MObject& attribute, Fill fill) {
    MStatus status;
    Ramp ramp(MPlug(node, attribute), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int count = ramp.getNumEntries(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    // Never replace values restored from a scene or supplied by the user.
    return count == 0U ? fill(ramp) : MStatus(MS::kSuccess);
}

template <class Value, class Array>
MStatus initializeDefaultRamp(const MObject& node, const MObject& attribute,
                              std::initializer_list<Value> values,
                              std::initializer_list<float> positions,
                              std::initializer_list<int> interpolation) {
    if (values.size() == 0U || values.size() != positions.size() ||
        values.size() != interpolation.size())
        return MS::kInvalidParameter;
    return initializeEmptyRamp<MRampAttribute>(
        node, attribute, [&](MRampAttribute& ramp) -> MStatus {
            Array valueArray;
            MFloatArray positionArray;
            MIntArray interpolationArray;
            DOMMY_RETURN_IF_FAILURE(makeDefaultRampArray(values, valueArray));
            DOMMY_RETURN_IF_FAILURE(makeDefaultRampArray(positions, positionArray));
            DOMMY_RETURN_IF_FAILURE(makeDefaultRampArray(interpolation, interpolationArray));
            return ramp.setRamp(valueArray, positionArray, interpolationArray);
        });
}
} // namespace detail

inline MStatus initializeDefaultCurve(const MObject& node, const MObject& attribute,
                                      std::initializer_list<float> values,
                                      std::initializer_list<float> positions) {
    if (values.size() == 0U || values.size() != positions.size())
        return MS::kInvalidParameter;
    return detail::initializeEmptyRamp<MCurveAttribute>(
        node, attribute, [&](MCurveAttribute& ramp) -> MStatus {
            MFloatArray valueArray, positionArray;
            DOMMY_RETURN_IF_FAILURE(detail::makeDefaultRampArray(values, valueArray));
            DOMMY_RETURN_IF_FAILURE(detail::makeDefaultRampArray(positions, positionArray));
            return ramp.setCurve(valueArray, positionArray);
        });
}

inline MStatus initializeDefaultValueRamp(const MObject& node, const MObject& attribute,
                                          std::initializer_list<float> values,
                                          std::initializer_list<float> positions,
                                          std::initializer_list<int> interpolation) {
    return detail::initializeDefaultRamp<float, MFloatArray>(node, attribute, values, positions,
                                                             interpolation);
}

inline MStatus initializeDefaultColorRamp(const MObject& node, const MObject& attribute,
                                          std::initializer_list<MColor> values,
                                          std::initializer_list<float> positions,
                                          std::initializer_list<int> interpolation) {
    return detail::initializeDefaultRamp<MColor, MColorArray>(node, attribute, values, positions,
                                                              interpolation);
}

// Maya's void postConstructor cannot return MStatus or roll back earlier ramps.
// Stop at the first failure, report it, and contain exceptions at the host entry.
template <class Initialize>
void initializeNodeRamps(const char* nodeName, Initialize initialize) noexcept {
    try {
        const MStatus status = initialize();
        if (!status) {
            const MString context = MString(nodeName) + "::postConstructor";
            status.pAPIerror(context.asChar(), __LINE__);
        }
    } catch (...) {
        (void)reportEntryPointException(nodeName, "postConstructor", std::current_exception());
    }
}
} // namespace maya
} // namespace dommy
