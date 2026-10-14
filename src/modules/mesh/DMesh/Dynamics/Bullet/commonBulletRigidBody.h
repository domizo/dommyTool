// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Walt Disney Studios
 
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
 
1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 
Written by: Nicola Candussi <nicola@fluidinteractive.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_rigid_body.h

//
//  commonBulletRigidBody.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/03.
//


#pragma once

#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "mathUtils.h"
#include "Collision/DShapeFrameMath.h"
#include "Constraint/DConstraintFrame.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MUuid.h>
#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>


#include "db_constraint_base.h"
#include "DBulletUserData.h"

enum class DBulletBodyMode { Dynamic, Kinematic };

struct btRigidParams
{
    btRigidParams()
    : m_mass(1.0)
    , m_restitution(0.1)
    , m_friction(0.5)
    , m_rollingFriction(0.0)
    , m_linearDamping(0.3)
    , m_angularDamping(0.3)
    , m_initPos(MVector(0.0,0.0,0.0))
    , m_initRot(MVector(0.0,0.0,0.0))
    , m_initVel(MVector(0.0,0.0,0.0))
    , m_initSpin(MVector(0.0,0.0,0.0))
    , m_collisionMargin(0.04)
    , m_collisionScale(MVector(1.0,1.0,1.0))
    , m_start_sleeping(false)
    , m_constParam(DbtConstraintParams())
    { }
    
    float m_mass;
    
    double m_restitution;
    double m_friction;
    double m_rollingFriction;
    double m_linearDamping;
    double m_angularDamping;
    
    MVector m_initPos;
    MVector m_initRot;
    MVector m_initVel;
    MVector m_initSpin;
    
    float m_collisionMargin;
    MVector m_collisionScale;
    
    bool m_start_sleeping;
    
    DbtConstraintParams m_constParam;
};

inline bool GetBulletCollisionScale(const MVector& requested,
                                   const MVector& source, btVector3& output)
{
    btVector3 candidate(0, 0, 0);
    for (int axis = 0; axis < 3; ++axis) {
        const double value = requested[axis] * source[axis];
        if (!dommy::bullet::isSafeCollisionCoordinate(value)) return false;
        candidate[axis] = static_cast<btScalar>(value);
        // Native box/compound rescaling divides by the previous scale.
        // Reject singular or underflowed values before changing an owned shape.
        if (candidate[axis] == 0 ||
            !dommy::bullet::isSafeInverseOperand(std::abs(candidate[axis]))) return false;
    }
    output = candidate;
    return true;
}

inline bool ValidateBulletRigidParameters(const btRigidParams& params)
{
    dommy::bullet::RigidBodyParameters checked;
    if (!dommy::bullet::validateRigidBodyParameters(params.m_mass,
            params.m_restitution, params.m_friction, params.m_linearDamping,
            params.m_angularDamping, checked) ||
        !dommy::bullet::isSafeCollisionCoordinate(params.m_collisionMargin) ||
        params.m_collisionMargin < 0 || !std::isfinite(params.m_rollingFriction) ||
        params.m_rollingFriction < 0 || params.m_rollingFriction > std::numeric_limits<float>::max()) return false;
    for (const MVector* vector : {&params.m_initPos, &params.m_initRot,
            &params.m_initVel, &params.m_initSpin, &params.m_collisionScale}) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::bullet::isSafeCollisionCoordinate((*vector)[axis])) return false;
        }
    }
    return true;
}


inline void getDBtTransform(btRigidBody *m_body, mat4x4f &xform)
{
    float m[16];
    
    btTransform btxformLocal = m_body->getWorldTransform();
    btxformLocal.getOpenGLMatrix(m);
    xform = mvl::trans(mvl::cmat<float, 4, 4>(m));
}

inline void getDBtTransformTR(btRigidBody *m_body, vec3f &position, quatf &rotation)
{
    btTransform btxformLocal = m_body->getWorldTransform();
    btQuaternion q = btxformLocal.getRotation();
    btVector3 p = btxformLocal.getOrigin();
    
    
    position = vec3f(p.x(), p.y(), p.z());
    rotation = quatf(q.w(), q.x(), q.y(), q.z());
}

inline void set_DB_RigidTransform(btRigidBody *m_body, vec3f const &position, quatf const &rotation)
{
    vec3f tp = position;
    quatf tr = rotation;
    btTransform xform(btQuaternion(tr[1], tr[2], tr[3], tr[0]), btVector3(tp[0], tp[1], tp[2]));
    m_body->getMotionState()->setWorldTransform(xform);
    
    // static bodies may got false "impulse" when editing in Maya, so...
}

inline void set_DB_RigidInterpolationTransform(btRigidBody *m_body, vec3f const &position, quatf const &rotation)
{
    vec3f tp = position;
    quatf tr = rotation;
    btTransform xform(btQuaternion(tr[1], tr[2], tr[3], tr[0]), btVector3(tp[0], tp[1], tp[2]));
    m_body->setInterpolationWorldTransform(xform);
}

inline void set_kinematic(btRigidBody *m_body, bool kinematic, float mass, btVector3 inertia)
{
    if(kinematic) {
        m_body->setCollisionFlags(m_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        m_body->setActivationState(DISABLE_DEACTIVATION);
        m_body->setMassProps(0, btVector3(0.0,0.0,0.0));
        m_body->updateInertiaTensor();
    } else {
        m_body->setCollisionFlags(m_body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        m_body->setActivationState(ACTIVE_TAG);
        m_body->setMassProps(mass, inertia);
        m_body->updateInertiaTensor();
    }
}


// Use the same motion policy for initial bodies and later additions.
inline void ConfigureBulletBodyMotion(btRigidBody& body,
    const btRigidParams& params, DBulletBodyMode mode,
    const btVector3& inertia, bool planar)
{
    const bool kinematic = mode == DBulletBodyMode::Kinematic;
    set_kinematic(&body, kinematic, params.m_mass, inertia);
    body.setLinearVelocity(kinematic ? btVector3(0,0,0) :
        btVector3(params.m_initVel[0], params.m_initVel[1], params.m_initVel[2]));
    body.setAngularVelocity(kinematic ? btVector3(0,0,0) :
        btVector3(params.m_initSpin[0], params.m_initSpin[1], params.m_initSpin[2]));
    if (planar) {
        body.setLinearFactor(btVector3(1,1,0));
        body.setAngularFactor(btVector3(0,0,1));
        if (!kinematic && params.m_start_sleeping)
            body.setActivationState(ISLAND_SLEEPING);
    }
}

inline float getDBtRigidBodyMass(btRigidBody *m_body)
{
    float invMass = m_body->getInvMass();
    return invMass? 1.f/invMass : 0.f;
}

inline void getDBtRigidLinearVelocity(btRigidBody *m_body, vec3f& v)
{
    const btVector3 &val = m_body->getLinearVelocity();
    v = vec3f(val.x(), val.y(), val.z());
}


inline void BMatToMMat(btTransform srcMat, MMatrix &distMat) {
    btQuaternion q = srcMat.getRotation();
    btVector3 p = srcMat.getOrigin();
    
    MTransformationMatrix transMat;
    transMat.setRotationQuaternion(q[0],  q[1], q[2], q[3]);
    transMat.setTranslation(MVector(p[0], p[1], p[2]), MSpace::kTransform);
    
    distMat = transMat.asMatrix();
}

// Bullet owns rigid translation/rotation, not the visible geometry's scale.
// Preserve the copied input's scale/shear; CollisionScale is physics-only.
inline MStatus UpdateBulletGeometryTransform(const btTransform& transform,
    const MMatrix& source, MMatrix& output)
{
    if (!dommy::bullet::finiteConstraintTransform(transform) ||
        !dommy::maya::isFiniteMatrix(source)) return MS::kInvalidParameter;
    const btQuaternion rotation = transform.getRotation();
    quatf normalized;
    if (!dommy::bullet::normalizeQuaternion(quatf(rotation.w(), rotation.x(),
            rotation.y(), rotation.z()), normalized)) return MS::kInvalidParameter;
    MTransformationMatrix candidate(source);
    DOMMY_RETURN_IF_FAILURE(candidate.setRotationQuaternion(
        normalized[1], normalized[2], normalized[3], normalized[0]));
    const btVector3 position = transform.getOrigin();
    DOMMY_RETURN_IF_FAILURE(candidate.setTranslation(
        MVector(position.x(), position.y(), position.z()), MSpace::kTransform));
    const MMatrix result = candidate.asMatrix();
    if (!dommy::maya::isFiniteMatrix(result)) return MS::kInvalidParameter;
    output = result;
    return MS::kSuccess;
}

// A compound collision shape can also represent ONE decomposed input mesh.
// Root metadata takes precedence; only grouped bodies use child metadata.
template <class Visitor>
MStatus VisitBulletRigidGeometry(btDynamicsWorld& world, Visitor visit)
{
    for (int i = world.getNumCollisionObjects() - 1; i >= 0; --i) {
        btRigidBody* body = btRigidBody::upcast(world.getCollisionObjectArray()[i]);
        if (!body) continue;
        const auto* metadata = static_cast<const customDBData*>(body->getUserPointer());
        if (metadata) {
            btTransform transform = body->getWorldTransform();
            if (body->getMotionState()) body->getMotionState()->getWorldTransform(transform);
            DOMMY_RETURN_IF_FAILURE(visit(*metadata, transform));
        } else if (body->getCollisionShape() && body->getCollisionShape()->isCompound()) {
            auto* compound = static_cast<btCompoundShape*>(body->getCollisionShape());
            for (int child = 0; child < compound->getNumChildShapes(); ++child) {
                const auto* childData = static_cast<const customDBData*>(
                    compound->getChildShape(child)->getUserPointer());
                if (childData) DOMMY_RETURN_IF_FAILURE(visit(*childData,
                    body->getWorldTransform() * compound->getChildTransform(child)));
            }
        }
    }
    return MS::kSuccess;
}

inline void MMatToBMat(MMatrix srcMat, btTransform &distMat) {
    
    MTransformationMatrix transMat(srcMat);
    MVector pos = transMat.getTranslation(MSpace::kTransform);
    double quartx,quarty,quartz,quartw;
    transMat.getRotationQuaternion(quartx, quarty, quartz, quartw);
    
    btTransform xformLocal(btQuaternion( quartx, quarty, quartz,quartw ), btVector3(pos[0], pos[1], pos[2]));
    distMat = xformLocal;
}
