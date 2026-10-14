//
//  DCbtRope.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/13.
//

#include "DCbtRope.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"



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
#include "db_collision_base_shapes.h"
#include "commonDBullet.h"


#define DM_BULLET_ROPE_DEBUG DOMMY_ENABLE_DEBUG_LOGS



const MTypeId DCbtRope::typeId( ID_DM_BT_ROPE );

MObject DCbtRope::inDC;

MObject DCbtRope::aType;
MObject DCbtRope::aMass;
MObject DCbtRope::aRestitution;//反発
MObject DCbtRope::aFriction;//摩擦
MObject DCbtRope::aRollingFriction;//回転摩擦
MObject DCbtRope::aLinearDamping;//減衰
MObject DCbtRope::aAngularDamping;//回転減衰

MObject DCbtRope::aRadius;




//constraint
MObject DCbtRope::aBehaviorType;
MObject DCbtRope::aLockRoot;
MObject DCbtRope::aLockTip;

MObject DCbtRope::aLockRootType;
MObject DCbtRope::aLockRootPosition;
MObject DCbtRope::aRootDP;

MObject DCbtRope::aLockTipType;
MObject DCbtRope::aLockTipPosition;
MObject DCbtRope::aTipDP;

MObject DCbtRope::aIsLockAnimation;


MObject DCbtRope::aConstAngularDamping;
MObject DCbtRope::aLimitAngle;
MObject DCbtRope::aSoftness;
MObject DCbtRope::aBiasFactor;
MObject DCbtRope::aRelaxationFactor;


MObject DCbtRope::aUseUniformStiffness;
MObject DCbtRope::aUseUniformDamping;
MObject DCbtRope::aUniformStiffness;
MObject DCbtRope::aUniformDamping;
MObject DCbtRope::aTranslateStiffness;
MObject DCbtRope::aRotateStiffness;
MObject DCbtRope::aTranslateDamping;
MObject DCbtRope::aRotateDamping;
MObject DCbtRope::aIsRootSpring;

MObject DCbtRope::aDebugDisplaySize;

MObject DCbtRope::outDC;


DCbtRope::~DCbtRope()
{
    detachFromWorld();
}


void DCbtRope::detachFromWorld() noexcept
{
    const auto world = m_worldReference.lock();
    DetachAndReleaseBulletConstraints(world, rootConstLists);
    DetachAndReleaseBulletConstraints(world, tipConstLists);
    DetachAndReleaseBulletConstraints(world, midConstLists);
    DetachAndReleaseBulletRigidBodies(world, rbList);
    DetachAndReleaseBulletRigidBodies(world, rootRbList);
    DetachAndReleaseBulletRigidBodies(world, tipRbList);
    ReleaseBulletCollisionShapes(ropeShapes);
    ReleaseBulletCollisionShapes(rootRopeShapes);
    ReleaseBulletCollisionShapes(tipRopeShapes);
    m_worldReference.reset();
    m_initialized = false;
    m_curvePointCounts.clear();
}




void *DCbtRope::creator() {
    return ::dommy::maya::createObject("DCbtRope", [&]() {
        return new DCbtRope();
    });
}




#pragma mark -
#pragma mark init method

MStatus DCbtRope::evalParms(btRopeParams& parms) {
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aType), parms.m_type));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBehaviorType), parms.m_behavior_type));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMass), parms.m_mass));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRestitution), parms.m_restitution));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aFriction), parms.m_friction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRollingFriction), parms.m_rollingFriction));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLinearDamping), parms.m_linearDamping));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aAngularDamping), parms.m_angularDamping));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRadius), parms.m_radius));
    
    
    
    
    //constraint
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockRoot), parms.m_lock_root));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockTip), parms.m_lock_tip));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsLockAnimation), parms.m_is_lock));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockRootType), parms.m_root_lock_type));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockRootPosition), parms.m_root_lock_pos));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockTipType), parms.m_tip_lock_type));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aLockTipPosition), parms.m_tip_lock_pos));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aConstAngularDamping), parms.m_constParam.m_const_angular_damping));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(),aLimitAngle), parms.m_constParam.m_limit_angle));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSoftness), parms.m_constParam.m_softness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBiasFactor), parms.m_constParam.m_biasFactor));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRelaxationFactor), parms.m_constParam.m_relaxationFactor));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseUniformStiffness), parms.m_constParam.m_use_uniform_stiffness));
    
    if (parms.m_constParam.m_use_uniform_stiffness) {
        
        double stiff = 0.0;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUniformStiffness), stiff));
        parms.m_constParam.m_translate_stiffness = MVector(stiff,stiff,stiff);
        parms.m_constParam.m_rotate_stiffness = MVector(stiff,stiff,stiff);
    } else {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(),aTranslateStiffness), parms.m_constParam.m_translate_stiffness));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(),aRotateStiffness), parms.m_constParam.m_rotate_stiffness));
    }
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseUniformDamping), parms.m_constParam.m_use_uniform_damping));
    
    if (parms.m_constParam.m_use_uniform_damping) {
        
        double damp = 0.0;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUniformDamping), damp));
        parms.m_constParam.m_translate_damping = MVector(damp,damp,damp);
        parms.m_constParam.m_rotate_damping = MVector(damp,damp,damp);
    } else {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(),aTranslateDamping), parms.m_constParam.m_translate_damping));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(),aRotateDamping), parms.m_constParam.m_rotate_damping));
    }
   
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsRootSpring), parms.m_is_root_spring));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_constParam.m_debug_display_size));
    // Validate double-valued Maya positions before narrowing them to btScalar.
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::bullet::isSafeCollisionCoordinate(parms.m_root_lock_pos[axis]) ||
            !dommy::bullet::isSafeCollisionCoordinate(parms.m_tip_lock_pos[axis]))
            return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}



#pragma mark -
#pragma mark init method

MStatus getLockPosition(std::vector<btVector3> &positions, MPlug dpPlug) {
    MStatus status;
    const bool connected = dpPlug.isConnected(&status);
    if (!status) return status;
    if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "dpPlug connected:" << connected);

    if (connected) {
        dommy::maya::ScopedPlugDataHandle dpHndOwner;
        MDataHandle& dpHnd = dpHndOwner.handle();
        DOMMY_RETURN_IF_FAILURE(dpHndOwner.read(dpPlug));
        
        const DPData* const dpoint(getInputDPfromHandle(dpHnd));
        if (!dpoint) return MS::kFailure;
        
        if (dpoint && dpoint->numberOfPoints()) {
            if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "dpoint:" << dpoint->numberOfPoints());
            size_t ptCount = dpoint->numberOfPoints();
            const DPointList *ptsPtr = dpoint->getPositionPtrConst();
            
            size_t pIndex =0;
            for (auto pIt=positions.begin(); pIt!=positions.end(); ++pIt) {
                
                (*pIt) = btVector3((*ptsPtr)[pIndex][0],(*ptsPtr)[pIndex][1],(*ptsPtr)[pIndex][2]);
                
                pIndex++;
                if (ptCount <= pIndex) {
                    pIndex=0;
                }
            }
        }
    }
    for (const btVector3& position : positions) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::bullet::isSafeCollisionCoordinate(position[axis]))
                return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}



std::unique_ptr<db_sixdof_spring_constraint> createSixdofSpringConst(
    btRopeParams &param, btRigidBody *rbA, btRigidBody *rbB,
    btTransform &transA, btTransform &transB) {
    
    std::unique_ptr<db_sixdof_spring_constraint> sixSpConst;
    if (rbB == nullptr) {
        sixSpConst.reset(new db_sixdof_spring_constraint(
            *rbA, transA, true));
    }else {
        sixSpConst.reset(new db_sixdof_spring_constraint(
            *rbA, *rbB, transA, transB, true));
    }
   
    
    //sixSpConst->set_LinLimit(btVector3 &lower, btVector3 &upper);
    //sixSpConst->set_AngLimit(btVector3 &lower, btVector3 &upper);
    
    
    // enable spring
    sixSpConst->set_enableSpring(true);
    btVector3 tStiff(param.m_constParam.m_translate_stiffness.x, param.m_constParam.m_translate_stiffness.y, param.m_constParam.m_translate_stiffness.z);
    sixSpConst->set_translateStiffness(tStiff);
    btVector3 rStiff(param.m_constParam.m_rotate_stiffness.x, param.m_constParam.m_rotate_stiffness.y, param.m_constParam.m_rotate_stiffness.z);
    sixSpConst->set_rotateStiffness(rStiff);
    btVector3 tDamp(param.m_constParam.m_translate_damping.x, param.m_constParam.m_translate_damping.y, param.m_constParam.m_translate_damping.z);
    sixSpConst->set_translateDamping(tDamp);
    btVector3 rDamp(param.m_constParam.m_rotate_damping.x, param.m_constParam.m_rotate_damping.y, param.m_constParam.m_rotate_damping.z);
    sixSpConst->set_rotateDamping(rDamp);
    
    // constraint force mixing prevents "locking" on limits
    //                        sixSpConst->setParam(BT_CONSTRAINT_STOP_CFM, 1.0e-5f, 5);
    sixSpConst->setEquilibriumPoint(); // set current position as equilibrium point
    sixSpConst->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
    
    return sixSpConst;
}

std::unique_ptr<db_cone_constraint> createConeConst(
    btRopeParams &param, btRigidBody *rbA, btRigidBody *rbB,
    btTransform &transA, btTransform &transB) {
    std::unique_ptr<db_cone_constraint> constObj(
        new db_cone_constraint(*rbA, *rbB, transA, transB));
    btVector3 angleVec(param.m_constParam.m_limit_angle.x,
                       param.m_constParam.m_limit_angle.y,
                       param.m_constParam.m_limit_angle.z);
    constObj->set_Limit(angleVec,
                        param.m_constParam.m_softness,
                        param.m_constParam.m_biasFactor,
                        param.m_constParam.m_relaxationFactor);
    constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
    
    return constObj;
}



namespace {
MStatus readRopeTopology(const DCurveData& input, std::vector<std::size_t>& result)
{
    if (input.numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount)
        return MS::kInvalidParameter;
    std::vector<std::size_t> candidate;
    candidate.reserve(input.numberOfCurves());
    std::size_t segments = 0;
    for (std::size_t i = 0; i < input.numberOfCurves(); ++i) {
        const auto curve = input.getCurvePtr(i);
        if (!curve || curve->numberOfPoints() < 2 ||
            !dommy::generation::checkedSum(segments, curve->numberOfPoints() - 1,
                dommy::generation::kMaxGeneratedTransformCount, segments))
            return MS::kInvalidParameter;
        candidate.push_back(curve->numberOfPoints());
    }
    result.swap(candidate);
    return MS::kSuccess;
}
}

MStatus DCbtRope::resetRopes(
    btSoftRigidDynamicsWorld* m_dynamicsWorld,
    btSoftBodyWorldInfo* m_worldInfo,
    const DBulletWorldReferencePtr& worldReference) {
    if (!m_dynamicsWorld || !m_worldInfo || !worldReference ||
        worldReference->get() != m_dynamicsWorld) return MS::kInvalidParameter;
    
    MPlug dcPlug(thisMObject(), inDC);
    dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
    MDataHandle& dCurveHnd = dCurveHndOwner.handle();
    DOMMY_RETURN_IF_FAILURE(dCurveHndOwner.read(dcPlug));
    
    const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
    if (!dCurves) return MS::kFailure;

    std::vector<std::size_t> topology;
    DOMMY_RETURN_IF_FAILURE(readRopeTopology(*dCurves, topology));
    btRopeParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    
    
    //check input Dpoint for root Positions
    std::vector<btVector3> rootPositions(dCurves->numberOfCurves(),btVector3(param.m_root_lock_pos[0],param.m_root_lock_pos[1],param.m_root_lock_pos[2]));
    std::vector<btVector3> tipPositions(dCurves->numberOfCurves(),btVector3(param.m_tip_lock_pos[0],param.m_tip_lock_pos[1],param.m_tip_lock_pos[2]));
    if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "rootPositions:" << rootPositions.size());
    if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "tipPositions:" << tipPositions.size());
    if (param.m_root_lock_type == DB_LOCK_DPOINT) {
        DOMMY_RETURN_IF_FAILURE(getLockPosition(rootPositions, MPlug(thisMObject(), aRootDP)));
    }
    if (param.m_tip_lock_type == DB_LOCK_DPOINT) {
        DOMMY_RETURN_IF_FAILURE(getLockPosition(tipPositions, MPlug(thisMObject(), aTipDP)));
    }

    // Preflight every segment before detaching any existing world resource.
    std::vector<float> capsuleHeights;
    for (auto curve = dCurves->beginPrims(); curve != dCurves->endPrims(); ++curve) {
        const DPointList& points = *(*curve)->getPositionPtrConst();
        for (size_t i = 1; i < points.size(); ++i) {
            float height = 0.0f;
            if (!dommy::bullet::ropeCapsuleHeight(points[i-1], points[i], param.m_radius, height)) {
                EchoWarning(name().asChar(), "Rope segments must be at least twice the positive capsule radius.");
                return MS::kInvalidParameter;
            }
            capsuleHeights.push_back(height);
        }
    }
    for (const auto* positions : {&rootPositions, &tipPositions}) {
        for (const btVector3& position : *positions) {
            for (int axis = 0; axis < 3; ++axis) {
                if (!dommy::bullet::isSafeCollisionCoordinate(position[axis]))
                    return MS::kInvalidParameter;
            }
        }
    }

    detachFromWorld();
    m_worldReference = worldReference;
    if (dCurves->numberOfCurves()) {
        size_t segmentIndex = 0;
        MFnDependencyNode nodeFn(thisMObject());
        
        
        MVector upAxis(0,1,0);
        
        size_t cIndex = 0;
        for (auto cIt=dCurves->beginPrims(); cIt!=dCurves->endPrims(); ++cIt) {
            
            
            size_t cvCount = (*cIt)->numberOfPoints();
            const DPointList *ptsPtr = (*cIt)->getPositionPtrConst();
            
            size_t numcap = cvCount-1;
            
            //pre rigidbody data
            btRigidBody* preRb = nullptr;
            float preConstLength;
            btTransform preTransMat;
            
            for (size_t i=0; i<numcap; i++) {
                
                DPoint sourcePt((*ptsPtr)[i]);
                DPoint targetPt((*ptsPtr)[i+1]);
                
                MVector lookVec(
                    static_cast<double>(targetPt[0]) - sourcePt[0],
                    static_cast<double>(targetPt[1]) - sourcePt[1],
                    static_cast<double>(targetPt[2]) - sourcePt[2]);
                const float height = capsuleHeights[segmentIndex++];
                float constLength = height*.5+param.m_radius;
                
//                if(DM_BULLET_ROPE_DEBUG) cout << "cap height:" << height << endl;
//                if(DM_BULLET_ROPE_DEBUG) cout << "constLength:" << constLength << endl;
                
                std::unique_ptr<db_collision_shape_base> colShape(
                    new db_collision_capsule(param.m_radius, height));
                btVector3 localInertia(0,0,0);
                colShape->shape()->calculateLocalInertia(param.m_mass,localInertia);
                
                
                
                DPoint center(
                    sourcePt[0] + static_cast<float>(lookVec.x * 0.5),
                    sourcePt[1] + static_cast<float>(lookVec.y * 0.5),
                    sourcePt[2] + static_cast<float>(lookVec.z * 0.5));
                
                MVector unitLookVec = lookVec.normal();
                MQuaternion mquart = upAxis.rotateTo(unitLookVec);
                
                btTransform xformLocal(btQuaternion(mquart.x, mquart.y, mquart.z, mquart.w), btVector3(center[0], center[1], center[2]));
                
                
                std::unique_ptr<btMotionState> motionState(
                    new btDefaultMotionState(xformLocal));
                
                
                //Create Rigid Body
                btRigidBody::btRigidBodyConstructionInfo rbInfo(
                    param.m_mass, motionState.get(), colShape->shape(),
                    localInertia);
                rbInfo.m_restitution = param.m_restitution ;
                rbInfo.m_friction = param.m_friction;
                rbInfo.m_rollingFriction = param.m_rollingFriction;
                rbInfo.m_linearDamping = param.m_linearDamping;
                rbInfo.m_angularDamping = param.m_angularDamping;
                std::unique_ptr<customDBData> userData(new customDBData());
                userData->nodeUuid = nodeFn.uuid();
                userData->prefixName = nodeFn.name().asChar();
                userData->index = cIndex;
                userData->ptIndex = i;
                DBulletRigidBodyPtr body = MakeBulletRigidBody(
                    std::move(motionState), rbInfo, std::move(userData));
                btRigidBody* const rb = body.get();

                ropeShapes.reserve(ropeShapes.size() + 1);
                ropeShapes.push_back(colShape.get());
                try {
                    AddBulletRigidBody(
                        *m_dynamicsWorld, rbList, std::move(body));
                } catch (...) {
                    ropeShapes.pop_back();
                    throw;
                }
                colShape.release();
                
                
                //create transform
                MPoint tipPos(0,constLength,0);
                MPoint rootPos(0,-constLength,0);
                btTransform tipTransMat, rootTransMat;
                tipTransMat = btTransform::getIdentity();
                tipTransMat.getBasis().setEulerZYX(0, 0, (SIMD_PI)*0.5f);
                tipTransMat.setOrigin(btVector3(tipPos[0], tipPos[1], tipPos[2]));
                rootTransMat = btTransform::getIdentity();
                rootTransMat.getBasis().setEulerZYX(0,0, (SIMD_PI)*0.5f);
                rootTransMat.setOrigin(btVector3(rootPos[0], rootPos[1], rootPos[2]));
                
                
#pragma mark root constraint
                if (i==0 && param.m_lock_root) {
                    
                    if (param.m_behavior_type == DB_ROPE_BHVR_SHAPE_SPRING && param.m_is_root_spring) {
                        
                        
                        btVector3 rootPos(0,-constLength,0);
                        
                        if (param.m_root_lock_type == DB_LOCK_POINT ||
                            param.m_root_lock_type == DB_LOCK_DPOINT) {
                            
                            std::unique_ptr<db_collision_shape_base> col(
                                new db_collision_box(
                                    vec3f(0.01f, 0.01f, 0.01f)));
                            DBulletRigidBodyPtr dummyBody =
                                MakeSimpleKinematicRigidBody(
                                    col->shape(), rootPositions[cIndex]);
                            btRigidBody* const rbimpt = dummyBody.get();
                            AddBulletRigidBodyWithShape(
                                *m_dynamicsWorld, rootRbList,
                                rootRopeShapes, std::move(dummyBody),
                                std::move(col));
                            
                            btTransform boxTrans;
                            boxTrans.setIdentity();boxTrans.setOrigin(btVector3(0,0,0));
                            auto constObj = createSixdofSpringConst(
                                param, rbimpt, rb, boxTrans,
                                rootTransMat);
                            AddBulletConstraint(
                                worldReference, rootConstLists,
                                std::move(constObj), false);
                        } else {
                            btTransform empty;
                            auto constObj = createSixdofSpringConst(
                                param, rb, nullptr, rootTransMat, empty);
                            AddBulletConstraint(
                                worldReference, rootConstLists,
                                std::move(constObj), false);
                        }
                        
                    } else {
                        
                        btVector3 rootPos(0,-constLength,0);
                        
                        if (param.m_root_lock_type == DB_LOCK_POINT ||
                            param.m_root_lock_type == DB_LOCK_DPOINT) {
                            
                            
                            std::unique_ptr<db_collision_shape_base> col(
                                new db_collision_box(
                                    vec3f(0.01f, 0.01f, 0.01f)));
                            DBulletRigidBodyPtr dummyBody =
                                MakeSimpleKinematicRigidBody(
                                    col->shape(), rootPositions[cIndex]);
                            btRigidBody* const rbimpt = dummyBody.get();
                            AddBulletRigidBodyWithShape(
                                *m_dynamicsWorld, rootRbList,
                                rootRopeShapes, std::move(dummyBody),
                                std::move(col));
                            
                            std::unique_ptr<db_p2p_constraint> constObj(
                                new db_p2p_constraint(
                                    *rbimpt, *rb, btVector3(0,0,0),
                                    rootPos));
                            constObj->set_damping(param.m_constParam.m_const_angular_damping);
                            constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
                            AddBulletConstraint(
                                worldReference, rootConstLists,
                                std::move(constObj), true);
                        } else {
                            
                            std::unique_ptr<db_p2p_constraint> constObj(
                                new db_p2p_constraint(*rb, rootPos));
                            constObj->set_damping(param.m_constParam.m_const_angular_damping);
                            constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
                            AddBulletConstraint(
                                worldReference, rootConstLists,
                                std::move(constObj));
                        }
                        
                        
                    }
                    
                    
                    
                }
                
                
#pragma mark middle constraint
                if (preRb) {
                    
                    
                    if(param.m_behavior_type == DB_ROPE_BHVR_FREE) {
                        std::unique_ptr<db_p2p_constraint> constObj(
                            new db_p2p_constraint(
                                *preRb, *rb,
                                btVector3(0,preConstLength,0),
                                btVector3(0,-constLength,0)));
                        constObj->set_damping(param.m_constParam.m_const_angular_damping);
                        constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
                        AddBulletConstraint(
                            worldReference, midConstLists,
                            std::move(constObj));
                        
                        
                        
                    } else if (param.m_behavior_type == DB_ROPE_BHVR_ANGULAR_LIMIT){
                        
                        auto constObj = createConeConst(
                            param, preRb, rb, preTransMat,
                            rootTransMat);
                        AddBulletConstraint(
                            worldReference, midConstLists,
                            std::move(constObj));
                        
                        
                    } else {
                        //                        DB_ROPE_BHVR_SHAPE_SPRING
                        auto constObj = createSixdofSpringConst(
                            param, preRb, rb, preTransMat,
                            rootTransMat);
                        AddBulletConstraint(
                            worldReference, midConstLists,
                            std::move(constObj), false);
                    }
                    
                    
                    
                    
                }
                
#pragma mark tip constraint
                if (i==numcap-1 && param.m_lock_tip) {
                    
                    
                    btVector3 tipPos(0,constLength,0);
                    
                    if (param.m_tip_lock_type == DB_LOCK_POINT ||
                        param.m_tip_lock_type == DB_LOCK_DPOINT) {
                        
                        std::unique_ptr<db_collision_shape_base> col(
                            new db_collision_box(
                                vec3f(0.01f, 0.01f, 0.01f)));
                        DBulletRigidBodyPtr dummyBody =
                            MakeSimpleKinematicRigidBody(
                                col->shape(), tipPositions[cIndex]);
                        btRigidBody* const rbimpt = dummyBody.get();
                        AddBulletRigidBodyWithShape(
                            *m_dynamicsWorld, tipRbList,
                            tipRopeShapes, std::move(dummyBody),
                            std::move(col));
                        
                        std::unique_ptr<db_p2p_constraint> constObj(
                            new db_p2p_constraint(
                                *rbimpt, *rb, btVector3(0,0,0),
                                tipPos));
                        constObj->set_damping(param.m_constParam.m_const_angular_damping);
                        constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
                        AddBulletConstraint(
                            worldReference, tipConstLists,
                            std::move(constObj), true);
                    } else {
                        
                        std::unique_ptr<db_p2p_constraint> constObj(
                            new db_p2p_constraint(*rb, tipPos));
                        constObj->set_damping(param.m_constParam.m_const_angular_damping);
                        constObj->set_debugDrawSIze(param.m_constParam.m_debug_display_size);
                        AddBulletConstraint(
                            worldReference, tipConstLists,
                            std::move(constObj));
                    }
                    
                    
                    
                }
                
                
                preRb = rb;
                preConstLength = constLength;
                preTransMat = tipTransMat;
                
            }//loop point
            
            
            
            
            cIndex++;
            
        }//loop curve
        
    }//check curve count
    m_initialParameters = param;
    m_curvePointCounts.swap(topology);
    m_initialized = true;
    return MS::kSuccess;
}

#pragma mark -
#pragma mark update

MStatus DCbtRope::updateRopes(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo) {
    if (!m_initialized || !m_dynamicsWorld || !m_worldInfo ||
        ResolveBulletWorld(m_worldReference) != m_dynamicsWorld) return MS::kFailure;

    dommy::maya::ScopedPlugDataHandle inputOwner;
    DOMMY_RETURN_IF_FAILURE(inputOwner.read(MPlug(thisMObject(), inDC)));
    const DCurveData* input = getInputDCfromHandle(inputOwner.handle());
    if (!input) return MS::kInvalidParameter;
    std::vector<std::size_t> topology;
    DOMMY_RETURN_IF_FAILURE(readRopeTopology(*input, topology));
    if (topology.empty()) {
        const auto reference = m_worldReference;
        detachFromWorld();
        m_worldReference = reference;
        m_initialized = true;
        return MS::kSuccess;
    }
    if (topology != m_curvePointCounts) {
        EchoWarning(name().asChar(), "Rope topology changed; return to StartTime.");
        return MS::kFailure;
    }
    btRopeParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));
    // Turning an existing constraint off/on is safe. Creating a new endpoint,
    // changing its frame type or rebuilding a spring requires StartTime.
    if (!rbList.empty() &&
        ((param.m_lock_root && !m_initialParameters.m_lock_root) ||
         (param.m_lock_tip && !m_initialParameters.m_lock_tip) ||
         param.m_root_lock_type != m_initialParameters.m_root_lock_type ||
         param.m_tip_lock_type != m_initialParameters.m_tip_lock_type ||
         param.m_is_root_spring != m_initialParameters.m_is_root_spring ||
         param.m_behavior_type != m_initialParameters.m_behavior_type)) {
        EchoWarning(name().asChar(), "Rope constraint configuration changed; return to StartTime.");
        return MS::kFailure;
    }
    for (const auto* list : {&rootConstLists, &tipConstLists})
        for (auto* constraint : *list)
            if (!constraint || !constraint->constraint()) return MS::kFailure;
    for (auto* constraint : rootConstLists)
        constraint->constraint()->setEnabled(param.m_lock_root);
    for (auto* constraint : tipConstLists)
        constraint->constraint()->setEnabled(param.m_lock_tip);
    return MS::kSuccess;
}



MStatus DCbtRope::updateLockPosition(btSoftRigidDynamicsWorld* m_dynamicsWorld, btSoftBodyWorldInfo* m_worldInfo, float time) {
    if (!m_initialized || !m_dynamicsWorld || !m_worldInfo ||
        ResolveBulletWorld(m_worldReference) != m_dynamicsWorld ||
        !std::isfinite(time) || time < 0.0f || time > 1.0f) return MS::kInvalidParameter;
    if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "updateLockPosition:");
    if(DM_BULLET_ROPE_DEBUG) DOMMY_MAYA_DEBUG("DCbtRope", "time:" << time);
    
    bool isLockAnim = false;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsLockAnimation), isLockAnim));
    
    if (!isLockAnim) {
        return MS::kSuccess;
    }
    
    btRopeParams param;
    DOMMY_RETURN_IF_FAILURE(evalParms(param));

    // Read both endpoints before changing either motion state.
    std::vector<btVector3> rootPositions(rootRopeShapes.size(),
        btVector3(param.m_root_lock_pos[0], param.m_root_lock_pos[1], param.m_root_lock_pos[2]));
    std::vector<btVector3> tipPositions(tipRopeShapes.size(),
        btVector3(param.m_tip_lock_pos[0], param.m_tip_lock_pos[1], param.m_tip_lock_pos[2]));
    if (!rootPositions.empty() && param.m_root_lock_type == DB_LOCK_DPOINT)
        DOMMY_RETURN_IF_FAILURE(getLockPosition(rootPositions, MPlug(thisMObject(), aRootDP)));
    if (!tipPositions.empty() && param.m_tip_lock_type == DB_LOCK_DPOINT)
        DOMMY_RETURN_IF_FAILURE(getLockPosition(tipPositions, MPlug(thisMObject(), aTipDP)));
    for (const auto* positions : {&rootPositions, &tipPositions}) {
        for (const btVector3& position : *positions) {
            for (int axis = 0; axis < 3; ++axis) {
                if (!dommy::bullet::isSafeCollisionCoordinate(position[axis]))
                    return MS::kInvalidParameter;
            }
        }
    }

    for (const auto* list : {&rootConstLists, &tipConstLists})
        for (auto* constraint : *list)
            if (!constraint || !constraint->constraint()) return MS::kFailure;

    //update lock positions
    if (rootRopeShapes.size()>0 && rootRopeShapes.size() == rootConstLists.size()) {
        size_t index = 0;
        for (auto it=rootConstLists.begin(); it!=rootConstLists.end(); ++it) {
            
            // Borrow the world-owned body; copying also copies its constraint references.
            btRigidBody& body = (*it)->constraint()->getRigidBodyA();
            
            vec3f pos0;
            quatf rot0;
            getDBtTransformTR(&body, pos0, rot0);
            
            vec3f pos1 = vec3f((float)rootPositions[index][0], (float)rootPositions[index][1], (float)rootPositions[index][2]);
            quatf rot1 = quatf(1.0, 0.0, 0.0, 0.0);
            
            set_DB_RigidTransform(&body, (pos0 + time * (pos1 - pos0)), rot1);
            
            
            index++;
        }
        
        
    }
    
    if (tipRopeShapes.size()>0 && tipRopeShapes.size() == tipConstLists.size()) {
        size_t index = 0;
        for (auto it=tipConstLists.begin(); it!=tipConstLists.end(); ++it) {
            
            // Borrow the world-owned body; copying also copies its constraint references.
            btRigidBody& body = (*it)->constraint()->getRigidBodyA();
            
            vec3f pos0;
            quatf rot0;
            getDBtTransformTR(&body, pos0, rot0);
            
            vec3f pos1 = vec3f((float)tipPositions[index][0], (float)tipPositions[index][1], (float)tipPositions[index][2]);
            quatf rot1 = quatf(1.0, 0.0, 0.0, 0.0);
            
            set_DB_RigidTransform(&body, (pos0 + time * (pos1 - pos0)), rot1);
            
            
            index++;
        }
        
        
    }
    return MS::kSuccess;
}

#pragma mark -
#pragma mark compute

MStatus DCbtRope::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    MStatus status;
    MDataHandle inputHandle = data.inputValue(inDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DCurveData* input = getInputDCfromHandle(inputHandle);
    if (!input) return MS::kInvalidParameter;
    MObject outputObject;
    DCurveData* outputData = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject(
        DCurveData::id, outputObject, outputData));
    if (!outputData) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*input));
    MDataHandle output = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
        output, outputObject, *outputData));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCbtRope")


MStatus DCbtRope::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    MFnEnumAttribute aeroEnumFn;
    aType = aeroEnumFn.create( "Type", "typ", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "Rigid", 0 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.addField( "Wire", 1 ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(aeroEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));
    
#pragma mark rigid body attribute
    aMass = nAttr.create("Mass", "ma", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50.0));
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
    
    aRollingFriction = nAttr.create("RollingFriction", "rfc", MFnNumericData::kDouble, 0.05, &stat);
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
    
    
    
    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kDouble, 0.5, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));
    
    
    
    
    
    
    
    
    
    
    
    //constraint
    
    
    MFnEnumAttribute behaviorEnumFn;
    aBehaviorType = behaviorEnumFn.create( "BehaviorType", "bhvr", DB_ROPE_BHVR_FREE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(behaviorEnumFn.addField( "Free", DB_ROPE_BHVR_FREE ));
    DOMMY_RETURN_IF_FAILURE(behaviorEnumFn.addField( "Angle Limit", DB_ROPE_BHVR_ANGULAR_LIMIT ));
    DOMMY_RETURN_IF_FAILURE(behaviorEnumFn.addField( "Shape Spring", DB_ROPE_BHVR_SHAPE_SPRING ));
    DOMMY_RETURN_IF_FAILURE(behaviorEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(behaviorEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBehaviorType));
    
    
    aLockRoot = nAttr.create("LockRoot", "lckrt", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockRoot));
    
    aLockTip = nAttr.create("LockTip", "lcktp", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockTip));
    
    
    
    
    MFnEnumAttribute lockRootEnumFn;
    aLockRootType = lockRootEnumFn.create( "LockRootType", "lckrtty", DB_LOCK_BASE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(lockRootEnumFn.addField( "Base", DB_LOCK_BASE ));
    DOMMY_RETURN_IF_FAILURE(lockRootEnumFn.addField( "Point", DB_LOCK_POINT ));
    DOMMY_RETURN_IF_FAILURE(lockRootEnumFn.addField( "DPoint", DB_LOCK_DPOINT ));
    DOMMY_RETURN_IF_FAILURE(lockRootEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(lockRootEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockRootType));
    
    aLockRootPosition = nAttr.create("LockRootPosition", "lckrtpos", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockRootPosition));
    
    
    aRootDP = tAttr.create("RootDP", "rtdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRootDP));
    
    
    
    MFnEnumAttribute lockTipEnumFn;
    aLockTipType = lockTipEnumFn.create( "LockTipType", "lcktpty", DB_LOCK_BASE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(lockTipEnumFn.addField( "Base", DB_LOCK_BASE ));
    DOMMY_RETURN_IF_FAILURE(lockTipEnumFn.addField( "Point", DB_LOCK_POINT ));
    DOMMY_RETURN_IF_FAILURE(lockTipEnumFn.addField( "DPoint", DB_LOCK_DPOINT ));
    DOMMY_RETURN_IF_FAILURE(lockTipEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(lockTipEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockTipType));
    
    aLockTipPosition = nAttr.create("LockTipPosition", "lcktppos", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLockTipPosition));
    
    
    aTipDP = tAttr.create("TipDP", "tpdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTipDP));
    
    
    aIsLockAnimation = nAttr.create("IsLockAnimation", "islckanim", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsLockAnimation));
    
    
    
    
    aConstAngularDamping = nAttr.create("ConstAngularDamping", "cnstangldmp", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConstAngularDamping));
    
    aLimitAngle = nAttr.create("LimitAngle", "lmtsngl", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(90.0,10.0,90.0));
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
    
    
    
    aUseUniformStiffness = nAttr.create("UseUniformStiffness", "usunistif", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseUniformStiffness));
    
    aUniformStiffness = nAttr.create("UniformStiffness", "unistif", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(200.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformStiffness));
    
    aUseUniformDamping = nAttr.create("UseUniformDamping", "usunidmp", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseUniformDamping));
    
    aUniformDamping = nAttr.create("UniformDamping", "unidmp", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformDamping));
    
    
    
    
    aTranslateStiffness = nAttr.create("TranslateStiffness", "transstif", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0,40.0,40.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateStiffness));
    
    aRotateStiffness = nAttr.create("RotateStiffness", "rotstif", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0,40.0,40.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateStiffness));
    
    aTranslateDamping = nAttr.create("TranslateDamping", "transdmp", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01,0.01,0.01));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateDamping));
    
    aRotateDamping = nAttr.create("RotateDamping", "rotdmp", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01,0.01,0.01));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateDamping));
    
    
    aIsRootSpring = nAttr.create("IsRootSpring", "usrtsp", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRootSpring));
    
    
    
    
    //debug
    aDebugDisplaySize = nAttr.create("DebugDisplaySize", "dbgdspsz", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDebugDisplaySize));
    
    
    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMass, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestitution, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFriction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRollingFriction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLinearDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularDamping, outDC ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadius, outDC ));
    
    //constraint
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBehaviorType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockRoot, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockTip, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockRootType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockRootPosition, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRootDP, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockTipType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLockTipPosition, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTipDP, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsLockAnimation, outDC ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstAngularDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLimitAngle, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSoftness, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBiasFactor, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRelaxationFactor, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseUniformStiffness, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseUniformDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformStiffness, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateStiffness, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateStiffness, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsRootSpring, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outDC ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCbtRope", "initialize", std::current_exception());
}
