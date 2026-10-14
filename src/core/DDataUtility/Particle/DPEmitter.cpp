#include "common/utility/DMayaEntryPointGuard.h"
#include "DPEmitter.h"

#include "DPAttributeAccess.h"
#include "DPData.h"
#include "DPointDataAccess.h"
#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "dpoint_ids.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
#include <string>

namespace {

enum EmitTiming
{
    kEmitOnce = 0,
    kEmitRange = 1,
    kEmitSequence = 2
};

enum VelocityType
{
    kVelocityNone = 0,
    kVelocityDirection = 1,
    kVelocityNextPoint = 2,
    kVelocityAttribute = 3
};

const double kMinimumDirectionLength = 1.0e-12;

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

void warnInvalidTransfer(const std::string& sourceName,
                         const std::string& targetName)
{
    MGlobal::displayWarning(
        MString("DPEmitter skipped invalid attribute transfer '") +
        sourceName.c_str() + "' -> '" + targetName.c_str() + "'.");
}

} // namespace

MTypeId DPEmitter::id(ID_DP_EMITTER);

MObject DPEmitter::inDP;
MObject DPEmitter::aEmitTiming;
MObject DPEmitter::aEmitStartEnd;
MObject DPEmitter::aVelocityType;
MObject DPEmitter::aVelocityAttribute;
MObject DPEmitter::aTransferList;
MObject DPEmitter::aSourceAttribute;
MObject DPEmitter::aTargetAttribute;

void* DPEmitter::creator()
{
    return ::dommy::maya::createObject("DPEmitter", [&]() {
        return new DPEmitter();
    });
}

MStatus DPEmitter::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError("DPEmitter received an invalid output index.");
        return MS::kFailure;
    }
    const unsigned int logicalIndex =
        static_cast<unsigned int>(rawLogicalIndex);

    MArrayDataHandle outputArray = data.outputArrayValue(mOutput, &status);
    if (!status) return status;
    MArrayDataBuilder outputBuilder = outputArray.builder(&status);
    if (!status) return status;
    MDataHandle outputHandle = outputBuilder.addElement(logicalIndex, &status);
    if (!status) return status;

    MFnArrayAttrsData outputData;
    const MObject outputObject = outputData.create(&status);
    if (!status) return status;

    const auto finishOutput = [&]() -> MStatus {
        MStatus finishStatus = outputHandle.set(outputObject);
        if (!finishStatus) return finishStatus;
        return data.setClean(plug);
    };

    bool targetIsFull = false;
    status = dommy::particle::readArrayBoolInput(
        data, mIsFull, logicalIndex, targetIsFull);
    if (!status) return status;
    if (targetIsFull) return finishOutput();

    MDataHandle currentTimeHandle = data.inputValue(mCurrentTime, &status);
    if (!status) return status;
    const MTime currentTime = currentTimeHandle.asTime();

    MTime deltaTime;
    status = dommy::particle::readArrayTimeInput(
        data, mDeltaTime, logicalIndex, deltaTime);
    if (!status) return status;
    const double currentFrame = currentTime.value();
    const double deltaSeconds = deltaTime.as(MTime::kSeconds);
    if (!std::isfinite(currentFrame) || !std::isfinite(deltaSeconds)) {
        MGlobal::displayError("DPEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (deltaSeconds <= 0.0) return finishOutput();

    MDataHandle startEndHandle = data.inputValue(aEmitStartEnd, &status);
    if (!status) return status;
    const double2& startEnd = startEndHandle.asDouble2();
    const double startFrame = startEnd[0];
    const double endFrame = startEnd[1];
    if (!std::isfinite(startFrame) || !std::isfinite(endFrame)) {
        MGlobal::displayError("DPEmitter frame range must be finite.");
        return MS::kFailure;
    }

    MDataHandle timingHandle = data.inputValue(aEmitTiming, &status);
    if (!status) return status;
    const short timing = timingHandle.asShort();
    bool shouldEmit = false;
    switch (timing) {
        case kEmitOnce:
            shouldEmit = currentFrame == startFrame;
            break;
        case kEmitRange:
            shouldEmit = dommy::numeric::isWithinInclusiveBounds(
                currentFrame, startFrame, endFrame);
            break;
        case kEmitSequence:
            shouldEmit = currentFrame >= startFrame;
            break;
        default:
            MGlobal::displayWarning(
                "DPEmitter has an unsupported emission timing mode.");
            return finishOutput();
    }
    if (!shouldEmit) return finishOutput();

    MDataHandle rateHandle = data.inputValue(mRate, &status);
    if (!status) return status;
    const double rate = rateHandle.asDouble();
    if (!std::isfinite(rate)) {
        MGlobal::displayError("DPEmitter rate must be finite.");
        return MS::kFailure;
    }
    // Preserve the historical node contract: rate enables emission, while
    // each DPoint contributes exactly one particle per evaluation.
    if (rate <= 0.0) return finishOutput();

    const DPData* inputData = nullptr;
    status = getOptionalInputDP(thisMObject(), inDP, data, inputData);
    if (!status) return status;
    if (inputData == NULL) {
        MGlobal::displayWarning("DPEmitter requires valid DPoint input.");
        return finishOutput();
    }

    const DPointList* const points = inputData->getPositionPtrConst();
    if (points == NULL || points->empty()) return finishOutput();
    const std::size_t pointCount = points->size();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount ||
        pointCount > static_cast<std::size_t>(
                         std::numeric_limits<unsigned int>::max())) {
        MGlobal::displayError(
            "DPEmitter DPoint input exceeds the supported point limit.");
        return MS::kFailure;
    }

    MVectorArray positions;
    status = dommy::maya::buildCheckedArray(
        pointCount,
        [&](unsigned int index, MVector& position) -> MStatus {
            const DPoint& point = (*points)[index];
            position = MVector(point[0], point[1], point[2]);
            if (!isFiniteVector(position)) {
                MGlobal::displayError("DPEmitter DPoint positions must be finite.");
                return MS::kFailure;
            }
            return MS::kSuccess;
        }, positions);
    if (!status) return status;

    MDataHandle velocityTypeHandle = data.inputValue(aVelocityType, &status);
    if (!status) return status;
    const short velocityType = velocityTypeHandle.asShort();
    MVectorArray velocities;
    status = dommy::maya::buildCheckedArray(
        pointCount,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        }, velocities);
    if (!status) return status;
    switch (velocityType) {
        case kVelocityNone:
            break;
        case kVelocityDirection: {
            MDataHandle directionHandle = data.inputValue(mDirection, &status);
            if (!status) return status;
            const double3& rawDirection = directionHandle.asDouble3();
            const MVector direction(
                rawDirection[0], rawDirection[1], rawDirection[2]);
            if (!isFiniteVector(direction)) {
                MGlobal::displayError(
                    "DPEmitter direction must be finite.");
                return MS::kFailure;
            }
            for (unsigned int index = 0U;
                 index < velocities.length(); ++index) {
                velocities[index] = direction;
            }
            break;
        }
        case kVelocityNextPoint:
            for (std::size_t index = 0U; index < pointCount; ++index) {
                const std::size_t nextIndex = (index + 1U) % pointCount;
                MVector direction =
                    positions[static_cast<unsigned int>(nextIndex)] -
                    positions[static_cast<unsigned int>(index)];
                const double length = direction.length();
                if (!std::isfinite(length)) {
                    MGlobal::displayError(
                        "DPEmitter could not calculate a finite next-point velocity.");
                    return MS::kFailure;
                }
                if (length > kMinimumDirectionLength) {
                    direction /= length;
                    if (!isFiniteVector(direction)) return MS::kFailure;
                    velocities[static_cast<unsigned int>(index)] = direction;
                }
            }
            break;
        case kVelocityAttribute: {
            MDataHandle attributeNameHandle = data.inputValue(
                aVelocityAttribute, &status);
            if (!status) return status;
            const std::string attributeName =
                attributeNameHandle.asString().asChar();
            if (!attributeName.empty() && inputData->isExistAttr(attributeName) &&
                !getCheckedMVectorArrayFromAttribute(
                    *inputData, attributeName, pointCount, velocities)) {
                MGlobal::displayWarning(
                    "DPEmitter velocity attribute is invalid; no particles were emitted.");
                return finishOutput();
            }
            break;
        }
        default:
            MGlobal::displayWarning(
                "DPEmitter has an unsupported velocity mode.");
            return finishOutput();
    }

    status = dommy::particle::copyArrayAttribute(
        outputData, "position", positions);
    if (!status) return status;
    status = dommy::particle::copyArrayAttribute(
        outputData, "velocity", velocities);
    if (!status) return status;

    MArrayDataHandle transferArray = data.inputArrayValue(
        aTransferList, &status);
    if (!status) return status;
    const unsigned int transferCount = transferArray.elementCount(&status);
    if (!status) return status;

    std::size_t arrayCount = 0U;
    std::size_t transferWorkItems = 0U;
    if (!dommy::generation::checkedSum(
            static_cast<std::size_t>(transferCount), 2U,
            dommy::generation::kMaxGeneratedPointCount, arrayCount) ||
        !dommy::generation::checkedProduct(
            pointCount, arrayCount,
            dommy::generation::kMaxWorkItemCount, transferWorkItems)) {
        MGlobal::displayError(
            "DPEmitter attribute transfer exceeds the supported work limit.");
        return MS::kFailure;
    }

    std::set<std::string> targetNames;
    targetNames.insert("position");
    targetNames.insert("velocity");
    for (unsigned int transferIndex = 0U;
         transferIndex < transferCount; ++transferIndex) {
        status = transferArray.jumpToArrayElement(transferIndex);
        if (!status) return status;
        MDataHandle transferHandle = transferArray.inputValue(&status);
        if (!status) return status;

        const std::string sourceName =
            transferHandle.child(aSourceAttribute).asString().asChar();
        const std::string targetName =
            transferHandle.child(aTargetAttribute).asString().asChar();
        if (sourceName.empty() || targetName.empty() ||
            !inputData->isExistAttr(sourceName)) {
            continue;
        }
        if (targetNames.find(targetName) != targetNames.end()) {
            warnInvalidTransfer(sourceName, targetName);
            continue;
        }

        const DPAttributeData::ConstPtr attribute =
            inputData->getAttr(sourceName);
        if (!attribute) {
            warnInvalidTransfer(sourceName, targetName);
            continue;
        }

        bool valuesAreValid = false;
        switch (attribute->getAttrType()) {
            case DP_ATTR_VEC3I:
            case DP_ATTR_VEC3F:
            case DP_ATTR_VEC3D: {
                MVectorArray values;
                valuesAreValid = getCheckedMVectorArrayFromAttribute(
                    *inputData, sourceName, pointCount, values);
                if (valuesAreValid) {
                    status = dommy::particle::copyArrayAttribute(
                        outputData, targetName.c_str(), values);
                }
                break;
            }
            case DP_ATTR_FLOAT:
            case DP_ATTR_DOUBLE: {
                MDoubleArray values;
                valuesAreValid = getCheckedMDoubleArrayFromAttribute(
                    *inputData, sourceName, pointCount, values);
                if (valuesAreValid) {
                    status = dommy::particle::copyArrayAttribute(
                        outputData, targetName.c_str(), values);
                }
                break;
            }
            case DP_ATTR_BOOL:
            case DP_ATTR_INT:
            case DP_ATTR_UINT32:
            case DP_ATTR_INT64: {
                MIntArray values;
                valuesAreValid = getCheckedMIntArrayFromAttribute(
                    *inputData, sourceName, pointCount, values);
                if (valuesAreValid) {
                    status = dommy::particle::copyArrayAttribute(
                        outputData, targetName.c_str(), values);
                }
                break;
            }
            case DP_ATTR_STRING: {
                MStringArray values;
                valuesAreValid = getCheckedMStringArrayFromAttribute(
                    *inputData, sourceName, pointCount, values);
                if (valuesAreValid) {
                    status = dommy::particle::copyArrayAttribute(
                        outputData, targetName.c_str(), values);
                }
                break;
            }
            default:
                break;
        }
        if (!valuesAreValid) {
            warnInvalidTransfer(sourceName, targetName);
            status = MS::kSuccess;
            continue;
        }
        if (!status) return status;
        targetNames.insert(targetName);
    }

    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DPEmitter")

MStatus DPEmitter::initialize()
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

    MFnEnumAttribute enumAttribute;
    aEmitTiming = enumAttribute.create(
        "EmitTiming", "etime", kEmitOnce, &status);
    if (!status) return status;
    status = enumAttribute.addField("Once", kEmitOnce);
    if (!status) return status;
    status = enumAttribute.addField("Range", kEmitRange);
    if (!status) return status;
    status = enumAttribute.addField("Sequence", kEmitSequence);
    if (!status) return status;
    status = addAttribute(aEmitTiming);
    if (!status) return status;

    MFnNumericAttribute numericAttribute;
    aEmitStartEnd = numericAttribute.create(
        "EmitStartEnd", "ese", MFnNumericData::k2Double, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(1.0, 10.0);
    if (!status) return status;
    status = addAttribute(aEmitStartEnd);
    if (!status) return status;

    aVelocityType = enumAttribute.create(
        "VelocityType", "velt", kVelocityNone, &status);
    if (!status) return status;
    status = enumAttribute.addField("None", kVelocityNone);
    if (!status) return status;
    status = enumAttribute.addField("Direction", kVelocityDirection);
    if (!status) return status;
    status = enumAttribute.addField("Next ID", kVelocityNextPoint);
    if (!status) return status;
    status = enumAttribute.addField("From Attribute", kVelocityAttribute);
    if (!status) return status;
    status = addAttribute(aVelocityType);
    if (!status) return status;

    MFnStringData stringData;
    MObject defaultValue = stringData.create("N", &status);
    if (!status) return status;
    aVelocityAttribute = typedAttribute.create(
        "VelocityAttribute", "velattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;
    status = addAttribute(aVelocityAttribute);
    if (!status) return status;

    defaultValue = stringData.create("Cd", &status);
    if (!status) return status;
    aSourceAttribute = typedAttribute.create(
        "SourceAttribute", "srcattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;

    defaultValue = stringData.create("rgbPP", &status);
    if (!status) return status;
    aTargetAttribute = typedAttribute.create(
        "TargetAttribute", "tarattr", MFnData::kString,
        defaultValue, &status);
    if (!status) return status;
    status = typedAttribute.setConnectable(false);
    if (!status) return status;

    MFnCompoundAttribute compoundAttribute;
    aTransferList = compoundAttribute.create(
        "TransferList", "trl", &status);
    if (!status) return status;
    status = compoundAttribute.addChild(aSourceAttribute);
    if (!status) return status;
    status = compoundAttribute.addChild(aTargetAttribute);
    if (!status) return status;
    status = compoundAttribute.setArray(true);
    if (!status) return status;
    status = compoundAttribute.setUsesArrayDataBuilder(true);
    if (!status) return status;
    status = compoundAttribute.setWritable(true);
    if (!status) return status;
    status = addAttribute(aTransferList);
    if (!status) return status;

    const MObject inputs[] = {
        inDP,
        aEmitTiming,
        aEmitStartEnd,
        aVelocityType,
        aVelocityAttribute,
        aTransferList,
        aSourceAttribute,
        aTargetAttribute};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }

    return MS::kSuccess;
}
