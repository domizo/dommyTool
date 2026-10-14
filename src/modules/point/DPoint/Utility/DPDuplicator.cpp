//
//  DPDuplicator.cpp
//  dommyTool
//
//  Created by yuzo on 5/18/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDuplicator.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointMergeBudget.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DDataUtility/Data/DPData.h"
#include "DPoint/dpoint_ids.h"
#include <maya/MFnTypedAttribute.h>

#include <cstddef>

const MTypeId DPDuplicator::typeId(ID_DP_DUPLICATOR);

MObject DPDuplicator::inDP;

MObject DPDuplicator::aTargetDP;

MObject DPDuplicator::outDP;

void* DPDuplicator::creator() {
    return ::dommy::maya::createObject("DPDuplicator", [&]() {
        return new DPDuplicator();
    });
}

MStatus DPDuplicator::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;

    const DPData* target = nullptr;
    const std::size_t pointCount = input->numberOfPoints();
    if (pointCount > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;
    // An empty source historically produces an empty output without reading TargetDP.
    if (pointCount != 0U) {
        const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, target));
        if (!target || !target->hasValidPointTopology()) return MS::kInvalidParameter;
        std::size_t outputPointCount = 0U, workItems = 0U;
        if (!dommy::generation::checkedProduct(pointCount, target->numberOfPoints(),
                dommy::generation::kMaxGeneratedPointCount, outputPointCount) ||
            !dommy::point::buildPointCopyBudget(*input, target->numberOfPoints(), workItems))
            return MS::kInvalidParameter;
    }

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, target](DPData& output) -> MStatus {
            if (!target || target->numberOfPoints() == 0U) return MS::kSuccess;
            const DPointList& positions = *target->getPositionPtrConst();
            return output.copyRepeatedFrom(*input, positions.size(),
                [&positions](DPoint& point, std::size_t copyIndex, std::size_t) -> MStatus {
                    for (std::size_t axis = 0U; axis < 3U; ++axis) {
                        if (!dommy::numeric::checkedNumericCast(
                                static_cast<double>(point[axis]) + positions[copyIndex][axis], point[axis]))
                            return MS::kInvalidParameter;
                    }
                    return MS::kSuccess;
                });
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPDuplicator")

MStatus DPDuplicator::initialize() try {
    MStatus stat;

    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPDuplicator", "initialize", std::current_exception());
}
