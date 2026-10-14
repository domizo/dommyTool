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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/rigidBodyArrayNode.cpp

//
//  DBRigidBodyArrayNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBRigidBodyArrayNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"

#include "BulletSoftBody/btSoftBody.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DBCollisionShapeNode.h"
#include "DB_RigidBody_t.h"
#include "DBRigidBodyParameters.h"
#include "../Collision/DBCollisionShapeInput.h"
#include "../Collision/DCollisionMeshInput.h"
#include "../Collision/DShapeFrameMath.h"


#include "dbSolver_t.h"

#include "dpoint_ids.h"

#include <array>
#include <cmath>

MTypeId DBRigidBodyArrayNode::typeId(ID_DB_CONSTRAINT_RIGIDBODYARRAY);
const MString DBRigidBodyArrayNode::typeName("DBRigidBodyArrayNode");

MObject DBRigidBodyArrayNode::ia_collisionShape;
MObject DBRigidBodyArrayNode::ia_solver;
MObject DBRigidBodyArrayNode::ia_numBodies;

MObject DBRigidBodyArrayNode::ia_mass;
MObject DBRigidBodyArrayNode::ia_restitution;
MObject DBRigidBodyArrayNode::ia_friction;
MObject DBRigidBodyArrayNode::ia_linearDamping;
MObject DBRigidBodyArrayNode::ia_angularDamping;
MObject DBRigidBodyArrayNode::ia_initialPosition;
MObject DBRigidBodyArrayNode::ia_initialRotation;
MObject DBRigidBodyArrayNode::ia_initialVelocity;
MObject DBRigidBodyArrayNode::ia_initialSpin;
MObject DBRigidBodyArrayNode::ia_fileIO;
MObject DBRigidBodyArrayNode::ia_fioFiles;
MObject DBRigidBodyArrayNode::io_position;
MObject DBRigidBodyArrayNode::io_rotation;
MObject DBRigidBodyArrayNode::ca_rigidBodies;
MObject DBRigidBodyArrayNode::ca_rigidBodyParam;
MObject DBRigidBodyArrayNode::ca_solver;

MStatus DBRigidBodyArrayNode::initialize()
try {
    MStatus stat;
    MFnMessageAttribute fnMsgAttr;
    MFnNumericAttribute fnNumericAttr;
    MFnMatrixAttribute  fnMatrixAttr;
    MFnTypedAttribute    fnTypedAttr;
    MFnStringData       fnStringData;
    
    ia_collisionShape = fnMsgAttr.create("inCollisionShape", "incs", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_collisionShape));
    
    
    ia_solver = fnMsgAttr.create("solver", "solv", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_solver));
    
    
    ia_numBodies = fnNumericAttr.create("numBodies", "nbds", MFnNumericData::kInt, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(
        static_cast<double>(
            dommy::generation::kMaxGeneratedTransformCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_numBodies));
    
    
    
    ia_mass = fnNumericAttr.create("mass", "ma", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_mass));
    
    
    ia_restitution = fnNumericAttr.create("restitution", "rst", MFnNumericData::kDouble, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_restitution));
    
    
    ia_friction = fnNumericAttr.create("friction", "fc", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_friction));
    
    
    ia_linearDamping = fnNumericAttr.create("linearDamping", "ld", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_linearDamping));
    
    
    ia_angularDamping = fnNumericAttr.create("angularDamping", "ad", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_angularDamping));
    
    
    ia_initialPosition = fnNumericAttr.createPoint("initialPosition", "inpo", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialPosition));
    
    
    ia_initialRotation = fnNumericAttr.createPoint("initialRotation", "inro", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialRotation));
    
    
    ia_initialVelocity = fnNumericAttr.createPoint("initialVelocity", "inve", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialVelocity));
    
    
    ia_initialSpin = fnNumericAttr.createPoint("initialSpin", "insp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialSpin));
    
    
    ia_fileIO = fnNumericAttr.create("fileIO", "fio", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_fileIO));
    
    
    const MObject ia_fioFilesDefaultData = fnStringData.create("/tmp/rigidBodyArray.${frame}.pdb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    ia_fioFiles = fnTypedAttr.create( "files", "fls", MFnData::kString,
                                     ia_fioFilesDefaultData, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnTypedAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_fioFiles));
    
    
    io_position = fnNumericAttr.createPoint("position", "pos", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(io_position));
    
    
    io_rotation = fnNumericAttr.createPoint("rotation", "rot", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(io_rotation));
    
    
    ca_rigidBodies = fnNumericAttr.create("ca_rigidBodies", "carb", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_rigidBodies));
    
    
    ca_rigidBodyParam = fnNumericAttr.create("ca_rigidBodyParam", "carbp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_rigidBodyParam));
    
    
    ca_solver = fnNumericAttr.create("ca_solver", "caso", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_solver));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_numBodies, ca_rigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_numBodies, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_collisionShape, ca_rigidBodies));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_collisionShape, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_mass, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_restitution, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_friction, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_linearDamping, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_angularDamping, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_solver, ca_solver));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBRigidBodyArrayNode", "initialize", std::current_exception());
}

DBRigidBodyArrayNode::DBRigidBodyArrayNode()
{
    m_numRigidBodies = 0;
}

DBRigidBodyArrayNode::~DBRigidBodyArrayNode()
{
    destroyRigidBodies();
}

void DBRigidBodyArrayNode::nodeRemoved(MObject& node, void*)
{
    MFnDependencyNode fnNode(node);
    DBRigidBodyArrayNode *thisNode =
        dommy::maya::checkedUserNode<DBRigidBodyArrayNode>(
            fnNode, DBRigidBodyArrayNode::typeId);
    if (!thisNode) return;
    thisNode->destroyRigidBodies();
}

void DBRigidBodyArrayNode::postConstructor()
{
}

void* DBRigidBodyArrayNode::creator()
{
    return ::dommy::maya::createObject("DBRigidBodyArrayNode", [&]() {
        return new DBRigidBodyArrayNode();
    });
}


bool DBRigidBodyArrayNode::setInternalValueInContext(
    const MPlug& /*plug*/, const MDataHandle& /*dataHandle*/,
    MDGContext& /*context*/)
{
    return false;
}

MStatus DBRigidBodyArrayNode::compute(const MPlug& plug, MDataBlock& data) try
{
    if(plug == ca_rigidBodies) {
        return computeRigidBodies(plug, data);
    } else if(plug == ca_rigidBodyParam) {
        return computeRigidBodyParam(plug, data);
    } else if(plug == ca_solver) {
        MStatus status;
        data.inputValue(ia_solver, &status);
        if (!status) return status;
        MDataHandle output = data.outputValue(ca_solver, &status);
        if (!status) return status;
        output.set(true);
        output.setClean();
        return data.setClean(plug);
    } else if(plug.isElement()) {
        if(plug.array() == worldMatrix) {
            return computeWorldMatrix(plug, data);
        } else {
            return MStatus::kUnknownParameter;
        }
    } else {
        return MStatus::kUnknownParameter;
    }
    return MStatus::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBRigidBodyArrayNode")

void DBRigidBodyArrayNode::draw( M3dView & view, const MDagPath&,
                              M3dView::DisplayStyle style,
                              M3dView::DisplayStatus stat ) try
{
    dbSolver_t::LockedSolverAccess solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return;
    
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    
    dommy::maya::ScopedLegacyGLMatrix worldMatrix;
    //remove the parent transform here
    double m[4][4];
    m_worldMatrix.inverse().get(m);
    glMultMatrixd(&(m[0][0]));
    
    if(!m_rigid_bodies.empty()) {
        mat4x4f xform;
        if(style == M3dView::kFlatShaded ||
           style == M3dView::kGouraudShaded) {
            glEnable(GL_LIGHTING);
            float material[] = { 0.4, 0.3, 1.0, 1.0 };
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material);
            for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
                if (!m_rigid_bodies[i] ||
                    !m_rigid_bodies[i]->collision_shape()) continue;
                dommy::maya::ScopedLegacyGLMatrix bodyMatrix;
                m_rigid_bodies[i]->get_transform(xform);
                glMultMatrixf(xform.begin());
                m_rigid_bodies[i]->collision_shape()->gl_draw(dbCollisionShape_t::kDSSolid);
            }
        }
        
        
        if( stat == M3dView::kActive ||
           stat == M3dView::kLead ||
           stat == M3dView::kHilite ||
           ( style != M3dView::kGouraudShaded && style != M3dView::kFlatShaded ) ) {
            
            glDisable(GL_LIGHTING);
            for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
                if (!m_rigid_bodies[i] ||
                    !m_rigid_bodies[i]->collision_shape()) continue;
                dommy::maya::ScopedLegacyGLMatrix bodyMatrix;
                m_rigid_bodies[i]->get_transform(xform);
                glMultMatrixf(xform.begin());
                m_rigid_bodies[i]->collision_shape()->gl_draw(dbCollisionShape_t::kDSWireframe);
            }
        }
    }
}
DOMMY_DRAW_CATCH("DBRigidBodyArrayNode")

bool DBRigidBodyArrayNode::isBounded() const
{
    return false;
}

MBoundingBox DBRigidBodyArrayNode::boundingBox() const
{
    MPoint corner1(-1, -1, -1);
    MPoint corner2(1, 1, 1);
    return MBoundingBox(corner1, corner2);
}

//standard attributes
void DBRigidBodyArrayNode::destroyRigidBodies()
{
    for(size_t i = 0; i < m_rigid_bodies.size(); ++i)
    {
        dbSolver_t::remove_rigid_body(m_rigid_bodies[i]);
    }
    m_rigid_bodies.clear();
    m_numRigidBodies = 0;
}

MStatus DBRigidBodyArrayNode::computeRigidBodies(
    const MPlug& plug,
    MDataBlock& data)
{
    MStatus status;
    MObject thisObject(thisMObject());
    MPlug plgCollisionShape(thisObject, ia_collisionShape);
    const MDataHandle countHandle =
        data.inputValue(ia_numBodies, &status);
    if (!status) return status;
    std::size_t requestedBodyCount = 0;
    if (!dommy::generation::checkedCount(
            countHandle.asInt(),
            dommy::generation::kMaxGeneratedTransformCount,
            requestedBodyCount)) {
        return MS::kInvalidParameter;
    }
    dbCollisionShape_t::pointer collision_shape;
    status = dommy::bullet::resolveCollisionShapeInput(
        plgCollisionShape, collision_shape);
    if (!status) return status;
    
    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    if (m_rigid_bodies.size() > dommy::generation::kMaxGeneratedTransformCount) {
        return MS::kInvalidParameter;
    }
    // Validate the complete retained set before allocating any replacement body.
    std::vector<vec3f> savedPositions(m_rigid_bodies.size());
    std::vector<quatf> savedRotations(m_rigid_bodies.size());
    for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
        if (!m_rigid_bodies[i] || !m_rigid_bodies[i]->impl()) return MS::kFailure;
        m_rigid_bodies[i]->get_transform(
            savedPositions[i], savedRotations[i]);
        if (!dommy::bullet::isFiniteVec3(savedPositions[i]) ||
            !dommy::bullet::hasValidQuaternion(savedRotations[i])) {
            return MS::kInvalidParameter;
        }
    }

    // Build the replacement set before removing the current bodies. A failed
    // allocation or Bullet factory call therefore leaves the live simulation
    // unchanged.
    std::vector<dbRigidBody_t::pointer> replacementBodies(
        requestedBodyCount);
    for(size_t i = 0; i < replacementBodies.size(); ++i)
    {
        replacementBodies[i] =
            dbSolver_t::create_rigid_body(collision_shape);
        if (!replacementBodies[i]) return MS::kFailure;
        
        if (i < savedPositions.size())
        {
            replacementBodies[i]->set_transform(
                savedPositions[i], savedRotations[i]);
        } else
        {
            const float offset = static_cast<float>(i) * 100.0f;
            replacementBodies[i]->set_transform(
                vec3f(offset, offset, offset),
                quatf(1.f, 0.f, 0.f, 0.f));
        }
    }

    MDataHandle output =
        data.outputValue(ca_rigidBodies, &status);
    if (!status) return status;

    std::size_t registeredReplacementCount = 0U;
    for (; registeredReplacementCount < replacementBodies.size();
         ++registeredReplacementCount) {
        if (!dbSolver_t::add_rigid_body(
                replacementBodies[registeredReplacementCount],
                name().asChar())) {
            for (std::size_t rollbackIndex = 0U;
                 rollbackIndex < registeredReplacementCount;
                 ++rollbackIndex) {
                dbSolver_t::remove_rigid_body(
                    replacementBodies[rollbackIndex]);
            }
            return MS::kFailure;
        }
    }
    for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
        dbSolver_t::remove_rigid_body(m_rigid_bodies[i]);
    }

    m_rigid_bodies.swap(replacementBodies);
    m_numRigidBodies = requestedBodyCount;

    output.set(true);
    output.setClean();
    return data.setClean(plug);
}


MStatus DBRigidBodyArrayNode::computeRigidBodyParam(
    const MPlug& plug,
    MDataBlock& data)
{
    MStatus status;
    MObject thisObject(thisMObject());
    MObject update;
    
    status = MPlug(thisObject, ca_rigidBodies).getValue(update);
    if (!status) return status;

    const MDataHandle massHandle =
        data.inputValue(ia_mass, &status);
    if (!status) return status;
    const MDataHandle restitutionHandle =
        data.inputValue(ia_restitution, &status);
    if (!status) return status;
    const MDataHandle frictionHandle =
        data.inputValue(ia_friction, &status);
    if (!status) return status;
    const MDataHandle linearDampingHandle =
        data.inputValue(ia_linearDamping, &status);
    if (!status) return status;
    const MDataHandle angularDampingHandle =
        data.inputValue(ia_angularDamping, &status);
    if (!status) return status;

    dommy::bullet::RigidBodyParameters parameters;
    if (!dommy::bullet::validateRigidBodyParameters(
            massHandle.asDouble(),
            restitutionHandle.asDouble(),
            frictionHandle.asDouble(),
            linearDampingHandle.asDouble(),
            angularDampingHandle.asDouble(),
            parameters)) {
        return MS::kInvalidParameter;
    }
    
    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    std::vector<vec3f> inertia(m_rigid_bodies.size());
    for (std::size_t i = 0; i < m_rigid_bodies.size(); ++i) {
        if (!m_rigid_bodies[i] || !m_rigid_bodies[i]->collision_shape()) return MS::kFailure;
        if (!dommy::bullet::massScaledInertia(parameters.mass,
                m_rigid_bodies[i]->collision_shape()->local_inertia(), inertia[i]))
            return MS::kInvalidParameter;
    }

    MDataHandle output =
        data.outputValue(ca_rigidBodyParam, &status);
    if (!status) return status;
    
    if (!dbSolver_t::reconfigure_rigid_bodies(m_rigid_bodies, parameters.mass, inertia,
            parameters.mass <= 0, name().asChar())) return MS::kFailure;
    for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
        m_rigid_bodies[i]->set_restitution(parameters.restitution);
        m_rigid_bodies[i]->set_friction(parameters.friction);
        m_rigid_bodies[i]->set_linear_damping(
            parameters.linearDamping);
        m_rigid_bodies[i]->set_angular_damping(
            parameters.angularDamping);
    }
    
    output.set(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBRigidBodyArrayNode::computeWorldMatrix(
    const MPlug& plug,
    MDataBlock& data)
{
    MStatus status;
    MObject thisObject(thisMObject());
    MFnDagNode fnDagNode(thisObject, &status);
    if (!status) return status;
    
    MObject update;
    status = MPlug(thisObject, ca_rigidBodies).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_rigidBodyParam).getValue(update);
    if (!status) return status;
    
    const MObject parent = fnDagNode.parent(0, &status);
    if (!status) return status;
    MFnTransform fnParentTransform(parent, &status);
    if (!status) return status;
    const MTransformationMatrix parentTransformation =
        fnParentTransform.transformation(&status);
    if (!status) return status;
    const MMatrix parentMatrix = parentTransformation.asMatrix();
    if (!dommy::maya::isFiniteMatrix(parentMatrix) ||
        !dommy::maya::isFiniteMatrix(m_worldMatrix)) {
        return MS::kInvalidParameter;
    }

    const MMatrix deltaMatrix =
        parentMatrix * m_worldMatrix.inverse();
    if (!dommy::maya::isFiniteMatrix(deltaMatrix)) {
        return MS::kInvalidParameter;
    }
    MTransformationMatrix deltaTransformation(deltaMatrix);
    const MVector translation =
        deltaTransformation.getTranslation(MSpace::kTransform, &status);
    if (!status) return status;
    if (!dommy::maya::isFloatRepresentableVector(translation)) {
        return MS::kInvalidParameter;
    }
    
    if (m_rigid_bodies.size() >
        dommy::generation::kMaxGeneratedTransformCount) {
        return MS::kInvalidParameter;
    }
    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;

    MArrayDataHandle hInitPosArray =
        data.outputArrayValue(ia_initialPosition, &status);
    if (!status) return status;
    const unsigned int initialPositionCount =
        hInitPosArray.elementCount(&status);
    if (!status) return status;
    if(static_cast<std::size_t>(initialPositionCount) <
       m_rigid_bodies.size()) {
        EchoWarning("DBRigidBodyArrayNode", "World-matrix array size mismatch.");
        return MS::kInvalidParameter;
    }

    struct BodyUpdate {
        dbRigidBody_t::pointer body;
        btRigidBody* nativeBody;
        MDataHandle initialPositionHandle;
        btTransform previousWorld;
        btTransform previousInterpolation;
        std::array<float, 3> previousInitialPosition;
        std::array<float, 3> initialPosition;
        vec3f position;
        quatf rotation;

        BodyUpdate(const dbRigidBody_t::pointer& owner,
                   btRigidBody* native, const MDataHandle& handle)
            : body(owner), nativeBody(native), initialPositionHandle(handle),
              previousWorld(native->getWorldTransform()),
              previousInterpolation(native->getInterpolationWorldTransform())
        {
            const float3& value = handle.asFloat3();
            for (int component = 0; component < 3; ++component) {
                previousInitialPosition[component] = value[component];
            }
        }
    };
    std::vector<BodyUpdate> updates;
    updates.reserve(m_rigid_bodies.size());

    for(size_t i = 0; i < m_rigid_bodies.size(); ++i) {
        if (!m_rigid_bodies[i]) return MS::kFailure;
        DB_RigidBody_t* const implementation =
            dynamic_cast<DB_RigidBody_t*>(m_rigid_bodies[i]->impl());
        if (implementation == nullptr || implementation->body() == nullptr) {
            return MS::kFailure;
        }
        // Solver initialization uses elementByLogicalIndex(i). Physical array
        // positions can instead refer to an unrelated sparse element.
        status = hInitPosArray.jumpToElement(
            static_cast<unsigned int>(i));
        if (!status) return status;
        MDataHandle hInitPos = hInitPosArray.outputValue(&status);
        if (!status) return status;
        const float3 &initialPosition = hInitPos.asFloat3();
        if (!std::isfinite(initialPosition[0]) ||
            !std::isfinite(initialPosition[1]) ||
            !std::isfinite(initialPosition[2])) {
            return MS::kInvalidParameter;
        }
        
        updates.emplace_back(m_rigid_bodies[i], implementation->body(), hInitPos);
        BodyUpdate& candidate = updates.back();
        candidate.body->get_transform(candidate.position, candidate.rotation);
        if (!dommy::bullet::hasValidQuaternion(candidate.rotation)) {
            return MS::kInvalidParameter;
        }
        candidate.position =
            candidate.position +
            vec3f(
                static_cast<float>(translation.x),
                static_cast<float>(translation.y),
                static_cast<float>(translation.z));
        if (!dommy::bullet::isFiniteVector(candidate.position) ||
            !dommy::numeric::checkedFloatCast(
                static_cast<double>(initialPosition[0]) + translation.x,
                candidate.initialPosition[0]) ||
            !dommy::numeric::checkedFloatCast(
                static_cast<double>(initialPosition[1]) + translation.y,
                candidate.initialPosition[1]) ||
            !dommy::numeric::checkedFloatCast(
                static_cast<double>(initialPosition[2]) + translation.z,
                candidate.initialPosition[2])) {
            return MS::kInvalidParameter;
        }
    }

    struct RestoreUpdates {
        std::vector<BodyUpdate>& updates;
        std::size_t touched;
        explicit RestoreUpdates(std::vector<BodyUpdate>& values)
            : updates(values), touched(0U) {}
        ~RestoreUpdates() noexcept
        {
            // Restore exact native frames: set_transform() would overwrite the
            // distinct interpolation frame and round-trip the center-of-mass shift.
            for (std::size_t i = 0; i < touched; ++i) {
                BodyUpdate& previous = updates[i];
                previous.nativeBody->setWorldTransform(previous.previousWorld);
                previous.nativeBody->updateInertiaTensor();
                previous.nativeBody->setInterpolationWorldTransform(
                    previous.previousInterpolation);
                previous.initialPositionHandle.set3Float(
                    previous.previousInitialPosition[0],
                    previous.previousInitialPosition[1],
                    previous.previousInitialPosition[2]);
            }
        }
    } restore(updates);

    for (std::size_t i = 0; i < updates.size(); ++i) {
        BodyUpdate& candidate = updates[i];
        restore.touched = i + 1U;
        candidate.body->set_transform(candidate.position, candidate.rotation);
        candidate.initialPositionHandle.set3Float(
            candidate.initialPosition[0],
            candidate.initialPosition[1],
            candidate.initialPosition[2]);
    }
    status = hInitPosArray.setAllClean();
    if (!status) return status;
    status = data.setClean(plug);
    if (!status) return status;

    m_worldMatrix = parentMatrix;
    restore.touched = 0U;
    return MS::kSuccess;
}

MStatus DBRigidBodyArrayNode::evaluate_rigid_bodies(
    const std::vector<dbRigidBody_t::pointer>*& rigidBodies)
{
    rigidBodies = nullptr;
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_rigidBodies).getValue(updateToken);
    if (!status) {
        return status;
    }
    status = MPlug(thisObject, ca_rigidBodyParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    rigidBodies = &m_rigid_bodies;
    return MS::kSuccess;
}


MStatus DBRigidBodyArrayNode::update()
{
    MObject thisObject(thisMObject());
    
    MObject update;
    MStatus status =
        MPlug(thisObject, ca_rigidBodies).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_rigidBodyParam).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_solver).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, worldMatrix)
        .elementByLogicalIndex(0)
        .getValue(update);
    return status;
}
