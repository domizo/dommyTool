//
//  DBLinkConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/30.
//

#include "DBLinkConstraint.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DDataOp.h"

#include "DMbtRigid.h"
#include "DMbtFracture.h"

#include "BTP2PConstraint.h"
#include "BTFixConstraint.h"
#include "BTConeTwistConstraint.h"
#include "BT6DofConstraint.h"


#include "commonDBullet.h"
#include "commonBulletRigidBody.h"
#include "commonDBulletConstraint.h"
#include <cmath>



#define DB_LINK_CONSTRAINT DOMMY_ENABLE_DEBUG_LOGS


const MTypeId DBLinkConstraint::typeId( ID_DB_LINK_CONSTRAINT );

MObject DBLinkConstraint::inRB;

MObject DBLinkConstraint::aTargetRB;

MObject DBLinkConstraint::inConst;
MObject DBLinkConstraint::inConnectInfoDC;

MObject DBLinkConstraint::aIsEnable;
MObject DBLinkConstraint::aIsUpdate;

MObject DBLinkConstraint::outConst;


DBLinkConstraint::~DBLinkConstraint()
{
    detachFromWorld();
}


void DBLinkConstraint::detachFromWorld() noexcept
{
    DetachAndReleaseBulletConstraints(
        m_worldReference.lock(), constraintLists);
    m_worldReference.reset();
}


void *DBLinkConstraint::creator() {
    return ::dommy::maya::createObject("DBLinkConstraint", [&]() {
        return new DBLinkConstraint();
    });
}



#pragma mark -
#pragma mark get param


MStatus DBLinkConstraint::evalParms(DBLinkConstParams& parms) {
    MStatus status;
    parms.m_is_enable = MPlug(thisMObject(), aIsEnable).asBool(&status);
    if (status != MS::kSuccess) return status;
    parms.m_is_update = MPlug(thisMObject(), aIsUpdate).asBool(&status);
    if (status != MS::kSuccess) return status;
    return MS::kSuccess;
}


#pragma mark -
#pragma mark init
MStatus DBLinkConstraint::createConstraint(
    btSoftRigidDynamicsWorld* world,
    const DBulletWorldReferencePtr& worldReference)
{
    if (!world || !worldReference || worldReference->get() != world)
        return MS::kInvalidParameter;
    DBLinkConstParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    DbtConstraintParams constParam;
    DOMMY_RETURN_IF_FAILURE(getConstraintInfo(MPlug(thisMObject(), inConst), constParam));

    dommy::maya::ScopedPlugDataHandle curveOwner;
    DOMMY_RETURN_IF_FAILURE(curveOwner.read(MPlug(thisMObject(), inConnectInfoDC)));
    const DCurveData* curves = getInputDCfromHandle(curveOwner.handle());
    if (!curves) return MS::kInvalidParameter;

    std::vector<std::vector<btVector3>> sourcePositions, targetPositions;
    std::vector<std::vector<btRigidBody*>> sourceBodies, targetBodies;
    DOMMY_RETURN_IF_FAILURE(getRBPositionToBTVecList(
        MPlug(thisMObject(), inRB), sourcePositions, sourceBodies, *world));
    DOMMY_RETURN_IF_FAILURE(getRBPositionToBTVecList(
        MPlug(thisMObject(), aTargetRB), targetPositions, targetBodies, *world));

    std::vector<DBulletConstraintPtr> constraints;
    if (!sourceBodies.empty()) {
        // Preserve the legacy first-group mapping of prim_index.
        const auto& sources = sourceBodies.front();
        const auto& targets = targetBodies.empty() ? sources : targetBodies.front();
        for (auto curve = curves->beginPrims(); curve != curves->endPrims(); ++curve) {
            if (!*curve) return MS::kInvalidParameter;
            if (!(*curve)->isExistAttr("prim_index")) continue;
            MIntArray ids;
            getDCPAttributeMIntArrayFromScalar(*curve, "prim_index", ids);
            if (ids.length() != 2 || ids[0] < 0 || ids[1] < 0) continue;
            const size_t first = static_cast<size_t>(ids[0]);
            const size_t second = static_cast<size_t>(ids[1]);
            if (first >= sources.size() || second >= targets.size()) continue;
            btRigidBody* source = sources[first];
            btRigidBody* target = targets[second];
            if (source == target) continue;
            const DPointList* points = (*curve)->getPositionPtrConst();
            if (!points || points->size() != 2) continue;
            vec3f midpoint;
            for (int axis = 0; axis < 3; ++axis)
                midpoint[axis] = static_cast<float>(
                    0.5 * static_cast<double>((*points)[0][axis]) +
                    0.5 * static_cast<double>((*points)[1][axis]));
            // Historical world frame: 90 degrees around Z.
            const float halfAngle = static_cast<float>(SIMD_PI) * 0.25f;
            const quatf rotation(std::cos(halfAngle), 0, 0, std::sin(halfAngle));
            btTransform frameA, frameB;
            if (!dommy::bullet::prepareConstraintFrames(
                    midpoint, rotation, source->getWorldTransform(),
                    target->getWorldTransform(), frameA, frameB))
                return MS::kInvalidParameter;
            DBulletConstraintPtr constraint(makeNewConstraint(
                *source, *target, frameA, frameB, constParam));
            if (!constraint || !constraint->constraint()) return MS::kFailure;
            constraint->constraint()->setEnabled(param.m_is_enable);
            constraints.push_back(std::move(constraint));
        }
    }
    detachFromWorld();
    m_worldReference = worldReference;
    for (auto& constraint : constraints)
        AddBulletConstraint(worldReference, constraintLists, std::move(constraint),
                            constParam.m_disable_colide);
    return MS::kSuccess;
}
    
#pragma mark -
#pragma mark update

MStatus DBLinkConstraint::updateConstraint(btSoftRigidDynamicsWorld* world, float time)
{
    if (!world || ResolveBulletWorld(m_worldReference) != world ||
        !std::isfinite(time) || time < 0.0f || time > 1.0f)
        return MS::kInvalidParameter;
    DBLinkConstParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    if (param.m_is_enable && param.m_is_update)
        return createConstraint(world, m_worldReference.lock());
    for (auto* constraint : constraintLists)
        if (!constraint || !constraint->constraint()) return MS::kFailure;
    for (auto* constraint : constraintLists)
        constraint->constraint()->setEnabled(param.m_is_enable);
    return MS::kSuccess;
}

#pragma mark -
#pragma mark compute

MStatus DBLinkConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBLinkConstraint")


#pragma mark -
#pragma mark initialize

MStatus DBLinkConstraint::initialize() try {
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
    
    aTargetRB = fnMsgAttr.create("TargetRB", "tarrb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetRB));
    
    
    inConst = fnMsgAttr.create("inConst", "incnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setConnectable( true ));
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(inConst));
    
    
    inConnectInfoDC = tAttr.create("inConnectInfoDC", "icifdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inConnectInfoDC));
    
    
    
    
    aIsEnable = nAttr.create("IsEnable", "enble", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEnable));
    
    aIsUpdate = nAttr.create("IsUpdate", "updat", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsUpdate));
    
    
    //create custom data
    outConst = fnMsgAttr.create("outConst", "ocnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outConst));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inRB, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inConst, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetRB, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inConnectInfoDC, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEnable, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsUpdate, outConst ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBLinkConstraint", "initialize", std::current_exception());
}
