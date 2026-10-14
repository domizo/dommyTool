//
//  DMbtSoft.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/10.
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "commonDBulletCollision.h"
#include "commonBulletSoftBody.h"
#include "commonDBullet.h"




// Exact connectivity snapshot: deformation is allowed, topology changes need a restart.
struct DBulletSoftTopology {
    std::size_t points = 0;
    std::vector<int> faces, connections;
    bool operator==(const DBulletSoftTopology& other) const {
        return points == other.points && faces == other.faces && connections == other.connections;
    }
};

//BulletEnginのためのSoftBodyを生成
class DMbtSoft final : public MPxNode
{
public:
    DMbtSoft();
    ~DMbtSoft() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDM;
    
    static MObject aMass;
    static MObject aVolumeMass;
    
    static MObject aRestLengthScale;
    static MObject aRestitution;
    static MObject aFriction;
    static MObject aRollingFriction;
    static MObject aSpinningFriction;
    
    
    static MObject aStiffness;
    static MObject aAngularStiffness;
    static MObject aVolumeStiffness;
    
    static MObject aIsGenerateBendConstraint;
    static MObject aIsCreateCluster;
    
    
    
    //! Dynamic friction coefficient
    static MObject aAeromodel;
    
    
    static MObject aVelocityCorrectionFactor;
    static MObject aDynamicFrictionCoeff;
    static MObject aDamping;
    static MObject aDrag;
    static MObject aLift;
    static MObject aPressure;
    static MObject aVolumeConversation;
    static MObject aPoseMatch;
    
    static MObject aRigidContactHardness;
    static MObject aKineticContactHardness;
    static MObject aSoftContactHardness;
    static MObject aAnchorContactHardness;
    
    
    static MObject aRigidImpulseSplit;
    static MObject aKinetictImpulseSplit;
    static MObject aSoftImpulseSplit;
    
    static MObject aSetPoseFrame;
    static MObject aSetPoseVolume;
    
    static MObject aIsRandomizeConstraints;
    
    
    static MObject aBendConstraintDistance;
    static MObject aNumClusters;
    
//    static MObject aInitialPosition;
//    static MObject aInitialRotation;
//    static MObject aInitialVelocity;
//    static MObject aInitialSpin;
    
    //For collision shape Param
//    static MObject aCollisionType;
//    static MObject aCollisionScale;
    static MObject aCollisionMargin;
    static MObject aPositionIterations;
    static MObject aVelocityIterations;
    static MObject aDriftIterations;
    static MObject aClusterIterations;
    
    static MObject aMaxVolume;
    
    static MObject aCollideRigidBody;
    static MObject aCollideSoftBody;
    static MObject aCollideSelf;
    
    static MObject aScale;
    
    static MObject outDM;
    
public:
    MStatus evalParms(btSoftParams& parms);
    
    
    MStatus resetSoftBodies(
        btSoftRigidDynamicsWorld* m_dynamicsWorld,
        btSoftBodyWorldInfo* m_worldInfo,
        const DBulletWorldReferencePtr& worldReference);
    MStatus updateSoftBodies(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo);
    void releaseSoftBodies() noexcept;
    
private:
    DBulletWeakWorldReference m_worldReference;
    
    std::vector<db_softbody_base*> softBodyShapes;
    std::vector<DBulletSoftTopology> m_inputTopology;
    std::size_t m_nativeWork = 0;
    
    
    size_t preSoftBodyCount;
};
    
    
    
    
    
