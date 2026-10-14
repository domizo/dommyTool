#pragma once

#include "common/utility/DMayaCheckedArray.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDGModifier.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MVectorArray.h>

namespace dommy {

namespace particle {

inline MStatus readArrayBoolInput(MDataBlock& data,
                                  const MObject& attribute,
                                  unsigned int logicalIndex,
                                  bool& value)
{
    MStatus status;
    MArrayDataHandle array = data.inputArrayValue(attribute, &status);
    if (!status) return status;
    status = array.jumpToElement(logicalIndex);
    if (!status) return status;
    MDataHandle handle = array.inputValue(&status);
    if (!status) return status;
    value = handle.asBool();
    return MS::kSuccess;
}

inline MStatus readArrayTimeInput(MDataBlock& data,
                                  const MObject& attribute,
                                  unsigned int logicalIndex,
                                  MTime& value)
{
    MStatus status;
    MArrayDataHandle array = data.inputArrayValue(attribute, &status);
    if (!status) return status;
    status = array.jumpToElement(logicalIndex);
    if (!status) return status;
    MDataHandle handle = array.inputValue(&status);
    if (!status) return status;
    value = handle.asTime();
    return MS::kSuccess;
}

inline MStatus copyArrayAttribute(MFnArrayAttrsData& outputData,
                                  const MString& name,
                                  const MVectorArray& source)
{
    MStatus status;
    MVectorArray output = outputData.vectorArray(name, &status);
    if (!status) return status;
    return dommy::maya::copyCheckedArray(source, output);
}

inline MStatus copyArrayAttribute(MFnArrayAttrsData& outputData,
                                  const MString& name,
                                  const MDoubleArray& source)
{
    MStatus status;
    MDoubleArray output = outputData.doubleArray(name, &status);
    if (!status) return status;
    return dommy::maya::copyCheckedArray(source, output);
}

inline MStatus copyArrayAttribute(MFnArrayAttrsData& outputData,
                                  const MString& name,
                                  const MIntArray& source)
{
    MStatus status;
    MIntArray output = outputData.intArray(name, &status);
    if (!status) return status;
    return dommy::maya::copyCheckedArray(source, output);
}

inline MStatus copyArrayAttribute(MFnArrayAttrsData& outputData,
                                  const MString& name,
                                  const MStringArray& source)
{
    MStatus status;
    MStringArray output = outputData.stringArray(name, &status);
    if (!status) return status;
    return dommy::maya::copyCheckedArray(source, output);
}

} // namespace particle

inline MStatus queueMissingParticleRgbAttributes(
    const MFnParticleSystem& particle,
    MDGModifier& modifier,
    bool& hasPendingEdit)
{
    hasPendingEdit = false;
    MStatus status;
    const MObject particleObject = particle.object(&status);
    if (status != MS::kSuccess) {
        return status;
    }

    MFnDependencyNode particleNode(particleObject, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MFnTypedAttribute attribute;
    const char* const attributeNames[] = {"rgbPP", "rgbPP0"};
    for (unsigned int index = 0; index < 2; ++index) {
        const MString name(attributeNames[index]);
        const bool exists = particleNode.hasAttribute(name, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        if (exists) {
            continue;
        }

        const MObject newAttribute =
            attribute.create(name, name, MFnData::kVectorArray, MObject::kNullObj, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        status = modifier.addAttribute(particleObject, newAttribute);
        if (status != MS::kSuccess) {
            return status;
        }
        hasPendingEdit = true;
    }

    return MS::kSuccess;
}

inline MStatus ensureParticleRgbAttributes(const MFnParticleSystem& particle) {
    MDGModifier modifier;
    bool hasPendingEdit = false;
    MStatus status = queueMissingParticleRgbAttributes(
        particle, modifier, hasPendingEdit);
    if (!status) {
        return status;
    }
    return hasPendingEdit ? modifier.doIt() : MStatus(MS::kSuccess);
}

} // namespace dommy
