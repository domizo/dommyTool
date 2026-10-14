//
//  DPStringArrayGen.cpp
//  dommyTool
//
//  Created by yuzo on 6/22/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPStringArrayGen.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "dpoint_ids.h"

#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MStringArray.h>

const MTypeId DPStringArrayGen::typeId(ID_DP_STRING_ARRAY);

MObject DPStringArrayGen::value;
MObject DPStringArrayGen::valueSize;
MObject DPStringArrayGen::outDataArray;

void* DPStringArrayGen::creator() {
    return ::dommy::maya::createObject("DPStringArrayGen", [&]() {
        return new DPStringArrayGen();
    });
}

MStatus DPStringArrayGen::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outDataArray) {
        const MDataHandle sizeHandle = block.inputValue(valueSize, &stat);
        if (!stat)
            return stat;
        const int rawValueSize = sizeHandle.asInt();

        std::size_t valueSizeValue = 0U;
        if (!dommy::generation::checkedCount(
                rawValueSize, dommy::generation::kMaxGeneratedStringCount, valueSizeValue) ||
            valueSizeValue == 0U) {
            MGlobal::displayError("DPStringArrayGen: valueSize is outside the supported range.");
            return MS::kInvalidParameter;
        }

        const MDataHandle stringHandle = block.inputValue(value, &stat);
        if (!stat)
            return stat;
        const MString defaultString = stringHandle.asString();

        int utf8Length = 0;
        defaultString.asUTF8(utf8Length);
        std::size_t checkedLength = 0U;
        std::size_t bytesPerString = 0U;
        std::size_t totalStringBytes = 0U;
        if (!dommy::generation::checkedCount(
                utf8Length, dommy::generation::kMaxGeneratedStringByteCount, checkedLength) ||
            !dommy::generation::checkedSum(checkedLength, 1U,
                                           dommy::generation::kMaxGeneratedStringByteCount,
                                           bytesPerString) ||
            !dommy::generation::checkedProduct(valueSizeValue, bytesPerString,
                                               dommy::generation::kMaxGeneratedStringByteCount,
                                               totalStringBytes)) {
            MGlobal::displayError(
                "DPStringArrayGen: requested string payload exceeds the safety budget.");
            return MS::kInvalidParameter;
        }

        MStringArray newStringArray;
        stat = newStringArray.setLength(static_cast<unsigned int>(valueSizeValue));
        if (!stat)
            return stat;
        if (newStringArray.length() != valueSizeValue)
            return MS::kFailure;
        for (unsigned int index = 0U; index < newStringArray.length(); ++index) {
            DOMMY_RETURN_IF_FAILURE(newStringArray.set(defaultString, index));
        }

        MDataHandle outputData = block.outputValue(outDataArray, &stat);
        if (!stat)
            return stat;

        MFnStringArrayData uData;
        MObject outplug = uData.create(newStringArray, &stat);
        if (!stat)
            return stat;

        stat = outputData.set(outplug);
        if (!stat)
            return stat;

        outputData.setClean();
        return block.setClean(plug);

    } else {
        return MS::kUnknownParameter;
    }

    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DPStringArrayGen")

MStatus DPStringArrayGen::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    MFnStringData fnStringData;
    MObject defStrVal = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    value = tAttr.create("value", "val", MFnData::kString, defStrVal, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(value));

    valueSize = nAttr.create("valueSize", "vs", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedStringCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(valueSize));

    MFnGenericAttribute outAttr;
    outDataArray = outAttr.create("outDataArray", "out", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kStringArray));
    DOMMY_RETURN_IF_FAILURE(outAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(outAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(value, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(valueSize, outDataArray));

    return MS::kSuccess;
}
