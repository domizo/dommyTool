// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2014 Erwin Coumans  http://continuousphysics.com/Bullet/

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
// Source comparison reference: https://github.com/bulletphysics/bullet3/blob/126b6762200b1a9ec8df39b8fd53cb2ad62fb146/examples/VoronoiFracture/btConvexConvexMprAlgorithm.cpp

//
//  btConvexConvexMprAlgorithm.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/18.
//

#include "btConvexConvexMprAlgorithm.h"

#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/CollisionDispatch/btManifoldResult.h"
#include "BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h"

#include "btComputeGjkEpaPenetration.h"
#include "btMprPenetration.h"

btConvexConvexMprAlgorithm::CreateFunc::CreateFunc() {}

btConvexConvexMprAlgorithm::CreateFunc::~CreateFunc() {}

btConvexConvexMprAlgorithm::btConvexConvexMprAlgorithm(
    btPersistentManifold* mf, const btCollisionAlgorithmConstructionInfo& ci,
    const btCollisionObjectWrapper* body0Wrap, const btCollisionObjectWrapper* body1Wrap)
    : btActivatingCollisionAlgorithm(ci, body0Wrap, body1Wrap), m_ownManifold(false),
      m_manifoldPtr(mf) {
    (void)body0Wrap;
    (void)body1Wrap;
}

btConvexConvexMprAlgorithm::~btConvexConvexMprAlgorithm() {
    if (m_ownManifold && m_manifoldPtr) {
        m_dispatcher->releaseManifold(m_manifoldPtr);
    }
}

namespace {

struct MprConvexAdapter {
    const btConvexShape* m_convex;
    btTransform m_worldTrans;
    inline btScalar getMargin() const {
        return m_convex->getMargin();
    }
    inline btVector3 getObjectCenterInWorld() const {
        return m_worldTrans.getOrigin();
    }
    inline const btTransform& getWorldTransform() const {
        return m_worldTrans;
    }
    inline btVector3 getLocalSupportWithMargin(const btVector3& dir) const {
        return m_convex->localGetSupportingVertex(dir);
    }
    inline btVector3 getLocalSupportWithoutMargin(const btVector3& dir) const {
        return m_convex->localGetSupportingVertexWithoutMargin(dir);
    }
};

struct MprDistanceInfo {
    btVector3 m_pointOnA;
    btVector3 m_pointOnB;
    btVector3 m_normalBtoA;
    btScalar m_distance;
};

} // namespace

//
// Convex-Convex collision algorithm
//
void btConvexConvexMprAlgorithm ::processCollision(const btCollisionObjectWrapper* body0Wrap,
                                                   const btCollisionObjectWrapper* body1Wrap,
                                                   const btDispatcherInfo& dispatchInfo,
                                                   btManifoldResult* resultOut) {
    (void)dispatchInfo;

    if (!m_manifoldPtr) {
        // swapped?
        m_manifoldPtr = m_dispatcher->getNewManifold(body0Wrap->getCollisionObject(),
                                                     body1Wrap->getCollisionObject());
        m_ownManifold = true;
    }
    resultOut->setPersistentManifold(m_manifoldPtr);

    MprConvexAdapter a;
    MprConvexAdapter b;
    a.m_worldTrans = body0Wrap->getWorldTransform();
    b.m_worldTrans = body1Wrap->getWorldTransform();
    a.m_convex = static_cast<const btConvexShape*>(body0Wrap->getCollisionShape());
    b.m_convex = static_cast<const btConvexShape*>(body1Wrap->getCollisionShape());

    btGjkCollisionDescription collisionDescription;
    MprDistanceInfo distanceInfo;
    int result = btComputeGjkDistance(a, b, collisionDescription, &distanceInfo);
    if (result != 0) {
        btMprCollisionDescription mprDescription;
        result = btComputeMprPenetration(a, b, mprDescription, &distanceInfo);
    }
    if (result == 0 && distanceInfo.m_distance <= 0) {
        resultOut->addContactPoint(distanceInfo.m_normalBtoA, distanceInfo.m_pointOnB,
                                   distanceInfo.m_distance);
    }
    if (m_ownManifold) {
        resultOut->refreshContactPoints();
    }
}

btScalar btConvexConvexMprAlgorithm::calculateTimeOfImpact(btCollisionObject* col0,
                                                           btCollisionObject* col1,
                                                           const btDispatcherInfo& dispatchInfo,
                                                           btManifoldResult* resultOut) {
    (void)col0;
    (void)col1;
    (void)resultOut;
    (void)dispatchInfo;
    // This narrow-phase implementation only supports discrete collision.
    // Returning 1 means that no earlier time of impact was computed.
    return btScalar(1.0);
}
