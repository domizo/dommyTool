//
//  DMbtRigid.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/03.
//

#include "DMbtRigid.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "DBulletGeometryPreparation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DMeshDataHeaders.h"
#include "DDataOp.h"
#include "DNodeOperation.h"


//bullet
#include "DBulletDataHeaders.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"

#include "mathUtils.h"

#include "commonDBullet.h"
#include "Solver/DPassiveTransformMath.h"
#include "commonDBulletCollision.h"

#include "db_collision_base_shapes.h"
#include "db_collision_mesh_shapes.h"


#define DM_BULLET_RIGID_DEBUG DOMMY_ENABLE_DEBUG_LOGS


const MTypeId DMbtRigid::typeId( ID_DM_BT_RIGID );

MObject DMbtRigid::inDM;

MObject DMbtRigid::aMass;
MObject DMbtRigid::aRestitution;//反発
MObject DMbtRigid::aFriction;//摩擦
MObject DMbtRigid::aRollingFriction;//回転摩擦
MObject DMbtRigid::aLinearDamping;//減衰
MObject DMbtRigid::aAngularDamping;//回転減衰

MObject DMbtRigid::aInitialPosition;
MObject DMbtRigid::aInitialRotation;
MObject DMbtRigid::aInitialVelocity;
MObject DMbtRigid::aInitialSpin;

MObject DMbtRigid::aExternalForce;
MObject DMbtRigid::aExternalTorque;

MObject DMbtRigid::aCollisionType;
MObject DMbtRigid::aCollisionScale;
MObject DMbtRigid::aCollisionMargin;
MObject DMbtRigid::aIsCollisionDeform;

MObject DMbtRigid::outDM;


DMbtRigid::~DMbtRigid()
{
    clearCollisionGeometry();
}


void DMbtRigid::detachFromWorld() noexcept
{
    DetachAndReleaseBulletRigidBodies(
        m_worldReference.lock(), rbList);
    m_worldReference.reset();
}



void *DMbtRigid::creator() {
    return ::dommy::maya::createObject("DMbtRigid", [&]() {
        return new DMbtRigid();
    });
}




#pragma mark -
#pragma mark parameter

MStatus DMbtRigid::evalParms(btRigidParams& parms) {
    
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
    
    
    return ValidateBulletRigidParameters(parms) ? MS::kSuccess : MS::kInvalidParameter;
}




#pragma mark -
#pragma mark init

void DMbtRigid::clearRigidBodies() {
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "DMbtRigid::clearRigidBodies");
    
    
    
}


MStatus DMbtRigid::resetRigidBodies(
    btSoftRigidDynamicsWorld* m_dynamicsWorld,
    const DBulletWorldReferencePtr& worldReference) {
    
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "create collisionShapes:" << collisionShapes.size());
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "startTransformList count:" << startTransformList.size());
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "startScaleList count:" << startScaleList.size());
    
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "delete rigid body:" << collisionShapes.size());
    
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
    detachFromWorld();
    m_worldReference = worldReference;
    
    
    
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
            
//            vec3f  inertia = collisionShapes[i]->local_inertia();
//            btVector3 inertiaVec(inertia[0],inertia[1],inertia[2]);
            
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
            userData->index = i;
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();

//            rb->setWorldTransform(startTransformList[i]);
            // static bodies may got false "impulse" when editing in Maya, so...
//            rb->setInterpolationWorldTransform(startTransformList[i]);
            
            ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Dynamic, localInertia, false);
            
            
            
            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
            
        }
        
        
        if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "Fin NumCollisionObjects:" << m_dynamicsWorld->getNumCollisionObjects());
        
        
    }
    
    
    preCollsionCount = collisionShapes.size();
    
    
    return MS::kSuccess;
}


MStatus DMbtRigid::resetRigidBodiesKinematic(
    btSoftRigidDynamicsWorld* m_dynamicsWorld,
    const DBulletWorldReferencePtr& worldReference) {
    
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "create collisionShapes:" << collisionShapes.size());
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "startTransformList count:" << startTransformList.size());
    
    
    if (!m_dynamicsWorld || !worldReference ||
        worldReference->get() != m_dynamicsWorld) {
        return MS::kFailure;
    }
    if (collisionShapes.size() != startTransformList.size() ||
        collisionShapes.size() != startScaleList.size()) return MS::kFailure;
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
            userData->index = i;
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();
            
            ConfigureBulletBodyMotion(*rb, rParam, DBulletBodyMode::Kinematic, localInertia, false);
            
            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
        }
        
    }
    
    
    preCollsionCount = collisionShapes.size();
    
    
    return MS::kSuccess;
}


#pragma mark -
#pragma mark update

//update new mesh and attribute value update
MStatus DMbtRigid::updateRigidBodies(btSoftRigidDynamicsWorld* m_dynamicsWorld, DBulletBodyMode mode) {
    if (!m_dynamicsWorld || ResolveBulletWorld(m_worldReference) != m_dynamicsWorld)
        return MS::kFailure;

    
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "updateRigidBodies:");
    
    //update positions of all objects
    
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "preCollsionCount:" << preCollsionCount);
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "collisionShapes count:" << collisionShapes.size());
    if (collisionShapes.size() != startTransformList.size() ||
        collisionShapes.size() != startScaleList.size() ||
        preCollsionCount > collisionShapes.size()) return MS::kFailure;
    if (collisionShapes.size()>preCollsionCount) {
        if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "Exist new object.");
        
        
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
            userData->index = i;
            DBulletRigidBodyPtr body = MakeBulletRigidBody(
                std::move(motionState), rbInfo, std::move(userData));
            btRigidBody* const rb = body.get();
            
            //            rb->setWorldTransform(startTransformList[i]);
            // static bodies may got false "impulse" when editing in Maya, so...
            //            rb->setInterpolationWorldTransform(startTransformList[i]);
            
            ConfigureBulletBodyMotion(*rb, rParam, mode, localInertia, false);
            
            
            AddBulletRigidBody(*m_dynamicsWorld, rbList, std::move(body));
        }
        
        
        
        preCollsionCount = collisionShapes.size();
    }
    return MS::kSuccess;
}


//massが0で元のモーションを利用するための読み込み
MStatus DMbtRigid::updateRigidBodiesKinematic(btSoftRigidDynamicsWorld* m_dynamicsWorld, float time) {
    if (!m_dynamicsWorld || ResolveBulletWorld(m_worldReference) != m_dynamicsWorld)
        return MS::kFailure;

    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "updateRigidBodiesKinematic:");
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "collisionShapes count:" << collisionShapes.size());
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "kinematicTransformList count:" << kinematicTransformList.size());
    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "time:" << time);
    if (!std::isfinite(time) || time < 0.0f || time > 1.0f ||
        collisionShapes.size() != kinematicTransformList.size()) return MS::kInvalidParameter;
    if (collisionShapes.size()==kinematicTransformList.size()) {
        if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "same count:");
        MFnDependencyNode nodeFn(thisMObject());
        MUuid thisUuid = nodeFn.uuid();
        if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "collision count:"<< m_dynamicsWorld->getNumCollisionObjects());
        
        
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
                if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "cd->index:"<< cd->index);
                if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "cd->prefixName:"<< cd->prefixName);
                
                if(cd->index >= 0 &&
                   static_cast<size_t>(cd->index) <
                       kinematicTransformList.size()) {
                    //rigidbody for kimatic object
                    vec3f pos0;
                    quatf rot0;
                    getDBtTransformTR(body, pos0, rot0);
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "pos0:" << pos0[0] << " " << pos0[1] << " " << pos0[2]);
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "rot0:" << rot0[0] << " " << rot0[1] << " " << rot0[2] << " " << rot0[3]);
                    //transform for kimatic object
                    MTransformationMatrix transMat(kinematicTransformList[cd->index]);
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "transMat:" << transMat.asMatrix());
                    
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
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "pos1:" << pos1[0] << " " << pos1[1] << " " << pos1[2]);
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "rot1:" << rot1[0] << " " << rot1[1] << " " << rot1[2] << " " << rot1[3]);
                    
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
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "set_DB_RigidTransform end");
                    
                    
                    body->getCollisionShape()->setLocalScaling(finScale);
                    m_dynamicsWorld->updateSingleAabb(body);
                    if(DM_BULLET_RIGID_DEBUG) DOMMY_MAYA_DEBUG("DMbtRigid", "setLocalScaling end");
                }
                
            }
        }
        
    }
    
    return MS::kSuccess;
}



MStatus DMbtRigid::updateCollisionShape(btSoftRigidDynamicsWorld* m_dynamicsWorld) {
    if (!m_dynamicsWorld || ResolveBulletWorld(m_worldReference) != m_dynamicsWorld)
        return MS::kFailure;

    
    bool isDeform = false;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aIsCollisionDeform), isDeform));
    
    if (!isDeform) {
        return MS::kSuccess;
    }
    
    
    
    MPlug dmPlug(thisMObject(), inDM);
    dommy::maya::ScopedPlugDataHandle dMeshHndOwner;
    MDataHandle& dMeshHnd = dMeshHndOwner.handle();
    if (dMeshHndOwner.read(dmPlug) != MS::kSuccess) {
        return MS::kFailure;
    }
    
    const DMeshData* const dMeshes(getInputDMfromHandle(dMeshHnd));
    if (!dMeshes) return MS::kFailure;
    
    if(dMeshes && dMeshes->numberOfMesh()) {
        
        MFnDependencyNode nodeFn(thisMObject());
        MUuid thisUuid = nodeFn.uuid();
        DBulletCollisionType colType;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionType), colType));
        
        float m_mass = 0.0f, m_collisionMargin = 0.0f;
        double rawMass = 0.0;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aMass), rawMass));
        if (!dommy::numeric::checkedFloatCast(rawMass, m_mass) ||
            (rawMass != 0 && m_mass == 0)) return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aCollisionMargin), m_collisionMargin));
        MVector m_collisionScale;
        if (readPlugValue(MPlug(thisMObject(), aCollisionScale),
                          m_collisionScale) != MS::kSuccess) {
            return MS::kFailure;
        }
        
        
        size_t meshCount =  dMeshes->numberOfMesh();
        
        for (int j=m_dynamicsWorld->getNumCollisionObjects()-1; j>=0 ;j--)
        {
            
            btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (!body) {
                continue;
            }
            customDBData* cd =
                static_cast<customDBData*>(body->getUserPointer());
            if(cd && cd->nodeUuid == thisUuid && cd->index >= 0 &&
               static_cast<size_t>(cd->index) < meshCount) {
                
                
                std::unique_ptr<db_collision_shape_base> colShape;
                
                switch (colType) {//meshのみ
                    case D_COL_CONVEX_HULL:
                    case D_COL_MESH:
                    case D_COL_BVH_MESH:
                    case D_COL_HACD:
                        {
                            ConstDMItem mIt =
                                dMeshes->getMeshPtrConst(cd->index);
                            colShape.reset(
                                createCollisionShape(colType, &mIt));
                        }
                        break;
                    default:
                        continue;
                        break;
                }
                
                
                if (!colShape) return MS::kFailure;
                if (colShape) {
                
                    
                    
                    const size_t index = static_cast<size_t>(cd->index);
                    if (index >= collisionShapes.size() || index >= startScaleList.size())
                        return MS::kFailure;
                    btVector3 scale;
                    if (!GetBulletCollisionScale(m_collisionScale, startScaleList[index], scale) ||
                        !ReplaceBulletCollisionShape(*m_dynamicsWorld, *body, collisionShapes[index],
                            std::move(colShape), scale, m_collisionMargin, m_mass))
                        return MS::kInvalidParameter;
	                }
                
                
            }
            
        }
    }
    
    return MS::kSuccess;
}


std::vector<btRigidBody*>* DMbtRigid::getRigidBodyData() {
    return &rbList;
}

#pragma mark -
#pragma mark compute

void DMbtRigid::clearCollisionGeometry() noexcept
{
    detachFromWorld();
    ReleaseBulletCollisionShapes(collisionShapes);
    startTransformList.clear();
    startScaleList.clear();
    kinematicTransformList.clear();
    preCollsionCount = 0;
    m_geometryCount = 0;
}

MStatus DMbtRigid::prepareCollisionGeometry(bool rebuild)
{
    dommy::maya::ScopedPlugDataHandle inputOwner;
    DOMMY_RETURN_IF_FAILURE(inputOwner.read(MPlug(thisMObject(), inDM)));
    const DMeshData* input = getInputDMfromHandle(inputOwner.handle());
    if (!input || !input->hasValidMeshTopology()) return MS::kInvalidParameter;
    const std::size_t count = input->numberOfMesh();
    if (count > dommy::generation::kMaxGeneratedTransformCount) return MS::kInvalidParameter;
    const bool replace = rebuild || count == 0;
    if (!replace && count < m_geometryCount) {
        EchoWarning(name().asChar(), "Collision geometry shrank; return to StartTime.");
        return MS::kFailure;
    }
    const std::vector<DPIndexList> groups;
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
    ReserveBulletGeometry(candidate, replace, collisionShapes, startTransformList, startScaleList);
    if (replace) {
        // Empty input releases native bodies too, while keeping the solver
        // association so subsequent empty update callbacks remain valid.
        const auto reference = m_worldReference;
        clearCollisionGeometry();
        m_worldReference = reference;
    }
    AppendBulletGeometry(candidate, collisionShapes, startTransformList, startScaleList);
    kinematicTransformList.swap(candidate.currentTransforms);
    m_geometryCount = count;
    return MS::kSuccess;
}

MStatus DMbtRigid::compute(const MPlug& plug, MDataBlock& data) try
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
DOMMY_COMPUTE_CATCH("DMbtRigid")






MStatus DMbtRigid::initialize() try {
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
    
    
    
#pragma mark collision shape attribute
    
    MFnEnumAttribute colEnumAttr;
    aCollisionType = colEnumAttr.create("CollisionType", "ctp", D_COL_CONVEX_HULL, &stat);
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
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    
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
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExternalForce, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExternalTorque, outDM ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCollisionMargin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCollisionDeform, outDM ));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMbtRigid", "initialize", std::current_exception());
}
