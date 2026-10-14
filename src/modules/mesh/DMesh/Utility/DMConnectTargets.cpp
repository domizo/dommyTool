//
//  DMConnectTargets.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/29.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMConnectTargets.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DSpatialSearchPlan.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DAttributeArrayConversion.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DCurve/Common/DConnectionCurveBuilder.h"
#include "DMeshDataHeaders.h"
#include "DPointOp.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct ComponentIdentity {
    ComponentIdentity(std::int32_t primitiveValue = -1,
                      std::int32_t pointValue = -1)
        : primitive(primitiveValue), point(pointValue) {}

    std::int32_t primitive;
    std::int32_t point;
};

typedef std::tuple<std::int32_t, std::int32_t,
                   std::int32_t, std::int32_t> ConnectionKey;

struct ConnectionCandidate {
    double distance;
    ConnectionKey key;
    Imath::V3d first;
    Imath::V3d second;
};

bool checkedComponentIndex(std::size_t value, std::int32_t& result)
{
    if (value >
        static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    result = static_cast<std::int32_t>(value);
    return true;
}

template <typename Point>
bool isFinitePoint(const Point& point)
{
    return std::isfinite(point[0]) &&
           std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}

template <typename Point>
bool appendCloudPoint(PointCloud<double>& cloud,
                      const Point& point)
{
    if (!isFinitePoint(point) ||
        cloud.pts.size() >=
            dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }

    PointCloud<double>::Point cloudPoint;
    cloudPoint.x = point[0];
    cloudPoint.y = point[1];
    cloudPoint.z = point[2];
    cloud.pts.push_back(cloudPoint);
    return true;
}

bool appendMeshPoints(const DMeshData& meshes,
                      PointCloud<double>& cloud,
                      std::vector<ComponentIdentity>& identities)
{
    std::size_t totalPoints = 0U;
    if (!meshes.hasValidMeshTopology() ||
        !meshes.getCheckedTotalVertex(totalPoints) ||
        !dommy::attribute::validArrayConversionBudget(
            totalPoints, sizeof(PointCloud<double>::Point) + sizeof(ComponentIdentity))) {
        return false;
    }
    cloud.pts.reserve(totalPoints);
    identities.reserve(totalPoints);

    std::size_t meshIndex = 0U;
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh, ++meshIndex) {
        std::int32_t primitive = -1;
        if (!*mesh || !(*mesh)->hasValidMeshData() ||
            !checkedComponentIndex(meshIndex, primitive)) {
            return false;
        }

        std::size_t pointIndex = 0U;
        for (ConstDPointIter point = (*mesh)->beginPt();
             point != (*mesh)->endPt(); ++point, ++pointIndex) {
            std::int32_t pointId = -1;
            std::array<double, 3> world;
            if (!checkedComponentIndex(pointIndex, pointId) ||
                !dommy::numeric::checkedTransformPoint(*point, (*mesh)->getTransformMatrix(), world) ||
                !appendCloudPoint(cloud, world)) {
                return false;
            }
            identities.push_back(
                ComponentIdentity(primitive, pointId));
        }
    }
    return cloud.pts.size() == totalPoints &&
           identities.size() == totalPoints;
}

bool appendCurvePoints(const DCurveData& curves,
                       PointCloud<double>& cloud,
                       std::vector<ComponentIdentity>& identities)
{
    std::size_t totalPoints = 0U;
    if (!curves.hasValidCurveTopology() ||
        !curves.getCheckedTotalCvs(totalPoints) ||
        !dommy::attribute::validArrayConversionBudget(
            totalPoints, sizeof(PointCloud<double>::Point) + sizeof(ComponentIdentity))) {
        return false;
    }
    cloud.pts.reserve(totalPoints);
    identities.reserve(totalPoints);

    std::size_t curveIndex = 0U;
    for (DCurveData::ConstDCurveIter curve = curves.beginCurves();
         curve != curves.endCurves(); ++curve, ++curveIndex) {
        std::int32_t primitive = -1;
        if (!*curve || !(*curve)->hasValidCurveTopology() ||
            !checkedComponentIndex(curveIndex, primitive)) {
            return false;
        }

        std::size_t pointIndex = 0U;
        for (ConstDPointIter point = (*curve)->beginPt();
             point != (*curve)->endPt(); ++point, ++pointIndex) {
            std::int32_t pointId = -1;
            std::array<double, 3> world;
            if (!checkedComponentIndex(pointIndex, pointId) ||
                !dommy::numeric::checkedTransformPoint(*point, (*curve)->getTransformMatrix(), world) ||
                !appendCloudPoint(cloud, world)) {
                return false;
            }
            identities.push_back(
                ComponentIdentity(primitive, pointId));
        }
    }
    return cloud.pts.size() == totalPoints &&
           identities.size() == totalPoints;
}

bool appendMeshCenters(const DMeshData& meshes,
                       PointCloud<double>& cloud,
                       std::vector<ComponentIdentity>& identities)
{
    if (!meshes.hasValidMeshTopology() ||
        !dommy::attribute::validArrayConversionBudget(meshes.numberOfMesh(),
            sizeof(PointCloud<double>::Point) + sizeof(ComponentIdentity))) {
        return false;
    }
    cloud.pts.reserve(meshes.numberOfMesh());
    identities.reserve(meshes.numberOfMesh());

    std::size_t meshIndex = 0U;
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh, ++meshIndex) {
        std::int32_t primitive = -1;
        if (!*mesh || !checkedComponentIndex(meshIndex, primitive) ||
            !appendCloudPoint(cloud, (*mesh)->getCenterDouble())) {
            return false;
        }
        identities.push_back(ComponentIdentity(primitive, -1));
    }
    return true;
}

MStatus appendCurveCenters(const DCurveData& curves,
                           PointCloud<double>& cloud,
                           std::vector<ComponentIdentity>& identities)
{
    if (!curves.hasValidCurveTopology() ||
        !dommy::attribute::validArrayConversionBudget(curves.numberOfCurves(),
            sizeof(PointCloud<double>::Point) + sizeof(ComponentIdentity))) {
        return MS::kFailure;
    }
    cloud.pts.reserve(curves.numberOfCurves());
    identities.reserve(curves.numberOfCurves());

    MStatus status;
    std::size_t curveIndex = 0U;
    for (DCurveData::ConstDCurveIter curve = curves.beginCurves();
         curve != curves.endCurves(); ++curve, ++curveIndex) {
        std::int32_t primitive = -1;
        if (!*curve || !(*curve)->hasValidCurveTopology() ||
            !checkedComponentIndex(curveIndex, primitive)) {
            return MS::kFailure;
        }

        MObject curveObject;
        status = generateMCurveFromDcurve(*curve, curveObject);
        if (!status) return status;
        MFnNurbsCurve curveFunction(curveObject, &status);
        if (!status) return status;
        // Length and half-length parameter must be evaluated after the affine
        // transform, especially for nonuniform scale/shear.
        WorldPointList worldPoints;
        if (!dommy::attribute::validArrayConversionBudget((*curve)->numberOfPoints(),
                sizeof(Imath::V3d) + sizeof(MPoint)) ||
            !dommy::numeric::checkedTransformPoints(
                *(*curve)->getPositionPtrConst(), (*curve)->getTransformMatrix(), worldPoints))
            return MS::kInvalidParameter;
        MPointArray worldCVs;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(worldPoints.size(),
            [&](unsigned int i, MPoint& point) -> MStatus {
                point = MPoint(worldPoints[i][0], worldPoints[i][1], worldPoints[i][2]);
                return MS::kSuccess;
            }, worldCVs));
        DOMMY_RETURN_IF_FAILURE(curveFunction.setCVs(worldCVs, MSpace::kObject));
        DOMMY_RETURN_IF_FAILURE(curveFunction.updateCurve());
        const double length =
            curveFunction.length(kMFnNurbsEpsilon, &status);
        if (!status) return status;
        if (!std::isfinite(length) || length < 0.0) {
            return MS::kInvalidParameter;
        }
        const double parameter =
            curveFunction.findParamFromLength(length * 0.5, &status);
        if (!status) return status;
        if (!std::isfinite(parameter)) return MS::kInvalidParameter;

        MPoint center;
        status = curveFunction.getPointAtParam(
            parameter, center, MSpace::kObject);
        if (!status) return status;
        if (!appendCloudPoint(cloud, center)) {
            return MS::kInvalidParameter;
        }
        identities.push_back(ComponentIdentity(primitive, -1));
    }
    return MS::kSuccess;
}

bool appendSourcePoints(const DMeshData& meshes,
                        DComponentType componentType,
                        WorldPointList& points,
                        std::vector<ComponentIdentity>& identities)
{
    if (!meshes.hasValidMeshTopology()) return false;

    if (componentType == D_COMP_PRIM) {
        if (!dommy::attribute::validArrayConversionBudget(meshes.numberOfMesh(),
                sizeof(Imath::V3d) + sizeof(ComponentIdentity))) {
            return false;
        }
        points.reserve(meshes.numberOfMesh());
        identities.reserve(meshes.numberOfMesh());
        std::size_t meshIndex = 0U;
        for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
             mesh != meshes.endMeshes(); ++mesh, ++meshIndex) {
            std::int32_t primitive = -1;
            if (!*mesh ||
                !checkedComponentIndex(meshIndex, primitive)) {
                return false;
            }
            const MVector center = (*mesh)->getCenterDouble();
            if (!isFinitePoint(center)) return false;
            points.push_back(Imath::V3d(center[0], center[1], center[2]));
            identities.push_back(ComponentIdentity(primitive, -1));
        }
        return true;
    }

    if (componentType != D_COMP_POINT) return false;
    std::size_t totalPoints = 0U;
    if (!meshes.getCheckedTotalVertex(totalPoints) ||
        !dommy::attribute::validArrayConversionBudget(
            totalPoints, sizeof(Imath::V3d) + sizeof(ComponentIdentity))) {
        return false;
    }
    points.reserve(totalPoints);
    identities.reserve(totalPoints);
    std::size_t meshIndex = 0U;
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh, ++meshIndex) {
        std::int32_t primitive = -1;
        if (!*mesh ||
            !checkedComponentIndex(meshIndex, primitive)) {
            return false;
        }
        std::size_t pointIndex = 0U;
        for (ConstDPointIter point = (*mesh)->beginPt();
             point != (*mesh)->endPt(); ++point, ++pointIndex) {
            std::int32_t pointId = -1;
            std::array<double, 3> world;
            if (!checkedComponentIndex(pointIndex, pointId) ||
                !dommy::numeric::checkedTransformPoint(*point, (*mesh)->getTransformMatrix(), world)) {
                return false;
            }
            points.push_back(Imath::V3d(world[0], world[1], world[2]));
            identities.push_back(
                ComponentIdentity(primitive, pointId));
        }
    }
    return points.size() == totalPoints &&
           identities.size() == totalPoints;
}

} // namespace

const MTypeId DMConnectTargets::typeId(ID_DM_CONNECT_TARGETS);

MObject DMConnectTargets::inDM;
MObject DMConnectTargets::aType;
MObject DMConnectTargets::aSearchRadius;
MObject DMConnectTargets::aTargetDP;
MObject DMConnectTargets::aTargetDC;
MObject DMConnectTargets::aTargetDM;
MObject DMConnectTargets::aSourceComponentType;
MObject DMConnectTargets::aTargetComponentType;
MObject DMConnectTargets::aMaxSampleCount;
MObject DMConnectTargets::aMaxConnections;
MObject DMConnectTargets::aOffsetFromCentroid;
MObject DMConnectTargets::outDC;

void* DMConnectTargets::creator()
{
    return ::dommy::maya::createObject("DMConnectTargets", [&]() {
        return new DMConnectTargets();
    });
}

MStatus DMConnectTargets::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* sourceMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, sourceMeshes));
    if (!sourceMeshes || !sourceMeshes->hasValidMeshTopology()) {
        return MS::kFailure;
    }

    MFnPluginData pluginData;
    DCurveData* output = NULL;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
        pluginData, DCurveData::id, output));
    if (output == NULL) return MS::kFailure;
    DCurveOutputBatch outputBatch;

    const MDataHandle rawTargetTypeHandle = data.inputValue(aType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short rawTargetType = rawTargetTypeHandle.asShort();
    const MDataHandle rawSourceComponentHandle = data.inputValue(aSourceComponentType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short rawSourceComponent = rawSourceComponentHandle.asShort();
    const MDataHandle rawTargetComponentHandle = data.inputValue(aTargetComponentType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short rawTargetComponent = rawTargetComponentHandle.asShort();
    if (rawTargetType < DM_C_TAR_DPOINT ||
        rawTargetType >= DM_CONNECT_TARGET_TYPE_COUNT ||
        (rawSourceComponent != D_COMP_PRIM &&
         rawSourceComponent != D_COMP_POINT) ||
        (rawTargetComponent != D_COMP_PRIM &&
         rawTargetComponent != D_COMP_POINT)) {
        return MS::kInvalidParameter;
    }

    const DM_CONNECT_TARGET_TYPE targetType =
        static_cast<DM_CONNECT_TARGET_TYPE>(rawTargetType);
    const DComponentType sourceComponent =
        static_cast<DComponentType>(rawSourceComponent);
    const DComponentType targetComponent =
        static_cast<DComponentType>(rawTargetComponent);
    const MDataHandle searchRadiusHandle = data.inputValue(aSearchRadius, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double searchRadius = searchRadiusHandle.asDouble();
    const MDataHandle maximumSamplingHandle = data.inputValue(aMaxSampleCount, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int maximumSampling = maximumSamplingHandle.asInt();
    const MDataHandle maximumConnectionsHandle = data.inputValue(aMaxConnections, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int maximumConnections = maximumConnectionsHandle.asInt();
    const MDataHandle offsetFromCentroidHandle = data.inputValue(aOffsetFromCentroid, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double offsetFromCentroid = offsetFromCentroidHandle.asDouble();
    if (!std::isfinite(offsetFromCentroid) ||
        offsetFromCentroid < 0.0 || offsetFromCentroid > 1.0) {
        return MS::kInvalidParameter;
    }

    PointCloud<double> targetCloud;
    std::vector<ComponentIdentity> targetIdentities;
    bool targetConnected = false;
    if (targetType == DM_C_TAR_DPOINT) {
        MPlug targetPlug(thisMObject(), aTargetDP);
        targetConnected = targetPlug.isConnected(&status);
        if (!status) return status;
        if (targetConnected) {
            const DPData* target = nullptr;
            const MDataHandle handle = data.inputValue(aTargetDP, &status);
            if (!status) return status;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, target));
            if (!target || !target->hasValidPointTopology() ||
                !dommy::attribute::validArrayConversionBudget(target->numberOfPoints(),
                    sizeof(PointCloud<double>::Point) + sizeof(ComponentIdentity))) {
                return MS::kFailure;
            }
            targetCloud.pts.reserve(target->numberOfPoints());
            targetIdentities.reserve(target->numberOfPoints());
            std::size_t pointIndex = 0U;
            for (ConstDPointIter point = target->beginPt();
                 point != target->endPt(); ++point, ++pointIndex) {
                std::int32_t pointId = -1;
                if (!checkedComponentIndex(pointIndex, pointId) ||
                    !appendCloudPoint(targetCloud, *point)) {
                    return MS::kFailure;
                }
                targetIdentities.push_back(
                    ComponentIdentity(-1, pointId));
            }
        }
    } else if (targetType == DM_C_TAR_DCURVE) {
        MPlug targetPlug(thisMObject(), aTargetDC);
        targetConnected = targetPlug.isConnected(&status);
        if (!status) return status;
        if (targetConnected) {
            const DCurveData* target = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDC(aTargetDC, data, target));
            if (!target || !target->hasValidCurveTopology()) {
                return MS::kFailure;
            }
            if (targetComponent == D_COMP_POINT) {
                if (!appendCurvePoints(
                        *target, targetCloud, targetIdentities)) {
                    return MS::kFailure;
                }
            } else {
                DOMMY_RETURN_IF_FAILURE(appendCurveCenters(
                    *target, targetCloud, targetIdentities));
            }
        }
    } else {
        MPlug targetPlug(thisMObject(), aTargetDM);
        targetConnected = targetPlug.isConnected(&status);
        if (!status) return status;
        if (targetConnected) {
            const DMeshData* target = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDM(aTargetDM, data, target));
            if (!target || !target->hasValidMeshTopology()) {
                return MS::kFailure;
            }
            if (targetComponent == D_COMP_POINT) {
                if (!appendMeshPoints(
                        *target, targetCloud, targetIdentities)) {
                    return MS::kFailure;
                }
            } else if (!appendMeshCenters(
                           *target, targetCloud, targetIdentities)) {
                return MS::kFailure;
            }
        }
    }

    if (!targetConnected) {
        EchoWarning(name().asChar(), "Not connected target node.");
    }

    WorldPointList sourcePoints;
    std::vector<ComponentIdentity> sourceIdentities;
    if (!appendSourcePoints(
            *sourceMeshes, sourceComponent,
            sourcePoints, sourceIdentities)) {
        return MS::kFailure;
    }

    if (!sourcePoints.empty() && !targetCloud.pts.empty()) {
        dommy::spatial::SpatialSearchPlan plan;
        if (!dommy::spatial::buildSpatialSearchPlan(
                targetCloud.pts.size(), sourcePoints.size(),
                maximumSampling, maximumConnections,
                searchRadius, plan)) {
            return MS::kInvalidParameter;
        }
        if (targetIdentities.size() != plan.searchPointCount ||
            sourceIdentities.size() != plan.queryPointCount) {
            return MS::kFailure;
        }

        DPkdtree index(
            3, targetCloud,
            nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();
        IDsListArray neighborIds(sourcePoints.size());
        DScalarListArray neighborDistances(sourcePoints.size());
        WorldPointSearchKDTreeRadiusWithDistanceOp search(
            index, sourcePoints, maximumSampling, searchRadius,
            neighborIds, neighborDistances);
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(
                0U, sourcePoints.size(), 10000U),
            search);
        if (!dommy::spatial::validateSpatialNeighborLists(
                neighborIds, neighborDistances, plan)) {
            return MS::kFailure;
        }

        std::set<ConnectionKey> uniqueConnections;
        std::size_t outputCount = 0U;
        for (std::size_t sourceIndex = 0U;
             sourceIndex < sourcePoints.size(); ++sourceIndex) {
            std::vector<ConnectionCandidate> candidates;
            candidates.reserve(neighborIds[sourceIndex].size());
            for (std::size_t neighbor = 0U;
                 neighbor < neighborIds[sourceIndex].size();
                 ++neighbor) {
                const std::size_t targetIndex =
                    static_cast<std::size_t>(
                        neighborIds[sourceIndex][neighbor]);
                const ComponentIdentity& sourceIdentity =
                    sourceIdentities[sourceIndex];
                const ComponentIdentity& targetIdentity =
                    targetIdentities[targetIndex];
                ConnectionCandidate candidate;
                candidate.distance =
                    neighborDistances[sourceIndex][neighbor];
                candidate.key = std::make_tuple(
                    sourceIdentity.primitive,
                    sourceIdentity.point,
                    targetIdentity.primitive,
                    targetIdentity.point);
                candidate.first = sourcePoints[sourceIndex];
                const PointCloud<double>::Point& targetPoint =
                    targetCloud.pts[targetIndex];
                candidate.second = Imath::V3d(
                    targetPoint.x, targetPoint.y, targetPoint.z);
                candidates.push_back(candidate);
            }
            std::stable_sort(
                candidates.begin(), candidates.end(),
                [](const ConnectionCandidate& lhs,
                   const ConnectionCandidate& rhs) {
                    return lhs.distance < rhs.distance;
                });

            std::size_t considered = 0U;
            for (std::vector<ConnectionCandidate>::const_iterator
                     candidate = candidates.begin();
                 candidate != candidates.end() &&
                 considered < plan.maximumConnections;
                 ++candidate, ++considered) {
                if (!uniqueConnections.insert(candidate->key).second) {
                    continue;
                }
                ++outputCount;
                if (outputCount > plan.outputPairBudget ||
                    !dommy::curve::appendConnectionCurve(
                        outputBatch,
                        MPoint(candidate->first[0],
                               candidate->first[1],
                               candidate->first[2]),
                        MPoint(candidate->second[0],
                               candidate->second[1],
                               candidate->second[2]),
                        std::get<0>(candidate->key),
                        std::get<2>(candidate->key))) {
                    return MS::kFailure;
                }
            }
        }
    }

    if (!outputBatch.publishTo(*output) || !output->hasValidCurveTopology()) return MS::kFailure;
    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DCurveData>(
            outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMConnectTargets")

MStatus DMConnectTargets::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aTargetDP = typedAttribute.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    aTargetDC =
        typedAttribute.create("TargetDC", "tdc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDC));

    aTargetDM = typedAttribute.create("TargetDM", "tdm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDM));

    MFnEnumAttribute typeAttribute;
    aType = typeAttribute.create("Type", "typ", DM_C_TAR_DPOINT, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("DPoint", DM_C_TAR_DPOINT));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("DCurve", DM_C_TAR_DCURVE));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("DMesh", DM_C_TAR_DMESH));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));

    aSearchRadius =
        numericAttribute.create("SearchRadius", "srad", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(std::numeric_limits<double>::epsilon()));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchRadius));

    MFnEnumAttribute sourceComponentAttribute;
    aSourceComponentType =
        sourceComponentAttribute.create("SourceComponentType", "srctyp", D_COMP_PRIM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(sourceComponentAttribute.addField("Primitive", D_COMP_PRIM));
    DOMMY_RETURN_IF_FAILURE(sourceComponentAttribute.addField("Point", D_COMP_POINT));
    DOMMY_RETURN_IF_FAILURE(sourceComponentAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(sourceComponentAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceComponentType));

    MFnEnumAttribute targetComponentAttribute;
    aTargetComponentType =
        targetComponentAttribute.create("TargetComponentType", "tartyp", D_COMP_PRIM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(targetComponentAttribute.addField("Primitive", D_COMP_PRIM));
    DOMMY_RETURN_IF_FAILURE(targetComponentAttribute.addField("Point", D_COMP_POINT));
    DOMMY_RETURN_IF_FAILURE(targetComponentAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(targetComponentAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetComponentType));

    aMaxSampleCount =
        numericAttribute.create("MaxSampleCount", "mxsmpcnt", MFnNumericData::kInt, 100, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(dommy::spatial::kMaxSpatialSampling));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSampleCount));

    aMaxConnections =
        numericAttribute.create("MaxConnections", "mxcnn", MFnNumericData::kInt, 1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(dommy::spatial::kMaxSpatialConnections));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(16));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxConnections));

    aOffsetFromCentroid = numericAttribute.create("OffsetFromCentroid", "offrmct",
                                                  MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetFromCentroid));

    outDC = typedAttribute.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDM, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSourceComponentType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetComponentType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSearchRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxSampleCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxConnections, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetFromCentroid, outDC));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMConnectTargets", "initialize",
                                                  std::current_exception());
}
