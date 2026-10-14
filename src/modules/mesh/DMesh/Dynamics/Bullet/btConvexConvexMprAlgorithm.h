// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

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
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/VoronoiFracture/btConvexConvexMprAlgorithm.h

//
//  btConvexConvexMprAlgorithm.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/18.
//

#ifndef BT_CONVEX_CONVEX_MPR_ALGORITHM_H
#define BT_CONVEX_CONVEX_MPR_ALGORITHM_H

#include "BulletCollision/CollisionDispatch/btActivatingCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btCollisionCreateFunc.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"

#include <new>

/// Discrete convex-convex collision using GJK distance and MPR penetration.
///
/// Continuous time-of-impact calculation is not implemented; callers receive
/// the Bullet no-earlier-impact fraction.
class btConvexConvexMprAlgorithm : public btActivatingCollisionAlgorithm {

    bool m_ownManifold;
    btPersistentManifold* m_manifoldPtr;

  public:
    btConvexConvexMprAlgorithm(btPersistentManifold* mf,
                               const btCollisionAlgorithmConstructionInfo& ci,
                               const btCollisionObjectWrapper* body0Wrap,
                               const btCollisionObjectWrapper* body1Wrap);

    ~btConvexConvexMprAlgorithm() override;

    void processCollision(const btCollisionObjectWrapper* body0Wrap,
                          const btCollisionObjectWrapper* body1Wrap,
                          const btDispatcherInfo& dispatchInfo,
                          btManifoldResult* resultOut) override;

    btScalar calculateTimeOfImpact(btCollisionObject* body0, btCollisionObject* body1,
                                   const btDispatcherInfo& dispatchInfo,
                                   btManifoldResult* resultOut) override;

    void getAllContactManifolds(btManifoldArray& manifoldArray) override {
        if (m_manifoldPtr && m_ownManifold) {
            manifoldArray.push_back(m_manifoldPtr);
        }
    }

    const btPersistentManifold* getManifold() const {
        return m_manifoldPtr;
    }

    struct CreateFunc : public btCollisionAlgorithmCreateFunc {

        CreateFunc();

        ~CreateFunc() override;

        btCollisionAlgorithm*
        CreateCollisionAlgorithm(btCollisionAlgorithmConstructionInfo& ci,
                                 const btCollisionObjectWrapper* body0Wrap,
                                 const btCollisionObjectWrapper* body1Wrap) override {
            void* mem =
                ci.m_dispatcher1->allocateCollisionAlgorithm(sizeof(btConvexConvexMprAlgorithm));
            return new (mem) btConvexConvexMprAlgorithm(ci.m_manifold, ci, body0Wrap, body1Wrap);
        }
    };
};

#endif // BT_CONVEX_CONVEX_MPR_ALGORITHM_H
