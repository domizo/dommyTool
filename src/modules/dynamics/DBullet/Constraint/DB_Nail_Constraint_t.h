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
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_nail_constraint.h

//
//  DB_Nail_Constraint_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#pragma once

#include "DB_RigidBody_t.h"
#include "DB_Constraint_t.h"
#include "DConstraintFrame.h"
#include "nail_constraint_impl.h"


//bt_nail_constraint
class DB_Nail_Constraint_t: public DB_Constraint_t, public nail_constraint_impl_t {
public:
    
    virtual void set_damping(float d) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        p2pc->m_setting.m_damping = d;
    }
    
    virtual float damping() const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        return p2pc->m_setting.m_damping;
    }
    
    virtual void set_breakThreshold(float d) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        p2pc->setBreakingImpulseThreshold(d);
    }
    
    virtual void set_enabled(bool e) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        p2pc->setEnabled(e);
    }
    
    virtual void set_pivotA(vec3f const &p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 bt_pivot(p[0], p[1], p[2]);
        p2pc->setPivotA(bt_pivot);
        setPivotChanged(true);
    }
    
    virtual void get_pivotA(vec3f &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        p[0] = p2pc->getPivotInA().x();
        p[1] = p2pc->getPivotInA().y();
        p[2] = p2pc->getPivotInA().z();
    }
    
    virtual void set_pivotB(vec3f const &p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 bt_pivot(p[0], p[1], p[2]);
        p2pc->setPivotB(bt_pivot);
        setPivotChanged(true);
    }
    
    virtual void get_pivotB(vec3f &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        p[0] = p2pc->getPivotInB().x();
        p[1] = p2pc->getPivotInB().y();
        p[2] = p2pc->getPivotInB().z();
    }
    
    virtual void get_world_pivotA(vec3f &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotAinW = p2pc->getRigidBodyA().getCenterOfMassTransform()* p2pc->getPivotInA();
        p[0] = pivotAinW.x();
        p[1] = pivotAinW.y();
        p[2] = pivotAinW.z();
    }
    virtual void get_world_pivotB(vec3f &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotBinW = p2pc->getRigidBodyB().getCenterOfMassTransform()* p2pc->getPivotInB();
        p[0] = pivotBinW.x();
        p[1] = pivotBinW.y();
        p[2] = pivotBinW.z();
    }
    
    virtual bool set_world(vec3f const& p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 pivotA, pivotB;
        if (!dommy::bullet::prepareConstraintPivots(
                p, p2pc->getRigidBodyA().getWorldTransform(),
                p2pc->getRigidBodyB().getWorldTransform(), pivotA, pivotB)) return false;
        p2pc->setPivotA(pivotA);
        p2pc->setPivotB(pivotB);
        setPivotChanged(true);
        return true;
    }
    
    virtual void get_world(vec3f &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotAinW = p2pc->getRigidBodyA().getCenterOfMassTransform()* p2pc->getPivotInA();
        p[0] = pivotAinW.x();
        p[1] = pivotAinW.y();
        p[2] = pivotAinW.z();
    }
    
    virtual void update_constraint(rigid_body_impl_t* rb)
    {
        btRigidBody* bt_body = static_cast<DB_RigidBody_t*>(rb)->body();
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 world, pivot;
        if(bt_body == &p2pc->getRigidBodyA())
        {
            world = p2pc->getRigidBodyB().getWorldTransform() * p2pc->getPivotInB();
            pivot = p2pc->getRigidBodyA().getWorldTransform().inverse() * world;
            p2pc->setPivotA(pivot);
        }
        else if(bt_body == &p2pc->getRigidBodyB())
        {
            world = p2pc->getRigidBodyA().getWorldTransform() * p2pc->getPivotInA();
            pivot = p2pc->getRigidBodyB().getWorldTransform().inverse() * world;
            p2pc->setPivotB(pivot);
        }
        else
        {
            world.setValue(0.f, 0.f, 0.f);
        }
        setPivotChanged(true);
    }
protected:
    friend class DB_BulletSolver_t;
    
    DB_Nail_Constraint_t(rigid_body_impl_t* rb, vec3f const& pivot):
    nail_constraint_impl_t()
    {
        btVector3 bulPivot(pivot[0], pivot[1], pivot[2]);
        btRigidBody& bt_body = *static_cast<DB_RigidBody_t*>(rb)->body();
        m_constraint.reset(new btPoint2PointConstraint(bt_body, bulPivot));
    }
    DB_Nail_Constraint_t(rigid_body_impl_t* rbA, rigid_body_impl_t* rbB, vec3f const& pivotInA, vec3f const& pivotInB):
    nail_constraint_impl_t()
    {
        btRigidBody& bt_bodyA = *static_cast<DB_RigidBody_t*>(rbA)->body();
        btRigidBody& bt_bodyB = *static_cast<DB_RigidBody_t*>(rbB)->body();
        btVector3 bulPivotInA(pivotInA[0], pivotInA[1], pivotInA[2]);
        btVector3 bulPivotInB(pivotInB[0], pivotInB[1], pivotInB[2]);
        m_constraint.reset(new btPoint2PointConstraint(bt_bodyA, bt_bodyB, bulPivotInA, bulPivotInB));
    }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName) const
    {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        solver->register_name(p2pc,objectName);
    }
    
private:
    
};

