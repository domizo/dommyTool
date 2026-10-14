//
//  DCGroupFromDP.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCGroupFromDP.h"
#include "common/utility/DGroupGenerationPlan.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DSpatialSearchPlan.h"

#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DPointOp.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

MTypeId DCGroupFromDP::typeId(ID_DC_GROUP_FROM_DP);

MObject DCGroupFromDP::inDC;
MObject DCGroupFromDP::aGroupName;
MObject DCGroupFromDP::aTargetDP;
MObject DCGroupFromDP::aUseDistance;
MObject DCGroupFromDP::aDistance;
MObject DCGroupFromDP::outDC;

void* DCGroupFromDP::creator()
{
    return ::dommy::maya::createObject("DCGroupFromDP", [&]() {
        return new DCGroupFromDP();
    });
}

MStatus DCGroupFromDP::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    MStatus status;
    const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* targets = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, targets));
    if (!targets || !targets->hasValidPointTopology()) return MS::kInvalidParameter;

    const MDataHandle groupHandle = data.inputValue(aGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString baseGroupName = groupHandle.asString();
    const MDataHandle distanceFlagHandle = data.inputValue(aUseDistance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useDistance = distanceFlagHandle.asBool();
    const MDataHandle distanceHandle = data.inputValue(aDistance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double maximumDistance = distanceHandle.asDouble();
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0 ||
        baseGroupName.length() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;
    // MString length is not a byte budget for multibyte names.
    const std::size_t baseNameBytes = std::strlen(baseGroupName.asChar());
    if (baseNameBytes > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;

    const std::size_t curveCount = input->numberOfCurves();
    const std::size_t targetCount = targets->numberOfPoints();
    std::map<std::size_t, DPIndexList> groupedCurves;
    std::vector<std::string> groupNames;
    if (curveCount != 0U && targetCount != 0U) {
        dommy::spatial::SpatialSearchPlan plan;
        if (!dommy::spatial::buildSpatialSearchPlan(
                targetCount, curveCount, 1, 1, 1.0, plan))
            return MS::kInvalidParameter;

        PointCloud<double> targetCloud;
        targetCloud.pts.reserve(targetCount);
        for (ConstDPointIter point = targets->beginPt(); point != targets->endPt(); ++point) {
            if (!std::isfinite((*point)[0]) || !std::isfinite((*point)[1]) ||
                !std::isfinite((*point)[2])) return MS::kInvalidParameter;
            PointCloud<double>::Point cloudPoint;
            cloudPoint.x = (*point)[0];
            cloudPoint.y = (*point)[1];
            cloudPoint.z = (*point)[2];
            targetCloud.pts.push_back(cloudPoint);
        }
        if (targetCloud.pts.size() != targetCount) return MS::kFailure;

        DPointList centers;
        centers.reserve(curveCount);
        for (DCurveData::ConstDCurveIter curve = input->beginCurves();
             curve != input->endCurves(); ++curve) {
            if (!*curve || !(*curve)->hasValidCurveTopology()) return MS::kFailure;
            const DPoint center = (*curve)->getCenter();
            if (!std::isfinite(center[0]) || !std::isfinite(center[1]) ||
                !std::isfinite(center[2])) return MS::kFailure;
            centers.push_back(center);
        }
        if (centers.size() != curveCount) return MS::kFailure;

        DPkdtree index(3, targetCloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();
        IDs nearestIds(curveCount);
        DScalarList nearestDistances(curveCount);
        PointSearchKDTreeNearestWithDistanceOp nearestSearch(
            index, centers, nearestIds, nearestDistances);
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0U, curveCount, 10000U), nearestSearch);

        for (std::size_t curveIndex = 0U; curveIndex < curveCount; ++curveIndex) {
            const std::size_t targetIndex = static_cast<std::size_t>(nearestIds[curveIndex]);
            const double distance = nearestDistances[curveIndex];
            if (targetIndex >= targetCount || !std::isfinite(distance) || distance < 0.0)
                return MS::kFailure;
            if (useDistance && distance > maximumDistance) continue;
            groupedCurves[targetIndex].push_back(curveIndex);
        }

        if (!dommy::group::buildGeneratedGroupNames(
                *input, curveCount, groupedCurves.size(), baseNameBytes,
                [&](int index) { return std::string((baseGroupName + index).asChar()); },
                groupNames)) return MS::kInvalidParameter;
    }

    // No output copy or dense group mask is allocated until preflight succeeds.
    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    std::size_t createdCount = 0U;
    for (auto group = groupedCurves.begin(); group != groupedCurves.end(); ++group, ++createdCount) {
        const std::string& name = groupNames[createdCount];
        DPIdList groupIds(curveCount, false);
        for (const std::size_t curveIndex : group->second) {
            if (curveIndex >= curveCount) return MS::kFailure;
            groupIds[curveIndex] = true;
        }
        if (output->isExistGrp(name)) output->removeGrp(name);
        output->addGrp(name, groupIds);
    }

    if (!output->hasValidCurveTopology()) return MS::kFailure;
    MDataHandle outputHandle = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DCurveData>(
            outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCGroupFromDP")

MStatus DCGroupFromDP::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inDC = typedAttribute.create(
        "inDC", "idc", DCurveData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    const MObject defaultGroupName = stringData.create("piece", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aGroupName = typedAttribute.create(
        "GroupName", "grpn", MFnData::kString,
        defaultGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

    aTargetDP = typedAttribute.create(
        "TargetDP", "tdp", DPData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    aUseDistance = numericAttribute.create(
        "UseDistance", "usedist",
        MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseDistance));

    aDistance = numericAttribute.create(
        "Distance", "dist",
        MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroupName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseDistance, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistance, outDC));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCGroupFromDP", "initialize", std::current_exception());
}
