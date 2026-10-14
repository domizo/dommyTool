// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Herbert Law
 
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
 
Written by: Herbert Law <Herbert.Law@gmail.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_slider_constraint.h

//
//  DB_Slider_Constraint_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//



#pragma once

#include <cmath>

#include "DB_RigidBody_t.h"
#include "DB_Constraint_t.h"
#include "DConstraintFrame.h"
#include "slider_constraint_impl.h"


//DB_Slider_Constraint
class DB_Slider_Constraint_t: public DB_Constraint_t, public slider_constraint_impl_t {
public:
    
    virtual void set_damping(float d) {
        if (!std::isfinite(d)) return;
        const btScalar dampingValue = btClamped(btScalar(d), btScalar(0), btScalar(1));
        btSliderConstraint* slider = static_cast<btSliderConstraint*>(m_constraint.get());
        slider->setDampingDirLin(dampingValue);
        slider->setDampingDirAng(dampingValue);
        slider->setDampingLimLin(dampingValue);
        slider->setDampingLimAng(dampingValue);
        slider->setDampingOrthoLin(dampingValue);
        slider->setDampingOrthoAng(dampingValue);
    }
    
    virtual float damping() const {
        // Bullet 2.88's getter is not const even though it only returns a
        // scalar value.
        btSliderConstraint* slider =
            static_cast<btSliderConstraint*>(m_constraint.get());
        return static_cast<float>(slider->getDampingDirLin());
    }
    
    virtual void set_breakThreshold(float d) {
        btSliderConstraint* p2pc = static_cast<btSliderConstraint*>(m_constraint.get());
        p2pc->setBreakingImpulseThreshold(d);
    }
    
    virtual void set_enabled(bool e) {
        btSliderConstraint* p2pc = static_cast<btSliderConstraint*>(m_constraint.get());
        p2pc->setEnabled(e);
    }
    
    virtual void set_LinLimit(float lower, float upper) {
        btSliderConstraint* slider = static_cast<btSliderConstraint*>(m_constraint.get());
        slider->setLowerLinLimit(lower);
        slider->setUpperLinLimit(upper);
    }
    
    virtual void set_AngLimit(float lower, float upper) {
        btSliderConstraint* slider = static_cast<btSliderConstraint*>(m_constraint.get());
        slider->setLowerAngLimit(lower);
        slider->setUpperAngLimit(upper);
    }
    
    virtual void get_frameA(vec3f& p, quatf& r) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform& btxform = sc->getFrameOffsetA();
        btQuaternion bq = btxform.getRotation();
        btVector3 bp = btxform.getOrigin();
        p = vec3f(bp.x(), bp.y(), bp.z());
        r = quatf(bq.w(), bq.x(), bq.y(), bq.z());
    }
    virtual void get_frameB(vec3f& p, quatf& r) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform& btxform = sc->getFrameOffsetB();
        btQuaternion bq = btxform.getRotation();
        btVector3 bp = btxform.getOrigin();
        p = vec3f(bp.x(), bp.y(), bp.z());
        r = quatf(bq.w(), bq.x(), bq.y(), bq.z());
    }
    virtual void get_invFrameA(vec3f& p, quatf& r) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform btxform = sc->getFrameOffsetA().inverse();
        btQuaternion bq = btxform.getRotation();
        btVector3 bp = btxform.getOrigin();
        p = vec3f(bp.x(), bp.y(), bp.z());
        r = quatf(bq.w(), bq.x(), bq.y(), bq.z());
    }
    virtual void get_invFrameB(vec3f& p, quatf& r) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform btxform = sc->getFrameOffsetB().inverse();
        btQuaternion bq = btxform.getRotation();
        btVector3 bp = btxform.getOrigin();
        p = vec3f(bp.x(), bp.y(), bp.z());
        r = quatf(bq.w(), bq.x(), bq.y(), bq.z());
    }
    virtual void worldToA(vec3f& w, vec3f& p) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform w2a = (sc->getRigidBodyA().getWorldTransform() * sc->getFrameOffsetA()).inverse();
        btVector3 bw(w[0], w[1], w[2]);
        btVector3 bp = w2a * bw;
        p = vec3f(bp[0], bp[1], bp[2]);
    }
    virtual void worldFromB(vec3f& p, vec3f& w) const
    {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        const btTransform b2w = sc->getRigidBodyB().getWorldTransform() * sc->getFrameOffsetB();
        btVector3 bp(p[0], p[1], p[2]);
        btVector3 bw = b2w * bp;
        w = vec3f(bw[0], bw[1], bw[2]);
    }
    
    virtual void get_local_frameA(vec3f& p, quatf& r)const
    {
        const btSliderConstraint* sc = static_cast<const btSliderConstraint*>(m_constraint.get());
        p[0] = sc->getFrameOffsetA().getOrigin().x();
        p[1] = sc->getFrameOffsetA().getOrigin().y();
        p[2] = sc->getFrameOffsetA().getOrigin().z();
        
        btQuaternion orn = sc->getFrameOffsetA().getRotation();
        r[0] = orn.getW();
        r[1] = orn.getX();
        r[2] = orn.getY();
        r[3] = orn.getZ();
    }
    
    virtual void get_local_frameB(vec3f& p, quatf& r) const
    {
        const btSliderConstraint* sc = static_cast<const btSliderConstraint*>(m_constraint.get());
        p[0] = sc->getFrameOffsetB().getOrigin().x();
        p[1] = sc->getFrameOffsetB().getOrigin().y();
        p[2] = sc->getFrameOffsetB().getOrigin().z();
        
        btQuaternion orn = sc->getFrameOffsetB().getRotation();
        r[0] = orn.getW();
        r[1] = orn.getX();
        r[2] = orn.getY();
        r[3] = orn.getZ();
    }
    
    
    virtual bool set_world(vec3f const& p, quatf const& r) {
        btSliderConstraint* sc = static_cast<btSliderConstraint*>(m_constraint.get());
        btTransform frameA, frameB;
        if (!dommy::bullet::prepareConstraintFrames(
                p, r, sc->getRigidBodyA().getWorldTransform(),
                sc->getRigidBodyB().getWorldTransform(), frameA, frameB)) return false;
        sc->getFrameOffsetA() = frameA;
        sc->getFrameOffsetB() = frameB;
        return true;
    }
    
    virtual void get_world(vec3f &p, quatf& r) const {
        btSliderConstraint const* sc = static_cast<btSliderConstraint const*>(m_constraint.get());
        btTransform frameAinW = sc->getRigidBodyA().getWorldTransform() * sc->getFrameOffsetA();
        btQuaternion bq = frameAinW.getRotation();
        btVector3 bp = frameAinW.getOrigin();
        p = vec3f(bp.x(), bp.y(), bp.z());
        r = quatf(bq.w(), bq.x(), bq.y(), bq.z());
    }
    
    virtual void update_constraint(rigid_body_impl_t* rb)
    {
        btSliderConstraint* sc = static_cast<btSliderConstraint*>(m_constraint.get());
        btRigidBody* bt_body = static_cast<DB_RigidBody_t*>(rb)->body();
        btTransform frameW, frameL;
        if(bt_body == &sc->getRigidBodyA())
        {
            frameW = sc->getRigidBodyB().getWorldTransform() * sc->getFrameOffsetB();
            frameL = sc->getRigidBodyA().getWorldTransform().inverse() * frameW;
            sc->getFrameOffsetA() = frameL;
        }
        else if(bt_body == &sc->getRigidBodyB())
        {
            frameW = sc->getRigidBodyA().getWorldTransform() * sc->getFrameOffsetA();
            frameL = sc->getRigidBodyB().getWorldTransform().inverse() * frameW;
            sc->getFrameOffsetB() = frameL;
        }
        setPivotChanged(true);
    }
    
    
protected:
    friend class DB_BulletSolver_t;
    
    DB_Slider_Constraint_t(rigid_body_impl_t* rb, vec3f const& pivot, quatf const& rot):
    slider_constraint_impl_t()
    {
        btRigidBody& bt_body = *static_cast<DB_RigidBody_t*>(rb)->body();
        btVector3 p(pivot[0], pivot[1], pivot[2]);
        btQuaternion q(rot[1], rot[2], rot[3], rot[0]);
        btTransform frameInA(q, p);
        btSliderConstraint* slider = new btSliderConstraint(bt_body, frameInA, false);
        m_constraint.reset(slider);
    }
    DB_Slider_Constraint_t(rigid_body_impl_t* rbA, vec3f const& pivotA, quatf const& rotA, rigid_body_impl_t* rbB, vec3f const& pivotB, quatf const& rotB):
    slider_constraint_impl_t()
    {
        btRigidBody& bt_bodyA = *static_cast<DB_RigidBody_t*>(rbA)->body();
        btRigidBody& bt_bodyB = *static_cast<DB_RigidBody_t*>(rbB)->body();
        btVector3 pA(pivotA[0], pivotA[1], pivotA[2]);
        btQuaternion qA(rotA[1], rotA[2], rotA[3], rotA[0]);
        btTransform frameInA(qA, pA);
        btVector3 pB(pivotB[0], pivotB[1], pivotB[2]);
        btQuaternion qB(rotB[1], rotB[2], rotB[3], rotB[0]);
        btTransform frameInB(qB, pB);
        btSliderConstraint* slider = new btSliderConstraint(bt_bodyA, bt_bodyB, frameInA, frameInB, false);
        m_constraint.reset(slider);
    }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName) const
    {
        const btSliderConstraint* sc = static_cast<const btSliderConstraint*>(m_constraint.get());
        solver->register_name(sc,objectName);
    }
    
    
private:
    
};
