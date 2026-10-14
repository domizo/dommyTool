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
//  DBulletFractureSolver.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//

#include "DBulletFractureSolver.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaObjectArrayOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "modules/dynamics/DBullet/Solver/DBulletEvaluation.h"
#include "modules/dynamics/DBullet/Solver/DBulletMayaState.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"


#include "DMbtRigid.h"
#include "DMbtSoft.h"
#include "DCbtRope.h"
#include "commonBulletRigidBody.h"
#include "commonDBullet.h"

//Bullet
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "LinearMath/btGeometryUtil.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"

#include <memory>
#include <stdexcept>



#define  DM_BULLET_FRACTURE_DEBUG DOMMY_ENABLE_DEBUG_LOGS


MTypeId DBulletFractureSolver::typeId( ID_DM_BT_FRACTURE_SOLVER );
const MString DBulletFractureSolver::typeName("DBulletFractureSolver");


MObject DBulletFractureSolver::inDM;



MObject DBulletFractureSolver::aEnabled;
MObject DBulletFractureSolver::aCurrentTime;
MObject DBulletFractureSolver::aStartTime;


//solver attribute
MObject DBulletFractureSolver::aGravity;
MObject DBulletFractureSolver::aAirDensity;
MObject DBulletFractureSolver::aWaterDensity;
MObject DBulletFractureSolver::aWaterOffset;
MObject DBulletFractureSolver::aWaterNormal;
MObject DBulletFractureSolver::aMaxDisplacement;

MObject DBulletFractureSolver::aIsFractureMode;

MObject DBulletFractureSolver::aDisableCollisionsBetweenLinkedBodies;
MObject DBulletFractureSolver::aSplitImpulse;
MObject DBulletFractureSolver::aSubsteps;
MObject DBulletFractureSolver::aFixedPhysicsRate;


MObject DBulletFractureSolver::aContactData;

//plane attribute
MObject DBulletFractureSolver::aIsPlane;
MObject DBulletFractureSolver::aPlanePosition;
MObject DBulletFractureSolver::aPlaneAxis;
MObject DBulletFractureSolver::aPlaneRestitution;
MObject DBulletFractureSolver::aPlaneFriction;


MObject DBulletFractureSolver::ia_DBG_DrawWireframe;
MObject DBulletFractureSolver::ia_DBG_DrawAabb;
MObject DBulletFractureSolver::ia_DBG_DrawFeaturesText;
MObject DBulletFractureSolver::ia_DBG_DrawContactPoints;
MObject DBulletFractureSolver::ia_DBG_NoDeactivation;
MObject DBulletFractureSolver::ia_DBG_NoHelpText;
MObject DBulletFractureSolver::ia_DBG_DrawText;
MObject DBulletFractureSolver::ia_DBG_ProfileTimings;
MObject DBulletFractureSolver::ia_DBG_EnableSatComparison;
MObject DBulletFractureSolver::ia_DBG_DisableBulletLCP;
MObject DBulletFractureSolver::ia_DBG_EnableCCD;
MObject DBulletFractureSolver::ia_DBG_DrawConstraints;
MObject DBulletFractureSolver::ia_DBG_DrawConstraintLimits;
MObject DBulletFractureSolver::ia_DBG_FastWireframe;

MObject DBulletFractureSolver::outDM;




DBulletFractureSolver::DBulletFractureSolver()
{
}


DBulletFractureSolver::~DBulletFractureSolver()
{
    clearBullet();
}




void *DBulletFractureSolver::creator() {
    return ::dommy::maya::createObject("DBulletFractureSolver", [&]() {
        return new DBulletFractureSolver();
    });
}



void DBulletFractureSolver::postConstructor()
{
    //prevent deletion if all the rigidbodies are deleted
    setExistWithoutOutConnections(true);
}

MPxNode::SchedulingType DBulletFractureSolver::schedulingType() const
{
    return MPxNode::kUntrusted;
}

bool DBulletFractureSolver::setInternalValueInContext(
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

void DBulletFractureSolver::draw( M3dView & view, const MDagPath&,
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
DOMMY_DRAW_CATCH("DBulletFractureSolver")

void DBulletFractureSolver::initBullet(MDataBlock& data) {
    if (m_collisionConfiguration || m_dispatcher || m_broadphase ||
        m_solver || m_debugDrawer || m_dynamicsWorld ||
        m_plane_col_shape) {
        throw std::logic_error(
            "DBulletFractureSolver: resources must be cleared before initialization.");
    }

    const auto inputValue = [&](const MObject& attribute) -> MDataHandle {
        MStatus status;
        const MDataHandle handle = data.inputValue(attribute, &status);
        if (!status) {
            throw std::runtime_error(
                "DBulletFractureSolver: failed to read an input value.");
        }
        return handle;
    };

    const MVector gravity = inputValue(aGravity).asVector();
    if (!dommy::maya::isFloatRepresentableVector(gravity)) {
        throw std::invalid_argument(
            "DBulletFractureSolver: gravity must contain finite Bullet values.");
    }

    const bool fractureMode = inputValue(aIsFractureMode).asBool();
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
                "DBulletFractureSolver: plane values must be finite and its axis non-zero.");
        }
    }

    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration(
        new btSoftBodyRigidBodyCollisionConfiguration());
    std::unique_ptr<btCollisionDispatcher> dispatcher(
        new btCollisionDispatcher(collisionConfiguration.get()));
    std::unique_ptr<btBroadphaseInterface> broadphase(new btDbvtBroadphase());
    std::unique_ptr<btConstraintSolver> solver(
        new btSequentialImpulseConstraintSolver());
    std::unique_ptr<DBFractureDynamicsWorld> dynamicsWorld(
        new DBFractureDynamicsWorld(
            dispatcher.get(), broadphase.get(), solver.get(),
            collisionConfiguration.get()));

    btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher.get());
    dynamicsWorld->setGravity(
        btVector3(gravity.x, gravity.y, gravity.z));
    dynamicsWorld->getDispatchInfo().m_enableSPU = true;
    dynamicsWorld->setFractureMode(fractureMode);

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

        std::unique_ptr<btMotionState> motionState(
            new btDefaultMotionState(groundTransform));
        btRigidBody::btRigidBodyConstructionInfo bodyInfo(
            0.0, motionState.get(), planeShape.get(), btVector3(0, 0, 0));
        bodyInfo.m_restitution = static_cast<btScalar>(planeRestitution);
        bodyInfo.m_friction = static_cast<btScalar>(planeFriction);
        planeBody = MakeBulletRigidBody(std::move(motionState), bodyInfo);
        dynamicsWorld->addRigidBody(planeBody.get());
    }

    m_collisionConfiguration = std::move(collisionConfiguration);
    m_dispatcher = std::move(dispatcher);
    m_broadphase = std::move(broadphase);
    m_solver = std::move(solver);
    m_debugDrawer = std::move(debugDrawer);
    m_plane_col_shape = std::move(planeShape);
    m_dynamicsWorld = std::move(dynamicsWorld);
    planeBody.release();
}





void DBulletFractureSolver::clearBullet() {
    if (m_dynamicsWorld) {
        if(DM_BULLET_FRACTURE_DEBUG)DOMMY_MAYA_DEBUG("DBulletFractureSolver", "delete rigidbodies from the dynamics world:" << m_dynamicsWorld->getNumCollisionObjects());
        while (m_dynamicsWorld->getNumCollisionObjects() > 0) {
            btCollisionObject* const object =
                m_dynamicsWorld->getCollisionObjectArray()[
                    m_dynamicsWorld->getNumCollisionObjects() - 1];
            btRigidBody* const body = btRigidBody::upcast(object);
            if (body) {
                m_dynamicsWorld->destroyRigidBody(body);
            } else {
                m_dynamicsWorld->removeCollisionObject(object);
                delete object;
            }
        }
        if(DM_BULLET_FRACTURE_DEBUG)DOMMY_MAYA_DEBUG("DBulletFractureSolver", "delete constraint from the dynamics world");
        while (m_dynamicsWorld->getNumConstraints()) {
            btTypedConstraint* const constraint =
                m_dynamicsWorld->getConstraint(0);
            m_dynamicsWorld->removeConstraint(constraint);
            delete constraint;
        }

        m_dynamicsWorld->setDebugDrawer(nullptr);
        m_dynamicsWorld.reset();
    }
    m_debugDrawer.reset();
    m_plane_col_shape.reset();
    if(DM_BULLET_FRACTURE_DEBUG)DOMMY_MAYA_DEBUG("DBulletFractureSolver", "clear all");
    m_dispatcher.reset();
    m_broadphase.reset();
    m_solver.reset();
    m_collisionConfiguration.reset();
    if(DM_BULLET_FRACTURE_DEBUG)DOMMY_MAYA_DEBUG("DBulletFractureSolver", "all deleted");
}


// -----------------------------------------------------------------------------
// Main compute
MStatus DBulletFractureSolver::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug.attribute() != outDM) return MS::kUnknownParameter;

    MStatus status;
    dommy::bullet::BulletWorldMutationGuard mutationGuard(m_requiresStartFrame);
    MTime candidateTime = m_prevTime;
    std::vector<double> candidateSettings;

    // Preserve connected-input order and compact output indices. Disconnected
    // array slots are not inputs; every connected input must copy successfully.
    MFnDependencyNode node(thisMObject(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MPlug inputs = node.findPlug(inDM, true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int count = inputs.numConnectedElements(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (count > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;

    std::vector<MObject> objects;
    objects.reserve(count);
    for (unsigned int index = 0; index < count; ++index) {
        const MPlug input = inputs.connectionByPhysicalIndex(index, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        MObject inputObject;
        const DMeshData* inputData = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromPlug(
            input, DMeshData::id, inputObject, inputData));
        if (!inputData) return MS::kFailure;

        MObject object;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
            DMeshData::id, [&](DMeshData& output) -> MStatus {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*inputData));
                output.updateBBox();
                return MS::kSuccess;
            }, object));
        objects.push_back(object);
    }

    const MDataHandle enabledInput = data.inputValue(aEnabled, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool enabled = enabledInput.asBool();
    if (enabled) {
        const MDataHandle timeInput = data.inputValue(aCurrentTime, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MTime time = timeInput.asTime();
        const MDataHandle startTimeInput = data.inputValue(aStartTime, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MTime startTime = startTimeInput.asTime();
        if (!std::isfinite(time.value()) || !std::isfinite(startTime.value()))
            return MS::kInvalidParameter;
        const MDataHandle gravityInput = data.inputValue(aGravity, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MVector gravity = gravityInput.asVector();
        if (!dommy::maya::isFloatRepresentableVector(gravity))
            return MS::kInvalidParameter;
        const MDataHandle splitImpulseInput = data.inputValue(aSplitImpulse, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const bool splitImpulse = splitImpulseInput.asBool();
        const MDataHandle fractureInput = data.inputValue(aIsFractureMode, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const bool fractureMode = fractureInput.asBool();
        const bool playback = MConditionMessage::getConditionState("playingBack", &status);
        DOMMY_RETURN_IF_FAILURE(status);

        DOMMY_RETURN_IF_FAILURE(dommy::bullet::readPlaneRestartSettings(
            data, startTime.as(MTime::kSeconds),
            {aIsPlane, aPlanePosition, aPlaneAxis, aPlaneRestitution, aPlaneFriction},
            candidateSettings));
        candidateSettings.push_back(fractureMode ? 1.0 : 0.0);
        if (!(time == startTime) && candidateSettings != m_worldSettings)
            m_requiresStartFrame = true;
        const dommy::bullet::TimelineAction action = dommy::bullet::timelineAction(
            !m_requiresStartFrame && bool(m_dynamicsWorld), time == startTime,
            (time - m_prevTime).value(), playback);
        if (action == dommy::bullet::TimelineAction::RestartRequired) {
            m_requiresStartFrame = true;
            EchoWarning(name().asChar(),
                "Restart simulation from StartTime after a time jump, world-setting change or failed evaluation.");
            return MS::kFailure;
        }
        candidateTime = time;

        if (action == dommy::bullet::TimelineAction::Initialize) {
            mutationGuard.arm();
            clearBullet();
            initBullet(data);
            m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulse;
        } else if (action == dommy::bullet::TimelineAction::Advance) {
            // Retain the prototype's single variable step. Its unused substep
            // and fixed-rate attributes do not acquire new behavior here.
            float seconds = 0;
            if (!dommy::numeric::checkedFloatCast(
                    (time - m_prevTime).as(MTime::kSeconds), seconds) ||
                seconds <= 0 || !dommy::bullet::isSafeInverseOperand(seconds) ||
                btFuzzyZero(seconds))
                return MS::kInvalidParameter;
            mutationGuard.arm();
            m_dynamicsWorld->setGravity(btVector3(gravity[0], gravity[1], gravity[2]));
            m_dynamicsWorld->getSolverInfo().m_splitImpulse = splitImpulse;
            // solveConstraints() selects fracture or glue once per native step.
            m_dynamicsWorld->stepSimulation(seconds, 0);
        }
    }

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setObjectArrayOutput(data, outDM, plug, objects));
    if (enabled) {
        m_prevTime = candidateTime;
        m_worldSettings.swap(candidateSettings);
        m_requiresStartFrame = false;
    }
    mutationGuard.commit();
    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DBulletFractureSolver")




MStatus DBulletFractureSolver::initialize() try {
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
    
    
    aIsFractureMode = nAttr.create("IsFractureMode", "isfrac", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFractureMode));
    
    
    
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
    
    
    
    const MObject simulationInputs[] = {
        inDM, aCurrentTime, aStartTime, aEnabled, aGravity, aSplitImpulse,
        aIsFractureMode, aIsPlane, aPlanePosition, aPlaneAxis,
        aPlaneRestitution, aPlaneFriction
    };
    for (const MObject& input : simulationInputs) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outDM));
    }
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBulletFractureSolver", "initialize", std::current_exception());
}


    

    
    
    
    
    
    
    
    
    
    
    
    
