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
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/FractureDemo/btFractureBody.cpp

//
//  dbtFractureBody.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//

#include "dbtFractureBody.h"


#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletDynamics/Dynamics/btDynamicsWorld.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"

#include <memory>
#include <stdexcept>
#include <vector>


void    dbtFractureBody::recomputeConnectivity(btCollisionWorld* world)
{
    m_connections.clear();
    if (!world || !getCollisionShape()) {
        return;
    }
    //@todo use the AABB tree to avoid N^2 checks
    
    if (getCollisionShape()->isCompound())
    {
        btCompoundShape* compound =
            static_cast<btCompoundShape*>(getCollisionShape());
        for (int i=0;i<compound->getNumChildShapes();i++)
        {
            for (int j=i+1;j<compound->getNumChildShapes();j++)
            {
                
                struct   MyContactResultCallback : public btCollisionWorld::ContactResultCallback
                {
                    bool m_connected;
                    btScalar m_margin;
                    MyContactResultCallback() :m_connected(false),m_margin(0.05)
                    {
                    }
                    btScalar addSingleResult(
                        btManifoldPoint& cp,
                        const btCollisionObjectWrapper* /*colObj0Wrap*/,
                        int /*partId0*/,
                        int /*index0*/,
                        const btCollisionObjectWrapper* /*colObj1Wrap*/,
                        int /*partId1*/,
                        int /*index1*/) override
                    {
                        if (cp.getDistance()<=m_margin)
                            m_connected = true;
                        return 1.f;
                    }
                };
                
                MyContactResultCallback result;
                
                btCollisionObject obA;
                obA.setWorldTransform(compound->getChildTransform(i));
                obA.setCollisionShape(compound->getChildShape(i));
                btCollisionObject obB;
                obB.setWorldTransform(compound->getChildTransform(j));
                obB.setCollisionShape(compound->getChildShape(j));
                world->contactPairTest(&obA,&obB,result);
                if (result.m_connected)
                {
                    btConnection tmp;
                    tmp.m_childIndex0 = i;
                    tmp.m_childIndex1 = j;
                    tmp.m_childShape0 = compound->getChildShape(i);
                    tmp.m_childShape1 = compound->getChildShape(j);
                    tmp.m_strength = 1.f;//??
                    m_connections.push_back(tmp);
                }
            }
        }
    }
    
    
}

btCompoundShape* dbtFractureBody::shiftTransformDistributeMass(btCompoundShape* boxCompound,btScalar mass,btTransform& shift)
{
    if (!boxCompound) {
        throw std::invalid_argument(
            "Cannot distribute mass for a null compound shape.");
    }
    btVector3 principalInertia;

    const int childCount = boxCompound->getNumChildShapes();
    if (childCount == 0) {
        shift.setIdentity();
        return new btCompoundShape();
    }
    std::vector<btScalar> masses(childCount);
    for (int j=0;j<childCount;j++)
    {
        //evenly distribute mass
        masses[j]=mass/childCount;
    }

    return shiftTransform(boxCompound, &masses[0], shift, principalInertia);
}


btCompoundShape* dbtFractureBody::shiftTransform(btCompoundShape* boxCompound,btScalar* masses,btTransform& shift, btVector3& principalInertia)
{
    if (!boxCompound || !masses ||
        boxCompound->getNumChildShapes() == 0) {
        throw std::invalid_argument(
            "Cannot shift an empty fracture compound.");
    }
    btTransform principal;
    btScalar totalMass = 0;
    for (int i = 0; i < boxCompound->getNumChildShapes(); ++i) {
        if (masses[i] <= 0 || !dommy::bullet::isSafeInverseOperand(masses[i]))
            throw std::invalid_argument("Fracture child masses must be positive and finite.");
        totalMass += masses[i];
    }
    if (!dommy::bullet::isSafeInverseOperand(totalMass))
        throw std::invalid_argument("Unsafe fracture total mass.");
    boxCompound->calculatePrincipalAxisTransform(masses,principal,principalInertia);
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(principal.getOrigin()[axis]) ||
            !dommy::bullet::isSafeInverseOperand(principalInertia[axis]))
            throw std::invalid_argument("Unsafe fracture principal frame.");
        for (int column = 0; column < 3; ++column) {
            if (!std::isfinite(principal.getBasis()[axis][column]))
                throw std::invalid_argument("Unsafe fracture principal orientation.");
        }
    }
    
    
    // Build a separate, non-recursive compound. Child shapes remain borrowed;
    // the input compound and its child frames are never mutated.
    std::unique_ptr<btCompoundShape> newBoxCompound(new btCompoundShape());
    for (int i=0;i<boxCompound->getNumChildShapes();i++)
    {
        btTransform newChildTransform = principal.inverse()*boxCompound->getChildTransform(i);
        newBoxCompound->addChildShape(newChildTransform,boxCompound->getChildShape(i));
    }
    
    
    
    shift = principal;
    return newBoxCompound.release();
}
