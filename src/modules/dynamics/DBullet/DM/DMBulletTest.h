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

Modified by Francisco Gochez <fjgochez@gmail.com>
Nov 2011 - Dec 2011 : Added soft body logic

Modified by Dongsoo Han <dongsoo.han@amd.com>
04/11/2012 : Added contactData attribute and contact information code to enable/disable updating contactCount, contactName and 
			 contactPosition attributes in rigidBodyNode.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/dSolverNode.h

//
//  DMBulletTest.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/02.
//

#pragma once






#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MTime.h>

#include <vector>

#include "mathUtils.h"
#include "DBRigidBodyNode.h"

#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "Solver/DBulletDynamicsWorld.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"

#include "commonDBullet.h"
#include "DBulletResourceOwnership.h"




enum {
    DB_SLV_INIT_RB = 0,
    DB_SLV_INIT_KINEMA,
    DB_SLV_INIT_SB,
    DB_SLV_INIT_ROPE,
    DB_SLV_INIT_CONST,
    DB_SLV_UPDATE_RB,
    DB_SLV_UPDATE_SHAPE_RB,
    DB_SLV_UPDATE_KINEMA,
    DB_SLV_UPDATE_SB,
    DB_SLV_UPDATE_ROPE,
    DB_SLV_UPDATE_CONST,
    DB_SLV_TRANS_KINEMA,
    DB_SLV_TRANS_ROPE,
    DB_SLV_DETACH_WORLD_RESOURCES,
    DB_SLV_PROC_TYPE_COUNT
};typedef short DB_SLV_PROC_TYPE;

struct DBtSolverOptionParams
{
    DBtSolverOptionParams()
    : m_solver_proc_type(DB_SLV_PROC_TYPE_COUNT)
    , m_timestep(0.0)
    { }
    
    DB_SLV_PROC_TYPE m_solver_proc_type;
    
    float m_timestep;
};

///DBulletSolver Bulletを使用したリジッドボディシミュレーションを行います。
class DMBulletTest final : public MPxLocatorNode
{
public:
    
    DMBulletTest();
    ~DMBulletTest() override;
    void postConstructor() override;
    void draw(    M3dView & view, const MDagPath & path,
                      M3dView::DisplayStyle style,
                      M3dView::DisplayStatus status ) override;
    
    
    bool isBounded() const override {
        return false;
    }
    MBoundingBox boundingBox() const override
    {
        MPoint corner1(-1, -1, -1);
        MPoint corner2(1, 1, 1);
        return MBoundingBox(corner1, corner2);
    }
    
    bool        excludeAsLocator() const override {
        return false;
    }
    bool        isTransparent() const override {
        return false;
    }
    
    static MTypeId typeId;
    static const MString typeName;
    static  void * creator();
    static  MStatus initialize();
    
    bool setInternalValueInContext ( const  MPlug & plug, const  MDataHandle & dataHandle,  MDGContext & ctx ) override;
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    SchedulingType schedulingType() const override;
    
    
    
    //Attribute
    static MObject inDM;
    static MObject inKinematicDMs;
    static MObject inSoftDM;
    static MObject inRopeDC;
    
    static MObject inConst;
    
    
    static MObject aEnabled;
    static MObject aCurrentTime;
    static MObject aStartTime;
    
    
    //solver attribute
    static MObject aGravity;
    static MObject aAirDensity;
    static MObject aWaterDensity;
    static MObject aWaterOffset;
    static MObject aWaterNormal;
    static MObject aMaxDisplacement;
    
    static MObject aCollisionMargin;
    static MObject aDisableCollisionsBetweenLinkedBodies;
    static MObject aSplitImpulse;
    static MObject aSubsteps;
    static MObject aFixedPhysicsRate;
    
    static MObject aContactData;
    
    //
    
   
    
    
    //plane attribute
    static MObject aIsPlane;
    static MObject aPlanePosition;
    static MObject aPlaneAxis;
    static MObject aPlaneRestitution;
    static MObject aPlaneFriction;
    
    //field attribute
    static MObject aAffectMayaFiels;
    
    //contact data
    static MObject outContactDP;
    
    
    //check problem
    static MObject aIsCheckAndDeletePanicSoftBody;
    
    static MObject ia_DBG_DrawWireframe;
    static MObject ia_DBG_DrawAabb;
    static MObject ia_DBG_DrawFeaturesText;
    static MObject ia_DBG_DrawContactPoints;
    static MObject ia_DBG_NoDeactivation;
    static MObject ia_DBG_NoHelpText;
    static MObject ia_DBG_DrawText;
    static MObject ia_DBG_ProfileTimings;
    static MObject ia_DBG_EnableSatComparison;
    static MObject ia_DBG_DisableBulletLCP;
    static MObject ia_DBG_EnableCCD;
    static MObject ia_DBG_DrawConstraints;
    static MObject ia_DBG_DrawConstraintLimits;
    static MObject ia_DBG_FastWireframe;
    
    
    
    static MObject outDM;
    static MObject outSoftDM;
    static MObject outRopeDC;
    
    
    static float collisionMarginOffset; //mb
    
public:
    
    bool m_reInitialize;
    
    
protected:
    
    struct xforms_t {
        vec3f m_x0;
        vec3f m_x1;
        quatf m_q0;
        quatf m_q1;
    };
    
    
    //solver method
    void initBullet(MDataBlock& data);
    void clearBullet() noexcept;
    
    
    
    
    //useful method
    MStatus solverProcessFromPlug(MObject targetPlugObj, DBtSolverOptionParams option);
    
    
    //mesh meshod
    MStatus initRigidBodies();
    MStatus updateRigidBodies(
        DMeshDataPtrList* outDMeshList, UuidList* nodeIdList);
    void updateSoftBodies(
        DMeshDataPtrList* outDMeshList, UuidList* nodeIdList,
        std::vector<DPIdList>* panicIdList);
    bool updateRopes(
        DCurveDataPtrList* outDCurveList, UuidList* nodeIdList);
    MStatus checkRigidBodies();
    void deleteRigidBodies() noexcept;
    
    //field method
    MStatus applyFields(MDataBlock& data, float dt, dommy::bullet::SoftBodyForces& forces);
    
protected:
    struct ContactSamples {
        DPointList positions;
        DFloatArray distances, impulses, ages;
        DVec3DataList normals;

        void swap(ContactSamples& other) noexcept {
            positions.swap(other.positions);
            distances.swap(other.distances);
            impulses.swap(other.impulses);
            ages.swap(other.ages);
            normals.swap(other.normals);
        }
    };
    MStatus appendContactSamples(ContactSamples& samples) const;
    ContactSamples m_contactSamples;
    bool m_contactSamplesValid = false;
    
    MTime m_prevTime;
    bool m_requiresStartFrame = true;
    std::vector<double> m_worldSettings;
    
    DBulletRigidBodyPtr m_plane_rb;
    std::unique_ptr<btCollisionShape> m_plane_col_shape;
    
private:
    DBulletWorldReferencePtr m_worldReference;
    std::unique_ptr<btDefaultCollisionConfiguration>
        m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btConstraintSolver> m_solver;
    DBulletSoftBodyWorldInfoPtr m_worldInfo;
    std::unique_ptr<btIDebugDraw> m_debugDrawer;
    std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld> m_dynamicsWorld;
};
