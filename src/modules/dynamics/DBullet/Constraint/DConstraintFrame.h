#pragma once

#include "Collision/DCollisionMeshInput.h"
#include "Solver/DPassiveTransformMath.h"
#include "LinearMath/btTransform.h"

namespace dommy {
namespace bullet {

inline bool finiteConstraintTransform(const btTransform& value)
{
    if (!isFiniteVec3(value.getOrigin())) return false;
    for (int row = 0; row < 3; ++row) {
        if (!isFiniteVec3(value.getBasis()[row])) return false;
    }
    return true;
}

// Compute both sides before committing either native frame. Finite input alone
// does not guarantee that inverse-transform arithmetic remains representable.
inline bool prepareConstraintPivots(
    const vec3f& position, const btTransform& bodyA, const btTransform& bodyB,
    btVector3& pivotA, btVector3& pivotB)
{
    if (!isFiniteVec3(position) || !finiteConstraintTransform(bodyA) ||
        !finiteConstraintTransform(bodyB)) return false;
    const btVector3 world(position[0], position[1], position[2]);
    const btVector3 candidateA = bodyA.inverse() * world;
    const btVector3 candidateB = bodyB.inverse() * world;
    if (!isFiniteVec3(candidateA) || !isFiniteVec3(candidateB)) return false;
    pivotA = candidateA;
    pivotB = candidateB;
    return true;
}

inline bool prepareConstraintFrames(
    const vec3f& position, const quatf& rotation,
    const btTransform& bodyA, const btTransform& bodyB,
    btTransform& frameA, btTransform& frameB)
{
    quatf normalized;
    if (!isFiniteVec3(position) || !normalizeQuaternion(rotation, normalized) ||
        !finiteConstraintTransform(bodyA) || !finiteConstraintTransform(bodyB)) return false;
    const btTransform world(
        btQuaternion(normalized[1], normalized[2], normalized[3], normalized[0]),
        btVector3(position[0], position[1], position[2]));
    const btTransform candidateA = bodyA.inverse() * world;
    const btTransform candidateB = bodyB.inverse() * world;
    if (!finiteConstraintTransform(candidateA) || !finiteConstraintTransform(candidateB)) return false;
    frameA = candidateA;
    frameB = candidateB;
    return true;
}

} // namespace bullet
} // namespace dommy
