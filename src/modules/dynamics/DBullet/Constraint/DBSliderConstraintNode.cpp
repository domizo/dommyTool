// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Herbert Law
 
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
 
Written by: Herbert Law <Herbert.Law@gmail.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/sliderConstraintNode.cpp

//
//  DBSliderConstraintNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBSliderConstraintNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DBConstraintNodeUtils.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaUserNode.h"

#include "BulletSoftBody/btSoftBody.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DBRigidBodyNode.h"
#include "DBSliderConstraintNode.h"


#include "dbSolver_t.h"
#include "DBSolverNode.h"
#include "DB_Slider_Constraint_t.h"


#include "dpoint_ids.h"



MTypeId DBSliderConstraintNode::typeId(ID_DB_CONSTRAINT_SLIDER);
const MString DBSliderConstraintNode::typeName("DBSliderConstraintNode");

MObject DBSliderConstraintNode::ia_rigidBodyA;
MObject DBSliderConstraintNode::ia_rigidBodyB;
MObject DBSliderConstraintNode::ia_damping;
MObject DBSliderConstraintNode::ia_breakThreshold;
MObject DBSliderConstraintNode::ia_disableCollide;
MObject DBSliderConstraintNode::ca_constraint;
MObject DBSliderConstraintNode::ca_constraintParam;
MObject DBSliderConstraintNode::ia_lowerLinLimit;
MObject DBSliderConstraintNode::ia_upperLinLimit;
MObject DBSliderConstraintNode::ia_lowerAngLimit;
MObject DBSliderConstraintNode::ia_upperAngLimit;
MObject DBSliderConstraintNode::ia_rotationInA;
MObject DBSliderConstraintNode::ia_rotationInB;
MObject DBSliderConstraintNode::ia_pivotInA;
MObject DBSliderConstraintNode::ia_pivotInB;

void* DBSliderConstraintNode::creator()
{
    return ::dommy::maya::createObject("DBSliderConstraintNode", [&]() {
        return new DBSliderConstraintNode();
    });
}


MStatus DBSliderConstraintNode::initialize()
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
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(1.0));
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
    
    
    ia_lowerLinLimit = fnNumericAttr.create("lowerLinLimit", "lllt", MFnNumericData::kDouble, 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_lowerLinLimit));
    
    
    ia_upperLinLimit = fnNumericAttr.create("upperLinLimit", "ullt", MFnNumericData::kDouble, -1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_upperLinLimit));
    
    
    ia_lowerAngLimit = fnNumericAttr.create("lowerAngLimit", "lalt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_lowerAngLimit));
    
    
    ia_upperAngLimit = fnNumericAttr.create("upperAngLimit", "ualt", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_upperAngLimit));
    
    
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
    
    
    
    ia_rotationInA = fnNumericAttr.createPoint("rotationInA", "hgRotA", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = fnNumericAttr.setDefault((double) 0.0, (double) 0.0, (double) 0.0);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_rotationInA));
    
    
    ia_rotationInB = fnNumericAttr.createPoint("rotationInB", "hgRotB", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = fnNumericAttr.setDefault((double) 0.0, (double) 0.0, (double) 0.0);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_rotationInB));
    
    
    ia_pivotInA = fnNumericAttr.createPoint("pivotInA", "pivinA", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = fnNumericAttr.setDefault((double) 0.0, (double) 0.0, (double) 0.0);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_pivotInA));
    
    
    ia_pivotInB = fnNumericAttr.createPoint("pivotInB", "pivinB", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = fnNumericAttr.setDefault((double) 0.0, (double) 0.0, (double) 0.0);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_pivotInB));
    
    
    
    stat = attributeAffects(ia_rigidBodyA, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rigidBodyA, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rigidBodyB, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rigidBodyB, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_damping, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_breakThreshold, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_disableCollide, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_disableCollide, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_lowerLinLimit, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_upperLinLimit, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_lowerAngLimit, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_upperAngLimit, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_rotationInA, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_rotationInA, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_rotationInB, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_rotationInB, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    stat = attributeAffects(ia_pivotInA, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_pivotInA, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_pivotInB, ca_constraint);
    DOMMY_RETURN_IF_FAILURE(stat);
    stat = attributeAffects(ia_pivotInB, ca_constraintParam);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSliderConstraintNode", "initialize", std::current_exception());
}

DBSliderConstraintNode::DBSliderConstraintNode()
    : m_PivInA(0.0F, 0.0F, 0.0F)
    , m_PivInB(0.0F, 0.0F, 0.0F)
    , m_RotInA(1.0F, 0.0F, 0.0F, 0.0F)
    , m_RotInB(1.0F, 0.0F, 0.0F, 0.0F)
{
}

DBSliderConstraintNode::~DBSliderConstraintNode()
{
    destroyConstraint();
}

void DBSliderConstraintNode::nodeRemoved(MObject& node, void*)
{
    MFnDependencyNode fnNode(node);
    DBSliderConstraintNode *pNode =
        dommy::maya::checkedUserNode<DBSliderConstraintNode>(
            fnNode, DBSliderConstraintNode::typeId);
    if (!pNode) return;
    pNode->destroyConstraint();
}




bool DBSliderConstraintNode::setInternalValueInContext(
    const MPlug&, const MDataHandle&, MDGContext&)
{
    return false;
}

MStatus DBSliderConstraintNode::compute(const MPlug& plug, MDataBlock& data) try
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
DOMMY_COMPUTE_CATCH("DBSliderConstraintNode")

void DBSliderConstraintNode::draw( M3dView & view, const MDagPath&,
                               M3dView::DisplayStyle style,
                               M3dView::DisplayStatus stat ) try
{
    dbSolver_t::LockedSolverAccess solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return;
    vec3f zeroVec(0.f,0.f,0.f);
    vec3f minusXVec(-0.2f,0.f,0.f);
    vec3f posXVec(0.2f,0.f,0.f);
    vec3f minusYVec (0.f,-0.2f,0.f);
    vec3f posYVec (0.f,0.2f,0.f);
    vec3f minusZVec (0.f,0.f,-0.2f);
    vec3f posZVec (0.f,0.f,0.2f);
    
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
    vec3f pivB(0.0f, 0.0f, 0.0f);
    dbRigidBody_t::pointer rigid_bodyB = nullptr;
    if (m_constraint)
    {
        vec3f pos;
        quatf rot;
        m_constraint->rigid_bodyA()->get_transform(pos, rot);
        m_constraint->worldToA(pos, posA);
        rigid_bodyB = m_constraint->rigid_bodyB();
        if(rigid_bodyB)
        {
            rigid_bodyB->get_transform(pos, rot);
            m_constraint->worldToA(pos, posB);
        }
        m_constraint->worldFromB(zeroVec, pos);
        m_constraint->worldToA(pos, pivB);
    }
    
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(posA[0], posA[1], posA[2]);
    
    //glVertex3f(0.0, 0.0, 0.0);
    //glVertex3f(pivB[0], pivB[1], pivB[2]);
    
    if(rigid_bodyB)
    {
        glVertex3f(pivB[0], pivB[1], pivB[2]);
        glVertex3f(posB[0], posB[1], posB[2]);
    }
    
    
    glVertex3f(-0.2, 0.0, 0.0);
    glVertex3f(0.2, 0.0, 0.0);
    
    glVertex3f(0.0, -0.2, 0.0);
    glVertex3f(0.0, 0.2, 0.0);
    
    glVertex3f(0.0, 0.0, -0.2);
    glVertex3f(0.0, 0.0, 0.2);
    
    vec3f posT, posP, posM;
    
    if (m_constraint)
    {
        m_constraint->worldFromB(minusXVec, posT);
        m_constraint->worldToA(posT, posM);
        m_constraint->worldFromB(posXVec, posT);
        m_constraint->worldToA(posT, posP);
        glVertex3f(posM[0], posM[1], posM[2]);
        glVertex3f(posP[0], posP[1], posP[2]);
        m_constraint->worldFromB(minusYVec, posT);
        m_constraint->worldToA(posT, posM);
        m_constraint->worldFromB(posYVec, posT);
        m_constraint->worldToA(posT, posP);
        glVertex3f(posM[0], posM[1], posM[2]);
        glVertex3f(posP[0], posP[1], posP[2]);
        m_constraint->worldFromB(minusZVec, posT);
        m_constraint->worldToA(posT, posM);
        m_constraint->worldFromB(posZVec, posT);
        m_constraint->worldToA(posT, posP);
        glVertex3f(posM[0], posM[1], posM[2]);
        glVertex3f(posP[0], posP[1], posP[2]);
    }
    
    glEnd();
    
}
DOMMY_DRAW_CATCH("DBSliderConstraintNode")

bool DBSliderConstraintNode::isBounded() const
{
    return false;
}

MBoundingBox DBSliderConstraintNode::boundingBox() const
{
    MPoint corner1(-1, -1, -1);
    MPoint corner2(1, 1, 1);
    return MBoundingBox(corner1, corner2);
}

void DBSliderConstraintNode::destroyConstraint()
{
    dbConstraint_t::pointer constraint = static_cast<dbConstraint_t::pointer>(m_constraint);
    dbSolver_t::remove_constraint(constraint);
    m_constraint.reset();
}

//standard attributes
MStatus DBSliderConstraintNode::computeConstraint(
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
    quatf rotationA;
    status = dommy::bullet::constraint_node::finiteRotation(
        data, ia_rotationInA, rotationA);
    if (!status) {
        return status;
    }
    quatf rotationB;
    status = dommy::bullet::constraint_node::finiteRotation(
        data, ia_rotationInB, rotationB);
    if (!status) {
        return status;
    }
    bool disableCollisions = false;
    status = dommy::bullet::constraint_node::boolean(
        data, ia_disableCollide, disableCollisions);
    if (!status) {
        return status;
    }

    dbSlider_constraint_t::pointer replacement =
        rigidBodyB
            ? dbSolver_t::create_slider_constraint(
                  rigidBodyA, pivotA, rotationA,
                  rigidBodyB, pivotB, rotationB)
            : dbSolver_t::create_slider_constraint(
                  rigidBodyA, pivotA, rotationA);
    if (!replacement) {
        return MS::kFailure;
    }
    vec3f localPivotA;
    vec3f localPivotB;
    quatf localRotationA;
    quatf localRotationB;
    replacement->get_local_frameA(localPivotA, localRotationA);
    replacement->get_local_frameB(localPivotB, localRotationB);
    if (!dommy::bullet::constraint_node::finiteFrame(
            localPivotA, localRotationA) ||
        !dommy::bullet::constraint_node::finiteFrame(
            localPivotB, localRotationB)) {
        return MS::kInvalidParameter;
    }
    status = applyConstraintParameters(data, replacement);
    if (!status) return status;
    MDataHandle output = data.outputValue(ca_constraint, &status);
    if (!status) return status;
    status = dommy::bullet::constraint_node::commitConstraint(
        m_constraint, replacement, disableCollisions);
    if (!status) {
        return status;
    }
    m_PivInA = localPivotA;
    m_PivInB = localPivotB;
    m_RotInA = localRotationA;
    m_RotInB = localRotationB;
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}



MStatus DBSliderConstraintNode::computeWorldMatrix(
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

        MQuaternion rotation;
        status = parentTransform.getRotation(
            rotation, MSpace::kTransform);
        quatf checkedWorldRotation;
        if (!status || !dommy::bullet::constraint_node::checkedRotation(
                rotation, checkedWorldRotation)) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        auto solverAccess = dbSolver_t::lock_solver();
        if (!solverAccess) return MS::kFailure;
        bool updatePivot = m_constraint->getPivotChanged();
        if (!updatePivot) {
            vec3f worldP;
            quatf worldR;
            m_constraint->get_world(worldP, worldR);
            if (!dommy::bullet::constraint_node::finiteFrame(
                    worldP, worldR)) {
                return MS::kInvalidParameter;
            }
            const double deltas[] = {
                double(worldP[0]) - translatedPosition[0],
                double(worldP[1]) - translatedPosition[1],
                double(worldP[2]) - translatedPosition[2],
                checkedWorldRotation[1] - worldR[1],
                checkedWorldRotation[2] - worldR[2],
                checkedWorldRotation[3] - worldR[3],
                checkedWorldRotation[0] - worldR[0]};
            double deltaSquared = 0.0;
            for (std::size_t index = 0U;
                 index < sizeof(deltas) / sizeof(deltas[0]); ++index) {
                deltaSquared += deltas[index] * deltas[index];
            }
            updatePivot = deltaSquared > FLT_EPSILON;
        }
        if (updatePivot) {
            const vec3f worldP(
                translatedPosition[0],
                translatedPosition[1],
                translatedPosition[2]);
            const quatf worldR(
                checkedWorldRotation[0],
                checkedWorldRotation[1],
                checkedWorldRotation[2],
                checkedWorldRotation[3]);
            if (!m_constraint->set_world(worldP, worldR)) return MS::kInvalidParameter;

            vec3f pivotA;
            vec3f pivotB;
            quatf rotationA;
            quatf rotationB;
            m_constraint->get_frameA(pivotA, rotationA);
            m_constraint->get_frameB(pivotB, rotationB);
            status = dommy::bullet::constraint_node::writeFrame(
                data, ia_pivotInA, ia_rotationInA, pivotA, rotationA);
            if (!status) {
                return status;
            }
            status = dommy::bullet::constraint_node::writeFrame(
                data, ia_pivotInB, ia_rotationInB, pivotB, rotationB);
            if (!status) {
                return status;
            }
            m_constraint->setPivotChanged(false);
            m_constraint->get_local_frameA(m_PivInA, m_RotInA);
            m_constraint->get_local_frameB(m_PivInB, m_RotInB);
            if (!dommy::bullet::constraint_node::finiteFrame(
                    m_PivInA, m_RotInA) ||
                !dommy::bullet::constraint_node::finiteFrame(
                    m_PivInB, m_RotInB)) {
                return MS::kInvalidParameter;
            }
        }
    } else {
        vec3f worldP;
        quatf worldR;
        m_constraint->get_world(worldP, worldR);
        if (!dommy::bullet::constraint_node::finiteFrame(
                worldP, worldR)) {
            return MS::kInvalidParameter;
        }
        status = parentTransform.setTranslation(
            MVector(worldP[0], worldP[1], worldP[2]),
            MSpace::kTransform);
        if (!status) {
            return status;
        }
        status = parentTransform.setRotation(
            MQuaternion(worldR[1], worldR[2], worldR[3], worldR[0]));
        if (!status) {
            return status;
        }
    }
    return data.setClean(plug);
}

MStatus DBSliderConstraintNode::computeConstraintParam(
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

MStatus DBSliderConstraintNode::applyConstraintParameters(
    MDataBlock& data, const dbSlider_constraint_t::pointer& constraint)
{
    if (!constraint) return MS::kFailure;
    MStatus status;

    float damping = 0.0F;
    float breakThreshold = 0.0F;
    float lowerLinear = 0.0F;
    float upperLinear = 0.0F;
    float lowerAngular = 0.0F;
    float upperAngular = 0.0F;
    const MObject attributes[] = {
        ia_damping, ia_breakThreshold,
        ia_lowerLinLimit, ia_upperLinLimit,
        ia_lowerAngLimit, ia_upperAngLimit};
    float* const values[] = {
        &damping, &breakThreshold,
        &lowerLinear, &upperLinear,
        &lowerAngular, &upperAngular};
    for (std::size_t index = 0U;
         index < sizeof(attributes) / sizeof(attributes[0]); ++index) {
        status = dommy::bullet::constraint_node::finiteFloat(
            data, attributes[index], *values[index]);
        if (!status) {
            return status;
        }
    }
    if (damping < 0.0F || damping > 1.0F ||
        breakThreshold < 0.0F) {
        return MS::kInvalidParameter;
    }

    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    constraint->set_damping(damping);
    constraint->set_breakThreshold(breakThreshold);
    constraint->set_LinLimit(lowerLinear, upperLinear);
    constraint->set_AngLimit(
        deg2rad(lowerAngular), deg2rad(upperAngular));
    return MS::kSuccess;
}

MStatus DBSliderConstraintNode::evaluate_constraint(
    dbSlider_constraint_t::pointer& constraint)
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

MStatus DBSliderConstraintNode::update()
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
