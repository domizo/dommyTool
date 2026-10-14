//
//  DPAnimMatrixGen.cpp
//  dommyTool
//
//  Created by yuzo on 2016/03/02.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAnimMatrix.h"
#include "common/utility/DStatusOperation.h"
#include "common/object/DNodeOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DTimeSampling.h"
#include "common/utility/DPointPayloadBudget.h"

#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "dpoint_ids.h"
#include "common/utility/DAttributeLookup.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include <cmath>

const MTypeId DPAnimMatrix::typeId(ID_DP_ANIM_MATRIX);

// Attribute
MObject DPAnimMatrix::inDP;

MObject DPAnimMatrix::aAnimAttributeName;

MObject DPAnimMatrix::animationMatrix;
MObject DPAnimMatrix::globalOffset;

MObject DPAnimMatrix::aOffsetType;
MObject DPAnimMatrix::aOffsetAttribute;
MObject DPAnimMatrix::aInOffsetArray;

MObject DPAnimMatrix::aStretchType;
MObject DPAnimMatrix::aStretchAttribute;
MObject DPAnimMatrix::aInStretchArray;

MObject DPAnimMatrix::aCurrentTime;

MObject DPAnimMatrix::outDP;

void* DPAnimMatrix::creator() {
    return ::dommy::maya::createObject("DPAnimMatrix", [&]() {
        return new DPAnimMatrix();
    });
}

MStatus DPAnimMatrix::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDP))
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle timeHandle = data.inputValue(aCurrentTime, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTime currentTime = timeHandle.asTime();
    if (!std::isfinite(currentTime.value()))
        return MS::kInvalidParameter;

    MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* inputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, inputDPData));
    if (!inputDPData || !inputDPData->hasValidPointTopology())
        return MS::kInvalidParameter;
    const std::size_t pointCount = inputDPData->numberOfPoints();
    if (pointCount > dommy::generation::kMaxGeneratedTransformCount)
        return MS::kInvalidParameter;

    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), workspace);
    std::string matrixAttribute;
    std::vector<MMatrix> matrices;

    // Preserve the historical empty-point pass-through.
    if (pointCount != 0U) {
        const MDataHandle offsetHandle = data.inputValue(globalOffset, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double globalOffsetValue = offsetHandle.asDouble();
        if (!std::isfinite(globalOffsetValue))
            return MS::kInvalidParameter;
        const MDataHandle nameHandle = data.inputValue(aAnimAttributeName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        matrixAttribute = nameHandle.asString().asChar();
        if (!dommy::attribute::validAttributeName(matrixAttribute))
            return MS::kInvalidParameter;
        // Retained metadata, timing scratch, sampled matrices and the new
        // attribute share one allowance; admit them before allocating arrays.
        workspace.add(pointCount, 2U * sizeof(float) + sizeof(MMatrix));
        dommy::point::preflightGeneratedPointAttribute(
            pointCount, matrixAttribute, sizeof(MMatrix), workspace);

        // This node historically uses float-backed timing, exact-length plug
        // arrays, and default values for missing/non-scalar attributes. Keep
        // those contracts while checking every handle before conversion.
        const auto readTiming = [&](const MObject& modeAttribute, const MObject& nameAttribute,
                                    const MObject& arrayAttribute, float defaultValue,
                                    DScalarDataList& values) -> MStatus {
            MStatus inputStatus;
            const MDataHandle modeHandle = data.inputValue(modeAttribute, &inputStatus);
            if (!inputStatus)
                return inputStatus;
            const int mode = modeHandle.asInt();
            if (mode < 0 || mode > 2)
                return MS::kInvalidParameter;
            values.assign(pointCount, defaultValue);
            if (mode == 1) {
                const MDataHandle attributeHandle = data.inputValue(nameAttribute, &inputStatus);
                if (!inputStatus)
                    return inputStatus;
                getDScalarArrayFromAttribute(*inputDPData, attributeHandle.asString().asChar(),
                                             values);
            } else if (mode == 2) {
                const bool connected = MPlug(thisMObject(), arrayAttribute).isConnected(&inputStatus);
                if (!inputStatus)
                    return inputStatus;
                if (connected) {
                    MDataHandle arrayHandle = data.inputValue(arrayAttribute, &inputStatus);
                    if (!inputStatus)
                        return inputStatus;
                    inputStatus = getDScalarArrayFromPlug(arrayHandle, values);
                    if (!inputStatus)
                        return inputStatus;
                }
            }
            if (values.size() != pointCount)
                return MS::kFailure;
            for (float value : values) {
                if (!std::isfinite(value))
                    return MS::kInvalidParameter;
            }
            return MS::kSuccess;
        };
        DScalarDataList offsets, stretches;
        DOMMY_RETURN_IF_FAILURE(
            readTiming(aOffsetType, aOffsetAttribute, aInOffsetArray, 0.0f, offsets));
        DOMMY_RETURN_IF_FAILURE(
            readTiming(aStretchType, aStretchAttribute, aInStretchArray, 1.0f, stretches));

        matrices.assign(pointCount, MMatrix());
        const MPlug matrixPlug(thisMObject(), animationMatrix);
        const bool matrixConnected = matrixPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (matrixConnected) {
            for (std::size_t index = 0U; index < pointCount; ++index) {
                double sampleValue = 0.0;
                if (!dommy::maya::computeSampleTime(currentTime.value(), offsets[index],
                                                    stretches[index], globalOffsetValue,
                                                    sampleValue)) {
                    return MS::kInvalidParameter;
                }
                const MTime sampleTime(sampleValue, currentTime.unit());
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::sampleMatrixPlugAtTime(matrixPlug, sampleTime, matrices[index]));
            }
        }
    }

    MObject outputDataObject;
    DPData* outputDPData = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginDataObject<DPData>(DPData::id, outputDataObject, outputDPData));
    if (!outputDPData)
        return MS::kFailure;
    outputDPData->setDPData(*inputDPData);
    if (pointCount != 0U) {
        checkDPAttribute(matrixAttribute, *outputDPData, true);
        outputDPData->addAttrWithValues(matrixAttribute, matrices);
    }

    MDataHandle output = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outputDPData));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAnimMatrix")

MStatus DPAnimMatrix::initialize() {

    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

#pragma mark input attribute
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnStringData fnStringData;
    MObject attrName = fnStringData.create("amat", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAnimAttributeName =
        tAttr.create("AnimAttributeName", "aniattr", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimAttributeName));

    MFnMatrixAttribute matAttr;
    animationMatrix = matAttr.create("animationMatrix", "amat", MFnMatrixAttribute::kDouble, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(animationMatrix));

    globalOffset = nAttr.create("globalOffset", "gof", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(globalOffset));

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

    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInStretchArray));

    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create("CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAnimAttributeName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(animationMatrix, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(globalOffset, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInOffsetArray, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStretchAttribute, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInStretchArray, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, outDP));
    return MS::kSuccess;
}
