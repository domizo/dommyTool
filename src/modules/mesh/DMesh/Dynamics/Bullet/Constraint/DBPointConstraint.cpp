//
//  DBPointConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/24.
//

#include "DBPointConstraint.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DDataOp.h"
#include "DNodeOperation.h"

#include "DMbtRigid.h"
#include "DMbtFracture.h"

#include "BTP2PConstraint.h"
#include "BTFixConstraint.h"
#include "BTConeTwistConstraint.h"
#include "BT6DofConstraint.h"


#include "commonDBullet.h"
#include "commonDBulletConstraint.h"
#include "commonBulletRigidBody.h"

#include <cmath>

#define DB_POINT_CONSTRAINT DOMMY_ENABLE_DEBUG_LOGS



const MTypeId DBPointConstraint::typeId( ID_DB_POINT_CONSTRAINT );

MObject DBPointConstraint::inRB;
MObject DBPointConstraint::inConst;

MObject DBPointConstraint::aIsEnable;
MObject DBPointConstraint::aIsUpdate;

MObject DBPointConstraint::aTargetType;
MObject DBPointConstraint::aPosition;
MObject DBPointConstraint::aTargetRB;
MObject DBPointConstraint::aTargetDP;


MObject DBPointConstraint::aDebugDisplaySize;

MObject DBPointConstraint::outConst;


DBPointConstraint::~DBPointConstraint()
{
    detachFromWorld();
}


void DBPointConstraint::detachFromWorld() noexcept
{
    const auto world = m_worldReference.lock();
    DetachAndReleaseBulletConstraints(world, constraintLists);
    DetachAndReleaseBulletRigidBodies(world, rbLists);
    ReleaseBulletCollisionShapes(colShapes);
    m_worldReference.reset();
}




void *DBPointConstraint::creator() {
    return ::dommy::maya::createObject("DBPointConstraint", [&]() {
        return new DBPointConstraint();
    });
}





#pragma mark -
#pragma mark get param


MStatus DBPointConstraint::evalParms(DBPointConstParams& parms) {
    MStatus status;
    parms.m_is_enable = MPlug(thisMObject(), aIsEnable).asBool(&status);
    if (status != MS::kSuccess) return status;

    parms.m_is_update = MPlug(thisMObject(), aIsUpdate).asBool(&status);
    if (status != MS::kSuccess) return status;

    const short targetType = MPlug(thisMObject(), aTargetType).asShort(&status);
    if (status != MS::kSuccess) return status;
    if (targetType != DB_CONST_TAR_POINT && targetType != DB_CONST_TAR_RB &&
        targetType != DB_CONST_TAR_DP) {
        return MS::kInvalidParameter;
    }
    parms.m_target_type = targetType;

    const MPlug positionPlug(thisMObject(), aPosition);
    if (positionPlug.numChildren() != 3U) return MS::kInvalidParameter;
    const double x = positionPlug.child(0U).asDouble(&status);
    if (status != MS::kSuccess) return status;
    const double y = positionPlug.child(1U).asDouble(&status);
    if (status != MS::kSuccess) return status;
    const double z = positionPlug.child(2U).asDouble(&status);
    if (status != MS::kSuccess) return status;
    parms.m_position = MVector(x, y, z);
    if (!dommy::maya::isFloatRepresentableVector(parms.m_position)) {
        return MS::kInvalidParameter;
    }
    for (int axis = 0; axis < 3; ++axis)
        if (!dommy::bullet::isSafeCollisionCoordinate(parms.m_position[axis]))
            return MS::kInvalidParameter;

    parms.m_debug_display_size =
        MPlug(thisMObject(), aDebugDisplaySize).asDouble(&status);
    if (status != MS::kSuccess) return status;
    if (!std::isfinite(parms.m_debug_display_size) ||
        parms.m_debug_display_size < 0.0) {
        return MS::kInvalidParameter;
    }

    return MS::kSuccess;
}








#pragma mark -
#pragma mark init
MStatus DBPointConstraint::createConstraint(
    btSoftRigidDynamicsWorld* world,
    const DBulletWorldReferencePtr& worldReference)
{
    if (!world || !worldReference || worldReference->get() != world)
        return MS::kInvalidParameter;

    DBPointConstParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    DbtConstraintParams constParam;
    DOMMY_RETURN_IF_FAILURE(getConstraintInfo(MPlug(thisMObject(), inConst), constParam));

    std::vector<btVector3> targets;
    std::vector<btRigidBody*> targetBodies;
    if (param.m_target_type == DB_CONST_TAR_DP) {
        DOMMY_RETURN_IF_FAILURE(getDPointPositionToBTVec(
            MPlug(thisMObject(), aTargetDP), targets));
    } else if (param.m_target_type == DB_CONST_TAR_RB) {
        DOMMY_RETURN_IF_FAILURE(getRBPositionToBTVec(
            MPlug(thisMObject(), aTargetRB), targets, targetBodies, *world));
    }
    if (targets.empty())
        targets.emplace_back(param.m_position[0], param.m_position[1], param.m_position[2]);

    std::vector<std::vector<btVector3>> sourcePositions;
    std::vector<std::vector<btRigidBody*>> sourceBodies;
    DOMMY_RETURN_IF_FAILURE(getRBPositionToBTVecList(
        MPlug(thisMObject(), inRB), sourcePositions, sourceBodies, *world));

    // Reverse destruction order keeps bodies and shapes alive until every
    // staged constraint has been destroyed, including exception paths.
    std::vector<std::unique_ptr<db_collision_shape_base>> shapes;
    std::vector<DBulletRigidBodyPtr> bodies;
    std::vector<DBulletConstraintPtr> constraints;
    for (const auto& group : sourceBodies) {
        size_t targetIndex = 0;
        for (btRigidBody* source : group) {
            btRigidBody* targetBody = nullptr;
            if (!targetBodies.empty()) {
                targetBody = targetBodies[targetIndex];
            } else {
                if (targetIndex == bodies.size()) {
                    std::unique_ptr<db_collision_shape_base> shape(
                        new db_collision_box(vec3f(0.01f, 0.01f, 0.01f)));
                    DBulletRigidBodyPtr body =
                        MakeSimpleKinematicRigidBody(shape->shape(), targets[targetIndex]);
                    shapes.push_back(std::move(shape));
                    bodies.push_back(std::move(body));
                }
                targetBody = bodies[targetIndex].get();
            }
            if (!source || !targetBody || source == targetBody) return MS::kInvalidParameter;
            const btVector3& point = targets[targetIndex];
            btTransform frameA, frameB;
            if (!dommy::bullet::prepareConstraintFrames(
                    vec3f(point[0], point[1], point[2]), quatf(1, 0, 0, 0),
                    targetBody->getWorldTransform(), source->getWorldTransform(),
                    frameA, frameB)) return MS::kInvalidParameter;
            DBulletConstraintPtr constraint(makeNewConstraint(
                *targetBody, *source, frameA, frameB, constParam));
            if (!constraint || !constraint->constraint()) return MS::kFailure;
            constraint->constraint()->setEnabled(param.m_is_enable);
            constraints.push_back(std::move(constraint));
            targetIndex = (targetIndex + 1) % targets.size();
        }
    }

    // Do not discard the previous constraints on an input/preparation failure.
    detachFromWorld();
    m_worldReference = worldReference;
    for (size_t i = 0; i < bodies.size(); ++i)
        AddBulletRigidBodyWithShape(*world, rbLists, colShapes,
                                   std::move(bodies[i]), std::move(shapes[i]));
    for (auto& constraint : constraints)
        AddBulletConstraint(worldReference, constraintLists, std::move(constraint),
                            constParam.m_disable_colide);
    return MS::kSuccess;
}




MStatus DBPointConstraint::updateConstraint(btSoftRigidDynamicsWorld* world, float time)
{
    if (!world || ResolveBulletWorld(m_worldReference) != world)
        return MS::kInvalidParameter;
    if (!std::isfinite(time) || time < 0.0f || time > 1.0f)
        return MS::kInvalidParameter;
    DBPointConstParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    for (auto* constraint : constraintLists)
        if (!constraint || !constraint->constraint()) return MS::kFailure;

    std::vector<vec3f> positions;
    std::vector<quatf> rotations;
    if (param.m_is_enable && param.m_is_update) {
        std::vector<btVector3> targets;
        if (param.m_target_type == DB_CONST_TAR_DP)
            DOMMY_RETURN_IF_FAILURE(getDPointPositionToBTVec(
                MPlug(thisMObject(), aTargetDP), targets));
        if (targets.empty())
            targets.emplace_back(param.m_position[0], param.m_position[1], param.m_position[2]);
        positions.reserve(rbLists.size());
        rotations.reserve(rbLists.size());
        for (size_t i = 0; i < rbLists.size(); ++i) {
            btRigidBody* body = rbLists[i];
            if (!body || !body->getMotionState()) return MS::kFailure;
            vec3f start;
            quatf rotation;
            getDBtTransformTR(body, start, rotation);
            const btVector3& target = targets[i % targets.size()];
            vec3f position;
            quatf resultRotation;
            if (!dommy::bullet::interpolatePassiveTransform(
                    start, vec3f(target[0], target[1], target[2]),
                    rotation, quatf(1, 0, 0, 0), time, position, resultRotation))
                return MS::kInvalidParameter;
            positions.push_back(position);
            rotations.push_back(resultRotation);
        }
    }
    for (auto* constraint : constraintLists)
        constraint->constraint()->setEnabled(param.m_is_enable);
    for (size_t i = 0; i < positions.size(); ++i)
        set_DB_RigidTransform(rbLists[i], positions[i], rotations[i]);
    return MS::kSuccess;
}


#pragma mark -
#pragma mark compute

MStatus DBPointConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBPointConstraint")


MStatus DBPointConstraint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    // ----------------------- INPUTS --------------------------
    MFnMessageAttribute fnMsgAttr;
    inRB = fnMsgAttr.create("inRB", "inrb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(inRB));
    
    
    inConst = fnMsgAttr.create("inConst", "incnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setConnectable( true ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(inConst));
    
    
    aIsEnable = nAttr.create("IsEnable", "enble", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEnable));
    
    aIsUpdate = nAttr.create("IsUpdate", "updat", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsUpdate));
    
    
    
    
    
    MFnEnumAttribute posTypeEnumFn;
    aTargetType = posTypeEnumFn.create( "TargetType", "tarty", DB_CONST_TAR_POINT, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(posTypeEnumFn.addField( "Position", DB_CONST_TAR_POINT));
    DOMMY_RETURN_IF_FAILURE(posTypeEnumFn.addField( "Target RigidBody", DB_CONST_TAR_RB ));
    DOMMY_RETURN_IF_FAILURE(posTypeEnumFn.addField( "Target DPoint", DB_CONST_TAR_DP ));
    
    DOMMY_RETURN_IF_FAILURE(posTypeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(posTypeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetType));
    
    
    aPosition = nAttr.create("Position", "pos", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPosition));
    
    
    
    aTargetRB = fnMsgAttr.create("TargetRB", "tarrb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetRB));
    
    
    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));
    
    
    //debug
    aDebugDisplaySize = nAttr.create("DebugDisplaySize", "dbgdspsz", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDebugDisplaySize));
    
    
    
    //create custom data
    outConst = fnMsgAttr.create("outConst", "ocnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outConst));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inRB, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inConst, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEnable, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsUpdate, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetType, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPosition, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetRB, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDP, outConst ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBPointConstraint", "initialize", std::current_exception());
}
