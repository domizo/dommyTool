//
//  DCbtRigid2D.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/14.
//

#include "DCbtRigid2D.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "DBulletGeometryPreparation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveDataHeaders.h"
#include "DDataOp.h"
#include "DNodeOperation.h"


//bullet
#include "DBulletDataHeaders.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "BulletCollision/CollisionShapes/btBox2dShape.h"
#include "BulletCollision/CollisionShapes/btConvex2dShape.h"


#include "mathUtils.h"

#include "commonDBullet.h"
#include "Solver/DPassiveTransformMath.h"
#include "commonDBulletCollision.h"

#include "db_collision_base_shapes.h"



#define DC_BULLET_RIGID_DEBUG DOMMY_ENABLE_DEBUG_LOGS



const MTypeId DCbtRigid2D::typeId( ID_DC_BT_RIGID2D );

MObject DCbtRigid2D::inDC;

MObject DCbtRigid2D::aMass;
MObject DCbtRigid2D::aRestitution;//反発
MObject DCbtRigid2D::aFriction;//摩擦
MObject DCbtRigid2D::aRollingFriction;//回転摩擦
MObject DCbtRigid2D::aLinearDamping;//減衰
MObject DCbtRigid2D::aAngularDamping;//回転減衰

MObject DCbtRigid2D::aInitialPosition;
MObject DCbtRigid2D::aInitialRotation;
MObject DCbtRigid2D::aInitialVelocity;
MObject DCbtRigid2D::aInitialSpin;

MObject DCbtRigid2D::aExternalForce;
MObject DCbtRigid2D::aExternalTorque;

//MObject DCbtRigid2D::aCollisionType;
MObject DCbtRigid2D::aCollisionScale;
MObject DCbtRigid2D::aCollisionMargin;
MObject DCbtRigid2D::aIsCollisionDeform;

MObject DCbtRigid2D::aIsCompound;
MObject DCbtRigid2D::aCompoundGroupName;

MObject DCbtRigid2D::aStartSleeping;

MObject DCbtRigid2D::outDC;


DCbtRigid2D::~DCbtRigid2D()
{
    clearCollisionGeometry();
}


void DCbtRigid2D::detachFromWorld() noexcept
{
    const auto world = m_worldReference.lock();
    DetachAndReleaseBulletRigidBodies(world, rbList);
    DetachAndReleaseBulletRigidBodies(world, compRbList);
    m_worldReference.reset();
}



void *DCbtRigid2D::creator() {
    return ::dommy::maya::createObject("DCbtRigid2D", [&]() {
        return new DCbtRigid2D();
    });
}





#pragma mark -
#pragma mark parameter

MStatus DCbtRigid2D::evalParms(btRigidParams& parms) {
    
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
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aCollisionScale),
                      parms.m_collisionScale));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aStartSleeping), parms.m_start_sleeping));
    
    return ValidateBulletRigidParameters(parms) ? MS::kSuccess : MS::kInvalidParameter;
}




#pragma mark -
#pragma mark init

void DCbtRigid2D::clearRigidBodies() {
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "DCbtRigid2D::clearRigidBodies");
    
    
    
}


MStatus DCbtRigid2D::resetRigidBodies(
    btDiscreteDynamicsWorld* m_dynamicsWorld,
    const DBulletWorldReferencePtr& worldReference) {
    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "create collisionShapes:" << collisionShapes.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "startTransformList count:" << startTransformList.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "startScaleList count:" << startScaleList.size());
    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "delete rigid body:" << collisionShapes.size());
    
    if (!m_dynamicsWorld || !worldReference ||
        worldReference->get() != m_dynamicsWorld) {
        return MS::kFailure;
    }
    if (collisionShapes.size() != startTransformList.size() ||
        collisionShapes.size() != startScaleList.size()) return MS::kFailure;
    btRigidParams rParam;
    DOMMY_RETURN_IF_FAILURE(evalParms(rParam));
    const btVector3 gravity = m_dynamicsWorld->getGravity();
    if (!dommy::bullet::safeMassGravity(rParam.m_mass,
            vec3f(gravity.x(), gravity.y(), gravity.z()))) return MS::kInvalidParameter;
    for (size_t i = 0; i < collisionShapes.size(); ++i) {
        btVector3 scale;
        if (!collisionShapes[i] || !collisionShapes[i]->shape() ||
            !GetBulletCollisionScale(rParam.m_collisionScale, startScaleList[i], scale))
            return MS::kInvalidParameter;
    }
    if (compoundCollisionLists.size() != compoundStartTransformList.size()) return MS::kFailure;
    for (const auto* shape : compoundCollisionLists) {
        if (!shape || !shape->shape() || !shape->shape()->isCompound()) return MS::kFailure;
    }
    detachFromWorld();
    m_worldReference = worldReference;
    

    
    //Create Single Rigid
    if ((collisionShapes.size() == startTransformList.size()) &&
        (collisionShapes.size() == startScaleList.size()) ) {
        
        
        MFnDependencyNode nodeFn(thisMObject());
        
        btTransform startTransform;
        startTransform.setIdentity();
        
        
        for (size_t i=0; i<startTransformList.size(); ++i) {
            
            btVector3 finScale, localInertia(0,0,0);
            if (!GetBulletCollisionScale(rParam.m_collisionScale, startScaleList[i], finScale) ||
                !collisionShapes[i] || !PrepareBulletCollisionShape(*collisionShapes[i], finScale,
                    rParam.m_collisionMargin, rParam.m_mass, localInertia))
                return MS::kInvalidParameter;

            std::unique_ptr<btMotionState> motionState(
                new btDefaultMotionState(startTransformList[i]));
            
            
            btRigidBody::btRigidBodyConstructionInfo rbInfo(btScalar(rParam.m_mass),
                                                            motionState.get(),
                                                            collisionShapes[i]->shape(),
                                                            localInertia);
            rbInfo.m_restitution = rParam.m_restitution ;
            rbInfo.m_friction = rParam.m_friction;
            rbInfo.m_rollingFriction = rParam.m_rollingFriction;
            rbInfo.m_linearDamping = rParam.m_linearDamping;
            rbInfo.m_angularDamping = rParam.m_angularDamping;
            std::unique_ptr<customDBData> userData(new customDBData());
            userData->nodeUuid = nodeFn.uuid();
            userData->prefixName = nodeFn.name().asChar();
            userData->index = static_cast<int>(collisionShapes[i]->itemIndex());
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();

            
            ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Dynamic, localInertia, true);
            
            
            
//            rb->setActivationState(ISLAND_SLEEPING);

            

            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
            
        }
        
        
        if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "Fin NumCollisionObjects:" << m_dynamicsWorld->getNumCollisionObjects());
        
        
    }
    
    
    preCollsionCount = collisionShapes.size();
    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "compoundCollisionLists:"<< compoundCollisionLists.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "compoundStartTransformList:"<< compoundStartTransformList.size());
    
    //Create Compound
    if ((compoundCollisionLists.size() == compoundStartTransformList.size())) {
        
        for (size_t i=0; i<compoundStartTransformList.size(); ++i) {
            
            btVector3 finScale, localInertia(0,0,0);
            if (!GetBulletCollisionScale(rParam.m_collisionScale, MVector(1,1,1), finScale) ||
                !compoundCollisionLists[i] || !PrepareBulletCollisionShape(*compoundCollisionLists[i], finScale,
                    rParam.m_collisionMargin, rParam.m_mass, localInertia))
                return MS::kInvalidParameter;

            std::unique_ptr<btMotionState> motionState(
                new btDefaultMotionState(compoundStartTransformList[i]));
            
            btRigidBody::btRigidBodyConstructionInfo rbInfo(btScalar(rParam.m_mass),
                                                            motionState.get(),
                                                            compoundCollisionLists[i]->shape(),
                                                            localInertia);
            
            rbInfo.m_restitution = rParam.m_restitution ;
            rbInfo.m_friction = rParam.m_friction;
            rbInfo.m_rollingFriction = rParam.m_rollingFriction;
            rbInfo.m_linearDamping = rParam.m_linearDamping;
            rbInfo.m_angularDamping = rParam.m_angularDamping;
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo);
            btRigidBody* const rb = body.get();
            
            
            ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Dynamic, localInertia, true);
            

            

            AddBulletRigidBody(*m_dynamicsWorld, compRbList, std::move(body));
        }
        
        
        if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "Fin compRbList:" << compRbList.size());
    }//Create Compound
    
    
    
    return MS::kSuccess;
}


MStatus DCbtRigid2D::resetRigidBodiesKinematic(
    btDiscreteDynamicsWorld* m_dynamicsWorld,
    const DBulletWorldReferencePtr& worldReference) {
    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "create collisionShapes:" << collisionShapes.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "startTransformList count:" << startTransformList.size());
    
    
    if (!m_dynamicsWorld || !worldReference ||
        worldReference->get() != m_dynamicsWorld) {
        return MS::kFailure;
    }
    if (collisionShapes.size() != startTransformList.size() ||
        collisionShapes.size() != startScaleList.size()) return MS::kFailure;
    if (!compoundCollisionLists.empty()) return MS::kInvalidParameter;
    btRigidParams rParam;
    DOMMY_RETURN_IF_FAILURE(evalParms(rParam));
    for (size_t i = 0; i < collisionShapes.size(); ++i) {
        btVector3 scale;
        if (!collisionShapes[i] || !collisionShapes[i]->shape() ||
            !GetBulletCollisionScale(rParam.m_collisionScale, startScaleList[i], scale))
            return MS::kInvalidParameter;
    }
    detachFromWorld();
    m_worldReference = worldReference;
    
    
    
    if (collisionShapes.size() == startTransformList.size()) {
        
        

        
        MFnDependencyNode nodeFn(thisMObject());
        
        btTransform startTransform;
        startTransform.setIdentity();
        
        
        for (size_t i=0; i<startTransformList.size(); ++i) {
            
            
            btVector3 finScale, localInertia(0,0,0);
            if (!GetBulletCollisionScale(rParam.m_collisionScale, startScaleList[i], finScale) ||
                !collisionShapes[i] || !PrepareBulletCollisionShape(*collisionShapes[i], finScale,
                    rParam.m_collisionMargin, 0.0f, localInertia))
                return MS::kInvalidParameter;

            std::unique_ptr<btMotionState> motionState(
                new btDefaultMotionState(startTransformList[i]));
            
            
            
            btRigidBody::btRigidBodyConstructionInfo rbInfo(btScalar(0.0),
                                                            motionState.get(),
                                                            collisionShapes[i]->shape(),
                                                            localInertia);
            rbInfo.m_restitution = rParam.m_restitution;
            rbInfo.m_friction = rParam.m_friction;
            rbInfo.m_rollingFriction = rParam.m_rollingFriction;
            rbInfo.m_linearDamping = rParam.m_linearDamping;
            rbInfo.m_angularDamping = rParam.m_angularDamping;
            std::unique_ptr<customDBData> userData(new customDBData());
            userData->nodeUuid = nodeFn.uuid();
            userData->prefixName = nodeFn.name().asChar();
            userData->index = static_cast<int>(collisionShapes[i]->itemIndex());
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();
            
            ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Kinematic, localInertia, true);
            

            
            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
        }
        
    }
    
    
    preCollsionCount = collisionShapes.size();
    
    
    return MS::kSuccess;
}


#pragma mark -
#pragma mark update

//update new mesh and attribute value update
MStatus DCbtRigid2D::updateRigidBodies(btDiscreteDynamicsWorld* m_dynamicsWorld, DBulletBodyMode mode) {
    if (!m_dynamicsWorld || ResolveBulletWorld(m_worldReference) != m_dynamicsWorld)
        return MS::kFailure;

    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "updateRigidBodies:");
    
    //update positions of all objects
    
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "preCollsionCount:" << preCollsionCount);
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "collisionShapes count:" << collisionShapes.size());
    if (collisionShapes.size() != startTransformList.size() ||
        collisionShapes.size() != startScaleList.size() ||
        preCollsionCount > collisionShapes.size()) return MS::kFailure;
    if (collisionShapes.size()>preCollsionCount) {
        if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "Exist new object.");
        
        
        btRigidParams rParam;
        DOMMY_RETURN_IF_FAILURE(evalParms(rParam));
        const float mass = mode == DBulletBodyMode::Kinematic ? 0.0f : rParam.m_mass;
        const btVector3 gravity = m_dynamicsWorld->getGravity();
        if (!dommy::bullet::safeMassGravity(mass,
                vec3f(gravity.x(), gravity.y(), gravity.z()))) return MS::kInvalidParameter;
        
        MFnDependencyNode nodeFn(thisMObject());
        
        btTransform startTransform;
        startTransform.setIdentity();
        
        
        for (size_t i=preCollsionCount; i<collisionShapes.size(); ++i) {
            
            btVector3 finScale, localInertia(0,0,0);
            if (!GetBulletCollisionScale(rParam.m_collisionScale, startScaleList[i], finScale) ||
                !collisionShapes[i] || !PrepareBulletCollisionShape(*collisionShapes[i], finScale,
                    rParam.m_collisionMargin, mass, localInertia))
                return MS::kInvalidParameter;

            std::unique_ptr<btMotionState> motionState(
                new btDefaultMotionState(startTransformList[i]));
            
            btRigidBody::btRigidBodyConstructionInfo rbInfo(btScalar(mass),
                                                            motionState.get(),
                                                            collisionShapes[i]->shape(),
                                                            localInertia);
            rbInfo.m_restitution = rParam.m_restitution ;
            rbInfo.m_friction = rParam.m_friction;
            rbInfo.m_rollingFriction = rParam.m_rollingFriction;
            rbInfo.m_linearDamping = rParam.m_linearDamping;
            rbInfo.m_angularDamping = rParam.m_angularDamping;
            std::unique_ptr<customDBData> userData(new customDBData());
            userData->nodeUuid = nodeFn.uuid();
            userData->prefixName = nodeFn.name().asChar();
            userData->index = static_cast<int>(collisionShapes[i]->itemIndex());
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();
            
            //            rb->setWorldTransform(startTransformList[i]);
            // static bodies may got false "impulse" when editing in Maya, so...
            //            rb->setInterpolationWorldTransform(startTransformList[i]);
            
            ConfigureBulletBodyMotion(*rb, rParam, mode, localInertia, true);
            
            


            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
        }
        
        
        
        preCollsionCount = collisionShapes.size();
    }
    return MS::kSuccess;
}


//massが0で元のモーションを利用するための読み込み
MStatus DCbtRigid2D::updateRigidBodiesKinematic(btDiscreteDynamicsWorld* m_dynamicsWorld, float time) {
    if (!m_dynamicsWorld || ResolveBulletWorld(m_worldReference) != m_dynamicsWorld)
        return MS::kFailure;

    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "updateRigidBodiesKinematic:");
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "collisionShapes count:" << collisionShapes.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "kinematicTransformList count:" << kinematicTransformList.size());
    if(DC_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DCbtRigid2D", "time:" << time);
    if (!std::isfinite(time) || time < 0.0f || time > 1.0f ||
        collisionShapes.size() != kinematicTransformList.size()) return MS::kInvalidParameter;
    if (collisionShapes.size()==kinematicTransformList.size()) {
        //        cout << "same count:"<< endl;
        MFnDependencyNode nodeFn(thisMObject());
        MUuid thisUuid = nodeFn.uuid();
        //        cout << "collision count:"<< m_dynamicsWorld->getNumCollisionObjects() << endl;
        
        
        MVector collisionScale;
        if (readPlugValue(MPlug(thisMObject(), aCollisionScale),
                          collisionScale) != MS::kSuccess) {
            return MS::kFailure;
        }
        
        for (int j=m_dynamicsWorld->getNumCollisionObjects()-1; j>=0 ;j--)
        {
            
            btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (!body) {
                continue;
            }
            customDBData* cd =
                static_cast<customDBData*>(body->getUserPointer());
            if(cd && cd->nodeUuid == thisUuid) {
                //                cout << "cd->index:"<< cd->index << endl;
                //                cout << "cd->prefixName:"<< cd->prefixName << endl;
                
                if(cd->index >= 0 &&
                   static_cast<size_t>(cd->index) <
                       kinematicTransformList.size()) {
                    //rigidbody for kimatic object
                    vec3f pos0;
                    quatf rot0;
                    getDBtTransformTR(body, pos0, rot0);
                    //                    cout << "pos0:" << pos0[0] << " " << pos0[1] << " " << pos0[2] << endl;
                    //                    cout << "rot0:" << rot0[0] << " " << rot0[1] << " " << rot0[2] << " " << rot0[3] << endl;
                    //transform for kimatic object
                    MTransformationMatrix transMat(kinematicTransformList[cd->index]);
                    //                    cout << "transMat:" << transMat.asMatrix() << endl;
                    
                    double quartx, quarty,quartz,quartw;
                    DOMMY_RETURN_IF_FAILURE(transMat.getRotationQuaternion(quartx, quarty, quartz, quartw));
                    MStatus status;
                    MVector mpos(transMat.getTranslation(MSpace::kTransform, &status));
                    DOMMY_RETURN_IF_FAILURE(status);
                    double scale[3];
                    DOMMY_RETURN_IF_FAILURE(transMat.getScale(scale, MSpace::kTransform));
                    for (int axis = 0; axis < 3; ++axis) {
                        if (!dommy::bullet::isSafeCollisionCoordinate(mpos[axis])) return MS::kInvalidParameter;
                    }
                    if (!std::isfinite(quartx) || !std::isfinite(quarty) ||
                        !std::isfinite(quartz) || !std::isfinite(quartw)) return MS::kInvalidParameter;
                    
                    
                    
                    
                    vec3f pos1 = vec3f((float)mpos.x, (float)mpos.y, (float)mpos.z);
                    quatf rot1 = quatf((float)quartw, (float)quartx, (float)quarty, (float)quartz);
                    //                    cout << "pos1:" << pos1[0] << " " << pos1[1] << " " << pos1[2] << endl;
                    //                    cout << "rot1:" << rot1[0] << " " << rot1[1] << " " << rot1[2] << " " << rot1[3] << endl;
                    
                    vec3f position;
                    quatf rotation;
                    if (!dommy::bullet::interpolatePassiveTransform(
                            pos0, pos1, rot0, rot1, time, position, rotation))
                        return MS::kInvalidParameter;
                    btVector3 finScale;
                    if (!GetBulletCollisionScale(collisionScale, MVector(scale[0],scale[1],scale[2]), finScale))
                        return MS::kInvalidParameter;
                    if (!body->getMotionState() || !body->getCollisionShape()) return MS::kFailure;
                    set_DB_RigidTransform(body, position, rotation);
                    body->getCollisionShape()->setLocalScaling(finScale);
                    m_dynamicsWorld->updateSingleAabb(body);
                    
                }
                
            }
        }
        
    }
    
    return MS::kSuccess;
}



MStatus DCbtRigid2D::updateCollisionShape(btDiscreteDynamicsWorld* world)
{
    if (!world || ResolveBulletWorld(m_worldReference) != world) return MS::kFailure;
    bool deform = false;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsCollisionDeform), deform));
    if (!deform) return MS::kSuccess;

    dommy::maya::ScopedPlugDataHandle input;
    DOMMY_RETURN_IF_FAILURE(input.read(MPlug(thisMObject(), inDC)));
    const DCurveData* curves = getInputDCfromHandle(input.handle());
    if (!curves || rbList.size() != collisionShapes.size() ||
        rbList.size() != startScaleList.size()) return MS::kFailure;

    double rawMass = 0;
    float mass, margin = 0;
    MVector requestedScale;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMass), rawMass));
    if (!dommy::numeric::checkedFloatCast(rawMass, mass) ||
        (rawMass != 0 && mass == 0)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionMargin), margin));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionScale), requestedScale));
    const MUuid uuid = MFnDependencyNode(thisMObject()).uuid();

    // These arrays contain only singles. Their slot is not an input curve ID
    // when preceding curves belong to a compound body.
    for (std::size_t slot = 0; slot < rbList.size(); ++slot) {
        btRigidBody* body = rbList[slot];
        db_collision_shape_base* shape = collisionShapes[slot];
        const customDBData* metadata = body ?
            static_cast<const customDBData*>(body->getUserPointer()) : nullptr;
        if (!shape || !metadata || metadata->nodeUuid != uuid || metadata->index < 0 ||
            std::size_t(metadata->index) != shape->itemIndex() ||
            shape->itemIndex() >= curves->numberOfCurves()) return MS::kInvalidParameter;
        ConstDCItem curve = curves->getCurvePtr(shape->itemIndex());
        if (!curve) return MS::kInvalidParameter;
        std::unique_ptr<db_collision_shape_base> replacement(create2DCollisionShape(&curve));
        if (!replacement) return MS::kFailure;
        btVector3 scale;
        if (!GetBulletCollisionScale(requestedScale, startScaleList[slot], scale) ||
            !ReplaceBulletCollisionShape(*world, *body, collisionShapes[slot],
                std::move(replacement), scale, margin, mass)) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}


std::vector<btRigidBody*>* DCbtRigid2D::getRigidBodyData() {
    return &rbList;
}


#pragma mark -
#pragma mark compute

void DCbtRigid2D::clearCollisionGeometry() noexcept
{
    detachFromWorld();
    ReleaseBulletCollisionShapes(collisionShapes);
    startTransformList.clear();
    startScaleList.clear();
    kinematicTransformList.clear();
    ReleaseBulletCollisionShapes(compoundCollisionLists);
    compoundIdLists.clear();
    compoundStartTransformList.clear();
    preCollsionCount = 0;
    m_geometryCount = 0;
    m_geometryGrouped = false;
    m_geometryGroupPattern.clear();
}

MStatus DCbtRigid2D::prepareCollisionGeometry(bool rebuild, DBulletBodyMode mode)
{
    dommy::maya::ScopedPlugDataHandle inputOwner;
    DOMMY_RETURN_IF_FAILURE(inputOwner.read(MPlug(thisMObject(), inDC)));
    const DCurveData* input = getInputDCfromHandle(inputOwner.handle());
    if (!input) return MS::kInvalidParameter;
    const std::size_t count = input->numberOfCurves();
    if (count > dommy::generation::kMaxGeneratedTransformCount) return MS::kInvalidParameter;
    const bool replace = rebuild || count == 0;
    bool grouped = false;
    MString pattern;
    if (count != 0) {
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsCompound), grouped));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCompoundGroupName), pattern));
    }
    std::string groupPattern = grouped ? pattern.asChar() : "";
    if (grouped && mode == DBulletBodyMode::Kinematic) {
        EchoWarning(name().asChar(), "Compound collision groups are not supported for kinematic input.");
        return MS::kInvalidParameter;
    }
    if (!replace && (count < m_geometryCount ||
            (m_geometryGrouped && count != m_geometryCount) ||
            grouped != m_geometryGrouped || groupPattern != m_geometryGroupPattern)) {
        EchoWarning(name().asChar(), "Collision geometry configuration changed; return to StartTime.");
        return MS::kFailure;
    }
    std::vector<DPIndexList> groups;
    if (replace && grouped && !groupPattern.empty()) {
        std::vector<std::string> names;
        getDCurveMatchedGroups(*input, groupPattern, names);
        for (const std::string& groupName : names) {
            const auto group = input->getGrpPtr(groupName);
            if (!group) return MS::kInvalidParameter;
            DPIndexList ids;
            group->getActiveIndexList(ids);
            if (!ids.empty()) groups.push_back(std::move(ids));
        }
    }
    MStatus status;
    MFnDependencyNode node(thisMObject(), &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MUuid uuid = node.uuid(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString nodeName = node.name(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    DBulletGeometryCandidate candidate;
    DOMMY_RETURN_IF_FAILURE(PrepareBulletGeometryCandidate(count, replace ? 0 : m_geometryCount,
        groups, [&](std::size_t index) { return input->getCurvePtr(index); },
        [&](ConstDCItem item) {
            return create2DCollisionShape(&item);
        }, uuid, nodeName.asChar(), candidate));
    ReserveBulletGeometry(candidate, replace, collisionShapes, startTransformList, startScaleList);
    compoundCollisionLists.reserve((replace ? 0 : compoundCollisionLists.size()) + candidate.compound.size());
    if (replace) {
        // Empty input releases native bodies too, while keeping the solver
        // association so subsequent empty update callbacks remain valid.
        const auto reference = m_worldReference;
        clearCollisionGeometry();
        m_worldReference = reference;
    }
    AppendBulletGeometry(candidate, collisionShapes, startTransformList, startScaleList);
    kinematicTransformList.swap(candidate.currentTransforms);
    if (replace) {
        compoundIdLists.swap(candidate.groups);
        compoundStartTransformList.swap(candidate.compoundTransforms);
        for (auto& shape : candidate.compound) compoundCollisionLists.push_back(shape.release());
    }
    m_geometryGrouped = grouped;
    m_geometryGroupPattern.swap(groupPattern);
    m_geometryCount = count;
    return MS::kSuccess;
}

MStatus DCbtRigid2D::compute(const MPlug& plug, MDataBlock& data) try
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
DOMMY_COMPUTE_CATCH("DCbtRigid2D")






MStatus DCbtRigid2D::initialize() try {
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
    
    
    
    aExternalForce = nAttr.create("ExternalForce", "exfo", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExternalForce));
    
    aExternalTorque = nAttr.create("ExternalTorque", "exto", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExternalTorque));
    
#pragma mark compound attribute
    
    aIsCompound = nAttr.create("IsCompound", "iscmpd", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCompound));
    
    
    const MObject aCompoundGroupNameDefaultData = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCompoundGroupName = tAttr.create("CompoundGroupName", "cmpgrpnm", MFnData::kString, aCompoundGroupNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCompoundGroupName));
    
#pragma mark collision shape attribute
    
    
    
    aCollisionScale = nAttr.create("CollisionScale", "csc", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionScale));
    
    
    aCollisionMargin = nAttr.create("CollisionMargin", "colm", MFnNumericData::kFloat, 0.02, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollisionMargin));
    
    aIsCollisionDeform = nAttr.create("IsCollisionDeform", "iscoldef", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCollisionDeform));
    
    
    aStartSleeping = nAttr.create("StartSleeping", "statslp", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartSleeping));
    
    
    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMass, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRestitution, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFriction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRollingFriction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLinearDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularDamping, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialPosition, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialRotation, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialVelocity, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInitialSpin, outDC ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExternalForce, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExternalTorque, outDC ));
    
    //cluster
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCompound, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCompoundGroupName, outDC ));
    
//    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionScale, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionMargin, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCollisionDeform, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStartSleeping, outDC ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCbtRigid2D", "initialize", std::current_exception());
}
