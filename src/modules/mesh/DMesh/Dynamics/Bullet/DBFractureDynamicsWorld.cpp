// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
The files in this repository are licensed under the zlib license, except for the files under 'Extras' and examples/ThirdPartyLibs.

Bullet Continuous Collision Detection and Physics Library
http://bulletphysics.org

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/FractureDemo/btFractureDynamicsWorld.cpp

//
//  DBFractureDynamicsWorld.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//

#include "DBFractureDynamicsWorld.h"




#include "dbtFractureBody.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"

#include "BulletCollision/CollisionDispatch/btUnionFind.h"
#include "BulletDynamics/ConstraintSolver/btTypedConstraint.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

dbtFractureBody* fractureBodyFrom(btCollisionObject* object) noexcept
{
    if (!object ||
        !(object->getInternalType() & CUSTOM_FRACTURE_TYPE)) {
        return nullptr;
    }
    return static_cast<dbtFractureBody*>(object);
}

btCompoundShape* compoundShapeFrom(btCollisionShape* shape) noexcept
{
    return shape && shape->isCompound()
        ? static_cast<btCompoundShape*>(shape)
        : nullptr;
}

btVector3 spinAngularMomentum(const btRigidBody& body)
{
    // Rotate principal-axis inertia, rather than inverting a possibly stale
    // cached world tensor (or a singular tensor for locked axes).
    const btMatrix3x3& basis = body.getWorldTransform().getBasis();
    const btVector3 localSpin = basis.transpose() * body.getAngularVelocity();
    const btVector3 momentum = basis * (body.getLocalInertia() * localSpin);
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(momentum[axis]))
            throw std::invalid_argument("Unsafe fracture angular momentum.");
    }
    return momentum;
}

} // namespace

DBFractureDynamicsWorld::DBFractureDynamicsWorld ( btDispatcher* dispatcher,btBroadphaseInterface* pairCache,btConstraintSolver* constraintSolver,btCollisionConfiguration* collisionConfiguration)
:btDiscreteDynamicsWorld(dispatcher,pairCache,constraintSolver,collisionConfiguration),
m_fracturingMode(true)
{
    
}

DBFractureDynamicsWorld::~DBFractureDynamicsWorld()
{
    for (int index = m_ownedGeneratedCollisionShapes.size() - 1; index >= 0; --index) {
        delete m_ownedGeneratedCollisionShapes[index];
    }
    m_ownedGeneratedCollisionShapes.clear();
}

void DBFractureDynamicsWorld::ownGeneratedCollisionShape(btCollisionShape* shape)
{
    if (shape && m_ownedGeneratedCollisionShapes.findLinearSearch(shape) ==
                     m_ownedGeneratedCollisionShapes.size()) {
        m_ownedGeneratedCollisionShapes.push_back(shape);
    }
}

void DBFractureDynamicsWorld::releaseGeneratedCollisionShape(btCollisionShape* shape)
{
    const int index = m_ownedGeneratedCollisionShapes.findLinearSearch(shape);
    if (index < m_ownedGeneratedCollisionShapes.size()) {
        m_ownedGeneratedCollisionShapes.remove(shape);
        delete shape;
    }
}

dbtFractureBody* DBFractureDynamicsWorld::adoptGeneratedBody(
    std::unique_ptr<dbtFractureBody> body, std::unique_ptr<btCompoundShape> shape)
{
    ownGeneratedCollisionShape(shape.get());
    btCompoundShape* const generated = shape.release();
    try {
        addRigidBody(body.get());
    } catch (...) {
        body.reset();
        releaseGeneratedCollisionShape(generated);
        throw;
    }
    return body.release();
}


void DBFractureDynamicsWorld::glueCallback()
{
    
    int numManifolds = getDispatcher()->getNumManifolds();
    
    ///first build the islands based on axis aligned bounding box overlap
    
    btUnionFind unionFind;
    
    int index = 0;
    {
        
        int i;
        for (i=0;i<getCollisionObjectArray().size(); i++)
        {
            btCollisionObject*   collisionObject= getCollisionObjectArray()[i];
            //    btRigidBody* body = btRigidBody::upcast(collisionObject);
            //Adding filtering here
#ifdef STATIC_SIMULATION_ISLAND_OPTIMIZATION
            if (!collisionObject->isStaticOrKinematicObject())
            {
                collisionObject->setIslandTag(index++);
            } else
            {
                collisionObject->setIslandTag(-1);
            }
#else
            collisionObject->setIslandTag(i);
            index=i+1;
#endif
        }
    }
    
    unionFind.reset(index);
    
    for (int i=0;i<numManifolds;i++)
    {
        btPersistentManifold* manifold = getDispatcher()->getManifoldByIndexInternal(i);
        if (!manifold->getNumContacts())
            continue;
        
        btScalar minDist = 1e30f;
        for (int v=0;v<manifold->getNumContacts();v++)
        {
            minDist = btMin(minDist,manifold->getContactPoint(v).getDistance());
        }
        if (minDist>0.)
            continue;
        
        const btCollisionObject* colObj0 =
            static_cast<const btCollisionObject*>(manifold->getBody0());
        const btCollisionObject* colObj1 =
            static_cast<const btCollisionObject*>(manifold->getBody1());
        if (!colObj0 || !colObj1) {
            continue;
        }
        int tag0 = (colObj0)->getIslandTag();
        int tag1 = (colObj1)->getIslandTag();
        //btRigidBody* body0 = btRigidBody::upcast(colObj0);
        //btRigidBody* body1 = btRigidBody::upcast(colObj1);
        
        
        if (!colObj0->isStaticOrKinematicObject() && !colObj1->isStaticOrKinematicObject())
        {
            unionFind.unite(tag0, tag1);
        }
    }
    
    
    
    
    const int numElem = unionFind.getNumElements();
    
    
    
    index=0;
    for (int ai=0;ai<getCollisionObjectArray().size();ai++)
    {
        btCollisionObject* collisionObject= getCollisionObjectArray()[ai];
        if (!collisionObject->isStaticOrKinematicObject())
        {
            int tag = unionFind.find(index);
            
            collisionObject->setIslandTag( tag);
            
            //Set the correct object offset in Collision Object Array
#if STATIC_SIMULATION_ISLAND_OPTIMIZATION
            unionFind.getElement(index).m_sz = ai;
#endif //STATIC_SIMULATION_ISLAND_OPTIMIZATION
            
            index++;
        }
    }
    unionFind.sortIslands();
    
    
    
    int endIslandIndex=1;
    int startIslandIndex;
    
    btAlignedObjectArray<btCollisionObject*> removedObjects;
    
    ///iterate over all islands
    for ( startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
    {
        int islandId = unionFind.getElement(startIslandIndex).m_id;
        for (endIslandIndex = startIslandIndex+1;(endIslandIndex<numElem) && (unionFind.getElement(endIslandIndex).m_id == islandId);endIslandIndex++)
        {
        }
        
        int fractureObjectIndex = -1;
        
        int numObjects=0;
        
        int idx;
        for (idx=startIslandIndex;idx<endIslandIndex;idx++)
        {
            int i = unionFind.getElement(idx).m_sz;
            btCollisionObject* colObj0 = getCollisionObjectArray()[i];
            if (colObj0->getInternalType()& CUSTOM_FRACTURE_TYPE)
            {
                fractureObjectIndex = i;
            }
            btRigidBody* otherObject = btRigidBody::upcast(colObj0);
            if (!otherObject || !otherObject->getInvMass())
                continue;
            numObjects++;
        }
        
        ///Then for each island that contains at least two objects and one fracture object
        if (fractureObjectIndex>=0 && numObjects>1)
        {
            
            dbtFractureBody* fracObj = fractureBodyFrom(
                getCollisionObjectArray()[fractureObjectIndex]);
            if (!fracObj || fracObj->m_masses.size() == 0 ||
                fracObj->getInvMass() <= 0 ||
                !dommy::bullet::isSafeInverseOperand(fracObj->getInvMass())) {
                continue;
            }

            btCompoundShape* fractureCompound = nullptr;
            if (fracObj->getCollisionShape()->isCompound()) {
                fractureCompound =
                    compoundShapeFrom(fracObj->getCollisionShape());
                if (!fractureCompound ||
                    fracObj->m_masses.size() !=
                        fractureCompound->getNumChildShapes()) {
                    continue;
                }
            } else if (fracObj->m_masses.size() != 1) {
                throw std::invalid_argument("Fracture shape and mass counts disagree.");
            }
            
            ///glueing objects means creating a new compound and removing the old objects
            ///delay the removal of old objects to avoid array indexing problems
            removedObjects.push_back(fracObj);
            
            btAlignedObjectArray<btScalar> massArray;
            
            btAlignedObjectArray<btVector3> oldImpulses;
            btAlignedObjectArray<btVector3> oldAngularMomenta;
            btAlignedObjectArray<btVector3> oldCenterOfMassesWS;
            
            oldImpulses.push_back(fracObj->getLinearVelocity()/1./fracObj->getInvMass());
            oldAngularMomenta.push_back(spinAngularMomentum(*fracObj));
            oldCenterOfMassesWS.push_back(fracObj->getCenterOfMassPosition());
            
            btScalar totalMass = 1.f / fracObj->getInvMass();
            
            
            std::unique_ptr<btCompoundShape> compound(new btCompoundShape());
            if (fracObj->getCollisionShape()->isCompound())
            {
                btTransform tr;
                tr.setIdentity();
                btCompoundShape* oldCompound = fractureCompound;
                for (int c=0;c<oldCompound->getNumChildShapes();c++)
                {
                    compound->addChildShape(oldCompound->getChildTransform(c),oldCompound->getChildShape(c));
                    massArray.push_back(fracObj->m_masses[c]);
                }
                
            } else
            {
                btTransform tr;
                tr.setIdentity();
                compound->addChildShape(tr,fracObj->getCollisionShape());
                massArray.push_back(fracObj->m_masses[0]);
            }
            
            for (idx=startIslandIndex;idx<endIslandIndex;idx++)
            {
                
                int i = unionFind.getElement(idx).m_sz;
                
                if (i==fractureObjectIndex)
                    continue;
                
                btCollisionObject* otherCollider = getCollisionObjectArray()[i];
                
                btRigidBody* otherObject = btRigidBody::upcast(otherCollider);
                //don't glue/merge with static objects right now, otherwise everything gets stuck to the ground
                ///todo: expose this as a callback
                if (!otherObject || !otherObject->getInvMass())
                    continue;
                if (!dommy::bullet::isSafeInverseOperand(otherObject->getInvMass()))
                    throw std::invalid_argument("Unsafe fracture source mass.");

                btCompoundShape* otherCompound = nullptr;
                dbtFractureBody* otherFracture = fractureBodyFrom(otherObject);
                if (otherObject->getCollisionShape()->isCompound()) {
                    otherCompound =
                        compoundShapeFrom(otherObject->getCollisionShape());
                    if (!otherCompound ||
                        otherCompound->getNumChildShapes() == 0 ||
                        (otherFracture && otherFracture->m_masses.size() !=
                            otherCompound->getNumChildShapes())) {
                        continue;
                    }
                }
                
                
                oldImpulses.push_back(otherObject->getLinearVelocity()*(1.f/otherObject->getInvMass()));
                oldAngularMomenta.push_back(spinAngularMomentum(*otherObject));
                oldCenterOfMassesWS.push_back(otherObject->getCenterOfMassPosition());
                
                removedObjects.push_back(otherObject);
                
                btScalar curMass = 1.f/otherObject->getInvMass();
                
                
                if (otherObject->getCollisionShape()->isCompound())
                {
                    btTransform tr;
                    btCompoundShape* oldCompound = otherCompound;
                    for (int c=0;c<oldCompound->getNumChildShapes();c++)
                    {
                        tr = fracObj->getWorldTransform().inverseTimes(otherObject->getWorldTransform()*oldCompound->getChildTransform(c));
                        compound->addChildShape(tr,oldCompound->getChildShape(c));
                        // Fracture compounds carry real per-child masses. Only
                        // ordinary Bullet compounds require the uniform fallback.
                        massArray.push_back(otherFracture ? otherFracture->m_masses[c] :
                            curMass/(btScalar)oldCompound->getNumChildShapes());
                        
                    }
                } else
                {
                    btTransform tr;
                    tr = fracObj->getWorldTransform().inverseTimes(otherObject->getWorldTransform());
                    compound->addChildShape(tr,otherObject->getCollisionShape());
                    massArray.push_back(curMass);
                }
                totalMass+=curMass;
            }
            
            
            
            btTransform shift;
            shift.setIdentity();
            btVector3 localInertia;
            btScalar childMass = 0;
            for (int i = 0; i < massArray.size(); ++i) childMass += massArray[i];
            if (!dommy::bullet::isSafeInverseOperand(totalMass) || totalMass <= 0 ||
                !dommy::bullet::isSafeInverseOperand(childMass) ||
                btFabs(childMass - totalMass) > totalMass * btScalar(1e-4)) {
                throw std::invalid_argument("Fracture child masses disagree with body mass.");
            }
            totalMass = childMass;
            std::unique_ptr<btCompoundShape> newCompound(
                dbtFractureBody::shiftTransform(compound.get(), &massArray[0], shift, localInertia));
            if (newCompound.get() == compound.get()) {
                compound.release();
            }
            int numChildren = newCompound->getNumChildShapes();
            if (numChildren != massArray.size() || numChildren == 0) {
                throw std::runtime_error(
                    "Fracture compound and mass counts do not match.");
            }
            
            std::unique_ptr<dbtFractureBody> newBody(new dbtFractureBody(
                totalMass,0,newCompound.get(),localInertia, &massArray[0], numChildren,this));
            newBody->recomputeConnectivity(this);
            newBody->setCenterOfMassTransform(fracObj->getWorldTransform()*shift);

            btVector3 linearMomentum(0, 0, 0), angularMomentum(0, 0, 0);
            for (int i=0;i<oldImpulses.size();i++)
            {
                btVector3 rel_pos = oldCenterOfMassesWS[i]-newBody->getCenterOfMassPosition();
                const btVector3& imp = oldImpulses[i];
                linearMomentum += imp;
                angularMomentum += oldAngularMomenta[i] + rel_pos.cross(imp);
            }
            const btVector3 linearVelocity = linearMomentum * newBody->getInvMass();
            const btVector3 angularVelocity = newBody->getInvInertiaTensorWorld() * angularMomentum;
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(linearVelocity[axis]) || !std::isfinite(angularVelocity[axis]))
                    throw std::invalid_argument("Unsafe glued-body velocity.");
            }
            newBody->setLinearVelocity(linearVelocity);
            newBody->setAngularVelocity(angularVelocity);
            newBody->setInterpolationLinearVelocity(linearVelocity);
            newBody->setInterpolationAngularVelocity(angularVelocity);

            adoptGeneratedBody(std::move(newBody), std::move(newCompound));
            
            
        }
        
        
    }
    
    //remove the objects from the world at the very end,
    //otherwise the island tags would not match the world collision object array indices anymore
    while (removedObjects.size())
    {
        btCollisionObject* otherCollider = removedObjects[removedObjects.size()-1];
        removedObjects.pop_back();
        
        btRigidBody* otherObject = btRigidBody::upcast(otherCollider);
        if (!otherObject || !otherObject->getInvMass())
            continue;
        destroyRigidBody(otherObject);
    }
    
}


struct    btFracturePair
{
    dbtFractureBody* m_fracObj;
    btAlignedObjectArray<btPersistentManifold*>    m_contactManifolds;
};



void DBFractureDynamicsWorld::solveConstraints(btContactSolverInfo& solverInfo)
{
    // todo: after fracture we should run the solver again for better realism
    // for example
    //    save all velocities and if one or more objects fracture:
    //    1) revert all velocties
    //    2) apply impulses for the fracture bodies at the contact locations
    //    3)and run the constaint solver again
    
    btDiscreteDynamicsWorld::solveConstraints(solverInfo);
    
    fractureCallback();
}

dbtFractureBody* DBFractureDynamicsWorld::addNewBody(
    const btTransform& oldTransform, btScalar* masses,
    std::unique_ptr<btCompoundShape> oldCompound)
{
    if (!masses || !oldCompound ||
        oldCompound->getNumChildShapes() == 0) {
        throw std::invalid_argument(
            "Cannot create a fracture body without shapes and masses.");
    }
    int i;
    
    btTransform shift;
    shift.setIdentity();
    btVector3 localInertia;
    std::unique_ptr<btCompoundShape> newCompound(
        dbtFractureBody::shiftTransform(oldCompound.get(), masses, shift, localInertia));
    if (newCompound.get() == oldCompound.get()) {
        oldCompound.release();
    }
    btScalar totalMass = 0;
    for (i=0;i<newCompound->getNumChildShapes();i++)
        totalMass += masses[i];
    std::unique_ptr<dbtFractureBody> newBody(new dbtFractureBody(
        totalMass,0,newCompound.get(),localInertia, masses,newCompound->getNumChildShapes(), this));
    newBody->recomputeConnectivity(this);
    
    newBody->setCollisionFlags(newBody->getCollisionFlags()|btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    newBody->setCenterOfMassTransform(oldTransform*shift);
    return adoptGeneratedBody(std::move(newBody), std::move(newCompound));
}

void DBFractureDynamicsWorld::addRigidBody(btRigidBody* body)
{
    if (!body) {
        return;
    }
    dbtFractureBody* const fbody = fractureBodyFrom(body);
    if (fbody) m_fractureBodies.reserve(m_fractureBodies.size() + 1);
    try {
        btDiscreteDynamicsWorld::addRigidBody(body);
    } catch (...) {
        btDiscreteDynamicsWorld::removeRigidBody(body);
        throw;
    }
    if (fbody) m_fractureBodies.push_back(fbody);
}

void    DBFractureDynamicsWorld::removeRigidBody(btRigidBody* body)
{
    if (!body) {
        return;
    }
    btAlignedObjectArray<btTypedConstraint*> constraints;
    for (int index = 0; index < body->getNumConstraintRefs(); ++index) {
        constraints.push_back(body->getConstraintRef(index));
    }
    for (int index = 0; index < constraints.size(); ++index) {
        btDiscreteDynamicsWorld::removeConstraint(constraints[index]);
    }

    if (dbtFractureBody* fbody = fractureBodyFrom(body)) {
        m_fractureBodies.remove(fbody);
    }
    
    
    
    btDiscreteDynamicsWorld::removeRigidBody(body);
}

void DBFractureDynamicsWorld::destroyRigidBody(btRigidBody* body)
{
    if (!body) {
        return;
    }

    btAlignedObjectArray<btTypedConstraint*> constraints;
    for (int index = 0; index < body->getNumConstraintRefs(); ++index) {
        constraints.push_back(body->getConstraintRef(index));
    }

    btCollisionShape* const collisionShape = body->getCollisionShape();
    btMotionState* const motionState = body->getMotionState();
    removeRigidBody(body);

    for (int index = 0; index < constraints.size(); ++index) {
        delete constraints[index];
    }
    delete motionState;
    delete body;
    releaseGeneratedCollisionShape(collisionShape);
}

void    DBFractureDynamicsWorld::breakDisconnectedParts( dbtFractureBody* fracObj)
{
    if (!fracObj)
        return;

    btCompoundShape* compound =
        compoundShapeFrom(fracObj->getCollisionShape());
    if (!compound)
        return;
    int numChildren = compound->getNumChildShapes();
    
    if (numChildren<=1 ||
        fracObj->m_masses.size() != numChildren)
        return;
    
    //compute connectivity
    btUnionFind unionFind;
    
    btAlignedObjectArray<int> tags;
    tags.resize(numChildren);
    int i, index = 0;
    for ( i=0;i<numChildren;i++)
    {
#ifdef STATIC_SIMULATION_ISLAND_OPTIMIZATION
        tags[i] = index++;
#else
        tags[i] = i;
        index=i+1;
#endif
    }
    
    unionFind.reset(index);
    for (i=0;i<fracObj->m_connections.size();i++)
    {
        btConnection& connection = fracObj->m_connections[i];
        if (connection.m_strength > 0.)
        {
            if (connection.m_childIndex0 < 0 ||
                connection.m_childIndex0 >= tags.size() ||
                connection.m_childIndex1 < 0 ||
                connection.m_childIndex1 >= tags.size()) {
                continue;
            }
            int tag0 = tags[connection.m_childIndex0];
            int tag1 = tags[connection.m_childIndex1];
            unionFind.unite(tag0, tag1);
        }
    }
    const int numElem = unionFind.getNumElements();
    
    index=0;
    for (int ai=0;ai<numChildren;ai++)
    {
        int tag = unionFind.find(index);
        tags[ai] = tag;
        //Set the correct object offset in Collision Object Array
#if STATIC_SIMULATION_ISLAND_OPTIMIZATION
        unionFind.getElement(index).m_sz = ai;
#endif //STATIC_SIMULATION_ISLAND_OPTIMIZATION
        index++;
    }
    unionFind.sortIslands();
    
    int endIslandIndex=1;
    int startIslandIndex;
    
    btAlignedObjectArray<btCollisionObject*> removedObjects;
    
    for ( startIslandIndex=0;startIslandIndex<numElem;startIslandIndex = endIslandIndex)
    {
        int islandId = unionFind.getElement(startIslandIndex).m_id;
        for (endIslandIndex = startIslandIndex+1;(endIslandIndex<numElem) && (unionFind.getElement(endIslandIndex).m_id == islandId);endIslandIndex++)
        {
        }
        
        //    int fractureObjectIndex = -1;
        
        int numShapes=0;
        
        
        std::unique_ptr<btCompoundShape> newCompound(new btCompoundShape());
        btAlignedObjectArray<btScalar> masses;
        
        int idx;
        for (idx=startIslandIndex;idx<endIslandIndex;idx++)
        {
            int i = unionFind.getElement(idx).m_sz;
            //        btCollisionShape* shape = compound->getChildShape(i);
            newCompound->addChildShape(compound->getChildTransform(i),compound->getChildShape(i));
            masses.push_back(fracObj->m_masses[i]);
            numShapes++;
        }
        if (numShapes)
        {
            dbtFractureBody* newBody = addNewBody(
                fracObj->getWorldTransform(), &masses[0], std::move(newCompound));
            const btVector3 offset = newBody->getCenterOfMassPosition() - fracObj->getCenterOfMassPosition();
            const btVector3 velocity = fracObj->getVelocityInLocalPoint(offset);
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(velocity[axis]) ||
                    !std::isfinite(fracObj->getAngularVelocity()[axis])) {
                    destroyRigidBody(newBody);
                    throw std::invalid_argument("Unsafe fractured-body velocity.");
                }
            }
            newBody->setLinearVelocity(velocity);
            newBody->setAngularVelocity(fracObj->getAngularVelocity());
            newBody->setInterpolationLinearVelocity(newBody->getLinearVelocity());
            newBody->setInterpolationAngularVelocity(newBody->getAngularVelocity());
            
        }
    }
    
    
    
    
    
    destroyRigidBody(fracObj);
    
    
}

#include <stdio.h>


void DBFractureDynamicsWorld::fractureCallback( )
{
    
    btAlignedObjectArray<btFracturePair> sFracturePairs;
    
    if (!m_fracturingMode)
    {
        glueCallback();
        return;
    }
    
    int numManifolds = getDispatcher()->getNumManifolds();
    
    sFracturePairs.clear();
    
    
    for (int i=0;i<numManifolds;i++)
    {
        btPersistentManifold* manifold = getDispatcher()->getManifoldByIndexInternal(i);
        if (!manifold->getNumContacts())
            continue;
        
        btScalar totalImpact = 0.f;
        for (int p=0;p<manifold->getNumContacts();p++)
        {
            totalImpact += manifold->getContactPoint(p).m_appliedImpulse;
        }
        
        
        //some threshold otherwise resting contact would break objects after a while
        if (totalImpact < 40.f)
            continue;
        
        //        printf("strong impact\n");
        
        
        //@todo: add better logic to decide what parts to fracture
        //For example use the idea from the SIGGRAPH talk about the fracture in the movie 2012:
        //
        //Breaking thresholds can be stored as connectivity information between child shapes in the fracture object
        //
        //You can calculate some "impact value" by simulating all the individual child shapes
        //as rigid bodies, without constraints, running it in a separate simulation world
        //(or by running the constraint solver without actually modifying the dynamics world)
        //Then measure some "impact value" using the offset and applied impulse for each child shape
        //weaken the connections based on this "impact value" and only break
        //if this impact value exceeds the breaking threshold.
        //you can propagate the weakening and breaking of connections using the connectivity information
        
        const btCollisionObject* const body0 =
            static_cast<const btCollisionObject*>(manifold->getBody0());
        const btCollisionObject* const body1 =
            static_cast<const btCollisionObject*>(manifold->getBody1());
        int f0 = m_fractureBodies.size();
        int f1 = m_fractureBodies.size();
        for (int bodyIndex = 0;
             bodyIndex < m_fractureBodies.size(); ++bodyIndex) {
            const btCollisionObject* const fractureBody =
                m_fractureBodies[bodyIndex];
            if (fractureBody == body0) {
                f0 = bodyIndex;
            }
            if (fractureBody == body1) {
                f1 = bodyIndex;
            }
        }
        
        if (f0 == m_fractureBodies.size() && f1 == m_fractureBodies.size())
            continue;
        
        
        if (f0<m_fractureBodies.size())
        {
            int j=f0;
            
            //    if (!otherOb->getInvMass())
            //        continue;
            
            int pi=-1;
            
            for (int p=0;p<sFracturePairs.size();p++)
            {
                if (sFracturePairs[p].m_fracObj == m_fractureBodies[j])
                {
                    pi = p; break;
                }
            }
            
            if (pi<0)
            {
                btFracturePair p;
                p.m_fracObj = m_fractureBodies[j];
                p.m_contactManifolds.push_back(manifold);
                sFracturePairs.push_back(p);
            } else
            {
                if (sFracturePairs[pi].m_contactManifolds
                        .findLinearSearch(manifold) ==
                    sFracturePairs[pi].m_contactManifolds.size()) {
                    sFracturePairs[pi].m_contactManifolds.push_back(manifold);
                }
            }
        }
        
        
        if (f1 < m_fractureBodies.size())
        {
            int j=f1;
            {
                //    if (!otherOb->getInvMass())
                //        continue;
                
                
                int pi=-1;
                
                for (int p=0;p<sFracturePairs.size();p++)
                {
                    if (sFracturePairs[p].m_fracObj == m_fractureBodies[j])
                    {
                        pi = p; break;
                    }
                }
                if (pi<0)
                {
                    btFracturePair p;
                    p.m_fracObj = m_fractureBodies[j];
                    p.m_contactManifolds.push_back( manifold);
                    sFracturePairs.push_back(p);
                } else
                {
                    if (sFracturePairs[pi].m_contactManifolds
                            .findLinearSearch(manifold) ==
                        sFracturePairs[pi].m_contactManifolds.size()) {
                        sFracturePairs[pi].m_contactManifolds.push_back(
                            manifold);
                    }
                }
            }
        }
        
        //
    }
    
    //printf("m_fractureBodies size=%d\n",m_fractureBodies.size());
    //printf("sFracturePairs size=%d\n",sFracturePairs.size());
    if (!sFracturePairs.size())
        return;
    
    
    {
        //        printf("fracturing\n");
        
        for (int i=0;i<sFracturePairs.size();i++)
        {
            //check impulse/displacement at impact
            
            //weaken/break connections (and propagate breaking)
            
            //compute connectivity of connected child shapes
            
            
            if (sFracturePairs[i].m_fracObj->getCollisionShape()->isCompound())
            {
                btTransform tr;
                tr.setIdentity();
                btCompoundShape* oldCompound = compoundShapeFrom(
                    sFracturePairs[i].m_fracObj->getCollisionShape());
                if (!oldCompound) {
                    continue;
                }
                if (oldCompound->getNumChildShapes()>1)
                {
                    bool needsBreakingCheck = false;
                    
                    
                    //weaken/break the connections
                    
                    //@todo: propagate along the connection graph
                    for (int j=0;j<sFracturePairs[i].m_contactManifolds.size();j++)
                    {
                        btPersistentManifold* manifold = sFracturePairs[i].m_contactManifolds[j];
                        for (int k=0;k<manifold->getNumContacts();k++)
                        {
                            btManifoldPoint& pt = manifold->getContactPoint(k);
                            if (manifold->getBody0()==sFracturePairs[i].m_fracObj)
                            {
                                for (int f=0;f<sFracturePairs[i].m_fracObj->m_connections.size();f++)
                                {
                                    btConnection& connection = sFracturePairs[i].m_fracObj->m_connections[f];
                                    if (    (connection.m_childIndex0 == pt.m_index0) ||
                                        (connection.m_childIndex1 == pt.m_index0)
                                        )
                                    {
                                        connection.m_strength -= pt.m_appliedImpulse;
                                        if (connection.m_strength<0)
                                        {
                                            //remove or set to zero
                                            connection.m_strength=0.f;
                                            needsBreakingCheck = true;
                                        }
                                    }
                                }
                            } else
                            {
                                for (int f=0;f<sFracturePairs[i].m_fracObj->m_connections.size();f++)
                                {
                                    btConnection& connection = sFracturePairs[i].m_fracObj->m_connections[f];
                                    if (    (connection.m_childIndex0 == pt.m_index1) ||
                                        (connection.m_childIndex1 == pt.m_index1)
                                        )
                                    {
                                        connection.m_strength -= pt.m_appliedImpulse;
                                        if (connection.m_strength<0)
                                        {
                                            //remove or set to zero
                                            connection.m_strength=0.f;
                                            needsBreakingCheck = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (needsBreakingCheck)
                    {
                        breakDisconnectedParts(sFracturePairs[i].m_fracObj);
                    }
                }
                
            }
            
        }
    }
    
    sFracturePairs.clear();
    
}
