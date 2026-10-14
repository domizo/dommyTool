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
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/FractureDemo/btFractureBody.h

//
//  dbtFractureBody.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//


#pragma once

class btCollisionShape;
class btDynamicsWorld;
class btCollisionWorld;
class btCompoundShape;
class btManifoldPoint;

#include "LinearMath/btAlignedObjectArray.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"

#include <stdexcept>

#define CUSTOM_FRACTURE_TYPE (btRigidBody::CO_USER_TYPE+1)


struct btConnection
{
    
    btCollisionShape*    m_childShape0;
    btCollisionShape*    m_childShape1;
    int    m_childIndex0;
    int    m_childIndex1;
    btScalar    m_strength;
};

class dbtFractureBody : public btRigidBody
{
    //connections
public:
    
    btDynamicsWorld*    m_world;
    btAlignedObjectArray<btScalar>    m_masses;
    btAlignedObjectArray<btConnection>    m_connections;
    
    
    
    dbtFractureBody(    const btRigidBodyConstructionInfo& constructionInfo, btDynamicsWorld*    world)
    :btRigidBody(constructionInfo),
    m_world(world)
    {
        m_masses.push_back(constructionInfo.m_mass);
        m_internalType=CUSTOM_FRACTURE_TYPE+CO_RIGID_BODY;
    }
    
    
    
    ///btRigidBody constructor for backwards compatibility.
    ///To specify friction (etc) during rigid body construction, please use the other constructor (using btRigidBodyConstructionInfo)
    dbtFractureBody(    btScalar mass, btMotionState* motionState, btCollisionShape* collisionShape, const btVector3& localInertia, btScalar* masses, int numMasses, btDynamicsWorld*    world)
    :btRigidBody(mass,motionState,collisionShape,localInertia),
    m_world(world)
    {
        if (numMasses < 0 || (numMasses > 0 && !masses)) {
            throw std::invalid_argument(
                "Fracture body masses must match the requested count.");
        }
        for (int i=0;i<numMasses;i++)
            m_masses.push_back(masses[i]);
        
        m_internalType=CUSTOM_FRACTURE_TYPE+CO_RIGID_BODY;
    }
    
    
    
    void    recomputeConnectivity(btCollisionWorld* world);
    
    
    static btCompoundShape* shiftTransform(btCompoundShape* boxCompound,btScalar* masses,btTransform& shift, btVector3& principalInertia);
    
    static btCompoundShape* shiftTransformDistributeMass(btCompoundShape* boxCompound,btScalar mass,btTransform& shift);
    
    static bool collisionCallback(btManifoldPoint& cp,    const btCollisionObject* colObj0,int partId0,int index0,const btCollisionObject* colObj1,int partId1,int index1);
    
};


void fractureCallback(btDynamicsWorld* world, btScalar timeStep);
void glueCallback(btDynamicsWorld* world, btScalar timeStep);
