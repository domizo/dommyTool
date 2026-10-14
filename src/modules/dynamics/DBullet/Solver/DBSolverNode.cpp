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
//  DBSolverNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/26.
//

#include "DBSolverNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DPassiveTransformMath.h"
#include "DBulletEvaluation.h"
#include "PluginFeatures.h"
#include "common/file/DExportPath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DMayaUserNode.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DSequencePath.h"


#include <fstream>
#include <sstream>
#include <iterator>
#include <limits>
#include <map>
#include <cmath>

#include <maya/MDGMessage.h>
#include <maya/MSceneMessage.h>

//Bullet
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "LinearMath/btGeometryUtil.h"


//local Bullet
#include "dbSolver_t.h"
#include "DBRigidBodyNode.h"
#include "DBRigidBodyArrayNode.h"
#include "DBNailConstraintNode.h"
#include "DBHingeConstraintNode.h"
#include "DBSliderConstraintNode.h"
#include "DBSixdofConstraintNode.h"

#include "pdbIO.h"
#include "DBCollisionShapeNode.h"
#include "DBSoftBodyNode.h"
#include "dbSoftBody_t.h"
#include "DB_RigidBody_t.h"
#include "DB_BulletSolver_t.h"



float DBSolverNode::collisionMarginOffset; //mb
std::atomic<bool> DBSolverNode::s_isStartTime(false);
std::atomic<DBSolverNode*> DBSolverNode::s_activeSolver(nullptr);
MCallbackId DBSolverNode::updateSceneCallbackId = 0;
MCallbackId DBSolverNode::sceneLoadedCallbackId = 0;
#define ATTR_POSITION "position"
//#define ATTR_POSITION_TYPE VECTOR_ATTR
#define ATTR_VELOCITY "velocity"
//#define ATTR_VELOCITY_TYPE VECTOR_ATTR

#define ATTR_IN_RAXIS  "in_rotvec"               /* vector */
//#define ATTR_IN_RAXIS_TYPE VECTOR_ATTR
#define ATTR_IN_RANGLE "in_rotang"               /* float  */
//#define ATTR_IN_RANGLE_TYPE FLOAT_ATTR
#define ATTR_IN_ROTVEL "in_rotvel"               /* float  */
//#define ATTR_IN_ROTVEL_TYPE FLOAT_ATTR
#define ATTR_IN_RAXISNEXT  "in_rotvec_next"      /* vector */
//#define ATTR_IN_RAXISNEXT_TYPE VECTOR_ATTR
#define ATTR_IN_RANGLENEXT "in_rotang_next"      /* float  */
//#define ATTR_IN_RANGLENEXT_TYPE FLOAT_ATTR

namespace {

MStatus readVectorPlug(const MPlug& plug, vec3f& value)
{
    for (unsigned int component = 0U; component < 3U; ++component) {
        MStatus status;
        MPlug child = plug.child(component, &status);
        if (!status) {
            return status;
        }
        status = child.getValue(value[component]);
        if (!status) {
            return status;
        }
        if (!std::isfinite(value[component])) {
            return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

MStatus writeVectorPlug(const MPlug& plug, const vec3f& value)
{
    for (unsigned int component = 0U; component < 3U; ++component) {
        if (!std::isfinite(value[component])) {
            return MS::kInvalidParameter;
        }
        MStatus status;
        MPlug child = plug.child(component, &status);
        if (!status) {
            return status;
        }
        status = child.setValue(value[component]);
        if (!status) {
            return status;
        }
    }
    return MS::kSuccess;
}

bool isFinite(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool isFinite(const MQuaternion& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

bool isFinite(const vec3f& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
           std::isfinite(value[2]);
}

bool isFinite(const quatf& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
           std::isfinite(value[2]) && std::isfinite(value[3]);
}

bool checkedAccumulateVector(const MVector& value, MVector& total)
{
    if (!isFinite(value) || !isFinite(total)) {
        return false;
    }
    const double x = total.x + value.x;
    const double y = total.y + value.y;
    const double z = total.z + value.z;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return false;
    }
    total = MVector(x, y, z);
    return true;
}

bool checkedVectorToVec3f(const MVector& value, vec3f& converted)
{
    if (!isFinite(value)) {
        return false;
    }
    const double maximum = std::numeric_limits<float>::max();
    if (std::fabs(value.x) > maximum || std::fabs(value.y) > maximum ||
        std::fabs(value.z) > maximum) {
        return false;
    }
    converted = vec3f(
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z));
    return isFinite(converted);
}

using dommy::bullet::BulletWorldMutationGuard;

} // namespace

#pragma mark DPTransform


MTypeId DBSolverNode::typeId( ID_DB_SOLVER );
const MString DBSolverNode::typeName("DBSolverNode");

MObject DBSolverNode::inDP;
MObject DBSolverNode::inDM;
MObject DBSolverNode::aEnabled;
MObject DBSolverNode::aCurrentTime;
MObject DBSolverNode::aStartTime;


//solver attribute
MObject DBSolverNode::aGravity;
MObject DBSolverNode::aCollisionMargin;
MObject DBSolverNode::aDisableCollisionsBetweenLinkedBodies;
MObject DBSolverNode::aSplitImpulse;
MObject DBSolverNode::aSubsteps;
MObject DBSolverNode::aFixedPhysicsRate;

MObject DBSolverNode::aRigidBodies;
MObject DBSolverNode::aSoftBodies;
MObject DBSolverNode::aContactData;


MObject DBSolverNode::ia_DBG_DrawWireframe;
MObject DBSolverNode::ia_DBG_DrawAabb;
MObject DBSolverNode::ia_DBG_DrawContactPoints;
MObject DBSolverNode::ia_DBG_NoDeactivation;
MObject DBSolverNode::ia_DBG_ProfileTimings;
MObject DBSolverNode::ia_DBG_DrawConstraints;
MObject DBSolverNode::ia_DBG_DrawConstraintLimits;

MObject DBSolverNode::outDM;

DBSolverNode::DBSolverNode() {
    // Claiming the shared world is not successful scene initialization.
    m_reInitialize = true;
    m_debugDrawMode.store(1, std::memory_order_relaxed);
    m_requiresStartFrame = true;
    m_startFrameErrorReported = false;
    DBSolverNode* expectedOwner = nullptr;
    m_ownsSharedWorld = s_activeSolver.compare_exchange_strong(
        expectedOwner, this, std::memory_order_acq_rel);
}

DBSolverNode::~DBSolverNode() {
    if (m_ownsSharedWorld) {
        DBSolverNode* expectedOwner = this;
        s_activeSolver.compare_exchange_strong(
            expectedOwner, nullptr, std::memory_order_acq_rel);
    }
}

void* DBSolverNode::creator() {
    return ::dommy::maya::createObject("DBSolverNode", [&]() {
        return new DBSolverNode();
    });
}

void DBSolverNode::postConstructor() {
    // prevent deletion if all the rigidbodies are deleted
    setExistWithoutOutConnections(true);
}

MPxNode::SchedulingType DBSolverNode::schedulingType() const {
    // The Bullet world and several compatibility flags are process-global, and
    // this legacy solver still pulls connected-node plugs during evaluation.
    // Keep it out of Evaluation Manager scheduling until those dependencies are
    // represented as data-block inputs.
    return MPxNode::kUntrusted;
}

bool DBSolverNode::isStartFrame() noexcept {
    return s_isStartTime.load(std::memory_order_acquire);
}

bool DBSolverNode::setInternalValueInContext(const MPlug& plug, const MDataHandle& dataHandle,
                                             MDGContext& context) {
    (void)plug;
    (void)dataHandle;
    (void)context;
    return false;
}

void DBSolverNode::draw(M3dView& view, const MDagPath&, M3dView::DisplayStyle style,
                        M3dView::DisplayStatus status) try {
    if (s_activeSolver.load(std::memory_order_acquire) != this) return;
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    
    if( !(status == M3dView::kActive ||
          status == M3dView::kLead ||
          status == M3dView::kHilite ||
          ( style != M3dView::kGouraudShaded && style != M3dView::kFlatShaded )) ) {
        glColor3f(1.0, 1.0, 0.0);
    }
    dbSolver_t::debug_draw(
        m_debugDrawMode.load(std::memory_order_acquire));

}
DOMMY_DRAW_CATCH("DBSolverNode")

#pragma mark -
#pragma mark init method
// init the rigid bodies to it's first frame configuration
MStatus DBSolverNode::initRigidBodies(
    MPlugArray& rbConnections, MDataBlock& data)
{
    MStatus status;
    const MDataHandle collisionMargin =
        data.inputValue(aCollisionMargin, &status);
    if (!status) {
        return status;
    }
    DBSolverNode::collisionMarginOffset = collisionMargin.asFloat();
    if (!std::isfinite(DBSolverNode::collisionMarginOffset)) {
        return MS::kInvalidParameter;
    }

    for (unsigned int i = 0U; i < rbConnections.length(); ++i) {
        MObject node = rbConnections[i].node();
        MFnDependencyNode fnNode(node, &status);
        if (!status) {
            return status;
        }
        if (fnNode.typeId() == DBRigidBodyNode::typeId) {
            status = initRigidBody(node);
        } else if (fnNode.typeId() == DBRigidBodyArrayNode::typeId) {
            status = initRigidBodyArray(node);
        } else if (fnNode.typeId() == DBSoftBodyNode::typeId) {
            status = initSoftBody(node);
        } else {
            continue;
        }
        if (!status) {
            return status;
        }
    }

    for (unsigned int i = 0U; i < rbConnections.length(); ++i) {
        MObject node = rbConnections[i].node();
        MFnDependencyNode fnNode(node, &status);
        if (!status) {
            return status;
        }
        if (fnNode.typeId() == DBRigidBodyNode::typeId) {
            status = initConstraint(node);
            if (!status) {
                return status;
            }
        }
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::initRigidBody(MObject& node)
{
    MStatus status;
    MFnDagNode fnDagNode(node, &status);
    if (!status) {
        return status;
    }
    DBRigidBodyNode* rbNode =
        dommy::maya::checkedUserNode<DBRigidBodyNode>(
            fnDagNode, DBRigidBodyNode::typeId);
    if (!rbNode) {
        return MS::kFailure;
    }

    dbRigidBody_t::pointer rb;
    status = rbNode->evaluate_rigid_body(rb);
    if (!status) {
        return status;
    }
    if (!dbSolver_t::add_rigid_body(rb, rbNode->name().asChar())) {
        return MS::kFailure;
    }

    const unsigned int parentCount = fnDagNode.parentCount(&status);
    if (!status) {
        return status;
    }
    if (parentCount == 0U) {
        EchoWarning(name().asChar(), "Rigid body has no parent transform.");
        return MS::kFailure;
    }

    const MObject parent = fnDagNode.parent(0U, &status);
    if (!status) {
        return status;
    }
    MFnTransform fnTransform(parent, &status);
    if (!status) {
        return status;
    }

    MPlug plgMass(node, DBRigidBodyNode::ia_mass);
    float mass = 0.f;
    status = plgMass.getValue(mass);
    if (!status) {
        return status;
    }
    if (!std::isfinite(mass)) {
        return MS::kInvalidParameter;
    }
    if (mass > 0.f) {
        // active rigid body, set the world transform from the initial* attributes
        vec3f pos;
        const MPlug initialPosition(
            node, DBRigidBodyNode::ia_initialPosition);
        status = readVectorPlug(initialPosition, pos);
        if (!status) {
            return status;
        }
        
        vec3f rot;
        const MPlug initialRotation(
            node, DBRigidBodyNode::ia_initialRotation);
        status = readVectorPlug(initialRotation, rot);
        if (!status) {
            return status;
        }
        
        vec3f vel;
        const MPlug initialVelocity(
            node, DBRigidBodyNode::ia_initialVelocity);
        status = readVectorPlug(initialVelocity, vel);
        if (!status) {
            return status;
        }
        
        vec3f spin;
        const MPlug initialSpin(node, DBRigidBodyNode::ia_initialSpin);
        status = readVectorPlug(initialSpin, spin);
        if (!status) {
            return status;
        }
        
        const MEulerRotation meuler(
            deg2rad(rot[0]), deg2rad(rot[1]), deg2rad(rot[2]));
        const MQuaternion mquat = meuler.asQuaternion();
        if (!isFinite(mquat)) {
            return MS::kInvalidParameter;
        }
        rb->set_transform(pos, quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z));
        rb->set_linear_velocity(vel);
        rb->set_angular_velocity(spin);
        rb->set_kinematic(false);
        
        //rbNode->updateShape(plug, data, dSolverNode::collisionMarginOffset); //mb
        
        status = fnTransform.setRotation(meuler);
        if (!status) {
            return status;
        }
        status = fnTransform.setTranslation(
            MVector(pos[0], pos[1], pos[2]), MSpace::kTransform);
        if (!status) {
            return status;
        }
    } else {
        //passive rigid body, get the world trasform from from the shape node
        MQuaternion mquat;
        status = fnTransform.getRotation(mquat);
        if (!status || !isFinite(mquat)) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        const MVector mpos =
            fnTransform.getTranslation(MSpace::kTransform, &status);
        if (!status || !isFinite(mpos)) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        rb->set_transform(vec3f((float)mpos.x, (float)mpos.y, (float)mpos.z), quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z));
        rb->set_interpolation_transform(vec3f((float)mpos.x, (float)mpos.y, (float)mpos.z), quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z));
        rb->set_kinematic(true);
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::initRigidBodyArray(MObject& node)
{
    MStatus status;
    MFnDagNode fnDagNode(node, &status);
    if (!status) {
        return status;
    }
    
    DBRigidBodyArrayNode *rbNode =
        dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
            fnDagNode, DBRigidBodyArrayNode::typeId);
    if (!rbNode) {
        return MS::kFailure;
    }
    const std::vector<dbRigidBody_t::pointer>* evaluatedBodies = nullptr;
    status = rbNode->evaluate_rigid_bodies(evaluatedBodies);
    if (!status || evaluatedBodies == nullptr) {
        return status ? MStatus(MS::kFailure) : status;
    }
    const std::vector<dbRigidBody_t::pointer>& rbs = *evaluatedBodies;
    if (rbs.size() > dommy::generation::kMaxGeneratedTransformCount) {
        return MS::kInvalidParameter;
    }
    for (std::size_t index = 0U; index < rbs.size(); ++index) {
        if (!rbs[index] ||
            !dbSolver_t::add_rigid_body(
                rbs[index], rbNode->name().asChar())) {
            return MS::kFailure;
        }
    }
    
    const unsigned int parentCount = fnDagNode.parentCount(&status);
    if (!status) {
        return status;
    }
    if (parentCount == 0U) {
        EchoWarning(name().asChar(), "Rigid-body array has no parent transform.");
        return MS::kFailure;
    }
    
    MPlug plgMass(node, DBRigidBodyArrayNode::ia_mass);
    float mass = 0.f;
    status = plgMass.getValue(mass);
    if (!status) {
        return status;
    }
    if (!std::isfinite(mass)) {
        return MS::kInvalidParameter;
    }
    const bool active = mass > 0.f;
    
    if(active) {
        const MPlug initialPosition(
            node, DBRigidBodyArrayNode::ia_initialPosition);
        const MPlug initialRotation(
            node, DBRigidBodyArrayNode::ia_initialRotation);
        const MPlug initialVelocity(
            node, DBRigidBodyArrayNode::ia_initialVelocity);
        const MPlug initialSpin(node, DBRigidBodyArrayNode::ia_initialSpin);
        const MPlug positionPlug(node, DBRigidBodyArrayNode::io_position);
        const MPlug rotationPlug(node, DBRigidBodyArrayNode::io_rotation);

        for (std::size_t j = 0U; j < rbs.size(); ++j) {
            if (!rbs[j]) {
                return MS::kFailure;
            }
            const unsigned int logicalIndex = static_cast<unsigned int>(j);
            vec3f pos;
            const MPlug positionElement =
                initialPosition.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(positionElement, pos);
            if (!status) {
                return status;
            }
            
            vec3f rot;
            const MPlug rotationElement =
                initialRotation.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(rotationElement, rot);
            if (!status) {
                return status;
            }
            
            vec3f vel;
            const MPlug velocityElement =
                initialVelocity.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(velocityElement, vel);
            if (!status) {
                return status;
            }
            
            vec3f spin;
            const MPlug spinElement =
                initialSpin.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(spinElement, spin);
            if (!status) {
                return status;
            }
            
            const MEulerRotation meuler(
                deg2rad(rot[0]), deg2rad(rot[1]), deg2rad(rot[2]));
            const MQuaternion mquat = meuler.asQuaternion();
            if (!isFinite(mquat)) {
                return MS::kInvalidParameter;
            }
            
            rbs[j]->set_transform(pos, quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z));
            rbs[j]->set_linear_velocity(vel);
            rbs[j]->set_angular_velocity(spin);
            rbs[j]->set_kinematic(false);
            
            const MPlug outputPosition =
                positionPlug.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = writeVectorPlug(outputPosition, pos);
            if (!status) {
                return status;
            }

            const MPlug outputRotation =
                rotationPlug.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = writeVectorPlug(outputRotation, rot);
            if (!status) {
                return status;
            }
        }
        
    } else {
        const MPlug positionPlug(node, DBRigidBodyArrayNode::io_position);
        const MPlug rotationPlug(node, DBRigidBodyArrayNode::io_rotation);

        for (std::size_t j = 0U; j < rbs.size(); ++j) {
            if (!rbs[j]) {
                return MS::kFailure;
            }
            const unsigned int logicalIndex = static_cast<unsigned int>(j);
            vec3f pos;
            const MPlug positionElement =
                positionPlug.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(positionElement, pos);
            if (!status) {
                return status;
            }
            
            vec3f rot;
            const MPlug rotationElement =
                rotationPlug.elementByLogicalIndex(logicalIndex, &status);
            if (!status) {
                return status;
            }
            status = readVectorPlug(rotationElement, rot);
            if (!status) {
                return status;
            }
            
            const MEulerRotation meuler(
                deg2rad(rot[0]), deg2rad(rot[1]), deg2rad(rot[2]));
            const MQuaternion mquat = meuler.asQuaternion();
            if (!isFinite(mquat)) {
                return MS::kInvalidParameter;
            }
            const quatf rotation(
                (float)mquat.w, (float)mquat.x,
                (float)mquat.y, (float)mquat.z);
            rbs[j]->set_transform(pos, rotation);
            rbs[j]->set_interpolation_transform(pos, rotation);
            rbs[j]->set_kinematic(true);
        }
    }
    return MS::kSuccess;
}



MStatus DBSolverNode::initSoftBody(MObject& node)
{
    MStatus status;
    MFnDagNode fnDagNode(node, &status);
    if (!status) {
        return status;
    }
    
    DBSoftBodyNode *sbNode =
        dommy::maya::checkedUserNode<DBSoftBodyNode>(
            fnDagNode, DBSoftBodyNode::typeId);
    if (!sbNode) {
        return MS::kFailure;
    }
    const MPlug inputMesh(node, DBSoftBodyNode::inputMesh);
    MObject upd;
    status = inputMesh.getValue(upd);
    if (!status) return status;
    const bool inputConnected = inputMesh.isConnected(&status);
    if (!status) return status;
    if (!inputConnected) return MS::kFailure;

    MPlugArray inputConnections;
    const bool hasInputConnection = inputMesh.connectedTo(
        inputConnections, true, false, &status);
    if (!status) {
        return status;
    }
    if (!hasInputConnection || inputConnections.length() == 0U ||
        !inputConnections[0U].node().hasFn(MFn::kMesh)) {
        return MS::kFailure;
    }

    MFnMesh inputMeshFn(inputConnections[0U].node(), &status);
    if (!status) {
        return status;
    }

    const MPlug outputMesh(node, DBSoftBodyNode::outputMesh);
    MObject update;
    status = outputMesh.getValue(update);
    if (!status) return status;
    const bool outputConnected = outputMesh.isConnected(&status);
    if (!status) return status;
    if (!outputConnected) return MS::kFailure;

    MPlugArray outputConnections;
    const bool hasOutputConnection = outputMesh.connectedTo(
        outputConnections, false, true, &status);
    if (!status) {
        return status;
    }
    if (!hasOutputConnection || outputConnections.length() == 0U ||
        !outputConnections[0U].node().hasFn(MFn::kMesh)) {
        return MS::kFailure;
    }
    MFnMesh outputMeshFn(outputConnections[0U].node(), &status);
    if (!status) {
        return status;
    }

    std::vector<int> triVertIndices;
    std::vector<float> triVertCoords;

    status = DBSoftBodyNode::createHelperMesh(
        inputMeshFn, triVertIndices, triVertCoords, MSpace::kTransform);
    if (!status) {
        return status;
    }
    if (triVertCoords.empty() || triVertCoords.size() % 3U != 0U ||
        triVertCoords.size() / 3U >
            dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }
    for (std::size_t index = 0U; index < triVertCoords.size(); ++index) {
        if (!std::isfinite(triVertCoords[index])) {
            return MS::kInvalidParameter;
        }
    }
    const std::size_t vertexCount = triVertCoords.size() / 3U;
    for (std::size_t index = 0U; index < triVertIndices.size(); ++index) {
        if (triVertIndices[index] < 0 ||
            static_cast<std::size_t>(triVertIndices[index]) >= vertexCount) {
            return MS::kInvalidParameter;
        }
    }

    MFloatPointArray newPoints;
    status = newPoints.setLength(static_cast<unsigned int>(vertexCount));
    if (!status) {
        return status;
    }
    for (std::size_t index = 0U; index < vertexCount; ++index) {
        newPoints[static_cast<unsigned int>(index)] = MFloatPoint(
            triVertCoords[index * 3U],
            triVertCoords[index * 3U + 1U],
            triVertCoords[index * 3U + 2U],
            1.0F);
    }

    dommy::bullet::SoftBodyParameters parameters;
    status = sbNode->readSoftBodyParameters(parameters);
    if (!status) return status;
    dbSoftBody_t::pointer newSoftBody =
        dbSolver_t::create_soft_body(triVertCoords, triVertIndices, parameters);
    if (!newSoftBody) {
        return MS::kFailure;
    }

    dbSoftBody_t::pointer previousSoftBody = sbNode->cached_soft_body();
    if (!dbSolver_t::add_soft_body(
            newSoftBody, sbNode->name().asChar())) {
        return MS::kFailure;
    }
    if (previousSoftBody) {
        dbSolver_t::remove_soft_body(previousSoftBody);
    }
    sbNode->set_soft_body(newSoftBody);

    status = outputMeshFn.setPoints(newPoints, MSpace::kTransform);
    if (!status) {
        dbSolver_t::remove_soft_body(newSoftBody);
        // A cached body from the previous world cannot be restored after a
        // StartTime rebuild. Same-world rollback remains supported.
        if (previousSoftBody && !previousSoftBody->valid()) previousSoftBody.reset();
        if (previousSoftBody) {
            if (!dbSolver_t::add_soft_body(
                    previousSoftBody, sbNode->name().asChar())) {
                sbNode->set_soft_body(dbSoftBody_t::pointer());
                return MS::kFailure;
            }
        }
        sbNode->set_soft_body(previousSoftBody);
        return status;
    }

    return MS::kSuccess;
}


MStatus DBSolverNode::initConstraint(MObject& bodyNode)
{
    MStatus status;
    MFnDagNode fnDagNode(bodyNode, &status);
    if (!status) {
        return status;
    }
    MPlug plgMessages(bodyNode, DBRigidBodyNode::message);
    MPlugArray rbMsgConnections;
    plgMessages.connectedTo(
        rbMsgConnections, false, true, &status);
    if (!status) {
        return status;
    }
    for (unsigned int j = 0U; j < rbMsgConnections.length(); ++j)
    {
        MObject msgNode = rbMsgConnections[j].node();
        MFnDagNode msgDagNode(msgNode, &status);
        if (!status) {
            return status;
        }
        if(msgDagNode.typeId() == DBNailConstraintNode::typeId)
        {
            DBNailConstraintNode* nailNode =
                dommy::maya::checkedUserNode<DBNailConstraintNode>(
                    msgDagNode, DBNailConstraintNode::typeId);
            if (!nailNode) {
                return MS::kFailure;
            }
            
            const unsigned int parentCount =
                msgDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U)
            {
                EchoWarning(name().asChar(), "Nail constraint has no parent transform.");
                return MS::kFailure;
            }
            const MObject parent = msgDagNode.parent(0U, &status);
            if (!status) {
                return status;
            }
            MFnTransform msgTransform(parent, &status);
            if (!status) {
                return status;
            }
            dbNail_constraint_t::pointer nail;
            status = nailNode->evaluate_constraint(nail);
            if (!status) {
                return status;
            }
            bool disableCollisions = false;
            status = MPlug(
                msgNode, DBNailConstraintNode::ia_disableCollide)
                .getValue(disableCollisions);
            if (!status) {
                return status;
            }
            dbConstraint_t::pointer nailBase = nail;
            if (!dbSolver_t::add_constraint(
                    nailBase, disableCollisions)) {
                return MS::kFailure;
            }
            vec3f constrPos;
            nail->get_world(constrPos);
            if (!std::isfinite(constrPos[0]) ||
                !std::isfinite(constrPos[1]) ||
                !std::isfinite(constrPos[2])) {
                return MS::kInvalidParameter;
            }
            nail->set_enabled(true);
            status = msgTransform.setTranslation(
                MVector(constrPos[0], constrPos[1], constrPos[2]),
                MSpace::kTransform);
            if (!status) {
                return status;
            }
            status = msgTransform.setRotation(MEulerRotation(0., 0., 0.));
            if (!status) {
                return status;
            }
        }
        if(msgDagNode.typeId() == DBHingeConstraintNode::typeId)
        {
            DBHingeConstraintNode* hingeNode =
                dommy::maya::checkedUserNode<DBHingeConstraintNode>(
                    msgDagNode, DBHingeConstraintNode::typeId);
            if (!hingeNode) {
                return MS::kFailure;
            }
            const unsigned int parentCount =
                msgDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U)
            {
                EchoWarning(name().asChar(), "Hinge constraint has no parent transform.");
                return MS::kFailure;
            }
            const MObject parent = msgDagNode.parent(0U, &status);
            if (!status) {
                return status;
            }
            MFnTransform msgTransform(parent, &status);
            if (!status) {
                return status;
            }
            dbHinge_constraint_t::pointer hinge;
            status = hingeNode->evaluate_constraint(hinge);
            if (!status) {
                return status;
            }
            bool disableCollisions = false;
            status = MPlug(
                msgNode, DBHingeConstraintNode::ia_disableCollide)
                .getValue(disableCollisions);
            if (!status) {
                return status;
            }
            dbConstraint_t::pointer hingeBase = hinge;
            if (!dbSolver_t::add_constraint(
                    hingeBase, disableCollisions)) {
                return MS::kFailure;
            }
            vec3f constrPos;
            quatf constrRot;
            hinge->get_world(constrPos, constrRot);
            const MVector translation(
                constrPos[0], constrPos[1], constrPos[2]);
            const MQuaternion rotation(
                constrRot[1], constrRot[2], constrRot[3], constrRot[0]);
            if (!isFinite(translation) || !isFinite(rotation)) {
                return MS::kInvalidParameter;
            }
            hinge->set_enabled(true); //re-enables constraint if broken dynamically
            status = msgTransform.setTranslation(
                translation, MSpace::kTransform);
            if (!status) {
                return status;
            }
            status = msgTransform.setRotation(rotation);
            if (!status) {
                return status;
            }
        }
        if(msgDagNode.typeId() == DBSliderConstraintNode::typeId)
        {
            DBSliderConstraintNode* sliderNode =
                dommy::maya::checkedUserNode<DBSliderConstraintNode>(
                    msgDagNode, DBSliderConstraintNode::typeId);
            if (!sliderNode) {
                return MS::kFailure;
            }
            
            const unsigned int parentCount =
                msgDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U)
            {
                EchoWarning(name().asChar(), "Slider constraint has no parent transform.");
                return MS::kFailure;
            }
            const MObject parent = msgDagNode.parent(0U, &status);
            if (!status) {
                return status;
            }
            MFnTransform msgTransform(parent, &status);
            if (!status) {
                return status;
            }
            dbSlider_constraint_t::pointer slider;
            status = sliderNode->evaluate_constraint(slider);
            if (!status) {
                return status;
            }
            bool disableCollisions = false;
            status = MPlug(
                msgNode, DBSliderConstraintNode::ia_disableCollide)
                .getValue(disableCollisions);
            if (!status) {
                return status;
            }
            dbConstraint_t::pointer sliderBase = slider;
            if (!dbSolver_t::add_constraint(
                    sliderBase, disableCollisions)) {
                return MS::kFailure;
            }
            vec3f constrPos;
            quatf constrRot;
            slider->get_world(constrPos, constrRot);
            const MVector translation(
                constrPos[0], constrPos[1], constrPos[2]);
            const MQuaternion rotation(
                constrRot[1], constrRot[2], constrRot[3], constrRot[0]);
            if (!isFinite(translation) || !isFinite(rotation)) {
                return MS::kInvalidParameter;
            }
            slider->set_enabled(true); //re-enables constraint if broken dynamically
            status = msgTransform.setTranslation(
                translation, MSpace::kTransform);
            if (!status) {
                return status;
            }
            status = msgTransform.setRotation(rotation);
            if (!status) {
                return status;
            }
            
        }
        if(msgDagNode.typeId() == DBSixdofConstraintNode::typeId)
        {
            DBSixdofConstraintNode* sixdofNode =
                dommy::maya::checkedUserNode<DBSixdofConstraintNode>(
                    msgDagNode, DBSixdofConstraintNode::typeId);
            if (!sixdofNode) {
                return MS::kFailure;
            }
            
            const unsigned int parentCount =
                msgDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U)
            {
                EchoWarning(name().asChar(), "Six-DOF constraint has no parent transform.");
                return MS::kFailure;
            }
            const MObject parent = msgDagNode.parent(0U, &status);
            if (!status) {
                return status;
            }
            MFnTransform msgTransform(parent, &status);
            if (!status) {
                return status;
            }
            dbSixdof_constraint_t::pointer sixdof;
            status = sixdofNode->evaluate_constraint(sixdof);
            if (!status) {
                return status;
            }
            bool disableCollisions = false;
            status = MPlug(
                msgNode, DBSixdofConstraintNode::ia_disableCollide)
                .getValue(disableCollisions);
            if (!status) {
                return status;
            }
            dbConstraint_t::pointer sixdofBase = sixdof;
            if (!dbSolver_t::add_constraint(
                    sixdofBase, disableCollisions)) {
                return MS::kFailure;
            }
            vec3f constrPos;
            quatf constrRot;
            sixdof->get_world(constrPos, constrRot);
            const MVector translation(
                constrPos[0], constrPos[1], constrPos[2]);
            const MQuaternion rotation(
                constrRot[1], constrRot[2], constrRot[3], constrRot[0]);
            if (!isFinite(translation) || !isFinite(rotation)) {
                return MS::kInvalidParameter;
            }
            sixdof->set_enabled(true); //re-enables constraint if broken dynamically
            status = msgTransform.setTranslation(
                translation, MSpace::kTransform);
            if (!status) {
                return status;
            }
            status = msgTransform.setRotation(rotation);
            if (!status) {
                return status;
            }
        }
    }
    return MS::kSuccess;
}


#pragma mark -
#pragma mark update method

//update the scene after a simulation step

MStatus DBSolverNode::updateActiveRigidBodies(
    MPlugArray &rbConnections)
{
    MStatus status;
    //update the active rigid bodies to the new configuration
    for(unsigned int i = 0; i < rbConnections.length(); ++i) {
        
        MObject node = rbConnections[i].node();
        MFnDagNode fnDagNode(node, &status);
        if (!status) return status;
        if(fnDagNode.typeId() == DBRigidBodyNode::typeId) {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) return MS::kFailure;
            dbRigidBody_t::pointer rb = rbNode->cached_rigid_body();
            if (!rb) return MS::kFailure;
            {
                const unsigned int parentCount = fnDagNode.parentCount(&status);
                if (!status) return status;
                if (parentCount == 0) return MS::kFailure;
                
                const MObject parent = fnDagNode.parent(0, &status);
                if (!status) return status;
                MFnTransform fnTransform(parent, &status);
                if (!status) return status;
                
                MPlug plgMass(node, DBRigidBodyNode::ia_mass);
                float mass = 0.f;
                status = plgMass.getValue(mass);
                if (!status || !std::isfinite(mass)) {
                    return status ? MS::kInvalidParameter : status;
                }
                bool active = (mass>0.f);
                if(active) {
                    quatf rot;
                    vec3f pos;
                    rb->get_transform(pos, rot);
                    if (!isFinite(pos) || !isFinite(rot)) return MS::kInvalidParameter;
                    status = fnTransform.setRotation(
                        MQuaternion(rot[1], rot[2], rot[3], rot[0]));
                    if (!status) return status;
                    status = fnTransform.setTranslation(
                        MVector(pos[0], pos[1], pos[2]),
                        MSpace::kTransform);
                    if (!status) return status;
                }
            }
            
            status = updateConstraint(node);
            if (!status) return status;
            
        } else if(fnDagNode.typeId() == DBRigidBodyArrayNode::typeId) {
            DBRigidBodyArrayNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
                    fnDagNode, DBRigidBodyArrayNode::typeId);
            if (!rbNode) return MS::kFailure;
            const std::vector<dbRigidBody_t::pointer>& rbs =
                rbNode->cached_rigid_bodies();
            if (rbs.size() > dommy::generation::kMaxGeneratedTransformCount)
                return MS::kInvalidParameter;
            
            MPlug plgMass(node, DBRigidBodyArrayNode::ia_mass);
            float mass = 0.f;
            status = plgMass.getValue(mass);
            if (!status || !std::isfinite(mass)) {
                return status ? MS::kInvalidParameter : status;
            }
            bool active = (mass>0.f);
            //write the position and rotations
            if(active) {
                MPlug plgPosition(node, DBRigidBodyArrayNode::io_position);
                MPlug plgRotation(node, DBRigidBodyArrayNode::io_rotation);
                
                MPlug plgElement;
                for(size_t j = 0; j < rbs.size(); ++j) {
                    if (!rbs[j]) return MS::kFailure;
                    vec3f pos;
                    quatf rot;
                    rbs[j]->get_transform(pos, rot);
                    if (!isFinite(pos) || !isFinite(rot)) return MS::kInvalidParameter;
                    
                    MEulerRotation meuler(MQuaternion(rot[1], rot[2], rot[3], rot[0]).asEulerRotation());
                    const MVector rotationDegrees(rad2deg(meuler.x), rad2deg(meuler.y), rad2deg(meuler.z));
                    if (!isFinite(rotationDegrees)) return MS::kInvalidParameter;
                    
                    plgElement = plgPosition.elementByLogicalIndex(
                        static_cast<unsigned int>(j), &status);
                    if (!status) return status;
                    status = plgElement.child(0).setValue(pos[0]);
                    if (!status) return status;
                    status = plgElement.child(1).setValue(pos[1]);
                    if (!status) return status;
                    status = plgElement.child(2).setValue(pos[2]);
                    if (!status) return status;
                    
                    plgElement = plgRotation.elementByLogicalIndex(
                        static_cast<unsigned int>(j), &status);
                    if (!status) return status;
                    status = plgElement.child(0).setValue(
                        rotationDegrees.x);
                    if (!status) return status;
                    status = plgElement.child(1).setValue(
                        rotationDegrees.y);
                    if (!status) return status;
                    status = plgElement.child(2).setValue(
                        rotationDegrees.z);
                    if (!status) return status;
                    
                }
            }
            
            // File output is opt-in at build time because this method runs
            // during dependency-graph evaluation.
#if DOMMY_WITH_DG_FILE_WRITERS
            MPlug plgFileIO(node, DBRigidBodyArrayNode::ia_fileIO);
            bool doIO = false;
            if (plgFileIO.getValue(doIO) && doIO) {
                dumpRigidBodyArray(node);
            }
#endif
        }
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::updateActiveSoftBodies(
    MPlugArray &sbConnections)
{
    for(unsigned int i = 0; i < sbConnections.length(); ++i)
    {
        MStatus status;
        MObject node = sbConnections[i].node();
        MFnDagNode fnDagNode(node, &status);
        if (!status) return status;
        if(fnDagNode.typeId() == DBSoftBodyNode::typeId)
        {
            DBSoftBodyNode *sbNode =
                dommy::maya::checkedUserNode<DBSoftBodyNode>(
                    fnDagNode, DBSoftBodyNode::typeId);
            if (!sbNode) {
                return MS::kFailure;
            }
            dbSoftBody_t::pointer sb = sbNode->cached_soft_body();
            if (!sb || !sb->valid()) {
                return MS::kFailure;
            }
            
            MPlug plgOutputMesh(sbNode->thisMObject(), DBSoftBodyNode::outputMesh);
            MObject update;
            
            //force evaluation of the shape
            status = plgOutputMesh.getValue(update);
            if (!status) return status;
            MPlugArray connections;
            const bool connected = plgOutputMesh.connectedTo(
                connections, false, true, &status);
            if (!status) return status;
            // An intentionally disconnected output needs no mesh write.
            // A failed query or a connected non-mesh target is not success.
            if (!connected && connections.length() == 0U) {
                continue;
            }
            if (!connected || connections.length() == 0U ||
                !connections[0].node().hasFn(MFn::kMesh)) {
                return MS::kFailure;
            }

            MFnMesh meshFn(connections[0].node(), &status);
            if (status != MS::kSuccess) {
                return status;
            }
            MFloatPointArray newPoints;
            std::vector<int> triIndices;
            std::vector<vec3f> triCoords;
            sb->get_mesh(triIndices, triCoords);

            for(std::size_t j = 0; j < triCoords.size(); ++j)
            {
                if (!std::isfinite(triCoords[j][0]) ||
                    !std::isfinite(triCoords[j][1]) ||
                    !std::isfinite(triCoords[j][2])) {
                    return MS::kInvalidParameter;
                }
                status = newPoints.append(
                    triCoords[j][0],
                    triCoords[j][1],
                    triCoords[j][2],
                    1.0f);
                if (!status) return status;
            }
            
            status = meshFn.setPoints(
                newPoints, MSpace::kTransform);
            if (!status) return status;
        }
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::updateConstraint(MObject& bodyNode)
{
    MStatus status;
    MPlug plgMessages(bodyNode, DBRigidBodyNode::message);
    MPlugArray rbMsgConnections;
    plgMessages.connectedTo(
        rbMsgConnections, false, true, &status);
    if (!status) {
        return status;
    }
    for (unsigned int j = 0; j < rbMsgConnections.length(); ++j) {
        MObject msgNode = rbMsgConnections[j].node();
        MFnDagNode msgDagNode(msgNode, &status);
        if (!status) return status;
        const MTypeId type = msgDagNode.typeId(&status);
        if (!status) return status;
        const bool isNail = type == DBNailConstraintNode::typeId;
        if (!isNail && type != DBHingeConstraintNode::typeId &&
            type != DBSliderConstraintNode::typeId &&
            type != DBSixdofConstraintNode::typeId) continue;

        vec3f constrPos;
        quatf constrRot(1.f, 0.f, 0.f, 0.f);
        if (isNail) {
            DBNailConstraintNode* nailNode =
                dommy::maya::checkedUserNode<DBNailConstraintNode>(
                    msgDagNode, DBNailConstraintNode::typeId);
            if (!nailNode) return MS::kFailure;
            const dbNail_constraint_t::pointer nail =
                nailNode->cached_constraint();
            if (!nail) return MS::kFailure;
            nail->get_world(constrPos);
        } else if (type == DBHingeConstraintNode::typeId) {
            DBHingeConstraintNode* hingeNode =
                dommy::maya::checkedUserNode<DBHingeConstraintNode>(
                    msgDagNode, DBHingeConstraintNode::typeId);
            if (!hingeNode) return MS::kFailure;
            const dbHinge_constraint_t::pointer hinge =
                hingeNode->cached_constraint();
            if (!hinge) return MS::kFailure;
            hinge->get_world(constrPos, constrRot);
        } else if (type == DBSliderConstraintNode::typeId) {
            DBSliderConstraintNode* sliderNode =
                dommy::maya::checkedUserNode<DBSliderConstraintNode>(
                    msgDagNode, DBSliderConstraintNode::typeId);
            if (!sliderNode) return MS::kFailure;
            const dbSlider_constraint_t::pointer slider =
                sliderNode->cached_constraint();
            if (!slider) return MS::kFailure;
            slider->get_world(constrPos, constrRot);
        } else {
            DBSixdofConstraintNode* sixdofNode =
                dommy::maya::checkedUserNode<DBSixdofConstraintNode>(
                    msgDagNode, DBSixdofConstraintNode::typeId);
            if (!sixdofNode) return MS::kFailure;
            const dbSixdof_constraint_t::pointer sixdof =
                sixdofNode->cached_constraint();
            if (!sixdof) return MS::kFailure;
            sixdof->get_world(constrPos, constrRot);
        }

        // Validate the entire frame before either host write. Native frame
        // composition can overflow even when the original inputs were finite.
        if (!isFinite(constrPos) || !isFinite(constrRot)) return MS::kInvalidParameter;
        const unsigned int parentCount = msgDagNode.parentCount(&status);
        if (!status) return status;
        if (parentCount == 0) return MS::kFailure;
        const MObject parent = msgDagNode.parent(0, &status);
        if (!status) return status;
        MFnTransform msgTransform(parent, &status);
        if (!status) return status;
        status = msgTransform.setTranslation(
            MVector(constrPos[0], constrPos[1], constrPos[2]),
            MSpace::kTransform);
        if (!status) return status;
        status = isNail
            ? msgTransform.setRotation(MEulerRotation(0., 0., 0.))
            : msgTransform.setRotation(MQuaternion(
                constrRot[1], constrRot[2], constrRot[3], constrRot[0]));
        if (!status) return status;
    }
    return MS::kSuccess;
}




MStatus DBSolverNode::gatherPassiveTransforms(
    MPlugArray& rbConnections, std::vector<xforms_t>& xforms)
{
    xforms.clear();
    MStatus status;

    for (unsigned int i = 0U; i < rbConnections.length(); ++i) {
        MObject node = rbConnections[i].node();
        MFnDagNode fnDagNode(node, &status);
        if (!status) {
            return status;
        }
        if(fnDagNode.typeId() == DBRigidBodyNode::typeId) {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            dbRigidBody_t::pointer rb = rbNode->cached_rigid_body();
            if (!rb) {
                return MS::kFailure;
            }

            const unsigned int parentCount =
                fnDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U) {
                EchoWarning(name().asChar(), "Connected rigid body has no parent transform.");
                return MS::kFailure;
            }

            MPlug plgMass(node, DBRigidBodyNode::ia_mass);
            float mass = 0.f;
            status = plgMass.getValue(mass);
            if (!status) {
                return status;
            }
            if (!std::isfinite(mass)) {
                return MS::kInvalidParameter;
            }
            const bool active = mass > 0.f;
            if(!active) {
                const MObject parent = fnDagNode.parent(0U, &status);
                if (!status) {
                    return status;
                }
                MFnTransform fnTransform(parent, &status);
                if (!status) {
                    return status;
                }

                MQuaternion mquat;
                status = fnTransform.getRotation(mquat);
                if (!status || !isFinite(mquat)) {
                    return status ? MStatus(MS::kInvalidParameter) : status;
                }
                const MVector mpos =
                    fnTransform.getTranslation(MSpace::kTransform, &status);
                if (!status || !isFinite(mpos)) {
                    return status ? MStatus(MS::kInvalidParameter) : status;
                }

                xforms_t xform;
                rb->get_transform(xform.m_x0, xform.m_q0);
                if (!isFinite(xform.m_x0) || !isFinite(xform.m_q0)) {
                    return MS::kInvalidParameter;
                }
                
                xform.m_x1 = vec3f((float)mpos.x, (float)mpos.y, (float)mpos.z);
                xform.m_q1 = quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z);
                xforms.push_back(xform);
            }
        } else if(fnDagNode.typeId() == DBRigidBodyArrayNode::typeId) {
            DBRigidBodyArrayNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
                    fnDagNode, DBRigidBodyArrayNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            const std::vector<dbRigidBody_t::pointer>& rbs =
                rbNode->cached_rigid_bodies();
            if (rbs.size() >
                dommy::generation::kMaxGeneratedTransformCount) {
                return MS::kInvalidParameter;
            }

            const unsigned int parentCount =
                fnDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U) {
                EchoWarning(name().asChar(), "Connected rigid body has no parent transform.");
                return MS::kFailure;
            }
            
            MPlug plgMass(node, DBRigidBodyArrayNode::ia_mass);
            float mass = 0.f;
            status = plgMass.getValue(mass);
            if (!status) {
                return status;
            }
            if (!std::isfinite(mass)) {
                return MS::kInvalidParameter;
            }
            const bool active = mass > 0.f;
            if(!active) {
                const MPlug positionPlug(
                    node, DBRigidBodyArrayNode::io_position);
                const MPlug rotationPlug(
                    node, DBRigidBodyArrayNode::io_rotation);

                for (std::size_t j = 0U; j < rbs.size(); ++j) {
                    if (!rbs[j]) {
                        return MS::kFailure;
                    }
                    xforms_t xform;
                    rbs[j]->get_transform(xform.m_x0, xform.m_q0);
                    if (!isFinite(xform.m_x0) ||
                        !isFinite(xform.m_q0)) {
                        return MS::kInvalidParameter;
                    }

                    const unsigned int logicalIndex =
                        static_cast<unsigned int>(j);
                    const MPlug positionElement =
                        positionPlug.elementByLogicalIndex(
                            logicalIndex, &status);
                    if (!status) {
                        return status;
                    }
                    status = readVectorPlug(positionElement, xform.m_x1);
                    if (!status) {
                        return status;
                    }
                    
                    vec3f rot;
                    const MPlug rotationElement =
                        rotationPlug.elementByLogicalIndex(
                            logicalIndex, &status);
                    if (!status) {
                        return status;
                    }
                    status = readVectorPlug(rotationElement, rot);
                    if (!status) {
                        return status;
                    }
                    
                    const MEulerRotation meuler(
                        deg2rad(rot[0]), deg2rad(rot[1]), deg2rad(rot[2]));
                    const MQuaternion mquat = meuler.asQuaternion();
                    if (!isFinite(mquat)) {
                        return MS::kInvalidParameter;
                    }
                    xform.m_q1 = quatf((float)mquat.w, (float)mquat.x, (float)mquat.y, (float)mquat.z);
                    xforms.push_back(xform);
                }
            }
        }
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::updatePassiveRigidBodies(
    MPlugArray& rbConnections, const std::vector<xforms_t>& xforms, float t)
{
    if (!std::isfinite(t)) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    std::size_t passiveBodyIndex = 0U;
    for (unsigned int i = 0U; i < rbConnections.length(); ++i) {
        MObject node = rbConnections[i].node();
        MFnDagNode fnDagNode(node, &status);
        if (!status) {
            return status;
        }
        if(fnDagNode.typeId() == DBRigidBodyNode::typeId) {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            dbRigidBody_t::pointer rb = rbNode->cached_rigid_body();
            if (!rb) {
                return MS::kFailure;
            }

            const unsigned int parentCount =
                fnDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U) {
                EchoWarning(name().asChar(), "Connected rigid body has no parent transform.");
                return MS::kFailure;
            }
            
            MPlug plgMass(node, DBRigidBodyNode::ia_mass);
            float mass = 0.f;
            status = plgMass.getValue(mass);
            if (!status) {
                return status;
            }
            if (!std::isfinite(mass)) {
                return MS::kInvalidParameter;
            }
            const bool active = mass > 0.f;
            if(!active) {
                if (passiveBodyIndex >= xforms.size()) {
                    return MS::kFailure;
                }
                vec3f position;
                quatf rotation;
                if (!dommy::bullet::interpolatePassiveTransform(
                        xforms[passiveBodyIndex].m_x0,
                        xforms[passiveBodyIndex].m_x1,
                        xforms[passiveBodyIndex].m_q0,
                        xforms[passiveBodyIndex].m_q1,
                        t, position, rotation)) {
                    return MS::kInvalidParameter;
                }
                rb->set_transform(position, rotation);
                rb->set_interpolation_transform(position, rotation);
                ++passiveBodyIndex;
            }
        } else if(fnDagNode.typeId() == DBRigidBodyArrayNode::typeId) {
            DBRigidBodyArrayNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
                    fnDagNode, DBRigidBodyArrayNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            const std::vector<dbRigidBody_t::pointer>& rbs =
                rbNode->cached_rigid_bodies();
            if (rbs.size() >
                dommy::generation::kMaxGeneratedTransformCount) {
                return MS::kInvalidParameter;
            }

            const unsigned int parentCount =
                fnDagNode.parentCount(&status);
            if (!status) {
                return status;
            }
            if (parentCount == 0U) {
                EchoWarning(name().asChar(), "Connected rigid body has no parent transform.");
                return MS::kFailure;
            }
            
            MPlug plgMass(node, DBRigidBodyArrayNode::ia_mass);
            float mass = 0.f;
            status = plgMass.getValue(mass);
            if (!status) {
                return status;
            }
            if (!std::isfinite(mass)) {
                return MS::kInvalidParameter;
            }
            const bool active = mass > 0.f;
            if(!active) {
                for (std::size_t j = 0U; j < rbs.size(); ++j) {
                    if (!rbs[j] || passiveBodyIndex >= xforms.size()) {
                        return MS::kFailure;
                    }
                    vec3f position;
                    quatf rotation;
                    if (!dommy::bullet::interpolatePassiveTransform(
                            xforms[passiveBodyIndex].m_x0,
                            xforms[passiveBodyIndex].m_x1,
                            xforms[passiveBodyIndex].m_q0,
                            xforms[passiveBodyIndex].m_q1,
                            t, position, rotation)) {
                        return MS::kInvalidParameter;
                    }
                    rbs[j]->set_transform(position, rotation);
                    rbs[j]->set_interpolation_transform(position, rotation);
                    ++passiveBodyIndex;
                }
            }
        }
    }
    return passiveBodyIndex == xforms.size()
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kFailure);
}

MStatus DBSolverNode::clearContactRelatedAttributes(
    MPlugArray &rbConnections) {
    
    for(size_t i = 0; i < rbConnections.length(); ++i) {
        
        MObject node = rbConnections[i].node();
        MStatus status;
        MFnDagNode fnDagNode(node, &status);
        if (!status) return status;
        
        if(fnDagNode.typeId() == DBRigidBodyNode::typeId) {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) return MS::kFailure;
            status = rbNode->clearContactInfo();
            if (!status) return status;
        }
    }
    return MS::kSuccess;
}

#pragma mark -
#pragma mark dynamics field
MStatus DBSolverNode::applyFields(MPlugArray& rbConnections, float dt,
                                dbSolver_t::SoftBodyForces& sampledSoftForces)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return MS::kInvalidParameter;
    }

    MVectorArray position;
    MVectorArray velocity;
    MDoubleArray mass;

    std::vector<dbRigidBody_t*> rigid_bodies;
    std::vector<dbSoftBody_t::pointer> soft_bodies;
    std::map<dbRigidBody_t*, std::size_t> rigidBodyIndices;

    // Gather active bodies without changing their force state.
    for(size_t i = 0; i < rbConnections.length(); ++i) {
        MObject node = rbConnections[i].node();
        MStatus status;
        MFnDagNode fnDagNode(node, &status);
        if (!status) {
            return status;
        }

        if(fnDagNode.typeId() == DBRigidBodyNode::typeId) {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            dbRigidBody_t::pointer rb = rbNode->cached_rigid_body();
            if (!rb) {
                return MS::kFailure;
            }
            const float bodyMass = rb->get_mass();
            if (!std::isfinite(bodyMass)) {
                return MS::kInvalidParameter;
            }
            if (bodyMass > 0.0f &&
                rigidBodyIndices.find(rb.get()) == rigidBodyIndices.end()) {
                const std::size_t bodyIndex = rigid_bodies.size();
                rigid_bodies.push_back(rb.get());
                rigidBodyIndices.insert(
                    std::make_pair(rb.get(), bodyIndex));
            }
        } else if(fnDagNode.typeId() == DBRigidBodyArrayNode::typeId) {
            DBRigidBodyArrayNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
                    fnDagNode, DBRigidBodyArrayNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            const std::vector<dbRigidBody_t::pointer>& rbs =
                rbNode->cached_rigid_bodies();
            if (rbs.size() >
                dommy::generation::kMaxGeneratedTransformCount) {
                return MS::kInvalidParameter;
            }
            for(size_t j = 0; j < rbs.size(); ++j) {
                if (!rbs[j]) {
                    return MS::kFailure;
                }
                const float bodyMass = rbs[j]->get_mass();
                if (!std::isfinite(bodyMass)) {
                    return MS::kInvalidParameter;
                }
                if (bodyMass > 0.0f &&
                    rigidBodyIndices.find(rbs[j].get()) ==
                        rigidBodyIndices.end()) {
                    const std::size_t bodyIndex = rigid_bodies.size();
                    rigid_bodies.push_back(rbs[j].get());
                    rigidBodyIndices.insert(
                        std::make_pair(rbs[j].get(), bodyIndex));
                }
            }
        } else if(fnDagNode.typeId() == DBSoftBodyNode::typeId) {
            // soft body node
            DBSoftBodyNode *sbNode =
                dommy::maya::checkedUserNode<DBSoftBodyNode>(
                    fnDagNode, DBSoftBodyNode::typeId);
            if (!sbNode) {
                return MS::kFailure;
            }
            dbSoftBody_t::pointer body = sbNode->cached_soft_body();
            if (!body) {
                return MS::kFailure;
            }
            const float bodyMass = body->get_total_mass();
            if (!std::isfinite(bodyMass)) {
                return MS::kInvalidParameter;
            }
            if (bodyMass > 0.0f) {
                soft_bodies.push_back(body);
            }
        }
    }

    // Gather the values passed to Maya fields.
    for(size_t i = 0; i < rigid_bodies.size(); ++i) {
        vec3f pos, vel;
        quatf rot;
        rigid_bodies[i]->get_transform(pos, rot);
        rigid_bodies[i]->get_linear_velocity(vel);
        const MVector bodyPosition(pos[0], pos[1], pos[2]);
        const MVector bodyVelocity(vel[0], vel[1], vel[2]);
        const double bodyMass = rigid_bodies[i]->get_mass();
        if (!isFinite(bodyPosition) || !isFinite(bodyVelocity) ||
            !std::isfinite(bodyMass) || bodyMass <= 0.0) {
            return MS::kInvalidParameter;
        }
        MStatus status = position.append(bodyPosition);
        if (!status) {
            return status;
        }
        status = velocity.append(bodyVelocity);
        if (!status) {
            return status;
        }
        status = mass.append(bodyMass);
        if (!status) {
            return status;
        }
    }

    MVectorArray softPosition;
    MVectorArray softVelocity;
    MDoubleArray softMass;
    for(size_t i = 0; i < soft_bodies.size(); i++)
    {
        // get the central point of the soft body.  Note that
        // at the moment, forces are applied uniformly to all nodes
        // based on the value of the force at the centre, rather than
        // node by node
        vec3f pos = soft_bodies[i]->get_center();
        const MVector bodyPosition(pos[0], pos[1], pos[2]);
        
        vec3f vel = soft_bodies[i]->get_average_velocity();
        const MVector bodyVelocity(vel[0], vel[1], vel[2]);
        const double bodyMass = soft_bodies[i]->get_total_mass();
        if (!isFinite(bodyPosition) || !isFinite(bodyVelocity) ||
            !std::isfinite(bodyMass) || bodyMass <= 0.0) {
            return MS::kInvalidParameter;
        }
        MStatus status = softPosition.append(bodyPosition);
        if (!status) {
            return status;
        }
        status = softVelocity.append(bodyVelocity);
        if (!status) {
            return status;
        }
        status = softMass.append(bodyMass);
        if (!status) {
            return status;
        }
    }

    std::vector<MVector> rigidForceTotals(
        rigid_bodies.size(), MVector(0.0, 0.0, 0.0));
    std::vector<MVector> rigidTorqueTotals(
        rigid_bodies.size(), MVector(0.0, 0.0, 0.0));
    std::vector<MVector> softForceTotals(
        soft_bodies.size(), MVector(0.0, 0.0, 0.0));

    // Evaluate every Maya field into temporary totals.
    MVectorArray force;
    MVectorArray softForce;
    MStatus status;
    MItDag it(MItDag::kDepthFirst, MFn::kField, &status);
    if (!status) {
        return status;
    }
    while (!it.isDone()) {
        MFnField fnField(it.currentItem(), &status);
        if (!status) {
            return status;
        }

        status = force.setLength(0U);
        if (!status) {
            return status;
        }
        if (!rigid_bodies.empty()) {
            status = fnField.getForceAtPoint(
                position, velocity, mass, force, dt);
            if (!status) {
                return status;
            }
            if (force.length() != rigid_bodies.size()) {
                return MS::kFailure;
            }
            for(size_t i = 0; i < rigid_bodies.size(); ++i) {
                if (!checkedAccumulateVector(
                        force[i], rigidForceTotals[i])) {
                    return MS::kInvalidParameter;
                }
            }
        }

        status = softForce.setLength(0U);
        if (!status) {
            return status;
        }
        if (!soft_bodies.empty()) {
            status = fnField.getForceAtPoint(
                softPosition, softVelocity, softMass, softForce, dt);
            if (!status) {
                return status;
            }
            if (softForce.length() != soft_bodies.size()) {
                return MS::kFailure;
            }
            for(size_t i = 0; i < soft_bodies.size(); ++i) {
                if (!checkedAccumulateVector(
                        softForce[i], softForceTotals[i])) {
                    return MS::kInvalidParameter;
                }
            }
        }
        status = it.next();
        if (!status) {
            return status;
        }
    }

    // Add per-node external force and torque values to the same totals.
    for(size_t i = 0; i < rbConnections.length(); ++i)
    {
        MObject node = rbConnections[i].node();
        MStatus status;
        MFnDagNode fnDagNode(node, &status);
        if (!status) {
            return status;
        }
        if(fnDagNode.typeId() == DBRigidBodyNode::typeId)
        {
            DBRigidBodyNode *rbNode =
                dommy::maya::checkedUserNode<DBRigidBodyNode>(
                    fnDagNode, DBRigidBodyNode::typeId);
            if (!rbNode) {
                return MS::kFailure;
            }
            dbRigidBody_t::pointer rb = rbNode->cached_rigid_body();
            if (!rb) {
                return MS::kFailure;
            }
            const float bodyMass = rb->get_mass();
            if (!std::isfinite(bodyMass)) {
                return MS::kInvalidParameter;
            }
            if(bodyMass > 0.0f)
            {
                const std::map<dbRigidBody_t*, std::size_t>::const_iterator
                    bodyIndex = rigidBodyIndices.find(rb.get());
                if (bodyIndex == rigidBodyIndices.end()) {
                    return MS::kFailure;
                }

                MPlug plgExternalForce(
                    node, DBRigidBodyNode::ia_externalForce);
                vec3f forceValue;
                status = readVectorPlug(plgExternalForce, forceValue);
                if (!status) {
                    return status;
                }
                if (!checkedAccumulateVector(
                        MVector(
                            forceValue[0], forceValue[1], forceValue[2]),
                        rigidForceTotals[bodyIndex->second])) {
                    return MS::kInvalidParameter;
                }

                MPlug plgExternalTorque(
                    node, DBRigidBodyNode::ia_externalTorque);
                vec3f torqueValue;
                status = readVectorPlug(
                    plgExternalTorque, torqueValue);
                if (!status) {
                    return status;
                }
                if (!checkedAccumulateVector(
                        MVector(
                            torqueValue[0], torqueValue[1], torqueValue[2]),
                        rigidTorqueTotals[bodyIndex->second])) {
                    return MS::kInvalidParameter;
                }
            }
        }
    }

    // Convert every total before beginning the commit.
    std::vector<vec3f> rigidForces(rigid_bodies.size());
    std::vector<vec3f> rigidTorques(rigid_bodies.size());
    std::vector<vec3f> softForces(soft_bodies.size());
    for (std::size_t i = 0U; i < rigid_bodies.size(); ++i) {
        if (!checkedVectorToVec3f(rigidForceTotals[i], rigidForces[i]) ||
            !checkedVectorToVec3f(rigidTorqueTotals[i], rigidTorques[i])) {
            return MS::kInvalidParameter;
        }
    }
    for (std::size_t i = 0U; i < soft_bodies.size(); ++i) {
        if (!checkedVectorToVec3f(softForceTotals[i], softForces[i])) {
            return MS::kInvalidParameter;
        }
    }

    dbSolver_t::SoftBodyForces candidateSoftForces;
    candidateSoftForces.reserve(soft_bodies.size());
    for (std::size_t i = 0U; i < soft_bodies.size(); ++i)
        candidateSoftForces.emplace_back(soft_bodies[i], softForces[i]);

    // Commit only after every Maya read and numeric validation succeeds.
    for (std::size_t i = 0U; i < rigid_bodies.size(); ++i) {
        rigid_bodies[i]->clear_forces();
        rigid_bodies[i]->apply_central_force(rigidForces[i]);
        rigid_bodies[i]->apply_torque(rigidTorques[i]);
    }
    sampledSoftForces.swap(candidateSoftForces);
    return MS::kSuccess;
}


#pragma mark -
#pragma mark other method

MStatus DBSolverNode::refreshContactNodes(const MPlugArray& connections)
{
    // Never retain a stale snapshot after a failed refresh. Body identity is
    // separate from the native address, which Bullet's allocator may reuse.
    m_contactNodes.clear();
    ContactNodes candidate;
    for (unsigned int i = 0; i < connections.length(); ++i) {
        const MObject object = connections[i].node();
        MStatus status;
        MFnDagNode functionSet(object, &status);
        if (!status) return status;
        const MTypeId type = functionSet.typeId(&status);
        if (!status) return status;
        if (type != DBRigidBodyNode::typeId) continue;
        DBRigidBodyNode* node = dommy::maya::checkedUserNode<DBRigidBodyNode>(
            functionSet, DBRigidBodyNode::typeId);
        if (!node) return MS::kFailure;
        const dbRigidBody_t::pointer body = node->cached_rigid_body();
        if (!body) return MS::kFailure;
        const unsigned int parents = functionSet.parentCount(&status);
        if (!status) return status;
        if (parents == 0) return MS::kFailure;
        DB_RigidBody_t* native = dynamic_cast<DB_RigidBody_t*>(body->impl());
        if (!native || !native->body()) return MS::kFailure;
        const MObjectHandle handle(object);
        if (!handle.isValid() || !handle.isAlive()) return MS::kFailure;
        const auto inserted = candidate.emplace(native->body(), ContactNode{handle, body});
        if (!inserted.second && inserted.first->second.node != object) return MS::kFailure;
    }
    m_contactNodes.swap(candidate);
    return MS::kSuccess;
}

MStatus DBSolverNode::resolveRigidBodyNode(
    const btCollisionObject* collisionObject, DBRigidBodyNode*& node) const
{
    node = nullptr;
    const auto found = m_contactNodes.find(collisionObject);
    // Contacts involving unsupported node types keep the historical no-write
    // behavior. A known entry that lost its body/node is an evaluation failure.
    if (found == m_contactNodes.end()) return MS::kSuccess;
    const ContactNode& entry = found->second;
    if (!entry.node.isValid() || !entry.node.isAlive()) return MS::kFailure;
    const dbRigidBody_t::pointer body = entry.body.lock();
    if (!body) return MS::kFailure;
    MStatus status;
    MFnDependencyNode functionSet(entry.node.object(), &status);
    if (!status) return status;
    DBRigidBodyNode* candidate = dommy::maya::checkedUserNode<DBRigidBodyNode>(
        functionSet, DBRigidBodyNode::typeId);
    if (!candidate || candidate->cached_rigid_body() != body) return MS::kFailure;
    DB_RigidBody_t* native = dynamic_cast<DB_RigidBody_t*>(body->impl());
    if (!native || native->body() != collisionObject) return MS::kFailure;
    node = candidate;
    return MS::kSuccess;
}

MStatus DBSolverNode::updateContactInfo(MPlugArray& connections)
{
    MStatus status = clearContactRelatedAttributes(connections);
    if (!status) return status;
    status = refreshContactNodes(connections);
    if (!status) return status;

    dbSolver_t::LockedSolverAccess access = dbSolver_t::lock_solver();
    DB_BulletSolver_t* solver = static_cast<DB_BulletSolver_t*>(access.get());
    if (!solver || !solver->dyanmicsWorld()) return MS::kFailure;
    btCollisionWorld* world = solver->dyanmicsWorld()->getCollisionWorld();
    if (!world || !world->getDispatcher()) return MS::kFailure;
    struct Contact {
        const btCollisionObject* a;
        const btCollisionObject* b;
        MVector pointA;
        MVector pointB;
    };
    std::vector<Contact> contacts;
    // Snapshot native manifold data before any Maya writes can invoke callbacks
    // that remove bodies or invalidate the manifolds themselves.
    const int count = world->getDispatcher()->getNumManifolds();
    for (int i = 0; i < count; ++i) {
        btPersistentManifold* manifold = world->getDispatcher()->getManifoldByIndexInternal(i);
        if (!manifold) return MS::kFailure;
        const btCollisionObject* bodyA = manifold->getBody0();
        const btCollisionObject* bodyB = manifold->getBody1();
        if (m_contactNodes.find(bodyA) == m_contactNodes.end() ||
            m_contactNodes.find(bodyB) == m_contactNodes.end()) continue;
        for (int j = 0; j < manifold->getNumContacts(); ++j) {
            // Match the existing rigid-output record budget, rather than the
            // much larger work-item budget, for this retained snapshot.
            if (contacts.size() >= dommy::generation::kMaxGeneratedTransformCount)
                return MS::kInvalidParameter;
            const btManifoldPoint& point = manifold->getContactPoint(j);
            const btVector3 a = point.getPositionWorldOnA();
            const btVector3 b = point.getPositionWorldOnB();
            const MVector pointA(a.getX(), a.getY(), a.getZ());
            const MVector pointB(b.getX(), b.getY(), b.getZ());
            if (!isFinite(pointA) || !isFinite(pointB)) return MS::kInvalidParameter;
            contacts.push_back(Contact{bodyA, bodyB, pointA, pointB});
        }
    }
    for (const Contact& contact : contacts) {
        DBRigidBodyNode* a = nullptr;
        DBRigidBodyNode* b = nullptr;
        status = resolveRigidBodyNode(contact.a, a);
        if (!status) return status;
        status = resolveRigidBodyNode(contact.b, b);
        if (!status) return status;
        if (!a || !b) continue;
        const MString nameA = a->name();
        const MString nameB = b->name();
        status = a->addContactInfo(nameB, contact.pointA);
        if (!status) return status;
        // Re-resolve after the first write; do not borrow node pointers across
        // callbacks. A partial host write is not a successful solver update.
        status = resolveRigidBodyNode(contact.a, a);
        if (!status) return status;
        status = resolveRigidBodyNode(contact.b, b);
        if (!status) return status;
        if (!a || !b) return MS::kFailure;
        status = b->addContactInfo(nameA, contact.pointB);
        if (!status) return status;
    }
    return MS::kSuccess;
}

void tokenize(const std::string& str,
              std::vector<std::string>& tokens,
              const std::string& delimiters = " ")
{
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
    
    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

bool DBSolverNode::expandFileExpression(std::string const& expr, std::string &base_name, std::string &extension)
{
    std::vector<std::string> tokens;
    
    //check for extension
    tokenize(expr, tokens, ".");
    if(tokens.size() < 2) return false;
    if(tokens.back().size() != 3) return false;
    long long frame = 0;
    if (!dommy::file::checkedTruncatedFrame(m_prevTime.value(), frame)) return false;
    std::string str_time;
    if (!dommy::file::formatSignedInteger(frame, 0U, str_time)) return false;
    std::string candidateBase(expr.begin(), expr.end() - 4);
    std::string candidateExtension(tokens.back());
    //replace ${frame}
    dommy::file::replaceAll(candidateBase, "${frame}", str_time);

    // Preserve historical whole-string padding, including negative frames.
    while (str_time.size() < 4)
        str_time = "0" + str_time;
    dommy::file::replaceAll(candidateBase, "${waveFrame}", str_time);
    base_name.swap(candidateBase);
    extension.swap(candidateExtension);
    return true;
}

void DBSolverNode::dumpRigidBodyArray(MObject& node) {
#if !DOMMY_WITH_DG_FILE_WRITERS
    (void)node;
    return;
#else
    // std::cout << "dSolverNode::dumpRigidBodyArray" << std::endl;

    MFnDagNode fnDagNode(node);
    DBRigidBodyArrayNode* rbaNode =
        dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
            fnDagNode, DBRigidBodyArrayNode::typeId);
    if (!rbaNode) return;

    MPlug plgFiles(node, DBRigidBodyArrayNode::ia_fioFiles);
    //  MPlug plgPositionAttribute(node, rigidBodyArrayNode::ia_fioPositionAttribute);
    // MPlug plgRotationAttribute(node, rigidBodyArrayNode::ia_fioRotationAttribute);

    MString mstr;
    if (readPlugValue(plgFiles, mstr) != MS::kSuccess) {
        EchoWarning(
            name().asChar(),
            "Unable to read the rigid-body output file expression.");
        return;
    }
    std::string expr(mstr.asChar());
    std::string base_path, extension;
    if (!expandFileExpression(expr, base_path, extension)) {
        EchoWarning(name().asChar(), "Invalid rigid-body file expression.");
        return;
    }
    if (extension != "pdb") {
        EchoWarning(name().asChar(), "Only PDB rigid-body files are supported.");
        return;
    }
    std::string file_name = base_path + "." + extension;
    if (!dommy::file::isSafeExportPath(file_name, ".pdb")) {
        EchoWarning(
            name().asChar(),
            "Rigid-body output must be a bounded .pdb path without control "
            "characters or '..' components.");
        return;
    }

	const std::vector<dbRigidBody_t::pointer>& rbs =
        rbaNode->cached_rigid_bodies();
	if (rbs.size() > pdb_io_t::kMaxParticleCount) {
		EchoWarning(name().asChar(), "Too many rigid bodies for the PDB format.");
		return;
	}
	const double outputTime = m_prevTime.value();
	if (!std::isfinite(outputTime) ||
		std::fabs(outputTime) > std::numeric_limits<float>::max()) {
		EchoWarning(name().asChar(), "Rigid-body PDB time is not finite.");
		return;
	}
	pdb_io_t pdb_io;
	pdb_io.m_num_particles = static_cast<int>(rbs.size());
	pdb_io.m_time = static_cast<float>(outputTime);
    pdb_io.m_attributes.resize(3);
    // position
    pdb_io.m_attributes[0].m_num_elements = 1;
    pdb_io.m_attributes[0].m_name = ATTR_POSITION;
    pdb_io.m_attributes[0].m_type = pdb_io_t::kVector;
    pdb_io.m_attributes[0].m_vector_data.resize(rbs.size());

    // rotation angle (in degrees)
    pdb_io.m_attributes[1].m_num_elements = 1;
    pdb_io.m_attributes[1].m_name = ATTR_IN_RANGLE;
    pdb_io.m_attributes[1].m_type = pdb_io_t::kReal;
    pdb_io.m_attributes[1].m_real_data.resize(rbs.size());

    //rotation vector
    pdb_io.m_attributes[2].m_num_elements = 1;
    pdb_io.m_attributes[2].m_name = ATTR_IN_RAXIS;
    pdb_io.m_attributes[2].m_type = pdb_io_t::kVector;
    pdb_io.m_attributes[2].m_vector_data.resize(rbs.size());
    
    for(size_t i = 0; i < rbs.size(); ++i) {
        vec3f pos;
        quatf rot;
		rbs[i]->get_transform(pos, rot);
		if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) ||
			!std::isfinite(pos[2])) {
			EchoWarning(name().asChar(), "Rigid-body PDB position is not finite.");
			return;
		}
        pdb_io.m_attributes[0].m_vector_data[i].x = pos[0];
        pdb_io.m_attributes[0].m_vector_data[i].y = pos[1];
        pdb_io.m_attributes[0].m_vector_data[i].z = pos[2];
        
        //
        vec3f axis;
		float angle;
		q_to_axis_angle(rot, axis, angle);
		if (!std::isfinite(axis[0]) || !std::isfinite(axis[1]) ||
			!std::isfinite(axis[2]) || !std::isfinite(angle)) {
			EchoWarning(name().asChar(), "Rigid-body PDB rotation is not finite.");
			return;
		}
        pdb_io.m_attributes[1].m_real_data[i] = rad2deg(angle);

        pdb_io.m_attributes[2].m_vector_data[i].x = axis[0];
        pdb_io.m_attributes[2].m_vector_data[i].y = axis[1];
        pdb_io.m_attributes[2].m_vector_data[i].z = axis[2];
    }

	dommy::file::PendingOutputFile pending(file_name);
	std::ostream& out = pending.stream();
	if (!pdb_io.write(out)) {
		EchoWarning(name().asChar(), "Failed to write the rigid-body PDB file.");
		return;
	}
	out.flush();
	const bool closed = pending.close();
	if (out.fail() || !closed) {
		EchoWarning(name().asChar(), "Failed to close the rigid-body PDB file cleanly.");
		return;
	}
	if (!pending.commit()) {
		EchoWarning(name().asChar(), "Failed to commit the rigid-body PDB file.");
	}
#endif
}

#pragma mark -
#pragma mark sub compute

MStatus DBSolverNode::computeRigidBodies(const MPlug& plug, MDataBlock& data) {
    MStatus status;
    const MDataHandle enabledHandle = data.inputValue(aEnabled, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    const bool enabled = enabledHandle.asBool();

    if (!enabled) {
        s_isStartTime.store(false, std::memory_order_release);
        MDataHandle output = data.outputValue(aRigidBodies, &status);
        if (status != MS::kSuccess) {
            return status;
        }
        output.set(true);
        return data.setClean(plug);
    }

    BulletWorldMutationGuard mutationGuard(
        m_requiresStartFrame, m_startFrameErrorReported);

    const MDataHandle timeHandle = data.inputValue(aCurrentTime, &status);
    if (status != MS::kSuccess)
        return status;
    const MTime time = timeHandle.asTime();

    const MDataHandle startTimeHandle = data.inputValue(aStartTime, &status);
    if (status != MS::kSuccess)
        return status;
    const MTime startTime = startTimeHandle.asTime();
    if (!std::isfinite(time.value()) || !std::isfinite(startTime.value()))
        return MS::kInvalidParameter;

    // Compare MTime values (including their units), not dirty callbacks. Only
    // a successful enabled initialization may replace the committed setting.
    if (!m_requiresStartFrame && startTime != m_initializedStartTime)
        m_requiresStartFrame = true;

    bool playback = false;
    if (time != startTime) {
        playback = MConditionMessage::getConditionState("playingBack", &status);
        if (!status) return status;
    }
    const dommy::bullet::TimelineAction action = dommy::bullet::timelineAction(
        !m_requiresStartFrame, time == startTime,
        (time - m_prevTime).value(), playback);
    if (action == dommy::bullet::TimelineAction::RestartRequired) {
        m_requiresStartFrame = true;
        s_isStartTime.store(false, std::memory_order_release);
        if (!m_startFrameErrorReported) {
            MGlobal::displayError(
                "DBSolverNode must be evaluated at StartTime after a time "
                "jump, StartTime edit, uninitialized world or failed evaluation. Rewind to "
                "StartTime and evaluate the enabled solver again.");
            m_startFrameErrorReported = true;
        }
        return MS::kFailure;
    }

    const MDataHandle substepsHandle = data.inputValue(aSubsteps, &status);
    if (status != MS::kSuccess)
        return status;
    const int subSteps = substepsHandle.asInt();
    const MDataHandle physicsRateHandle = data.inputValue(aFixedPhysicsRate, &status);
    if (status != MS::kSuccess)
        return status;
    const int fixedPhysicsFrameHertz = physicsRateHandle.asInt();
    if (!dommy::bullet::validSimulationSettings(subSteps, fixedPhysicsFrameHertz)) {
        return MS::kInvalidParameter;
    }

    const MDataHandle splitImpulseHandle = data.inputValue(aSplitImpulse, &status);
    if (status != MS::kSuccess)
        return status;
    const bool splitImpulseEnabled = splitImpulseHandle.asBool();

    const MDataHandle contactDataHandle = data.inputValue(aContactData, &status);
    if (status != MS::kSuccess)
        return status;
    const bool contactDataEnabled = contactDataHandle.asBool();

    MObject thisObject = thisMObject();
    MPlug plgRigidBodies(thisObject, aRigidBodies);
    MPlugArray rbConnections;
    plgRigidBodies.connectedTo(
        rbConnections, true, true, &status);
    if (!status) {
        return status;
    }

    if (action == dommy::bullet::TimelineAction::Initialize) {
        // need to re-construct the world and re-insert all objects...
        s_isStartTime.store(true, std::memory_order_release);
        mutationGuard.arm();
        const auto failStartInitialization =
            [this](const MStatus& failure) -> MStatus {
                dbSolver_t::destroyWorld();
                dbSolver_t::createWorld();
                s_isStartTime.store(false, std::memory_order_release);
                m_contactNodes.clear();
                m_requiresStartFrame = true;
                return failure;
        };

        if (m_reInitialize) {
            dbSolver_t::destroyWorld();
            dbSolver_t::createWorld();
        }

        status = initRigidBodies(rbConnections, data);
        if (!status) {
            return failStartInitialization(status);
        }

        status = refreshContactNodes(rbConnections);
        if (!status) return failStartInitialization(status);

        status = clearContactRelatedAttributes(rbConnections);
        if (!status) {
            return failStartInitialization(status);
        }

        m_reInitialize = true;
        dbSolver_t::set_split_impulse(splitImpulseEnabled);
    } else {
        s_isStartTime.store(false, std::memory_order_release);

        if (action == dommy::bullet::TimelineAction::Advance) {

            dommy::bullet::SimulationStepPlan step;
            if (!dommy::bullet::prepareSimulationStep(
                    (time - m_prevTime).as(MTime::kSeconds), subSteps,
                    fixedPhysicsFrameHertz, step) ||
                !dbSolver_t::can_step_simulation(step.seconds, step.fixedSeconds, step.substeps)) {
                return MS::kInvalidParameter;
            }

            // gather start and end transform for passive rigid bodies, used for interpolation
            std::vector<xforms_t> passiveXForms;
            status = gatherPassiveTransforms(
                rbConnections, passiveXForms);
            if (!status) {
                return status;
            }

            // Set solver-owned input values from the current data block. The
            // remaining cross-node MPlug pulls are why this node is untrusted.
            const MDataHandle gravityHandle = data.inputValue(aGravity, &status);
            if (status != MS::kSuccess)
                return status;
            const double3& gravityValue = gravityHandle.asDouble3();
            vec3f gravity;
            gravity[0] = static_cast<float>(gravityValue[0]);
            gravity[1] = static_cast<float>(gravityValue[1]);
            gravity[2] = static_cast<float>(gravityValue[2]);
            if (!std::isfinite(gravity[0]) ||
                !std::isfinite(gravity[1]) ||
                !std::isfinite(gravity[2])) {
                return MS::kInvalidParameter;
            }

            if (!dbSolver_t::set_gravity(gravity)) return MS::kInvalidParameter;
            mutationGuard.arm();

            dbSolver_t::set_split_impulse(splitImpulseEnabled);

            for (int i = 1; i <= subSteps; ++i) {
                float interpolationFraction = 0.0F;
                if (!dommy::bullet::normalizedSubstepFraction(
                        i, subSteps, interpolationFraction)) {
                    return MS::kInvalidParameter;
                }
                // first update the passive rigid bodies
                status = updatePassiveRigidBodies(
                    rbConnections, passiveXForms, interpolationFraction);
                if (!status) {
                    return status;
                }

                // fields
                dbSolver_t::SoftBodyForces softForces;
                status = applyFields(rbConnections, step.seconds, softForces);
                if (!status) {
                    return status;
                }

                // step the simulation
                if (!dbSolver_t::step_simulation(step.seconds, step.fixedSeconds, softForces))
                    return MS::kInvalidParameter;

                // update contactCount, contactName and contactPosition attributes
                if (contactDataEnabled) {
                    status = updateContactInfo(rbConnections);
                    if (!status) return status;
                }
            }

            status = updateActiveRigidBodies(rbConnections);
            if (!status) return status;
            status = updateActiveSoftBodies(rbConnections);
            if (!status) return status;
        }
    }

    MDataHandle output = data.outputValue(aRigidBodies, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    output.set(true);
    status = data.setClean(plug);
    if (status == MS::kSuccess) {
        // Disabled passthrough never reaches this commit. Publish time and
        // readiness only after initialization/stepping AND host output succeed.
        m_prevTime = time;
        if (action == dommy::bullet::TimelineAction::Initialize) {
            m_initializedStartTime = startTime;
            m_requiresStartFrame = false;
            m_startFrameErrorReported = false;
        }
        mutationGuard.commit();
    }
    return status;
}

#pragma mark -
#pragma mark main compute

bool DBSolverNode::acquireSharedWorld() {
    if (m_ownsSharedWorld &&
        s_activeSolver.load(std::memory_order_acquire) == this) {
        return true;
    }

    DBSolverNode* expectedOwner = nullptr;
    if (s_activeSolver.compare_exchange_strong(
            expectedOwner, this, std::memory_order_acq_rel)) {
        m_ownsSharedWorld = true;
        m_requiresStartFrame = true;
        m_startFrameErrorReported = false;
        m_reInitialize = true;
        dbSolver_t::destroyWorld();
        dbSolver_t::createWorld();
        return true;
    }

    if (!m_startFrameErrorReported) {
        MGlobal::displayError(
            "Only one DBSolverNode can own the legacy shared Bullet world. "
            "Delete or disable the additional solver.");
        m_startFrameErrorReported = true;
    }
    return false;
}

MStatus DBSolverNode::updateDebugDrawMode(MDataBlock& data) {
    const MObject attributes[] = {
        ia_DBG_DrawWireframe,
        ia_DBG_DrawAabb,
        ia_DBG_DrawContactPoints,
        ia_DBG_NoDeactivation,
        ia_DBG_ProfileTimings,
        ia_DBG_DrawConstraints,
        ia_DBG_DrawConstraintLimits};
    const int flags[] = {0, 1, 3, 4, 7, 11, 12};

    int debugMode = 0;
    for (std::size_t index = 0;
         index < sizeof(attributes) / sizeof(attributes[0]); ++index) {
        MStatus status;
        const MDataHandle value = data.inputValue(attributes[index], &status);
        if (status != MS::kSuccess) return status;
        if (value.asBool()) debugMode |= (1 << flags[index]);
    }
    m_debugDrawMode.store(debugMode, std::memory_order_release);
    return MS::kSuccess;
}

MStatus DBSolverNode::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aRigidBodies) {
        return MS::kUnknownParameter;
    }

    MStatus status = updateDebugDrawMode(data);
    if (status != MS::kSuccess) return status;
    if (!acquireSharedWorld()) return MS::kFailure;

    return computeRigidBodies(plug, data);
}
DOMMY_COMPUTE_CATCH("DBSolverNode")

static void sceneLoadedCB(void* clientData) {
    (void)clientData;
    try {
        if (!DBSolverNode::updateAllRigidBodies()) {
            MGlobal::displayWarning(
                "DBSolverNode: scene-load update failed.");
        }
    } catch (...) {
        MGlobal::displayWarning("DBSolverNode: scene-load callback failed.");
    }
}
static void updateSceneCB(MTime& time, void* clientData) {
    (void)time;
    (void)clientData;
    try {
        if (!DBSolverNode::updateAllRigidBodies()) {
            MGlobal::displayWarning(
                "DBSolverNode: force update failed.");
        }
    } catch (...) {
        MGlobal::displayWarning("DBSolverNode: force-update callback failed.");
    }
}

MStatus DBSolverNode::registerCallbacks() {
    MStatus status = removeCallbacks();
    if (status != MS::kSuccess)
        return status;

    updateSceneCallbackId = MDGMessage::addForceUpdateCallback(updateSceneCB, nullptr, &status);
    if (status != MS::kSuccess) return status;
    if (updateSceneCallbackId == 0) return MS::kFailure;

    sceneLoadedCallbackId =
        MSceneMessage::addCallback(MSceneMessage::kAfterOpen, sceneLoadedCB, nullptr, &status);
    // The registration owner retains removeCallbacks() even on partial setup
    // failure. Never erase an ID before its removal has succeeded.
    if (status == MS::kSuccess && sceneLoadedCallbackId == 0)
        return MS::kFailure;
    return status;
}

MStatus DBSolverNode::removeCallbacks() {
    MStatus result = MS::kSuccess;
    if (updateSceneCallbackId != 0) {
        MStatus status = MMessage::removeCallback(updateSceneCallbackId);
        if (status == MS::kSuccess)
            updateSceneCallbackId = 0;
        else
            result = status;
    }
    if (sceneLoadedCallbackId != 0) {
        MStatus status = MMessage::removeCallback(sceneLoadedCallbackId);
        if (status == MS::kSuccess)
            sceneLoadedCallbackId = 0;
        else
            result = status;
    }
    return result;
}

MStatus DBSolverNode::updateAllRigidBodies() {
    MStatus stat;
    //    MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid, &stat);
    MItDag dagIterator(MItDag::kDepthFirst, MFn::kInvalid, &stat);
    if (stat != MS::kSuccess) {
        MGlobal::displayWarning("DBSolverNode: failed to create the DAG iterator.");
        return stat;
    }
    for (;; stat = dagIterator.next()) {
        if (!stat) return stat;
        const bool done = dagIterator.isDone(&stat);
        if (!stat) return stat;
        if (done) break;
        MDagPath dagPath;
        stat = dagIterator.getPath(dagPath);
        if (stat != MS::kSuccess) {
            MGlobal::displayWarning("DBSolverNode: failed to read a DAG path.");
            return stat;
        }
        // skip over intermediate objects
        MFnDagNode dagNode(dagPath, &stat);
        if (!stat) return stat;
        const bool intermediate = dagNode.isIntermediateObject(&stat);
        if (!stat) return stat;
        if (intermediate) {
            continue;
        }
        if (!dagPath.hasFn(MFn::kDependencyNode)) {
            continue;
        }
        MObject mObj = dagNode.object(&stat);
        if (stat != MS::kSuccess) {
            MGlobal::displayWarning("DBSolverNode: failed to read a DAG object.");
            return stat;
        }
        MFnDependencyNode fnNode(mObj, &stat);
        if (stat != MS::kSuccess) {
            MGlobal::displayWarning("DBSolverNode: failed to inspect a dependency node.");
            return stat;
        }

        const MTypeId nodeType = fnNode.typeId(&stat);
        if (!stat) return stat;
        if(nodeType == DBRigidBodyNode::typeId)
        {
            MPlug plgCollisionShape(mObj, DBRigidBodyNode::ia_collisionShape);
            MObject update;
            //force evaluation of the shape
            stat = plgCollisionShape.getValue(update);
            if (!stat) return stat;
            const bool connected = plgCollisionShape.isConnected(&stat);
            if (!stat) return stat;
            if(connected)
            {
                DBRigidBodyNode *rbNode =
                    dommy::maya::checkedUserNode<DBRigidBodyNode>(
                        dagNode, DBRigidBodyNode::typeId);
                if (!rbNode) return MS::kFailure;
                stat = rbNode->update();
                if (!stat) return stat;
            }
        }
        if(nodeType == DBRigidBodyArrayNode::typeId)
        {
            MPlug collisionShape(
                mObj, DBRigidBodyArrayNode::ia_collisionShape);
            MObject update;
            stat = collisionShape.getValue(update);
            if (!stat) return stat;
            const bool connected = collisionShape.isConnected(&stat);
            if (!stat) return stat;
            if (connected)
            {
                DBRigidBodyArrayNode* rigidBodyArrayNode =
                    dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
                        dagNode, DBRigidBodyArrayNode::typeId);
                if (!rigidBodyArrayNode) return MS::kFailure;
                stat = rigidBodyArrayNode->update();
                if (!stat) return stat;
            }
        }
        
        if(nodeType == DBSoftBodyNode::typeId)
        {
            MPlug plgMeshShape(mObj, DBSoftBodyNode::inputMesh);
            MObject update;
            stat = plgMeshShape.getValue(update);
            if (!stat) return stat;
            const bool connected = plgMeshShape.isConnected(&stat);
            if (!stat) return stat;
            if(connected)
            {
                DBSoftBodyNode *sbNode =
                    dommy::maya::checkedUserNode<DBSoftBodyNode>(
                        dagNode, DBSoftBodyNode::typeId);
                if (!sbNode) return MS::kFailure;
                stat = sbNode->update();
                if (!stat) return stat;
            }
        }
        if(nodeType == DBNailConstraintNode::typeId)
        {
            MPlug plgRbA(mObj, DBNailConstraintNode::ia_rigidBodyA);
            MPlug plgRbB(mObj, DBNailConstraintNode::ia_rigidBodyB);
            MObject update;
            //force evaluation
            stat = plgRbA.getValue(update);
            if (!stat) return stat;
            stat = plgRbB.getValue(update);
            if (!stat) return stat;
            const bool connA = plgRbA.isConnected(&stat);
            if (!stat) return stat;
            const bool connB = plgRbB.isConnected(&stat);
            if (!stat) return stat;
            if(connA || connB)
            {
                DBNailConstraintNode *ncNode =
                    dommy::maya::checkedUserNode<DBNailConstraintNode>(
                        dagNode, DBNailConstraintNode::typeId);
                if (!ncNode) return MS::kFailure;
                stat = ncNode->update();
                if (!stat) return stat;
            }
        }
        if(nodeType == DBHingeConstraintNode::typeId)
        {
            MPlug plgRbA(mObj, DBHingeConstraintNode::ia_rigidBodyA);
            MPlug plgRbB(mObj, DBHingeConstraintNode::ia_rigidBodyB);
            MObject update;
            //force evaluation
            stat = plgRbA.getValue(update);
            if (!stat) return stat;
            stat = plgRbB.getValue(update);
            if (!stat) return stat;
            const bool connA = plgRbA.isConnected(&stat);
            if (!stat) return stat;
            const bool connB = plgRbB.isConnected(&stat);
            if (!stat) return stat;
            if(connA || connB)
            {
                DBHingeConstraintNode *hcNode =
                    dommy::maya::checkedUserNode<DBHingeConstraintNode>(
                        dagNode, DBHingeConstraintNode::typeId);
                if (!hcNode) return MS::kFailure;
                stat = hcNode->update();
                if (!stat) return stat;
            }
        }
        if(nodeType == DBSliderConstraintNode::typeId)
        {
            MPlug plgRbA(mObj, DBSliderConstraintNode::ia_rigidBodyA);
            MPlug plgRbB(mObj, DBSliderConstraintNode::ia_rigidBodyB);
            MObject update;
            //force evaluation
            stat = plgRbA.getValue(update);
            if (!stat) return stat;
            stat = plgRbB.getValue(update);
            if (!stat) return stat;
            const bool connA = plgRbA.isConnected(&stat);
            if (!stat) return stat;
            const bool connB = plgRbB.isConnected(&stat);
            if (!stat) return stat;
            if(connA || connB)
            {
                DBSliderConstraintNode *scNode =
                    dommy::maya::checkedUserNode<DBSliderConstraintNode>(
                        dagNode, DBSliderConstraintNode::typeId);
                if (!scNode) return MS::kFailure;
                stat = scNode->update();
                if (!stat) return stat;
            }
        }
        if(nodeType == DBSixdofConstraintNode::typeId)
        {
            MPlug plgRbA(mObj, DBSixdofConstraintNode::ia_rigidBodyA);
            MPlug plgRbB(mObj, DBSixdofConstraintNode::ia_rigidBodyB);
            MObject update;
            //force evaluation
            stat = plgRbA.getValue(update);
            if (!stat) return stat;
            stat = plgRbB.getValue(update);
            if (!stat) return stat;
            const bool connA = plgRbA.isConnected(&stat);
            if (!stat) return stat;
            const bool connB = plgRbB.isConnected(&stat);
            if (!stat) return stat;
            if(connA || connB)
            {
                DBSixdofConstraintNode *sdNode =
                    dommy::maya::checkedUserNode<DBSixdofConstraintNode>(
                        dagNode, DBSixdofConstraintNode::typeId);
                if (!sdNode) return MS::kFailure;
                stat = sdNode->update();
                if (!stat) return stat;
            }
        }
    }
    return MS::kSuccess;
}

MStatus DBSolverNode::initialize() try {
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnMatrixAttribute matAttr;
    
    MStatus stat;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    MFnMessageAttribute fnMsgAttr;
    aRigidBodies = fnMsgAttr.create("rigidBodies", "rbds", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRigidBodies));
    
    
    aSoftBodies = fnMsgAttr.create("softBodies", "sbds", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSoftBodies));
    
    
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
    
    
#pragma mark bullet solver
    aGravity = nAttr.create("Gravity", "grvt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, -9.81, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravity));
    
    
    aCollisionMargin = nAttr.create("CollisionMargin", "colm", MFnNumericData::kFloat, 0.04, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionMargin));
    
    aSplitImpulse = nAttr.create("SplitImpulse", "spli", MFnNumericData::kBoolean,true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSplitImpulse));
    
    
    // Retained for archived scene compatibility. Constraint nodes own the
    // effective per-constraint disableCollide setting in this implementation.
    aDisableCollisionsBetweenLinkedBodies = nAttr.create("DisableCollisionsBetweenLinkedBodies", "dCol", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisableCollisionsBetweenLinkedBodies));
    
    
    
    
    aSubsteps = nAttr.create("substeps", "sbs", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));
    
    
    aFixedPhysicsRate = nAttr.create("FixedPhysicsRate", "fps", MFnNumericData::kInt, 240, &stat); //MB
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(60));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(6000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFixedPhysicsRate));
    
    
    
    
#pragma mark display attribute
    
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
    
    
    
    
#pragma mark out put
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, aSoftBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEnabled, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartTime, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGravity, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCollisionMargin, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSplitImpulse, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSubsteps, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFixedPhysicsRate, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aContactData, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_DrawWireframe, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_DrawAabb, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_DrawContactPoints, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_NoDeactivation, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_ProfileTimings, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_DBG_DrawConstraints, aRigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(
        ia_DBG_DrawConstraintLimits, aRigidBodies));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSolverNode", "initialize", std::current_exception());
}
