//
//  DPMotionCreate.cpp
//  dommyTool
//
//  Created by yuzo on 11/19/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPMotionCreate.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DTimeSampling.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"

#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "dpoint_ids.h"

#include "DNodeOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "common/utility/DEvaluationContext.h"

#include <cmath>

const MTypeId DPMotionCreate::typeId(ID_DP_MOTION_CREATE);

// Attribute
MObject DPMotionCreate::inDP;

MObject DPMotionCreate::aAnimationMatrix;

MObject DPMotionCreate::aGlobalOffset;
MObject DPMotionCreate::aOffsetType;
MObject DPMotionCreate::aOffsetAttribute;
MObject DPMotionCreate::aInOffsetArray;
MObject DPMotionCreate::aStretchType;
MObject DPMotionCreate::aStretchAttribute;
MObject DPMotionCreate::aInStretchArray;

MObject DPMotionCreate::aIsAxisAlign;

MObject DPMotionCreate::aTargetType;
MObject DPMotionCreate::aTargetAxis;
MObject DPMotionCreate::aTargetVec;
MObject DPMotionCreate::aTargetAtttribute;

MObject DPMotionCreate::aIsUseUpVector;
MObject DPMotionCreate::aUpType;
MObject DPMotionCreate::aUpAxis;
MObject DPMotionCreate::aUpVec;
MObject DPMotionCreate::aUpAtttribute;

MObject DPMotionCreate::outDP;

void* DPMotionCreate::creator() {
    return ::dommy::maya::createObject("DPMotionCreate", [&]() {
        return new DPMotionCreate();
    });
}

MStatus DPMotionCreate::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (!(plug == outDP))
        return MS::kUnknownParameter;

#pragma mark inDPの取得

    MDataHandle inputHandle = data.inputValue(inDP, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DPData* inputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, inputDPData));
    if (!inputDPData || !inputDPData->hasValidPointTopology())
        return MS::kInvalidParameter;
    const std::size_t ptCount = inputDPData->numberOfPoints();
    if (ptCount > dommy::generation::kMaxGeneratedTransformCount)
        return MS::kInvalidParameter;
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(
        *inputDPData, 2U * sizeof(DPoint) + 2U * sizeof(double) + sizeof(MMatrix) +
                          2U * sizeof(MVector) + sizeof(MStatus), workspace);

    const DPointList* sourcePoints = inputDPData->getPositionPtrConst();
    if (!sourcePoints || sourcePoints->size() != ptCount) return MS::kFailure;
    DPointList stagedPoints(*sourcePoints);

#pragma mark get offset data

    MDataHandle aOffsetTypeHandle = data.inputValue(aOffsetType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int offsetypeVal = aOffsetTypeHandle.asInt();
    MDataHandle aStretchTypeHandle = data.inputValue(aStretchType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int stretchtypeVal = aStretchTypeHandle.asInt();

    MDoubleArray offsetArray;
    MDoubleArray stretchArray;

    DOMMY_RETURN_IF_FAILURE(getTimeOffsetAndStretch(
        *inputDPData, data, thisMObject(), aOffsetAttribute, aStretchAttribute, aInOffsetArray,
        aInStretchArray, offsetypeVal, stretchtypeVal, offsetArray, stretchArray));

    MDataHandle aGlobalOffsetHandle = data.inputValue(aGlobalOffset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    double globalOffsetVal = aGlobalOffsetHandle.asDouble();
    if (!std::isfinite(globalOffsetVal)) {
        return MS::kInvalidParameter;
    }

#pragma mark get animation matrix

    MPlug animMatPlug(thisMObject(), aAnimationMatrix);

    DMatrixArray newMatArray(ptCount, MMatrix());

    const bool matrixConnected = animMatPlug.isConnected(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (matrixConnected) {

        MTime currentTime;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));

        for (std::size_t i = 0U; i < ptCount; ++i) {
            double sampleTimeValue = 0.0;
            if (!dommy::maya::computeSampleTime(currentTime.value(), offsetArray[i],
                                                stretchArray[i], globalOffsetVal,
                                                sampleTimeValue)) {
                return MS::kInvalidParameter;
            }
            const MTime sampleTime(sampleTimeValue, currentTime.unit());
            stat = dommy::maya::sampleMatrixPlugAtTime(animMatPlug, sampleTime, newMatArray[i]);
            if (!stat)
                return stat;
        }
    }

#pragma mark motion bind

    MDataHandle aIsAxisAlignHandle = data.inputValue(aIsAxisAlign, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool isAxisAlign = aIsAxisAlignHandle.asBool();

    MDataHandle aTargetTypeHandle = data.inputValue(aTargetType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short targetType = aTargetTypeHandle.asShort();
    MDataHandle aTargetAxisHandle = data.inputValue(aTargetAxis, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MVector targetAxis = aTargetAxisHandle.asVector();
    MDataHandle aTargetVecHandle = data.inputValue(aTargetVec, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MVector targetVec = aTargetVecHandle.asVector();
    MDataHandle aTargetAtttributeHandle = data.inputValue(aTargetAtttribute, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string targetAttrName = aTargetAtttributeHandle.asString().asChar();

    MDataHandle aUpTypeHandle = data.inputValue(aUpType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short upType = aUpTypeHandle.asShort();
    MDataHandle aUpAxisHandle = data.inputValue(aUpAxis, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MVector upAxisVec = aUpAxisHandle.asVector();
    MDataHandle aUpVecHandle = data.inputValue(aUpVec, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const MVector upVecVec = aUpVecHandle.asVector();
    MDataHandle aUpAtttributeHandle = data.inputValue(aUpAtttribute, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string upAttrName = aUpAtttributeHandle.asString().asChar();

    MDataHandle aIsUseUpVectorHandle = data.inputValue(aIsUseUpVector, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool isUseUpVector = aIsUseUpVectorHandle.asBool();

    MVectorArray targetVecArray;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        ptCount,
        [&](unsigned int, MVector& element) -> MStatus {
            element = targetVec;
            return dommy::maya::isFiniteVector(element) ? MStatus(MS::kSuccess)
                                                        : MStatus(MS::kInvalidParameter);
        },
        targetVecArray));
    if (targetType == 1 && inputDPData->isExistAttr(targetAttrName)) {
        getMVectorArrayFromAttribute(*inputDPData, targetAttrName, targetVecArray);
    }

    MVectorArray upVecArray;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        ptCount,
        [&](unsigned int, MVector& element) -> MStatus {
            element = upVecVec;
            return dommy::maya::isFiniteVector(element) ? MStatus(MS::kSuccess)
                                                        : MStatus(MS::kInvalidParameter);
        },
        upVecArray));
    if (upType == 1 && inputDPData->isExistAttr(upAttrName)) {
        getMVectorArrayFromAttribute(*inputDPData, upAttrName, upVecArray);
    }

    if (targetType < 0 || targetType > 1 || upType < 0 || upType > 1 ||
        !dommy::maya::isFiniteVector(targetAxis) || !dommy::maya::isFiniteVector(upAxisVec))
        return MS::kInvalidParameter;
    if (targetVecArray.length() != ptCount || upVecArray.length() != ptCount)
        return MS::kFailure;

    // These arrays outlive the synchronous parallel_for. Each worker
    // writes only its own point/status, and task copies stay small.
    const MVectorArray& targetValues = targetVecArray;
    const MVectorArray& upValues = upVecArray;
    const DMatrixArray& matrices = newMatArray;
    std::vector<MStatus> pointStatuses(ptCount, MS::kSuccess);
    size_t m = DGetGlobalProcessNum(ptCount);

    tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, ptCount, m);
    tbb::parallel_for(range, [&stagedPoints, &pointStatuses, &matrices, &targetValues, &upValues,
                              isAxisAlign, isUseUpVector, targetAxis,
                              upAxisVec](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); i++) {

            pointStatuses[i] = [&]() -> MStatus {
                MTransformationMatrix transMat(matrices[i]);
                MStatus transformStatus;
                const MVector translation =
                    transMat.getTranslation(MSpace::kWorld, &transformStatus);
                if (!transformStatus)
                    return transformStatus;

                MPoint currentPt(stagedPoints[i][0], stagedPoints[i][1], stagedPoints[i][2]);

                MVector transedPos;

                if (isAxisAlign) {
                    MTransformationMatrix rotMat;

                    MQuaternion baseQuat;
                    baseQuat = targetAxis.rotateTo(targetValues[i]);

                    transformStatus = rotMat.addRotationQuaternion(
                        baseQuat.x, baseQuat.y, baseQuat.z, baseQuat.w, MSpace::kWorld);
                    if (!transformStatus)
                        return transformStatus;

                    if (isUseUpVector) {
                        MQuaternion upQuat;
                        MVector upPosVec = upAxisVec.rotateBy(baseQuat);
                        upQuat = upPosVec.rotateTo(upValues[i]);
                        transformStatus = rotMat.addRotationQuaternion(upQuat.x, upQuat.y, upQuat.z,
                                                                       upQuat.w, MSpace::kWorld);
                        if (!transformStatus)
                            return transformStatus;
                    }

                    transedPos = currentPt + translation * rotMat.asMatrix();
                } else {

                    transedPos = currentPt + translation;
                }

                if (!dommy::maya::isFloatRepresentableVector(transedPos))
                    return MS::kInvalidParameter;
                stagedPoints[i] = DPoint(transedPos[0], transedPos[1], transedPos[2]);
                return MS::kSuccess;
            }();
        }
    });
    for (const MStatus& pointStatus : pointStatuses) {
        DOMMY_RETURN_IF_FAILURE(pointStatus);
    }
#pragma mark Create output
    MObject outputDataObject;
    DPData* outputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DPData>(
        DPData::id, outputDataObject, outputDPData));
    if (!outputDPData || stagedPoints.size() != ptCount) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputDPData->copyRepeatedFrom(*inputDPData, 1U,
        [&stagedPoints](DPoint& point, std::size_t, std::size_t index) -> MStatus {
            point = stagedPoints[index];
            return MS::kSuccess;
        }));
    MDataHandle output = data.outputValue(outDP, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outputDPData));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPMotionCreate")

MStatus DPMotionCreate::initialize() {

    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

#pragma mark input attribute
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnMatrixAttribute matAttr;
    aAnimationMatrix =
        matAttr.create("AnimationMatrix", "amat", MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimationMatrix));

#pragma mark time

    aGlobalOffset = nAttr.create("GlobalOffset", "gof", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalOffset));

    MFnEnumAttribute enumFn;
    aOffsetType = enumFn.create("OffsetType", "offt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Input Data", 2));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetType));

    MObject offsetAttrName = fnStringData.create("offset", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOffsetAttribute =
        tAttr.create("OffsetAttribute", "offattr", MFnData::kString, offsetAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetAttribute));

    aInOffsetArray = gAttr.create("InOffsetArray", "inoff", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInOffsetArray));

    aStretchType = enumFn.create("StretchType", "strt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Input Data", 2));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStretchType));

    MObject strechAttrName = fnStringData.create("stretch", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aStretchAttribute =
        tAttr.create("StretchAttribute", "strattr", MFnData::kString, strechAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStretchAttribute));

    aInStretchArray = gAttr.create("InStretchArray", "instr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInStretchArray));

#pragma mark axis
    aIsAxisAlign = nAttr.create("IsAxisAlign", "iaa", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAxisAlign));

    MFnEnumAttribute tarEnumAttr;
    aTargetType = tarEnumAttr.create("TargetType", "tart", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tarEnumAttr.addField("Target Vector", 0));
    DOMMY_RETURN_IF_FAILURE(tarEnumAttr.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(tarEnumAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetType));

    aTargetAxis = nAttr.create("TargetAxis", "tarax", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetAxis));

    aTargetVec = nAttr.create("TargetVec", "tarvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetVec));

    MObject emptyStr = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTargetAtttribute =
        tAttr.create("TargetAtttribute", "tattr", MFnData::kString, emptyStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetAtttribute));

    aIsUseUpVector = nAttr.create("IsUseUpVector", "iuuv", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsUseUpVector));

    MFnEnumAttribute upEnumAttr;
    aUpType = upEnumAttr.create("UpType", "upt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(upEnumAttr.addField("Up Vector", 0));
    DOMMY_RETURN_IF_FAILURE(upEnumAttr.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(upEnumAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpType));

    aUpAxis = nAttr.create("UpAxis", "upax", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpAxis));

    aUpVec = nAttr.create("UpVec", "upvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpVec));

    aUpAtttribute = tAttr.create("UpAtttribute", "uattr", MFnData::kString, emptyStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpAtttribute));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAnimationMatrix, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGlobalOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInOffsetArray, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInStretchArray, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAxisAlign, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetAxis, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetVec, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetAtttribute, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsUseUpVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpAxis, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpVec, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpAtttribute, outDP));

    return MS::kSuccess;
}
