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
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_nail_constraint.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_hinge_constraint.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_sixdof_constraint.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/bt_slider_constraint.h

//
//  commonBulletConstraint.h
//  dommyTool
//
//  Created by yuzo on 2017/10/15.
//


#pragma once



#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "shared_ptr.h"
#include "dommyMath.h"

#include "DBulletDataHeaders.h"
#include "common/utility/DNumericValidation.h"


struct DbtConstraintParams
{
    DbtConstraintParams()
    : m_sourceVec(btVector3(0,0,0))
    , m_targetVec(btVector3(0,0,0))
    , m_sourceMat(btTransform())
    , m_targetMat(btTransform())
    , m_rigid_src(0)
    , m_rigid_tar(0)
    , m_const_type(DBT_CONST_INVALID)
    , m_cfm(0.0)
    , m_erp(0.2)
    , m_tau(0.3)
    , m_const_angular_damping(1.0)
    , m_impulseClamp(0.)
    , m_limit_angle(MVector())
    , m_softness(0.9f)
    , m_biasFactor(0.3)
    , m_relaxationFactor(1.0)
    , m_linear_lower_limit(MVector())
    , m_linear_upper_limit(MVector())
    , m_angular_lower_limit(MVector())
    , m_angular_upper_limit(MVector())
    , m_use_spring(false)
    , m_use_uniform_stiffness(false)
    , m_use_uniform_damping(false)
    , m_translate_stiffness(MVector())
    , m_rotate_stiffness(MVector())
    , m_translate_damping(MVector())
    , m_rotate_damping(MVector())
    , m_axis(MVector(0,1,0))
    , m_hige_low(1.0)
    , m_hige_high(-1.0)
    , m_use_break_threshold(false)
    , m_break_threshold(0.0)
    , m_use_break_angle(false)
    , m_break_angle(0.01)
    , m_override_iterations(-1)
    , m_debug_display_size(1.0)
    , m_disable_colide(false)
    { }
    
    //target and src
    btVector3 m_sourceVec;
    btVector3 m_targetVec;
    
    btTransform m_sourceMat;
    btTransform m_targetMat;
    
    btRigidBody *m_rigid_src;
    btRigidBody *m_rigid_tar;
    
    
    //constraint
    DBT_CONST_TYPE m_const_type;
    
    
    //
    double m_cfm;
    double m_erp;
    
    //p2p
    double m_tau;
    double m_const_angular_damping;
    double m_impulseClamp;
    
    //cone angle
    MVector m_limit_angle;
    double m_softness;
    double m_biasFactor;
    double m_relaxationFactor;
    
    
    //6dof
    MVector m_linear_lower_limit;
    MVector m_linear_upper_limit;
    MVector m_angular_lower_limit;
    MVector m_angular_upper_limit;
    
    
    //spring
    bool m_use_spring;
    bool m_use_uniform_stiffness;
    bool m_use_uniform_damping;
    //0-5はtranslateとrotateの軸を表している
    MVector m_translate_stiffness;
    MVector m_rotate_stiffness;
    MVector m_translate_damping;
    MVector m_rotate_damping;
    
    
    
    //hinge
    MVector m_axis;
    double m_hige_low,m_hige_high;
    
    
    //breaking
    bool m_use_break_threshold;
    double m_break_threshold;
    
    bool m_use_break_angle;
    double m_break_angle;
    
    
    //solver
    int m_override_iterations;
    
    //display
    double m_debug_display_size;
    
    bool m_disable_colide;
};

// Validate the numeric settings shared by the legacy constraint nodes before
// narrowing Maya doubles into Bullet's btScalar. Negative limits retain their
// historical meaning (for example an unlocked 6-DoF axis).
inline bool ValidateBulletConstraintParameters(const DbtConstraintParams& params)
{
    float narrowed;
    for (double value : {params.m_cfm, params.m_erp, params.m_tau,
            params.m_const_angular_damping, params.m_impulseClamp,
            params.m_softness, params.m_biasFactor, params.m_relaxationFactor,
            params.m_hige_low, params.m_hige_high, params.m_break_threshold,
            params.m_break_angle, params.m_debug_display_size})
        if (!dommy::numeric::checkedFloatCast(value, narrowed)) return false;
    for (const MVector* vector : {&params.m_limit_angle,
            &params.m_linear_lower_limit, &params.m_linear_upper_limit,
            &params.m_angular_lower_limit, &params.m_angular_upper_limit,
            &params.m_translate_stiffness, &params.m_rotate_stiffness,
            &params.m_translate_damping, &params.m_rotate_damping, &params.m_axis})
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::numeric::checkedFloatCast((*vector)[axis], narrowed)) return false;
    return params.m_break_threshold >= 0 && params.m_break_angle >= 0 &&
           params.m_debug_display_size >= 0;
}

//bt_constraint_t
class db_constraint_base
{
public:
    db_constraint_base()
        : m_pivotChanged(false)
    {
    }

    virtual ~db_constraint_base() {}

    db_constraint_base(const db_constraint_base&) = delete;
    db_constraint_base& operator=(const db_constraint_base&) = delete;

    
    btTypedConstraint* constraint()                     { return m_constraint.get(); }
    void set_constraint(btTypedConstraint *constraint)  { m_constraint.reset(constraint); }
//    virtual void update_constraint(rigid_body_impl_t* rb) = 0;
    
    
    bool getPivotChanged() { return m_pivotChanged; }
    void setPivotChanged(bool isChanged) { m_pivotChanged = isChanged; }
    
    virtual void set_enabled(bool e) { (void)e; }
    virtual void set_debugDrawSIze(double s) { if(m_constraint.get()) m_constraint.get()->setDbgDrawSize(btScalar(s)); }
    
protected:
    shared_ptr<btTypedConstraint> m_constraint;
    bool    m_pivotChanged;
};



class db_fix_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btFixedConstraint* cc = static_cast<btFixedConstraint*>(m_constraint.get());
        
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
    }
    
    
    
    virtual void set_breakThreshold(float d) {
        btFixedConstraint* con = static_cast<btFixedConstraint*>(m_constraint.get());
        con->setBreakingImpulseThreshold(d);
    }
    virtual void set_OverrideNumSolverIterations(int num) {
        btFixedConstraint* con = static_cast<btFixedConstraint*>(m_constraint.get());
        con->setOverrideNumSolverIterations(num);
    }
    
    
    virtual void set_enabled(bool e) {
        btFixedConstraint* con = static_cast<btFixedConstraint*>(m_constraint.get());
        con->setEnabled(e);
    }
    
    
    db_fix_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transA, btTransform const& transB):
    db_constraint_base()
    {
        m_constraint.reset(new btFixedConstraint(rbA, rbB, transA, transB));
    }
    
    
    
};



class db_p2p_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        
        p2pc->m_setting.m_tau = params.m_tau;
        p2pc->m_setting.m_damping = params.m_const_angular_damping;
        p2pc->m_setting.m_impulseClamp = params.m_impulseClamp;
        
        p2pc->setParam(BT_CONSTRAINT_ERP, params.m_erp);
        p2pc->setParam(BT_CONSTRAINT_CFM, params.m_cfm);
        
        if(params.m_use_break_threshold) {
            p2pc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            p2pc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        p2pc->setDbgDrawSize(params.m_debug_display_size);
    }
    
    
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
    
    virtual void set_pivotA(btVector3 const &p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 bt_pivot(p[0], p[1], p[2]);
        p2pc->setPivotA(bt_pivot);
        setPivotChanged(true);
    }
    
    virtual void get_pivotA(btVector3 &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        p[0] = p2pc->getPivotInA().x();
        p[1] = p2pc->getPivotInA().y();
        p[2] = p2pc->getPivotInA().z();
    }
    
    virtual void set_pivotB(btVector3 const &p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 bt_pivot(p[0], p[1], p[2]);
        p2pc->setPivotB(bt_pivot);
        setPivotChanged(true);
    }
    
    virtual void get_pivotB(btVector3 &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        p[0] = p2pc->getPivotInB().x();
        p[1] = p2pc->getPivotInB().y();
        p[2] = p2pc->getPivotInB().z();
    }
    
    virtual void get_world_pivotA(btVector3 &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotAinW = p2pc->getRigidBodyA().getCenterOfMassTransform()* p2pc->getPivotInA();
        p[0] = pivotAinW.x();
        p[1] = pivotAinW.y();
        p[2] = pivotAinW.z();
    }
    virtual void get_world_pivotB(btVector3 &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotBinW = p2pc->getRigidBodyB().getCenterOfMassTransform()* p2pc->getPivotInB();
        p[0] = pivotBinW.x();
        p[1] = pivotBinW.y();
        p[2] = pivotBinW.z();
    }
    
    virtual void set_world(btVector3 const &p) {
        btPoint2PointConstraint* p2pc = static_cast<btPoint2PointConstraint*>(m_constraint.get());
        btVector3 world(p[0], p[1], p[2]);
        btVector3 pivotA = p2pc->getRigidBodyA().getWorldTransform().inverse() (world);
        p2pc->setPivotA(pivotA);
        btVector3 pivotB = p2pc->getRigidBodyB().getWorldTransform().inverse() (world);
        p2pc->setPivotB(pivotB);
        setPivotChanged(true);
    }
    
    virtual void get_world(btVector3 &p) const {
        btPoint2PointConstraint const* p2pc = static_cast<btPoint2PointConstraint const*>(m_constraint.get());
        btVector3 pivotAinW = p2pc->getRigidBodyA().getCenterOfMassTransform()* p2pc->getPivotInA();
        p[0] = pivotAinW.x();
        p[1] = pivotAinW.y();
        p[2] = pivotAinW.z();
    }
    
    virtual void update_constraint(btRigidBody& /*rigidBody*/)
    {
        setPivotChanged(true);
    }

    
    
    db_p2p_constraint(btRigidBody& rb, btVector3 const& pivot):
    db_constraint_base()
    {
        
        m_constraint.reset(new btPoint2PointConstraint(rb, pivot));
    }
    db_p2p_constraint(btRigidBody& rbA, btRigidBody& rbB, btVector3 const& pivotInA, btVector3 const& pivotInB):
    db_constraint_base()
    {
        m_constraint.reset(new btPoint2PointConstraint(rbA, rbB, pivotInA, pivotInB));
    }
    
    
private:
    
};



class db_cone_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btConeTwistConstraint* cc = static_cast<btConeTwistConstraint*>(m_constraint.get());
        
        cc->setLimit(dommy::math::degreesToRadians(params.m_limit_angle.z) ,//ローカルz軸周り
                     dommy::math::degreesToRadians(params.m_limit_angle.x),//ローカルy軸周り
                     dommy::math::degreesToRadians(params.m_limit_angle.y),//ローカルx軸周り(ひねり)
                     params.m_softness,
                     params.m_biasFactor,
                     params.m_relaxationFactor);
        
        
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
    }
    
    virtual void set_enabled(bool e) {
        btConeTwistConstraint* conec = static_cast<btConeTwistConstraint*>(m_constraint.get());
        conec->setEnabled(e);
    }
    
    virtual btScalar get_twistAngle() {
        btConeTwistConstraint* conec = static_cast<btConeTwistConstraint*>(m_constraint.get());
        return conec->getTwistAngle();
        
    }
    
    
    virtual void set_breakThreshold(float d) {
        btConeTwistConstraint* conec = static_cast<btConeTwistConstraint*>(m_constraint.get());
        conec->setBreakingImpulseThreshold(d);
    }
    
    virtual void set_OverrideNumSolverIterations(int num) {
        btConeTwistConstraint* con = static_cast<btConeTwistConstraint*>(m_constraint.get());
        con->setOverrideNumSolverIterations(num);
    }
    
    virtual void set_Limit(btVector3 const& angle,
                           double const&softness,
                           double const&biasFactor,
                           double const&relaxationFactor) {
        btConeTwistConstraint* conec = static_cast<btConeTwistConstraint*>(m_constraint.get());
        conec->setLimit(dommy::math::degreesToRadians(angle[2]) ,//ローカルz軸周り
                        dommy::math::degreesToRadians(angle[0]),//ローカルy軸周り
                        dommy::math::degreesToRadians(angle[1]),//ローカルx軸周り(ひねり)
                        softness,
                        biasFactor,
                        relaxationFactor);
    }
    
    
    db_cone_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transMatA, btTransform const& transMatB):
    db_constraint_base()
    {
        
        m_constraint.reset(new btConeTwistConstraint(rbA, rbB, transMatA, transMatB));
        //        rbA->add_constraint(this);
        //        rbB->add_constraint(this);
    }
    
private:
    
};


class db_sixdof_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btGeneric6DofConstraint* cc = static_cast<btGeneric6DofConstraint*>(m_constraint.get());
        
        //angle
        btVector3 linearLowerLimitVec(params.m_linear_lower_limit[0],params.m_linear_lower_limit[1],params.m_linear_lower_limit[2]);
        btVector3 linearUpperLimitVec(params.m_linear_upper_limit[0],params.m_linear_upper_limit[1],params.m_linear_upper_limit[2]);
        
        cc->setLinearLowerLimit(linearLowerLimitVec);
        cc->setLinearUpperLimit(linearUpperLimitVec);
        
        btVector3 angularLowerLimitVec(params.m_angular_lower_limit[0],params.m_angular_lower_limit[1],params.m_angular_lower_limit[2]);
        btVector3 angularUpperLimitVec(params.m_angular_upper_limit[0],params.m_angular_upper_limit[1],params.m_angular_upper_limit[2]);
        
        // Angular attributes use radians, matching Bullet's angular-limit API.
        cc->setAngularLowerLimit(angularLowerLimitVec);
        cc->setAngularUpperLimit(angularUpperLimitVec);
        
        //common
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
    }
    
    virtual void set_enabled(bool e) {
        btGeneric6DofConstraint* cc = static_cast<btGeneric6DofConstraint*>(m_constraint.get());
        cc->setEnabled(e);
    }
    
    
    db_sixdof_constraint(btRigidBody& rb, btTransform const& transA, bool useLinearReferenceFrameB):
    db_constraint_base()
    {
        btGeneric6DofConstraint* sixdof = new btGeneric6DofConstraint(rb, transA, useLinearReferenceFrameB);
        m_constraint.reset(sixdof);
    }
    db_sixdof_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transA, btTransform const& transB, bool useLinearReferenceFrameB):
    db_constraint_base()
    {
        btGeneric6DofConstraint* sixdof = new btGeneric6DofConstraint(rbA, rbB, transA, transB, useLinearReferenceFrameB);
        m_constraint.reset(sixdof);
        
    }
    
};



//DB_Sixdof_Constraint
class db_sixdof_spring_constraint: public db_constraint_base {
public:
    
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btGeneric6DofSpringConstraint* cc = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        
        //angle
        btVector3 linearLowerLimitVec(params.m_linear_lower_limit[0],params.m_linear_lower_limit[1],params.m_linear_lower_limit[2]);
        btVector3 linearUpperLimitVec(params.m_linear_upper_limit[0],params.m_linear_upper_limit[1],params.m_linear_upper_limit[2]);
        
        cc->setLinearLowerLimit(linearLowerLimitVec);
        cc->setLinearUpperLimit(linearUpperLimitVec);
        
        btVector3 angularLowerLimitVec(params.m_angular_lower_limit[0],params.m_angular_lower_limit[1],params.m_angular_lower_limit[2]);
        btVector3 angularUpperLimitVec(params.m_angular_upper_limit[0],params.m_angular_upper_limit[1],params.m_angular_upper_limit[2]);
        
        cc->setAngularLowerLimit(angularLowerLimitVec);
        cc->setAngularUpperLimit(angularUpperLimitVec);
        
        //spring
        if(params.m_use_spring) {
            cc->enableSpring(0, true);
            cc->enableSpring(1, true);
            cc->enableSpring(2, true);
            cc->enableSpring(3, true);
            cc->enableSpring(4, true);
            cc->enableSpring(5, true);
            
            
            // Stiffness is a spring coefficient, not an angle. Keep the same
            // native Bullet units as the direct stiffness setters below.
            btVector3 transStiff(params.m_translate_stiffness[0],
                                params.m_translate_stiffness[1],
                                params.m_translate_stiffness[2]);
            btVector3 rotStiff(params.m_rotate_stiffness[0],
                              params.m_rotate_stiffness[1],
                              params.m_rotate_stiffness[2]);
            btVector3 transDamp(params.m_translate_damping[0],
                                params.m_translate_damping[1],
                                params.m_translate_damping[2]);
            btVector3 rotDamp(params.m_rotate_damping[0],
                              params.m_rotate_damping[1],
                              params.m_rotate_damping[2]);
            
            
            cc->setStiffness(0, transStiff[0]);
            cc->setStiffness(1, transStiff[1]);
            cc->setStiffness(2, transStiff[2]);
            
            cc->setStiffness(3, rotStiff[0]);
            cc->setStiffness(4, rotStiff[1]);
            cc->setStiffness(5, rotStiff[2]);
            
            cc->setDamping(0, transDamp[0]);
            cc->setDamping(1, transDamp[1]);
            cc->setDamping(2, transDamp[2]);
            
            cc->setDamping(3, rotDamp[0]);
            cc->setDamping(4, rotDamp[1]);
            cc->setDamping(5, rotDamp[2]);
            
        }
        
        //common
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
    }
    
    
    
    
    virtual void set_breakThreshold(float d) {
        btGeneric6DofSpringConstraint* p2pc = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        p2pc->setBreakingImpulseThreshold(d);
    }
    
    virtual void set_enabled(bool e) {
        btGeneric6DofSpringConstraint* p2pc = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        p2pc->setEnabled(e);
    }
    
    
    virtual void set_LinLimit(btVector3& lower, btVector3& upper) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        
        sixdof->setLinearLowerLimit(lower);
        sixdof->setLinearUpperLimit(upper);
    }
    
    virtual void set_AngLimit(btVector3& lower, btVector3& upper) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setAngularLowerLimit(lower);
        sixdof->setAngularUpperLimit(upper);
    }
    
    
    virtual void set_enableSpring(bool enabled) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->enableSpring(0, enabled);
        sixdof->enableSpring(1, enabled);
        sixdof->enableSpring(2, enabled);
        sixdof->enableSpring(3, enabled);
        sixdof->enableSpring(4, enabled);
        sixdof->enableSpring(5, enabled);
    }
    
    virtual void set_translateStiffness(btVector3& stiff) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setStiffness(0, stiff[0]);
        sixdof->setStiffness(1, stiff[1]);
        sixdof->setStiffness(2, stiff[2]);
    }
    
    virtual void set_rotateStiffness(btVector3& stiff) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setStiffness(3, stiff[0]);
        sixdof->setStiffness(4, stiff[1]);
        sixdof->setStiffness(5, stiff[2]);
    }
    
    virtual void set_translateDamping(btVector3& damp) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setDamping(0, damp[0]);
        sixdof->setDamping(1, damp[1]);
        sixdof->setDamping(2, damp[2]);
    }
    
    virtual void set_rotateDamping(btVector3& damp) {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setDamping(3, damp[0]);
        sixdof->setDamping(4, damp[1]);
        sixdof->setDamping(5, damp[2]);
    }
    
    virtual void setEquilibriumPoint() {
        btGeneric6DofSpringConstraint* sixdof = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        sixdof->setEquilibriumPoint(); // set current position as equilibrium point
    }
    
    
    
    virtual void get_local_frameA(btVector3& p, btQuaternion& r)const
    {
        const btGeneric6DofSpringConstraint* sc = static_cast<const btGeneric6DofSpringConstraint*>(m_constraint.get());
        p = sc->getFrameOffsetA().getOrigin();
        r = sc->getFrameOffsetA().getRotation();
        
    }
    
    virtual void get_local_frameB(btVector3& p, btQuaternion& r) const
    {
        const btGeneric6DofSpringConstraint* sc = static_cast<const btGeneric6DofSpringConstraint*>(m_constraint.get());
        p = sc->getFrameOffsetB().getOrigin();
        r = sc->getFrameOffsetB().getRotation();
    }
    
    virtual void get_frameA(btVector3& p, btQuaternion& r) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform& btxform = sc->getFrameOffsetA();
        r = btxform.getRotation();
        p = btxform.getOrigin();
    }
    virtual void get_frameB(btVector3& p, btQuaternion& r) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform& btxform = sc->getFrameOffsetB();
        r = btxform.getRotation();
        p = btxform.getOrigin();
        
    }
    virtual void get_invFrameA(btVector3& p, btQuaternion& r) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform btxform = sc->getFrameOffsetA().inverse();
        r = btxform.getRotation();
        p = btxform.getOrigin();
    }
    virtual void get_invFrameB(btVector3& p, btQuaternion& r) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform btxform = sc->getFrameOffsetB().inverse();
        r = btxform.getRotation();
        p = btxform.getOrigin();
        
    }
    virtual void worldToA(btVector3& w, btVector3& p) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform w2a = (sc->getRigidBodyA().getWorldTransform() * sc->getFrameOffsetA()).inverse();
        btVector3 bw(w[0], w[1], w[2]);
        p = w2a * bw;
        
    }
    virtual void worldFromB(btVector3& p, btVector3& w) const
    {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        const btTransform b2w = sc->getRigidBodyB().getWorldTransform() * sc->getFrameOffsetB();
        btVector3 bp(p[0], p[1], p[2]);
        w = b2w * bp;
    }
    
    virtual void set_world(btVector3 const &p, btQuaternion const& r) {
        btGeneric6DofSpringConstraint* sc = static_cast<btGeneric6DofSpringConstraint*>(m_constraint.get());
        btTransform frameAinW(r, p);
        btTransform frameA = sc->getRigidBodyA().getWorldTransform().inverse() * frameAinW;
        btTransform frameB = sc->getRigidBodyB().getWorldTransform().inverse() * frameAinW;
        sc->getFrameOffsetA() = frameA;
        sc->getFrameOffsetB() = frameB;
    }
    
    virtual void get_world(btVector3 &p, btQuaternion& r) const {
        btGeneric6DofSpringConstraint const* sc = static_cast<btGeneric6DofSpringConstraint const*>(m_constraint.get());
        btTransform frameAinW = sc->getRigidBodyA().getWorldTransform() * sc->getFrameOffsetA();
        r = frameAinW.getRotation();
        p = frameAinW.getOrigin();
    }
    
    virtual void update_constraint(btRigidBody& /*rigidBody*/)
    {
        setPivotChanged(true);
    }
    
    
    

    
    
    db_sixdof_spring_constraint(btRigidBody& rb, btTransform const& transA, bool useLinearReferenceFrameB):
    db_constraint_base()
    {
        btGeneric6DofSpringConstraint* sixdof = new btGeneric6DofSpringConstraint(rb, transA, useLinearReferenceFrameB);
        m_constraint.reset(sixdof);
    }
    db_sixdof_spring_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transA, btTransform const& transB, bool useLinearReferenceFrameB):
    db_constraint_base()
    {
        btGeneric6DofSpringConstraint* sixdof = new btGeneric6DofSpringConstraint(rbA, rbB, transA, transB, useLinearReferenceFrameB);
        m_constraint.reset(sixdof);
    }
    
    
    
private:
    
};





class db_hinge_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btHingeConstraint* cc = static_cast<btHingeConstraint*>(m_constraint.get());
        
        //set Limit
        cc->setLimit(params.m_hige_low,//1
                     params.m_hige_high,//-1
                     params.m_softness,//0.9
                     params.m_biasFactor,//0.3
                     params.m_relaxationFactor);//1.0
        
        
        
        //axis
        btVector3 axisVec(params.m_axis[0],params.m_axis[1],params.m_axis[2]);
        cc->setAxis(axisVec);
        
        
        
        //common
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
        
        
    }
    
    virtual void set_enabled(bool e) {
        btHingeConstraint* cc = static_cast<btHingeConstraint*>(m_constraint.get());
        cc->setEnabled(e);
    }
    
    
    db_hinge_constraint(btRigidBody& rb, btTransform const& transA, bool useReferenceFrameA = false):
    db_constraint_base()
    {
        btHingeConstraint* sixdof = new btHingeConstraint(rb, transA, useReferenceFrameA);
        m_constraint.reset(sixdof);
    }
    db_hinge_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transA, btTransform const& transB, bool useReferenceFrameA = false):
    db_constraint_base()
    {
        btHingeConstraint* sixdof = new btHingeConstraint(rbA, rbB, transA, transB, useReferenceFrameA);
        m_constraint.reset(sixdof);
        
    }
    
};



class db_slider_constraint: public db_constraint_base {
public:
    
    virtual void set_param(DbtConstraintParams &params) {
        
        btSliderConstraint* cc = static_cast<btSliderConstraint*>(m_constraint.get());
        
        //set Slider
        cc->setLowerLinLimit(1.0);
        cc->setUpperLinLimit(-1.0);
        
        cc->setLowerAngLimit(0.);
        cc->setUpperAngLimit(0.);
        
        
        //Lim
        cc->setSoftnessLimLin(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingLimLin(0.);
        cc->setRestitutionLimLin(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_STOP_CFM, SLIDER_CONSTRAINT_DEF_CFM, 0);//Linear CFM
        
        cc->setSoftnessLimAng(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingLimAng(0.);
        cc->setRestitutionLimAng(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_STOP_CFM, SLIDER_CONSTRAINT_DEF_CFM, 3);//Angle CFM
        
        //Dir
        cc->setSoftnessDirLin(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingDirLin(0.);
        cc->setRestitutionDirLin(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_CFM, SLIDER_CONSTRAINT_DEF_CFM, 0);//Linear CFM
        
        cc->setSoftnessDirAng(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingDirAng(0.);
        cc->setRestitutionDirAng(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_CFM, SLIDER_CONSTRAINT_DEF_CFM, 3);//Linear CFM
        //Ortho
        cc->setSoftnessOrthoLin(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingOrthoLin(0.);
        cc->setRestitutionOrthoLin(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_STOP_CFM, SLIDER_CONSTRAINT_DEF_CFM, 2);//Linear CFM
        
        cc->setSoftnessOrthoAng(SLIDER_CONSTRAINT_DEF_SOFTNESS);
        cc->setDampingOrthoAng(0.);
        cc->setRestitutionOrthoAng(SLIDER_CONSTRAINT_DEF_RESTITUTION);
        cc->setParam(BT_CONSTRAINT_STOP_CFM, SLIDER_CONSTRAINT_DEF_CFM, 5);//Angle CFM
//        cc->setUseFrameOffset(bool frameOffsetOnOff);
        
        
        //common
        if(params.m_use_break_threshold) {
            cc->setBreakingImpulseThreshold(params.m_break_threshold);
        }
        
        if(params.m_override_iterations>0) {
            cc->setOverrideNumSolverIterations(params.m_override_iterations);
        }
        
        cc->setDbgDrawSize(params.m_debug_display_size);
        
        
    }
    
    virtual void set_enabled(bool e) {
        btSliderConstraint* cc = static_cast<btSliderConstraint*>(m_constraint.get());
        cc->setEnabled(e);
    }
    
    
    db_slider_constraint(btRigidBody& rb, btTransform const& transA, bool useReferenceFrameA = false):
    db_constraint_base()
    {
        btSliderConstraint* sixdof = new btSliderConstraint(rb, transA, useReferenceFrameA);
        m_constraint.reset(sixdof);
    }
    db_slider_constraint(btRigidBody& rbA, btRigidBody& rbB, btTransform const& transA, btTransform const& transB, bool useReferenceFrameA = false):
    db_constraint_base()
    {
        btSliderConstraint* sixdof = new btSliderConstraint(rbA, rbB, transA, transB, useReferenceFrameA);
        m_constraint.reset(sixdof);
        
    }
    
};

