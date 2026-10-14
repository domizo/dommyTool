//
//  DPFillSpace.cpp
//  dommyTool
//
//  Created by yuzo on 4/7/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPFillSpace.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DFillSpacePlan.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointPayloadBudget.h"

#include "DMayaHeaders.h"
#include "DPData.h"
#include "DPointDataAccess.h"
#include "dpoint_ids.h"



#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

MStatus appendFillSamples(
    const MPoint& source,
    const MPoint& target,
    double pairDistance,
    dommy::generation::FillSpacePlan& plan,
    dommy::point::PointPayloadBudget& workspace,
    MPointArray& output)
{
    std::size_t sampleCount = 0U;
    if (!dommy::generation::reserveFillSpaceSamples(
            plan, pairDistance, sampleCount)) {
        return MS::kInvalidParameter;
    }
    if (sampleCount == 0U) return MS::kSuccess;
    workspace.add(sampleCount, sizeof(MPoint) + sizeof(DPoint));

    const MVector delta = target - source;
    if (!dommy::maya::isFiniteVector(delta)) {
        return MS::kInvalidParameter;
    }
    const double divisor = static_cast<double>(sampleCount + 1U);
    for (std::size_t sample = 1U; sample <= sampleCount; ++sample) {
        const double weight = static_cast<double>(sample) / divisor;
        const MPoint candidate = source + delta * weight;
        if (!dommy::maya::isFinitePoint(candidate)) {
            return MS::kInvalidParameter;
        }
        const MStatus status = output.append(candidate);
        if (!status) return status;
    }
    return MS::kSuccess;
}

} // namespace


const MTypeId DPFillSpace::typeId( ID_DP_FILL_SPACE );

MObject DPFillSpace::inDP;

MObject DPFillSpace::samplingType;
MObject DPFillSpace::samplingCount;
MObject DPFillSpace::samplingSpace;

MObject DPFillSpace::isIDStepFill;
MObject DPFillSpace::nextStepSize;


MObject DPFillSpace::isDistanceFill;
MObject DPFillSpace::distance;

MObject DPFillSpace::outDP;



void *DPFillSpace::creator() {
    return ::dommy::maya::createObject("DPFillSpace", [&]() {
        return new DPFillSpace();
    });
}


MStatus DPFillSpace::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDP) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle samplingTypeHandle = data.inputValue(samplingType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int samplingTypeValue = samplingTypeHandle.asInt();
    const MDataHandle samplingCountHandle = data.inputValue(samplingCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int samplingCountValue = samplingCountHandle.asInt();
    const MDataHandle samplingSpaceHandle = data.inputValue(samplingSpace, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double samplingSpaceValue = samplingSpaceHandle.asDouble();
    const MDataHandle isIDStepFillHandle = data.inputValue(isIDStepFill, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool fillByIdStep = isIDStepFillHandle.asBool();
    const MDataHandle nextStepSizeHandle = data.inputValue(nextStepSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int neighborStepCount = nextStepSizeHandle.asInt();
    const MDataHandle isDistanceFillHandle = data.inputValue(isDistanceFill, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool fillByDistance = isDistanceFillHandle.asBool();
    const MDataHandle distanceHandle = data.inputValue(distance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double distanceLimit = distanceHandle.asDouble();

    const DPData* inputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputData));
    if (!inputData) return MS::kInvalidParameter;

    const std::size_t pointCount = inputData->numberOfPoints();
    dommy::generation::FillSpacePlan plan;
    if (!dommy::generation::buildFillSpacePlan(
            pointCount,
            samplingTypeValue, samplingCountValue, samplingSpaceValue,
            fillByIdStep, neighborStepCount,
            fillByDistance, distanceLimit, plan)) {
        MGlobal::displayError(
            "DPFillSpace: generation inputs exceed the point or work "
            "safety budget.");
        return MS::kInvalidParameter;
    }

    // This node retains positions only. Charge the native input export,
    // output staging and imported DPoint payload, not unrelated metadata.
    dommy::point::PointPayloadBudget workspace;
    workspace.add(pointCount, 2U * sizeof(MPoint) + sizeof(DPoint));
    dommy::point::PointPayloadBudget plannedWorkspace = workspace;
    plannedWorkspace.add(plan.minimumOutputPointCount - pointCount,
                         sizeof(MPoint) + sizeof(DPoint));
    const MPointArray points = inputData->getMPosition();
    if (static_cast<std::size_t>(points.length()) != pointCount) return MS::kFailure;

    MPointArray outputPoints;
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!dommy::maya::isFinitePoint(points[index])) {
            MGlobal::displayError(
                "DPFillSpace: input points must be finite.");
            return MS::kInvalidParameter;
        }
        DOMMY_RETURN_IF_FAILURE(outputPoints.append(points[index]));
    }

    if (plan.fillByIdStep) {
        for (std::size_t sourceIndex = 0U;
             sourceIndex < pointCount;
             ++sourceIndex) {
            const std::size_t available =
                pointCount - sourceIndex - 1U;
            const std::size_t neighborCount =
                std::min(plan.neighborStepCount, available);
            for (std::size_t offset = 1U;
                 offset <= neighborCount;
                 ++offset) {
                if (!dommy::generation::reserveFillSpacePairWork(plan)) {
                    return MS::kInvalidParameter;
                }
                const MPoint& source =
                    points[static_cast<unsigned int>(sourceIndex)];
                const MPoint& target =
                    points[static_cast<unsigned int>(sourceIndex + offset)];
                const double pairDistance = source.distanceTo(target);
                status = appendFillSamples(
                    source, target, pairDistance, plan, workspace, outputPoints);
                if (!status) {
                    MGlobal::displayError(
                        "DPFillSpace: generated ID-step samples exceed "
                        "the safety budget or are not finite.");
                    return status;
                }
            }
        }
    }

    if (plan.fillByDistance) {
        for (std::size_t sourceIndex = 0U;
             sourceIndex < pointCount;
             ++sourceIndex) {
            for (std::size_t targetIndex = sourceIndex + 1U;
                 targetIndex < pointCount;
                 ++targetIndex) {
                if (!dommy::generation::reserveFillSpacePairWork(plan)) {
                    return MS::kInvalidParameter;
                }
                const MPoint& source =
                    points[static_cast<unsigned int>(sourceIndex)];
                const MPoint& target =
                    points[static_cast<unsigned int>(targetIndex)];
                const double deltaX = std::abs(source.x - target.x);
                const double deltaY = std::abs(source.y - target.y);
                const double deltaZ = std::abs(source.z - target.z);
                if (deltaX > plan.distanceLimit ||
                    deltaY > plan.distanceLimit ||
                    deltaZ > plan.distanceLimit) {
                    continue;
                }

                const double pairDistance = source.distanceTo(target);
                if (!std::isfinite(pairDistance)) {
                    return MS::kInvalidParameter;
                }
                if (pairDistance > plan.distanceLimit) continue;

                status = appendFillSamples(
                    source, target, pairDistance, plan, workspace, outputPoints);
                if (!status) {
                    MGlobal::displayError(
                        "DPFillSpace: generated distance samples exceed "
                        "the safety budget or are not finite.");
                    return status;
                }
            }
        }
    }

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id,
        [&outputPoints](DPData& outputData) {
            outputData.setPosition(outputPoints);
            outputData.updateBBox();
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPFillSpace")



MStatus DPFillSpace::initialize() try {
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MStatus stat;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnEnumAttribute enumFn;
    samplingType = enumFn.create(
    "samplingType", "smt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Count", DP_FILL_COUNT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Space", DP_FILL_SPACE ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingType));

    samplingCount = nAttr.create( "samplingCount", "smc", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingCount));

    samplingSpace = nAttr.create( "samplingSpace", "sms", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingSpace));

    isIDStepFill  = nAttr.create( "isIDStepFill", "iid", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isIDStepFill));

    nextStepSize  = nAttr.create( "nextStepSize", "nss", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(nextStepSize));

    isDistanceFill  = nAttr.create( "isDistanceFill", "idf", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isDistanceFill));

    distance  = nAttr.create( "distance", "dis", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(distance));

    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( samplingType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( samplingCount, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( samplingSpace, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( isIDStepFill, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( nextStepSize, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( isDistanceFill, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( distance, outDP ));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPFillSpace", "initialize", std::current_exception());
}
