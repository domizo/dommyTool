#pragma once

#include "DBulletDynamicsWorld.h"
#include "common/utility/DParticleFieldMath.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MDoubleArray.h>
#include <maya/MFnField.h>
#include <maya/MItDag.h>
#include <maya/MVectorArray.h>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dommy {
namespace bullet {

inline MStatus appendFieldSample(const MVector& position, const MVector& velocity,
                                 double mass, MVectorArray& positions,
                                 MVectorArray& velocities, MDoubleArray& masses)
{
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!std::isfinite(position[axis]) || !std::isfinite(velocity[axis]))
            return MS::kInvalidParameter;
    if (!std::isfinite(mass) || mass <= 0) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(positions.append(position));
    DOMMY_RETURN_IF_FAILURE(velocities.append(velocity));
    return masses.append(mass);
}

inline MStatus accumulateFieldSample(const MVectorArray& values,
                                     std::vector<MVector>& totals)
{
    if (values.length() != totals.size()) return MS::kFailure;
    for (std::size_t i = 0; i < totals.size(); ++i)
        for (unsigned axis = 0; axis < 3; ++axis)
            if (!field::checkedAccumulateFieldForce(
                    totals[i][axis], values[static_cast<unsigned>(i)][axis], totals[i][axis]))
                return MS::kInvalidParameter;
    return MS::kSuccess;
}

inline bool fieldVector(const MVector& value, btVector3& output)
{
    float x, y, z;
    if (!numeric::checkedFloatCast(value.x, x) ||
        !numeric::checkedFloatCast(value.y, y) ||
        !numeric::checkedFloatCast(value.z, z)) return false;
    output = btVector3(x, y, z);
    return true;
}

// Host reads and force preparation precede every native write. On failure the
// previous rigid forces and the caller's soft-force batch remain untouched.
inline MStatus applyWorldFields(btDynamicsWorld* world, float dt,
                                SoftBodyForces& sampledSoftForces)
{
    if (!world || !std::isfinite(dt) || dt <= 0) return MS::kInvalidParameter;
    std::vector<btRigidBody*> rigidBodies;
    std::vector<btSoftBody*> softBodies;
    MVectorArray positions, velocities, softPositions, softVelocities;
    MDoubleArray masses, softMasses;
    const btCollisionObjectArray& objects = world->getCollisionObjectArray();
    for (int i = objects.size() - 1; i >= 0; --i) {
        if (btRigidBody* body = btRigidBody::upcast(objects[i])) {
            const btScalar inverseMass = body->getInvMass();
            if (!std::isfinite(inverseMass) || inverseMass < 0) return MS::kInvalidParameter;
            if (inverseMass == 0) continue;
            const btVector3& p = body->getWorldTransform().getOrigin();
            const btVector3& v = body->getLinearVelocity();
            DOMMY_RETURN_IF_FAILURE(appendFieldSample(
                MVector(p.x(), p.y(), p.z()), MVector(v.x(), v.y(), v.z()),
                1.0 / inverseMass, positions, velocities, masses));
            rigidBodies.push_back(body);
        } else if (btSoftBody* body = btSoftBody::upcast(objects[i])) {
            const double mass = body->getTotalMass();
            if (!std::isfinite(mass) || mass < 0) return MS::kInvalidParameter;
            if (mass == 0) continue;
            if (body->m_nodes.size() == 0) return MS::kInvalidParameter;
            MVector p(0, 0, 0), v(0, 0, 0);
            for (int node = 0; node < body->m_nodes.size(); ++node)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    if (!field::checkedAccumulateFieldForce(p[axis], body->m_nodes[node].m_x[axis], p[axis]) ||
                        !field::checkedAccumulateFieldForce(v[axis], body->m_nodes[node].m_v[axis], v[axis]))
                        return MS::kInvalidParameter;
                }
            DOMMY_RETURN_IF_FAILURE(appendFieldSample(
                p / body->m_nodes.size(), v / body->m_nodes.size(), mass,
                softPositions, softVelocities, softMasses));
            softBodies.push_back(body);
        }
    }

    std::vector<MVector> rigidTotals(rigidBodies.size(), MVector(0, 0, 0));
    std::vector<MVector> softTotals(softBodies.size(), MVector(0, 0, 0));
    MStatus status;
    MItDag fields(MItDag::kDepthFirst, MFn::kField, &status);
    if (!status) return status;
    for (;;) {
        const bool done = fields.isDone(&status);
        if (!status) return status;
        if (done) break;
        const MObject object = fields.currentItem(&status);
        if (!status) return status;
        MFnField fieldNode(object, &status);
        if (!status) return status;
        MVectorArray forces;
        if (!rigidBodies.empty()) {
            DOMMY_RETURN_IF_FAILURE(fieldNode.getForceAtPoint(positions, velocities, masses, forces, dt));
            DOMMY_RETURN_IF_FAILURE(accumulateFieldSample(forces, rigidTotals));
        }
        DOMMY_RETURN_IF_FAILURE(forces.setLength(0));
        if (!softBodies.empty()) {
            DOMMY_RETURN_IF_FAILURE(fieldNode.getForceAtPoint(softPositions, softVelocities, softMasses, forces, dt));
            DOMMY_RETURN_IF_FAILURE(accumulateFieldSample(forces, softTotals));
        }
        DOMMY_RETURN_IF_FAILURE(fields.next());
    }
    std::vector<btVector3> rigidForces(rigidBodies.size());
    SoftBodyForces softForces;
    softForces.reserve(softBodies.size());
    for (std::size_t i = 0; i < rigidBodies.size(); ++i) {
        if (!fieldVector(rigidTotals[i], rigidForces[i])) return MS::kInvalidParameter;
        for (int axis = 0; axis < 3; ++axis) {
            float product;
            if (!numeric::checkedFloatCast(double(rigidForces[i][axis]) *
                    rigidBodies[i]->getLinearFactor()[axis], product)) return MS::kInvalidParameter;
        }
    }
    for (std::size_t i = 0; i < softBodies.size(); ++i) {
        btVector3 force;
        if (!fieldVector(softTotals[i], force)) return MS::kInvalidParameter;
        softForces.push_back({softBodies[i], force});
    }
    for (std::size_t i = 0; i < rigidBodies.size(); ++i) {
        rigidBodies[i]->clearForces();
        rigidBodies[i]->applyCentralForce(rigidForces[i]);
    }
    sampledSoftForces.swap(softForces);
    return MS::kSuccess;
}

} // namespace bullet
} // namespace dommy
