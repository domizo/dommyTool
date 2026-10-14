//
//  DMAttributeTransfer.cpp
//  dommyTool
//
//  Created by yuzo on 5/11/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMAttributeTransfer.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaAttributeTransfer.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaNameList.h"
#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DPoint/Common/DPointAttributeTransfer.h"
#include "DMeshDataHeaders.h"


#include "DPointOp.h"
#include <limits>


const MTypeId DMAttributeTransfer::typeId( ID_DM_ATTRIBUTE_TRANSFER );


MObject DMAttributeTransfer::inDM;

MObject DMAttributeTransfer::aTargetDP;
MObject DMAttributeTransfer::aAttributeNames;
MObject DMAttributeTransfer::aDistance;

MObject DMAttributeTransfer::aTransferRamp;
MObject DMAttributeTransfer::aValueRange;
MObject DMAttributeTransfer::aScale;

MObject DMAttributeTransfer::outDM;



void *DMAttributeTransfer::creator()
{
    return ::dommy::maya::createObject("DMAttributeTransfer", [&]() {
        return new DMAttributeTransfer();
    });
}

MStatus DMAttributeTransfer::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    MStatus status;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    const DPData* source = nullptr;
    std::size_t totalTargetPoints = 0U;
    std::vector<std::string> validNames;
    dommy::transfer::AttributeTransferPlan plan;
    if (input->numberOfMesh() != 0U) {
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
        const MDataHandle rangeHandle = data.inputValue(aValueRange, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double2& valueRange = rangeHandle.asDouble2();
        const MDataHandle scaleHandle = data.inputValue(aScale, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const double scale = scaleHandle.asDouble();
        if (!input->getCheckedTotalVertex(totalTargetPoints)) return MS::kFailure;
        if (!dommy::transfer::buildAttributeTransferPlan(
                source->numberOfPoints(), totalTargetPoints,
                dommy::transfer::kUnlimitedSampling,
                distance, valueRange[0], valueRange[1], scale, plan))
            return MS::kInvalidParameter;
        std::size_t components = 0U, workItems = 0U;
        if (!dommy::transfer::selectTransferAttributes(*source, requested, validNames, components) ||
            (!validNames.empty() && !dommy::transfer::buildAttributeApplicationBudget(
                plan, validNames.size(), components, input->numberOfMesh(), workItems)))
            return MS::kInvalidParameter;
    }

    // Validate the complete request before copying output or starting the search.
    MFnPluginData pluginData;
    DMeshData* outputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, outputData));
    if (!outputData) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*input));
    if (source && !validNames.empty() && plan.sourcePointCount != 0U && totalTargetPoints != 0U) {
        const DPointList* sourcePoints =
            source->getPositionPtrConst();
        if (!sourcePoints ||
            sourcePoints->size() !=
                plan.sourcePointCount) {
            return MS::kFailure;
        }
        PointCloud<double> sourceCloud;
        CreateSearchDataPoint(*sourcePoints, sourceCloud);
        DPkdtree index(
            3, sourceCloud,
            nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();

        MPlug rampPlug(thisMObject(), aTransferRamp);
        MRampAttribute ramp(rampPlug, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        bool unsupportedType = false;

        for (DMeshData::DMeshIter mesh = outputData->beginMeshes();
             mesh != outputData->endMeshes(); ++mesh) {
            if (!*mesh || !(*mesh)->hasValidMeshData()) {
                return MS::kFailure;
            }
            const DPointList* points =
                (*mesh)->getPositionPtrConst();
            if (!points ||
                points->size() != (*mesh)->numberOfPoints()) {
                return MS::kFailure;
            }

            dommy::transfer::AttributeTransferPlan meshPlan;
            if (!dommy::transfer::buildAttributeTransferPlan(
                    source->numberOfPoints(), points->size(),
                    dommy::transfer::kUnlimitedSampling,
                    plan.distance, plan.valueMinimum, plan.valueMaximum, plan.scale,
                    meshPlan)) {
                return MS::kFailure;
            }
            IDsListArray neighborIds(points->size());
            DScalarListArray neighborDistances(points->size());
            PointSearchKDTreeRadiusWithDistanceOp search(
                index, *points,
                dommy::transfer::kUnlimitedSampling,
                meshPlan.distance,
                neighborIds, neighborDistances);
            tbb::parallel_for(
                tbb::blocked_range<size_t>(
                    0U, points->size(), 10000U),
                search);
            if (!dommy::transfer::validateNeighborLists(
                    neighborIds, neighborDistances, meshPlan)) {
                return MS::kFailure;
            }

            dommy::maya::AttributeTransferFactors factors;
            status = dommy::maya::sampleAttributeTransferFactors(
                neighborDistances, meshPlan, ramp, factors);
            DOMMY_RETURN_IF_FAILURE(status);

            for (unsigned int attributeIndex = 0U;
                 attributeIndex < validNames.size();
                 ++attributeIndex) {
                const dommy::transfer::AttributeApplyResult result =
                    dommy::transfer::applyPointAttribute(
                        *source, *(*mesh),
                        validNames[attributeIndex],
                        neighborIds, factors);
                if (result ==
                    dommy::transfer::kAttributeApplyUnsupported) {
                    unsupportedType = true;
                } else if (
                    result ==
                    dommy::transfer::kAttributeApplyFailure) {
                    return MS::kFailure;
                }
            }
        }

        if (unsupportedType) {
            MGlobal::displayWarning(
                "DMAttributeTransfer: skipped unsupported attribute types.");
        }
        outputData->updateBBox();
    }

    MDataHandle outputHandle = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            outputHandle, pluginData.object(), DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMAttributeTransfer")




MStatus DMAttributeTransfer::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
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
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeNames, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistance, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTransferRamp, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aValueRange, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDM ));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMAttributeTransfer", "initialize", std::current_exception());
}
