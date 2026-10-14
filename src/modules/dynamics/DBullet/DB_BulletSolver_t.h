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

Modified by Francisco Gochez <fjgochez@gmail.com>
Nov 2011 - Dec 2011 : Added soft body logic, and changed solvers used to
btSoftRigidBodyWorld.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_solver.h

//
//  DB_BulletSolver_t.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#pragma once


#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cmath>


#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"
#include "LinearMath/btHashMap.h"

#include "solver_impl.h"
#include "DB_RigidBody_t.h"
#include "dbSoftBody_t.h"
#include "DB_SphereShape_t.h"
#include "DB_PlaneShape_t.h"
#include "DB_BoxShape_t.h"
#include "bt_convex_hull_shape.h"
#include "DB_MeshShape_t.h"
#include "DB_HacdShape_t.h"
#include "bt_composite_shape.h"
#include "Collision/DCollisionMeshInput.h"
#include <vector>

#include "DB_Nail_Constraint_t.h"
#include "DB_Hinge_Constraint_t.h"
#include "DB_Slider_Constraint_t.h"
#include "DB_Sixdof_Constraint_t.h"
#include "DBulletResourceOwnership.h"


class DB_RigidBodyNode;


class DB_BulletSolver_t: public solver_impl_t
{
public:
    ~DB_BulletSolver_t() override;

    rigid_body_impl_t* create_rigid_body(collision_shape_impl_t* cs) override {
        DB_CollisionShape_t* bulletShape = dynamic_cast<DB_CollisionShape_t*>(cs);
        if (!bulletShape || !bulletShape->shape()) {
            return nullptr;
        }
        return new DB_RigidBody_t(cs);
    }
    
    /** \brief Generate a soft body object from a given mesh.
     
     This function will construct a soft_body_t object from a mesh using the btSoftBodyHelpers::CreateFromTriMesh
     from Bullet.  The mesh should be passed in as a vector of (integer) indices and and a float vector of coordinates in the form
     {x1, y1, z1, x2, y2, ..., x_n, y_n, z_n}, where n is the number of vertices (so this array will have 3 * num_vertices elements)
     The indices will thus index these coordinates to form triangles, so the index vector should have length equal to the number of triangles
     in the mesh * 3.
     
     \param triVertexCoords constant std::vector<float> reference to the "unrolled" vertex coordinates as described above
     \param triVertexIndices constant std::vector<int> reference to the triangle vertex indices
     
     \return soft_body_t object with the new soft body
     
     */
    dbSoftBody_t::pointer create_soft_body(const std::vector<float> &triVertexCoords,
        const std::vector<int> &triVertexIndices,
        const dommy::bullet::SoftBodyParameters& parameters) override;
    
    collision_shape_impl_t* create_sphere_shape(float radius) override {
        if (!dommy::bullet::hasValidSphereInput(radius)) {
            return nullptr;
        }
        DB_SphereShape_t* result = new DB_SphereShape_t(radius);
        if (!result->shape()) { delete result; return nullptr; }
        return result;
    }
    
    collision_shape_impl_t* create_plane_shape(vec3f const& normal, float d) override {
        if (!dommy::bullet::hasValidPlaneInput(normal, d)) {
            return nullptr;
        }
        return new DB_PlaneShape_t(normal, d);
    }
    
    collision_shape_impl_t* create_box_shape(vec3f const& halfExtents) override {
        if (!dommy::bullet::hasValidBoxInput(halfExtents)) {
            return nullptr;
        }
        DB_BoxShape_t* result = new DB_BoxShape_t(halfExtents);
        if (!result->shape()) { delete result; return nullptr; }
        return result;
    }
    
    collision_shape_impl_t* create_convex_hull_shape(vec3f const* vertices, size_t num_vertices,
                                                     vec3f const* normals,
                                                     unsigned int const *indices, size_t num_indices) override
    {
        if (!dommy::bullet::hasValidCollisionMeshInput(
                vertices, num_vertices, normals, indices, num_indices)) {
            return nullptr;
        }
        bt_convex_hull_shape_t* shape = new bt_convex_hull_shape_t(
            vertices, num_vertices, normals, indices, num_indices);
        if (shape->shape()) {
            return shape;
        }
        delete shape;
        return nullptr;
    }
    
    collision_shape_impl_t* create_dynamic_mesh_shape(vec3f const* vertices, size_t num_vertices,
                                                      vec3f const* normals,
                                                      unsigned int const *indices, size_t num_indices) override
    {
        if (!dommy::bullet::hasValidCollisionMeshInput(
                vertices, num_vertices, normals, indices, num_indices)) {
            return nullptr;
        }
        bool dynamicMesh = true;
        DB_MeshShape_t* shape = new DB_MeshShape_t(
            vertices, num_vertices, normals, indices, num_indices, dynamicMesh);
        if (shape->shape()) {
            return shape;
        }
        delete shape;
        return nullptr;
    }
    
    
    
    collision_shape_impl_t* create_hacd_shape(vec3f const* vertices, size_t num_vertices,
                                              vec3f const* normals,
                                              unsigned int const *indices, size_t num_indices) override
    {
        if (!dommy::bullet::hasValidCollisionMeshInput(
                vertices, num_vertices, normals, indices, num_indices)) {
            return nullptr;
        }
        bool dynamicMesh = true;
        DB_HacdShape_t* shape = new DB_HacdShape_t(
            vertices, num_vertices, normals, indices, num_indices, dynamicMesh);
        if (shape->shape()) {
            return shape;
        }

        delete shape;
        return nullptr;
    }
    
    collision_shape_impl_t* create_composite_shape(
        dbCollisionShape_t::pointer* childShapes,
        vec3f* childPositions,
        quatf* childOrientations,
        int numChildren) override
    {
        if (!childShapes || !childPositions || !childOrientations ||
            numChildren <= 0) {
            return nullptr;
        }
        for (int child = 0; child < numChildren; ++child) {
            if (!childShapes[child] ||
                !childShapes[child]->getBulletCollisionShape() ||
                !dommy::bullet::isFiniteVec3(childPositions[child]) ||
                !dommy::bullet::hasValidQuaternion(childOrientations[child])) {
                return nullptr;
            }
        }
        bt_composite_shape_t* tmp = new bt_composite_shape_t(childShapes, childPositions, childOrientations, numChildren);
        if (!tmp->shape()) { delete tmp; return nullptr; }
        return tmp;
    }
    
    collision_shape_impl_t* create_static_mesh_shape(vec3f const* vertices, size_t num_vertices,
                                                     vec3f const* normals,
                                                     unsigned int const *indices, size_t num_indices) override
    {
        if (!dommy::bullet::hasValidCollisionMeshInput(
                vertices, num_vertices, normals, indices, num_indices)) {
            return nullptr;
        }
        bool dynamicMesh = false;
        DB_MeshShape_t* shape = new DB_MeshShape_t(
            vertices, num_vertices, normals, indices, num_indices, dynamicMesh);
        if (shape->shape()) {
            return shape;
        }
        delete shape;
        return nullptr;
    }
    
    
    nail_constraint_impl_t* create_nail_constraint(rigid_body_impl_t* rb,
                                                   vec3f const& pivot) override
    {
        if (!rb || !dommy::bullet::isFiniteVec3(pivot)) return nullptr;
        return new DB_Nail_Constraint_t(rb, pivot);
    }
    nail_constraint_impl_t* create_nail_constraint(rigid_body_impl_t* rbA,
                                                   rigid_body_impl_t* rbB,
                                                   vec3f const& pivotInA,
                                                   vec3f const& pivotInB) override
    {
        if (!rbA || !rbB || !dommy::bullet::isFiniteVec3(pivotInA) ||
            !dommy::bullet::isFiniteVec3(pivotInB)) return nullptr;
        return new DB_Nail_Constraint_t(rbA, rbB, pivotInA, pivotInB);
    }
    hinge_constraint_impl_t* create_hinge_constraint(rigid_body_impl_t* rb,
                                                     vec3f const& pivot,
                                                     quatf const& rot) override
    {
        if (!rb || !dommy::bullet::isFiniteVec3(pivot) ||
            !dommy::bullet::hasValidQuaternion(rot)) return nullptr;
        return new DB_Hinge_Constraint_t(rb, pivot, rot);
    }
    hinge_constraint_impl_t* create_hinge_constraint(rigid_body_impl_t* rbA,
                                                     vec3f const& pivotA,
                                                     quatf const& rotA,
                                                     rigid_body_impl_t* rbB,
                                                     vec3f const& pivotB,
                                                     quatf const& rotB) override
    {
        if (!rbA || !rbB || !dommy::bullet::isFiniteVec3(pivotA) ||
            !dommy::bullet::isFiniteVec3(pivotB) ||
            !dommy::bullet::hasValidQuaternion(rotA) ||
            !dommy::bullet::hasValidQuaternion(rotB)) return nullptr;
        return new DB_Hinge_Constraint_t(rbA, pivotA, rotA, rbB, pivotB, rotB);
    }
    slider_constraint_impl_t* create_slider_constraint(rigid_body_impl_t* rb,
                                                       vec3f const& pivot,
                                                       quatf const& rot) override
    {
        if (!rb || !dommy::bullet::isFiniteVec3(pivot) ||
            !dommy::bullet::hasValidQuaternion(rot)) return nullptr;
        return new DB_Slider_Constraint_t(rb, pivot, rot);
    }
    slider_constraint_impl_t* create_slider_constraint(rigid_body_impl_t* rbA,
                                                       vec3f const& pivotA,
                                                       quatf const& rotA,
                                                       rigid_body_impl_t* rbB,
                                                       vec3f const& pivotB,
                                                       quatf const& rotB) override
    {
        if (!rbA || !rbB || !dommy::bullet::isFiniteVec3(pivotA) ||
            !dommy::bullet::isFiniteVec3(pivotB) ||
            !dommy::bullet::hasValidQuaternion(rotA) ||
            !dommy::bullet::hasValidQuaternion(rotB)) return nullptr;
        return new DB_Slider_Constraint_t(rbA, pivotA, rotA, rbB, pivotB, rotB);
    }
    sixdof_constraint_impl_t* create_sixdof_constraint(rigid_body_impl_t* rb,
                                                       vec3f const& pivot,
                                                       quatf const& rot) override
    {
        if (!rb || !dommy::bullet::isFiniteVec3(pivot) ||
            !dommy::bullet::hasValidQuaternion(rot)) return nullptr;
        return new DB_Sixdof_Constraint_t(rb, pivot, rot);
    }
    sixdof_constraint_impl_t* create_sixdof_constraint(rigid_body_impl_t* rbA,
                                                       vec3f const& pivotA,
                                                       quatf const& rotA,
                                                       rigid_body_impl_t* rbB,
                                                       vec3f const& pivotB,
                                                       quatf const& rotB) override
    {
        if (!rbA || !rbB || !dommy::bullet::isFiniteVec3(pivotA) ||
            !dommy::bullet::isFiniteVec3(pivotB) ||
            !dommy::bullet::hasValidQuaternion(rotA) ||
            !dommy::bullet::hasValidQuaternion(rotB)) return nullptr;
        return new DB_Sixdof_Constraint_t(rbA, pivotA, rotA, rbB, pivotB, rotB);
    }
    
    bool add_rigid_body(
        rigid_body_impl_t* rb, const char* name) override
    {
        (void)name;
        if (!m_dynamicsWorld) return false;
        DB_RigidBody_t* bt_body = dynamic_cast<DB_RigidBody_t*>(rb);
        if (!bt_body || !bt_body->body()) return false;
        float mass;
        vec3f inertia;
        bool kinematic;
        bt_body->get_mass_properties(mass, inertia, kinematic);
        if (!can_set_mass_properties(rb, mass, inertia, kinematic)) return false;
        if (m_dynamicsWorld->getCollisionObjectArray().findLinearSearch(
                bt_body->body()) !=
            m_dynamicsWorld->getCollisionObjectArray().size()) {
            return true;
        }
        bt_body->body()->setActivationState(DISABLE_DEACTIVATION);
        m_dynamicsWorld->addRigidBody(bt_body->body());
        return m_dynamicsWorld->getCollisionObjectArray().findLinearSearch(
                   bt_body->body()) !=
               m_dynamicsWorld->getCollisionObjectArray().size();
    }
    
    /** \brief    Add a soft body to the existing "physics world" of this solver.
     
     \param sb Pointer to an object of class soft_body_impl_t, which must also be
     of class bt_soft_body_t.  If sb is 0, nothing will happen.
     \para name Character pointer.  Does nothing at the moment.
     */
    
    bool add_soft_body(
        soft_body_impl_t* sb, const char* name) override
    {
        (void)name;
        if(sb == 0 || !m_dynamicsWorld)
        {
            return false;
        }
        DB_SoftBody_t* bt_body = dynamic_cast<DB_SoftBody_t*>(sb);
        if (!bt_body || !bt_body->belongs_to_world(m_softBodyWorldReference)) return false;
        if (m_dynamicsWorld->getCollisionObjectArray().findLinearSearch(
                bt_body->body()) !=
            m_dynamicsWorld->getCollisionObjectArray().size()) {
            return true;
        }
        bt_body->body()->setActivationState(DISABLE_DEACTIVATION);
        m_dynamicsWorld->addSoftBody(bt_body->body());
        return m_dynamicsWorld->getCollisionObjectArray().findLinearSearch(
                   bt_body->body()) !=
               m_dynamicsWorld->getCollisionObjectArray().size();
    }
    
    /** \brief Remove a soft body from the physics world.
     
     \param sb Pointer to a soft_body_impl_t object.  Must be of class bt_soft_body.
     
     */
    void remove_soft_body(soft_body_impl_t* sb) override
    {
        if (!m_dynamicsWorld) return;
        DB_SoftBody_t* bt_body = dynamic_cast<DB_SoftBody_t*>(sb);
        if (!bt_body || !bt_body->belongs_to_world(m_softBodyWorldReference)) return;
        m_dynamicsWorld->removeSoftBody(bt_body->body());
    }
    
    void remove_rigid_body(rigid_body_impl_t* rb) override
    {
        if (!m_dynamicsWorld) return;
        DB_RigidBody_t* bt_body = dynamic_cast<DB_RigidBody_t*>(rb);
        if (!bt_body || !bt_body->body()) return;
        if (m_dynamicsWorld->getCollisionObjectArray().findLinearSearch(bt_body->body())!=m_dynamicsWorld->getCollisionObjectArray().size())
        {
            m_dynamicsWorld->removeRigidBody(bt_body->body());
        }
    }
    
    bool add_constraint(
        constraint_impl_t* c, bool disableCollide) override
    {
        if (!m_dynamicsWorld) return false;
        DB_Constraint_t* btc = dynamic_cast<DB_Constraint_t*>(c);
        if (!btc || !btc->constraint()) return false;
        for (int index = 0;
             index < m_dynamicsWorld->getNumConstraints(); ++index) {
            if (m_dynamicsWorld->getConstraint(index) ==
                btc->constraint()) {
                return true;
            }
        }
        m_dynamicsWorld->addConstraint(btc->constraint(), disableCollide); //MB
        for (int index = 0;
             index < m_dynamicsWorld->getNumConstraints(); ++index) {
            if (m_dynamicsWorld->getConstraint(index) ==
                btc->constraint()) {
                return true;
            }
        }
        return false;
    }
    
    void remove_constraint(constraint_impl_t* c) override
    {
        if (!m_dynamicsWorld) return;
        DB_Constraint_t* btc = dynamic_cast<DB_Constraint_t*>(c);
        if (!btc || !btc->constraint()) return;
        m_dynamicsWorld->removeConstraint(btc->constraint());
    }
    
    /** \brief Modify the solver's global gravity level.
     
     \param g 3 element vector with the gravitational force in the 3 spacial
     directions.
     */
    bool can_set_mass_properties(rigid_body_impl_t* body, float mass,
                                 const vec3f& inertia, bool kinematic) const override
    {
        if (!body || !m_dynamicsWorld ||
            !dommy::bullet::safeBodyMassProperties(mass, inertia)) return false;
        const btVector3& g = m_dynamicsWorld->getGravity();
        return kinematic || dommy::bullet::safeMassGravity(mass, vec3f(g.x(),g.y(),g.z()));
    }

    bool set_gravity(vec3f const& g) override
    {
        if (!m_worldInfo || !m_dynamicsWorld || !dommy::bullet::isFiniteVec3(g)) return false;
        // Validate every registered rigid body before changing any gravity.
        const btCollisionObjectArray& objects = m_dynamicsWorld->getCollisionObjectArray();
        for (int i = 0; i < objects.size(); ++i) {
            const btRigidBody* body = btRigidBody::upcast(objects[i]);
            if (!body || body->isStaticOrKinematicObject()) continue;
            const float inverseMass = body->getInvMass();
            if (!dommy::bullet::isSafeInverseOperand(inverseMass) ||
                !dommy::bullet::safeMassGravity(inverseMass ? 1.0f / inverseMass : 0, g)) return false;
        }
        btVector3 btG(g[0], g[1], g[2]);
        this->m_worldInfo->m_gravity = btG;
        
        // we must modify the m_dynamicsWorld descriptor for soft-bodies as well
        m_dynamicsWorld->setGravity(btG);
        return true;
    }
    
    void set_split_impulse(bool enabled) override
    {
        if (!m_dynamicsWorld) return;
        m_dynamicsWorld->getSolverInfo().m_splitImpulse = enabled;
    }
    
    bool can_step_simulation(float dt, float fixedSeconds, int substeps) const override
    {
        return m_dynamicsWorld && m_dynamicsWorld->canAdvance(dt, fixedSeconds, substeps);
    }

    bool step_simulation(float dt, float fixedSeconds,
                         const dommy::bullet::SoftBodyForces& forces) override
    {
        return m_dynamicsWorld && m_dynamicsWorld->stepChecked(dt, fixedSeconds, forces);
    }
    
    void debug_draw(int dbgMode) override;
    
    bool export_bullet_file(const char* fileName) override;
    
    void register_name(const void* pointer, const char* objectName) override;
    
    void destroyWorld() noexcept override;
    
    void createWorld() override;
    
protected:
    friend class dbSolver_t;
    DB_BulletSolver_t();
    DB_BulletSolver_t(const DB_BulletSolver_t&) = delete;
    DB_BulletSolver_t& operator=(const DB_BulletSolver_t&) = delete;
    DB_BulletSolver_t(DB_BulletSolver_t&&) = delete;
    DB_BulletSolver_t& operator=(DB_BulletSolver_t&&) = delete;
    
    
private:
    std::unique_ptr<btDefaultCollisionConfiguration>
        m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btConstraintSolver> m_solver;
    DBulletSoftBodyWorldInfoPtr m_worldInfo;
    DBulletSoftBodyWorldReferencePtr m_softBodyWorldReference;
    std::unique_ptr<btIDebugDraw> m_debugDrawer;
    std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld> m_dynamicsWorld;
    
public:
    std::map<const void*, std::string> m_nameMap;
    
    btSoftRigidDynamicsWorld* dyanmicsWorld() {
        return m_dynamicsWorld.get();
    }
};
