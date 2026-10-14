//
//  DMConnetedPieces.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/19.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMConnetedPieces.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DSpatialSearchPlan.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributeArrayConversion.h"

#include "DMayaHeaders.h"

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

typedef std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>
    VertexPair;
typedef std::pair<std::size_t, std::size_t> PrimitivePair;

bool collectMeshPoints(const DMeshData& meshes,
                       PointCloud<double>& cloud,
                       WorldPointList& points,
                       DPIndexList& meshIndices,
                       DPIndexList& vertexIndices)
{
    std::size_t pointCount = 0U;
    if (!meshes.getCheckedTotalVertex(pointCount) ||
        pointCount > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::attribute::validArrayConversionBudget(pointCount,
            2U * sizeof(Imath::V3d) + 2U * sizeof(std::size_t))) {
        return false;
    }

    cloud.pts.resize(pointCount);
    points.resize(pointCount);
    meshIndices.resize(pointCount);
    vertexIndices.resize(pointCount);

    std::size_t pointIndex = 0U;
    std::size_t meshIndex = 0U;
    for (DMeshData::ConstDMeshIter mesh = meshes.beginMeshes();
         mesh != meshes.endMeshes(); ++mesh, ++meshIndex) {
        if (!*mesh || !(*mesh)->hasValidMeshData()) return false;

        std::size_t vertexIndex = 0U;
        for (ConstDPointIter point = (*mesh)->beginPt();
             point != (*mesh)->endPt(); ++point, ++vertexIndex) {
            if (pointIndex >= pointCount ||
                !std::isfinite((*point)[0]) ||
                !std::isfinite((*point)[1]) ||
                !std::isfinite((*point)[2])) {
                return false;
            }

            std::array<double, 3> world;
            if (!dommy::numeric::checkedTransformPoint(*point, (*mesh)->getTransformMatrix(), world))
                return false;
            cloud.pts[pointIndex].x = world[0];
            cloud.pts[pointIndex].y = world[1];
            cloud.pts[pointIndex].z = world[2];
            points[pointIndex] = Imath::V3d(world[0], world[1], world[2]);
            meshIndices[pointIndex] = meshIndex;
            vertexIndices[pointIndex] = vertexIndex;
            ++pointIndex;
        }
    }
    return pointIndex == pointCount;
}

VertexPair normalizedVertexPair(std::size_t firstMesh,
                                std::size_t firstVertex,
                                std::size_t secondMesh,
                                std::size_t secondVertex)
{
    if (std::make_pair(firstMesh, firstVertex) >
        std::make_pair(secondMesh, secondVertex)) {
        std::swap(firstMesh, secondMesh);
        std::swap(firstVertex, secondVertex);
    }
    return std::make_tuple(
        firstMesh, firstVertex, secondMesh, secondVertex);
}

bool buildVertexPairs(const IDsListArray& neighborIds,
                      const DScalarListArray& neighborDistances,
                      const DPIndexList& meshIndices,
                      const DPIndexList& vertexIndices,
                      bool allowSameMesh,
                      const dommy::spatial::SpatialSearchPlan& plan,
                      std::vector<VertexPair>& outputPairs)
{
    if (!dommy::spatial::validateSpatialNeighborLists(
            neighborIds, neighborDistances, plan) ||
        meshIndices.size() != plan.queryPointCount ||
        vertexIndices.size() != plan.queryPointCount) {
        return false;
    }

    std::set<VertexPair> uniquePairs;
    std::vector<VertexPair> result;
    for (std::size_t query = 0U; query < neighborIds.size(); ++query) {
        typedef std::pair<double, VertexPair> Candidate;
        std::vector<Candidate> candidates;
        candidates.reserve(neighborIds[query].size());
        for (std::size_t neighbor = 0U;
             neighbor < neighborIds[query].size(); ++neighbor) {
            const std::size_t target =
                static_cast<std::size_t>(neighborIds[query][neighbor]);
            if (query == target ||
                (!allowSameMesh &&
                 meshIndices[query] == meshIndices[target])) {
                continue;
            }

            candidates.push_back(Candidate(
                static_cast<double>(neighborDistances[query][neighbor]),
                normalizedVertexPair(
                    meshIndices[query], vertexIndices[query],
                    meshIndices[target], vertexIndices[target])));
        }
        std::stable_sort(
            candidates.begin(), candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
                return lhs.first < rhs.first;
            });

        std::size_t considered = 0U;
        for (std::vector<Candidate>::const_iterator candidate =
                 candidates.begin();
             candidate != candidates.end() &&
             considered < plan.maximumConnections;
             ++candidate, ++considered) {
            if (uniquePairs.insert(candidate->second).second) {
                result.push_back(candidate->second);
                if (result.size() > plan.outputPairBudget) return false;
            }
        }
    }

    outputPairs.swap(result);
    return true;
}

bool buildPrimitivePairs(
    const IDsListArray& neighborIds,
    const DScalarListArray& neighborDistances,
    const dommy::spatial::SpatialSearchPlan& plan,
    std::vector<PrimitivePair>& outputPairs)
{
    if (!dommy::spatial::validateSpatialNeighborLists(
            neighborIds, neighborDistances, plan)) {
        return false;
    }

    std::set<PrimitivePair> uniquePairs;
    std::vector<PrimitivePair> result;
    for (std::size_t query = 0U; query < neighborIds.size(); ++query) {
        typedef std::pair<double, PrimitivePair> Candidate;
        std::vector<Candidate> candidates;
        candidates.reserve(neighborIds[query].size());
        for (std::size_t neighbor = 0U;
             neighbor < neighborIds[query].size(); ++neighbor) {
            const std::size_t target =
                static_cast<std::size_t>(neighborIds[query][neighbor]);
            if (query == target) continue;
            candidates.push_back(Candidate(
                static_cast<double>(neighborDistances[query][neighbor]),
                std::minmax(query, target)));
        }
        std::stable_sort(
            candidates.begin(), candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
                return lhs.first < rhs.first;
            });

        std::size_t considered = 0U;
        for (std::vector<Candidate>::const_iterator candidate =
                 candidates.begin();
             candidate != candidates.end() &&
             considered < plan.maximumConnections;
             ++candidate, ++considered) {
            if (uniquePairs.insert(candidate->second).second) {
                result.push_back(candidate->second);
                if (result.size() > plan.outputPairBudget) return false;
            }
        }
    }

    outputPairs.swap(result);
    return true;
}

} // namespace

const MTypeId DMConnetedPieces::typeId(ID_DM_CONNECT_PIECES);

MObject DMConnetedPieces::inDM;
MObject DMConnetedPieces::aType;
MObject DMConnetedPieces::aSearchRadius;
MObject DMConnetedPieces::aMaxSampleCount;
MObject DMConnetedPieces::aMaxConnections;
MObject DMConnetedPieces::aOffsetFromCentroid;
MObject DMConnetedPieces::outDC;

void* DMConnetedPieces::creator()
{
    return ::dommy::maya::createObject("DMConnetedPieces", [&]() {
        return new DMConnetedPieces();
    });
}

MStatus DMConnetedPieces::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (!input || !input->hasValidMeshTopology()) {
        return MS::kFailure;
    }

    MFnPluginData pluginData;
    DCurveData* output = NULL;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
        pluginData, DCurveData::id, output));
    if (output == NULL) return MS::kFailure;
    DCurveOutputBatch outputBatch;

    MDataHandle typeHandle = data.inputValue(aType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short rawType = typeHandle.asShort();
    if (rawType < DM_C_POINT_PIECE ||
        rawType >= DM_CONNECT_PIECE_TYPE_COUNT) {
        return MS::kInvalidParameter;
    }
    const DM_CONNECT_PIECE_TYPE type =
        static_cast<DM_CONNECT_PIECE_TYPE>(rawType);

    MDataHandle radiusHandle =
        data.inputValue(aSearchRadius, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double searchRadius =
        static_cast<double>(radiusHandle.asFloat());
    MDataHandle samplingHandle =
        data.inputValue(aMaxSampleCount, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int maximumSampling = samplingHandle.asInt();
    MDataHandle connectionsHandle =
        data.inputValue(aMaxConnections, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int maximumConnections = connectionsHandle.asInt();
    MDataHandle offsetHandle =
        data.inputValue(aOffsetFromCentroid, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double offsetFromCentroid =
        static_cast<double>(offsetHandle.asFloat());
    if (!std::isfinite(offsetFromCentroid) ||
        offsetFromCentroid < 0.0 || offsetFromCentroid > 1.0) {
        return MS::kInvalidParameter;
    }

    if (input->numberOfMesh() != 0U) {
        if (type == DM_C_POINT_PIECE || type == DM_C_ALL_POINT) {
            PointCloud<double> cloud;
            WorldPointList points;
            DPIndexList meshIndices;
            DPIndexList vertexIndices;
            if (!collectMeshPoints(
                    *input, cloud, points, meshIndices, vertexIndices)) {
                return MS::kFailure;
            }

            if (!points.empty()) {
                dommy::spatial::SpatialSearchPlan plan;
                if (!dommy::spatial::buildSpatialSearchPlan(
                        points.size(), points.size(),
                        maximumSampling, maximumConnections,
                        searchRadius, plan)) {
                    return MS::kInvalidParameter;
                }

                DPkdtree index(
                    3, cloud,
                    nanoflann::KDTreeSingleIndexAdaptorParams(10));
                index.buildIndex();

                IDsListArray neighborIds(points.size());
                DScalarListArray neighborDistances(points.size());
                WorldPointSearchKDTreeRadiusWithDistanceOp search(
                    index, points, maximumSampling, searchRadius,
                    neighborIds, neighborDistances);
                tbb::parallel_for(
                    tbb::blocked_range<std::size_t>(
                        0U, points.size(), 10000U),
                    search);

                std::vector<VertexPair> pairs;
                if (!buildVertexPairs(
                        neighborIds, neighborDistances,
                        meshIndices, vertexIndices,
                        type == DM_C_ALL_POINT, plan, pairs)) {
                    return MS::kFailure;
                }

                for (std::vector<VertexPair>::const_iterator pair =
                         pairs.begin();
                     pair != pairs.end(); ++pair) {
                    const std::size_t firstMesh = std::get<0>(*pair);
                    const std::size_t firstVertex = std::get<1>(*pair);
                    const std::size_t secondMesh = std::get<2>(*pair);
                    const std::size_t secondVertex = std::get<3>(*pair);
                    if (firstMesh >= input->numberOfMesh() ||
                        secondMesh >= input->numberOfMesh() ||
                        firstVertex >=
                            input->getMesh(firstMesh).numberOfPoints() ||
                        secondVertex >=
                            input->getMesh(secondMesh).numberOfPoints()) {
                        return MS::kFailure;
                    }

                    const DPoint& first =
                        (*input->getMesh(firstMesh).getPositionPtrConst())
                            [firstVertex];
                    const DPoint& second =
                        (*input->getMesh(secondMesh).getPositionPtrConst())
                            [secondVertex];
                    std::array<double, 3> firstWorld, secondWorld;
                    if (!dommy::numeric::checkedTransformPoint(first,
                            input->getMesh(firstMesh).getTransformMatrix(), firstWorld) ||
                        !dommy::numeric::checkedTransformPoint(second,
                            input->getMesh(secondMesh).getTransformMatrix(), secondWorld))
                        return MS::kInvalidParameter;
                    if (!dommy::curve::appendConnectionCurve(
                            outputBatch,
                            MPoint(firstWorld[0], firstWorld[1], firstWorld[2]),
                            MPoint(secondWorld[0], secondWorld[1], secondWorld[2]),
                            static_cast<std::int32_t>(firstMesh),
                            static_cast<std::int32_t>(secondMesh))) {
                        return MS::kFailure;
                    }
                }
            }
        } else {
            const std::size_t meshCount = input->numberOfMesh();
            if (!dommy::attribute::validArrayConversionBudget(meshCount,
                    2U * sizeof(Imath::V3d) + sizeof(MObject))) return MS::kInvalidParameter;
            dommy::spatial::SpatialSearchPlan plan;
            if (!dommy::spatial::buildSpatialSearchPlan(
                    meshCount, meshCount,
                    maximumSampling, maximumConnections,
                    searchRadius, plan)) {
                return MS::kInvalidParameter;
            }

            PointCloud<double> cloud;
            cloud.pts.resize(meshCount);
            WorldPointList centers(meshCount);
            const bool useSurfaceOffset =
                offsetFromCentroid >
                std::numeric_limits<double>::epsilon();
            std::vector<MObject> meshObjects;
            if (useSurfaceOffset) meshObjects.reserve(meshCount);

            std::size_t meshIndex = 0U;
            for (DMeshData::ConstDMeshIter mesh = input->beginMeshes();
                 mesh != input->endMeshes(); ++mesh, ++meshIndex) {
                if (!*mesh || !(*mesh)->hasValidMeshData()) {
                    return MS::kFailure;
                }
                const MVector center = (*mesh)->getCenterDouble();
                if (!std::isfinite(center[0]) ||
                    !std::isfinite(center[1]) ||
                    !std::isfinite(center[2])) {
                    return MS::kFailure;
                }
                cloud.pts[meshIndex].x = center[0];
                cloud.pts[meshIndex].y = center[1];
                cloud.pts[meshIndex].z = center[2];
                centers[meshIndex] = Imath::V3d(center[0], center[1], center[2]);

                if (useSurfaceOffset) {
                    MObject meshObject;
                    DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(
                        **mesh, meshObject, DMeshPointSpace::Transformed));
                    meshObjects.push_back(meshObject);
                }
            }

            DPkdtree index(
                3, cloud,
                nanoflann::KDTreeSingleIndexAdaptorParams(10));
            index.buildIndex();

            IDsListArray neighborIds(meshCount);
            DScalarListArray neighborDistances(meshCount);
            WorldPointSearchKDTreeRadiusWithDistanceOp search(
                index, centers, maximumSampling, searchRadius,
                neighborIds, neighborDistances);
            tbb::parallel_for(
                tbb::blocked_range<std::size_t>(
                    0U, meshCount, 10000U),
                search);

            std::vector<PrimitivePair> pairs;
            if (!buildPrimitivePairs(
                    neighborIds, neighborDistances, plan, pairs)) {
                return MS::kFailure;
            }

            for (std::vector<PrimitivePair>::const_iterator pair =
                     pairs.begin();
                 pair != pairs.end(); ++pair) {
                const std::size_t firstIndex = pair->first;
                const std::size_t secondIndex = pair->second;
                MPoint first(
                    centers[firstIndex][0],
                    centers[firstIndex][1],
                    centers[firstIndex][2]);
                MPoint second(
                    centers[secondIndex][0],
                    centers[secondIndex][1],
                    centers[secondIndex][2]);

                if (useSurfaceOffset) {
                    dommy::direction::Delta direction;
                    if (!dommy::direction::difference(first, second, direction)) return MS::kInvalidParameter;
                    const MVector firstDirection(direction.values);
                    const MVector secondDirection = -firstDirection;
                    if (direction.length >
                        std::numeric_limits<double>::epsilon()) {
                        MFnMesh firstMesh(
                            meshObjects[firstIndex], &status);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                        MPointArray hitPoints;
                        const bool firstHit = firstMesh.intersect(first, firstDirection, hitPoints,
                            kMFnMeshPointTolerance, MSpace::kObject, nullptr, &status);
                        if (!status) return status;
                        if (firstHit && hitPoints.length() != 0U) {
                            for (unsigned int axis = 0U; axis < 3U; ++axis)
                                if (!dommy::attribute::checkedLinearBlend(first[axis],
                                        hitPoints[hitPoints.length() - 1U][axis], offsetFromCentroid, first[axis]))
                                    return MS::kInvalidParameter;
                        }

                        MFnMesh secondMesh(
                            meshObjects[secondIndex], &status);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                        DOMMY_RETURN_IF_FAILURE(hitPoints.clear());
                        const bool secondHit = secondMesh.intersect(second, secondDirection, hitPoints,
                            kMFnMeshPointTolerance, MSpace::kObject, nullptr, &status);
                        if (!status) return status;
                        if (secondHit && hitPoints.length() != 0U) {
                            for (unsigned int axis = 0U; axis < 3U; ++axis)
                                if (!dommy::attribute::checkedLinearBlend(second[axis],
                                        hitPoints[hitPoints.length() - 1U][axis], offsetFromCentroid, second[axis]))
                                    return MS::kInvalidParameter;
                        }
                    }
                }

                if (!dommy::curve::appendConnectionCurve(
                        outputBatch, first, second,
                        static_cast<std::int32_t>(firstIndex),
                        static_cast<std::int32_t>(secondIndex))) {
                    return MS::kFailure;
                }
            }
        }
    }

    if (!outputBatch.publishTo(*output) || !output->hasValidCurveTopology() ||
        output->numberOfCurves() >
            dommy::generation::kMaxGeneratedCurveCount) {
        return MS::kFailure;
    }

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DCurveData>(
            outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMConnetedPieces")

MStatus DMConnetedPieces::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute typeAttribute;
    aType = typeAttribute.create("Type", "typ", DM_C_PRIM_CENTER_PIECE, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("Primitive Point", DM_C_POINT_PIECE));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("Primitive Center", DM_C_PRIM_CENTER_PIECE));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.addField("All Point", DM_C_ALL_POINT));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(typeAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));

    aSearchRadius =
        numericAttribute.create("SearchRadius", "srad", MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(std::numeric_limits<float>::epsilon()));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchRadius));

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
                                                  MFnNumericData::kFloat, 0.0, &status);
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
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSearchRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxSampleCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxConnections, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetFromCentroid, outDC));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMConnetedPieces", "initialize",
                                                  std::current_exception());
}
