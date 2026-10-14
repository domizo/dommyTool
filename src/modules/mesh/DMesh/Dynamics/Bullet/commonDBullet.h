// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Walt Disney Studios
 
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
 
1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 
Written by: Nicola Candussi <nicola@fluidinteractive.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
01/27/2010 : Replaced COLLADA export with Bullet binary export
02/18/2010 : Re-enabled COLLADA export as option (next to Bullet binary export)

Modified by Francisco Gochez <fjgochez@gmail.com>
Nov 2011 - Dec 2011 : Added soft body logic
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_solver.cpp

//
//  commonDBullet.h
//  dommyTool
//
//  Created by yuzo on 2017/10/17.
//

#pragma once


#include <algorithm>
#include <memory>
#include <vector>

#include "DCurveData.h"
#include "DMeshData.h"
#include "db_collision_shape_base.h"
#include "db_constraint_base.h"


#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

#include "commonDBulletCollision.h"
#include "commonBulletRigidBody.h"


#if defined(__APPLE__) && !defined (VMDMESA)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

// The corresponding MObject arrays own these MPxData instances. These lists
// are temporary, non-owning views used while populating output handles.
typedef std::vector<DMeshData*> DMeshDataPtrList;
typedef std::vector<DCurveData*> DCurveDataPtrList;
typedef std::vector<MUuid> UuidList;
typedef std::shared_ptr<db_collision_shape_base> DBTCollisionShapePtr;
typedef std::vector<DBTCollisionShapePtr> DBTCollisionShapeList;


// The solver owns the world while companion nodes own the objects registered
// in it. Keeping the raw world pointer behind a shared lifetime token lets a
// node detect that its solver has already been destroyed.
class DBulletWorldReference final
{
public:
    explicit DBulletWorldReference(btDynamicsWorld* world) noexcept
        : m_world(world)
    {
    }

    DBulletWorldReference(const DBulletWorldReference&) = delete;
    DBulletWorldReference& operator=(const DBulletWorldReference&) = delete;

    btDynamicsWorld* get() const noexcept { return m_world; }
    void invalidate() noexcept
    {
        while (!m_constraints.empty()) detachConstraint(m_constraints.back());
        m_world = nullptr;
    }

    void trackConstraint(db_constraint_base* constraint)
    {
        m_constraints.push_back(constraint);
    }

    void forgetConstraint(db_constraint_base* constraint) noexcept
    {
        const auto found = std::find(m_constraints.begin(), m_constraints.end(), constraint);
        if (found != m_constraints.end()) m_constraints.erase(found);
    }

    void detachConstraint(db_constraint_base* owner) noexcept
    {
        if (!owner) return;
        btTypedConstraint* native = owner->constraint();
        // A body may already have detached this constraint. Never call Bullet's
        // removeConstraint again: it dereferences both bodies even when absent.
        if (m_world && native) {
            for (int i = 0; i < m_world->getNumConstraints(); ++i) {
                if (m_world->getConstraint(i) == native) {
                    m_world->removeConstraint(native);
                    break;
                }
            }
        }
        forgetConstraint(owner);
        owner->set_constraint(nullptr);
    }

    void detachConstraintsForBody(const btRigidBody* body) noexcept
    {
        if (!body) return;
        for (std::size_t i = m_constraints.size(); i > 0; --i) {
            db_constraint_base* owner = m_constraints[i - 1];
            btTypedConstraint* native = owner->constraint();
            if (native && (&native->getRigidBodyA() == body ||
                           &native->getRigidBodyB() == body))
                detachConstraint(owner);
        }
    }

private:
    btDynamicsWorld* m_world;
    // Non-owning registry; companion nodes retain ownership of the wrappers.
    std::vector<db_constraint_base*> m_constraints;
};

typedef std::shared_ptr<DBulletWorldReference> DBulletWorldReferencePtr;
typedef std::weak_ptr<DBulletWorldReference> DBulletWeakWorldReference;

inline btDynamicsWorld* ResolveBulletWorld(
    const DBulletWeakWorldReference& reference) noexcept
{
    const DBulletWorldReferencePtr worldReference = reference.lock();
    return worldReference ? worldReference->get() : nullptr;
}


struct DBulletRigidBodyDeleter
{
    void operator()(btRigidBody* body) const noexcept;
};

typedef std::unique_ptr<btRigidBody, DBulletRigidBodyDeleter>
    DBulletRigidBodyPtr;
typedef std::unique_ptr<db_constraint_base> DBulletConstraintPtr;

DBulletRigidBodyPtr MakeBulletRigidBody(
    std::unique_ptr<btMotionState> motionState,
    const btRigidBody::btRigidBodyConstructionInfo& constructionInfo,
    std::unique_ptr<customDBData> userData =
        std::unique_ptr<customDBData>());
DBulletRigidBodyPtr MakeSimpleKinematicRigidBody(
    btCollisionShape* collisionShape,
    const btVector3& position);

void AddBulletRigidBody(
    btDynamicsWorld& world,
    std::vector<btRigidBody*>& bodies,
    DBulletRigidBodyPtr body);
void AddBulletRigidBodyWithShape(
    btDynamicsWorld& world,
    std::vector<btRigidBody*>& bodies,
    std::vector<db_collision_shape_base*>& shapes,
    DBulletRigidBodyPtr body,
    std::unique_ptr<db_collision_shape_base> shape);
void AddBulletConstraint(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<db_constraint_base*>& constraints,
    DBulletConstraintPtr constraint,
    bool disableCollisionsBetweenLinkedBodies = false);

// Prepare the final scaled/margined shape before calculating its body inertia.
bool PrepareBulletCollisionShape(db_collision_shape_base& shape,
    const btVector3& scale, float margin, float mass, btVector3& inertia);
// Keep the old owner alive until native registration of its replacement succeeds.
bool ReplaceBulletCollisionShape(btDynamicsWorld& world, btRigidBody& body,
    db_collision_shape_base*& owner, std::unique_ptr<db_collision_shape_base> replacement,
    const btVector3& scale, float margin, float mass);


class dm_debug_draw : public btIDebugDraw
{
    int m_debugMode;
public:
    dm_debug_draw() : m_debugMode(btIDebugDraw::DBG_NoDebug) {}

    void drawLine(const btVector3& from,const btVector3& to,const btVector3& color) override
    {
        glBegin(GL_LINES);
        glColor3f(color.getX(), color.getY(), color.getZ());
        glVertex3d(from.getX(), from.getY(), from.getZ());
        glVertex3d(to.getX(), to.getY(), to.getZ());
        glEnd();
    }
    void drawContactPoint(
        const btVector3& /*pointOnB*/, const btVector3& /*normalOnB*/,
        btScalar /*distance*/, int /*lifeTime*/,
        const btVector3& /*color*/) override {}
    void reportErrorWarning(const char* /*warningString*/) override {}
    void draw3dText(
        const btVector3& /*location*/, const char* /*textString*/) override {}
    void setDebugMode(int debugMode) override { m_debugMode = debugMode; }
    int getDebugMode() const override { return m_debugMode; }
};




void ReleaseBulletCollisionShapes(std::vector<db_collision_shape_base*> &collisionShapes);
void ReleaseBulletRigidBodies(std::vector<btRigidBody*> &rbList);
void ReleaseBulletConstrains(std::vector<db_constraint_base*> &constraintLists);
void DetachAndReleaseBulletRigidBodies(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<btRigidBody*>& bodies) noexcept;
void DetachAndReleaseBulletConstraints(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<db_constraint_base*>& constraints) noexcept;
