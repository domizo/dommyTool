#pragma once

#include <cfloat>
#include <cmath>
#include <cstddef>

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>

#include "DBRigidBodyNode.h"
#include "Collision/DCollisionMeshInput.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DNumericValidation.h"
#include "Solver/DPassiveTransformMath.h"
#include "dbSolver_t.h"
#include "mathUtils.h"

namespace dommy {
namespace bullet {
namespace constraint_node {

inline MStatus connectedRigidBody(
    const MObject& owner,
    const MObject& attribute,
    bool required,
    dbRigidBody_t::pointer& rigidBody)
{
    rigidBody.reset();

    MStatus status;
    const MPlug input(owner, attribute);
    MPlugArray sources;
    const bool hasConnection =
        input.connectedTo(sources, true, false, &status);
    if (!status) {
        return status;
    }
    if (!hasConnection || sources.length() == 0U) {
        return required ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
    }
    if (sources.length() != 1U) {
        return MS::kInvalidParameter;
    }

    MFnDependencyNode dependencyNode(sources[0].node(), &status);
    if (!status) {
        return status;
    }
    if (dependencyNode.typeId() != DBRigidBodyNode::typeId) {
        return MS::kInvalidParameter;
    }

    DBRigidBodyNode* const rigidBodyNode =
        dommy::maya::checkedUserNode<DBRigidBodyNode>(
            dependencyNode, DBRigidBodyNode::typeId);
    if (rigidBodyNode == nullptr) {
        return MS::kFailure;
    }
    return rigidBodyNode->evaluate_rigid_body(rigidBody);
}

inline MStatus finiteFloat(
    MDataBlock& data, const MObject& attribute, float& value)
{
    value = 0.0F;
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) {
        return status;
    }
    const double input = handle.asDouble();
    return dommy::numeric::checkedFloatCast(input, value)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

inline MStatus boolean(
    MDataBlock& data, const MObject& attribute, bool& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) {
        return status;
    }
    value = handle.asBool();
    return MS::kSuccess;
}

inline MStatus finiteVector(
    MDataBlock& data, const MObject& attribute, vec3f& value)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(attribute, &status);
    if (!status) {
        return status;
    }
    const float3& input = handle.asFloat3();
    for (int component = 0; component < 3; ++component) {
        if (!std::isfinite(input[component])) {
            return MS::kInvalidParameter;
        }
        value[component] = input[component];
    }
    return MS::kSuccess;
}

inline bool checkedPosition(const MVector& input, vec3f& output)
{
    vec3f candidate;
    if (!dommy::numeric::checkedFloatCast(input.x, candidate[0]) ||
        !dommy::numeric::checkedFloatCast(input.y, candidate[1]) ||
        !dommy::numeric::checkedFloatCast(input.z, candidate[2])) return false;
    output = candidate;
    return true;
}

inline bool checkedRotation(const MQuaternion& input, quatf& output)
{
    quatf candidate;
    return dommy::numeric::checkedFloatCast(input.w, candidate[0]) &&
           dommy::numeric::checkedFloatCast(input.x, candidate[1]) &&
           dommy::numeric::checkedFloatCast(input.y, candidate[2]) &&
           dommy::numeric::checkedFloatCast(input.z, candidate[3]) &&
           dommy::bullet::normalizeQuaternion(candidate, output);
}

inline MStatus finiteRotation(
    MDataBlock& data, const MObject& attribute, quatf& value)
{
    vec3f degrees;
    MStatus status = finiteVector(data, attribute, degrees);
    if (!status) {
        return status;
    }

    const MEulerRotation euler(
        deg2rad(degrees[0]), deg2rad(degrees[1]), deg2rad(degrees[2]));
    const MQuaternion quaternion = euler.asQuaternion();
    return checkedRotation(quaternion, value)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

inline MStatus publishBoolean(
    const MPlug& plug, MDataBlock& data, const MObject& outputAttribute)
{
    MStatus status;
    MDataHandle output = data.outputValue(outputAttribute, &status);
    if (!status) {
        return status;
    }
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}

inline bool finiteFrame(const vec3f& pivot, const quatf& rotation)
{
    return dommy::bullet::isFiniteVec3(pivot) &&
           dommy::bullet::hasValidQuaternion(rotation);
}

inline MStatus writeFrame(
    MDataBlock& data,
    const MObject& pivotAttribute,
    const MObject& rotationAttribute,
    const vec3f& pivot,
    const quatf& rotation)
{
    if (!finiteFrame(pivot, rotation)) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MDataHandle pivotHandle = data.outputValue(pivotAttribute, &status);
    if (!status) {
        return status;
    }
    const MQuaternion quaternion(
        rotation[1], rotation[2], rotation[3], rotation[0]);
    const MEulerRotation euler = quaternion.asEulerRotation();
    if (!std::isfinite(euler.x) ||
        !std::isfinite(euler.y) ||
        !std::isfinite(euler.z)) {
        return MS::kInvalidParameter;
    }

    MDataHandle rotationHandle =
        data.outputValue(rotationAttribute, &status);
    if (!status) {
        return status;
    }
    // Acquire and validate both outputs before changing either value.
    float3& pivotValue = pivotHandle.asFloat3();
    for (int component = 0; component < 3; ++component) {
        pivotValue[component] = pivot[component];
    }
    float3& rotationValue = rotationHandle.asFloat3();
    rotationValue[0] = rad2deg(static_cast<float>(euler.x));
    rotationValue[1] = rad2deg(static_cast<float>(euler.y));
    rotationValue[2] = rad2deg(static_cast<float>(euler.z));
    return MS::kSuccess;
}

template <typename ConstraintPointer>
MStatus commitConstraint(
    ConstraintPointer& liveConstraint,
    const ConstraintPointer& replacement,
    bool disableCollisions)
{
    if (!replacement) {
        return MS::kFailure;
    }

    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    dbConstraint_t::pointer replacementBase = replacement;
    if (!dbSolver_t::add_constraint(
            replacementBase, disableCollisions)) {
        return MS::kFailure;
    }

    ConstraintPointer previous = liveConstraint;
    liveConstraint = replacement;
    if (previous) {
        dbConstraint_t::pointer previousBase = previous;
        dbSolver_t::remove_constraint(previousBase);
    }
    return MS::kSuccess;
}

}  // namespace constraint_node
}  // namespace bullet
}  // namespace dommy
