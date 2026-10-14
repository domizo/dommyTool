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
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/nailConstraintNode.cpp

//
//  DBNailConstraintNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBNailConstraintNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DBConstraintNodeUtils.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaUserNode.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DBRigidBodyNode.h"
#include "dbRigidBody_t.h"

#include "dbSolver_t.h"
#include "DBSolverNode.h"
#include "DB_Nail_Constraint_t.h"
#include "BulletSoftBody/btSoftBody.h"


#include "dpoint_ids.h"

MTypeId DBNailConstraintNode::typeId(ID_DB_CONSTRAINT_NAIL);
const MString DBNailConstraintNode::typeName("DBNailConstraintNode");

MObject DBNailConstraintNode::ia_rigidBodyA;
MObject DBNailConstraintNode::ia_rigidBodyB;
MObject DBNailConstraintNode::ia_damping;
MObject DBNailConstraintNode::ia_breakThreshold;
MObject DBNailConstraintNode::ia_disableCollide;
MObject DBNailConstraintNode::ia_pivotInA;
MObject DBNailConstraintNode::ia_pivotInB;
MObject DBNailConstraintNode::ca_constraint;
MObject DBNailConstraintNode::ca_constraintParam;


MStatus DBNailConstraintNode::initialize()
try {
    MStatus stat;
    MFnMessageAttribute fnMsgAttr;
    MFnNumericAttribute fnNumericAttr;
    MFnMatrixAttribute fnMatrixAttr;
    
    ia_rigidBodyA = fnMsgAttr.create("inRigidBodyA", "inrbA", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_rigidBodyA));
    
    
    ia_rigidBodyB = fnMsgAttr.create("inRigidBodyB", "inrbB", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_rigidBodyB));
    
    
    ia_damping = fnNumericAttr.create("damping", "dmp", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_damping));
    
    
    ia_breakThreshold = fnNumericAttr.create("breakThreshold", "brkThrsh", MFnNumericData::kDouble, 100.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_breakThreshold));
    
    
    ia_disableCollide = fnNumericAttr.create("disableCollide", "dsblColl", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_disableCollide));
    
    
    ia_pivotInA = fnNumericAttr.createPoint("pivotInA", "piva", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_pivotInA));
    
    
    ia_pivotInB = fnNumericAttr.createPoint("pivotInB", "pivb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_pivotInB));
    
    
    ca_constraint = fnNumericAttr.create("ca_constraint", "caco", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_constraint));
    
    
    ca_constraintParam = fnNumericAttr.create("ca_constraintParam", "cacop", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_constraintParam));
    
    
    
    stat = attributeAffects(ia_rigidBodyA, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    stat = attributeAffects(ia_rigidBodyA, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rigidBodyB, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rigidBodyB, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_pivotInA, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_pivotInA, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_pivotInB, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_pivotInB, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_damping, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_breakThreshold, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_disableCollide, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_disableCollide, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBNailConstraintNode", "initialize", std::current_exception());
}

DBNailConstraintNode::DBNailConstraintNode()
    : m_pivInA(0.0F, 0.0F, 0.0F)
    , m_pivInB(0.0F, 0.0F, 0.0F)
{
}

DBNailConstraintNode::~DBNailConstraintNode()
{
    destroyConstraint();
}

void DBNailConstraintNode::nodeRemoved(MObject& node, void*)
{
    MFnDependencyNode fnNode(node);
    DBNailConstraintNode *pNode =
        dommy::maya::checkedUserNode<DBNailConstraintNode>(
            fnNode, DBNailConstraintNode::typeId);
    if (!pNode) return;
    pNode->destroyConstraint();
}

void* DBNailConstraintNode::creator()
{
    return ::dommy::maya::createObject("DBNailConstraintNode", [&]() {
        return new DBNailConstraintNode();
    });
}


bool DBNailConstraintNode::setInternalValueInContext(
    const MPlug&, const MDataHandle&, MDGContext&)
{
    return false;
}


MStatus DBNailConstraintNode::compute(const MPlug& plug, MDataBlock& data) try
{
    if(plug == ca_constraint) {
        return computeConstraint(plug, data);
    } else if(plug == ca_constraintParam) {
        return computeConstraintParam(plug, data);
    } else if(plug.isElement()) {
        if(plug.array() == worldMatrix && plug.logicalIndex() == 0) {
            return computeWorldMatrix(plug, data);
        } else {
            return MStatus::kUnknownParameter;
        }
    } else {
        return MStatus::kUnknownParameter;
    }
}
DOMMY_COMPUTE_CATCH("DBNailConstraintNode")

void DBNailConstraintNode::draw( M3dView & view, const MDagPath&,
                              M3dView::DisplayStyle style,
                              M3dView::DisplayStatus stat ) try
{
    dbSolver_t::LockedSolverAccess solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return;
    
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    
    if( !(stat == M3dView::kActive ||
          stat == M3dView::kLead ||
          stat == M3dView::kHilite ||
          ( style != M3dView::kGouraudShaded && style != M3dView::kFlatShaded )) ) {
        glColor3f(1.0, 1.0, 0.0);
    }
    vec3f posA(0.0f, 0.0f, 0.0f);
    vec3f posB(0.0f, 0.0f, 0.0f);
    dbRigidBody_t::pointer rigid_bodyB = nullptr;
    vec3f world(0.0f, 0.0f, 0.0f);
    if (m_constraint) {
        m_constraint->get_world(world);
        vec3f posT;
        quatf rotT;
        m_constraint->rigid_bodyA()->get_transform(posT, rotT);
        posA = posT - world;
        rigid_bodyB = m_constraint->rigid_bodyB();
        if(rigid_bodyB)
        {
            rigid_bodyB->get_transform(posT, rotT);
            posB = posT - world;
        }
    }
    
    glBegin(GL_LINES);
    
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(posA[0], posA[1], posA[2]);
    
    if(rigid_bodyB)
    {
        vec3f pivB;
        m_constraint->get_world_pivotB(pivB);
        for(int j = 0; j < 3; j++) pivB[j] -= world[j];
        glVertex3f(pivB[0], pivB[1], pivB[2]);
        glVertex3f(posB[0], posB[1], posB[2]);
        glVertex3f(-0.2f+pivB[0],  0.0f+pivB[1],  0.0f+pivB[2]);
        glVertex3f( 0.2f+pivB[0],  0.0f+pivB[1],  0.0f+pivB[2]);
        glVertex3f( 0.0f+pivB[0], -0.2f+pivB[1],  0.0f+pivB[2]);
        glVertex3f( 0.0f+pivB[0],  0.2f+pivB[1],  0.0f+pivB[2]);
        glVertex3f( 0.0f+pivB[0],  0.0f+pivB[1], -0.2f+pivB[2]);
        glVertex3f( 0.0f+pivB[0],  0.0f+pivB[1],  0.2f+pivB[2]);
    }
    
    
    glVertex3f(-0.2, 0.0, 0.0);
    glVertex3f(0.2, 0.0, 0.0);
    
    glVertex3f(0.0, -0.2, 0.0);
    glVertex3f(0.0, 0.2, 0.0);
    
    glVertex3f(0.0, 0.0, -0.2);
    glVertex3f(0.0, 0.0, 0.2);
    glEnd();
    
}
DOMMY_DRAW_CATCH("DBNailConstraintNode")

bool DBNailConstraintNode::isBounded() const
{
    return false;
}

MBoundingBox DBNailConstraintNode::boundingBox() const
{
    MPoint corner1(-1, -1, -1);
    MPoint corner2(1, 1, 1);
    return MBoundingBox(corner1, corner2);
}

void DBNailConstraintNode::destroyConstraint()
{
    dbConstraint_t::pointer constraint = static_cast<dbConstraint_t::pointer>(m_constraint);
    dbSolver_t::remove_constraint(constraint);
    m_constraint.reset();
}

//standard attributes
MStatus DBNailConstraintNode::computeConstraint(
    const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject = thisMObject();
    dbRigidBody_t::pointer rigidBodyA;
    MStatus status = dommy::bullet::constraint_node::connectedRigidBody(
        thisObject, ia_rigidBodyA, true, rigidBodyA);
    if (!status) {
        return status;
    }
    dbRigidBody_t::pointer rigidBodyB;
    status = dommy::bullet::constraint_node::connectedRigidBody(
        thisObject, ia_rigidBodyB, false, rigidBodyB);
    if (!status) {
        return status;
    }

    vec3f pivotA;
    status = dommy::bullet::constraint_node::finiteVector(
        data, ia_pivotInA, pivotA);
    if (!status) {
        return status;
    }
    vec3f pivotB;
    status = dommy::bullet::constraint_node::finiteVector(
        data, ia_pivotInB, pivotB);
    if (!status) {
        return status;
    }
    bool disableCollisions = false;
    status = dommy::bullet::constraint_node::boolean(
        data, ia_disableCollide, disableCollisions);
    if (!status) {
        return status;
    }

    dbNail_constraint_t::pointer replacement =
        rigidBodyB
            ? dbSolver_t::create_nail_constraint(
                  rigidBodyA, rigidBodyB, pivotA, pivotB)
            : dbSolver_t::create_nail_constraint(rigidBodyA, pivotA);
    status = applyConstraintParameters(data, replacement);
    if (!status) return status;
    MDataHandle output = data.outputValue(ca_constraint, &status);
    if (!status) return status;
    status = dommy::bullet::constraint_node::commitConstraint(
        m_constraint, replacement, disableCollisions);
    if (!status) {
        return status;
    }
    m_pivInA = pivotA;
    m_pivInB = pivotB;
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBNailConstraintNode::computeWorldMatrix(
    const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject = thisMObject();
    MObject update;
    MStatus status = MPlug(thisObject, ca_constraint).getValue(update);
    if (!status) {
        return status;
    }
    status = MPlug(thisObject, ca_constraintParam).getValue(update);
    if (!status) {
        return status;
    }
    if (!m_constraint) {
        return MS::kFailure;
    }

    MFnDagNode dagNode(thisObject, &status);
    if (!status) {
        return status;
    }
    if (dagNode.parentCount(&status) == 0U || !status) {
        return status ? MStatus(MS::kFailure) : status;
    }
    const MObject parent = dagNode.parent(0U, &status);
    if (!status) {
        return status;
    }
    MFnTransform parentTransform(parent, &status);
    if (!status) {
        return status;
    }
    const MVector translation =
        parentTransform.getTranslation(MSpace::kTransform, &status);
    vec3f translatedPosition;
    if (!status || !dommy::bullet::constraint_node::checkedPosition(
            translation, translatedPosition)) {
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    status = parentTransform.setRotation(MEulerRotation(0.0, 0.0, 0.0));
    if (!status) {
        return status;
    }
    double fixedScale[3] = {1.0, 1.0, 1.0};
    status = parentTransform.setScale(fixedScale);
    if (!status) {
        return status;
    }

    if (DBSolverNode::isStartFrame()) {
        dbRigidBody_t::pointer rigidBodyA;
        status = dommy::bullet::constraint_node::connectedRigidBody(
            thisObject, ia_rigidBodyA, true, rigidBodyA);
        if (!status) {
            return status;
        }
        dbRigidBody_t::pointer rigidBodyB;
        status = dommy::bullet::constraint_node::connectedRigidBody(
            thisObject, ia_rigidBodyB, false, rigidBodyB);
        if (!status) {
            return status;
        }
        (void)rigidBodyA;
        (void)rigidBodyB;

        auto solverAccess = dbSolver_t::lock_solver();
        if (!solverAccess) return MS::kFailure;
        bool updatePivot = m_constraint->getPivotChanged();
        if (!updatePivot) {
            vec3f world;
            m_constraint->get_world(world);
            if (!dommy::bullet::isFiniteVec3(world)) {
                return MS::kInvalidParameter;
            }
            const double deltaX =
                double(world[0]) - translatedPosition[0];
            const double deltaY =
                double(world[1]) - translatedPosition[1];
            const double deltaZ =
                double(world[2]) - translatedPosition[2];
            const double deltaSquared =
                deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
            updatePivot = deltaSquared > FLT_EPSILON;
        }
        if (updatePivot) {
            const vec3f world(
                translatedPosition[0],
                translatedPosition[1],
                translatedPosition[2]);
            if (!m_constraint->set_world(world)) return MS::kInvalidParameter;
            m_constraint->get_pivotA(m_pivInA);
            m_constraint->get_pivotB(m_pivInB);
            if (!dommy::bullet::isFiniteVec3(m_pivInA) ||
                !dommy::bullet::isFiniteVec3(m_pivInB)) {
                return MS::kInvalidParameter;
            }

            MDataHandle pivotAHandle =
                data.outputValue(ia_pivotInA, &status);
            if (!status) {
                return status;
            }
            MDataHandle pivotBHandle =
                data.outputValue(ia_pivotInB, &status);
            if (!status) {
                return status;
            }
            float3& pivotAValue = pivotAHandle.asFloat3();
            float3& pivotBValue = pivotBHandle.asFloat3();
            for (int component = 0; component < 3; ++component) {
                pivotAValue[component] = m_pivInA[component];
                pivotBValue[component] = m_pivInB[component];
            }
            m_constraint->setPivotChanged(false);
        }
    } else {
        vec3f world;
        m_constraint->get_world(world);
        if (!dommy::bullet::isFiniteVec3(world)) {
            return MS::kInvalidParameter;
        }
        status = parentTransform.setTranslation(
            MVector(world[0], world[1], world[2]),
            MSpace::kTransform);
        if (!status) {
            return status;
        }
    }

    return data.setClean(plug);
}

MStatus DBNailConstraintNode::computeConstraintParam(
    const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject = thisMObject();
    MObject update;
    MStatus status = MPlug(thisObject, ca_constraint).getValue(update);
    if (!status) {
        return status;
    }
    if (!m_constraint) {
        return MS::kFailure;
    }

    MDataHandle output = data.outputValue(ca_constraintParam, &status);
    if (!status) return status;
    status = applyConstraintParameters(data, m_constraint);
    if (!status) return status;
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBNailConstraintNode::applyConstraintParameters(
    MDataBlock& data, const dbNail_constraint_t::pointer& constraint)
{
    if (!constraint) return MS::kFailure;
    MStatus status;

    float damping = 0.0F;
    status = dommy::bullet::constraint_node::finiteFloat(
        data, ia_damping, damping);
    if (!status) {
        return status;
    }
    float breakThreshold = 0.0F;
    status = dommy::bullet::constraint_node::finiteFloat(
        data, ia_breakThreshold, breakThreshold);
    if (!status) {
        return status;
    }
    if (damping < 0.0F || damping > 1.0F ||
        breakThreshold < 0.0F) {
        return MS::kInvalidParameter;
    }
    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    constraint->set_damping(damping);
    constraint->set_breakThreshold(breakThreshold);
    return MS::kSuccess;
}

MStatus DBNailConstraintNode::evaluate_constraint(
    dbNail_constraint_t::pointer& constraint)
{
    constraint.reset();
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_constraint).getValue(updateToken);
    if (!status) {
        return status;
    }
    status =
        MPlug(thisObject, ca_constraintParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    if (!m_constraint) {
        return MS::kFailure;
    }
    constraint = m_constraint;
    return MS::kSuccess;
}

MStatus DBNailConstraintNode::update()
{
    MObject thisObject(thisMObject());
    
    MObject update;
    MStatus status =
        MPlug(thisObject, ca_constraint).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_constraintParam).getValue(update);
    if (!status) return status;
    return MPlug(thisObject, worldMatrix)
        .elementByLogicalIndex(0)
        .getValue(update);
}
