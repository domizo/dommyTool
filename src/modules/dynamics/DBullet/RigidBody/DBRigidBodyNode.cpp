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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/rigidBodyNode.cpp

//
//  DBRigidBodyNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBRigidBodyNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaValueValidation.h"



#include "BulletSoftBody/btSoftBody.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DComputeGuard.h"

#include "DBCollisionShapeNode.h"
#include "../Collision/DBCollisionShapeInput.h"
#include "DBRigidBodyParameters.h"
#include "DB_RigidBody_t.h"
#include "Collision/DCollisionMeshInput.h"
#include "DShapeFrameMath.h"

#include <cmath>


#include "dbSolver_t.h"
#include "DBSolverNode.h"

#include "dpoint_ids.h"

MTypeId DBRigidBodyNode::typeId(ID_DB_CONSTRAINT_RIGIDBODY);
const MString DBRigidBodyNode::typeName("DBRigidBodyNode");

MObject DBRigidBodyNode::ia_collisionShape;
MObject DBRigidBodyNode::ia_solver;
MObject DBRigidBodyNode::ia_mass;
MObject DBRigidBodyNode::ia_restitution;
MObject DBRigidBodyNode::ia_friction;
MObject DBRigidBodyNode::ia_linearDamping;
MObject DBRigidBodyNode::ia_angularDamping;
MObject DBRigidBodyNode::ia_initialPosition;
MObject DBRigidBodyNode::ia_initialRotation;
MObject DBRigidBodyNode::ia_initialVelocity;
MObject DBRigidBodyNode::ia_initialSpin;
MObject DBRigidBodyNode::ia_externalForce;
MObject DBRigidBodyNode::ia_externalTorque;

MObject DBRigidBodyNode::ca_rigidBody;
MObject DBRigidBodyNode::ca_rigidBodyParam;
MObject DBRigidBodyNode::ca_solver;

MObject DBRigidBodyNode::oa_contactCount;
MObject DBRigidBodyNode::oa_contactName;
MObject DBRigidBodyNode::oa_contactPosition;

namespace {

MStatus readRigidBodyParameters(
    MDataBlock& data,
    const MObject& massAttribute,
    const MObject& restitutionAttribute,
    const MObject& frictionAttribute,
    const MObject& linearDampingAttribute,
    const MObject& angularDampingAttribute,
    dommy::bullet::RigidBodyParameters& parameters)
{
    MStatus status;
    const MDataHandle mass =
        data.inputValue(massAttribute, &status);
    if (!status) return status;
    const MDataHandle restitution =
        data.inputValue(restitutionAttribute, &status);
    if (!status) return status;
    const MDataHandle friction =
        data.inputValue(frictionAttribute, &status);
    if (!status) return status;
    const MDataHandle linearDamping =
        data.inputValue(linearDampingAttribute, &status);
    if (!status) return status;
    const MDataHandle angularDamping =
        data.inputValue(angularDampingAttribute, &status);
    if (!status) return status;

    if (!dommy::bullet::validateRigidBodyParameters(
            mass.asDouble(),
            restitution.asDouble(),
            friction.asDouble(),
            linearDamping.asDouble(),
            angularDamping.asDouble(),
            parameters)) {
        return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

bool isFiniteQuaternion(const MQuaternion& value)
{
    return dommy::numeric::canRepresentAsFloat(value.x) &&
           dommy::numeric::canRepresentAsFloat(value.y) &&
           dommy::numeric::canRepresentAsFloat(value.z) &&
           dommy::numeric::canRepresentAsFloat(value.w);
}

bool isFiniteScale(const double scale[3])
{
    return dommy::numeric::canRepresentAsFloat(scale[0]) &&
           dommy::numeric::canRepresentAsFloat(scale[1]) &&
           dommy::numeric::canRepresentAsFloat(scale[2]);
}

} // namespace

void* DBRigidBodyNode::creator()
{
    return ::dommy::maya::createObject("DBRigidBodyNode", [&]() {
        return new DBRigidBodyNode();
    });
}



MStatus DBRigidBodyNode::initialize()
try {
    MStatus stat;
    MFnMessageAttribute fnMsgAttr;
    MFnNumericAttribute fnNumericAttr;
    MFnMatrixAttribute fnMatrixAttr;
    MFnTypedAttribute typedAttr;
    
    ia_collisionShape = fnMsgAttr.create("inCollisionShape", "incs", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_collisionShape));
    
    
    ia_solver = fnMsgAttr.create("solver", "solv", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_solver));
    
    
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
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialPosition));
    
    
    ia_initialRotation = fnNumericAttr.createPoint("initialRotation", "inro", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialRotation));
    
    
    ia_initialVelocity = fnNumericAttr.createPoint("initialVelocity", "inve", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialVelocity));
    
    
    ia_initialSpin = fnNumericAttr.createPoint("initialSpin", "insp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialSpin));
    
    
    ia_externalForce = fnNumericAttr.createPoint("externalForce", "exfo", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_externalForce));
    
    
    ia_externalTorque = fnNumericAttr.createPoint("externalTorque", "exto", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_externalTorque));
    
    
    ca_rigidBody = fnNumericAttr.create("ca_rigidBody", "carb", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_rigidBody));
    
    
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
    
    
    oa_contactCount = fnNumericAttr.create("contactCount", "contactCount", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(oa_contactCount));
    
    
    MFnStringArrayData stringArrayData;
    const MObject oa_contactNameDefaultData = stringArrayData.create(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    oa_contactName = typedAttr.create("contactName", "contactName", MFnData::kStringArray, oa_contactNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(oa_contactName));
    
    
    oa_contactPosition = typedAttr.create("contactPosition", "contactPosition", MFnVectorArrayData::kVectorArray, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(typedAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(oa_contactPosition));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_mass, ca_rigidBody));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_collisionShape, ca_rigidBody));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_collisionShape, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_mass, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_restitution, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_friction, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_linearDamping, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_angularDamping, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_initialPosition, ca_rigidBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_solver, ca_solver));
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBRigidBodyNode", "initialize", std::current_exception());
}

DBRigidBodyNode::DBRigidBodyNode() : m_contactCount(0)
{
}

DBRigidBodyNode::~DBRigidBodyNode()
{
    destroyRigidBody();
}

void DBRigidBodyNode::nodeRemoved(MObject& node, void*)
{
    MFnDependencyNode fnNode(node);
    DBRigidBodyNode* rigidBodyNode =
        dommy::maya::checkedUserNode<DBRigidBodyNode>(
            fnNode, DBRigidBodyNode::typeId);
    if (rigidBodyNode) rigidBodyNode->destroyRigidBody();
}




bool DBRigidBodyNode::setInternalValueInContext(
    const MPlug&, const MDataHandle&, MDGContext&)
{
    return false;
}

MStatus DBRigidBodyNode::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus status;
    // Dispatch first: unrelated outputs must not evaluate physics inputs.
    if (!(plug == ca_rigidBody || plug == ca_rigidBodyParam || plug == ca_solver)) {
        const bool isElement = plug.isElement(&status);
        if (!status) return status;
        if (!isElement) return MS::kUnknownParameter;
        const MPlug array = plug.array(&status);
        if (!status) return status;
        if (!(array == worldMatrix)) return MS::kUnknownParameter;
        const unsigned int index = plug.logicalIndex(&status);
        if (!status) return status;
        if (index != 0U) return MS::kUnknownParameter;
    }
    MPlug plgCollisionShape(thisMObject(), ia_collisionShape);
    MPlug plgSolver(thisMObject(), ia_solver);
    const bool hasShape = plgCollisionShape.isConnected(&status);
    if (!status) return status;
    if (!hasShape) return MS::kUnknownParameter;
    const bool hasSolver = plgSolver.isConnected(&status);
    if (!status) return status;
    if (!hasSolver) return MS::kUnknownParameter;

    if (plug == ca_rigidBody) {
        return computeRigidBody(plug, data);
    } else if (plug == ca_rigidBodyParam) {
        return computeRigidBodyParam(plug, data);
    } else if (plug == ca_solver) {
        data.inputValue(ia_solver, &status);
        if (!status) return status;
        MDataHandle output = data.outputValue(ca_solver, &status);
        if (!status) return status;
        output.set(true);
        output.setClean();
        return data.setClean(plug);
    }
    return computeWorldMatrix(plug, data);
}
DOMMY_COMPUTE_CATCH("DBRigidBodyNode")

void DBRigidBodyNode::draw( M3dView & view, const MDagPath&,
                         M3dView::DisplayStyle style,
                         M3dView::DisplayStatus stat ) try
{
    dbSolver_t::LockedSolverAccess solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return;
    
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    
    if (m_rigid_body && m_rigid_body->collision_shape()) {
        //remove the scale, since it's already included in the node transform
        vec3f scale;
        m_rigid_body->collision_shape()->get_scale(scale);

        if (std::fabs(scale[0]) <= FLT_EPSILON ||
            std::fabs(scale[1]) <= FLT_EPSILON ||
            std::fabs(scale[2]) <= FLT_EPSILON) {
            return;
        }
        
        dommy::maya::ScopedLegacyGLMatrix matrix;
        glScalef(1/scale[0], 1/scale[1], 1/scale[2]);
        
        if(style == M3dView::kFlatShaded || style == M3dView::kGouraudShaded) {
            glEnable(GL_LIGHTING);
            if (m_rigid_body->get_mass() > 0.0f) {
                float material[] = { 0.2f, 1.0f, 0.2f, 1.0f };
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material);
            } else {
                float material[] = { 1.0f, 0.3f, 0.1f, 1.0f };
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material);
            }
            m_rigid_body->collision_shape()->gl_draw(dbCollisionShape_t::kDSSolid);
        }
        
        
        if( stat == M3dView::kActive ||
           stat == M3dView::kLead ||
           stat == M3dView::kHilite ||
           ( style != M3dView::kGouraudShaded && style != M3dView::kFlatShaded ) ) {
            
            glDisable(GL_LIGHTING);
            m_rigid_body->collision_shape()->gl_draw(dbCollisionShape_t::kDSWireframe);
            
        }
    }
}
DOMMY_DRAW_CATCH("DBRigidBodyNode")

bool DBRigidBodyNode::isBounded() const
{
    return false;
}

MBoundingBox DBRigidBodyNode::boundingBox() const
{
    MPoint corner1(-1, -1, -1);
    MPoint corner2(1, 1, 1);
    return MBoundingBox(corner1, corner2);
}

void DBRigidBodyNode::destroyRigidBody()
{
    if (m_rigid_body)
    {
        dbSolver_t::remove_rigid_body(m_rigid_body);
        m_rigid_body.reset();
    }
}

// Rigid bodies are added to the solver here.
MStatus DBRigidBodyNode::computeRigidBody(const MPlug& plug, MDataBlock& data)
{
    MStatus status;
    MObject thisObject(thisMObject());
    MPlug plgCollisionShape(thisObject, ia_collisionShape);

    dbCollisionShape_t::pointer collision_shape;
    status = dommy::bullet::resolveCollisionShapeInput(
        plgCollisionShape, collision_shape);
    if (!status) return status;

    // once at creation/load time : get transform from Maya transform node
    MFnDagNode fnDagNode(thisObject, &status);
    if (!status) return status;
    const MObject parent = fnDagNode.parent(0, &status);
    if (!status) return status;
    MFnTransform fnTransform(parent, &status);
    if (!status) return status;
    const MVector translation =
        fnTransform.getTranslation(MSpace::kTransform, &status);
    if (!status) return status;
    
    MQuaternion rotation;
    status = fnTransform.getRotation(
        rotation, MSpace::kTransform);
    if (!status) return status;
    double scale[3] = {1.0, 1.0, 1.0};
    status = fnTransform.getScale(scale);
    if (!status) return status;

    if (!dommy::maya::isFloatRepresentableVector(translation) ||
        !isFiniteQuaternion(rotation) || !isFiniteScale(scale)) {
        return MS::kInvalidParameter;
    }

    dommy::bullet::RigidBodyParameters parameters;
    status = readRigidBodyParameters(
        data,
        ia_mass,
        ia_restitution,
        ia_friction,
        ia_linearDamping,
        ia_angularDamping,
        parameters);
    if (!status) return status;

    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    collision_shape_scale_transaction<dbCollisionShape_t> scaleUpdate(*collision_shape,
        vec3f(
            static_cast<float>(scale[0]),
            static_cast<float>(scale[1]),
            static_cast<float>(scale[2])));
    vec3f inertia;
    if (!scaleUpdate || !dommy::bullet::massScaledInertia(
            parameters.mass, collision_shape->local_inertia(), inertia)) {
        return MS::kInvalidParameter;
    }
    dbRigidBody_t::pointer newRigidBody =
        dbSolver_t::create_rigid_body(collision_shape);
    if (!newRigidBody) return MS::kFailure;

    newRigidBody->set_transform(
        vec3f(
            static_cast<float>(translation.x),
            static_cast<float>(translation.y),
            static_cast<float>(translation.z)),
        quatf(
            static_cast<float>(rotation.w),
            static_cast<float>(rotation.x),
            static_cast<float>(rotation.y),
            static_cast<float>(rotation.z)));
    if (!newRigidBody->set_mass_properties(parameters.mass, inertia, parameters.mass <= 0))
        return MS::kInvalidParameter;
    newRigidBody->set_restitution(parameters.restitution);
    newRigidBody->set_friction(parameters.friction);
    newRigidBody->set_linear_damping(parameters.linearDamping);
    newRigidBody->set_angular_damping(parameters.angularDamping);

    if (!dbSolver_t::add_rigid_body(
            newRigidBody, name().asChar())) {
        return MS::kFailure;
    }
    // Failed validation/publication restores the provisional shape under lock.
    scaleUpdate.accept();
    dbRigidBody_t::pointer previousRigidBody = m_rigid_body;
    if (previousRigidBody) {
        dbSolver_t::remove_rigid_body(previousRigidBody);
    }
    m_rigid_body = newRigidBody;
    return data.setClean(plug);
}


MStatus DBRigidBodyNode::computeWorldMatrix(const MPlug& plug, MDataBlock& data)
{
    MObject thisObject(thisMObject());
    MStatus stat;
    MFnDagNode fnDagNode(thisObject, &stat);
    if (!stat) return stat;
    
    MObject update;
    stat = MPlug(thisObject, ca_rigidBody).getValue(update);
    if (!stat) return stat;
    stat = MPlug(thisObject, ca_rigidBodyParam).getValue(update);
    if (!stat) return stat;

    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    // Dependency evaluation can replace the cached body. Pin the result only
    // after both pulls, and retain it throughout preparation and restoration.
    dbRigidBody_t::pointer body = m_rigid_body;
    if (!body || !body->collision_shape()) return MS::kFailure;
    DB_RigidBody_t* const implementation = dynamic_cast<DB_RigidBody_t*>(body->impl());
    if (implementation == nullptr || implementation->body() == nullptr) return MS::kFailure;
    
    vec3f pos;
    quatf rot;
    
    const MObject parent = fnDagNode.parent(0, &stat);
    if (!stat) return stat;
    MFnTransform fnParentTransform(parent, &stat);
    if (!stat) return stat;
    
    double mscale[3];
    stat = fnParentTransform.getScale(mscale);
    if (!stat) return stat;
    if (!isFiniteScale(mscale)) return MS::kInvalidParameter;
    body->get_transform(pos, rot);
    if (!dommy::bullet::isFiniteVec3(pos) ||
        !dommy::bullet::hasValidQuaternion(rot)) {
        return MS::kFailure;
    }

    dommy::bullet::RigidBodyParameters parameters;
    stat = readRigidBodyParameters(
        data,
        ia_mass,
        ia_restitution,
        ia_friction,
        ia_linearDamping,
        ia_angularDamping,
        parameters);
    if (!stat) return stat;
    
    
    const bool atStart = DBSolverNode::isStartFrame();
    const bool restoreParent = !atStart && parameters.mass > 0.0F;
    MVector mtranslation;
    MQuaternion mrotation;
    bool moveBody = false;
    vec3f nextPosition = pos;
    quatf nextRotation = rot;
    vec3f initialRotation(0, 0, 0);
    std::vector<MDataHandle> initialHandles;
    std::vector<std::function<void()> > restoreConstraints;
    if (atStart || restoreParent) {
        mtranslation = fnParentTransform.getTranslation(MSpace::kTransform, &stat);
        if (!stat) return stat;
        stat = fnParentTransform.getRotation(
            mrotation, MSpace::kTransform);
        if (!stat) return stat;
        if (!dommy::maya::isFloatRepresentableVector(mtranslation) ||
            !isFiniteQuaternion(mrotation)) {
            return MS::kInvalidParameter;
        }
    }
    if (atStart) {
        nextPosition = vec3f(static_cast<float>(mtranslation.x),
            static_cast<float>(mtranslation.y), static_cast<float>(mtranslation.z));
        nextRotation = quatf(static_cast<float>(mrotation.w), static_cast<float>(mrotation.x),
            static_cast<float>(mrotation.y), static_cast<float>(mrotation.z));
        if (!dommy::bullet::hasValidQuaternion(nextRotation)) return MS::kInvalidParameter;
        double deltaSquared = 0.0;
        for (int i = 0; i < 3; ++i) {
            const double delta = static_cast<double>(nextPosition[i]) - pos[i];
            deltaSquared += delta * delta;
        }
        for (int i = 0; i < 4; ++i) {
            const double delta = static_cast<double>(nextRotation[i]) - rot[i];
            deltaSquared += delta * delta;
        }
        moveBody = deltaSquared > FLT_EPSILON;
        if (moveBody) {
            const MEulerRotation euler = mrotation.asEulerRotation();
            if (!dommy::numeric::checkedFloatCast(rad2deg(euler.x), initialRotation[0]) ||
                !dommy::numeric::checkedFloatCast(rad2deg(euler.y), initialRotation[1]) ||
                !dommy::numeric::checkedFloatCast(rad2deg(euler.z), initialRotation[2])) {
                return MS::kInvalidParameter;
            }
            initialHandles.reserve(2U);
            initialHandles.push_back(data.outputValue(ia_initialPosition, &stat));
            if (!stat) return stat;
            initialHandles.push_back(data.outputValue(ia_initialRotation, &stat));
            if (!stat) return stat;
            restoreConstraints = implementation->capture_constraint_frames();
        }
    }

    // Parameter updates form the existing coherent shape/mass transaction.
    // Complete them before starting the separate pose/output transaction.
    collision_shape_scale_transaction<dbCollisionShape_t> scaleUpdate(
        *body->collision_shape(),
            vec3f(static_cast<float>(mscale[0]), static_cast<float>(mscale[1]), static_cast<float>(mscale[2])));
    vec3f inertia;
    if (!scaleUpdate || !dommy::bullet::massScaledInertia(
            parameters.mass, body->collision_shape()->local_inertia(), inertia)) {
        return MS::kInvalidParameter;
    }

    if (!dbSolver_t::reconfigure_rigid_body(
                body,
                parameters.mass,
                inertia,
                parameters.mass <= 0.0F,
                name().asChar())) {
        return MS::kFailure;
    }
    scaleUpdate.accept();
    
    body->set_restitution(parameters.restitution);
    body->set_friction(parameters.friction);
    body->set_linear_damping(parameters.linearDamping);
    body->set_angular_damping(parameters.angularDamping);

    struct RestorePose {
        btRigidBody* native;
        btTransform world, interpolation;
        std::vector<MDataHandle>& handles;
        std::vector<std::function<void()> >& constraints;
        float initial[2][3];
        MFnTransform& parent;
        MVector translation;
        MQuaternion rotation;
        bool bodyTouched = false, parentTouched = false, accepted = false;
        RestorePose(btRigidBody* value, std::vector<MDataHandle>& h,
                    std::vector<std::function<void()> >& c, MFnTransform& p,
                    const MVector& t, const MQuaternion& r)
            : native(value), world(value->getWorldTransform()),
              interpolation(value->getInterpolationWorldTransform()),
              handles(h), constraints(c), parent(p), translation(t), rotation(r)
        {
            for (std::size_t i = 0; i < handles.size(); ++i) {
                const float3& previous = handles[i].asFloat3();
                for (int axis = 0; axis < 3; ++axis) initial[i][axis] = previous[axis];
            }
        }
        ~RestorePose() noexcept
        {
            if (accepted) return;
            if (bodyTouched) {
                native->setWorldTransform(world);
                native->updateInertiaTensor();
                native->setInterpolationWorldTransform(interpolation);
                for (auto& restore : constraints) restore();
                for (std::size_t i = 0; i < handles.size(); ++i) {
                    handles[i].set3Float(initial[i][0], initial[i][1], initial[i][2]);
                }
            }
            if (parentTouched) {
                // Maya transform setters may themselves fail. Attempt both,
                // without throwing from cleanup; target-host verification is required.
                try { (void)parent.setTranslation(translation, MSpace::kTransform); } catch (...) {}
                try { (void)parent.setRotation(rotation); } catch (...) {}
            }
        }
    } restore(implementation->body(), initialHandles, restoreConstraints,
              fnParentTransform, mtranslation, mrotation);

    if (moveBody) {
        restore.bodyTouched = true;
        body->set_transform(nextPosition, nextRotation);
        body->update_constraint();
        initialHandles[0].set3Float(nextPosition[0], nextPosition[1], nextPosition[2]);
        initialHandles[1].set3Float(initialRotation[0], initialRotation[1], initialRotation[2]);
    } else if (restoreParent) {
        restore.parentTouched = true;
        stat = fnParentTransform.setTranslation(MVector(pos[0], pos[1], pos[2]), MSpace::kTransform);
        if (!stat) return stat;
        stat = fnParentTransform.setRotation(MQuaternion(rot[1], rot[2], rot[3], rot[0]));
        if (!stat) return stat;
    }
    stat = data.setClean(plug);
    if (!stat) return stat;
    restore.accepted = true;
    return MS::kSuccess;
}


MStatus DBRigidBodyNode::computeRigidBodyParam(const MPlug& plug, MDataBlock& data)
{
    if (!m_rigid_body || !m_rigid_body->collision_shape()) {
        return MS::kFailure;
    }
    
    MObject thisObject(thisMObject());
    MObject update;
    
    MStatus status =
        MPlug(thisObject, ca_rigidBody).getValue(update);
    if (!status) return status;

    dommy::bullet::RigidBodyParameters parameters;
    status = readRigidBodyParameters(
        data,
        ia_mass,
        ia_restitution,
        ia_friction,
        ia_linearDamping,
        ia_angularDamping,
        parameters);
    if (!status) return status;

    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    vec3f inertia;
    if (!dommy::bullet::massScaledInertia(parameters.mass,
            m_rigid_body->collision_shape()->local_inertia(), inertia)) {
        return MS::kInvalidParameter;
    }

    MDataHandle output =
        data.outputValue(ca_rigidBodyParam, &status);
    if (!status) return status;
    
    if (!dbSolver_t::reconfigure_rigid_body(
                m_rigid_body,
                parameters.mass,
                inertia,
                parameters.mass <= 0.0F,
                name().asChar())) {
        return MS::kFailure;
    }
    
    m_rigid_body->set_restitution(parameters.restitution);
    m_rigid_body->set_friction(parameters.friction);
    m_rigid_body->set_linear_damping(parameters.linearDamping);
    m_rigid_body->set_angular_damping(parameters.angularDamping);
    
    output.set(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBRigidBodyNode::evaluate_rigid_body(
    dbRigidBody_t::pointer& rigidBody)
{
    rigidBody.reset();
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_rigidBody).getValue(updateToken);
    if (!status) {
        return status;
    }
    status = MPlug(thisObject, ca_rigidBodyParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    if (!m_rigid_body) {
        return MS::kFailure;
    }
    rigidBody = m_rigid_body;
    return MS::kSuccess;
}

MStatus DBRigidBodyNode::update()
{
    MObject thisObject(thisMObject());
    
    MObject update;
    
    MStatus status =
        MPlug(thisObject, ca_rigidBody).getValue(update);
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

MStatus DBRigidBodyNode::clearContactInfo()
{
    MStatus status;
    MObject thisObject(thisMObject());

    MPlug plugContactCount(thisObject, DBRigidBodyNode::oa_contactCount);
    MPlug plugContactName(thisObject, DBRigidBodyNode::oa_contactName);
    MPlug plugContactPosition(
        thisObject, DBRigidBodyNode::oa_contactPosition);
    if (plugContactCount.isNull() ||
        plugContactName.isNull() ||
        plugContactPosition.isNull()) {
        return MS::kFailure;
    }

    MStringArray stringArray;
    MFnStringArrayData stringArrayData;
    MObject strArrObject =
        stringArrayData.create(stringArray, &status);
    if (!status) return status;

    MVectorArray vectorArray;
    MFnVectorArrayData vectorArrayData;
    MObject arrObject = vectorArrayData.create(vectorArray, &status);
    if (!status) return status;

    status = plugContactName.setValue(strArrObject);
    if (!status) return status;
    status = plugContactPosition.setValue(arrObject);
    if (!status) return status;
    status = plugContactCount.setValue(0);
    if (!status) return status;
    m_contactCount = 0;
    return MS::kSuccess;
}

MStatus DBRigidBodyNode::addContactInfo(
    const MString& contactObjectName,
    const MVector& point)
{
    if (!dommy::maya::isFloatRepresentableVector(point) ||
        m_contactCount < 0 ||
        static_cast<std::size_t>(m_contactCount) >=
            dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MObject thisObject(thisMObject());
    MPlug plugContactCount(thisObject, DBRigidBodyNode::oa_contactCount);
    MPlug plugContactName(thisObject, DBRigidBodyNode::oa_contactName);
    MPlug plugContactPosition(thisObject, DBRigidBodyNode::oa_contactPosition);
    if (plugContactCount.isNull() ||
        plugContactName.isNull() ||
        plugContactPosition.isNull()) {
        return MS::kFailure;
    }

    int publishedCount = 0;
    status = plugContactCount.getValue(publishedCount);
    if (!status) return status;
    if (publishedCount != m_contactCount) return MS::kInvalidParameter;

    MObject strArrObject;
    status = plugContactName.getValue(strArrObject);
    if (!status) return status;
    MFnStringArrayData stringArrayData(strArrObject, &status);
    if (!status) return status;
    const unsigned int nameCount = stringArrayData.length(&status);
    if (!status) return status;
    if (nameCount != static_cast<unsigned int>(m_contactCount))
        return MS::kInvalidParameter;

    MObject arrObject;
    status = plugContactPosition.getValue(arrObject);
    if (!status) return status;
    MFnVectorArrayData vectorArrayData(arrObject, &status);
    if (!status) return status;
    const unsigned int positionCount = vectorArrayData.length(&status);
    if (!status) return status;
    if (positionCount != nameCount) return MS::kInvalidParameter;

    // array() returns an editable alias of the published Maya data. Copy into
    // detached storage so preparation failure cannot mutate the live arrays.
    MStringArray stringArray;
    status = stringArrayData.copyTo(stringArray);
    if (!status) return status;
    MVectorArray vectorArray;
    status = vectorArrayData.copyTo(vectorArray);
    if (!status) return status;
    status = stringArray.append(contactObjectName);
    if (!status) return status;
    status = vectorArray.append(point);
    if (!status) return status;

    MFnStringArrayData newStringArrayData;
    const MObject newStrArrObject =
        newStringArrayData.create(stringArray, &status);
    if (!status) return status;
    MFnVectorArrayData newVectorArrayData;
    const MObject newArrObject =
        newVectorArrayData.create(vectorArray, &status);
    if (!status) return status;

    // All preparation is complete. Host writes may still fail partway through;
    // the caller must propagate failure and require solver reinitialization.
    status = plugContactName.setValue(newStrArrObject);
    if (!status) return status;
    status = plugContactPosition.setValue(newArrObject);
    if (!status) return status;

    const int nextContactCount = m_contactCount + 1;
    status = plugContactCount.setValue(nextContactCount);
    if (!status) return status;
    m_contactCount = nextContactCount;
    return MS::kSuccess;
}
