//
//  DBLinkConstraint.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "DBulletDataHeaders.h"
#include "db_constraint_base.h"
#include "commonDBullet.h"




struct DBLinkConstParams
{
    DBLinkConstParams()
    : m_is_enable(true)
    , m_is_update(false)
    { }
    
    
    bool m_is_enable;
    bool m_is_update;
    
};

//BulletEnginのためのConstraintを生成
class DBLinkConstraint final : public MPxNode
{
public:
    ~DBLinkConstraint() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inRB;
    
    static MObject aTargetRB;
    
    static MObject inConst;
    
    static MObject inConnectInfoDC;
    
    
    
    static MObject aIsEnable;
    static MObject aIsUpdate;
    
    static MObject outConst;
    
public:
    
    MStatus evalParms(DBLinkConstParams& parms);
    MStatus createConstraint(
        btSoftRigidDynamicsWorld* m_dynamicsWorld,
        const DBulletWorldReferencePtr& worldReference);
    MStatus updateConstraint(btSoftRigidDynamicsWorld* m_dynamicsWorld, float time);
    void detachFromWorld() noexcept;
    
private:
    DBulletWeakWorldReference m_worldReference;
    
    
    std::vector<db_constraint_base*> constraintLists;//生成されたコンストレイントのリスト
   
    
};
