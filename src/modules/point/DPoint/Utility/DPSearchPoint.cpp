//
//  DPSearchPoint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/12/22.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSearchPoint.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DAttributeTransferPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DPointOp.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

bool collectTargetCloud(const DPData& targets,
                        PointCloud<double>& cloud)
{
    if (!targets.hasValidPointTopology() ||
        targets.numberOfPoints() >
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    cloud.pts.reserve(targets.numberOfPoints());
    for (ConstDPointIter point = targets.beginPt();
         point != targets.endPt(); ++point) {
        if (!std::isfinite((*point)[0]) ||
            !std::isfinite((*point)[1]) ||
            !std::isfinite((*point)[2])) {
            return false;
        }
        PointCloud<double>::Point cloudPoint;
        cloudPoint.x = (*point)[0];
        cloudPoint.y = (*point)[1];
        cloudPoint.z = (*point)[2];
        cloud.pts.push_back(cloudPoint);
    }
    return cloud.pts.size() == targets.numberOfPoints();
}

bool collectCurveCenters(const DCurveData& curves,
                         DPointList& centers)
{
    if (!curves.hasValidCurveTopology() ||
        curves.numberOfCurves() >
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    centers.reserve(curves.numberOfCurves());
    for (DCurveData::ConstDCurveIter curve = curves.beginCurves();
         curve != curves.endCurves(); ++curve) {
        if (!*curve || !(*curve)->hasValidCurveTopology()) {
            return false;
        }
        const DPoint center = (*curve)->getCenter();
        if (!std::isfinite(center[0]) ||
            !std::isfinite(center[1]) ||
            !std::isfinite(center[2])) {
            return false;
        }
        centers.push_back(center);
    }
    return true;
}

bool collectMeshCenters(const DMeshData& meshes,
                        DPointList& centers)
{
    if (!meshes.hasValidMeshTopology() ||
        meshes.numberOfMesh() >
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    centers.reserve(meshes.numberOfMesh());
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh) {
        if (!*mesh || !(*mesh)->hasValidMeshData()) return false;
        const DPoint center = (*mesh)->getCenter();
        if (!std::isfinite(center[0]) ||
            !std::isfinite(center[1]) ||
            !std::isfinite(center[2])) {
            return false;
        }
        centers.push_back(center);
    }
    return true;
}

bool buildSearchAttributeValues(
    const DPointList& queryPoints,
    const PointCloud<double>& targetCloud,
    int maximumCount,
    double radius,
    bool detectNearest,
    std::vector<DIntArray>& outputValues)
{
    if (queryPoints.size() >
        dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }

    std::vector<DIntArray> candidate(queryPoints.size());
    if (queryPoints.empty() || targetCloud.pts.empty()) {
        outputValues.swap(candidate);
        return true;
    }

    const int sampling = detectNearest ? 1 : maximumCount;
    const double planningRadius = detectNearest ? 1.0 : radius;
    dommy::transfer::AttributeTransferPlan plan;
    if (!dommy::transfer::buildAttributeTransferPlan(
            targetCloud.pts.size(), queryPoints.size(),
            sampling, planningRadius, 0.0, 1.0, 1.0, plan)) {
        return false;
    }

    DPkdtree index(
        3, targetCloud,
        nanoflann::KDTreeSingleIndexAdaptorParams(10));
    index.buildIndex();

    if (detectNearest) {
        IDs nearestIds(queryPoints.size());
        DScalarList nearestDistances(queryPoints.size());
        PointSearchKDTreeNearestWithDistanceOp search(
            index, queryPoints, nearestIds, nearestDistances);
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(
                0U, queryPoints.size(), 10000U),
            search);
        for (std::size_t query = 0U;
             query < queryPoints.size(); ++query) {
            const std::size_t target =
                static_cast<std::size_t>(nearestIds[query]);
            if (target >= targetCloud.pts.size() ||
                !std::isfinite(nearestDistances[query]) ||
                target >
                    static_cast<std::size_t>(
                        std::numeric_limits<std::int32_t>::max())) {
                return false;
            }
            candidate[query].push_back(
                static_cast<std::int32_t>(target));
        }
    } else {
        IDsListArray neighborIds(queryPoints.size());
        DScalarListArray neighborDistances(queryPoints.size());
        PointSearchKDTreeRadiusWithDistanceOp search(
            index, queryPoints, maximumCount, radius,
            neighborIds, neighborDistances);
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(
                0U, queryPoints.size(), 10000U),
            search);
        if (!dommy::transfer::validateNeighborLists(
                neighborIds, neighborDistances, plan)) {
            return false;
        }

        for (std::size_t query = 0U;
             query < neighborIds.size(); ++query) {
            candidate[query].reserve(neighborIds[query].size());
            for (std::size_t neighbor = 0U;
                 neighbor < neighborIds[query].size(); ++neighbor) {
                const std::size_t target =
                    static_cast<std::size_t>(
                        neighborIds[query][neighbor]);
                if (target >
                    static_cast<std::size_t>(
                        std::numeric_limits<std::int32_t>::max())) {
                    return false;
                }
                candidate[query].push_back(
                    static_cast<std::int32_t>(target));
            }
        }
    }

    outputValues.swap(candidate);
    return true;
}

} // namespace

const MTypeId DPSearchPoint::typeId(ID_DP_SEARCH_POINT);

MObject DPSearchPoint::inDD;
MObject DPSearchPoint::aTargetDP;
MObject DPSearchPoint::aPrimAttributeNames;
MObject DPSearchPoint::aIndexAttributeNames;
MObject DPSearchPoint::aRadius;
MObject DPSearchPoint::aMaxCount;
MObject DPSearchPoint::aDetectNearestPoint;
MObject DPSearchPoint::outDD;

void* DPSearchPoint::creator()
{
    return ::dommy::maya::createObject("DPSearchPoint", [&]() {
        return new DPSearchPoint();
    });
}

MStatus DPSearchPoint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle handle = data.inputValue(aTargetDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* targets = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, targets));
    if (!targets || !targets->hasValidPointTopology()) return MS::kInvalidParameter;

    handle = data.inputValue(aIndexAttributeNames, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MString attributeName = handle.asString();
    if (attributeName.length() == 0U) attributeName = "idx";
    if (attributeName.length() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;

    handle = data.inputValue(aRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double radius = handle.asFloat();
    handle = data.inputValue(aMaxCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int maximumCount = handle.asInt();
    handle = data.inputValue(aDetectNearestPoint, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool detectNearest = handle.asBool();
    if ((!detectNearest && (!std::isfinite(radius) ||
             radius <= std::numeric_limits<float>::epsilon())) ||
        maximumCount <= 0 || maximumCount > dommy::transfer::kMaxTransferSampling)
        return MS::kInvalidParameter;

    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTypeId inputType = inputHandle.typeId();
    const DPData* points = nullptr;
    const DCurveData* curves = nullptr;
    const DMeshData* meshes = nullptr;
    DPointList centers;
    const DPointList* queries = nullptr;
    if (inputType == DPData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, points));
        if (!points || !points->hasValidPointTopology()) return MS::kInvalidParameter;
        queries = points->getPositionPtrConst();
    } else if (inputType == DCurveData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, curves));
        if (!curves || !collectCurveCenters(*curves, centers)) return MS::kInvalidParameter;
        queries = &centers;
    } else if (inputType == DMeshData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DMeshData::id, meshes));
        if (!meshes || !collectMeshCenters(*meshes, centers)) return MS::kInvalidParameter;
        queries = &centers;
    } else {
        return MS::kInvalidParameter;
    }
    if (!queries) return MS::kFailure;

    PointCloud<double> cloud;
    if (!collectTargetCloud(*targets, cloud)) return MS::kInvalidParameter;
    std::vector<DIntArray> values;
    if (!buildSearchAttributeValues(*queries, cloud, maximumCount, radius, detectNearest, values) ||
        values.size() != queries->size()) return MS::kFailure;

    const std::string outputName = attributeName.asChar();
    if (points) {
        status = dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) {
                output.setDPData(*points);
                if (output.isExistAttr(outputName)) output.removeAttr(outputName);
                output.addAttr(outputName, DPIntArrayAttr(values));
                return MS::kSuccess;
            });
    } else if (curves) {
        status = dommy::maya::setPluginDataOutput<DCurveData>(data, outDD, DCurveData::id,
            [&](DCurveData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*curves));
                if (output.isExistAttr(outputName)) output.removeAttr(outputName);
                output.addAttr(outputName, DPIntArrayAttr(values));
                return MStatus(MS::kSuccess);
            });
    } else {
        status = dommy::maya::setPluginDataOutput<DMeshData>(data, outDD, DMeshData::id,
            [&](DMeshData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*meshes));
                if (output.isExistAttr(outputName)) output.removeAttr(outputName);
                output.addAttr(outputName, DPIntArrayAttr(values));
                return MStatus(MS::kSuccess);
            });
    }
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPSearchPoint")

MStatus DPSearchPoint::initialize() try {
    MStatus status;
    MFnGenericAttribute genericAttribute;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inDD = genericAttribute.create("inDD", "idd", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    aTargetDP = typedAttribute.create(
        "TargetDP", "tdp", DPData::id,
        MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    const MObject defaultPrimitiveName =
        stringData.create("prim", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aPrimAttributeNames = typedAttribute.create(
        "PrimAttributeNames", "prmattrn",
        MFnData::kString, defaultPrimitiveName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPrimAttributeNames));

    const MObject defaultIndexName =
        stringData.create("idx", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aIndexAttributeNames = typedAttribute.create(
        "IndexAttributeNames", "idxattrn",
        MFnData::kString, defaultIndexName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIndexAttributeNames));

    aRadius = numericAttribute.create(
        "Radius", "rad",
        MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(
        std::numeric_limits<float>::epsilon()));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aMaxCount = numericAttribute.create(
        "MaxCount", "mxcnt",
        MFnNumericData::kInt, 5, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(
        dommy::transfer::kMaxTransferSampling));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxCount));

    aDetectNearestPoint = numericAttribute.create(
        "DetectNearestPoint", "dtnrpt",
        MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDetectNearestPoint));

    outDD = genericAttribute.create("outDD", "odd", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPrimAttributeNames, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIndexAttributeNames, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxCount, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDetectNearestPoint, outDD));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPSearchPoint", "initialize", std::current_exception());
}
