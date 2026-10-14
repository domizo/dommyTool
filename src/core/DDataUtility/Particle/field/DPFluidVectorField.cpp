#include "common/utility/DMayaEntryPointGuard.h"
#include "DPFluidVectorField.h"

#include "DPData.h"
#include "DPointDataAccess.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DParticleFieldMath.h"
#include "dpoint_ids.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

MObject DPFluidVectorField::inDP;
MObject DPFluidVectorField::aVectorAttribute;
MTypeId DPFluidVectorField::typeId(ID_DP_FLUID_VECTOR_FIELD);

void* DPFluidVectorField::creator()
{
    return ::dommy::maya::createObject("DPFluidVectorField", [&]() {
        return new DPFluidVectorField();
    });
}

MStatus DPFluidVectorField::initialize()
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

    MFnStringData stringData;
    const MObject defaultName = stringData.create("vel", &status);
    if (!status) return status;
    aVectorAttribute = typedAttribute.create(
        "VectorAttribute", "vecattr", MFnData::kString,
        defaultName, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aVectorAttribute);
    if (!status) return status;

    status = attributeAffects(inDP, mOutputForce);
    if (!status) return status;
    status = attributeAffects(aVectorAttribute, mOutputForce);
    if (!status) return status;
    return MS::kSuccess;
}

MStatus DPFluidVectorField::compute(const MPlug& plug,
                                    MDataBlock& data) try
{
    if (plug != mOutputForce) return MS::kUnknownParameter;

    MStatus status;
    const int logicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (logicalIndex < 0) {
        MGlobal::displayError(
            "DPFluidVectorField received an invalid output index.");
        return MS::kFailure;
    }

    // Autodesk's field samples deliberately use outputArrayValue here so
    // that only this logical input is evaluated and a DG cycle is avoided.
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

    const DPData* inputData = nullptr;
    status = getOptionalInputDP(thisMObject(), inDP, data, inputData);
    if (!status) return status;
    if (!inputData) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPFluidVectorField requires DPoint data; zero force was output.");
    } else if (inputData->numberOfPoints() != receptorCount) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPFluidVectorField DPoint and receptor counts differ; "
            "zero force was output.");
    } else if (!inputsAreValid) {
        MGlobal::displayWarning(
            "DPFluidVectorField velocity count is invalid; "
            "zero force was output.");
    }

    MDataHandle attributeNameHandle = data.inputValue(
        aVectorAttribute, &status);
    if (!status) return status;
    const std::string attributeName =
        attributeNameHandle.asString().asChar();

    MDataHandle magnitudeHandle = data.inputValue(mMagnitude, &status);
    if (!status) return status;
    const double magnitude = magnitudeHandle.asDouble();
    if (!std::isfinite(magnitude)) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPFluidVectorField magnitude is not finite; "
            "zero force was output.");
    }

    // Missing attributes use zero directly; present values need no Maya array
    // allocation between typed conversion and the force calculation.
    std::vector<Imath::V3d> attributeValues;
    if (inputsAreValid && inputData->isExistAttr(attributeName) &&
        !dommy::attribute::convertVector3Attribute(
            inputData->getAttr(attributeName), receptorCount, attributeValues)) {
        inputsAreValid = false;
        MGlobal::displayWarning(
            "DPFluidVectorField vector attribute is invalid; "
            "zero force was output.");
    }

    const bool valuesWereReady = inputsAreValid;
    for (unsigned int index = 0U;
         inputsAreValid && index < receptorCount; ++index) {
        const MVector velocity = velocities.length() == 0U
            ? MVector::zero : velocities[index];
        MVector result;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            const double attributeValue = attributeValues.empty()
                ? 0.0 : attributeValues[index][axis];
            if (!dommy::field::computeVectorFieldForceComponent(
                    velocity[axis], attributeValue,
                    magnitude, result[axis])) {
                inputsAreValid = false;
                break;
            }
        }
        if (inputsAreValid) forceArray[index] = result;
    }
    if (valuesWereReady && !inputsAreValid) {
        MGlobal::displayWarning(
            "DPFluidVectorField force calculation overflowed or received "
            "a non-finite value; zero force was output.");
    }
    if (!inputsAreValid) {
        for (unsigned int index = 0U; index < receptorCount; ++index) {
            forceArray[index] = MVector::zero;
        }
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
DOMMY_COMPUTE_CATCH("DPFluidVectorField")
