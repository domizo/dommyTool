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
Nov 2011 - Dec 2011 : Added logic for soft bodies

Modified by Dongsoo Han <dongsoo.han@amd.com>
04/11/2012 : Added contactData attribute and contact information code to enable/disable updating contactCount, contactName and 
			 contactPosition attributes in rigidBodyNode.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/dSolverNode.cpp

//
//  DCBullet2DSolver.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/14.
//

#include "DCBullet2DSolver.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "Solver/DBulletWorldFields.h"
#include "Solver/DBulletEvaluation.h"
#include "Solver/DBulletMayaState.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaValueValidation.h"
#include "DBulletInputBudget.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DNodeOperation.h"
#include "DDataOp.h"

#include "DCbtRigid2D.h"



//constraint
#include "DBPointConstraint.h"
#include "DBLinkConstraint.h"

#include "commonBulletRigidBody.h"

#include <cmath>
#include <memory>
#include <stdexcept>


//Bullet
#include "btBulletDynamicsCommon.h"

#include "LinearMath/btGeometryUtil.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"

#include "BulletCollision/CollisionShapes/btConvex2dShape.h"


#include "btConvexConvexMprAlgorithm.h"




#define DM_BULLET_2D_DEBUG DOMMY_ENABLE_DEBUG_LOGS









MTypeId DCBullet2DSolver::typeId( ID_DB_BULLET_2D_SOLVER );
const MString DCBullet2DSolver::typeName("DCBullet2DSolver");


MObject DCBullet2DSolver::inDC;
MObject DCBullet2DSolver::inKinematicDCs;
MObject DCBullet2DSolver::inConst;

MObject DCBullet2DSolver::aEnabled;
MObject DCBullet2DSolver::aCurrentTime;
MObject DCBullet2DSolver::aStartTime;


//solver attribute
MObject DCBullet2DSolver::aGravity;

MObject DCBullet2DSolver::aDisableCollisionsBetweenLinkedBodies;
MObject DCBullet2DSolver::aSplitImpulse;
MObject DCBullet2DSolver::aSubsteps;
MObject DCBullet2DSolver::aFixedPhysicsRate;


MObject DCBullet2DSolver::aContactData;

//plane attribute
MObject DCBullet2DSolver::aIsPlane;
MObject DCBullet2DSolver::aPlanePosition;
MObject DCBullet2DSolver::aPlaneAxis;
MObject DCBullet2DSolver::aPlaneRestitution;
MObject DCBullet2DSolver::aPlaneFriction;

//field attribute
MObject DCBullet2DSolver::aAffectMayaFiels;




//Solver Settings


MObject DCBullet2DSolver::ia_DBG_DrawWireframe;
MObject DCBullet2DSolver::ia_DBG_DrawAabb;
MObject DCBullet2DSolver::ia_DBG_DrawFeaturesText;
MObject DCBullet2DSolver::ia_DBG_DrawContactPoints;
MObject DCBullet2DSolver::ia_DBG_NoDeactivation;
MObject DCBullet2DSolver::ia_DBG_NoHelpText;
MObject DCBullet2DSolver::ia_DBG_DrawText;
MObject DCBullet2DSolver::ia_DBG_ProfileTimings;
MObject DCBullet2DSolver::ia_DBG_EnableSatComparison;
MObject DCBullet2DSolver::ia_DBG_DisableBulletLCP;
MObject DCBullet2DSolver::ia_DBG_EnableCCD;
MObject DCBullet2DSolver::ia_DBG_DrawConstraints;
MObject DCBullet2DSolver::ia_DBG_DrawConstraintLimits;
MObject DCBullet2DSolver::ia_DBG_FastWireframe;

MObject DCBullet2DSolver::outDC;



DCBullet2DSolver::DCBullet2DSolver()
    : m_reInitialize(false)
{
}



DCBullet2DSolver::~DCBullet2DSolver()
{
    clearBullet();
    deleteRigidBodies();
}




void *DCBullet2DSolver::creator() {
    return ::dommy::maya::createObject("DCBullet2DSolver", [&]() {
        return new DCBullet2DSolver();
    });
}

void DCBullet2DSolver::postConstructor()
{
    //prevent deletion if all the rigidbodies are deleted
    setExistWithoutOutConnections(true);
}

MPxNode::SchedulingType DCBullet2DSolver::schedulingType() const
{
    return MPxNode::kUntrusted;
}

bool DCBullet2DSolver::setInternalValueInContext(
    const MPlug&, const MDataHandle&, MDGContext&)
{
    return false;
}

static int getDbgDrawVal(const MObject& thisObj, const MObject& attr, int flag)
{
    MPlug plug(thisObj, attr);
    bool retVal = false;
    if (readPlugValue(plug, retVal) != MS::kSuccess) {
        return 0;
    }
    return retVal ? (1 << flag) : 0;
}

void DCBullet2DSolver::draw( M3dView & view, const MDagPath&,
                        M3dView::DisplayStyle style,
                        M3dView::DisplayStatus status ) try
{
    if(!m_dynamicsWorld) return;
    
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    
    if( !(status == M3dView::kActive ||
          status == M3dView::kLead ||
          status == M3dView::kHilite ||
          ( style != M3dView::kGouraudShaded && style != M3dView::kFlatShaded )) ) {
        glColor3f(1.0, 1.0, 0.0);
    }
    MObject thisObject = thisMObject();

    int dbgMode = 0;
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawWireframe, 0);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawAabb, 1);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawFeaturesText, 2);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawContactPoints, 3);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_NoDeactivation, 4);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_NoHelpText, 5);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawText, 6);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_ProfileTimings, 7);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_EnableSatComparison, 8);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DisableBulletLCP, 9);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_EnableCCD, 10);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawConstraints, 11);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_DrawConstraintLimits, 12);
    dbgMode |= getDbgDrawVal(thisObject,ia_DBG_FastWireframe, 13);
    
    m_dynamicsWorld->getDebugDrawer()->setDebugMode(dbgMode);
    m_dynamicsWorld->debugDrawWorld();
    
}
DOMMY_DRAW_CATCH("DCBullet2DSolver")



// field method
MStatus DCBullet2DSolver::applyFields(MDataBlock& data, float dt)
{
    (void)data;
    dommy::bullet::SoftBodyForces unused;
    return dommy::bullet::applyWorldFields(m_dynamicsWorld.get(), dt, unused);
}


// mesh method
MStatus DCBullet2DSolver::initRigidBodies() {
    //iniit rigid body
    DBt2DSolverOptionParams option;
    option.m_solver_proc_type = DB_SLV_2D_INIT_RB;
    
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDC, option));
    
    
    //iniit kinematic body
    option.m_solver_proc_type = DB_SLV_2D_INIT_KINEMA;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDCs, option));
    
    
    //init constraint
    option.m_solver_proc_type = DB_SLV_2D_INIT_CONST;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inConst, option));
    
    return MS::kSuccess;
}

MStatus DCBullet2DSolver::updateRigidBodies(
    DCurveDataPtrList* outputs, UuidList* nodeIds)
{
    if (!m_dynamicsWorld || !outputs || !nodeIds || outputs->size() != nodeIds->size())
        return MS::kInvalidParameter;
    return VisitBulletRigidGeometry(*m_dynamicsWorld,
        [&](const customDBData& metadata, const btTransform& transform) -> MStatus {
            const auto found = std::find(nodeIds->begin(), nodeIds->end(), metadata.nodeUuid);
            if (found == nodeIds->end()) return MS::kSuccess; // Another output connection.
            const std::size_t slot = std::distance(nodeIds->begin(), found);
            const auto data = (*outputs)[slot];
            if (!data || metadata.index < 0 ||
                std::size_t(metadata.index) >= data->numberOfCurves()) return MS::kInvalidParameter;
            const auto item = data->getCurvePtr(metadata.index);
            if (!item) return MS::kInvalidParameter;
            MMatrix matrix;
            DOMMY_RETURN_IF_FAILURE(UpdateBulletGeometryTransform(
                transform, item->getTransformMatrix(), matrix));
            item->setTransformMatrix(matrix);
            return MS::kSuccess;
        });
}


MStatus DCBullet2DSolver::solverProcessFromPlug(MObject targetPlugObj, DBt2DSolverOptionParams option) {
    
    
    MPlug arrayPlug = MPlug(thisMObject(), targetPlugObj);
    const bool detaching = option.m_solver_proc_type == DB_SLV_2D_DETACH_WORLD_RESOURCES;
    MStatus status;
    const unsigned int count = arrayPlug.numConnectedElements(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", arrayPlug.name().asChar() << ": process");
    
    //Plug array base
    for (unsigned int i=0; i<count; ++i) {
        MPlug plg = arrayPlug.connectionByPhysicalIndex(i, &status);
        if (!status) { if (detaching) continue; return status; }
        
        MPlugArray dmConnections;
        plg.connectedTo(dmConnections, true, false, &status);
        if (!status) { if (detaching) continue; return status; }
        
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", plg.name().asChar() << ":connected count:" << dmConnections.length());
        for(size_t j = 0; j < dmConnections.length(); ++j) {
            MObject node = dmConnections[j].node(&status);
            if (!status) { if (detaching) continue; return status; }
            MFnDependencyNode fnNode(node, &status);
            if (!status) { if (detaching) continue; return status; }
            const MTypeId nodeType = fnNode.typeId(&status);
            if (!status) { if (detaching) continue; return status; }
            
            if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", j << ":" << fnNode.name().asChar());
            
            
            if(nodeType == DCbtRigid2D::typeId) {
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "DCbtRigid2D node:");
                
                DCbtRigid2D *dmRbNode =
                    dommy::maya::checkedUserNode<DCbtRigid2D>(
                        fnNode, DCbtRigid2D::typeId);
                if (!dmRbNode) { if (detaching) continue; return MS::kFailure; }
                
                switch (option.m_solver_proc_type) {
                    case DB_SLV_2D_INIT_RB:
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(true));
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->resetRigidBodies(
                            m_dynamicsWorld.get(), m_worldReference));
                        break;
                    case DB_SLV_2D_INIT_KINEMA:
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(true, DBulletBodyMode::Kinematic));
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->resetRigidBodiesKinematic(
                            m_dynamicsWorld.get(), m_worldReference));
                        break;
                    case DB_SLV_2D_UPDATE_RB:
                    case DB_SLV_2D_UPDATE_KINEMA:
                    {
                        const DBulletBodyMode mode = option.m_solver_proc_type == DB_SLV_2D_UPDATE_KINEMA ?
                            DBulletBodyMode::Kinematic : DBulletBodyMode::Dynamic;
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(false, mode));
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->updateRigidBodies(m_dynamicsWorld.get(), mode));
                        break;
                    }
                    case DB_SLV_2D_UPDATE_SHAPE_RB:
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->updateCollisionShape(m_dynamicsWorld.get()));
                        break;
                    case DB_SLV_2D_TRANS_KINEMA:
                    {
                        DOMMY_RETURN_IF_FAILURE(dmRbNode->updateRigidBodiesKinematic(
                            m_dynamicsWorld.get(), option.m_timestep));
                        
                    }
                        break;
                    case DB_SLV_2D_DETACH_WORLD_RESOURCES:
                        dmRbNode->detachFromWorld();
                        break;
                    default:
                        break;
                }
                
            } else if (nodeType == DBPointConstraint::typeId) {
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "DBPointConstraint node:");
                switch (option.m_solver_proc_type) {
                    case DB_SLV_2D_INIT_CONST:
                    case DB_SLV_2D_UPDATE_CONST:
                        // The 2D solver does not create the legacy 3D
                        // point-constraint implementation.
                        break;
                    case DB_SLV_2D_DETACH_WORLD_RESOURCES:
                    {
                        DBPointConstraint *cNode =
                            dommy::maya::checkedUserNode<DBPointConstraint>(
                                fnNode, DBPointConstraint::typeId);
                        if (cNode) cNode->detachFromWorld();
                    }
                        break;
                    default:
                        break;
                }
                
                
            } else if (nodeType == DBLinkConstraint::typeId) {
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "DBLinkConstraint node:");
                switch (option.m_solver_proc_type) {
                    case DB_SLV_2D_INIT_CONST:
                    case DB_SLV_2D_UPDATE_CONST:
                        // The 2D solver does not create the legacy 3D
                        // link-constraint implementation.
                        break;
                    case DB_SLV_2D_DETACH_WORLD_RESOURCES:
                    {
                        DBLinkConstraint *cNode =
                            dommy::maya::checkedUserNode<DBLinkConstraint>(
                                fnNode, DBLinkConstraint::typeId);
                        if (cNode) cNode->detachFromWorld();
                    }
                        break;
                    default:
                        break;
                }
                
                
            }
        }
    }
    
    return MS::kSuccess;
}


MStatus DCBullet2DSolver::checkRigidBodies() {
    
    //Plug array base
    DBt2DSolverOptionParams option;
    option.m_solver_proc_type = DB_SLV_2D_UPDATE_RB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDC, option));
    
    
    option.m_solver_proc_type = DB_SLV_2D_UPDATE_KINEMA;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDCs, option));
    
    
    //update Shape
    option.m_solver_proc_type = DB_SLV_2D_UPDATE_SHAPE_RB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDC, option));
    
    return MS::kSuccess;
}



// Init solver

void DCBullet2DSolver::initBullet(MDataBlock& data) {
    if (m_collisionConfiguration || m_dispatcher || m_simplexSolver ||
        m_pdSolver || m_convexAlgo2d || m_box2dbox2dAlgo || m_broadphase ||
        m_solver || m_dynamicsWorld || m_worldReference || m_plane_rb ||
        m_plane_col_shape) {
        throw std::logic_error(
            "DCBullet2DSolver: Bullet resources must be cleared before initialization.");
    }

    const auto inputValue = [&](const MObject& attribute) -> MDataHandle {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) {
            throw std::runtime_error(
                "DCBullet2DSolver: failed to read a Bullet input value.");
        }
        return handle;
    };

    const MVector gravity = inputValue(aGravity).asVector();
    if (!dommy::maya::isFloatRepresentableVector(gravity)) {
        throw std::invalid_argument(
            "DCBullet2DSolver: gravity must contain finite Bullet scalar values.");
    }

    const bool isPlane = inputValue(aIsPlane).asBool();
    MVector planePosition;
    MVector planeAxis;
    double planeRestitution = 0.0;
    double planeFriction = 0.0;
    if (isPlane) {
        planePosition = inputValue(aPlanePosition).asVector();
        const MVector rawPlaneAxis = inputValue(aPlaneAxis).asVector();
        planeRestitution = inputValue(aPlaneRestitution).asDouble();
        planeFriction = inputValue(aPlaneFriction).asDouble();

        if (!dommy::maya::isFloatRepresentableVector(planePosition) ||
            !dommy::maya::normalizedVector(rawPlaneAxis, planeAxis) ||
            !dommy::numeric::canRepresentAsFloat(planeRestitution) ||
            !dommy::numeric::canRepresentAsFloat(planeFriction)) {
            throw std::invalid_argument(
                "DCBullet2DSolver: plane values must be finite and the axis must be non-zero.");
        }
    }

    // Keep every allocation locally owned until the complete world is valid.
    // If construction throws, reverse-order unique_ptr destruction leaves the
    // node in its original empty state.
    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration(
        new btDefaultCollisionConfiguration());
    std::unique_ptr<btCollisionDispatcher> dispatcher(
        new btCollisionDispatcher(collisionConfiguration.get()));
    std::unique_ptr<btVoronoiSimplexSolver> simplexSolver(
        new btVoronoiSimplexSolver());
    std::unique_ptr<btMinkowskiPenetrationDepthSolver> penetrationDepthSolver(
        new btMinkowskiPenetrationDepthSolver());
    std::unique_ptr<btConvex2dConvex2dAlgorithm::CreateFunc> convexAlgorithm(
        new btConvex2dConvex2dAlgorithm::CreateFunc(
            simplexSolver.get(), penetrationDepthSolver.get()));
    std::unique_ptr<btBox2dBox2dCollisionAlgorithm::CreateFunc> boxAlgorithm(
        new btBox2dBox2dCollisionAlgorithm::CreateFunc());

    dispatcher->registerCollisionCreateFunc(
        CONVEX_2D_SHAPE_PROXYTYPE, CONVEX_2D_SHAPE_PROXYTYPE,
        convexAlgorithm.get());
    dispatcher->registerCollisionCreateFunc(
        BOX_2D_SHAPE_PROXYTYPE, CONVEX_2D_SHAPE_PROXYTYPE,
        convexAlgorithm.get());
    dispatcher->registerCollisionCreateFunc(
        CONVEX_2D_SHAPE_PROXYTYPE, BOX_2D_SHAPE_PROXYTYPE,
        convexAlgorithm.get());
    dispatcher->registerCollisionCreateFunc(
        BOX_2D_SHAPE_PROXYTYPE, BOX_2D_SHAPE_PROXYTYPE,
        boxAlgorithm.get());

    std::unique_ptr<btBroadphaseInterface> broadphase(new btDbvtBroadphase());
    std::unique_ptr<btConstraintSolver> solver(
        new btSequentialImpulseConstraintSolver());
    std::unique_ptr<dommy::bullet::DBoundedRigidDynamicsWorld> dynamicsWorld(
        new dommy::bullet::DBoundedRigidDynamicsWorld(
            dispatcher.get(), broadphase.get(), solver.get(),
            collisionConfiguration.get()));
    DBulletWorldReferencePtr worldReference(
        new DBulletWorldReference(dynamicsWorld.get()));
    dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
    dynamicsWorld->getDispatchInfo().m_enableSPU = true;
    
    
    std::unique_ptr<dm_debug_draw> debugDrawer(new dm_debug_draw());
    dynamicsWorld->setDebugDrawer(debugDrawer.get());

    std::unique_ptr<btCollisionShape> planeShape;
    DBulletRigidBodyPtr planeBody;
    if (isPlane) {
        planeShape.reset(new btStaticPlaneShape(
            btVector3(planeAxis.x, planeAxis.y, planeAxis.z), 0.0));
        btTransform groundTransform;
        groundTransform.setIdentity();
        groundTransform.setOrigin(btVector3(
            planePosition.x, planePosition.y, planePosition.z));

        std::unique_ptr<btMotionState> planeMotionState(
            new btDefaultMotionState(groundTransform));
        btRigidBody::btRigidBodyConstructionInfo bodyInfo(
            0.0, planeMotionState.get(), planeShape.get(), btVector3(0, 0, 0));
        bodyInfo.m_restitution = static_cast<btScalar>(planeRestitution);
        bodyInfo.m_friction = static_cast<btScalar>(planeFriction);
        planeBody = MakeBulletRigidBody(
            std::move(planeMotionState), bodyInfo);
        dynamicsWorld->addRigidBody(planeBody.get());
    }

    // Commit only after every allocation and configuration step succeeded.
    m_collisionConfiguration = std::move(collisionConfiguration);
    m_dispatcher = std::move(dispatcher);
    m_simplexSolver = std::move(simplexSolver);
    m_pdSolver = std::move(penetrationDepthSolver);
    m_convexAlgo2d = std::move(convexAlgorithm);
    m_box2dbox2dAlgo = std::move(boxAlgorithm);
    m_broadphase = std::move(broadphase);
    m_solver = std::move(solver);
    m_worldReference = worldReference;
    m_debugDrawer = std::move(debugDrawer);
    m_plane_col_shape = std::move(planeShape);
    m_plane_rb = std::move(planeBody);
    m_dynamicsWorld = std::move(dynamicsWorld);
}


void DCBullet2DSolver::clearBullet() noexcept {
    if (m_dynamicsWorld) {
        DBt2DSolverOptionParams detachResources;
        detachResources.m_solver_proc_type =
            DB_SLV_2D_DETACH_WORLD_RESOURCES;
        const auto detachConnectedNodes = [&](const MObject& attribute) {
            try {
                (void)solverProcessFromPlug(attribute, detachResources); // Best-effort teardown.
            } catch (...) {
                // Continue with the remaining connections when Maya has
                // already removed one node during graph teardown.
            }
        };
        detachConnectedNodes(inConst);
        detachConnectedNodes(inDC);
        detachConnectedNodes(inKinematicDCs);

        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "delete rigidbodies from the dynamics world:" << m_dynamicsWorld->getNumCollisionObjects());
        // Remove objects owned by Maya nodes before destroying the world.
        for (int i=m_dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
        {
            btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
            if (obj) {
                m_dynamicsWorld->removeCollisionObject( obj );
            }
        }
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "delete constraint from the dynamics world");
        while (m_dynamicsWorld->getNumConstraints())
        {
            m_dynamicsWorld->removeConstraint(m_dynamicsWorld->getConstraint(0));
        }

        m_dynamicsWorld->setDebugDrawer(nullptr);
        if (m_worldReference) {
            m_worldReference->invalidate();
        }
        m_dynamicsWorld.reset();
        m_worldReference.reset();
    }

    if (m_worldReference) {
        m_worldReference->invalidate();
        m_worldReference.reset();
    }

    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "clear all");

    m_debugDrawer.reset();
    m_dispatcher.reset();
    m_broadphase.reset();
    m_solver.reset();
    m_convexAlgo2d.reset();
    m_box2dbox2dAlgo.reset();
    m_simplexSolver.reset();
    m_pdSolver.reset();
    m_collisionConfiguration.reset();

    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "all deleted");
}




void DCBullet2DSolver::deleteRigidBodies() noexcept
{
    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "Deleting rigidbodies");
    
    m_plane_rb.reset();
    m_plane_col_shape.reset();
    
}


// -----------------------------------------------------------------------------
// Main compute
MStatus DCBullet2DSolver::compute( const MPlug& plug, MDataBlock& data ) try
{
    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "Calling DMBulletTest::compute \n");
    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "Plug: " << plug.name().asChar());
    
    if(plug == outDC) {
        
        
        MStatus stat;
        dommy::geometry::GeometryBatchBudget inputBudget;
        dommy::bullet::BulletWorldMutationGuard mutationGuard(m_requiresStartFrame);
        MTime candidateTime = m_prevTime;
        std::vector<double> candidateSettings;
        
// get inputDCs and outputDC        
        MArrayDataHandle arrayHandle = data.inputArrayValue(inDC, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        MFnDependencyNode nodeFn(thisMObject(), &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MPlug inDCArrayPlug = nodeFn.findPlug(inDC, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int curveInputCount = inDCArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (curveInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "inDM array plug: " << inDCArrayPlug.isArray());
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "connected curve inputs: " << curveInputCount);
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "Object Type num is " << arrayHandle.elementCount());
        
        
        DCurveDataPtrList outDCurveList;
        UuidList nodeIdList;
        std::vector<MObject> objArray; // Own copied data until publication completes.
        
        //Plug array base
        for (unsigned int i = 0; i < curveInputCount; ++i) {
            MPlug plg = inDCArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "plug name:" << plg.name().asChar());
            if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "plug connected:" << inputConnected);
            
            
            if(inputConnected) {
                
                
                MPlugArray dcConnections;
                const bool connected = plg.connectedTo(
                    dcConnections, true, true, &stat);
                if (!stat) return stat;
                if (!connected || dcConnections.length() == 0U) {
                    continue;
                }
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", plg.name().asChar() << ":connected count:" << dcConnections.length());
                
                MObject sourceNode = dcConnections[0].node(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                MFnDependencyNode fnNode(sourceNode, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MUuid cuuid = fnNode.uuid(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!cuuid.valid()) return MS::kInvalidParameter;
                
                
                MObject obj;
                const DCurveData* inputDCData = nullptr;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
                    plg, DCurveData::id, obj, inputDCData));
                if (!inputDCData) return MS::kFailure;
                if (!dommy::maya::validatePluginData(*inputDCData))
                    return MS::kInvalidParameter;
                if (!dommy::bullet::admitCurveInput(*inputDCData, inputBudget))
                    return MS::kInvalidParameter;
                
                
                
                if (inputDCData) {
                    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "inputDCData num is " << inputDCData->numberOfCurves());
                    
                    MFnPluginData pluginData;
                    MObject newObj = pluginData.create(DCurveData::id, &stat);
                    
                    if (!stat) return stat;
                    
                    DCurveData* cache = nullptr;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginData(pluginData, cache));
                    if (!cache) return MS::kFailure;

                    DOMMY_RETURN_IF_FAILURE(cache->copyFrom(*inputDCData));
                    objArray.push_back(newObj);

                    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "cache num is " << cache->numberOfCurves());

                    outDCurveList.push_back(cache);
                    nodeIdList.push_back(cuuid);
                }
                
                //                }
            }
            
        }
        
// for kinema        //for update
        MArrayDataHandle kinemaArrayHandle = data.inputArrayValue(inKinematicDCs, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        unsigned int kinemaCount = kinemaArrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "kinemaCount:" << kinemaCount);
        MPlug inKinemaDCArrayPlug = nodeFn.findPlug(inKinematicDCs, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int kinematicInputCount = inKinemaDCArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (kinematicInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        for (unsigned int i = 0; i < kinematicInputCount; ++i) {
            MPlug plg = inKinemaDCArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "plug name:" << plg.name().asChar());
            if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "plug connected:" << inputConnected);
            
            if(inputConnected) {
                
                MObject obj;
                const DCurveData* inputDCData = nullptr;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
                    plg, DCurveData::id, obj, inputDCData));
                if (!inputDCData) return MS::kFailure;
                if (!dommy::maya::validatePluginData(*inputDCData))
                    return MS::kInvalidParameter;
                if (!dommy::bullet::admitCurveInput(*inputDCData, inputBudget, false))
                    return MS::kInvalidParameter;
            }
        }
        
        
        MArrayDataHandle inConstHandle = data.inputArrayValue(inConst, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        size_t constCount = inConstHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "outDCurveList num is " << outDCurveList.size());
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "objArray num is " << objArray.size());
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "nodeIdList num is " << nodeIdList.size());
        if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "constCount is " << constCount);
        
        
        const MDataHandle enabledInput = data.inputValue(aEnabled, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        bool enabled = enabledInput.asBool();
        
// Simulation
        if(enabled) {
            
            //Get time
            const MDataHandle timeInput = data.inputValue(aCurrentTime, &stat);
            if (!stat) return stat;
            const MTime time = timeInput.asTime();
            const MDataHandle startTimeInput = data.inputValue(aStartTime, &stat);
            if (!stat) return stat;
            const MTime startTime = startTimeInput.asTime();
            const MDataHandle substepsInput = data.inputValue(aSubsteps, &stat);
            if (!stat) return stat;
            const int subSteps = substepsInput.asInt();
            const MDataHandle fixedRateInput = data.inputValue(aFixedPhysicsRate, &stat);
            if (!stat) return stat;
            const int fixedPhysicsFrameHertz = fixedRateInput.asInt();
            if (!std::isfinite(time.value()) || !std::isfinite(startTime.value()))
                return MS::kInvalidParameter;
            if (!dommy::bullet::validSimulationSettings(subSteps, fixedPhysicsFrameHertz))
                return MS::kInvalidParameter;
            
            const MDataHandle gravityInput = data.inputValue(aGravity, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            MVector gravity = gravityInput.asVector();
            if (!dommy::maya::isFloatRepresentableVector(gravity))
                return MS::kInvalidParameter;
            
            const MDataHandle splitImpulseInput = data.inputValue(aSplitImpulse, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            bool splitImpulseEnabled = splitImpulseInput.asBool();
            const MDataHandle mayaFieldInput = data.inputValue(aAffectMayaFiels, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool affectMayaField = mayaFieldInput.asBool();
            
            
            const bool playback = MConditionMessage::getConditionState("playingBack", &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            DOMMY_RETURN_IF_FAILURE(dommy::bullet::readPlaneRestartSettings(
                data, startTime.as(MTime::kSeconds),
                {aIsPlane, aPlanePosition, aPlaneAxis, aPlaneRestitution, aPlaneFriction},
                candidateSettings));
            if (!(time == startTime) && candidateSettings != m_worldSettings)
                m_requiresStartFrame = true;
            const dommy::bullet::TimelineAction action = dommy::bullet::timelineAction(
                !m_requiresStartFrame && bool(m_dynamicsWorld), time == startTime,
                (time - m_prevTime).value(), playback);
            if (action == dommy::bullet::TimelineAction::RestartRequired) {
                m_requiresStartFrame = true;
                EchoWarning(name().asChar(), "Restart simulation from StartTime after a time jump, world-setting change or failed evaluation.");
                return MS::kFailure;
            }
            candidateTime = time;

            if (action == dommy::bullet::TimelineAction::Initialize) {
                mutationGuard.arm();
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "first frame, init the simulation");
                
                //need to re-construct the world and re-insert all objects...
                
                
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "clearBullet");
                clearBullet();
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "deleteRigidBodies");
                deleteRigidBodies();
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "initBullet");
                initBullet(data);
                
                
                if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "initRigidBodies");
                DOMMY_RETURN_IF_FAILURE(initRigidBodies());
                
                
                
                m_reInitialize = true;
                m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulseEnabled;
                
            } else {
                
                if(m_dynamicsWorld) {
                    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", time.value());
                    if(DM_BULLET_2D_DEBUG)DOMMY_MAYA_DEBUG("DCBullet2DSolver", "dynamics world collision num:" << m_dynamicsWorld->getNumCollisionObjects());
                    if (action == dommy::bullet::TimelineAction::Advance) {
                        // Advance only an accepted forward interval.
                        
                        dommy::bullet::SimulationStepPlan step;
                        if (!dommy::bullet::prepareSimulationStep(
                                (time - m_prevTime).as(MTime::kSeconds), subSteps,
                                fixedPhysicsFrameHertz, step) ||
                            !m_dynamicsWorld->canAdvance(step.seconds, step.fixedSeconds, step.substeps))
                            return MS::kInvalidParameter;
                        
                        mutationGuard.arm();
                        //update solver
                        m_dynamicsWorld->setGravity(btVector3(gravity[0], gravity[1], gravity[2]));
                        m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulseEnabled;
                        // Prepare additions, empty inputs and current transforms before stepping.
                        DOMMY_RETURN_IF_FAILURE(checkRigidBodies());
                        
                        DBt2DSolverOptionParams option,optionConst;
                        option.m_solver_proc_type = DB_SLV_2D_TRANS_KINEMA;
                        optionConst.m_solver_proc_type = DB_SLV_2D_UPDATE_CONST;
                        
                        
                        float kinemaStep = 1.0 / (float)subSteps;
                        if (subSteps==1) kinemaStep = 1.0;
                        
                        
                        
                        for (int i = 1; i <= subSteps; ++i)
                        {
                            //update kinematice body　サブステップごとにトランスフォームをアプデートする
                            option.m_timestep = optionConst.m_timestep = (float)i * kinemaStep;
                            if(kinemaCount>0) DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDCs, option));
                            
                            //update Constraint
                            if(constCount>0) DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inConst, optionConst));
                            
                            //fields
                            if (affectMayaField) DOMMY_RETURN_IF_FAILURE(applyFields(data, step.seconds));
                            
                            
                            if (!m_dynamicsWorld->stepChecked(step.seconds, step.fixedSeconds))
                                return MS::kInvalidParameter;
                            
                        }
                        
                    }
                    
                    //update positions of all objects
                    if (outDCurveList.size()) {
                        DOMMY_RETURN_IF_FAILURE(updateRigidBodies(&outDCurveList, &nodeIdList));
                    }
                    
                    
                } else {
                    return MS::kFailure;
                    
                }
                
            }//step sim
            
            
        }//calc sim
        
        
        
        
        
// Publish the copied Maya-owned objects; no timestamp commit on failure.
        for (auto* curve : outDCurveList) curve->updateBBox();
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput(
            data, outDC, plug, objArray));
        if (enabled) {
            m_prevTime = candidateTime;
            m_worldSettings.swap(candidateSettings);
            m_requiresStartFrame = false;
        }
        mutationGuard.commit();
        return MS::kSuccess;
        
    }
    
    return MS::kUnknownParameter;
    
}
DOMMY_COMPUTE_CATCH("DCBullet2DSolver")




MStatus DCBullet2DSolver::initialize() try {
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnMatrixAttribute matAttr;
    
    MStatus stat;
    
    
    
    // ----------------------- INPUTS --------------------------
    
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    inKinematicDCs = tAttr.create("inKinematicDCs", "iknmdcs", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inKinematicDCs));
    
    MFnMessageAttribute fnMsgAttr;
    inConst = fnMsgAttr.create("inConst", "incnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(inConst));
    
    
    
    
    aContactData = nAttr.create("contactData", "contactData", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aContactData));
    
    
    
    
    
    
    
    aEnabled = nAttr.create("enabled", "enbl", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnabled));
    
    
    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create( "CurrentTime", "ctm" , MFnUnitAttribute::kTime,  0.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));
    
    
    aStartTime = uniAttr.create( "StartTime", "stm" , MFnUnitAttribute::kTime, 1.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartTime));
    
    
// Bullet solver
    aGravity = nAttr.create("Gravity", "grvt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, -9.81, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravity));

    aSplitImpulse = nAttr.create("SplitImpulse", "spli", MFnNumericData::kBoolean,false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSplitImpulse));
    
    
    aDisableCollisionsBetweenLinkedBodies = nAttr.create("DisableCollisionsBetweenLinkedBodies", "dCol", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisableCollisionsBetweenLinkedBodies));
    
    
    
    
    aSubsteps = nAttr.create("substeps", "sbs", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));
    
    
    aFixedPhysicsRate = nAttr.create("FixedPhysicsRate", "fps", MFnNumericData::kInt, 60, &stat); //MB
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(60));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(6000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFixedPhysicsRate));
    
    
// plane attribute    
    aIsPlane = nAttr.create("IsPlane", "ispln", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsPlane));
    
    aPlanePosition = nAttr.create("PlanePosition", "plnpos", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlanePosition));
    
    aPlaneAxis = nAttr.create("PlaneAxis", "plnaxs", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlaneAxis));
    
    
    aPlaneRestitution = nAttr.create("PlaneRestitution", "plnrst", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlaneRestitution));
    
    aPlaneFriction = nAttr.create("PlaneFriction", "plnfc", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlaneFriction));
    
// Field attributes
    aAffectMayaFiels = nAttr.create("AffectMayaFiels", "affmaflds", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectMayaFiels));
    
    
    
    
    
// display attribute    
    ia_DBG_DrawWireframe = nAttr.create("drawWireframe", "dwfr", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_DrawWireframe));
    
    ia_DBG_DrawAabb = nAttr.create("drawAabb", "daabb", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_DrawAabb));
    
    ia_DBG_DrawContactPoints = nAttr.create("drawContactPoints", "dcp", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_DrawContactPoints));
    
    ia_DBG_NoDeactivation = nAttr.create("noDeactivation", "dnda", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_NoDeactivation));
    
    ia_DBG_ProfileTimings = nAttr.create("pProfileTimings", "dptm", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_ProfileTimings));
    
    ia_DBG_DrawConstraints = nAttr.create("drawConstraints", "dcnst", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_DrawConstraints));
    
    ia_DBG_DrawConstraintLimits = nAttr.create("drawConstraintLimits", "dcsl", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_DBG_DrawConstraintLimits));
    
    
    
// Outputs
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    const MObject simulationInputs[] = {
        inDC, inKinematicDCs, inConst, aCurrentTime, aEnabled, aStartTime,
        aGravity, aSplitImpulse, aSubsteps, aFixedPhysicsRate, aAffectMayaFiels,
        aIsPlane, aPlanePosition, aPlaneAxis, aPlaneRestitution, aPlaneFriction
    };
    for (const MObject& input : simulationInputs)
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outDC));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCBullet2DSolver", "initialize", std::current_exception());
}
