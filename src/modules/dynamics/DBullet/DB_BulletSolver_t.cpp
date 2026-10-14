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
//  DB_BulletSolver_t.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#include "DB_BulletSolver_t.h"

#include "DBRigidBodyNode.h"
#include "DBNailConstraintNode.h"
#include "DBHingeConstraintNode.h"
#include "DBSliderConstraintNode.h"
#include "DBSixdofConstraintNode.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"

#include "LinearMath/btSerializer.h"

#include "dbSoftBody_t.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include <cstdio>
#include <exception>
#include <memory>
#include <sys/types.h>
#include "DMayaHeaders.h"





class MySerializer : public btDefaultSerializer
{
    DB_BulletSolver_t* m_solver;
    
public:
    MySerializer(DB_BulletSolver_t* solver)
    :btDefaultSerializer(),
    m_solver(solver)
    {
    }
    
    const char* findNameForPointer(const void* ptr) const override
    {
        std::map<const void*, std::string>::const_iterator nameIt = m_solver->m_nameMap.find(ptr);
        if (nameIt != m_solver->m_nameMap.end())
            return nameIt->second.c_str();
        return 0;
        
    }
    
    
};

bool DB_BulletSolver_t::export_bullet_file(const char* fileName)
{
    if (!fileName || fileName[0] == '\0' || !m_dynamicsWorld) {
        MGlobal::displayError(
            "Bullet export requires a file name and initialized world.");
        return false;
    }
    if (!dommy::file::isSafePathSyntax(fileName)) {
        MGlobal::displayError(
            "Bullet export path is too long or contains unsafe components.");
        return false;
    }

	try {
		MySerializer serializer(this);
		m_dynamicsWorld->serialize(&serializer);

		const int bufferSize = serializer.getCurrentBufferSize();
		const void* buffer = serializer.getBufferPointer();
		if (bufferSize < 0 || (bufferSize > 0 && !buffer)) {
			MGlobal::displayError(
				MString("Bullet serialization failed: ") + fileName);
			return false;
		}

		dommy::file::PendingOutputFile pending(fileName);
		std::ostream& output = pending.stream();
		if (bufferSize > 0) {
			output.write(static_cast<const char*>(buffer), bufferSize);
		}
		if (!pending.close() || !output || !pending.commit()) {
			MGlobal::displayError(
				MString("Unable to write the complete Bullet file: ") + fileName);
			return false;
		}
		return true;
	} catch (const std::exception& error) {
		MGlobal::displayError(
			MString("Bullet export failed for ") + fileName + ": " +
			error.what());
	} catch (...) {
		MGlobal::displayError(
			MString("Bullet export failed for ") + fileName + ".");
	}
	return false;
}



void DB_BulletSolver_t::register_name(const void* pointer,const char* objectName)
{
    if (pointer && objectName)
    {
        m_nameMap[pointer] = objectName;
    }
}

class bt_debug_draw : public btIDebugDraw
{
    int m_debugMode;
public:
    bt_debug_draw() : m_debugMode(btIDebugDraw::DBG_NoDebug) {}

    void drawLine(const btVector3& from,const btVector3& to,const btVector3& color) override
    {
        glBegin(GL_LINES);
        glColor3f(color.getX(), color.getY(), color.getZ());
        glVertex3d(from.getX(), from.getY(), from.getZ());
        glVertex3d(to.getX(), to.getY(), to.getZ());
        glEnd();
    }
    void drawContactPoint(
        const btVector3&, const btVector3&, btScalar, int,
        const btVector3&) override {}
    void reportErrorWarning(const char*) override {}
    void draw3dText(const btVector3&, const char*) override {}
    void setDebugMode(int debugMode) override { m_debugMode = debugMode; }
    int getDebugMode() const override { return m_debugMode; }
};

DB_BulletSolver_t::DB_BulletSolver_t()
{
    createWorld();
}

DB_BulletSolver_t::~DB_BulletSolver_t()
{
    destroyWorld();
}

void DB_BulletSolver_t::destroyWorld() noexcept
{
    if (m_softBodyWorldReference) m_softBodyWorldReference->invalidate();
    if (m_dynamicsWorld)
    {
        while (m_dynamicsWorld->getNumConstraints())
            m_dynamicsWorld->removeConstraint(m_dynamicsWorld->getConstraint(0));

        m_dynamicsWorld->setDebugDrawer(nullptr);
        m_dynamicsWorld.reset();
    }

    m_debugDrawer.reset();
    m_softBodyWorldReference.reset();
    m_worldInfo.reset();
    m_solver.reset();
    m_broadphase.reset();
    m_dispatcher.reset();
    m_collisionConfiguration.reset();
    m_nameMap.clear();
}

void DB_BulletSolver_t::createWorld()
{
    destroyWorld();

    // Build the complete world locally. Members remain empty if any allocation
    // or Bullet initialization step throws.
    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration(
        new btSoftBodyRigidBodyCollisionConfiguration());
    std::unique_ptr<btCollisionDispatcher> dispatcher(
        new btCollisionDispatcher(collisionConfiguration.get()));
    std::unique_ptr<btBroadphaseInterface> broadphase(new btDbvtBroadphase());
    std::unique_ptr<btConstraintSolver> solver(
        new btSequentialImpulseConstraintSolver());
    std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld> dynamicsWorld(
        new dommy::bullet::DBoundedSoftDynamicsWorld(dispatcher.get(),
                                     broadphase.get(),
                                     solver.get(),
                                     collisionConfiguration.get()));

    //register algorithm for concave meshes
    btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher.get());
    
    // set default gravity
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
    
    dynamicsWorld->getDispatchInfo().m_enableSPU = true;
    
    DBulletSoftBodyWorldInfoPtr worldInfo(
        new btSoftBodyWorldInfo());
    worldInfo->m_broadphase = broadphase.get();
    worldInfo->m_dispatcher = dispatcher.get();
    worldInfo->m_gravity = dynamicsWorld->getGravity();
    worldInfo->m_sparsesdf.Initialize();
    DBulletSoftBodyWorldReferencePtr softBodyWorldReference(
        new DBulletSoftBodyWorldReference(worldInfo.get()));

    std::unique_ptr<bt_debug_draw> debugDrawer(new bt_debug_draw());
    dynamicsWorld->setDebugDrawer(debugDrawer.get());

    // Commit only after the complete object graph is ready. Member ownership
    // preserves the dependency order even if a later call exits early.
    m_collisionConfiguration = std::move(collisionConfiguration);
    m_dispatcher = std::move(dispatcher);
    m_broadphase = std::move(broadphase);
    m_solver = std::move(solver);
    m_worldInfo = std::move(worldInfo);
    m_softBodyWorldReference = std::move(softBodyWorldReference);
    m_debugDrawer = std::move(debugDrawer);
    m_dynamicsWorld = std::move(dynamicsWorld);
}

void DB_BulletSolver_t::debug_draw(int dbgMode)
{
    if (!m_dynamicsWorld || !m_dynamicsWorld->getDebugDrawer()) return;
    m_dynamicsWorld->getDebugDrawer()->setDebugMode(dbgMode);
    m_dynamicsWorld->debugDrawWorld();
}

dbSoftBody_t::pointer DB_BulletSolver_t::create_soft_body(const std::vector<float> &triVertexCoords,
    const std::vector<int> &triVertexIndices, const dommy::bullet::SoftBodyParameters& parameters)
{
    if (!m_worldInfo || !m_softBodyWorldReference || !dommy::bullet::hasValidSoftBodyMeshInput(
            triVertexCoords, triVertexIndices)) {
        return dbSoftBody_t::pointer();
    }
    // we simply invoke the soft body constructor together with this solver's world info
    dbSoftBody_t::pointer sptr(new dbSoftBody_t(*m_worldInfo, triVertexCoords, triVertexIndices,
                                             parameters, m_softBodyWorldReference));
    return sptr->valid() ? sptr : dbSoftBody_t::pointer();
}
