//
//  DPMerge.cpp
//  dommyTool
//
//  Created by yuzo on 2015/03/26.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPMerge.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointMergeBudget.h"

#include "DDataUtility/Data/DPData.h"
#include "DPoint/dpoint_ids.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MDataHandle.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MStatus.h>

#include <stdexcept>
#include <vector>

const MTypeId DPMerge::typeId(ID_DP_MERGE);

// Attribute
MObject DPMerge::inDPs;
MObject DPMerge::outDP;

void* DPMerge::creator() {
    return ::dommy::maya::createObject("DPMerge", [&]() {
        return new DPMerge();
    });
}

MStatus DPMerge::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    MArrayDataHandle inputs = data.inputArrayValue(inDPs, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    const unsigned int inputCount = inputs.elementCount(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (inputCount > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;
    std::vector<const DPData*> inputData;
    inputData.reserve(inputCount);
    for (unsigned int index = 0; index < inputCount; ++index) {
        MDataHandle inputHandle = inputs.inputValue(&status);
        if (status != MS::kSuccess) {
            return status;
        }

        // Keep genuinely empty array slots optional, but do not hide a failed
        // binding or a wrong payload type as an absent input.
        if (!inputHandle.data().isNull()) {
            const DPData* currentInput = nullptr;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
                inputHandle, DPData::id, currentInput));
            if (!currentInput) return MS::kInvalidParameter;
            inputData.push_back(currentInput);
        }

        if (index + 1 < inputCount) {
            status = inputs.next();
            if (status != MS::kSuccess) {
                return status;
            }
        }
    }

    std::size_t workItems = 0U;
    if (!dommy::point::buildPointMergeBudget(inputData, workItems))
        return MS::kInvalidParameter;

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id,
        [&inputData](DPData& outputData) { return outputData.mergeFrom(inputData); });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPMerge")

MStatus DPMerge::initialize() try {
    MStatus stat;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDPs = tAttr.create("inDPs", "idps", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDPs));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDPs, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPMerge", "initialize", std::current_exception());
}
