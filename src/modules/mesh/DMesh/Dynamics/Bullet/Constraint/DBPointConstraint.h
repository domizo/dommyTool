//
//  DBPointConstraint.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/24.
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "DBulletDataHeaders.h"
#include "db_constraint_base.h"
#include "db_collision_shape_base.h"

#include "commonDBullet.h"


struct DBPointConstParams
{
    DBPointConstParams()
    : m_target_type(DB_CONST_TAR_POINT)
    , m_is_enable(true)
    , m_is_update(false)
    , m_position(MVector(0,0,0))
    , m_debug_display_size(1.0)
    { }
    
    DBConstraintTargetType m_target_type;
    
    bool m_is_enable;
    bool m_is_update;
    
    //constraint
    MVector m_position;
    
    double m_debug_display_size;
};

//BulletEnginのためのConstraintを生成
class DBPointConstraint final : public MPxNode
{
public:
    ~DBPointConstraint() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inRB;
    static MObject inConst;
    
    
    static MObject aIsEnable;
    static MObject aIsUpdate;
    
    
    
    static MObject aTargetType;
    static MObject aPosition;
    static MObject aTargetRB;
    static MObject aTargetDP;
    
    static MObject aDebugDisplaySize;
    
    static MObject outConst;
    
public:
    
    MStatus evalParms(DBPointConstParams& parms);
    MStatus createConstraint(
        btSoftRigidDynamicsWorld* m_dynamicsWorld,
        const DBulletWorldReferencePtr& worldReference);
    MStatus updateConstraint(btSoftRigidDynamicsWorld* m_dynamicsWorld, float time);
    void detachFromWorld() noexcept;
    
private:
    DBulletWeakWorldReference m_worldReference;
    
    std::vector<db_constraint_base*> constraintLists;//生成されたコンストレイントのリスト
    std::vector<btRigidBody*> rbLists;
    std::vector<db_collision_shape_base*> colShapes;
    
};
