//
//  DCbtRope.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/13.
//


#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "commonDBulletCollision.h"
#include "commonBulletRigidBody.h"
#include "commonBulletSoftBody.h"
#include "commonBulletRope.h"

#include "commonDBulletConstraint.h"
#include "db_constraint_base.h"
#include "commonDBullet.h"


//BulletEnginのためのSoftBodyを生成
class DCbtRope final : public MPxNode
{
public:
    ~DCbtRope() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDC;
    
    static MObject aType;
    static MObject aMass;
    static MObject aRestitution;//反発
    static MObject aFriction;//摩擦
    static MObject aRollingFriction;//回転摩擦
    static MObject aLinearDamping;//減衰
    static MObject aAngularDamping;//回転減衰
    
    
    static MObject aRadius;
    
    
    
    
    
    static MObject aBehaviorType;
    //constraint
    static MObject aLockRoot;
    static MObject aLockTip;
    
    static MObject aLockRootType;
    static MObject aLockRootPosition;
    static MObject aRootDP;
    
    static MObject aLockTipType;
    static MObject aLockTipPosition;
    static MObject aTipDP;
    
    static MObject aIsLockAnimation;
    
    
    //free
    static MObject aConstAngularDamping;
    
    //angle
    static MObject aLimitAngle;
    static MObject aSoftness;
    static MObject aBiasFactor;
    static MObject aRelaxationFactor;
    
    //shape spring
    static MObject aUseUniformStiffness;
    static MObject aUseUniformDamping;
    static MObject aUniformStiffness;
    static MObject aUniformDamping;
    
    static MObject aTranslateStiffness;
    static MObject aRotateStiffness;
    static MObject aTranslateDamping;
    static MObject aRotateDamping;
    static MObject aIsRootSpring;
    
    static MObject aDebugDisplaySize;
    
    
    
    
    static MObject outDC;
    
public:
    MStatus evalParms(btRopeParams& parms);
    
    
    MStatus resetRopes(
        btSoftRigidDynamicsWorld* m_dynamicsWorld,
        btSoftBodyWorldInfo* m_worldInfo,
        const DBulletWorldReferencePtr& worldReference);
    MStatus updateRopes(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo);
    void detachFromWorld() noexcept;
    
    MStatus updateLockPosition(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo, float time);
    
    
private:
    DBulletWeakWorldReference m_worldReference;
    btRopeParams m_initialParameters;
    bool m_initialized = false;
    std::vector<std::size_t> m_curvePointCounts;
    std::vector<db_collision_shape_base*> ropeShapes;
    std::vector<db_collision_shape_base*> rootRopeShapes;
    std::vector<db_constraint_base*> rootConstLists;//根元のコンストレイントのリスト
    std::vector<db_collision_shape_base*> tipRopeShapes;
    std::vector<db_constraint_base*> tipConstLists;//根元のコンストレイントのリスト
    
    std::vector<db_constraint_base*> midConstLists;//中間のコンストレイントのリスト
    
    
    std::vector<btRigidBody*> rbList;//中間のRigidBodyのリスト
    std::vector<btRigidBody*> rootRbList;//根元のRigidBodyのリスト
    std::vector<btRigidBody*> tipRbList;
//    std::vector<db_softbody_base*> softBodyShapes;
    
//    std::vector<btTransform> startTransformList;
//    std::vector<MVector> startScaleList;
    
};

