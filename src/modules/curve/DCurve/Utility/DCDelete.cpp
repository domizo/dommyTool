//
//  DCDelete.cpp
//  dommyTool
//
//  Created by yuzo on 1/30/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCDelete.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DCurveDuplicatePlan.h"



const MTypeId DCDelete::typeId( ID_DC_DELETE );


MObject DCDelete::inDC;

MObject DCDelete::aDeleteType;

MObject DCDelete::outDC;



void *DCDelete::creator()
{
    return ::dommy::maya::createObject("DCDelete", [&]() {
        return new DCDelete();
    });
}


MStatus DCDelete::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* inputCurves = nullptr;
    MStatus status = getValidInputDC(inDC, data, inputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MDataHandle handle = data.inputValue(aDeleteType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (handle.asShort() != 0) return MS::kInvalidParameter;

    DPIdList deleteIds;
    if (!dommy::curve::buildWorldLineDeletionPlan(inputCurves->numberOfCurves(),
            [&](std::size_t index) -> const DPointList& {
                return *inputCurves->getCurve(index).getPositionPtrConst();
            },
            [&](std::size_t index) { return inputCurves->getCurve(index).getTransformMatrix(); },
            deleteIds)) {
        MGlobal::displayWarning("DCDelete rejected invalid transforms or an excessive comparison request.");
        return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*inputCurves));
    if (!outputCurves->deleteFromIds(deleteIds)) return MS::kFailure;

    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCDelete")




MStatus DCDelete::initialize() {
    MStatus stat;
    MFnTypedAttribute tAttr;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute delEnumFn;
    aDeleteType = delEnumFn.create("aDeleteType", "uvty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(delEnumFn.addField("Simple Linear Line", 0));
    DOMMY_RETURN_IF_FAILURE(delEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeleteType));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeleteType, outDC));
    return MS::kSuccess;
}
