//
//  DMbtFracture.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/21.
//

#include "DMbtFracture.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "DBulletGeometryPreparation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DVectorLookup.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "Constraint/DConstraintFrame.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DDataOp.h"
#include "DNodeOperation.h"

//bullet
#include "DBulletDataHeaders.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"

#include "mathUtils.h"

#include "commonDBullet.h"

#include "db_collision_base_shapes.h"
#include "db_collision_mesh_shapes.h"

#include "db_constraint_base.h"
#include <cmath>
#include <limits>
#include <regex>

#define DM_BT_RIGID_FRAC_DEBUG DOMMY_ENABLE_DEBUG_LOGS


const MTypeId DMbtFracture::typeId( ID_DM_BT_RIGID_FRACTURE );

MObject DMbtFracture::inDM;
MObject DMbtFracture::inConnectInfoDC;

//For RigidBody Param

MObject DMbtFracture::aMass;
MObject DMbtFracture::aRestitution;//反発
MObject DMbtFracture::aFriction;//摩擦
MObject DMbtFracture::aRollingFriction;//回転摩擦
MObject DMbtFracture::aLinearDamping;//減衰
MObject DMbtFracture::aAngularDamping;//回転減衰

MObject DMbtFracture::aInitialPosition;
MObject DMbtFracture::aInitialRotation;
MObject DMbtFracture::aInitialVelocity;
MObject DMbtFracture::aInitialSpin;

//For collision shape Param
MObject DMbtFracture::aCollisionType;
MObject DMbtFracture::aCollisionMargin;

//cluster
MObject DMbtFracture::aUseCluster;
MObject DMbtFracture::aClusterGroupName;

//constraint
MObject DMbtFracture::aUseBreakThreshold;
MObject DMbtFracture::aBreakThreshold;
MObject DMbtFracture::aUseBreakAngle;
MObject DMbtFracture::aBreakAngle;
//angle
MObject DMbtFracture::aLimitAngle;
MObject DMbtFracture::aSoftness;
MObject DMbtFracture::aBiasFactor;
MObject DMbtFracture::aRelaxationFactor;
MObject DMbtFracture::aDebugDisplaySize;

MObject DMbtFracture::aOverrideIterations;

MObject DMbtFracture::aStartTime;

MObject DMbtFracture::outDM;


DMbtFracture::~DMbtFracture()
{
    clearCollisionGeometry();
}


void DMbtFracture::detachFromWorld() noexcept
{
    const auto world = m_worldReference.lock();
    DetachAndReleaseBulletConstraints(world, connectConstLists);
    DetachAndReleaseBulletRigidBodies(world, rbList);
    DetachAndReleaseBulletRigidBodies(world, clsRbList);
    m_worldReference.reset();
}



void *DMbtFracture::creator() {
    return ::dommy::maya::createObject("DMbtFracture", [&]() {
        return new DMbtFracture();
    });
}


#pragma mark -
#pragma mark parameter

MStatus DMbtFracture::evalParms(btRigidParams& parms) {
    
    double rawMass = 0.0;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMass), rawMass));
    if (!dommy::numeric::checkedFloatCast(rawMass, parms.m_mass) ||
        (rawMass != 0 && parms.m_mass == 0)) return MS::kInvalidParameter;
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRestitution), parms.m_restitution));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aFriction), parms.m_friction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRollingFriction), parms.m_rollingFriction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLinearDamping), parms.m_linearDamping));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aAngularDamping), parms.m_angularDamping));
    
    
    
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aInitialPosition), parms.m_initPos));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aInitialRotation), parms.m_initRot));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aInitialVelocity), parms.m_initVel));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aInitialSpin), parms.m_initSpin));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionMargin), parms.m_collisionMargin));
    parms.m_collisionScale = MVector(1.0,1.0,1.0);
    
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakThreshold), parms.m_constParam.m_use_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakThreshold), parms.m_constParam.m_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakAngle), parms.m_constParam.m_use_break_angle));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakAngle), parms.m_constParam.m_break_angle));
    
    
    
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aLimitAngle),
                      parms.m_constParam.m_limit_angle));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSoftness), parms.m_constParam.m_softness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBiasFactor), parms.m_constParam.m_biasFactor));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRelaxationFactor), parms.m_constParam.m_relaxationFactor));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aOverrideIterations), parms.m_constParam.m_override_iterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_constParam.m_debug_display_size));
    
    
    return ValidateBulletRigidParameters(parms) &&
        ValidateBulletConstraintParameters(parms.m_constParam)
        ? MS::kSuccess : MS::kInvalidParameter;
}



#pragma mark -
#pragma mark init



MStatus DMbtFracture::resetRigidBodies(
    btSoftRigidDynamicsWorld* world,
    const DBulletWorldReferencePtr& worldReference)
{
    if (!world || !worldReference || worldReference->get() != world)
        return MS::kInvalidParameter;
    btRigidParams params;
    DOMMY_RETURN_IF_FAILURE(evalParms(params));
    const btVector3 gravity = world->getGravity();
    if (!dommy::bullet::safeMassGravity(params.m_mass,
            vec3f(gravity[0], gravity[1], gravity[2]))) return MS::kInvalidParameter;

    // Geometry has already been prepared by the solver. Read connection inputs
    // before replacing native bodies; output evaluation has no world side effects.
    MStatus status;
    const MPlug connectionPlug(thisMObject(), inConnectInfoDC);
    const bool connected = connectionPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::maya::ScopedPlugDataHandle curveOwner;
    const DCurveData* curves = nullptr;
    if (connected) {
        DOMMY_RETURN_IF_FAILURE(curveOwner.read(connectionPlug));
        curves = getInputDCfromHandle(curveOwner.handle());
        if (!curves) return MS::kInvalidParameter;
    }
    if (collisionShapes.size() != startTransformList.size() ||
        clusteredCollisionLists.size() != clusteredStartTransformList.size() ||
        clusteredCollisionLists.size() != clusteredIdLists.size()) return MS::kFailure;

    DPIndexList ids;
    for (size_t i = 0; i < collisionShapes.size(); ++i) {
        if (!collisionShapes[i] || !collisionShapes[i]->shape() ||
            !dommy::bullet::finiteConstraintTransform(startTransformList[i]))
            return MS::kInvalidParameter;
        ids.push_back(collisionShapes[i]->itemIndex());
    }
    for (size_t i = 0; i < clusteredCollisionLists.size(); ++i)
        if (!clusteredCollisionLists[i] || !clusteredCollisionLists[i]->shape() ||
            !clusteredCollisionLists[i]->shape()->isCompound() ||
            !dommy::bullet::finiteConstraintTransform(clusteredStartTransformList[i]))
            return MS::kInvalidParameter;
    btVector3 scale;
    if (!GetBulletCollisionScale(params.m_collisionScale, MVector(1, 1, 1), scale))
        return MS::kInvalidParameter;

    struct Connection {
        size_t first, second;
        bool firstCluster, secondCluster;
        btTransform frameA, frameB;
    };
    std::vector<Connection> connections;
    const auto findBody = [&](size_t id, size_t& index, bool& clustered) {
        clustered = false;
        for (size_t i = 0; i < clusteredIdLists.size(); ++i) {
            if (isContainDData(clusteredIdLists[i], id)) {
                index = i;
                clustered = true;
                return true;
            }
        }
        return dommy::container::findIndex(ids, id, index);
    };
    if (curves) {
        for (auto curve = curves->beginPrims(); curve != curves->endPrims(); ++curve) {
            if (!*curve) return MS::kInvalidParameter;
            if (!(*curve)->isExistAttr("prim_index")) continue;
            MIntArray indices;
            getDCPAttributeMIntArrayFromScalar(*curve, "prim_index", indices);
            if (indices.length() != 2 || indices[0] < 0 || indices[1] < 0) continue;
            Connection connection;
            if (!findBody(static_cast<size_t>(indices[0]), connection.first, connection.firstCluster) ||
                !findBody(static_cast<size_t>(indices[1]), connection.second, connection.secondCluster))
                continue;
            if (connection.firstCluster == connection.secondCluster &&
                connection.first == connection.second) continue;
            const DPointList* points = (*curve)->getPositionPtrConst();
            if (!points || points->size() != 2) continue;
            vec3f midpoint;
            for (int axis = 0; axis < 3; ++axis)
                midpoint[axis] = static_cast<float>(
                    0.5 * static_cast<double>((*points)[0][axis]) +
                    0.5 * static_cast<double>((*points)[1][axis]));
            const btTransform& first = connection.firstCluster
                ? clusteredStartTransformList[connection.first] : startTransformList[connection.first];
            const btTransform& second = connection.secondCluster
                ? clusteredStartTransformList[connection.second] : startTransformList[connection.second];
            const float halfAngle = static_cast<float>(SIMD_PI) * 0.25f;
            if (!dommy::bullet::prepareConstraintFrames(
                    midpoint, quatf(std::cos(halfAngle), 0, 0, std::sin(halfAngle)),
                    first, second, connection.frameA, connection.frameB))
                return MS::kInvalidParameter;
            connections.push_back(connection);
        }
    }
    MFnDependencyNode node(thisMObject(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MUuid uuid = node.uuid(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString nodeName = node.name(&status);
    DOMMY_RETURN_IF_FAILURE(status);

    detachFromWorld();
    m_worldReference = worldReference;
    const auto createBodies = [&](const std::vector<db_collision_shape_base*>& shapes,
                                  const std::vector<btTransform>& transforms,
                                  std::vector<btRigidBody*>& bodies,
                                  bool clustered) -> MStatus {
        for (size_t i = 0; i < shapes.size(); ++i) {
            btVector3 inertia;
            if (!PrepareBulletCollisionShape(*shapes[i], scale,
                    params.m_collisionMargin, params.m_mass, inertia))
                return MS::kInvalidParameter;
            std::unique_ptr<btMotionState> motion(new btDefaultMotionState(transforms[i]));
            btRigidBody::btRigidBodyConstructionInfo info(
                params.m_mass, motion.get(), shapes[i]->shape(), inertia);
            info.m_restitution = params.m_restitution;
            info.m_friction = params.m_friction;
            info.m_rollingFriction = params.m_rollingFriction;
            info.m_linearDamping = params.m_linearDamping;
            info.m_angularDamping = params.m_angularDamping;
            std::unique_ptr<customDBData> user;
            if (!clustered) {
                user.reset(new customDBData);
                user->nodeUuid = uuid;
                user->prefixName = nodeName.asChar();
                user->index = shapes[i]->itemIndex();
            }
            DBulletRigidBodyPtr body = MakeBulletRigidBody(std::move(motion), info, std::move(user));
            body->setLinearVelocity(btVector3(params.m_initVel[0], params.m_initVel[1], params.m_initVel[2]));
            body->setAngularVelocity(btVector3(params.m_initSpin[0], params.m_initSpin[1], params.m_initSpin[2]));
            AddBulletRigidBody(*world, bodies, std::move(body));
        }
        return MS::kSuccess;
    };
    DOMMY_RETURN_IF_FAILURE(createBodies(collisionShapes, startTransformList, rbList, false));
    DOMMY_RETURN_IF_FAILURE(createBodies(clusteredCollisionLists, clusteredStartTransformList, clsRbList, true));
    for (const auto& connection : connections) {
        btRigidBody* first = connection.firstCluster ? clsRbList[connection.first] : rbList[connection.first];
        btRigidBody* second = connection.secondCluster ? clsRbList[connection.second] : rbList[connection.second];
        std::unique_ptr<db_cone_constraint> constraint(
            new db_cone_constraint(*first, *second, connection.frameA, connection.frameB));
        constraint->set_Limit(
            btVector3(params.m_constParam.m_limit_angle[0], params.m_constParam.m_limit_angle[1],
                      params.m_constParam.m_limit_angle[2]),
            params.m_constParam.m_softness, params.m_constParam.m_biasFactor,
            params.m_constParam.m_relaxationFactor);
        if (params.m_constParam.m_use_break_threshold)
            constraint->set_breakThreshold(params.m_constParam.m_break_threshold);
        if (params.m_constParam.m_override_iterations > 0)
            constraint->set_OverrideNumSolverIterations(params.m_constParam.m_override_iterations);
        constraint->set_debugDrawSIze(params.m_constParam.m_debug_display_size);
        constraint->set_enabled(true);
        AddBulletConstraint(worldReference, connectConstLists, std::move(constraint), false);
    }
    preCollsionCount = collisionShapes.size();
    return MS::kSuccess;
}

#pragma mark -
#pragma mark update

MStatus DMbtFracture::updateRigidBodies(btSoftRigidDynamicsWorld* world)
{
    if (!world || ResolveBulletWorld(m_worldReference) != world)
        return MS::kInvalidParameter;
    btRigidParams params;
    DOMMY_RETURN_IF_FAILURE(evalParms(params));
    std::vector<db_cone_constraint*> broken;
    if (params.m_constParam.m_use_break_angle) {
        for (auto* constraint : connectConstLists) {
            if (!constraint || !constraint->constraint()) return MS::kFailure;
            if (constraint->constraint()->getConstraintType() == CONETWIST_CONSTRAINT_TYPE) {
                auto* cone = static_cast<db_cone_constraint*>(constraint);
                const double angle = cone->get_twistAngle();
                if (!std::isfinite(angle)) return MS::kInvalidParameter;
                if (params.m_constParam.m_break_angle <= angle) broken.push_back(cone);
            }
        }
    }
    for (auto* constraint : broken) constraint->set_enabled(false);
    return MS::kSuccess;
}


#pragma mark -
#pragma mark compute

void DMbtFracture::clearCollisionGeometry() noexcept
{
    detachFromWorld();
    ReleaseBulletCollisionShapes(collisionShapes);
    startTransformList.clear();
    ReleaseBulletCollisionShapes(clusteredCollisionLists);
    clusteredIdLists.clear();
    clusteredStartTransformList.clear();
    preCollsionCount = 0;
    m_geometryCount = 0;
    m_geometryGrouped = false;
    m_geometryGroupPattern.clear();
}

MStatus DMbtFracture::prepareCollisionGeometry(bool rebuild)
{
    dommy::maya::ScopedPlugDataHandle inputOwner;
    DOMMY_RETURN_IF_FAILURE(inputOwner.read(MPlug(thisMObject(), inDM)));
    const DMeshData* input = getInputDMfromHandle(inputOwner.handle());
    if (!input || !input->hasValidMeshTopology()) return MS::kInvalidParameter;
    const std::size_t count = input->numberOfMesh();
    if (count > dommy::generation::kMaxGeneratedTransformCount) return MS::kInvalidParameter;
    const bool replace = rebuild || count == 0;
    bool grouped = false;
    MString pattern;
    if (count != 0) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseCluster), grouped));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aClusterGroupName), pattern));
    }
    std::string groupPattern = grouped ? pattern.asChar() : "";
    if (!replace && (count != m_geometryCount ||
            grouped != m_geometryGrouped || groupPattern != m_geometryGroupPattern)) {
        EchoWarning(name().asChar(), "Collision geometry configuration changed; return to StartTime.");
        return MS::kFailure;
    }
    std::vector<DPIndexList> groups;
    if (replace && grouped && !groupPattern.empty()) {
        std::vector<std::string> names;
        getDMeshMatchedGroups(*input, groupPattern, names);
        for (const std::string& groupName : names) {
            const auto group = input->getGrpPtr(groupName);
            if (!group) return MS::kInvalidParameter;
            DPIndexList ids;
            group->getActiveIndexList(ids);
            if (!ids.empty()) groups.push_back(std::move(ids));
        }
    }
    DBulletCollisionType collisionType;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionType), collisionType));
    MStatus status;
    MFnDependencyNode node(thisMObject(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MUuid uuid = node.uuid(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString nodeName = node.name(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    DBulletGeometryCandidate candidate;
    DOMMY_RETURN_IF_FAILURE(PrepareBulletGeometryCandidate(count, replace ? 0 : m_geometryCount,
        groups, [&](std::size_t index) { return input->getMeshPtrConst(index); },
        [&](ConstDMItem item) {
            return createCollisionShape(collisionType, &item);
        }, uuid, nodeName.asChar(), candidate));
    collisionShapes.reserve((replace ? 0 : collisionShapes.size()) + candidate.single.size());
    startTransformList.reserve((replace ? 0 : startTransformList.size()) + candidate.transforms.size());
    clusteredCollisionLists.reserve((replace ? 0 : clusteredCollisionLists.size()) + candidate.compound.size());
    if (replace) {
        // Empty input releases native bodies too, while keeping the solver
        // association so subsequent empty update callbacks remain valid.
        const auto reference = m_worldReference;
        clearCollisionGeometry();
        m_worldReference = reference;
    }
    startTransformList.insert(startTransformList.end(), candidate.transforms.begin(), candidate.transforms.end());
    for (auto& shape : candidate.single) collisionShapes.push_back(shape.release());
    if (replace) {
        clusteredIdLists.swap(candidate.groups);
        clusteredStartTransformList.swap(candidate.compoundTransforms);
        for (auto& shape : candidate.compound) clusteredCollisionLists.push_back(shape.release());
    }
    m_geometryGrouped = grouped;
    m_geometryGroupPattern.swap(groupPattern);
    m_geometryCount = count;
    return MS::kSuccess;
}

MStatus DMbtFracture::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle inputHandle = data.inputValue(inDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DMeshData* input = getInputDMfromHandle(inputHandle);
    if (!input || !input->hasValidMeshTopology()) return MS::kInvalidParameter;
    MObject outputObject;
    DMeshData* outputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject(
        DMeshData::id, outputObject, outputData));
    if (!outputData) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*input));
    MDataHandle output = data.outputValue(outDM, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
        output, outputObject, *outputData));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMbtFracture")


MStatus DMbtFracture::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    inConnectInfoDC = tAttr.create("inConnectInfoDC", "icifdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inConnectInfoDC));
    
    
    
#pragma mark rigid body attribute
    aMass = nAttr.create("Mass", "ma", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMass));
    
    
    aRestitution = nAttr.create("Restitution", "rst", MFnNumericData::kDouble, 0.1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestitution));
    
    aFriction = nAttr.create("Friction", "fc", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFriction));
    
    aRollingFriction = nAttr.create("RollingFriction", "rfc", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRollingFriction));
    
    aLinearDamping = nAttr.create("LinearDamping", "ld", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLinearDamping));
    
    
    aAngularDamping = nAttr.create("AngularDamping", "ad", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngularDamping));
    
    
    aInitialPosition = nAttr.create("InitialPosition", "inpo", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInitialPosition));
    
    aInitialRotation = nAttr.create("InitialRotation", "inro", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInitialRotation));
    
    aInitialVelocity = nAttr.create("InitialVelocity", "inve", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInitialVelocity));
    
    aInitialSpin = nAttr.create("InitialSpin", "insp", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInitialSpin));
    
    
    
    
    
#pragma mark collision shape attribute
    
    MFnEnumAttribute colEnumAttr;
    aCollisionType = colEnumAttr.create("CollisionType", "ctp", D_COL_BVH_MESH, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("Convex Hull", D_COL_CONVEX_HULL));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("Mesh", D_COL_MESH));
    //    fnEnumAttr.addField("Cylinder", D_COL_CYLINDER);
    //    fnEnumAttr.addField("Capsule", D_COL_CAPSULE);
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("Box", D_COL_BOX));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("Sphere", D_COL_SPHERE));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("Plane", D_COL_PLANE));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("BvhMesh", D_COL_BVH_MESH));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.addField("HACD", D_COL_HACD));
    DOMMY_RETURN_IF_FAILURE(colEnumAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionType));
    
    aCollisionMargin = nAttr.create("CollisionMargin", "colm", MFnNumericData::kFloat, 0.04, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionMargin));
    
    
#pragma mark cluster attribute
    
    aUseCluster = nAttr.create("UseCluster", "useclstr", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseCluster));
    
    
    const MObject aClusterGroupNameDefaultData = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aClusterGroupName = tAttr.create("ClusterGroupName", "clstrgrpnm", MFnData::kString, aClusterGroupNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aClusterGroupName));
                                   
                                   
                                   
    
#pragma mark constraint attribute
    aUseBreakThreshold = nAttr.create("UseBreakThreshold", "usebrkthsd", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseBreakThreshold));
    
    aBreakThreshold = nAttr.create("BreakThreshold", "brkthsd", MFnNumericData::kDouble, 3.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBreakThreshold));
    
    aUseBreakAngle = nAttr.create("UseBreakAngle", "usebrkangl", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseBreakAngle));
    
    aBreakAngle = nAttr.create("BreakAngle", "brkangl", MFnNumericData::kDouble, 0.01, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBreakAngle));
    
    
    
    
    aLimitAngle = nAttr.create("LimitAngle", "lmtsngl", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLimitAngle));
    
    
    aSoftness = nAttr.create("Softness", "sftns", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSoftness));
    
    aBiasFactor = nAttr.create("BiasFactor", "bifac", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBiasFactor));
    
    aRelaxationFactor = nAttr.create("RelaxationFactor", "rlxfac", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRelaxationFactor));
    
    aOverrideIterations = nAttr.create("OverrideIterations", "ovriter", MFnNumericData::kInt, -1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOverrideIterations));
    
    
    //debug
    aDebugDisplaySize = nAttr.create("DebugDisplaySize", "dbgdspsz", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDebugDisplaySize));
    
    
    
    
    MFnUnitAttribute uniAttr;
    aStartTime = uniAttr.create( "StartTime", "stm" , MFnUnitAttribute::kTime, 1.0, &stat  );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartTime));
    
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inConnectInfoDC, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMass, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestitution, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFriction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRollingFriction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLinearDamping, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularDamping, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialPosition, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialRotation, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialVelocity, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialSpin, outDM ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionMargin, outDM ));
    
    
    //cluster
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseCluster, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aClusterGroupName, outDM ));
    
    
    //constarint
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakThreshold, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakThreshold, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakAngle, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakAngle, outDM ));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLimitAngle, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSoftness, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBiasFactor, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRelaxationFactor, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOverrideIterations, outDM ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStartTime, outDM ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMbtFracture", "initialize", std::current_exception());
}
