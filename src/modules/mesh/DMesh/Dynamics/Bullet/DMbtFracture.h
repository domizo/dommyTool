//
//  DMbtFracture.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/21.
//



#pragma once

#include <vector>

#include <maya/MPxNode.h>


#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "commonDBulletCollision.h"
#include "commonBulletRigidBody.h"
#include "db_constraint_base.h"
#include "db_collision_shape_base.h"
#include "commonDBullet.h"



///BulletEnginのためのRigidBodyを生成 (Fracture専用)
class DMbtFracture final : public MPxNode
{
public:
    ~DMbtFracture() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDM;
    static MObject inConnectInfoDC;
    
    
    
    //For RigidBody Param
    
    static MObject aMass;
    static MObject aRestitution;//反発
    static MObject aFriction;//摩擦
    static MObject aRollingFriction;//回転摩擦
    static MObject aLinearDamping;//減衰
    static MObject aAngularDamping;//回転減衰
    
    static MObject aInitialPosition;
    static MObject aInitialRotation;
    static MObject aInitialVelocity;
    static MObject aInitialSpin;
    
   
    
    //For collision shape Param
    static MObject aCollisionType;
    static MObject aCollisionMargin;
    
    
    //cluster
    static MObject aUseCluster;
    static MObject aClusterGroupName;
    
    
    //constraint
    static MObject aUseBreakThreshold;
    static MObject aBreakThreshold;
    static MObject aUseBreakAngle;
    static MObject aBreakAngle;
    
    //cone
    //angle
    static MObject aLimitAngle;
    static MObject aSoftness;
    static MObject aBiasFactor;
    static MObject aRelaxationFactor;
    
    static MObject aOverrideIterations;
    static MObject aDebugDisplaySize;
    
    static MObject aStartTime;
    
    static MObject outDM;
    
public:
    MStatus evalParms(btRigidParams& parms);
    // Called by the solver: rebuild at its StartTime, refresh on advance.
    MStatus prepareCollisionGeometry(bool rebuild);
    void clearCollisionGeometry() noexcept;
    
    MStatus resetRigidBodies(
        btSoftRigidDynamicsWorld* m_dynamicsWorld,
        const DBulletWorldReferencePtr& worldReference);
    MStatus updateRigidBodies(btSoftRigidDynamicsWorld* m_dynamicsWorld);
    void detachFromWorld() noexcept;
    
    
    std::vector<btRigidBody*>* getRigidBodyData();
    
    
    std::vector<btRigidBody*> rbList;
    std::vector<btRigidBody*> clsRbList;
    std::vector<db_collision_shape_base*> collisionShapes;
    std::vector<db_constraint_base*> connectConstLists;
    
    
    //Cluster Data
    std::vector<DPIndexList> clusteredIdLists;
    std::vector<db_collision_shape_base*> clusteredCollisionLists;
    
private:
    DBulletWeakWorldReference m_worldReference;
    std::vector<btTransform> startTransformList;
    std::vector<btTransform> clusteredStartTransformList;
    
    size_t preCollsionCount = 0;
    std::size_t m_geometryCount = 0;
    bool m_geometryGrouped = false;
    std::string m_geometryGroupPattern;
};
