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
//  DBSolverNode.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/26.
//

#pragma once

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MTime.h>
#include <maya/MMessage.h>
#include <maya/MObjectHandle.h>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "mathUtils.h"
#include "DBRigidBodyNode.h"
#include "dbSolver_t.h"

/// DBulletSolver Bulletを使用したリジッドボディシミュレーションを行います。
class DBSolverNode final : public MPxLocatorNode {
  public:
    DBSolverNode();
    ~DBSolverNode() override;
    void postConstructor() override;
    void draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style,
              M3dView::DisplayStatus status) override;

    bool isBounded() const override {
        return false;
    }
    MBoundingBox boundingBox() const override {
        MPoint corner1(-1, -1, -1);
        MPoint corner2(1, 1, 1);
        return MBoundingBox(corner1, corner2);
    }

    bool excludeAsLocator() const override {
        return false;
    }
    bool isTransparent() const override {
        return false;
    }

    SchedulingType schedulingType() const override;

    static MTypeId typeId;
    static const MString typeName;
    static void* creator();
    static MStatus initialize();
    static MStatus registerCallbacks();
    static MStatus removeCallbacks();

    bool setInternalValueInContext(const MPlug& plug, const MDataHandle& dataHandle,
                                   MDGContext& context) override;

    MStatus initConstraint(MObject& bodyNode);
    MStatus initRigidBodyArray(MObject& node);
    MStatus initRigidBody(MObject& node);

    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    // Attribute
    static MObject inDP;
    static MObject inDM;

    static MObject aEnabled;
    static MObject aCurrentTime;
    static MObject aStartTime;

    // solver attribute
    static MObject aGravity;
    static MObject aCollisionMargin;
    static MObject aDisableCollisionsBetweenLinkedBodies;
    static MObject aSplitImpulse;
    static MObject aSubsteps;
    static MObject aFixedPhysicsRate;

    static MObject aRigidBodies;
    static MObject aSoftBodies;
    static MObject aContactData;

    static MObject ia_DBG_DrawWireframe;
    static MObject ia_DBG_DrawAabb;
    static MObject ia_DBG_DrawContactPoints;
    static MObject ia_DBG_NoDeactivation;
    static MObject ia_DBG_ProfileTimings;
    static MObject ia_DBG_DrawConstraints;
    static MObject ia_DBG_DrawConstraintLimits;

    static MObject outDM;

    static float collisionMarginOffset; // mb

  public:
    static bool isStartFrame() noexcept;
    bool m_reInitialize;

    static MStatus updateAllRigidBodies();
    static MCallbackId updateSceneCallbackId;
    static MCallbackId sceneLoadedCallbackId;

  protected:
    static std::atomic<bool> s_isStartTime;
    static std::atomic<DBSolverNode*> s_activeSolver;

    struct xforms_t {
        vec3f m_x0;
        vec3f m_x1;
        quatf m_q0;
        quatf m_q1;
    };

    MStatus computeRigidBodies(const MPlug& plug, MDataBlock& data);
    //    void computeSoftBodies(const MPlug& plug, MDataBlock& data);
    MStatus initSoftBody(MObject& node);
    void dumpRigidBodyArray(MObject& node);
    bool expandFileExpression(std::string const& expr, std::string& base_name,
                              std::string& extension);

    MStatus initRigidBodies(
        MPlugArray& rbConnections, MDataBlock& data);
    MStatus gatherPassiveTransforms(
        MPlugArray& rbConnections, std::vector<xforms_t>& xforms);
    MStatus updatePassiveRigidBodies(
        MPlugArray& rbConnections, const std::vector<xforms_t>& xforms,
        float t);
    MStatus updateActiveRigidBodies(MPlugArray& rbConnections);
    MStatus updateActiveSoftBodies(MPlugArray& sbConnections);
    MStatus applyFields(MPlugArray& rbConnections, float dt,
                        dbSolver_t::SoftBodyForces& softForces);
    MStatus updateConstraint(MObject& bodyNode);
    MStatus clearContactRelatedAttributes(
        MPlugArray& rbConnections);

    MStatus refreshContactNodes(const MPlugArray& connections);
    MStatus resolveRigidBodyNode(const btCollisionObject* collisionObject,
                                DBRigidBodyNode*& node) const;
    MStatus updateContactInfo(MPlugArray& connections);
    bool acquireSharedWorld();
    MStatus updateDebugDrawMode(MDataBlock& data);

  protected:
    MTime m_prevTime;
    MTime m_initializedStartTime;
    struct ContactNode {
        MObjectHandle node;
        std::weak_ptr<dbRigidBody_t> body;
    };
    typedef std::map<const btCollisionObject*, ContactNode> ContactNodes;
    ContactNodes m_contactNodes;
    std::atomic<int> m_debugDrawMode;
    bool m_ownsSharedWorld;
    bool m_requiresStartFrame;
    bool m_startFrameErrorReported;
};
