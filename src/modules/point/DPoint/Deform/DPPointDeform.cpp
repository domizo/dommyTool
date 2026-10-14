//
//  DPPointDeform.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/18.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPPointDeform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DPointDeformMath.h"
#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "DPointOp.h"
#include "DTransformOp.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

MMatrix zeroMatrix()
{
    double values[4][4] = {};
    return MMatrix(values);
}

// Account for retained neighbors, detached geometry, matrices and copied point
// metadata before any bulk read/copy. The shared four-copy staging allowance
// also covers temporary neighbor vectors and an estimated KD-tree workspace;
// it is not a cap on Maya/TBB/native allocator memory.
void preflightPointDeform(const DPData& input, std::size_t vertices,
                          std::size_t connections)
{
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(input, sizeof(DPoint), workspace);
    const std::size_t queries = input.numberOfPoints();
    if (queries == 0U) return;
    std::size_t neighbors = 0U;
    if (!dommy::generation::checkedProduct(queries, std::min(vertices, std::size_t(100)),
            dommy::generation::kMaxWorkItemCount, neighbors))
        throw std::length_error("DPPointDeform neighbor count exceeds its budget");
    workspace.add(queries, sizeof(DPoint) + sizeof(IDs) +
                  sizeof(DScalarList) + sizeof(DDoubleArray));
    workspace.add(vertices, 2U * sizeof(MFloatPoint) + sizeof(PointCloud<double>::Point) +
                  sizeof(MMatrix) + 64U);
    workspace.add(neighbors, sizeof(uint32_t) + 2U * sizeof(double));
    workspace.add(connections, sizeof(int));
}

} // namespace

const MTypeId DPPointDeform::typeId(ID_DP_POINT_DEFORM);

MObject DPPointDeform::inDP;
MObject DPPointDeform::inRestMesh;
MObject DPPointDeform::inAnimMesh;
MObject DPPointDeform::aRadius;
MObject DPPointDeform::outDP;

void* DPPointDeform::creator()
{
    return ::dommy::maya::createObject("DPPointDeform", [&]() {
        return new DPPointDeform();
    });
}

MStatus DPPointDeform::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    MObject restObject, animatedObject;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getConnectedMeshObject(
        thisMObject(), inRestMesh, data, restObject));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getConnectedMeshObject(
        thisMObject(), inAnimMesh, data, animatedObject));
    MFnMesh restMesh(restObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    MFnMesh animatedMesh(animatedObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int restCount = restMesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int animatedCount = animatedMesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t vertices = 0U;
    if (!dommy::generation::checkedCount(restCount,
            dommy::generation::kMaxGeneratedPointCount, vertices) ||
        restCount != animatedCount) return MS::kInvalidParameter;

    const MDataHandle radiusHandle = data.inputValue(aRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float radius = radiusHandle.asFloat();
    if (!std::isfinite(radius) || radius <= 0.0f) return MS::kInvalidParameter;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kInvalidParameter;

    std::size_t connections = 0U;
    if (input->numberOfPoints() != 0U) {
        if (vertices == 0U) return MS::kInvalidParameter;
        const int edges = restMesh.numEdges(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t edgeCount = 0U;
        if (!dommy::generation::checkedCount(edges,
                dommy::generation::kMaxWorkItemCount, edgeCount) ||
            !dommy::generation::checkedProduct(edgeCount, 2U,
                dommy::generation::kMaxWorkItemCount, connections))
            return MS::kInvalidParameter;
    }
    preflightPointDeform(*input, vertices, connections);

    DPointList stagedPoints;
    if (input->numberOfPoints() != 0U) {
        MFloatPointArray restPoints, animatedPoints;
        // Preserve the archived world-space query convention. Native Maya
        // connection/transform behavior remains part of the runtime gate.
        DOMMY_RETURN_IF_FAILURE(restMesh.getPoints(restPoints, MSpace::kWorld));
        DOMMY_RETURN_IF_FAILURE(animatedMesh.getPoints(animatedPoints, MSpace::kWorld));
        if (restPoints.length() != vertices || animatedPoints.length() != vertices)
            return MS::kFailure;
        for (unsigned int i = 0U; i < restPoints.length(); ++i)
            if (!dommy::maya::isFinitePoint(restPoints[i]) ||
                !dommy::maya::isFinitePoint(animatedPoints[i]))
                return MS::kInvalidParameter;

        stagedPoints = input->getPositionConst();
        PointCloud<double> cloud;
        CreateSearchDataMFP(restPoints, cloud);
        DPkdtree index(3, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();
        IDsListArray neighborIds(stagedPoints.size());
        DScalarListArray neighborDistances(stagedPoints.size());
        PointSearchKDTreeRadiusWithDistanceOp search(
            index, stagedPoints, 100, radius, neighborIds, neighborDistances);
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0U, stagedPoints.size(), 10000U),
                          search);

        std::vector<DDoubleArray> weights(stagedPoints.size());
        for (std::size_t i = 0U; i < stagedPoints.size(); ++i) {
            double effectiveRadius = radius;
            if (neighborIds[i].size() != neighborDistances[i].size()) return MS::kFailure;
            if (neighborIds[i].empty()) {
                // Retain the legacy one-neighbor fallback and finite search
                // extent. A point with no match remains unchanged.
                DPointList query(1U, stagedPoints[i]);
                IDsListArray ids(1U);
                DScalarListArray distances(1U);
                PointSearchKDTreeRadiusWithDistanceOp nearest(
                    index, query, 1, 1e15, ids, distances);
                nearest(tbb::blocked_range<std::size_t>(0U, 1U));
                neighborIds[i].swap(ids[0]);
                neighborDistances[i].swap(distances[0]);
                if (!neighborDistances[i].empty())
                    effectiveRadius = 1.1 * neighborDistances[i].back();
            }
            if (neighborIds[i].size() != neighborDistances[i].size() ||
                neighborIds[i].size() > std::min(vertices, std::size_t(100)))
                return MS::kFailure;
            weights[i].reserve(neighborIds[i].size());
            for (std::size_t j = 0U; j < neighborIds[i].size(); ++j) {
                double weight = 0.0;
                if (neighborIds[i][j] >= vertices ||
                    !dommy::point::radialFalloffWeight(
                        neighborDistances[i][j], effectiveRadius, weight))
                    return MS::kInvalidParameter;
                weights[i].push_back(weight);
            }
        }

        MMatrixArray transforms;
        DOMMY_RETURN_IF_FAILURE(transforms.setLength(static_cast<unsigned int>(vertices)));
        if (transforms.length() != vertices) return MS::kFailure;
        // Isolated/degenerate vertices keep the legacy translation-only path.
        for (unsigned int i = 0U; i < transforms.length(); ++i)
            transforms[i] = MMatrix::identity;

        MItMeshVertex vertex(restObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        std::size_t visitedConnections = 0U;
        status = dommy::maya::visitMeshElements(vertex, vertices,
            [&](MItMeshVertex& current) -> MStatus {
                const int vertexId = current.index(&status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (!dommy::memory::indexInRange(vertexId, vertices))
                    return MS::kInvalidParameter;
                MIntArray ids;
                DOMMY_RETURN_IF_FAILURE(current.getConnectedVertices(ids));
                if (!dommy::generation::checkedSum(visitedConnections, ids.length(),
                        connections, visitedConnections)) return MS::kInvalidParameter;
                for (unsigned int i = 0U; i < ids.length(); ++i)
                    if (!dommy::memory::indexInRange(ids[i], vertices))
                        return MS::kInvalidParameter;
                if (ids.length() < 2U) return MS::kSuccess;

                const unsigned int center = static_cast<unsigned int>(vertexId);
                const unsigned int last = static_cast<unsigned int>(ids[ids.length() - 1U]);
                const MVector animatedCenter(animatedPoints[center]);
                MVector lastAnimated = MVector(animatedPoints[last]) - animatedCenter;
                MVector lastRest = MVector(restPoints[last]) - MVector(restPoints[center]);
                MMatrix totalTransform = zeroMatrix();
                double totalWeight = 0.0;
                for (unsigned int i = 0U; i < ids.length(); ++i) {
                    const unsigned int id = static_cast<unsigned int>(ids[i]);
                    const MVector animated = MVector(animatedPoints[id]) - animatedCenter;
                    const MVector rest = MVector(restPoints[id]) - MVector(restPoints[center]);
                    const MVector up = rest ^ lastRest;
                    const MVector animatedUp = animated ^ lastAnimated;
                    const double weight = std::sqrt(up.length() * animatedUp.length());
                    if (dommy::point::canNormalizeWeight(weight)) {
                        MTransformationMatrix animatedFrame, restFrame;
                        DMakeTransform(animated, animatedUp, animatedFrame);
                        DMakeTransform(rest, up, restFrame);
                        const MMatrix weighted = weight * restFrame.asRotateMatrix().inverse() *
                                                 animatedFrame.asRotateMatrix();
                        if (!dommy::maya::isFiniteMatrix(weighted)) return MS::kFailure;
                        totalTransform += weighted;
                        totalWeight += weight;
                    }
                    lastAnimated = animated;
                    lastRest = rest;
                }
                if (dommy::point::canNormalizeWeight(totalWeight)) {
                    DMatrixDivide(totalWeight, totalTransform);
                    if (!dommy::maya::isFiniteMatrix(totalTransform)) return MS::kFailure;
                    transforms[center] = totalTransform;
                }
                return MS::kSuccess;
            });
        DOMMY_RETURN_IF_FAILURE(status);

        for (std::size_t point = 0U; point < stagedPoints.size(); ++point) {
            double totalWeight = 0.0;
            MVector delta(0.0, 0.0, 0.0);
            for (std::size_t i = 0U; i < neighborIds[point].size(); ++i) {
                const unsigned int id = neighborIds[point][i];
                const MVector center(restPoints[id]);
                MVector moved = DPToMV(stagedPoints[point]) - center;
                moved *= transforms[id];
                moved += center;
                moved += MVector(animatedPoints[id]) - center;
                const MVector displacement = moved - DPToMV(stagedPoints[point]);
                if (!dommy::maya::isFiniteVector(displacement)) return MS::kFailure;
                delta += displacement * weights[point][i];
                totalWeight += weights[point][i];
            }
            if (!dommy::point::canNormalizeWeight(totalWeight)) continue;
            delta /= totalWeight;
            DPoint moved;
            if (!dommy::point::offsetAlongVector(
                    stagedPoints[point], delta, 1.0, 1.0, 1.0, moved))
                return MS::kInvalidParameter;
            stagedPoints[point] = moved;
        }
    }

    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, &stagedPoints](DPData& output) -> MStatus {
            return output.copyRepeatedFrom(*input, 1U,
                [&stagedPoints](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                    point = stagedPoints[index];
                    return MS::kSuccess;
                });
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPPointDeform")

MStatus DPPointDeform::initialize() try
{
    MStatus status;
    MFnNumericAttribute numeric;
    MFnGenericAttribute generic;
    MFnTypedAttribute typed;

    inDP = typed.create("inDP", "idp", DPData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typed.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typed.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    inRestMesh = generic.create("inRestMesh", "irstmsh", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inRestMesh));

    inAnimMesh = generic.create("inAnimMesh", "ianmmsh", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(generic.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(generic.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(generic.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(generic.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inAnimMesh));

    aRadius = numeric.create("Radius", "rad", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numeric.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(numeric.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numeric.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(numeric.setMin(1.0e-6f));
    DOMMY_RETURN_IF_FAILURE(numeric.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    outDP = typed.create("outDP", "odp", DPData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typed.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typed.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inRestMesh, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inAnimMesh, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDP));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPPointDeform", "initialize", std::current_exception());
}
