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
//  DB_RigidBody_t.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//



#pragma once

#include <stdexcept>
#include <functional>
#include <vector>

#include "shared_ptr.h"
#include "rigid_body_impl.h"
#include "DB_CollisionShape_t.h"
#include "DB_Constraint_t.h"
#include "constraint_impl.h"

#include "LinearMath/btAlignedObjectArray.h"

//bt_rigid_body.h  bt_rigid_body_t
class DB_RigidBody_t: public rigid_body_impl_t {
public:
    virtual void get_transform(mat4x4f &xform) const override
    {
        float m[16];
        
        btTransform btxformLocal = m_body->getWorldTransform();
        btTransform shiftTransform;
        m_collisionShapeObserver->getCenterOfMassTransformShift(shiftTransform);
        btTransform btxform = btxformLocal * shiftTransform.inverse();
        
        btxform .getOpenGLMatrix(m);
        xform = mvl::trans(mvl::cmat<float, 4, 4>(m));
        
        
    }
    
    virtual void get_transform(vec3f &position, quatf &rotation) const override
    {
        btTransform btxformLocal = m_body->getWorldTransform();
        btTransform shiftTransform;
        m_collisionShapeObserver->getCenterOfMassTransformShift(shiftTransform);
        btTransform btxform = btxformLocal * shiftTransform.inverse();
        
        btQuaternion q = btxform.getRotation();
        btVector3 p = btxform.getOrigin();
        
        position = vec3f(p.x(), p.y(), p.z());
        rotation = quatf(q.w(), q.x(), q.y(), q.z());
    }
    
    virtual float get_mass() const override
    {
        float invMass = m_body->getInvMass();
        return invMass? 1.f/invMass : 0.f;
    }
    
    virtual void set_transform(vec3f const &position, quatf const &rotation) override
    {
        vec3f tp = position;
        quatf tr = rotation;
        btTransform xformLocal(btQuaternion(tr[1], tr[2], tr[3], tr[0]),
                               btVector3(tp[0], tp[1], tp[2]));
        
        btTransform shiftTransform;
        shiftTransform.setIdentity();
        m_collisionShapeObserver->getCenterOfMassTransformShift(shiftTransform);
        
        btTransform xform = xformLocal * shiftTransform;
        m_body->setWorldTransform(xform);
        // The world inverse inertia depends on this center-of-mass orientation.
        m_body->updateInertiaTensor();
        // Keep editing from introducing a spurious interpolation impulse.
        // Do not use setCenterOfMassTransform: it also changes interpolation velocities.
        m_body->setInterpolationWorldTransform(xform);
    }
    
    virtual void set_interpolation_transform(vec3f const &position, quatf const &rotation) override
    {
        vec3f tp = position;
        quatf tr = rotation;
        btTransform xformLocal(btQuaternion(tr[1], tr[2], tr[3], tr[0]),
                               btVector3(tp[0], tp[1], tp[2]));
        
        
        btTransform shiftTransform;
        shiftTransform.setIdentity();
        m_collisionShapeObserver->getCenterOfMassTransformShift(shiftTransform);
        
        btTransform xform = xformLocal * shiftTransform ;
        
        m_body->setInterpolationWorldTransform(xform);
    }
    
    virtual void set_kinematic(bool kinematic) override
    {
        if (!set_mass_properties(m_mass, vec3f(m_inertia.x(),m_inertia.y(),m_inertia.z()), kinematic))
            throw std::invalid_argument("Unsafe Bullet mass properties");
    }
    
    virtual void set_mass(float mass) override
    {
        if (!set_mass_properties(mass, vec3f(m_inertia.x(),m_inertia.y(),m_inertia.z()),
                m_body->isKinematicObject())) throw std::invalid_argument("Unsafe Bullet mass properties");
    }
    
    virtual void set_inertia(vec3f const& I) override
    {
        if (!set_mass_properties(m_mass, I, m_body->isKinematicObject()))
            throw std::invalid_argument("Unsafe Bullet mass properties");
    }

    bool set_mass_properties(float mass, const vec3f& inertia, bool kinematic) override
    {
        const btVector3& gravity = m_body->getGravity();
        if (!dommy::bullet::safeBodyMassProperties(mass, inertia) ||
            (!kinematic && !dommy::bullet::safeMassGravity(mass,
                vec3f(gravity.x(),gravity.y(),gravity.z())))) return false;
        m_mass = mass;
        m_inertia = btVector3(inertia[0],inertia[1],inertia[2]);
        const int flags = m_body->getCollisionFlags();
        m_body->setCollisionFlags(kinematic ? flags | btCollisionObject::CF_KINEMATIC_OBJECT
                                            : flags & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        m_body->setMassProps(kinematic ? 0 : mass, kinematic ? btVector3(0,0,0) : m_inertia);
        m_body->setActivationState(kinematic ? DISABLE_DEACTIVATION : ACTIVE_TAG);
        m_body->updateInertiaTensor();
        return true;
    }

    void get_mass_properties(float& mass, vec3f& inertia, bool& kinematic) const override
    {
        mass = m_mass;
        inertia = vec3f(m_inertia.x(),m_inertia.y(),m_inertia.z());
        kinematic = m_body->isKinematicObject();
    }
    
    virtual void set_restitution(float r) override
    {
        m_body->setRestitution(r);
    }
    
    virtual void set_friction(float f) override
    {
        m_body->setFriction(f);
    }
    
    virtual void set_linear_damping(float d) override
    {
        m_linear_damping = d;
        m_body->setDamping(m_linear_damping, m_angular_damping);
    }
    
    virtual void set_angular_damping(float d) override
    {
        m_angular_damping = d;
        m_body->setDamping(m_linear_damping, m_angular_damping);
    }
    
    virtual void set_linear_velocity(vec3f const& v) override
    {
        m_body->setLinearVelocity(btVector3(v[0], v[1], v[2]));
    }
    
    virtual void get_linear_velocity(vec3f& v) const override
    {
        const btVector3 &val = m_body->getLinearVelocity();
        v = vec3f(val.x(), val.y(), val.z());
    }
    
    virtual void set_angular_velocity(vec3f const& v) override
    {
        m_body->setAngularVelocity(btVector3(v[0], v[1], v[2]));
    }
    
    virtual void get_angular_velocity(vec3f& v) const override
    {
        const btVector3 &val = m_body->getAngularVelocity();
        v = vec3f(val.x(), val.y(), val.z());
    }
    
    virtual void clear_forces() override
    {
        m_body->clearForces();
    }
    
    virtual void apply_central_force(vec3f const& f) override
    {
        m_body->applyCentralForce(btVector3(f[0], f[1], f[2]));
    }
    
    virtual void apply_torque(vec3f const& t) override
    {
        m_body->applyTorque(btVector3(t[0], t[1], t[2]));
    }
    
    virtual void update_constraint() override
    {
        int count = m_constraintRef.size();
        for(int i=0; i<count; i++)
        {
            m_constraintRef[i]->update_constraint(this);
        }
    }

    // Capture before editing under the solver lock. Keep the owning body and
    // registered constraints alive until these allocation-free restorers expire.
    std::vector<std::function<void()> > capture_constraint_frames() const
    {
        std::vector<std::function<void()> > restorers;
        restorers.reserve(static_cast<std::size_t>(m_constraintRef.size()));
        for (int i = 0; i < m_constraintRef.size(); ++i) {
            DB_Constraint_t* const owner = m_constraintRef[i];
            constraint_impl_t* const state =
                dynamic_cast<constraint_impl_t*>(owner);
            if (owner == nullptr || state == nullptr || owner->constraint() == nullptr) {
                throw std::invalid_argument("Unavailable rigid-body constraint.");
            }
            const bool changed = state->getPivotChanged();
            btTypedConstraint* const native = owner->constraint();
            if (btPoint2PointConstraint* const point =
                    dynamic_cast<btPoint2PointConstraint*>(native)) {
                const btVector3 a = point->getPivotInA(), b = point->getPivotInB();
                restorers.emplace_back([point, state, changed, a, b]() {
                    point->setPivotA(a); point->setPivotB(b);
                    state->setPivotChanged(changed);
                });
            } else if (btHingeConstraint* const hinge =
                           dynamic_cast<btHingeConstraint*>(native)) {
                const btTransform a = hinge->getAFrame(), b = hinge->getBFrame();
                restorers.emplace_back([hinge, state, changed, a, b]() {
                    hinge->getAFrame() = a; hinge->getBFrame() = b;
                    state->setPivotChanged(changed);
                });
            } else if (btSliderConstraint* const slider =
                           dynamic_cast<btSliderConstraint*>(native)) {
                const btTransform a = slider->getFrameOffsetA(), b = slider->getFrameOffsetB();
                restorers.emplace_back([slider, state, changed, a, b]() {
                    slider->getFrameOffsetA() = a; slider->getFrameOffsetB() = b;
                    state->setPivotChanged(changed);
                });
            } else if (btGeneric6DofConstraint* const sixdof =
                           dynamic_cast<btGeneric6DofConstraint*>(native)) {
                const btTransform a = sixdof->getFrameOffsetA(), b = sixdof->getFrameOffsetB();
                restorers.emplace_back([sixdof, state, changed, a, b]() {
                    sixdof->getFrameOffsetA() = a; sixdof->getFrameOffsetB() = b;
                    state->setPivotChanged(changed);
                });
            } else {
                throw std::invalid_argument("Unsupported rigid-body constraint frame.");
            }
        }
        return restorers;
    }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName) override;
    
    btRigidBody* body() { return m_body.get(); }
    
    virtual void add_constraint(
        DB_Constraint_t* constraint, bool /*disableCollide*/ = false) override
    {
        m_constraintRef.push_back(constraint);
    }
    virtual void remove_constraint(DB_Constraint_t* constraint) override
    {
        m_constraintRef.remove(constraint);
    }
    
protected:
    friend class DB_BulletSolver_t;
    btAlignedObjectArray<DB_Constraint_t*> m_constraintRef;
    
    DB_RigidBody_t(collision_shape_impl_t* cs):
    m_collisionShapeObserver(cs),
    m_mass(1),
    m_inertia(2.0f/5.0f, 2.0f/5.0f, 2.0f/5.0f),
    m_linear_damping(0),
    m_angular_damping(0)
    {
        DB_CollisionShape_t* bt_shape = dynamic_cast<DB_CollisionShape_t*>(cs);
        if (!bt_shape || !bt_shape->shape()) {
            throw std::invalid_argument(
                "DB_RigidBody_t requires a valid Bullet collision shape.");
        }
        btRigidBody::btRigidBodyConstructionInfo rbInfo(
            m_mass, nullptr, bt_shape->shape(), m_inertia);
        rbInfo.m_restitution = 0;
        rbInfo.m_friction = 0.1f;
        rbInfo.m_linearDamping = m_linear_damping;
        rbInfo.m_angularDamping = m_angular_damping;
        m_body.reset(new btRigidBody(rbInfo));
    }
    
    virtual const btRigidBody* getBulletRigidBodyPointer() override
    {
        return m_body.get();
    }
    
    
private:
    shared_ptr<btRigidBody> m_body;
    // The owning dbRigidBody_t retains the corresponding collision-shape
    // wrapper for at least as long as this implementation.
    collision_shape_impl_t* m_collisionShapeObserver;
    float m_mass;
    btVector3 m_inertia;
    float m_linear_damping;
    float m_angular_damping;
};
