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
//  DMBulletTest.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/02.
//

#include "DMBulletTest.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaValueValidation.h"
#include "DBulletInputBudget.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "Solver/DBulletWorldFields.h"
#include "Solver/DBulletEvaluation.h"
#include "Solver/DBulletMayaState.h"
#include "common/utility/DGenerationLimits.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"


#include "DMbtRigid.h"
#include "DMbtFracture.h"
#include "DMbtSoft.h"
#include "DCbtRope.h"

//constraint
#include "DBPointConstraint.h"
#include "DBLinkConstraint.h"

#include "commonBulletRigidBody.h"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>


//Bullet
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "LinearMath/btGeometryUtil.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"

#include "btConvexConvexMprAlgorithm.h"






float DMBulletTest::collisionMarginOffset; //mb








MTypeId DMBulletTest::typeId( ID_DB_BULLET_TEST );
const MString DMBulletTest::typeName("DMBulletTest");


MObject DMBulletTest::inDM;
MObject DMBulletTest::inKinematicDMs;
MObject DMBulletTest::inSoftDM;
MObject DMBulletTest::inRopeDC;

MObject DMBulletTest::inConst;

MObject DMBulletTest::aEnabled;
MObject DMBulletTest::aCurrentTime;
MObject DMBulletTest::aStartTime;


//solver attribute
MObject DMBulletTest::aGravity;
MObject DMBulletTest::aAirDensity;
MObject DMBulletTest::aWaterDensity;
MObject DMBulletTest::aWaterOffset;
MObject DMBulletTest::aWaterNormal;
MObject DMBulletTest::aMaxDisplacement;

MObject DMBulletTest::aCollisionMargin;
MObject DMBulletTest::aDisableCollisionsBetweenLinkedBodies;
MObject DMBulletTest::aSplitImpulse;
MObject DMBulletTest::aSubsteps;
MObject DMBulletTest::aFixedPhysicsRate;


MObject DMBulletTest::aContactData;

//plane attribute
MObject DMBulletTest::aIsPlane;
MObject DMBulletTest::aPlanePosition;
MObject DMBulletTest::aPlaneAxis;
MObject DMBulletTest::aPlaneRestitution;
MObject DMBulletTest::aPlaneFriction;

//field attribute
MObject DMBulletTest::aAffectMayaFiels;

//problem
MObject DMBulletTest::aIsCheckAndDeletePanicSoftBody;

//Solver Settings


MObject DMBulletTest::ia_DBG_DrawWireframe;
MObject DMBulletTest::ia_DBG_DrawAabb;
MObject DMBulletTest::ia_DBG_DrawFeaturesText;
MObject DMBulletTest::ia_DBG_DrawContactPoints;
MObject DMBulletTest::ia_DBG_NoDeactivation;
MObject DMBulletTest::ia_DBG_NoHelpText;
MObject DMBulletTest::ia_DBG_DrawText;
MObject DMBulletTest::ia_DBG_ProfileTimings;
MObject DMBulletTest::ia_DBG_EnableSatComparison;
MObject DMBulletTest::ia_DBG_DisableBulletLCP;
MObject DMBulletTest::ia_DBG_EnableCCD;
MObject DMBulletTest::ia_DBG_DrawConstraints;
MObject DMBulletTest::ia_DBG_DrawConstraintLimits;
MObject DMBulletTest::ia_DBG_FastWireframe;


MObject DMBulletTest::outContactDP;

MObject DMBulletTest::outDM;
MObject DMBulletTest::outSoftDM;
MObject DMBulletTest::outRopeDC;

DMBulletTest::DMBulletTest()
: m_reInitialize(false)
{
}



DMBulletTest::~DMBulletTest()
{
    clearBullet();
    deleteRigidBodies();
}




void *DMBulletTest::creator() {
    return ::dommy::maya::createObject("DMBulletTest", [&]() {
        return new DMBulletTest();
    });
}

void DMBulletTest::postConstructor()
{
    //prevent deletion if all the rigidbodies are deleted
    setExistWithoutOutConnections(true);
}

MPxNode::SchedulingType DMBulletTest::schedulingType() const
{
    return MPxNode::kUntrusted;
}

bool DMBulletTest::setInternalValueInContext(
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

void DMBulletTest::draw( M3dView & view, const MDagPath&,
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
DOMMY_DRAW_CATCH("DMBulletTest")



// field method
MStatus DMBulletTest::applyFields(MDataBlock& data, float dt,
                                dommy::bullet::SoftBodyForces& forces)
{
    (void)data;
    return dommy::bullet::applyWorldFields(m_dynamicsWorld.get(), dt, forces);
}

// mesh method
MStatus DMBulletTest::initRigidBodies() {
    //iniit rigid body
    DBtSolverOptionParams option;
    option.m_solver_proc_type = DB_SLV_INIT_RB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDM, option));
    
    //iniit kinematic body
    option.m_solver_proc_type = DB_SLV_INIT_KINEMA;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDMs, option));
    
    //init soft body
    option.m_solver_proc_type = DB_SLV_INIT_SB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inSoftDM, option));
    
    //init rope
    option.m_solver_proc_type = DB_SLV_INIT_ROPE;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inRopeDC, option));
    
    
    
    //init constraint
    option.m_solver_proc_type = DB_SLV_INIT_CONST;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inConst, option));
    
    return MS::kSuccess;
}

MStatus DMBulletTest::updateRigidBodies(
    DMeshDataPtrList* outputs, UuidList* nodeIds)
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
                std::size_t(metadata.index) >= data->numberOfMesh()) return MS::kInvalidParameter;
            const auto item = data->getMeshPtr(metadata.index);
            if (!item) return MS::kInvalidParameter;
            MMatrix matrix;
            DOMMY_RETURN_IF_FAILURE(UpdateBulletGeometryTransform(
                transform, item->getTransformMatrix(), matrix));
            item->setTransformMatrix(matrix);
            return MS::kSuccess;
        });
}

void DMBulletTest::updateSoftBodies(
    DMeshDataPtrList* outDMeshList, UuidList* nodeIdList,
    std::vector<DPIdList>* panicIdList) {
    
    for (int j=m_dynamicsWorld->getNumCollisionObjects()-1; j>=0 ;j--)
    {
        btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[j];
        btSoftBody* body = btSoftBody::upcast(obj);
        if(body) {
            customDBData* cd =
                static_cast<customDBData*>(body->getUserPointer());
            if(cd) {
                
                auto iter = std::find(nodeIdList->begin(), nodeIdList->end(), cd->nodeUuid);
                size_t index = std::distance(nodeIdList->begin(), iter);
                if(index < nodeIdList->size() && index < outDMeshList->size())
                {
                    size_t meshCount = (*outDMeshList)[index]->numberOfMesh();
                    
                    if(cd->index >= 0 &&
                       static_cast<size_t>(cd->index) < meshCount) {
                        //update shape
                        
                        btSoftBody::tNodeArray &nodes = body->m_nodes;
                        DPointList pts(nodes.size());
                        
                        for(int i = 0; i < nodes.size(); i ++)
                        {
                            DPoint pt(nodes[i].m_x[0], nodes[i].m_x[1], nodes[i].m_x[2] );
                            pts[i] = pt;
                        }
                        if ((*outDMeshList)[index]->getMeshPtr(cd->index)->numberOfPoints() == pts.size()) {
                            //ポイントの座標がワールドに変換されているのでマトリックスを変換
                            (*outDMeshList)[index]->getMeshPtr(cd->index)->setPosition(pts);
                            (*outDMeshList)[index]->getMeshPtr(cd->index)->setIdentity();
                            
                            if (std::isnan(body->getVolume())) {
                                
                                (*panicIdList)[index][cd->index]=true;
                            }
                            
                            
                        }
                    }
                    
                }
                
                
            }
        }
    
    
    }
}


bool DMBulletTest::updateRopes(
    DCurveDataPtrList* outDCurveList, UuidList* nodeIdList) {
    if (!m_dynamicsWorld || !outDCurveList || !nodeIdList) {
        return false;
    }
    
    
    for (int j=m_dynamicsWorld->getNumCollisionObjects()-1; j>=0 ;j--)
    {
        btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[j];
        btCollisionShape* shape = obj->getCollisionShape();
        
        if(shape->getShapeType() == CAPSULE_SHAPE_PROXYTYPE){
            btRigidBody* body = btRigidBody::upcast(obj);
            if(body) {
                customDBData* cd =
                    static_cast<customDBData*>(body->getUserPointer());
                if(cd) {
                
                    auto iter = std::find(nodeIdList->begin(), nodeIdList->end(), cd->nodeUuid);
                    size_t index = std::distance(nodeIdList->begin(), iter);
                    
                    
                    //check node id
                    if(index < nodeIdList->size() && index < outDCurveList->size())
                    {
                        
                        size_t crvCount = (*outDCurveList)[index]->numberOfCurves();
                        
                        
                        //check curve index
                        if(cd->index >= 0 &&
                           static_cast<size_t>(cd->index) < crvCount) {
                            
                            
                            
                            const DCItem curve =
                                (*outDCurveList)[index]->getCurvePtr(
                                    cd->index);
                            if (!curve) {
                                return false;
                            }
                            size_t ptCount = curve->numberOfPoints();
                            
                            
                            //check point index
                            if (cd->ptIndex !=
                                    std::numeric_limits<size_t>::max() &&
                                cd->ptIndex < ptCount) {
                                
                                btTransform trans;
                                if (body && body->getMotionState())
                                {
                                    body->getMotionState()->getWorldTransform(trans);
                                    
                                } else {
                                    trans = obj->getWorldTransform();
                                }
                                
                                MMatrix mmat;
                                BMatToMMat(trans, mmat);
                                
                                MTransformationMatrix transMat(mmat);
                                double quartx,quarty,quartz,quartw;
                                transMat.getRotationQuaternion(quartx,quarty,quartz,quartw);
                                MQuaternion mquart(quartx,quarty,quartz,quartw);
                                
                                const btCapsuleShape* capShape = static_cast<const btCapsuleShape*>(shape);
                                btScalar halfHeight = capShape->getHalfHeight();
                                btScalar radius = capShape->getRadius();
                                
                                MVector upVec(0,1,0);
                                MVector lookVec = upVec.rotateBy(mquart);
                                
                                btVector3 pos = trans.getOrigin();//capsule center
                                
                                
                                MPoint rootPos = MPoint(pos[0],pos[1],pos[2])-lookVec*(halfHeight+radius);
                                
                                DPointList stagedPoints(
                                    curve->getPositionConst());
                                stagedPoints[cd->ptIndex] =
                                    DPoint(
                                        rootPos[0], rootPos[1],
                                        rootPos[2]);
                                
                                if(cd->ptIndex==ptCount-2) {
                                    //Last point process
                                    MPoint tipPos = MPoint(pos[0],pos[1],pos[2])+lookVec*(halfHeight+radius);
                                    stagedPoints[cd->ptIndex + 1U] =
                                        DPoint(
                                            tipPos[0], tipPos[1],
                                            tipPos[2]);
                                }
                                if (!curve->replacePositions(stagedPoints)) {
                                    return false;
                                }
                                
                            }//check point index
                            
                        }//check curve index
                        
                    }//check node id
                    
                }
                
            }//check rigid body.
        }//check capsule.
        
    }
    return true;
}






MStatus DMBulletTest::solverProcessFromPlug(MObject targetPlugObj, DBtSolverOptionParams option) {
    
    
    
        MPlug arrayPlug = MPlug(thisMObject(), targetPlugObj);
        const bool detaching = option.m_solver_proc_type == DB_SLV_DETACH_WORLD_RESOURCES;
        MStatus status;
        const unsigned int count = arrayPlug.numConnectedElements(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        
        
        //Plug array base
        for (unsigned int i=0; i<count; ++i) {
            MPlug plg = arrayPlug.connectionByPhysicalIndex(i, &status);
            if (!status) { if (detaching) continue; return status; }
            
            MPlugArray dmConnections;
            plg.connectedTo(dmConnections, true, false, &status);
            if (!status) { if (detaching) continue; return status; }
            
            
            for(size_t j = 0; j < dmConnections.length(); ++j) {
                MObject node = dmConnections[j].node(&status);
                if (!status) { if (detaching) continue; return status; }
                MFnDependencyNode fnNode(node, &status);
                if (!status) { if (detaching) continue; return status; }
                const MTypeId nodeType = fnNode.typeId(&status);
                if (!status) { if (detaching) continue; return status; }
                
                
                
                
                if(nodeType == DMbtRigid::typeId) {
                    
                    
                    DMbtRigid *dmRbNode =
                        dommy::maya::checkedUserNode<DMbtRigid>(
                            fnNode, DMbtRigid::typeId);
                    if (!dmRbNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_RB:
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(true));
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->resetRigidBodies(
                                m_dynamicsWorld.get(), m_worldReference));
                            break;
                        case DB_SLV_INIT_KINEMA:
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(true));
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->resetRigidBodiesKinematic(
                                m_dynamicsWorld.get(), m_worldReference));
                            break;
                        case DB_SLV_UPDATE_RB:
                        case DB_SLV_UPDATE_KINEMA:
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->prepareCollisionGeometry(false));
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->updateRigidBodies(m_dynamicsWorld.get(),
                                option.m_solver_proc_type == DB_SLV_UPDATE_KINEMA ?
                                    DBulletBodyMode::Kinematic : DBulletBodyMode::Dynamic));
                            break;
                        case DB_SLV_UPDATE_SHAPE_RB:
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->updateCollisionShape(
                                m_dynamicsWorld.get()));
                            break;
                        case DB_SLV_TRANS_KINEMA:
                        {
                            DOMMY_RETURN_IF_FAILURE(dmRbNode->updateRigidBodiesKinematic(
                                m_dynamicsWorld.get(), option.m_timestep));
                            
                        }
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            dmRbNode->detachFromWorld();
                            break;
                        default:
                            break;
                    }
                
                } else if(nodeType == DMbtFracture::typeId) {
                    
                    
                    
                    DMbtFracture *fractureNode =
                        dommy::maya::checkedUserNode<DMbtFracture>(
                            fnNode, DMbtFracture::typeId);
                    if (!fractureNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_RB:
                            DOMMY_RETURN_IF_FAILURE(fractureNode->prepareCollisionGeometry(true));
                            DOMMY_RETURN_IF_FAILURE(fractureNode->resetRigidBodies(
                                m_dynamicsWorld.get(), m_worldReference));
                            break;
                        case DB_SLV_UPDATE_RB:
                            DOMMY_RETURN_IF_FAILURE(fractureNode->prepareCollisionGeometry(false));
                            DOMMY_RETURN_IF_FAILURE(fractureNode->updateRigidBodies(m_dynamicsWorld.get()));
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            fractureNode->detachFromWorld();
                            break;
                        default:
                            break;
                    }
                    
                } else if(nodeType == DMbtSoft::typeId) {
                    
                    
                    DMbtSoft *dmSbNode =
                        dommy::maya::checkedUserNode<DMbtSoft>(
                            fnNode, DMbtSoft::typeId);
                    if (!dmSbNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_SB:
                            DOMMY_RETURN_IF_FAILURE(dmSbNode->resetSoftBodies(
                                m_dynamicsWorld.get(), m_worldInfo.get(),
                                m_worldReference));
                            break;
                        case DB_SLV_UPDATE_SB:
                            DOMMY_RETURN_IF_FAILURE(dmSbNode->updateSoftBodies(
                                m_dynamicsWorld.get(), m_worldInfo.get()));
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            dmSbNode->releaseSoftBodies();
                            break;
                        default:
                            break;
                    }
                    
                } else if(nodeType == DCbtRope::typeId) {
                    
                    
                    DCbtRope *ropeNode =
                        dommy::maya::checkedUserNode<DCbtRope>(
                            fnNode, DCbtRope::typeId);
                    if (!ropeNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_ROPE:
                            DOMMY_RETURN_IF_FAILURE(ropeNode->resetRopes(
                                m_dynamicsWorld.get(), m_worldInfo.get(),
                                m_worldReference));
                            break;
                        case DB_SLV_UPDATE_ROPE:
                            DOMMY_RETURN_IF_FAILURE(ropeNode->updateRopes(
                                m_dynamicsWorld.get(), m_worldInfo.get()));
                            break;
                        case DB_SLV_TRANS_ROPE:
                            DOMMY_RETURN_IF_FAILURE(ropeNode->updateLockPosition(
                                m_dynamicsWorld.get(), m_worldInfo.get(),
                                option.m_timestep));
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            ropeNode->detachFromWorld();
                            break;
                        default:
                            break;
                    }
                } else if (nodeType == DBPointConstraint::typeId) {
                    
                    DBPointConstraint *cNode =
                        dommy::maya::checkedUserNode<DBPointConstraint>(
                            fnNode, DBPointConstraint::typeId);
                    if (!cNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_CONST:
                            DOMMY_RETURN_IF_FAILURE(cNode->createConstraint(
                                m_dynamicsWorld.get(), m_worldReference));
                            break;
                        case DB_SLV_UPDATE_CONST:
                            DOMMY_RETURN_IF_FAILURE(cNode->updateConstraint(
                                m_dynamicsWorld.get(), option.m_timestep));
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            cNode->detachFromWorld();
                            break;
                        default:
                            break;
                    }
                    
                    
                } else if (nodeType == DBLinkConstraint::typeId) {
                    
                    DBLinkConstraint *cNode =
                        dommy::maya::checkedUserNode<DBLinkConstraint>(
                            fnNode, DBLinkConstraint::typeId);
                    if (!cNode) { if (detaching) continue; return MS::kFailure; }
                    
                    switch (option.m_solver_proc_type) {
                        case DB_SLV_INIT_CONST:
                            DOMMY_RETURN_IF_FAILURE(cNode->createConstraint(
                                m_dynamicsWorld.get(), m_worldReference));
                            break;
                        case DB_SLV_UPDATE_CONST:
                            DOMMY_RETURN_IF_FAILURE(cNode->updateConstraint(
                                m_dynamicsWorld.get(), option.m_timestep));
                            break;
                        case DB_SLV_DETACH_WORLD_RESOURCES:
                            cNode->detachFromWorld();
                            break;
                        default:
                            break;
                    }
                    
                    
                }
            }
        }
        
        
    
    
    
    return MS::kSuccess;
}



MStatus DMBulletTest::checkRigidBodies() {
    
    
    
    
    
    DBtSolverOptionParams option;
    option.m_solver_proc_type = DB_SLV_UPDATE_RB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDM, option));
    
    
    
    option.m_solver_proc_type = DB_SLV_UPDATE_KINEMA;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDMs, option));
    
    
    option.m_solver_proc_type = DB_SLV_UPDATE_SHAPE_RB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inDM, option));
    
    
    
    option.m_solver_proc_type = DB_SLV_UPDATE_SB;
    DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inSoftDM, option));
    
    
    
    
    
    return MS::kSuccess;
}



// solver method

void DMBulletTest::initBullet(MDataBlock& data) {
    if (m_collisionConfiguration || m_dispatcher || m_broadphase || m_solver ||
        m_dynamicsWorld || m_worldInfo || m_worldReference || m_plane_rb ||
        m_plane_col_shape) {
        throw std::logic_error(
            "DMBulletTest: Bullet resources must be cleared before initialization.");
    }

    const auto inputValue = [&](const MObject& attribute) -> MDataHandle {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) {
            throw std::runtime_error(
                "DMBulletTest: failed to read a Bullet input value.");
        }
        return handle;
    };

    const MVector gravity = inputValue(aGravity).asVector();
    const double airDensity = inputValue(aAirDensity).asDouble();
    const double waterDensity = inputValue(aWaterDensity).asDouble();
    const double waterOffset = inputValue(aWaterOffset).asDouble();
    const MVector waterNormal = inputValue(aWaterNormal).asVector();
    const double maxDisplacement = inputValue(aMaxDisplacement).asDouble();
    if (!dommy::maya::isFloatRepresentableVector(gravity) ||
        !dommy::numeric::canRepresentAsFloat(airDensity) ||
        !dommy::numeric::canRepresentAsFloat(waterDensity) ||
        !dommy::numeric::canRepresentAsFloat(waterOffset) ||
        !dommy::maya::isFloatRepresentableVector(waterNormal) ||
        !dommy::numeric::canRepresentAsFloat(maxDisplacement)) {
        throw std::invalid_argument(
            "DMBulletTest: world values must contain finite Bullet scalar values.");
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
                "DMBulletTest: plane values must be finite and the axis must be non-zero.");
        }
    }

    // Build the complete world under local ownership. Member pointers remain
    // empty until all allocations and configuration steps have succeeded.
    std::unique_ptr<btSoftBodyRigidBodyCollisionConfiguration>
        collisionConfiguration(new btSoftBodyRigidBodyCollisionConfiguration());
    std::unique_ptr<btCollisionDispatcher> dispatcher(
        new btCollisionDispatcher(collisionConfiguration.get()));
    
    std::unique_ptr<btBroadphaseInterface> broadphase(new btDbvtBroadphase());
    std::unique_ptr<btConstraintSolver> solver(
        new btSequentialImpulseConstraintSolver());
    std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld> dynamicsWorld(
        new dommy::bullet::DBoundedSoftDynamicsWorld(
            dispatcher.get(), broadphase.get(), solver.get(),
            collisionConfiguration.get()));
    DBulletWorldReferencePtr worldReference(
        new DBulletWorldReference(dynamicsWorld.get()));
    //register algorithm for concave meshes
    btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher.get());
    dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
    dynamicsWorld->getDispatchInfo().m_enableSPU = true;

    DBulletSoftBodyWorldInfoPtr worldInfo(new btSoftBodyWorldInfo());
    worldInfo->m_broadphase = broadphase.get();
    worldInfo->m_dispatcher = dispatcher.get();
    worldInfo->m_gravity = dynamicsWorld->getGravity();
    worldInfo->m_sparsesdf.Initialize();
    worldInfo->air_density = static_cast<btScalar>(airDensity);
    worldInfo->water_density = static_cast<btScalar>(waterDensity);
    worldInfo->water_offset = static_cast<btScalar>(waterOffset);
    worldInfo->water_normal.setValue(
        waterNormal.x, waterNormal.y, waterNormal.z);
    worldInfo->m_maxDisplacement = static_cast<btScalar>(maxDisplacement);

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

    m_collisionConfiguration = std::move(collisionConfiguration);
    m_dispatcher = std::move(dispatcher);
    m_broadphase = std::move(broadphase);
    m_solver = std::move(solver);
    m_worldReference = worldReference;
    m_worldInfo = std::move(worldInfo);
    m_debugDrawer = std::move(debugDrawer);
    m_plane_col_shape = std::move(planeShape);
    m_plane_rb = std::move(planeBody);
    m_dynamicsWorld = std::move(dynamicsWorld);
}


void DMBulletTest::clearBullet() noexcept {
    if (m_dynamicsWorld) {
        DBtSolverOptionParams detachResources;
        detachResources.m_solver_proc_type =
            DB_SLV_DETACH_WORLD_RESOURCES;
        const auto detachConnectedNodes = [&](const MObject& attribute) {
            try {
                // Teardown remains best effort; residual objects are detached below.
                (void)solverProcessFromPlug(attribute, detachResources);
            } catch (...) {
                // Continue with the remaining connections when Maya has
                // already removed one node during graph teardown.
            }
        };
        detachConnectedNodes(inConst);
        detachConnectedNodes(inRopeDC);
        detachConnectedNodes(inSoftDM);
        detachConnectedNodes(inDM);
        detachConnectedNodes(inKinematicDMs);

        
        // Remove objects owned by Maya nodes before destroying the world.
        for (int i=m_dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
        {
            btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
            if (obj) {
                m_dynamicsWorld->removeCollisionObject( obj );
            }
        }
        
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

    

    m_debugDrawer.reset();
    m_worldInfo.reset();
    m_dispatcher.reset();
    m_broadphase.reset();
    m_solver.reset();
    m_collisionConfiguration.reset();
    
}




void DMBulletTest::deleteRigidBodies() noexcept
{
    
    
    m_plane_rb.reset();
    m_plane_col_shape.reset();
    
}


MStatus DMBulletTest::appendContactSamples(ContactSamples& samples) const
{
    if (!m_dynamicsWorld) return MS::kFailure;
    btDispatcher* dispatcher = m_dynamicsWorld->getDispatcher();
    if (!dispatcher) return MS::kFailure;
    for (int i = 0; i < dispatcher->getNumManifolds(); ++i) {
        const btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
        if (!manifold) return MS::kFailure;
        const int count = manifold->getNumContacts();
        std::size_t total;
        if (count < 0 || !dommy::generation::checkedSum(
                samples.positions.size(), static_cast<std::size_t>(count),
                dommy::generation::kMaxGeneratedPointCount, total))
            return MS::kInvalidParameter;
        for (int j = 0; j < count; ++j) {
            const btManifoldPoint& point = manifold->getContactPoint(j);
            const btVector3& position = point.getPositionWorldOnA();
            const btVector3& normal = point.m_normalWorldOnB;
            for (unsigned axis = 0; axis < 3; ++axis)
                if (!dommy::numeric::canRepresentAsFloat(position[axis]) ||
                    !dommy::numeric::canRepresentAsFloat(normal[axis]))
                    return MS::kInvalidParameter;
            if (!dommy::numeric::canRepresentAsFloat(point.getDistance()) ||
                !dommy::numeric::canRepresentAsFloat(point.getAppliedImpulse()))
                return MS::kInvalidParameter;
            samples.positions.push_back(DPoint(position.x(), position.y(), position.z()));
            samples.distances.push_back(point.getDistance());
            samples.impulses.push_back(point.getAppliedImpulse());
            samples.ages.push_back(point.getLifeTime());
            samples.normals.push_back(DVec3Data(normal.x(), normal.y(), normal.z()));
        }
    }
    return MS::kSuccess;
}

// main compute
MStatus DMBulletTest::compute( const MPlug& plug, MDataBlock& data ) try
{
    if(plug == outDM || plug == outSoftDM || plug == outRopeDC || plug == outContactDP) {
        
        
        MStatus stat;
        dommy::geometry::GeometryBatchBudget inputBudget;
        dommy::bullet::BulletWorldMutationGuard mutationGuard(m_requiresStartFrame);
        MTime candidateTime = m_prevTime;
        std::vector<double> candidateSettings;
        
// get inputDMs and outputDM        
        MFnDependencyNode nodeFn(thisMObject(), &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MPlug inDMArrayPlug = nodeFn.findPlug(inDM, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int meshInputCount = inDMArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (meshInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        
        
        
        
        
        DMeshDataPtrList outDMeshList;
        UuidList nodeIdList;
        std::vector<MObject> objArray; // Own copied data until publication completes.
        
        //Plug array base
        for (unsigned int i = 0; i < meshInputCount; ++i) {
            MPlug plg = inDMArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            
            
            
            
            if(inputConnected) {
                MPlugArray dmConnections;
                const bool connected = plg.connectedTo(
                    dmConnections, true, true, &stat);
                if (!stat) return stat;
                if (!connected || dmConnections.length() == 0U) {
                    continue;
                }
                
                
                MObject sourceNode = dmConnections[0].node(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                MFnDependencyNode fnNode(sourceNode, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MUuid cuuid = fnNode.uuid(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!cuuid.valid()) return MS::kInvalidParameter;
                
                MObject obj;
                const DMeshData* inputDMData = nullptr;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
                    plg, DMeshData::id, obj, inputDMData));
                if (!inputDMData) return MS::kFailure;
                if (!dommy::maya::validatePluginData(*inputDMData))
                    return MS::kInvalidParameter;
                if (!dommy::bullet::admitMeshInput(*inputDMData, inputBudget))
                    return MS::kInvalidParameter;
                if (inputDMData) {
                        
                        MFnPluginData pluginData;
                        MObject newObj = pluginData.create(DMeshData::id, &stat);
                        
                        if (!stat) return stat;
                        
                        DMeshData* cache = nullptr;
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginData(pluginData, cache));
                        if (!cache) return MS::kFailure;

                        objArray.push_back(newObj);
                        DOMMY_RETURN_IF_FAILURE(cache->copyFrom(*inputDMData));

                        

                        outDMeshList.push_back(cache);
                        nodeIdList.push_back(cuuid);
                    }
            }
            
        }
        
        
// get inputDMs and outputSoftDM        
        MPlug inSoftDMArrayPlug = nodeFn.findPlug(inSoftDM, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int softInputCount = inSoftDMArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (softInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        
        
        
        
        
        DMeshDataPtrList outSoftDMeshList;
        UuidList softNodeIdList;
        std::vector<MObject> softObjArray; // Own copied data until publication completes.
        
        std::vector<DPIdList> panicIdList;
        const MDataHandle panicInput = data.inputValue(aIsCheckAndDeletePanicSoftBody, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isCheckPanicSb = panicInput.asBool();
        
        //Plug array base
        for (unsigned int i = 0; i < softInputCount; ++i) {
            MPlug plg = inSoftDMArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            
            
            
            
            if(inputConnected) {
                MPlugArray dmConnections;
                const bool connected = plg.connectedTo(
                    dmConnections, true, true, &stat);
                if (!stat) return stat;
                if (!connected || dmConnections.length() == 0U) {
                    continue;
                }
                
                
                MObject sourceNode = dmConnections[0].node(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                MFnDependencyNode fnNode(sourceNode, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MTypeId sourceType = fnNode.typeId(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                
                if(sourceType == DMbtSoft::typeId) {
                    
                    
                    const MUuid cuuid = fnNode.uuid(&stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    if (!cuuid.valid()) return MS::kInvalidParameter;
                    
                    MObject obj;
                    const DMeshData* inputDMData = nullptr;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
                        plg, DMeshData::id, obj, inputDMData));
                    if (!inputDMData) return MS::kFailure;
                    if (!dommy::maya::validatePluginData(*inputDMData))
                        return MS::kInvalidParameter;
                    if (!dommy::bullet::admitMeshInput(*inputDMData, inputBudget))
                        return MS::kInvalidParameter;
                    if (inputDMData) {
                            
                            MFnPluginData pluginData;
                            MObject newObj = pluginData.create(DMeshData::id, &stat);
                            
                            if (!stat) return stat;
                            
                            DMeshData* cache = nullptr;
                            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginData(pluginData, cache));
                            if (!cache) return MS::kFailure;

                            softObjArray.push_back(newObj);
                            DOMMY_RETURN_IF_FAILURE(cache->copyFrom(*inputDMData));

                            

                            outSoftDMeshList.push_back(cache);
                            softNodeIdList.push_back(cuuid);

                            panicIdList.push_back(DPIdList(cache->numberOfMesh(),false));
                        }//check DMesh
                    
                }//check node type
                
            }//check plug connected
            
        }
        
        
// get inputRopeDC        
        MArrayDataHandle ropeDCArrayHandle = data.inputArrayValue(inRopeDC, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        unsigned int ropecount = ropeDCArrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        MPlug inRopeDCArrayPlug = nodeFn.findPlug(inRopeDC, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int ropeInputCount = inRopeDCArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (ropeInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        
        
        
        
        
        DCurveDataPtrList outRopeDCurveList;
        UuidList ropeNodeIdList;
        std::vector<MObject> ropeObjArray; // Own copied data until publication completes.
        
        
        //Plug array base
        for (unsigned int i = 0; i < ropeInputCount; ++i) {
            MPlug plg = inRopeDCArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            
            
            
            if(inputConnected) {
                MPlugArray dcConnections;
                const bool connected = plg.connectedTo(
                    dcConnections, true, true, &stat);
                if (!stat) return stat;
                if (!connected || dcConnections.length() == 0U) {
                    continue;
                }
                
                
                MObject sourceNode = dcConnections[0].node(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                MFnDependencyNode fnNode(sourceNode, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const MTypeId sourceType = fnNode.typeId(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                
                if(sourceType == DCbtRope::typeId) {
                
                    
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
                            
                            MFnPluginData pluginData;
                            MObject newObj = pluginData.create(DCurveData::id, &stat);
                            
                            if (!stat) return stat;
                            
                            DCurveData* cache = nullptr;
                            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginData(pluginData, cache));
                            if (!cache) return MS::kFailure;

                            DOMMY_RETURN_IF_FAILURE(cache->copyFrom(*inputDCData));
                            ropeObjArray.push_back(newObj);

                            

                            outRopeDCurveList.push_back(cache);
                            ropeNodeIdList.push_back(cuuid);
                        }//check DCurve
                    
                }//check node type
                
            }//check plug connected
            
        }
        
        
        
        
        
        // Force connected kinematic inputs to evaluate before traversal.
        MArrayDataHandle kinemaArrayHandle = data.inputArrayValue(inKinematicDMs, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        unsigned int kinemaCount = kinemaArrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        MPlug inKinemaDMArrayPlug = nodeFn.findPlug(inKinematicDMs, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int kinematicInputCount = inKinemaDMArrayPlug.numConnectedElements(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (kinematicInputCount > dommy::generation::kMaxConnectedGeometryCount)
            return MS::kInvalidParameter;
        for (unsigned int i = 0; i < kinematicInputCount; ++i) {
            MPlug plg = inKinemaDMArrayPlug.connectionByPhysicalIndex(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool inputConnected = plg.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            
            
            
            if(inputConnected) {
                // Force evaluation before the kinematic node is updated by
                // the solver traversal below.
                MObject inputObject;
                const DMeshData* inputDMData = nullptr;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
                    plg, DMeshData::id, inputObject, inputDMData));
                if (!inputDMData) return MS::kFailure;
                if (!dommy::maya::validatePluginData(*inputDMData))
                    return MS::kInvalidParameter;
                if (!dommy::bullet::admitMeshInput(*inputDMData, inputBudget, false))
                    return MS::kInvalidParameter;
            }
        }
        
        
        MArrayDataHandle inConstHandle = data.inputArrayValue(inConst, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        size_t constCount = inConstHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        
        
        
        
        
        const MDataHandle enabledInput = data.inputValue(aEnabled, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        bool enabled = enabledInput.asBool();
        
        const MDataHandle contactInput = data.inputValue(aContactData, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isContactData = contactInput.asBool();
        ContactSamples candidateContacts;
        bool reuseContacts = false;
        bool replaceContacts = false;
        
        
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
            DOMMY_RETURN_IF_FAILURE(dommy::bullet::appendRestartScalars(
                data, {aAirDensity, aWaterDensity, aWaterOffset, aMaxDisplacement},
                candidateSettings));
            DOMMY_RETURN_IF_FAILURE(dommy::bullet::appendRestartVectors(
                data, {aWaterNormal}, candidateSettings));
            if (!(time == startTime) && candidateSettings != m_worldSettings)
                m_requiresStartFrame = true;
            const dommy::bullet::TimelineAction action = dommy::bullet::timelineAction(
                !m_requiresStartFrame && bool(m_dynamicsWorld) && bool(m_worldInfo), time == startTime,
                (time - m_prevTime).value(), playback);
            if (action == dommy::bullet::TimelineAction::RestartRequired) {
                m_requiresStartFrame = true;
                EchoWarning(name().asChar(), "Restart simulation from StartTime after a time jump, world-setting change or failed evaluation.");
                return MS::kFailure;
            }
            candidateTime = time;
            replaceContacts = action != dommy::bullet::TimelineAction::Reuse;
            if (action == dommy::bullet::TimelineAction::Reuse && isContactData) {
                reuseContacts = m_contactSamplesValid;
                if (!reuseContacts) {
                    // Contact output can be enabled at the current time without
                    // advancing physics. Capture only the current manifolds.
                    DOMMY_RETURN_IF_FAILURE(appendContactSamples(candidateContacts));
                    replaceContacts = true;
                }
            }

            if (action == dommy::bullet::TimelineAction::Initialize) {
                mutationGuard.arm();
                
                
                //need to re-construct the world and re-insert all objects...
                
                clearBullet();
                
                deleteRigidBodies();
                
                initBullet(data);
                DOMMY_RETURN_IF_FAILURE(initRigidBodies());
                
                m_reInitialize = true;
                m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulseEnabled;
                
                //update positions of all objects
                if (outDMeshList.size()) {
                    DOMMY_RETURN_IF_FAILURE(updateRigidBodies(&outDMeshList, &nodeIdList));
                }
                //update softbidy shape of all objects
                if (outSoftDMeshList.size()) {
                    updateSoftBodies(
                        &outSoftDMeshList, &softNodeIdList, &panicIdList);
                }
                //update rope curves of all objects
                if (outRopeDCurveList.size()) {
                    if (!updateRopes(
                            &outRopeDCurveList, &ropeNodeIdList)) {
                        return MS::kFailure;
                    }
                }
                
            } else {
                
                if(m_dynamicsWorld) {
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
                        // Soft bodies retain this separate world-info object.
                        m_worldInfo->m_gravity = m_dynamicsWorld->getGravity();
                        m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulseEnabled;
                        // Refresh inputs and remove empty bodies before any physics step.
                        DOMMY_RETURN_IF_FAILURE(checkRigidBodies());
                        
                        DBtSolverOptionParams option,optRope,optionConst;
                        option.m_solver_proc_type = DB_SLV_TRANS_KINEMA;
                        // Apply both endpoint flags before the first physics substep.
                        optRope.m_solver_proc_type = DB_SLV_UPDATE_ROPE;
                        if (ropecount > 0)
                            DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inRopeDC, optRope));
                        optRope.m_solver_proc_type = DB_SLV_TRANS_ROPE;
                        optionConst.m_solver_proc_type = DB_SLV_UPDATE_CONST;
                        
                        
                        float kinemaStep = 1.0 / (float)subSteps;
                        if (subSteps==1) kinemaStep = 1.0;
                        
                        
                       
                        
                        
                        for (int i = 1; i <= subSteps; ++i)
                        {
                            //update kinematice body　サブステップごとにトランスフォームをアプデートする
                            option.m_timestep = optRope.m_timestep = optionConst.m_timestep = (float)i * kinemaStep;
                            
                            
                            if(kinemaCount>0) DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inKinematicDMs, option));
                            
                            if(ropecount>0) DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inRopeDC, optRope));
                            
                            if(constCount>0) DOMMY_RETURN_IF_FAILURE(solverProcessFromPlug(inConst, optionConst));
                            
                            
                            dommy::bullet::SoftBodyForces softForces;
                            if (affectMayaField) DOMMY_RETURN_IF_FAILURE(applyFields(data, step.seconds, softForces));
                            
                            
                            
                            if (!m_dynamicsWorld->stepChecked(step.seconds, step.fixedSeconds, softForces))
                                return MS::kInvalidParameter;
                            
                            
                            if (isContactData)
                                DOMMY_RETURN_IF_FAILURE(appendContactSamples(candidateContacts));
                            
                        }
                        
                        
                        
                    }
                    
                    //update positions of all objects
                    if (outDMeshList.size()) {
                        DOMMY_RETURN_IF_FAILURE(updateRigidBodies(&outDMeshList, &nodeIdList));
                    }
                    //update softbidy shape of all objects
                    if (outSoftDMeshList.size()) {
                        updateSoftBodies(
                            &outSoftDMeshList, &softNodeIdList,
                            &panicIdList);
                    }
                    //update rope curves of all objects
                    if (outRopeDCurveList.size()) {
                        if (!updateRopes(
                                &outRopeDCurveList, &ropeNodeIdList)) {
                            return MS::kFailure;
                        }
                    }
                    
                    
                } else {
                    return MS::kFailure;
                    
                }
                
            }//step sim
            
            
        }//calc sim
        
        
        
        
// Prepare every output before publishing or marking any of them clean.
        for (auto* mesh : outDMeshList) mesh->updateBBox();
        for (std::size_t n = 0; n < outSoftDMeshList.size(); ++n) {
            if (isCheckPanicSb && !outSoftDMeshList[n]->deleteFromIds(panicIdList[n]))
                return MS::kFailure;
            outSoftDMeshList[n]->updateBBox();
        }
        for (auto* curve : outRopeDCurveList) curve->updateBBox();

        const ContactSamples& contacts = reuseContacts ? m_contactSamples : candidateContacts;
        MObject contactObject;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DPData>(
            DPData::id, [&](DPData& outputData) {
                outputData.setPosition(contacts.positions);
                outputData.updateBBox();
                if (!contacts.positions.empty()) {
                    outputData.addAttr("dist", DPFloatAttr(contacts.distances));
                    outputData.addAttr("impulse", DPFloatAttr(contacts.impulses));
                    outputData.addAttr("age", DPFloatAttr(contacts.ages));
                    outputData.addAttr("N", DPVec3fAttr(contacts.normals));
                }
            }, contactObject));

        MArrayDataHandle outArrayHandle = data.outputArrayValue(outDM, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MArrayDataHandle outSoftArrayHandle = data.outputArrayValue(outSoftDM, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MArrayDataHandle outRopeDCArrayHandle = data.outputArrayValue(outRopeDC, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MDataHandle contactHandle = data.outputValue(outContactDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput(data, outDM, plug, objArray, false));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput(data, outSoftDM, plug, softObjArray, false));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput(data, outRopeDC, plug, ropeObjArray, false));
        DOMMY_RETURN_IF_FAILURE(contactHandle.set(contactObject));
        DOMMY_RETURN_IF_FAILURE(outArrayHandle.setAllClean());
        DOMMY_RETURN_IF_FAILURE(outSoftArrayHandle.setAllClean());
        DOMMY_RETURN_IF_FAILURE(outRopeDCArrayHandle.setAllClean());
        contactHandle.setClean();
        DOMMY_RETURN_IF_FAILURE(data.setClean(plug));
        if (enabled) {
            if (replaceContacts) {
                m_contactSamples.swap(candidateContacts);
                m_contactSamplesValid = isContactData;
            }
            m_prevTime = candidateTime;
            m_worldSettings.swap(candidateSettings);
            m_requiresStartFrame = false;
        }
        mutationGuard.commit();
        return MS::kSuccess;
        
    }

    return MS::kUnknownParameter;
    
    
}
DOMMY_COMPUTE_CATCH("DMBulletTest")




MStatus DMBulletTest::initialize() try {
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnMatrixAttribute matAttr;
    
    MStatus stat;
    
   
    // ----------------------- INPUTS --------------------------
    
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    inKinematicDMs = tAttr.create("inKinematicDMs", "iknmdms", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inKinematicDMs));
    
    inSoftDM = tAttr.create("inSoftDM", "isdm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inSoftDM));
    
    
    
    
    
    inRopeDC = tAttr.create("inRopeDC", "irdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inRopeDC));
    
    
    
    
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
    
    aAirDensity = nAttr.create("AirDensity", "adnt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAirDensity));
    
    aWaterDensity = nAttr.create("WaterDensity", "wdnt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWaterDensity));
    
    aWaterOffset = nAttr.create("WaterOffset", "woff", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWaterOffset));
    
    aWaterNormal = nAttr.create("WaterNormal", "wnorm", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWaterNormal));
    
    aMaxDisplacement = nAttr.create("MaxDisplacement", "mxdspmnt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1000.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxDisplacement));
    
    
    
    aCollisionMargin = nAttr.create("CollisionMargin", "colm", MFnNumericData::kFloat, 0.04, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionMargin));
    
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
    
    
// Panic-body handling
    aIsCheckAndDeletePanicSoftBody = nAttr.create("IsCheckAndDeletePanicSoftBody", "ischkanddelpncsb", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCheckAndDeletePanicSoftBody));
    
    
    
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
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    outSoftDM = tAttr.create("outSoftDM", "osdm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outSoftDM));
    
    outRopeDC = tAttr.create("outRopeDC", "ordc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outRopeDC));
    
    outContactDP= tAttr.create("outContactDP", "ocntdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outContactDP));
    
    
    const MObject simulationInputs[] = {
        inDM, inKinematicDMs, inSoftDM, inRopeDC, inConst,
        aCurrentTime, aEnabled, aStartTime, aGravity, aSplitImpulse,
        aSubsteps, aFixedPhysicsRate, aAffectMayaFiels,
        aIsPlane, aPlanePosition, aPlaneAxis, aPlaneRestitution, aPlaneFriction,
        aAirDensity, aWaterDensity, aWaterOffset, aWaterNormal, aMaxDisplacement,
        aContactData, aIsCheckAndDeletePanicSoftBody
    };
    const MObject simulationOutputs[] = {outDM, outSoftDM, outRopeDC, outContactDP};
    for (const MObject& input : simulationInputs)
        for (const MObject& output : simulationOutputs)
            DOMMY_RETURN_IF_FAILURE(attributeAffects(input, output));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMBulletTest", "initialize", std::current_exception());
}
