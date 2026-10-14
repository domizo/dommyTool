//
//  DCbtRigid2D.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/14.
//


#pragma once


#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"


#include "commonBulletRigidBody.h"

#include "commonDBullet.h"


///BulletEnginのための2DRigidBodyを生成
class DCbtRigid2D final : public MPxNode
{
public:
    ~DCbtRigid2D() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDC;
    
    
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
    
    static MObject aExternalForce;
    static MObject aExternalTorque;
    
    
    //    static MObject aContactCount;
    //    static MObject aContactName;
    //    static MObject aContactPosition;
    static MObject aIsCompound;
    static MObject aCompoundGroupName;
    
    //For collision shape Param
//    static MObject aCollisionType;
    static MObject aCollisionScale;
    static MObject aCollisionMargin;
    
    static MObject aIsCollisionDeform;
    //    static MObject ca_collisionShape;
    //    static MObject ca_collisionShapeParam;
    //    static MObject oa_collisionShape;
    static MObject aStartSleeping;
    
    static MObject outDC;
    
public:
    MStatus evalParms(btRigidParams& parms);
    // Called by the solver: rebuild at its StartTime, refresh on advance.
    MStatus prepareCollisionGeometry(bool rebuild,
        DBulletBodyMode mode = DBulletBodyMode::Dynamic);
    void clearCollisionGeometry() noexcept;
    
    void clearRigidBodies();
    MStatus resetRigidBodies(
        btDiscreteDynamicsWorld* m_dynamicsWorld,
        const DBulletWorldReferencePtr& worldReference);
    MStatus resetRigidBodiesKinematic(
        btDiscreteDynamicsWorld* m_dynamicsWorld,
        const DBulletWorldReferencePtr& worldReference);
    void detachFromWorld() noexcept;
    
    MStatus updateRigidBodies(btDiscreteDynamicsWorld* m_dynamicsWorld,
        DBulletBodyMode mode = DBulletBodyMode::Dynamic);
    MStatus updateRigidBodiesKinematic(btDiscreteDynamicsWorld* m_dynamicsWorld, float time);
    
    MStatus updateCollisionShape(btDiscreteDynamicsWorld* m_dynamicsWorld);
    
    
    std::vector<btRigidBody*>* getRigidBodyData();
    
    //  btAlignedObjectArray<btCollisionShape*> collisionShapes;
    std::vector<db_collision_shape_base*> collisionShapes;
    //Compound Data
    std::vector<DPIndexList> compoundIdLists;
    std::vector<db_collision_shape_base*>compoundCollisionLists;
    //    DBTCollisionShapeList collisionShapes;
    std::vector<btRigidBody*> rbList;
    std::vector<btRigidBody*> compRbList;
    //    std::vector<btRigidBody*> m_rigid_bodies;
private:
    DBulletWeakWorldReference m_worldReference;
    std::vector<btTransform> startTransformList;
    std::vector<MVector> startScaleList;
    std::vector<MMatrix> kinematicTransformList;
    
    std::vector<btTransform> compoundStartTransformList;
    
    size_t preCollsionCount = 0;
    std::size_t m_geometryCount = 0;
    bool m_geometryGrouped = false;
    std::string m_geometryGroupPattern;
};
