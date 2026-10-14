//
//  commonBulletRope.h
//  dommyTool
//
//  Created by yuzo on 2017/10/13.
//

#pragma once


#include "commonDBulletConstraint.h"
#include "db_constraint_base.h"

enum {
    DB_ROPE_SLV_RIGID = 0,
    DB_ROPE_SLV_SOFT,
    DB_ROPE_SOLVER_TYPE_COUNT
};typedef short DB_ROPE_SOLVER_TYPE;


enum {
    DB_ROPE_BHVR_FREE = 0,
    DB_ROPE_BHVR_ANGULAR_LIMIT,
    DB_ROPE_BHVR_SHAPE_SPRING,
    DB_ROPE_BEHAVIOR_TYPE_COUNT
};typedef short DB_ROPE_BEHAVIOR_TYPE;



enum {
    DB_LOCK_BASE = 0,
    DB_LOCK_POINT,
    DB_LOCK_DPOINT,
    DB_LOCK_POSITION_TYPE_COUNT
};typedef short DB_LOCK_POSITION_TYPE;





struct btRopeParams
{
    btRopeParams()
    : m_type(DB_ROPE_SLV_RIGID)
    , m_behavior_type(DB_ROPE_BHVR_FREE)
    , m_mass(1.0)
    , m_restitution(0.1)
    , m_friction(0.5)
    , m_rollingFriction(0.0)
    , m_linearDamping(0.3)
    , m_angularDamping(0.3)
    , m_radius(0.5)
    , m_lock_root(false)
    , m_lock_tip(false)
    , m_is_lock(false)
    , m_root_lock_type(DB_LOCK_BASE)
    , m_root_lock_pos()
    , m_tip_lock_type(DB_LOCK_BASE)
    , m_tip_lock_pos()
    , m_constParam(DbtConstraintParams())
    , m_is_root_spring(false)
//    , m_const_angular_damping(1.0)
//    , m_limit_angle(MVector())
//    , m_softness(1.0)
//    , m_biasFactor(0.3)
//    , m_relaxationFactor(1.0)
//    , m_debug_display_size(1.0)
    { }
    
    DB_ROPE_SOLVER_TYPE m_type;
    DB_ROPE_BEHAVIOR_TYPE m_behavior_type;
    
    double m_mass;
    
    //rigid body
    double m_restitution;
    double m_friction;
    double m_rollingFriction;
    double m_linearDamping;
    double m_angularDamping;
    
    double m_radius;
    
    
    //constraint
    bool m_lock_root;
    bool m_lock_tip;
    bool m_is_lock;
    DB_LOCK_POSITION_TYPE m_root_lock_type;
    MVector m_root_lock_pos;
    DB_LOCK_POSITION_TYPE m_tip_lock_type;
    MVector m_tip_lock_pos;
    
    DbtConstraintParams m_constParam;
    
    bool m_is_root_spring;
};


