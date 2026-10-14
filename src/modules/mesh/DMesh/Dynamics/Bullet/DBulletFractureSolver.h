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
//  DBulletFractureSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//

#pragma once


#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MTime.h>

#include <memory>
#include <vector>

#include "DBFractureDynamicsWorld.h"
#include "DMeshDataHeaders.h"


#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"



///DBulletSolver Bulletを使用したリジッドボディシミュレーションを行います。
class DBulletFractureSolver final : public MPxLocatorNode
{
public:
    
    DBulletFractureSolver();
    ~DBulletFractureSolver() override;
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
    
    static MObject aIsFractureMode;
    
    
    
    static MObject aDisableCollisionsBetweenLinkedBodies;
    static MObject aSplitImpulse;
    static MObject aSubsteps;
    static MObject aFixedPhysicsRate;
    
    static MObject aContactData;
    
    //plane attribute
    static MObject aIsPlane;
    static MObject aPlanePosition;
    static MObject aPlaneAxis;
    static MObject aPlaneRestitution;
    static MObject aPlaneFriction;
    
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
protected:
    //solver method
    void initBullet(MDataBlock& data);
    void clearBullet();
    
protected:
    
    MTime m_prevTime;
    bool m_requiresStartFrame = true;
    std::vector<double> m_worldSettings;
    
    std::unique_ptr<btCollisionShape> m_plane_col_shape;
    
private:
    std::unique_ptr<btDefaultCollisionConfiguration>
        m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btConstraintSolver> m_solver;
    std::unique_ptr<btIDebugDraw> m_debugDrawer;
    std::unique_ptr<DBFractureDynamicsWorld> m_dynamicsWorld;
};
