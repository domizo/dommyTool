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
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/FractureDemo/btFractureDynamicsWorld.h

//
//  DBFractureDynamicsWorld.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//


#pragma once



#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "LinearMath/btAlignedObjectArray.h"

#include <memory>

class dbtFractureBody;
class btCompoundShape;
class btTransform;


///The DBFractureDynamicsWorld class enabled basic glue and fracture of objects.
///If/once this implementation is stablized/tested we might merge it into btDiscreteDynamicsWorld and remove the class.
class DBFractureDynamicsWorld : public btDiscreteDynamicsWorld
{
    btAlignedObjectArray<dbtFractureBody*> m_fractureBodies;
    btAlignedObjectArray<btCollisionShape*> m_ownedGeneratedCollisionShapes;
    
    bool    m_fracturingMode;
    
    dbtFractureBody* addNewBody(const btTransform& oldTransform, btScalar* masses,
                                std::unique_ptr<btCompoundShape> oldCompound);

    void ownGeneratedCollisionShape(btCollisionShape* shape);
    void releaseGeneratedCollisionShape(btCollisionShape* shape);
    dbtFractureBody* adoptGeneratedBody(std::unique_ptr<dbtFractureBody> body,
                                      std::unique_ptr<btCompoundShape> shape);
    
    void    breakDisconnectedParts( dbtFractureBody* fracObj);
    
public:
    
    DBFractureDynamicsWorld ( btDispatcher* dispatcher,btBroadphaseInterface* pairCache,btConstraintSolver* constraintSolver,btCollisionConfiguration* collisionConfiguration);
    ~DBFractureDynamicsWorld() override;
    
    void    addRigidBody(btRigidBody* body) override;
    
    void    removeRigidBody(btRigidBody* body) override;

    /// Remove and destroy a rigid body together with resources created by this world.
    void destroyRigidBody(btRigidBody* body);
    
    void    solveConstraints(btContactSolverInfo& solverInfo) override;
    
    ///either fracture or glue (!fracture)
    void    setFractureMode(bool fracture)
    {
        m_fracturingMode = fracture;
    }
    
    bool getFractureMode() const { return m_fracturingMode;}
    
    ///normally those callbacks are called internally by the 'solveConstraints'
    void glueCallback();
    
    ///normally those callbacks are called internally by the 'solveConstraints'
    void fractureCallback();
    
};
