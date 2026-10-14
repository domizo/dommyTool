//
//  DMRemesh.cpp
//  dommyTool
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMRemesh.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaTriangleMesh.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DPointTransform.h"
#include "geometry/DMeshRemesher.h"
#include "geometry/DPolygonTriangulator.h"

#include "DMeshDataHeaders.h"
#include "DMayaHeaders.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

const unsigned int kMaximumRemeshIterations = 100U;
const unsigned int kMaximumFaceRefinementIterations = 16U;

bool appendTriangleMesh(
    const dommy::geometry::TriangleMesh& mesh,
    MPointArray& points,
    MIntArray& faceCounts,
    MIntArray& connections,
    const dommy::mesh::MeshMergePlan& retainedStorage,
    dommy::generation::WorkBudget& budget)
{
    std::size_t retainedBytes = 0U, workspaceBytes = 0U, extraBytes = 0U;
    if (!dommy::mesh::meshOperationStorageBytes(retainedStorage, retainedBytes) ||
        !dommy::geometry::triangleWorkspaceBytes(mesh.points.size(), mesh.triangles.size(), workspaceBytes) ||
        !dommy::generation::checkedSum(retainedBytes, workspaceBytes,
            dommy::mesh::kMaxMeshOperationBytes, extraBytes)) return false;
    if (!budget.consume(mesh.points.size()) ||
        !budget.consumeProduct(mesh.triangles.size(), 7U)) return false;
    return dommy::maya::appendTriangleMeshChecked(
        mesh.points.size(), [&](std::size_t i) { return mesh.points[i]; },
        mesh.triangles, points, faceCounts, connections, extraBytes);
}

bool convertMayaTriangles(
    const MPointArray& points,
    const MIntArray& triangleVertices,
    dommy::geometry::TriangleMesh& mesh,
    dommy::generation::WorkBudget& budget)
{
    if (points.length() < 3U ||
        points.length() >
            dommy::generation::kMaxGeneratedPointCount ||
        triangleVertices.length() % 3U != 0U ||
        triangleVertices.length() / 3U >
            dommy::generation::kMaxGeneratedFaceCount) {
        return false;
    }
    dommy::geometry::TriangleMesh candidate;
    if (!budget.consume(points.length()) ||
        !budget.consume(triangleVertices.length())) return false;
    candidate.points.reserve(points.length());
    for (unsigned int index = 0U; index < points.length(); ++index) {
        const dommy::geometry::Point3 point(
            points[index].x, points[index].y, points[index].z);
        if (!dommy::geometry::isFinite(point)) return false;
        candidate.points.push_back(point);
    }
    candidate.triangles.reserve(triangleVertices.length() / 3U);
    for (unsigned int index = 0U;
         index < triangleVertices.length(); index += 3U) {
        const int first = triangleVertices[index];
        const int second = triangleVertices[index + 1U];
        const int third = triangleVertices[index + 2U];
        if (first < 0 || second < 0 || third < 0) return false;
        candidate.triangles.push_back(dommy::geometry::Triangle(
            static_cast<std::size_t>(first),
            static_cast<std::size_t>(second),
            static_cast<std::size_t>(third)));
    }
    mesh.swap(candidate);
    return true;
}

bool buildProjectedFace(
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
        points.push_back(dommy::geometry::Point3(
            static_cast<double>(source[0]),
            static_cast<double>(source[1]),
            static_cast<double>(source[2])));
    }
    return dommy::geometry::projectPlanarPolygon(
        points, frame, polygon, budget, extraStorageBytes);
}

bool appendFaceTriangulation(
    const DPointList& meshPoints,
    const MIntArray& connections,
    std::size_t connectionOffset,
    std::size_t faceVertexCount,
    double maximumEdgeLength,
    unsigned int relaxationIterations,
    double relaxation,
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
    if (!buildProjectedFace(
            meshPoints, connections, connectionOffset,
            faceVertexCount, frame, polygon, stagedBytes, budget)) {
        return false;
    }

    dommy::geometry::PolygonTriangulation triangulation;
    if (!dommy::geometry::triangulateSimplePolygon(
            polygon, triangulation,
            dommy::generation::kMaxGeneratedPointCount,
            dommy::generation::kMaxGeneratedFaceCount,
            budget, 1.0e-10, stagedBytes) ||
        !dommy::geometry::refineTriangulation(
            triangulation, maximumEdgeLength,
            kMaximumFaceRefinementIterations,
            dommy::generation::kMaxGeneratedPointCount,
            dommy::generation::kMaxGeneratedFaceCount, budget, stagedBytes)) {
        return false;
    }
    if (relaxationIterations > 0U) {
        if (!dommy::geometry::relaxInteriorVertices(
                triangulation, relaxationIterations, budget, relaxation, stagedBytes)) return false;
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
            // sourceIndices still describes the original boundary prefix;
            // refinement appends only new points. Do not round-trip corners
            // through the best-fit plane, even for float-rounding residuals.
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

const MTypeId DMRemesh::typeId(ID_DM_REMESH);

MObject DMRemesh::inDM;
MObject DMRemesh::aMethodType;
MObject DMRemesh::aEdgeLength;
MObject DMRemesh::aAspectBound;
MObject DMRemesh::aIterate;
MObject DMRemesh::outDM;

void* DMRemesh::creator()
{
    return ::dommy::maya::createObject("DMRemesh", [&]() {
        return new DMRemesh();
    });
}

MStatus DMRemesh::compute(const MPlug& plug, MDataBlock& data) try
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

    const MDataHandle methodHandle = data.inputValue(aMethodType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short method = methodHandle.asShort();
    const MDataHandle edgeLengthHandle = data.inputValue(aEdgeLength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double edgeLength = static_cast<double>(edgeLengthHandle.asFloat());
    const MDataHandle aspectBoundHandle = data.inputValue(aAspectBound, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double aspectBound = static_cast<double>(aspectBoundHandle.asFloat());
    const MDataHandle rawIterationsHandle = data.inputValue(aIterate, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int rawIterations = rawIterationsHandle.asInt();
    if ((method != 0 && method != 1) ||
        !std::isfinite(edgeLength) || edgeLength <= 0.0 ||
        !std::isfinite(aspectBound) ||
        aspectBound < 0.0 || aspectBound > 1.0 ||
        rawIterations < 1 ||
        rawIterations >
            static_cast<int>(kMaximumRemeshIterations)) {
        return MS::kInvalidParameter;
    }
    const unsigned int iterations =
        static_cast<unsigned int>(rawIterations);

    // Reject oversized staging before native conversion or plug-in-data creation.
    dommy::mesh::MeshMergePlan storagePlan;
    dommy::geometry::PolygonWorkBudget polygonBudget;
    for (auto mesh = input->beginMeshes(); mesh != input->endMeshes(); ++mesh) {
        if (!polygonBudget.consume(1U) ||
            !dommy::mesh::appendMeshMergePlan((*mesh)->numberOfPoints(),
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
    // PolygonWorkBudget aliases the common WorkBudget used by both methods.
    for (auto mesh = input->beginMeshes();
         mesh != input->endMeshes(); ++mesh) {
        if (!polygonBudget.consume(1U)) return MS::kFailure;
        const MMatrix sourceTransform = (*mesh)->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(sourceTransform))
            return MS::kInvalidParameter;
        MPointArray outputPoints;
        MIntArray outputFaceCounts;
        MIntArray outputConnections;

        if (method == 0) {
            std::size_t twiceFaces = 0U, workspaceBytes = 0U;
            if (!dommy::generation::checkedProduct(static_cast<std::size_t>((*mesh)->numFace()),
                    2U, dommy::generation::kMaxWorkItemCount, twiceFaces) ||
                twiceFaces > (*mesh)->numberOfConnections() ||
                !dommy::geometry::triangleWorkspaceBytes((*mesh)->numberOfPoints(),
                    (*mesh)->numberOfConnections() - twiceFaces, workspaceBytes) ||
                !dommy::mesh::validMeshOperationStorage(storagePlan, workspaceBytes))
                return MS::kInvalidParameter;
            if (!polygonBudget.consumeProduct((*mesh)->numberOfPoints(), 2U) ||
                !polygonBudget.consumeProduct(static_cast<std::size_t>((*mesh)->numFace()), 2U) ||
                !polygonBudget.consumeProduct((*mesh)->numberOfConnections(), 2U) ||
                !polygonBudget.consumeProduct((*mesh)->numberOfUVs(), 2U)) return MS::kFailure;
            MObject meshObject;
            status = generateMMeshFromDmeshWithUV(*mesh->get(), meshObject);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (meshObject.isNull()) return MS::kFailure;
            MFnMesh meshFn(meshObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const int rawPoints = meshFn.numVertices(&status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            std::size_t pointCount = 0U, faceCount = 0U, triangleCount = 0U;
            if (!dommy::generation::checkedCount(rawPoints,
                    dommy::generation::kMaxGeneratedPointCount, pointCount))
                return MS::kInvalidParameter;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshTriangles(
                meshFn, meshObject, faceCount, triangleCount));
            if (!dommy::geometry::triangleWorkspaceBytes(pointCount, triangleCount, workspaceBytes) ||
                !dommy::mesh::validMeshOperationStorage(storagePlan, workspaceBytes))
                return MS::kInvalidParameter;
            MPointArray mayaPoints;
            MIntArray triangleCounts;
            MIntArray triangleVertices;
            // Charge bulk native staging/validation before retrieving arrays.
            // Maya's internal triangulation is still host-owned work.
            if (!polygonBudget.consume(pointCount) || !polygonBudget.consume(faceCount) ||
                !polygonBudget.consumeProduct(triangleCount, 6U)) return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(
                meshFn.getPoints(mayaPoints, MSpace::kObject));
            DOMMY_RETURN_IF_FAILURE(
                meshFn.getTriangles(
                    triangleCounts, triangleVertices));
            if (!dommy::maya::validMeshTriangleArrays(mayaPoints, triangleCounts,
                    triangleVertices, pointCount, faceCount, triangleCount))
                return MS::kInvalidParameter;

            dommy::geometry::TriangleMesh sourceMesh;
            dommy::geometry::TriangleMesh remeshed;
            if (!convertMayaTriangles(
                    mayaPoints, triangleVertices, sourceMesh, polygonBudget) ||
                !dommy::geometry::isotropicRemesh(
                    sourceMesh, edgeLength, iterations, remeshed,
                    dommy::generation::kMaxGeneratedPointCount,
                    dommy::generation::kMaxGeneratedFaceCount,
                    polygonBudget) ||
                !appendTriangleMesh(
                    remeshed, outputPoints, outputFaceCounts,
                    outputConnections, storagePlan, polygonBudget)) {
                MGlobal::displayError(
                    "DMRemesh could not produce a bounded isotropic mesh.");
                return MS::kFailure;
            }
        } else {
            const DPointList* const points =
                (*mesh)->getPositionPtrConst();
            const MIntArray faceCounts = (*mesh)->getFaceList();
            const MIntArray connections = (*mesh)->getConnectList();
            if (!points) return MS::kInvalidParameter;

            const double relaxation =
                std::max(0.05, std::min(0.75, 0.25 + aspectBound));
            std::size_t connectionOffset = 0U;
            for (unsigned int face = 0U;
                 face < faceCounts.length(); ++face) {
                if (!polygonBudget.consume(1U)) return MS::kFailure;
                const int rawFaceVertexCount = faceCounts[face];
                if (rawFaceVertexCount < 3) {
                    return MS::kInvalidParameter;
                }
                const std::size_t faceVertexCount =
                    static_cast<std::size_t>(rawFaceVertexCount);
                if (!appendFaceTriangulation(
                        *points, connections, connectionOffset,
                        faceVertexCount, edgeLength,
                        iterations > 1U ? iterations - 1U : 0U,
                        relaxation, outputPoints, outputFaceCounts,
                        outputConnections, storagePlan, polygonBudget)) {
                    MGlobal::displayError(
                        "DMRemesh Face Triangled requires finite, planar, simple faces within its safety budgets.");
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
                "DMRemesh total output exceeds the generation budget.");
            return MS::kFailure;
        }
        // Account for owned output copies and subsequent topology/bounds scans.
        if (!dommy::mesh::appendMeshMergePlan(outputPoints.length(), outputFaceCounts.length(),
                outputConnections.length(), 0U, storagePlan) ||
            !dommy::mesh::validMeshOperationStorage(storagePlan)) return MS::kFailure;
        if (method == 0 && (!polygonBudget.consumeProduct(outputPoints.length(), 3U) ||
            !polygonBudget.consumeProduct(outputFaceCounts.length(), 2U) ||
            !polygonBudget.consumeProduct(outputConnections.length(), 2U))) return MS::kFailure;
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
DOMMY_COMPUTE_CATCH("DMRemesh")

MStatus DMRemesh::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    MFnEnumAttribute methodAttribute;
    aMethodType = methodAttribute.create(
        "MethodType", "mtdty", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        methodAttribute.addField("Isotropic", 0));
    DOMMY_RETURN_IF_FAILURE(
        methodAttribute.addField("Face Triangled", 1));
    DOMMY_RETURN_IF_FAILURE(methodAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(methodAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMethodType));

    aEdgeLength = numericAttribute.create(
        "MaxRange", "mxr", MFnNumericData::kFloat, 1.0F, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.01F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.1F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeLength));

    aAspectBound = numericAttribute.create(
        "AspectBound", "aptbnd", MFnNumericData::kFloat,
        0.125F, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(1.0F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(0.3F));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAspectBound));

    aIterate = numericAttribute.create(
        "Iterate", "itr", MFnNumericData::kInt, 1, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(
        static_cast<int>(kMaximumRemeshIterations)));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterate));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMethodType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEdgeLength, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAspectBound, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterate, outDM));
    return MS::kSuccess;
}
