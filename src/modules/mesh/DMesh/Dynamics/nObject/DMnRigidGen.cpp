// DMnRigidGen.cpp — created by yuzo, 2017.
#include "common/utility/DMayaEntryPointGuard.h"
#include "DMnRigidGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMeshInputValidation.h"
#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"

#include <cmath>
#include <vector>

namespace {

MStatus buildRigidMesh(const DMeshData& input, MFnMesh& meshFn, MObject& owner)
{
    dommy::mesh::MeshMergePlan plan;
    for (auto it = input.beginMeshes(); it != input.endMeshes(); ++it) {
        if (!*it || !dommy::mesh::appendMeshMergePlan(
                (*it)->numberOfPoints(), (*it)->numFace(),
                (*it)->numberOfConnections(), 0U, plan)) return MS::kInvalidParameter;
    }
    if (plan.points == 0U || plan.faces == 0U ||
        !dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;

    MPointArray points;
    MIntArray faces, connections;
    DOMMY_RETURN_IF_FAILURE(points.setLength(static_cast<unsigned int>(plan.points)));
    DOMMY_RETURN_IF_FAILURE(faces.setLength(static_cast<unsigned int>(plan.faces)));
    DOMMY_RETURN_IF_FAILURE(connections.setLength(static_cast<unsigned int>(plan.connections)));
    if (points.length() != plan.points || faces.length() != plan.faces ||
        connections.length() != plan.connections) return MS::kFailure;

    unsigned int pointOffset = 0U, faceOffset = 0U, connectionOffset = 0U;
    for (auto it = input.beginMeshes(); it != input.endMeshes(); ++it) {
        const auto& mesh = **it;
        MIntArray sourceFaces, sourceConnections;
        DOMMY_RETURN_IF_FAILURE(mesh.copyTopology(sourceFaces, sourceConnections));
        if (sourceFaces.length() != static_cast<std::size_t>(mesh.numFace()) ||
            sourceConnections.length() != mesh.numberOfConnections()) return MS::kFailure;
        // Retain historical local coordinates; do not bake primitive transforms.
        unsigned int point = pointOffset;
        for (auto p = mesh.beginPt(); p != mesh.endPt(); ++p) {
            if (point >= points.length()) return MS::kFailure;
            const MPoint value((*p)[0], (*p)[1], (*p)[2]);
            if (!dommy::maya::isFloatRepresentablePoint(value)) return MS::kInvalidParameter;
            points[point++] = value;
        }
        if (point - pointOffset != mesh.numberOfPoints()) return MS::kFailure;
        for (unsigned int i = 0U; i < sourceFaces.length(); ++i)
            faces[faceOffset++] = sourceFaces[i];
        for (unsigned int i = 0U; i < sourceConnections.length(); ++i)
            connections[connectionOffset++] = sourceConnections[i] + static_cast<int>(pointOffset);
        pointOffset = point;
    }
    if (pointOffset != plan.points || faceOffset != plan.faces ||
        connectionOffset != plan.connections) return MS::kFailure;
    MStatus status;
    MFnMeshData creator;
    owner = creator.create(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (owner.isNull()) return MS::kFailure;
    const MObject mesh = meshFn.create(static_cast<int>(plan.points), static_cast<int>(plan.faces),
        points, faces, connections, owner, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    return mesh.isNull() ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
}

MStatus readRigidTopology(MFnMesh& meshFn, const dommy::mesh::MeshMergePlan& plan,
                          MIntArray& faces, MIntArray& connections, std::vector<int>& edges)
{
    MStatus status;
    const int rawEdges = meshFn.numEdges(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t edgeCount = 0U, edgeEntries = 0U, edgeBytes = 0U;
    if (!dommy::generation::checkedCount(rawEdges, plan.connections, edgeCount) ||
        !dommy::generation::checkedProduct(edgeCount, 2U,
            dommy::generation::kMaxWorkItemCount, edgeEntries) ||
        !dommy::generation::checkedProduct(edgeEntries, sizeof(int),
            dommy::mesh::kMaxMeshOperationBytes, edgeBytes) ||
        !dommy::mesh::validMeshOperationStorage(plan, edgeBytes)) return MS::kInvalidParameter;

    DOMMY_RETURN_IF_FAILURE(meshFn.getVertices(faces, connections));
    if (faces.length() != plan.faces || connections.length() != plan.connections)
        return MS::kFailure;
    std::size_t total = 0U;
    for (unsigned int i = 0U; i < faces.length(); ++i) {
        if (faces[i] < 3 || !dommy::generation::checkedSum(total,
                static_cast<std::size_t>(faces[i]), plan.connections, total))
            return MS::kInvalidParameter;
    }
    if (total != plan.connections) return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < connections.length(); ++i)
        if (connections[i] < 0 || static_cast<std::size_t>(connections[i]) >= plan.points)
            return MS::kInvalidParameter;

    edges.resize(edgeEntries);
    for (int i = 0; i < rawEdges; ++i) {
        int2 edge = {0, 0};
        DOMMY_RETURN_IF_FAILURE(meshFn.getEdgeVertices(i, edge));
        if (!dommy::mesh::validEdgeVertexIndices(edge[0], edge[1], plan.points))
            return MS::kInvalidParameter;
        edges[2U * static_cast<std::size_t>(i)] = edge[0];
        edges[2U * static_cast<std::size_t>(i) + 1U] = edge[1];
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DMnRigidGen::typeId(ID_DM_NOBJECT_GEN);
MObject DMnRigidGen::inDM;
MObject DMnRigidGen::aIsPassive;
MObject DMnRigidGen::aThickness;
MObject DMnRigidGen::aBounce;
MObject DMnRigidGen::aFriction;
MObject DMnRigidGen::aCollideVertex;
MObject DMnRigidGen::aCollideEdge;
MObject DMnRigidGen::aCollideFace;
MObject DMnRigidGen::outNObject;

DMnRigidGen::DMnRigidGen() : creationStatus_(fNObject.createNRigid()) {}

void* DMnRigidGen::creator() {
    return ::dommy::maya::createObject("DMnRigidGen", [&]() {
        return new DMnRigidGen();
    });
}

MStatus DMnRigidGen::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outNObject) return MS::kUnknownParameter;
    DOMMY_RETURN_IF_FAILURE(creationStatus_);
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (input->numberOfMesh() == 0U) return MS::kInvalidParameter;
    MFnMesh meshFn;
    MObject meshOwner;
    DOMMY_RETURN_IF_FAILURE(buildRigidMesh(*input, meshFn, meshOwner));
    dommy::mesh::MeshMergePlan plan;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(meshFn, plan));
    MTime time;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, time));
    if (!std::isfinite(time.value())) return MS::kInvalidParameter;
    MStatus status;
    const MDataHandle passiveHandle = data.inputValue(aIsPassive, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool passive = passiveHandle.asBool();
    unsigned int oldCount = 0U;
    // setTopology must precede other MnRigid calls after creation.
    if (topologyInitialized_)
        DOMMY_RETURN_IF_FAILURE(fNObject.getNumVertices(oldCount));
    const bool continuing = topologyInitialized_ && oldCount == plan.points && time.value() > 1.0;
    if (!passive || !continuing) {
        const MDataHandle thicknessHandle = data.inputValue(aThickness, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float thickness = thicknessHandle.asFloat();
        const MDataHandle bounceHandle = data.inputValue(aBounce, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float bounce = bounceHandle.asFloat();
        const MDataHandle frictionHandle = data.inputValue(aFriction, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float friction = frictionHandle.asFloat();
        if (!std::isfinite(thickness) || !std::isfinite(bounce) || !std::isfinite(friction))
            return MS::kInvalidParameter;
        const MDataHandle vertexHandle = data.inputValue(aCollideVertex, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const bool collideVertex = vertexHandle.asBool();
        const MDataHandle edgeHandle = data.inputValue(aCollideEdge, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const bool collideEdge = edgeHandle.asBool();
        const MDataHandle faceHandle = data.inputValue(aCollideFace, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const bool collideFace = faceHandle.asBool();

        MFloatPointArray points, previous, velocities;
        DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(points, MSpace::kObject));
        if (points.length() != plan.points) return MS::kFailure;
        for (unsigned int i = 0U; i < points.length(); ++i)
            if (!dommy::maya::isFinitePoint(points[i])) return MS::kInvalidParameter;
        double dt = 1.0;
        if (continuing) {
            DOMMY_RETURN_IF_FAILURE(fNObject.getPositions(previous));
            if (previous.length() != plan.points) return MS::kFailure;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationStepSeconds(data, dt));
        }
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(plan.points,
            [&](unsigned int i, MFloatPoint& velocity) -> MStatus {
                velocity = MFloatPoint(0.0f, 0.0f, 0.0f, 0.0f);
                if (!continuing) return MS::kSuccess;
                if (!dommy::maya::isFinitePoint(previous[i]) ||
                    !dommy::numeric::checkedFloatCast((double(points[i].x) - previous[i].x) / dt, velocity.x) ||
                    !dommy::numeric::checkedFloatCast((double(points[i].y) - previous[i].y) / dt, velocity.y) ||
                    !dommy::numeric::checkedFloatCast((double(points[i].z) - previous[i].z) / dt, velocity.z))
                    return MS::kInvalidParameter;
                return MS::kSuccess;
            }, velocities));

        MIntArray faces, connections;
        std::vector<int> edges;
        if (!continuing)
            DOMMY_RETURN_IF_FAILURE(readRigidTopology(meshFn, plan, faces, connections, edges));
        // Native setters are not transactional. After a failed setter, the next
        // evaluation reinitializes topology instead of trusting partial state.
        topologyInitialized_ = false;
        if (!continuing) {
            DOMMY_RETURN_IF_FAILURE(fNObject.setTopology(static_cast<int>(plan.faces),
                &faces[0], &connections[0], static_cast<int>(edges.size() / 2U),
                edges.empty() ? nullptr : edges.data()));
            DOMMY_RETURN_IF_FAILURE(fNObject.setPositions(points, true));
        }
        DOMMY_RETURN_IF_FAILURE(fNObject.setVelocities(velocities));
        DOMMY_RETURN_IF_FAILURE(fNObject.setThickness(thickness));
        DOMMY_RETURN_IF_FAILURE(fNObject.setBounce(bounce));
        DOMMY_RETURN_IF_FAILURE(fNObject.setFriction(friction));
        DOMMY_RETURN_IF_FAILURE(fNObject.setCollisionFlags(collideVertex, collideEdge, collideFace));
        topologyInitialized_ = true;
    }

    MFnNObjectData outputData;
    const MObject object = outputData.create(); // This API has no MStatus result.
    if (object.isNull()) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputData.setObject(object));
    DOMMY_RETURN_IF_FAILURE(outputData.setObjectPtr(&fNObject));
    DOMMY_RETURN_IF_FAILURE(outputData.setCached(false));
    MDataHandle output = data.outputValue(outNObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(output.set(outputData.object()));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMnRigidGen")

MStatus DMnRigidGen::initialize() try
{
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    aIsPassive = nAttr.create("IsPassive", "ispas", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsPassive));
    aThickness = nAttr.create("Thickness", "thick", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aThickness));
    aBounce = nAttr.create("Bounce", "bnc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBounce));
    aFriction = nAttr.create("Friction", "frc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFriction));
    aCollideVertex = nAttr.create("CollideVertex", "colvtx", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideVertex));
    aCollideEdge = nAttr.create("CollideEdge", "cole", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideEdge));
    aCollideFace = nAttr.create("CollideFace", "colf", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollideFace));
    outNObject = tAttr.create("outNObject", "onobj", MFnData::kNObject, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsPassive, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aThickness, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBounce, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFriction, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollideVertex, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollideEdge, outNObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollideFace, outNObject));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMnRigidGen", "initialize", std::current_exception());
}
