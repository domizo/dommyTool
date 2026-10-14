//
//  DPAttributeTransfer.cpp
//  dommyTool
//
//  Created by yuzo on 2017/05/20.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAttributeTransfer.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaAttributeTransfer.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DPoint/Common/DPointAttributeTransfer.h"
#include "DPointOp.h"

#include <limits>


const MTypeId DPAttributeTransfer::typeId( ID_DP_ATTR_TRANSFER );


MObject DPAttributeTransfer::inDP;

MObject DPAttributeTransfer::aTargetDP;
MObject DPAttributeTransfer::aAttributeNames;
MObject DPAttributeTransfer::aDistance;
MObject DPAttributeTransfer::aMaxSampleing;

MObject DPAttributeTransfer::aTransferRamp;
MObject DPAttributeTransfer::aValueRange;
MObject DPAttributeTransfer::aScale;

MObject DPAttributeTransfer::outDP;



void *DPAttributeTransfer::creator()
{
    return ::dommy::maya::createObject("DPAttributeTransfer", [&]() {
        return new DPAttributeTransfer();
    });
}



MStatus DPAttributeTransfer::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const DPData* input = nullptr;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;
    const DPData* source = nullptr;
    std::size_t totalTargetPoints = 0U;
    std::vector<std::string> validNames;
    dommy::transfer::AttributeTransferPlan plan;
    int maximumSampling = dommy::transfer::kUnlimitedSampling;
    if (input->numberOfPoints() != 0U) {
        const MDataHandle sourceHandle = data.inputValue(aTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::getPluginDataFromHandle(sourceHandle, DPData::id, source));
        if (!source || !source->hasValidPointTopology()) return MS::kInvalidParameter;
        const MDataHandle distanceHandle = data.inputValue(aDistance, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double distance = distanceHandle.asDouble();
        MStringArray requested;
        dommy::maya::NameListBudget nameBudget;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::readCheckedNameList(
            data, aAttributeNames, 1U, nameBudget, requested));
        const MDataHandle samplingHandle = data.inputValue(aMaxSampleing, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        maximumSampling = samplingHandle.asInt();
        const MDataHandle rangeHandle = data.inputValue(aValueRange, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double2& valueRange = rangeHandle.asDouble2();
        const MDataHandle scaleHandle = data.inputValue(aScale, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double scale = scaleHandle.asDouble();
        totalTargetPoints = input->numberOfPoints();
        if (!dommy::transfer::buildAttributeTransferPlan(
                source->numberOfPoints(), totalTargetPoints,
                maximumSampling,
                distance, valueRange[0], valueRange[1], scale, plan))
            return MS::kInvalidParameter;
        std::size_t components = 0U, workItems = 0U;
        if (!dommy::transfer::selectTransferAttributes(*source, requested, validNames, components) ||
            (!validNames.empty() && !dommy::transfer::buildAttributeApplicationBudget(
                plan, validNames.size(), components, 1U, workItems)))
            return MS::kInvalidParameter;
    }

    // Validate the complete request before copying output or starting the search.
    MFnPluginData pluginData;
    DPData* outputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DPData::id, outputData));
    if (!outputData) return MS::kFailure;
    outputData->setDPData(*input);
    if (source && !validNames.empty() && plan.sourcePointCount != 0U && totalTargetPoints != 0U) {
        const DPointList* sourcePoints =
            source->getPositionPtrConst();
        const DPointList* targetPoints =
            outputData->getPositionPtrConst();
        if (!sourcePoints ||
            sourcePoints->size() != plan.sourcePointCount ||
            !targetPoints ||
            targetPoints->size() != plan.targetPointCount) {
            return MS::kFailure;
        }

        PointCloud<double> sourceCloud;
        CreateSearchDataPoint(*sourcePoints, sourceCloud);
        DPkdtree index(
            3, sourceCloud,
            nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();

        IDsListArray neighborIds(plan.targetPointCount);
        DScalarListArray neighborDistances(plan.targetPointCount);
        PointSearchKDTreeRadiusWithDistanceOp search(
            index, *targetPoints, maximumSampling,
            plan.distance, neighborIds, neighborDistances);
        tbb::parallel_for(
            tbb::blocked_range<size_t>(
                0U, targetPoints->size(), 10000U),
            search);
        if (!dommy::transfer::validateNeighborLists(
                neighborIds, neighborDistances, plan)) {
            return MS::kFailure;
        }

        MPlug rampPlug(thisMObject(), aTransferRamp);
        MRampAttribute ramp(rampPlug, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        dommy::maya::AttributeTransferFactors factors;
        status = dommy::maya::sampleAttributeTransferFactors(
            neighborDistances, plan, ramp, factors);
        DOMMY_RETURN_IF_FAILURE(status);

        bool unsupportedType = false;
        for (unsigned int attributeIndex = 0U;
             attributeIndex < validNames.size();
             ++attributeIndex) {
            const std::string attributeName =
                validNames[attributeIndex];
            const dommy::transfer::AttributeApplyResult result =
                dommy::transfer::applyPointAttribute(
                    *source, *outputData, attributeName,
                    neighborIds, factors);
            if (result ==
                dommy::transfer::kAttributeApplyUnsupported) {
                unsupportedType = true;
                continue;
            }
            if (result == dommy::transfer::kAttributeApplyFailure) {
                MGlobal::displayError(
                    "DPAttributeTransfer: attribute data is invalid.");
                return MS::kFailure;
            }
        }
        if (unsupportedType) {
            MGlobal::displayWarning(
                "DPAttributeTransfer: skipped unsupported attribute types.");
        }
    }

    MDataHandle outputHandle = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(
            outputHandle, pluginData.object(), *outputData));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAttributeTransfer")








MStatus DPAttributeTransfer::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aTargetDP = tAttr.create("TargetDP", "tardp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    const MObject defaultString1 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeNames = tAttr.create("AttributeNames", "attrnms", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeNames));

    aDistance = nAttr.create( "Distance", "dst", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(std::numeric_limits<double>::epsilon()));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    aMaxSampleing = nAttr.create( "MaxSampleing", "maxsmp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(dommy::transfer::kMaxTransferSampling));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSampleing));

    aTransferRamp = MRampAttribute::createCurveRamp("TransferRamp", "trr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTransferRamp));

    aValueRange = nAttr.create( "ValueRange", "valr", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRange));

    aScale = nAttr.create( "Scale", "sc", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeNames, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistance, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMaxSampleing, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTransferRamp, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aValueRange, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDP ));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPAttributeTransfer", "initialize", std::current_exception());
}
