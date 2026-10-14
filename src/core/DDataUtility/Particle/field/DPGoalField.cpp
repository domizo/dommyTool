#include "common/utility/DMayaEntryPointGuard.h"
#include "DPGoalField.h"

#include "DPData.h"
#include "DPointDataAccess.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DParticleFieldMath.h"
#include "dpoint_ids.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>

MObject DPGoalField::inDP;
MObject DPGoalField::aDamping;
MObject DPGoalField::aSpringConstant;
MTypeId DPGoalField::typeId(ID_DP_GOAL_FIELD);

void* DPGoalField::creator()
{
    return ::dommy::maya::createObject("DPGoalField", [&]() {
        return new DPGoalField();
    });
}

MStatus DPGoalField::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;
    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id, MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setReadable(false);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inDP);
    if (!status) return status;

    MFnNumericAttribute numericAttribute;
    aDamping = numericAttribute.create(
        "Damping", "da", MFnNumericData::kDouble, 0.5, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = addAttribute(aDamping);
    if (!status) return status;

    aSpringConstant = numericAttribute.create(
        "SpringConstant", "spc", MFnNumericData::kDouble, 1.0, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = addAttribute(aSpringConstant);
    if (!status) return status;

    const MObject inputs[] = {inDP, aDamping, aSpringConstant};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutputForce);
        if (!status) return status;
    }
    return MS::kSuccess;
}

MStatus DPGoalField::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutputForce) return MS::kUnknownParameter;

    MStatus status;
    const int logicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (logicalIndex < 0) {
        MGlobal::displayError("DPGoalField received an invalid output index.");
        return MS::kFailure;
    }

    // This follows Autodesk's field-node pattern. outputArrayValue avoids
    // evaluating unrelated logical inputs and prevents a DG cycle.
    MArrayDataHandle inputArray = data.outputArrayValue(mInputData, &status);
    if (!status) return status;
    status = inputArray.jumpToElement(
        static_cast<unsigned int>(logicalIndex));
    if (!status) return status;

    MDataHandle compound = inputArray.inputValue(&status);
    if (!status) return status;

    MDataHandle positionHandle = compound.child(mInputPositions);
    MFnVectorArrayData positionData(positionHandle.data(), &status);
    if (!status) return status;
    const MVectorArray positions = positionData.array(&status);
    if (!status) return status;
    const unsigned int receptorCount = positions.length();
    if (receptorCount > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }

    MDataHandle velocityHandle = compound.child(mInputVelocities);
    MFnVectorArrayData velocityData(velocityHandle.data(), &status);
    if (!status) return status;
    const MVectorArray velocities = velocityData.array(&status);
    if (!status) return status;

    MVectorArray forceArray;
    status = dommy::maya::buildCheckedArray(
        receptorCount,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        }, forceArray);
    if (!status) return status;
    bool inputsAreValid = dommy::field::isValidOptionalFieldArrayCount(
        receptorCount, velocities.length());
    if (!inputsAreValid) {
        MGlobal::displayWarning(
            "DPGoalField velocity count is invalid; zero force was output.");
    }

    MDataHandle magnitudeHandle = data.inputValue(mMagnitude, &status);
    if (!status) return status;
    const double magnitude = magnitudeHandle.asDouble();
    MDataHandle dampingHandle = data.inputValue(aDamping, &status);
    if (!status) return status;
    const double damping = dampingHandle.asDouble();
    MDataHandle springHandle = data.inputValue(aSpringConstant, &status);
    if (!status) return status;
    const double springConstant = springHandle.asDouble();
    if (!std::isfinite(magnitude) || !std::isfinite(damping) ||
        !std::isfinite(springConstant)) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPGoalField spring parameters must be finite; "
            "zero force was output.");
    }

    MDataHandle useMaximumDistanceHandle = data.inputValue(
        mUseMaxDistance, &status);
    if (!status) return status;
    const bool useMaximumDistance = useMaximumDistanceHandle.asBool();
    double maximumDistance = 0.0;
    double attenuation = 0.0;
    if (useMaximumDistance) {
        MDataHandle maximumDistanceHandle = data.inputValue(
            mMaxDistance, &status);
        if (!status) return status;
        maximumDistance = maximumDistanceHandle.asDouble();
        MDataHandle attenuationHandle = data.inputValue(
            mAttenuation, &status);
        if (!status) return status;
        attenuation = attenuationHandle.asDouble();
        if (!std::isfinite(maximumDistance) || maximumDistance <= 1.0e-12 ||
            !std::isfinite(attenuation)) {
            inputsAreValid = false;
            MGlobal::displayWarning(
                "DPGoalField maximum-distance parameters are invalid; "
                "zero force was output.");
        }
    }

    const DPData* inputData = nullptr;
    status = getOptionalInputDP(thisMObject(), inDP, data, inputData);
    if (!status) return status;
    const DPointList* targetPoints = NULL;
    std::size_t targetCount = 0U;
    if (inputData) {
        targetPoints = inputData->getPositionPtrConst();
        targetCount = targetPoints->size();
    }
    if (receptorCount > 0U && targetCount == 0U) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPGoalField requires at least one DPoint target; "
            "zero force was output.");
    }

    bool rejectedNumericValue = false;
    for (unsigned int index = 0U;
         inputsAreValid && index < receptorCount; ++index) {
        const MVector& receptor = positions[index];
        const MVector velocity = velocities.length() == 0U
            ? MVector::zero : velocities[index];
        const DPoint& targetPoint =
            (*targetPoints)[static_cast<std::size_t>(index) % targetCount];
        const MVector target(targetPoint[0], targetPoint[1], targetPoint[2]);

        bool pointIsFinite = true;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!std::isfinite(receptor[axis]) ||
                !std::isfinite(velocity[axis]) ||
                !std::isfinite(target[axis])) {
                pointIsFinite = false;
                break;
            }
        }
        if (!pointIsFinite) {
            rejectedNumericValue = true;
            continue;
        }

        double scale = magnitude;
        if (useMaximumDistance) {
            const MVector displacement = target - receptor;
            const double distance = displacement.length();
            if (!dommy::field::computeGoalDistanceScale(
                    distance, maximumDistance, attenuation,
                    magnitude, scale)) {
                rejectedNumericValue = true;
                continue;
            }
            if (scale == 0.0) continue;

            const double normalizedDistance = distance / maximumDistance;
            const double curveScale = falloffCurve(
                normalizedDistance, &status);
            if (!status) return status;
            if (!dommy::field::checkedMultiplyFieldValue(
                    scale, curveScale, scale)) {
                rejectedNumericValue = true;
                continue;
            }
        }

        MVector result;
        bool forceIsValid = true;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::field::computeGoalFieldForceComponent(
                    receptor[axis], target[axis], velocity[axis],
                    springConstant, damping, scale, result[axis])) {
                forceIsValid = false;
                break;
            }
        }
        if (forceIsValid) {
            forceArray[index] = result;
        } else {
            rejectedNumericValue = true;
        }
    }
    if (rejectedNumericValue) {
        MGlobal::displayWarning(
            "DPGoalField rejected a non-finite or overflowing point value; "
            "the affected force was set to zero.");
    }

    MArrayDataHandle outputArray = data.outputArrayValue(
        mOutputForce, &status);
    if (!status) return status;
    MArrayDataBuilder outputBuilder = outputArray.builder(&status);
    if (!status) return status;
    MDataHandle outputHandle = outputBuilder.addElement(
        static_cast<unsigned int>(logicalIndex), &status);
    if (!status) return status;

    MFnVectorArrayData outputData;
    const MObject outputObject = outputData.create(forceArray, &status);
    if (!status) return status;
    status = outputHandle.set(outputObject);
    if (!status) return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPGoalField")
