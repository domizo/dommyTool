//
//  commonDBulletConstraint.h
//  dommyTool
//
//  Created by yuzo on 2017/10/21.
//


#pragma once



#include "DMayaHeaders.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include "DBulletDataHeaders.h"

#include "BTP2PConstraint.h"
#include "BTFixConstraint.h"
#include "BTConeTwistConstraint.h"
#include "BT6DofConstraint.h"

#include "DPointDataHeaders.h"

#include "DMbtRigid.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DStatusOperation.h"
#include "Constraint/DConstraintFrame.h"
#include "Collision/DShapeFrameMath.h"

#include <cmath>
#include <utility>






// Failed connected reads are errors; disconnected/valid empty point and body
// inputs remain successful empty results for the caller's documented fallback.
inline MStatus getConstraintInfo(const MPlug& constPlug, DbtConstraintParams& constParam)
{
    MStatus status;
    MPlugArray connections;
    constPlug.connectedTo(connections, true, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (connections.length() != 1) return MS::kInvalidParameter;
    MObject object = connections[0].node(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MFnDependencyNode node(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTypeId type = node.typeId(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    DbtConstraintParams candidate;
    if (type == BTP2PConstraint::typeId) {
        auto* value = dommy::maya::checkedUserNode<BTP2PConstraint>(node, type);
        if (!value) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(value->evalParms(candidate));
    } else if (type == BTFixConstraint::typeId) {
        auto* value = dommy::maya::checkedUserNode<BTFixConstraint>(node, type);
        if (!value) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(value->evalParms(candidate));
    } else if (type == BTConeTwistConstraint::typeId) {
        auto* value = dommy::maya::checkedUserNode<BTConeTwistConstraint>(node, type);
        if (!value) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(value->evalParms(candidate));
    } else if (type == BT6DofConstraint::typeId) {
        auto* value = dommy::maya::checkedUserNode<BT6DofConstraint>(node, type);
        if (!value) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(value->evalParms(candidate));
    } else return MS::kInvalidParameter;
    if (!ValidateBulletConstraintParameters(candidate) ||
        candidate.m_const_type <= DBT_CONST_INVALID ||
        candidate.m_const_type >= DBT_CONST_TYPE_COUNT) return MS::kInvalidParameter;
    constParam = candidate;
    return MS::kSuccess;
}

inline MStatus getDPointPositionToBTVec(const MPlug& dpPlug,
                                      std::vector<btVector3>& positions)
{
    MStatus status;
    const bool connected = dpPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::vector<btVector3> candidate;
    if (connected) {
        dommy::maya::ScopedPlugDataHandle owner;
        DOMMY_RETURN_IF_FAILURE(owner.read(dpPlug));
        const DPData* data = getInputDPfromHandle(owner.handle());
        if (!data) return MS::kInvalidParameter;
        candidate.reserve(data->numberOfPoints());
        for (auto point = data->beginPt(); point != data->endPt(); ++point) {
            const btVector3 value((*point)[0], (*point)[1], (*point)[2]);
            for (int axis = 0; axis < 3; ++axis)
                if (!dommy::bullet::isSafeCollisionCoordinate(value[axis])) return MS::kInvalidParameter;
            candidate.push_back(value);
        }
    }
    positions.swap(candidate);
    return MS::kSuccess;
}

inline MStatus getRBPositionToBTVecList(const MPlug& input,
    std::vector<std::vector<btVector3>>& positions,
    std::vector<std::vector<btRigidBody*>>& bodies, btDynamicsWorld& world)
{
    MStatus status;
    const unsigned count = input.numConnectedElements(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::vector<std::vector<btVector3>> candidatePositions;
    std::vector<std::vector<btRigidBody*>> candidateBodies;
    for (unsigned i = 0; i < count; ++i) {
        MPlug element = input.connectionByPhysicalIndex(i, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        MPlugArray connections;
        element.connectedTo(connections, true, false, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        for (unsigned j = 0; j < connections.length(); ++j) {
            MObject object = connections[j].node(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            MFnDependencyNode node(object, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const MTypeId type = node.typeId(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (type != DMbtRigid::typeId) return MS::kInvalidParameter;
            auto* value = dommy::maya::checkedUserNode<DMbtRigid>(node, type);
            if (!value || !value->getRigidBodyData()) return MS::kFailure;
            std::vector<btVector3> points;
            std::vector<btRigidBody*> rigidBodies;
            for (auto* body : *value->getRigidBodyData()) {
                if (!body || world.getCollisionObjectArray().findLinearSearch(body) ==
                    world.getNumCollisionObjects()) return MS::kFailure;
                if (!dommy::bullet::finiteConstraintTransform(body->getWorldTransform()))
                    return MS::kInvalidParameter;
                points.push_back(body->getWorldTransform().getOrigin());
                rigidBodies.push_back(body);
            }
            candidatePositions.push_back(std::move(points));
            candidateBodies.push_back(std::move(rigidBodies));
        }
    }
    positions.swap(candidatePositions);
    bodies.swap(candidateBodies);
    return MS::kSuccess;
}

inline MStatus getRBPositionToBTVec(const MPlug& input,
    std::vector<btVector3>& positions, std::vector<btRigidBody*>& bodies,
    btDynamicsWorld& world)
{
    std::vector<std::vector<btVector3>> groups;
    std::vector<std::vector<btRigidBody*>> bodyGroups;
    DOMMY_RETURN_IF_FAILURE(getRBPositionToBTVecList(input, groups, bodyGroups, world));
    std::vector<btVector3> candidatePositions;
    std::vector<btRigidBody*> candidateBodies;
    for (size_t i = 0; i < groups.size(); ++i) {
        candidatePositions.insert(candidatePositions.end(), groups[i].begin(), groups[i].end());
        candidateBodies.insert(candidateBodies.end(), bodyGroups[i].begin(), bodyGroups[i].end());
    }
    positions.swap(candidatePositions);
    bodies.swap(candidateBodies);
    return MS::kSuccess;
}

inline db_constraint_base* makeNewConstraint( btRigidBody &rbA, btRigidBody &rbB, btTransform &transA, btTransform &transB, DbtConstraintParams &constParam) {
    
    
//    MPlugArray cConnections;
//    plg.connectedTo(cConnections, true, true);
////    if(DB_POINT_CONSTRAINT)cout << "cConnections:" << cConnections.length() << endl;
//
//    if(cConnections.length()) {
    
        //Create constraint
        switch (constParam.m_const_type) {
            case DBT_CONST_P2P:
            {
                db_p2p_constraint *constObj = new db_p2p_constraint(rbA, rbB, transA.getOrigin(), transB.getOrigin());
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_SPRING:
            case DBT_CONST_FIX:
            {
                db_fix_constraint *constObj = new db_fix_constraint(rbA, rbB, transA, transB);
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_CONE_TWIST:
            {
                db_cone_constraint *constObj = new db_cone_constraint(rbA, rbB, transA, transB);
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_6DOF:
            {
                db_sixdof_constraint *constObj = new db_sixdof_constraint(rbA, rbB, transA, transB, false);
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_6DOF_SPRING:
            {
                db_sixdof_spring_constraint *constObj = new db_sixdof_spring_constraint(rbA, rbB, transA, transB, false);
                constObj->set_param(constParam);
                if(constParam.m_use_spring) {
                    constObj->setEquilibriumPoint();
                }
                
                return constObj;
            }
                break;
            case DBT_CONST_HINGE:
            {
                db_hinge_constraint *constObj = new db_hinge_constraint(rbA, rbB, transA, transB, false);
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_SLIDER:
            {
                db_slider_constraint *constObj = new db_slider_constraint(rbA, rbB, transA, transB, false);
                constObj->set_param(constParam);
                return constObj;
            }
                break;
            case DBT_CONST_INVALID:
            default:
                break;
        }
        
        
//    }
    
    return nullptr;
}
