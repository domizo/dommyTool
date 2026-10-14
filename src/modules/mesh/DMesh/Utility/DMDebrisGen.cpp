//
//  DMDebrisGen.cpp
//  dommyTool
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMDebrisGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaTriangleMesh.h"
#include "common/utility/DPointTransform.h"
#include "geometry/DPolygonTriangulator.h"

#include "DMeshDataHeaders.h"
#include "DMayaHeaders.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

const double kDebrisMaximumEdgeLength = 0.5;
const unsigned int kDebrisMaximumRefinementIterations = 12U;

bool buildFaceFrame(
    const DPointList& meshPoints,
    const MIntArray& connections,
    std::size_t connectionOffset,
    std::size_t faceVertexCount,
    dommy::geometry::PlaneFrame3& frame,
    std::vector<dommy::geometry::Point2>& polygon,
    std::size_t extraStorageBytes,
    dommy::geometry::PolygonWorkBudget& budget)
{
    if (faceVertexCount < 3U ||
        faceVertexCount >
            dommy::generation::kMaxGeneratedPointCount ||
        connectionOffset > connections.length() ||
        faceVertexCount > connections.length() - connectionOffset) {
        return false;
    }

    if (!dommy::geometry::validTriangleWorkspaceStorage(
            faceVertexCount, faceVertexCount - 2U, extraStorageBytes) ||
        !budget.consume(faceVertexCount)) return false;
    std::vector<dommy::geometry::Point3> points;
    points.reserve(faceVertexCount);
    for (std::size_t corner = 0U; corner < faceVertexCount; ++corner) {
        const int vertexIndex =
            connections[static_cast<unsigned int>(
                connectionOffset + corner)];
        if (vertexIndex < 0 ||
            static_cast<std::size_t>(vertexIndex) >= meshPoints.size()) {
            return false;
        }
        const DPoint& source =
            meshPoints[static_cast<std::size_t>(vertexIndex)];
        const dommy::geometry::Point3 point(
            static_cast<double>(source[0]),
            static_cast<double>(source[1]),
            static_cast<double>(source[2]));
        if (!dommy::geometry::isFinite(point)) return false;
        points.push_back(point);
    }
    return dommy::geometry::projectPlanarPolygon(
        points, frame, polygon, budget, extraStorageBytes);
}

bool appendTriangulatedFace(
    const DPointList& meshPoints,
    const MIntArray& connections,
    std::size_t connectionOffset,
    std::size_t faceVertexCount,
    MPointArray& outputPoints,
    MIntArray& outputFaceCounts,
    MIntArray& outputConnections,
    const dommy::mesh::MeshMergePlan& retainedStorage,
    dommy::geometry::PolygonWorkBudget& budget)
{
    dommy::mesh::MeshMergePlan stagedStorage = retainedStorage;
    std::size_t retainedBytes = 0U, stagedBytes = 0U;
    if (!dommy::mesh::meshOperationStorageBytes(retainedStorage, retainedBytes) ||
        !dommy::mesh::appendMeshMergePlan(outputPoints.length(), outputFaceCounts.length(),
            outputConnections.length(), 0U, stagedStorage) ||
        !dommy::mesh::meshOperationStorageBytes(stagedStorage, stagedBytes)) return false;
    dommy::geometry::PlaneFrame3 frame;
    std::vector<dommy::geometry::Point2> polygon;
    if (!buildFaceFrame(
            meshPoints, connections, connectionOffset, faceVertexCount,
            frame, polygon, stagedBytes, budget)) {
        return false;
    }

    dommy::geometry::PolygonTriangulation triangulation;
    if (!dommy::geometry::triangulateSimplePolygon(
            polygon, triangulation,
            dommy::generation::kMaxGeneratedPointCount,
            dommy::generation::kMaxGeneratedFaceCount,
            budget, 1.0e-10, stagedBytes) ||
        !dommy::geometry::refineTriangulation(
            triangulation, kDebrisMaximumEdgeLength,
            kDebrisMaximumRefinementIterations,
            dommy::generation::kMaxGeneratedPointCount,
            dommy::generation::kMaxGeneratedFaceCount, budget, stagedBytes)) {
        return false;
    }

    std::size_t workspaceBytes = 0U, extraBytes = 0U;
    if (!dommy::geometry::triangleWorkspaceBytes(triangulation.points.size(),
            triangulation.triangles.size(), workspaceBytes) ||
        !dommy::generation::checkedSum(retainedBytes, workspaceBytes,
            dommy::mesh::kMaxMeshOperationBytes, extraBytes)) return false;
    if (!budget.consume(triangulation.points.size()) ||
        !budget.consumeProduct(triangulation.triangles.size(), 7U)) return false;
    return dommy::maya::appendTriangleMeshChecked(
        triangulation.points.size(), [&](std::size_t i) {
            // Preserve source boundary positions instead of flattening or
            // introducing projection round-off into shared face corners.
            if (i < triangulation.sourceIndices.size()) {
                const std::size_t corner = connectionOffset + triangulation.sourceIndices[i];
                const DPoint& source = meshPoints[static_cast<std::size_t>(
                    connections[static_cast<unsigned int>(corner)])];
                return dommy::geometry::Point3(source[0], source[1], source[2]);
            }
            return dommy::geometry::unprojectPoint(frame, triangulation.points[i]);
        }, triangulation.triangles, outputPoints, outputFaceCounts, outputConnections, extraBytes);
}

} // namespace

const MTypeId DMDebrisGen::typeId(ID_DM_DEBRIS_GEN);

MObject DMDebrisGen::inDM;
MObject DMDebrisGen::outDM;

void* DMDebrisGen::creator()
{
    return ::dommy::maya::createObject("DMDebrisGen", [&]() {
        return new DMDebrisGen();
    });
}

MStatus DMDebrisGen::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (!input ||
        input->numberOfMesh() >
            dommy::generation::kMaxConnectedGeometryCount) {
        return MS::kInvalidParameter;
    }

    // Bound retained source geometry before creating any output staging.
    dommy::mesh::MeshMergePlan storagePlan;
    dommy::geometry::PolygonWorkBudget polygonBudget;
    for (auto mesh = input->beginMeshes(); mesh != input->endMeshes(); ++mesh) {
        if (!polygonBudget.consume(1U)) return MS::kFailure;
        if (!dommy::mesh::appendMeshMergePlan((*mesh)->numberOfPoints(),
                static_cast<std::size_t>((*mesh)->numFace()), (*mesh)->numberOfConnections(),
                (*mesh)->numberOfUVs(), storagePlan) ||
            !dommy::mesh::validMeshOperationStorage(storagePlan)) return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    pluginData.create(DMeshData::id, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DMeshData* const outputMeshes =
        dommy::maya::pluginDataPointer<DMeshData>(pluginData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!outputMeshes) return MS::kFailure;

    DMeshData::DMeshList generatedMeshes;
    generatedMeshes.reserve(input->numberOfMesh());
    std::size_t totalPointCount = 0U;
    std::size_t totalFaceCount = 0U;
    std::size_t totalConnectionCount = 0U;
    for (auto mesh = input->beginMeshes();
         mesh != input->endMeshes(); ++mesh) {
        if (!polygonBudget.consume(1U)) return MS::kFailure;
        const MMatrix sourceTransform = (*mesh)->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(sourceTransform))
            return MS::kInvalidParameter;
        const DPointList* const points = (*mesh)->getPositionPtrConst();
        const MIntArray& faceCounts = (*mesh)->getFaceList();
        const MIntArray& connections = (*mesh)->getConnectList();
        if (!points) return MS::kInvalidParameter;

        MPointArray outputPoints;
        MIntArray outputFaceCounts;
        MIntArray outputConnections;
        std::size_t connectionOffset = 0U;
        for (unsigned int face = 0U;
             face < faceCounts.length(); ++face) {
            if (!polygonBudget.consume(1U)) return MS::kFailure;
            const int rawFaceVertexCount = faceCounts[face];
            if (rawFaceVertexCount < 3) return MS::kInvalidParameter;
            const std::size_t faceVertexCount =
                static_cast<std::size_t>(rawFaceVertexCount);
            if (!appendTriangulatedFace(
                    *points, connections, connectionOffset,
                    faceVertexCount, outputPoints, outputFaceCounts,
                    outputConnections, storagePlan, polygonBudget)) {
                MGlobal::displayError(
                    "DMDebrisGen requires finite, planar, simple faces within its safety budgets.");
                return MS::kFailure;
            }
            if (connectionOffset >
                std::numeric_limits<std::size_t>::max() -
                    faceVertexCount) {
                return MS::kFailure;
            }
            connectionOffset += faceVertexCount;
        }
        if (connectionOffset != connections.length()) {
            return MS::kInvalidParameter;
        }

        std::size_t nextPointCount = 0U;
        std::size_t nextFaceCount = 0U;
        std::size_t nextConnectionCount = 0U;
        if (!dommy::generation::checkedSum(
                totalPointCount, outputPoints.length(),
                dommy::generation::kMaxGeneratedPointCount,
                nextPointCount) ||
            !dommy::generation::checkedSum(
                totalFaceCount, outputFaceCounts.length(),
                dommy::generation::kMaxGeneratedFaceCount,
                nextFaceCount) ||
            !dommy::generation::checkedSum(
                totalConnectionCount, outputConnections.length(),
                dommy::generation::kMaxWorkItemCount,
                nextConnectionCount)) {
            MGlobal::displayError(
                "DMDebrisGen total output exceeds the generation budget.");
            return MS::kFailure;
        }
        if (!dommy::mesh::appendMeshMergePlan(outputPoints.length(), outputFaceCounts.length(),
                outputConnections.length(), 0U, storagePlan) ||
            !dommy::mesh::validMeshOperationStorage(storagePlan)) return MS::kFailure;
        DMItem candidate(new DMeshInternalData(
            outputPoints, outputFaceCounts, outputConnections));
        // Geometry is rebuilt in local space; retain the primitive placement.
        candidate->setTransformMatrix(sourceTransform);
        generatedMeshes.push_back(candidate);
        totalPointCount = nextPointCount;
        totalFaceCount = nextFaceCount;
        totalConnectionCount = nextConnectionCount;
    }
    // The common batch transaction validates/copies/bounds the output once.
    // Keep earlier candidates private until every source has succeeded.
    if (!outputMeshes->insert(generatedMeshes)) return MS::kFailure;
    if (generatedMeshes.empty()) outputMeshes->updateBBox();

    MDataHandle output = data.outputValue(outDM, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishTypedPluginDataObject<DMeshData>(
            output, pluginData.object(), DMeshData::id));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMDebrisGen")

MStatus DMDebrisGen::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    return MS::kSuccess;
}
