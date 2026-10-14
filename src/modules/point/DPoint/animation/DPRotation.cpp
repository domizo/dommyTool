//
//  DPRotation.cpp
//  dommyTool
//
//  Created by yuzo on 11/21/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPRotation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DTimeSampling.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DAttributePublication.h"

#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "dpoint_ids.h"

#include "DNodeOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "dommyMath.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include "common/utility/DEvaluationContext.h"

#include <cmath>
#include <memory>
const MTypeId DPRotation::typeId(ID_DP_ROTATION);

// Attribute
MObject DPRotation::inDP;

MObject DPRotation::aRotateType;

// input
MObject DPRotation::aForwardVector;
MObject DPRotation::aForwardVectorAttribute;

MObject DPRotation::aIsUseUpVector;
MObject DPRotation::aUpVector;
MObject DPRotation::aUpVectorAttribute;

MObject DPRotation::aRotationMatrix;

/// space
MObject DPRotation::aSpaceForwardAxis;
MObject DPRotation::aSpaceUpAxis;

// option
MObject DPRotation::aRotateSpace;
MObject DPRotation::aRotateOrder;

// value
MObject DPRotation::aRotate;
MObject DPRotation::aRotateAttribute;

// output
MObject DPRotation::aOutputType;
MObject DPRotation::aOutFowardAttribute;
MObject DPRotation::aOutUpAttribute;
MObject DPRotation::aOutMatrixAttribute;

// time
MObject DPRotation::aGlobalOffset;
MObject DPRotation::aOffsetType;
MObject DPRotation::aOffsetAttribute;
MObject DPRotation::aInOffsetArray;
MObject DPRotation::aStretchType;
MObject DPRotation::aStretchAttribute;
MObject DPRotation::aInStretchArray;

MObject DPRotation::outDP;

void* DPRotation::creator() {
    return ::dommy::maya::createObject("DPRotation", [&]() {
        return new DPRotation();
    });
}

MStatus DPRotation::compute(const MPlug& plug, MDataBlock& data) try {
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
    dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);
    short outputType = 0;
    std::string fwAttrName, upAttrName, matAttrName;
    if (ptCount != 0U) {
        const MDataHandle outputTypeHandle = data.inputValue(aOutputType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        outputType = outputTypeHandle.asShort();
        if (outputType < 0 || outputType > 2)
            return MS::kInvalidParameter;
        const MDataHandle forwardNameHandle = data.inputValue(aOutFowardAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        fwAttrName = forwardNameHandle.asString().asChar();
        const MDataHandle upNameHandle = data.inputValue(aOutUpAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        upAttrName = upNameHandle.asString().asChar();
        const MDataHandle matrixNameHandle = data.inputValue(aOutMatrixAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        matAttrName = matrixNameHandle.asString().asChar();
        if (outputType == 1 && !fwAttrName.empty() && fwAttrName == upAttrName)
            return MS::kInvalidParameter;
        const auto validOutputName = [](const std::string& name) {
            return dommy::attribute::validAttributeName(name) &&
                   name.size() <= dommy::generation::kMaxGeneratedStringByteCount;
        };
        if ((outputType < 2 && !fwAttrName.empty() && !validOutputName(fwAttrName)) ||
            (outputType == 1 && !upAttrName.empty() && !validOutputName(upAttrName)) ||
            (outputType == 2 && !validOutputName(matAttrName)))
            return MS::kInvalidParameter;
        workspace.add(ptCount, 3U * sizeof(MVector) + 2U * sizeof(double) +
                                   sizeof(MMatrix) + 2U * sizeof(Imath::V3d));
        if (outputType == 2) workspace.add(ptCount, sizeof(MMatrix));
        const auto admitAttribute = [&](const std::string& name, std::size_t width) {
            if (name.empty()) return; // Historical empty-name suppression.
            dommy::point::preflightGeneratedPointAttribute(ptCount, name, width, workspace);
        };
        if (outputType < 2) admitAttribute(fwAttrName, sizeof(Imath::V3d));
        if (outputType == 1) admitAttribute(upAttrName, sizeof(Imath::V3d));
        if (outputType == 2) admitAttribute(matAttrName, sizeof(MMatrix));
    }

#pragma mark outDPの作成

    MObject outputDataObject;
    DPData* outputDPData = NULL;
    stat = dommy::maya::createPluginDataObject<DPData>(DPData::id, outputDataObject, outputDPData);
    if (!stat)
        return stat;

    if (!outputDPData)
        return MS::kFailure;

    outputDPData->setDPData(*inputDPData);

    if (ptCount) {

#pragma mark attributeの取得
        MDataHandle aRotateTypeHandle = data.inputValue(aRotateType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rotTypeVal = aRotateTypeHandle.asInt();

        MDataHandle aForwardVectorHandle = data.inputValue(aForwardVector, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MVector forwardVec = aForwardVectorHandle.asVector();
        MDataHandle aUpVectorHandle = data.inputValue(aUpVector, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MVector upVec = aUpVectorHandle.asVector();
        MDataHandle aForwardVectorAttributeHandle = data.inputValue(aForwardVectorAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string forwardAttrNameStr = aForwardVectorAttributeHandle.asString().asChar();
        MDataHandle aUpVectorAttributeHandle = data.inputValue(aUpVectorAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string upAttrNameStr = aUpVectorAttributeHandle.asString().asChar();

        MDataHandle aIsUseUpVectorHandle = data.inputValue(aIsUseUpVector, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isUseUpVec = aIsUseUpVectorHandle.asBool();

        MPlug animMatPlug(thisMObject(), aRotationMatrix);
        MMatrix rotMat;
        const bool matrixConnected = animMatPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (matrixConnected) {
            MDataHandle aRotationMatrixHandle = data.inputValue(aRotationMatrix, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            rotMat = aRotationMatrixHandle.asMatrix();
        }

        MVectorArray fVectorArray;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            ptCount,
            [&](unsigned int, MVector& element) -> MStatus {
                element = forwardVec;
                return dommy::maya::isFiniteVector(element) ? MStatus(MS::kSuccess)
                                                            : MStatus(MS::kInvalidParameter);
            },
            fVectorArray));
        if (inputDPData->isExistAttr(forwardAttrNameStr)) {
            getMVectorArrayFromAttribute(*inputDPData, forwardAttrNameStr, fVectorArray);
        }

        MVectorArray upVectorArray;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            ptCount,
            [&](unsigned int, MVector& element) -> MStatus {
                element = upVec;
                return dommy::maya::isFiniteVector(element) ? MStatus(MS::kSuccess)
                                                            : MStatus(MS::kInvalidParameter);
            },
            upVectorArray));
        if (inputDPData->isExistAttr(upAttrNameStr)) {
            getMVectorArrayFromAttribute(*inputDPData, upAttrNameStr, upVectorArray);
        }

#pragma mark get rotate option
        MDataHandle aRotateSpaceHandle = data.inputValue(aRotateSpace, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short rotateSpaceType = aRotateSpaceHandle.asShort();
        MDataHandle aRotateOrderHandle = data.inputValue(aRotateOrder, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short rotateOrderType = aRotateOrderHandle.asShort();
        MDataHandle aRotateHandle = data.inputValue(aRotate, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double3& rotateVal = aRotateHandle.asDouble3();
        MDataHandle aRotateAttributeHandle = data.inputValue(aRotateAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string rotAttrNameStr = aRotateAttributeHandle.asString().asChar();

        MVectorArray rotValueArray;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            ptCount,
            [&](unsigned int, MVector& element) -> MStatus {
                element = MVector(rotateVal);
                return dommy::maya::isFiniteVector(element) ? MStatus(MS::kSuccess)
                                                            : MStatus(MS::kInvalidParameter);
            },
            rotValueArray));
        if (inputDPData->isExistAttr(rotAttrNameStr)) {
            getMVectorArrayFromAttribute(*inputDPData, rotAttrNameStr, rotValueArray);
        }

        MTransformationMatrix::RotationOrder rotOrder;
        switch (rotateOrderType) {
        case ROTATE_ORDER_XYZ:
            rotOrder = MTransformationMatrix::kXYZ;
            break;
        case ROTATE_ORDER_YZX:
            rotOrder = MTransformationMatrix::kYZX;
            break;
        case ROTATE_ORDER_ZXY:
            rotOrder = MTransformationMatrix::kZXY;
            break;
        case ROTATE_ORDER_XZY:
            rotOrder = MTransformationMatrix::kXZY;
            break;
        case ROTATE_ORDER_YXZ:
            rotOrder = MTransformationMatrix::kYXZ;
            break;
        case ROTATE_ORDER_ZYX:
            rotOrder = MTransformationMatrix::kZYX;
            break;
        default:
            return MS::kInvalidParameter;
        }

#pragma mark time range value

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

        MPlug animRotMatPlug(thisMObject(), aRotationMatrix);
        MPlug animRotValuePlug(thisMObject(), aRotate);

        MTime currentTime;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));

        MMatrixArray rotMatArray;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            ptCount,
            [](unsigned int, MMatrix& element) -> MStatus {
                element = MMatrix();
                return MS::kSuccess;
            },
            rotMatArray));
        if (rotTypeVal < 0 || rotTypeVal > 1 || rotateSpaceType < 0 || rotateSpaceType > 1)
            return MS::kInvalidParameter;
        if (fVectorArray.length() != ptCount || upVectorArray.length() != ptCount ||
            rotValueArray.length() != ptCount || rotMatArray.length() != ptCount)
            return MS::kFailure;

        if (rotTypeVal == 0) {

            // A custom rotation attribute has already populated rotValueArray.
            if (!inputDPData->isExistAttr(rotAttrNameStr)) {
                bool isChildConnected = false;
                const unsigned int childCount = animRotValuePlug.numChildren(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                for (unsigned int i = 0U; i < childCount; i++) {
                    const MPlug child = animRotValuePlug.child(i, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const bool childConnected = child.isConnected(&stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    if (childConnected) {
                        isChildConnected = true;
                        break;
                    }
                }

                const bool rotationConnected = animRotValuePlug.isConnected(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (rotationConnected || isChildConnected) {

                    for (std::size_t i = 0U; i < ptCount; ++i) {
                        double sampleTimeValue = 0.0;
                        if (!dommy::maya::computeSampleTime(currentTime.value(), offsetArray[i],
                                                            stretchArray[i], globalOffsetVal,
                                                            sampleTimeValue)) {
                            return MS::kInvalidParameter;
                        }
                        const MTime sampleTime(sampleTimeValue, currentTime.unit());
                        stat = dommy::maya::sampleVectorPlugAtTime(animRotValuePlug, sampleTime,
                                                                   rotValueArray[i]);
                        if (!stat)
                            return stat;
                    }
                }
            }

        } else if (rotTypeVal == 1) {

            for (std::size_t i = 0U; i < ptCount; ++i) {

                if (matrixConnected) {
                    double sampleTimeValue = 0.0;
                    if (!dommy::maya::computeSampleTime(currentTime.value(), offsetArray[i],
                                                        stretchArray[i], globalOffsetVal,
                                                        sampleTimeValue)) {
                        return MS::kInvalidParameter;
                    }
                    const MTime sampleTime(sampleTimeValue, currentTime.unit());
                    MMatrix sampledMatrix;
                    stat = dommy::maya::sampleMatrixPlugAtTime(animRotMatPlug, sampleTime,
                                                               sampledMatrix);
                    if (!stat)
                        return stat;
                    rotMatArray[static_cast<unsigned int>(i)] = sampledMatrix;
                } else {

                    rotMatArray[static_cast<unsigned int>(i)] = rotMat;
                }
            }
        }

#pragma mark rotate vector
        std::vector<Imath::V3d> outfVecArray(fVectorArray.length(), Imath::V3d(0, 0, 0));
        std::vector<Imath::V3d> outupVecArray(upVectorArray.length(), Imath::V3d(0, 0, 0));
        std::vector<MMatrix> outupMatArray(outputType == 2 ? ptCount : 0, MMatrix());

        MDataHandle aSpaceForwardAxisHandle = data.inputValue(aSpaceForwardAxis, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MVector spaceForwardAxis = aSpaceForwardAxisHandle.asVector();
        MDataHandle aSpaceUpAxisHandle = data.inputValue(aSpaceUpAxis, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MVector spaceUpAxis = aSpaceUpAxisHandle.asVector();
        if (rotateSpaceType == 1 && isUseUpVec) {
            // Normalize without squaring the original magnitudes, so the
            // checked frame builder can also handle very small/large axes.
            if (!dommy::math::normalized3OrZero(
                    spaceForwardAxis.x, spaceForwardAxis.y, spaceForwardAxis.z,
                    spaceForwardAxis.x, spaceForwardAxis.y, spaceForwardAxis.z) ||
                !dommy::math::normalized3OrZero(
                    spaceUpAxis.x, spaceUpAxis.y, spaceUpAxis.z,
                    spaceUpAxis.x, spaceUpAxis.y, spaceUpAxis.z)) {
                return MS::kInvalidParameter;
            }
        } else {
            spaceForwardAxis = spaceForwardAxis.normal();
            spaceUpAxis = spaceUpAxis.normal();
        }

        for (uint i = 0; i < fVectorArray.length(); i++) {

            if (rotTypeVal == 0) {
#pragma mark euler
                double3 rotVal = {dommy::math::degreesToRadians(rotValueArray[i][0]),
                                  dommy::math::degreesToRadians(rotValueArray[i][1]),
                                  dommy::math::degreesToRadians(rotValueArray[i][2])};

                if (rotateSpaceType == 0) {

                    MVector rotatedFVec = fVectorArray[i].normal().rotateBy(rotVal, rotOrder);
                    outfVecArray[i] = Imath::V3d(rotatedFVec[0], rotatedFVec[1], rotatedFVec[2]);

                    MVector rotatedUpVec = upVectorArray[i].normal().rotateBy(rotVal, rotOrder);
                    outupVecArray[i] =
                        Imath::V3d(rotatedUpVec[0], rotatedUpVec[1], rotatedUpVec[2]);

                    if (!outupMatArray.empty()) {
                        MTransformationMatrix outTrans;
                        DOMMY_RETURN_IF_FAILURE(
                            outTrans.addRotation(rotVal, rotOrder, MSpace::kTransform));
                        outupMatArray.at(i) = outTrans.asMatrix();
                    }

                } else {
                    if (isUseUpVec) {

                        MMatrix alignment;
                        DOMMY_RETURN_IF_FAILURE(getAlignSpaceMatrix(
                            fVectorArray[i], upVectorArray[i], spaceForwardAxis,
                            spaceUpAxis, alignment));

                        MVector rotatedFVec = spaceForwardAxis.rotateBy(rotVal, rotOrder);
                        rotatedFVec = rotatedFVec * alignment;
                        outfVecArray[i] =
                            Imath::V3d(rotatedFVec[0], rotatedFVec[1], rotatedFVec[2]);

                        MVector rotatedUpVec = spaceUpAxis.rotateBy(rotVal, rotOrder);
                        rotatedUpVec = rotatedUpVec * alignment;

                        outupVecArray[i] =
                            Imath::V3d(rotatedUpVec[0], rotatedUpVec[1], rotatedUpVec[2]);

                        if (!outupMatArray.empty()) {
                            MTransformationMatrix outTrans;
                            DOMMY_RETURN_IF_FAILURE(
                                outTrans.addRotation(rotVal, rotOrder, MSpace::kTransform));
                            outupMatArray.at(i) = outTrans.asMatrix() * alignment;
                        }

                    } else {

                        MQuaternion baseQuat;
                        baseQuat = spaceForwardAxis.rotateTo(fVectorArray[i].normal());

                        MVector rotatedFVec = spaceForwardAxis.rotateBy(rotVal, rotOrder);
                        rotatedFVec = rotatedFVec.rotateBy(baseQuat);
                        outfVecArray[i] =
                            Imath::V3d(rotatedFVec[0], rotatedFVec[1], rotatedFVec[2]);

                        MVector rotatedUpVec = spaceUpAxis.normal().rotateBy(rotVal, rotOrder);
                        rotatedUpVec = rotatedUpVec.rotateBy(baseQuat);

                        outupVecArray[i] =
                            Imath::V3d(rotatedUpVec[0], rotatedUpVec[1], rotatedUpVec[2]);

                        if (!outupMatArray.empty()) {
                            MTransformationMatrix outTrans;
                            DOMMY_RETURN_IF_FAILURE(
                                outTrans.addRotation(rotVal, rotOrder, MSpace::kTransform));
                            DOMMY_RETURN_IF_FAILURE(
                                outTrans.addRotationQuaternion(baseQuat.x, baseQuat.y, baseQuat.z,
                                                               baseQuat.w, MSpace::kTransform));
                            outupMatArray.at(i) = outTrans.asMatrix();
                        }
                    }
                }

            } else if (rotTypeVal == 1) {

#pragma mark use matrix
                MTransformationMatrix transMat(rotMatArray[i]);
                MMatrix rma = transMat.asRotateMatrix();

                if (rotateSpaceType == 0) {

                    MVector rotatedFVec = fVectorArray[i].normal() * rma;
                    outfVecArray[i] = Imath::V3d(rotatedFVec[0], rotatedFVec[1], rotatedFVec[2]);

                    MVector rotatedUpVec = upVectorArray[i].normal() * rma;
                    outupVecArray[i] =
                        Imath::V3d(rotatedUpVec[0], rotatedUpVec[1], rotatedUpVec[2]);

                    if (!outupMatArray.empty()) {
                        MTransformationMatrix outTrans(rma);

                        outupMatArray.at(i) = outTrans.asMatrix();
                    }

                } else {

                    MVector rotatedFVec = spaceForwardAxis.normal() * rma;
                    MVector rotatedUpVec = spaceUpAxis.normal() * rma;

                    if (isUseUpVec) {
                        MMatrix alignment;
                        DOMMY_RETURN_IF_FAILURE(getAlignSpaceMatrix(
                            fVectorArray[i], upVectorArray[i], spaceForwardAxis,
                            spaceUpAxis, alignment));
                        rotatedFVec = rotatedFVec * alignment;
                        rotatedUpVec = rotatedUpVec * alignment;
                        if (!outupMatArray.empty()) {
                            outupMatArray.at(i) = rma * alignment;
                        }
                    } else {
                        const MQuaternion baseQuat =
                            spaceForwardAxis.rotateTo(fVectorArray[i].normal());
                        rotatedFVec = rotatedFVec.rotateBy(baseQuat);
                        rotatedUpVec = rotatedUpVec.rotateBy(baseQuat);
                        if (!outupMatArray.empty()) {
                            MTransformationMatrix outTrans(rma);
                            DOMMY_RETURN_IF_FAILURE(outTrans.addRotationQuaternion(
                                baseQuat.x, baseQuat.y, baseQuat.z, baseQuat.w,
                                MSpace::kTransform));
                            outupMatArray.at(i) = outTrans.asMatrix();
                        }
                    }
                    outfVecArray[i] = Imath::V3d(rotatedFVec[0], rotatedFVec[1], rotatedFVec[2]);
                    outupVecArray[i] =
                        Imath::V3d(rotatedUpVec[0], rotatedUpVec[1], rotatedUpVec[2]);
                }
            }
        }

#pragma mark output attribute

        if (outputType == 0 || outputType == 1) {

            if (!fwAttrName.empty()) {
                DPAttributeData::Ptr candidate =
                    std::make_shared<DPVec3dAttr>(outfVecArray, Imath::V3d(0.0, 0.0, 0.0));
                if (!dommy::attribute::commitStagedAttribute(*outputDPData, fwAttrName, candidate,
                                                            ptCount)) return MS::kFailure;
            }

            if (outputType == 1) {
                if (!upAttrName.empty()) {
                    DPAttributeData::Ptr candidate =
                        std::make_shared<DPVec3dAttr>(outupVecArray, Imath::V3d(0.0, 0.0, 0.0));
                    if (!dommy::attribute::commitStagedAttribute(*outputDPData, upAttrName,
                                                                candidate, ptCount))
                        return MS::kFailure;
                }
            }

        } else if (outputType == 2) {

            if (matAttrName.empty()) {
                MGlobal::displayError("DPRotation::Failed to create a matrix "
                                      "attribute. Attribute name is empty.");
                return MS::kInvalidParameter;
            } else {

                if (!outupMatArray.empty()) {
                    DPAttributeData::Ptr candidate =
                        std::make_shared<DPMat4Attr>(outupMatArray, MMatrix());
                    if (!dommy::attribute::commitStagedAttribute(*outputDPData, matAttrName,
                                                                candidate, ptCount))
                        return MS::kFailure;
                }
            }
        }
    }

#pragma mark Create output
    MDataHandle output = data.outputValue(outDP, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outputDPData));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPRotation")

MStatus DPRotation::initialize() {

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

    MFnEnumAttribute enumFn;
    aRotateType = enumFn.create("RotateType", "rvt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Euler", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Input Matrix", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateType));

    aForwardVector = nAttr.create("ForwardVector", "fvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForwardVector));

    aUpVector = nAttr.create("UpVector", "uvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpVector));

    MObject rotAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aForwardVectorAttribute =
        tAttr.create("ForwardVectorAttribute", "fvat", MFnData::kString, rotAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForwardVectorAttribute));

    aUpVectorAttribute =
        tAttr.create("UpVectorAttribute", "uvat", MFnData::kString, rotAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUpVectorAttribute));

    aIsUseUpVector = nAttr.create("IsUseUpVector", "iuuv", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsUseUpVector));

    MFnMatrixAttribute matAttr;
    aRotationMatrix = matAttr.create("RotationMatrix", "rmat", MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotationMatrix));

    aSpaceForwardAxis =
        nAttr.create("SpaceForwardAxis", "wfax", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpaceForwardAxis));

    aSpaceUpAxis = nAttr.create("SpaceUpAxis", "wuax", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpaceUpAxis));

    MFnEnumAttribute rotSpaceEnumAttr;
    aRotateSpace = rotSpaceEnumAttr.create("RotateSpace", "rs", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(rotSpaceEnumAttr.addField("World Space", 0));
    DOMMY_RETURN_IF_FAILURE(rotSpaceEnumAttr.addField("Object Space", 1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateSpace));

    MFnEnumAttribute rotEnumAttr;
    aRotateOrder = rotEnumAttr.create("RotateOrder", "ro", ROTATE_ORDER_XYZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xyz", ROTATE_ORDER_XYZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yzx", ROTATE_ORDER_YZX));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zxy", ROTATE_ORDER_ZXY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("xzy", ROTATE_ORDER_XZY));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("yxz", ROTATE_ORDER_YXZ));
    DOMMY_RETURN_IF_FAILURE(rotEnumAttr.addField("zyx", ROTATE_ORDER_ZYX));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateOrder));

    aRotate = nAttr.create("Rotate", "rot", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotate));

    MObject rAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRotateAttribute = tAttr.create("RotateAttribute", "rattr", MFnData::kString, rAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateAttribute));

    MFnEnumAttribute outEnumFn;
    aOutputType = outEnumFn.create("OutputType", "outt", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outEnumFn.addField("Forward Only", 0));
    DOMMY_RETURN_IF_FAILURE(outEnumFn.addField("Forward and Up", 1));
    DOMMY_RETURN_IF_FAILURE(outEnumFn.addField("Matrix", 2));
    DOMMY_RETURN_IF_FAILURE(outEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputType));

    MObject fvecAttrName = fnStringData.create("fwvec", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutFowardAttribute =
        tAttr.create("OutFowardAttribute", "ofattr", MFnData::kString, fvecAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutFowardAttribute));

    MObject upvecAttrName = fnStringData.create("upvec", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutUpAttribute =
        tAttr.create("OutUpAttribute", "oupattr", MFnData::kString, upvecAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutUpAttribute));

    MObject matAttrName = fnStringData.create("rotmat", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOutMatrixAttribute =
        tAttr.create("OutMatrixAttribute", "omattr", MFnData::kString, matAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutMatrixAttribute));

#pragma mark time

    aGlobalOffset = nAttr.create("GlobalOffset", "gof", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalOffset));

    MFnEnumAttribute offsetEnumFn;
    aOffsetType = offsetEnumFn.create("OffsetType", "offt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(offsetEnumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(offsetEnumFn.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(offsetEnumFn.addField("Input Data", 2));
    DOMMY_RETURN_IF_FAILURE(offsetEnumFn.setKeyable(true));
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

#pragma mark output
    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotateType, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aForwardVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aForwardVectorAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUpVectorAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsUseUpVector, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotationMatrix, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpaceForwardAxis, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpaceUpAxis, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotateSpace, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotateOrder, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotate, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotateAttribute, outDP));

    // ouput
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutFowardAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutUpAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutMatrixAttribute, outDP));

    // time
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGlobalOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInOffsetArray, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInStretchArray, outDP));

    return MS::kSuccess;
}
