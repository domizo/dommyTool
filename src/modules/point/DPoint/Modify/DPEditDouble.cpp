//
//  DPEditDouble.cpp
//  dommyTool
//
//  Created by yuzo on 2016/03/13.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPEditDouble.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaCheckedArray.h"

#include "DMayaHeaders.h"

#include "DPointCommon.h"
#include "dpoint_ids.h"

#include <cstddef>
#include <vector>

const MTypeId DPEditDouble::typeId(ID_DP_EDIT_DOUBLE);

MObject DPEditDouble::inputDataArray;

MObject DPEditDouble::isUseExpression;
MObject DPEditDouble::expression;
MObject DPEditDouble::exValue;

MObject DPEditDouble::editDataArray;
MObject DPEditDouble::dataIndex;
MObject DPEditDouble::dataValue;

MObject DPEditDouble::outDataArray;

void* DPEditDouble::creator() {
    return ::dommy::maya::createObject("DPEditDouble", [&]() {
        return new DPEditDouble();
    });
}

MStatus DPEditDouble::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outDataArray) {

        // get Attribute
        const MDataHandle expressionModeHandle = block.inputValue(isUseExpression, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isUseExpressionVal = expressionModeHandle.asBool();

        MFloatArray inDataArray;
        DOMMY_RETURN_IF_FAILURE(getFloatArrayFromData(block, inputDataArray, inDataArray));

        if (inDataArray.length() == 0) {
            MGlobal::displayError("DPEditDouble: input data array is empty.");
            return MStatus::kFailure;
        }

        MFloatArray newDoubleDataArray;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(inDataArray, newDoubleDataArray));

        if (isUseExpressionVal) {
            const MDataHandle expressionHandle = block.inputValue(expression, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MString dataExpression = expressionHandle.asString();

            bool reverse = false;
            std::vector<std::size_t> editIndices;
            if (!dommy::component::parseIndexPattern(
                    dataExpression.asChar(), static_cast<std::size_t>(newDoubleDataArray.length()),
                    dommy::generation::kMaxGeneratedPointCount, reverse, editIndices) ||
                reverse) {
                MGlobal::displayError("DPEditDouble: expression must contain non-negative "
                                      "indices and end-exclusive ranges.");
                return MS::kInvalidParameter;
            }

            const MDataHandle expressionValueHandle = block.inputValue(exValue, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double expressionValue = expressionValueHandle.asDouble();
            float checkedExpressionValue = 0.0f;
            if (!dommy::numeric::checkedFloatCast(expressionValue, checkedExpressionValue)) {
                MGlobal::displayError("DPEditDouble: expression value must be a finite "
                                      "float-range number.");
                return MS::kInvalidParameter;
            }

            for (std::size_t index = 0U; index < editIndices.size(); ++index) {
                DOMMY_RETURN_IF_FAILURE(newDoubleDataArray.set(
                    checkedExpressionValue, static_cast<unsigned int>(editIndices[index])));
            }
        }

        // editArray
        MArrayDataHandle customDataArrayHandle = block.inputArrayValue(editDataArray, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const unsigned int count = customDataArrayHandle.elementCount(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        for (unsigned int editDataObject = 0U; editDataObject < count; ++editDataObject) {
            DOMMY_RETURN_IF_FAILURE(customDataArrayHandle.jumpToArrayElement(editDataObject));
            MDataHandle customHandle = customDataArrayHandle.inputValue(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const int dataIndexValue = customHandle.child(dataIndex).asInt();
            const double editValue = customHandle.child(DPEditDouble::dataValue).asDouble();

            if (dataIndexValue < 0 ||
                static_cast<unsigned int>(dataIndexValue) >= newDoubleDataArray.length()) {
                MGlobal::displayError("DPEditDouble: editDataArray contains an out-of-range "
                                      "data index.");
                return MS::kInvalidParameter;
            }
            float checkedDataValue = 0.0f;
            if (!dommy::numeric::checkedFloatCast(editValue, checkedDataValue)) {
                MGlobal::displayError("DPEditDouble: edit values must be finite float-range "
                                      "numbers.");
                return MS::kInvalidParameter;
            }
            DOMMY_RETURN_IF_FAILURE(newDoubleDataArray.set(
                checkedDataValue, static_cast<unsigned int>(dataIndexValue)));
        }

#pragma mark Create output point array
        MDataHandle outputData = block.outputValue(outDataArray, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        MFnFloatArrayData uData;
        MObject outplug = uData.create(newDoubleDataArray, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        DOMMY_RETURN_IF_FAILURE(outputData.set(outplug));

        outputData.setClean();
        stat = block.setClean(plug);

        if (!stat) {
            stat.perror("DPEditDouble::compute setClean");
            return stat;
        }
    } else {
        return MS::kUnknownParameter;
    }

    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DPEditDouble")

MStatus DPEditDouble::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inputDataArray = gAttr.create("inputDataArray", "ida", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputDataArray));

    isUseExpression =
        nAttr.create("isUseExpression", "iue", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isUseExpression));

    MFnStringData expFnStringData;
    MObject expDefaultObject = expFnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    expression = tAttr.create("expression", "exp", MFnData::kString, expDefaultObject, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsedAsFilename(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(expression));

    exValue = nAttr.create("exValue", "exv", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(exValue));

    dataIndex = nAttr.create("dataIndex", "din", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dataIndex));

    dataValue = nAttr.create("dataValue", "dval", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dataValue));

    MFnCompoundAttribute cmpAttr;
    editDataArray = cmpAttr.create("editDataArray", "eda", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(dataIndex));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(dataValue));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(editDataArray));

    outDataArray =
        tAttr.create("outDataArray", "out", MFnData::kFloatArray, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputDataArray, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isUseExpression, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(expression, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(exValue, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(dataIndex, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(dataValue, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(editDataArray, outDataArray));
    return MS::kSuccess;
}
