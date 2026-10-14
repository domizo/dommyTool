#pragma once

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>

#include "DBCollisionShapeNode.h"
#include "common/utility/DMayaUserNode.h"
#include "dbSolver_t.h"

namespace dommy {
namespace bullet {

// A default shape is appropriate only for a successfully queried, unconnected
// input. A broken connection must not silently replace an existing simulation.
inline MStatus resolveCollisionShapeInput(
    const MPlug& input, dbCollisionShape_t::pointer& shape)
{
    MObject update;
    MStatus status = input.getValue(update);
    if (!status) return status;

    MPlugArray sources;
    const bool connected = input.connectedTo(sources, true, false, &status);
    if (!status) return status;

    dbCollisionShape_t::pointer candidate;
    if (!connected && sources.length() == 0U) {
        candidate = dbSolver_t::create_sphere_shape();
    } else {
        if (!connected || sources.length() != 1U) {
            return MS::kInvalidParameter;
        }
        const MObject source = sources[0].node(&status);
        if (!status) return status;
        MFnDependencyNode dependencyNode(source, &status);
        if (!status) return status;
        DBCollisionShapeNode* const shapeNode =
            dommy::maya::checkedUserNode<DBCollisionShapeNode>(
                dependencyNode, DBCollisionShapeNode::typeId, &status);
        if (!status) return status;
        if (shapeNode == nullptr) return MS::kFailure;
        status = shapeNode->evaluate_collision_shape(candidate);
        if (!status) return status;
    }
    if (!candidate) return MS::kFailure;
    shape = candidate;
    return MS::kSuccess;
}

} // namespace bullet
} // namespace dommy
