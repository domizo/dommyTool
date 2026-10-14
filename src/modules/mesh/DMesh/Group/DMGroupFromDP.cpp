//
//  DMGroupFromDP.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/23.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMGroupFromDP.h"
#include "common/utility/DGroupGenerationPlan.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DSpatialSearchPlan.h"

#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DPointOp.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

MTypeId DMGroupFromDP::typeId(ID_DM_GROUP_FROM_DP);

MObject DMGroupFromDP::inDM;
MObject DMGroupFromDP::aGroupName;
MObject DMGroupFromDP::aTargetDP;
MObject DMGroupFromDP::aUseDistance;
MObject DMGroupFromDP::aDistance;
MObject DMGroupFromDP::outDM;

void* DMGroupFromDP::creator()
{
    return ::dommy::maya::createObject("DMGroupFromDP", [&]() {
        return new DMGroupFromDP();
    });
}

MStatus DMGroupFromDP::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
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

    const std::size_t meshCount = input->numberOfMesh();
    const std::size_t targetCount = targets->numberOfPoints();
    std::map<std::size_t, DPIndexList> groupedMeshes;
    std::vector<std::string> groupNames;
    if (meshCount != 0U && targetCount != 0U) {
        dommy::spatial::SpatialSearchPlan plan;
        if (!dommy::spatial::buildSpatialSearchPlan(
                targetCount, meshCount, 1, 1, 1.0, plan))
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
        centers.reserve(meshCount);
        for (DMeshData::ConstDMeshIter mesh = input->beginMeshes();
             mesh != input->endMeshes(); ++mesh) {
            if (!*mesh || !(*mesh)->hasValidMeshData()) return MS::kFailure;
            const DPoint center = (*mesh)->getCenter();
            if (!std::isfinite(center[0]) || !std::isfinite(center[1]) ||
                !std::isfinite(center[2])) return MS::kFailure;
            centers.push_back(center);
        }
        if (centers.size() != meshCount) return MS::kFailure;

        DPkdtree index(3, targetCloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();
        IDs nearestIds(meshCount);
        DScalarList nearestDistances(meshCount);
        PointSearchKDTreeNearestWithDistanceOp nearestSearch(
            index, centers, nearestIds, nearestDistances);
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0U, meshCount, 10000U), nearestSearch);

        for (std::size_t meshIndex = 0U; meshIndex < meshCount; ++meshIndex) {
            const std::size_t targetIndex = static_cast<std::size_t>(nearestIds[meshIndex]);
            const double distance = nearestDistances[meshIndex];
            if (targetIndex >= targetCount || !std::isfinite(distance) || distance < 0.0)
                return MS::kFailure;
            if (useDistance && distance > maximumDistance) continue;
            groupedMeshes[targetIndex].push_back(meshIndex);
        }

        if (!dommy::group::buildGeneratedGroupNames(
                *input, meshCount, groupedMeshes.size(), baseNameBytes,
                [&](int index) { return std::string((baseGroupName + index).asChar()); },
                groupNames)) return MS::kInvalidParameter;
    }

    // No output copy or dense group mask is allocated until preflight succeeds.
    MFnPluginData pluginData;
    DMeshData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DMeshData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(output->copyFrom(*input));
    std::size_t createdCount = 0U;
    for (auto group = groupedMeshes.begin(); group != groupedMeshes.end(); ++group, ++createdCount) {
        const std::string& name = groupNames[createdCount];
        DPIdList groupIds(meshCount, false);
        for (const std::size_t meshIndex : group->second) {
            if (meshIndex >= meshCount) return MS::kFailure;
            groupIds[meshIndex] = true;
        }
        if (output->isExistGrp(name)) output->removeGrp(name);
        output->addGrp(name, groupIds);
    }

    if (!output->hasValidMeshTopology()) return MS::kFailure;
    MDataHandle outputHandle = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            outputHandle, pluginData.object(), DMeshData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMGroupFromDP")

MStatus DMGroupFromDP::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

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

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGroupName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseDistance, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistance, outDM));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMGroupFromDP", "initialize", std::current_exception());
}
