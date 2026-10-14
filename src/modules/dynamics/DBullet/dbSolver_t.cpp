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
Nov 2011 - Dec 2011 : Added logic for soft bodies
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/solver.cpp

//
//  dbSolver_t.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#include "dbSolver_t.h"

#include "DB_BulletSolver_t.h"
#include "dbSoftBody_t.h"
#include "Collision/DCollisionMeshInput.h"

#include <stdexcept>


std::recursive_mutex dbSolver_t::m_mutex;
shared_ptr<solver_impl_t> dbSolver_t::m_impl;
std::set<dbRigidBody_t::pointer> dbSolver_t::m_rigid_bodies;
std::set<dbSoftBody_t::pointer> dbSolver_t::m_soft_bodies;
std::set<dbConstraint_t::pointer> dbSolver_t::m_constraints;

dbSolver_t::LockedSolverAccess dbSolver_t::lock_solver()
{
    return LockedSolverAccess(m_mutex, m_impl);
}



void dbSolver_t::initialize()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    // Allocate first so a failed allocation leaves the existing world intact.
    shared_ptr<solver_impl_t> replacement(new DB_BulletSolver_t);
    cleanup();
    m_impl = std::move(replacement);
}

void dbSolver_t::cleanup()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if(!m_impl) {
        remove_all_constraints();
        m_soft_bodies.clear();
        m_rigid_bodies.clear();
        return;
    }
    remove_all_constraints();
    remove_all_soft_bodies();
    remove_all_rigid_bodies();
    m_impl.reset();
}

//creation methods
dbSphere_shape_t::pointer dbSolver_t::create_sphere_shape(float radius)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidSphereInput(radius)) {
        return dbSphere_shape_t::pointer();
    }
    shared_ptr<collision_shape_impl_t> shape(m_impl->create_sphere_shape(radius));
    return shape ? dbSphere_shape_t::pointer(new dbSphere_shape_t(shape))
                 : dbSphere_shape_t::pointer();
}

dbPlane_shape_t::pointer dbSolver_t::create_plane_shape(vec3f const& normal, float d)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidPlaneInput(normal, d)) {
        return dbPlane_shape_t::pointer();
    }
    shared_ptr<collision_shape_impl_t> shape(m_impl->create_plane_shape(normal, d));
    return shape ? dbPlane_shape_t::pointer(new dbPlane_shape_t(shape))
                 : dbPlane_shape_t::pointer();
}

dbBox_shape_t::pointer dbSolver_t::create_box_shape(vec3f const& halfExtents)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidBoxInput(halfExtents)) {
        return dbBox_shape_t::pointer();
    }
    shared_ptr<collision_shape_impl_t> shape(m_impl->create_box_shape(halfExtents));
    return shape ? dbBox_shape_t::pointer(new dbBox_shape_t(shape))
                 : dbBox_shape_t::pointer();
}

convex_hull_shape_t::pointer dbSolver_t::create_convex_hull_shape(vec3f const* vertices, size_t num_vertices,
                                                                vec3f const* normals,
                                                                unsigned int const *indices, size_t num_indices)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidCollisionMeshInput(
            vertices, num_vertices, normals, indices, num_indices)) {
        return convex_hull_shape_t::pointer();
    }
    shared_ptr<collision_shape_impl_t> shape(m_impl->create_convex_hull_shape(
        vertices, num_vertices, normals, indices, num_indices));
    return shape ? convex_hull_shape_t::pointer(new convex_hull_shape_t(shape))
                 : convex_hull_shape_t::pointer();
}


dbHacdShape_t::pointer dbSolver_t::create_hacd_shape(vec3f const* vertices, size_t num_vertices,
                                                  vec3f const* normals,
                                                  unsigned int const *indices, size_t num_indices, bool dynamicMesh)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    (void)dynamicMesh;
    if (!m_impl || !dommy::bullet::hasValidCollisionMeshInput(
            vertices, num_vertices, normals, indices, num_indices)) {
        return dbHacdShape_t::pointer();
    }
    shared_ptr<collision_shape_impl_t> sh(m_impl->create_hacd_shape(
        vertices, num_vertices, normals, indices, num_indices));
    if (sh)
    {
        return dbHacdShape_t::pointer(new dbHacdShape_t(sh));
    }
    return dbHacdShape_t::pointer();
    
}

composite_shape_t::pointer dbSolver_t::create_composite_shape(
                                                            dbCollisionShape_t::pointer* childShapes,
                                                            vec3f* childPositions,
                                                            quatf* childOrientations,
                                                            int numChildren)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !childShapes || !childPositions || !childOrientations ||
        numChildren <= 0) {
        return composite_shape_t::pointer();
    }
    for (int child = 0; child < numChildren; ++child) {
        if (!childShapes[child] || !childShapes[child]->impl() ||
            !dommy::bullet::isFiniteVec3(childPositions[child]) ||
            !dommy::bullet::hasValidQuaternion(childOrientations[child])) {
            return composite_shape_t::pointer();
        }
    }
    shared_ptr<collision_shape_impl_t> sh(m_impl->create_composite_shape(childShapes, childPositions, childOrientations, numChildren));
    if (sh)
    {
        return composite_shape_t::pointer(new composite_shape_t(sh));
    }
    
    return composite_shape_t::pointer();
}



dbMeshShape_t::pointer dbSolver_t::create_mesh_shape(vec3f const* vertices, size_t num_vertices,
                                                  vec3f const* normals,
                                                  unsigned int const *indices, size_t num_indices, bool dynamicMesh)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidCollisionMeshInput(
            vertices, num_vertices, normals, indices, num_indices)) {
        return dbMeshShape_t::pointer();
    }

    shared_ptr<collision_shape_impl_t> shape;
    if (dynamicMesh)
    {
        shape.reset(m_impl->create_dynamic_mesh_shape(
            vertices, num_vertices, normals, indices, num_indices));
    } else
    {
        shape.reset(m_impl->create_static_mesh_shape(
            vertices, num_vertices, normals, indices, num_indices));
    }
    return shape ? dbMeshShape_t::pointer(new dbMeshShape_t(shape))
                 : dbMeshShape_t::pointer();
}

dbRigidBody_t::pointer dbSolver_t::create_rigid_body(dbCollisionShape_t::pointer& cs)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !cs || !cs->impl()) {
        return dbRigidBody_t::pointer();
    }
    shared_ptr<rigid_body_impl_t> body(m_impl->create_rigid_body(cs->impl()));
    return body ? dbRigidBody_t::pointer(new dbRigidBody_t(body, cs))
                : dbRigidBody_t::pointer();
}

dbSoftBody_t::pointer dbSolver_t::create_soft_body(const std::vector<float> &triVertexCoords,
    const std::vector<int> &triVertexIndices, const dommy::bullet::SoftBodyParameters& parameters)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !dommy::bullet::hasValidSoftBodyMeshInput(
            triVertexCoords, triVertexIndices)) {
        return dbSoftBody_t::pointer();
    }
    return m_impl->create_soft_body(triVertexCoords, triVertexIndices, parameters);
}

dbNail_constraint_t::pointer  dbSolver_t::create_nail_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || !dommy::bullet::isFiniteVec3(pivot)) {
        return dbNail_constraint_t::pointer();
    }
    shared_ptr<nail_constraint_impl_t> constraint(m_impl->create_nail_constraint(rb->impl(), pivot));
    return constraint ? dbNail_constraint_t::pointer(new dbNail_constraint_t(constraint, rb))
                      : dbNail_constraint_t::pointer();
}
dbNail_constraint_t::pointer  dbSolver_t::create_nail_constraint(dbRigidBody_t::pointer& rbA, dbRigidBody_t::pointer& rbB, vec3f const& pivotInA, vec3f const& pivotInB)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rbA || !rbB || !rbA->impl() || !rbB->impl() ||
        !dommy::bullet::isFiniteVec3(pivotInA) ||
        !dommy::bullet::isFiniteVec3(pivotInB)) {
        return dbNail_constraint_t::pointer();
    }
    shared_ptr<nail_constraint_impl_t> constraint(m_impl->create_nail_constraint(
        rbA->impl(), rbB->impl(), pivotInA, pivotInB));
    return constraint ? dbNail_constraint_t::pointer(
                            new dbNail_constraint_t(constraint, rbA, rbB))
                      : dbNail_constraint_t::pointer();
}
dbHinge_constraint_t::pointer  dbSolver_t::create_hinge_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || !dommy::bullet::isFiniteVec3(pivot) ||
        !dommy::bullet::hasValidQuaternion(rot)) {
        return dbHinge_constraint_t::pointer();
    }
    shared_ptr<hinge_constraint_impl_t> constraint(m_impl->create_hinge_constraint(rb->impl(), pivot, rot));
    return constraint ? dbHinge_constraint_t::pointer(new dbHinge_constraint_t(constraint, rb))
                      : dbHinge_constraint_t::pointer();
}
dbHinge_constraint_t::pointer  dbSolver_t::create_hinge_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rbA || !rbB || !rbA->impl() || !rbB->impl() ||
        !dommy::bullet::isFiniteVec3(pivotA) || !dommy::bullet::isFiniteVec3(pivotB) ||
        !dommy::bullet::hasValidQuaternion(rotA) ||
        !dommy::bullet::hasValidQuaternion(rotB)) {
        return dbHinge_constraint_t::pointer();
    }
    shared_ptr<hinge_constraint_impl_t> constraint(m_impl->create_hinge_constraint(
        rbA->impl(), pivotA, rotA, rbB->impl(), pivotB, rotB));
    return constraint ? dbHinge_constraint_t::pointer(
                            new dbHinge_constraint_t(constraint, rbA, rbB))
                      : dbHinge_constraint_t::pointer();
}
dbSlider_constraint_t::pointer  dbSolver_t::create_slider_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || !dommy::bullet::isFiniteVec3(pivot) ||
        !dommy::bullet::hasValidQuaternion(rot)) {
        return dbSlider_constraint_t::pointer();
    }
    shared_ptr<slider_constraint_impl_t> constraint(m_impl->create_slider_constraint(rb->impl(), pivot, rot));
    return constraint ? dbSlider_constraint_t::pointer(new dbSlider_constraint_t(constraint, rb))
                      : dbSlider_constraint_t::pointer();
}
dbSlider_constraint_t::pointer  dbSolver_t::create_slider_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rbA || !rbB || !rbA->impl() || !rbB->impl() ||
        !dommy::bullet::isFiniteVec3(pivotA) || !dommy::bullet::isFiniteVec3(pivotB) ||
        !dommy::bullet::hasValidQuaternion(rotA) ||
        !dommy::bullet::hasValidQuaternion(rotB)) {
        return dbSlider_constraint_t::pointer();
    }
    shared_ptr<slider_constraint_impl_t> constraint(m_impl->create_slider_constraint(
        rbA->impl(), pivotA, rotA, rbB->impl(), pivotB, rotB));
    return constraint ? dbSlider_constraint_t::pointer(
                            new dbSlider_constraint_t(constraint, rbA, rbB))
                      : dbSlider_constraint_t::pointer();
}
dbSixdof_constraint_t::pointer  dbSolver_t::create_sixdof_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || !dommy::bullet::isFiniteVec3(pivot) ||
        !dommy::bullet::hasValidQuaternion(rot)) {
        return dbSixdof_constraint_t::pointer();
    }
    shared_ptr<sixdof_constraint_impl_t> constraint(m_impl->create_sixdof_constraint(rb->impl(), pivot, rot));
    return constraint ? dbSixdof_constraint_t::pointer(new dbSixdof_constraint_t(constraint, rb))
                      : dbSixdof_constraint_t::pointer();
}
dbSixdof_constraint_t::pointer  dbSolver_t::create_sixdof_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rbA || !rbB || !rbA->impl() || !rbB->impl() ||
        !dommy::bullet::isFiniteVec3(pivotA) || !dommy::bullet::isFiniteVec3(pivotB) ||
        !dommy::bullet::hasValidQuaternion(rotA) ||
        !dommy::bullet::hasValidQuaternion(rotB)) {
        return dbSixdof_constraint_t::pointer();
    }
    shared_ptr<sixdof_constraint_impl_t> constraint(m_impl->create_sixdof_constraint(
        rbA->impl(), pivotA, rotA, rbB->impl(), pivotB, rotB));
    return constraint ? dbSixdof_constraint_t::pointer(
                            new dbSixdof_constraint_t(constraint, rbA, rbB))
                      : dbSixdof_constraint_t::pointer();
}

//add/remove from world
bool dbSolver_t::add_rigid_body(
    const dbRigidBody_t::pointer& rb, const char* name)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || name == nullptr) {
        return false;
    }
    if (m_rigid_bodies.find(rb) != m_rigid_bodies.end()) {
        return true;
    }
    bool worldAttached = false;
    try {
        if (!m_impl->add_rigid_body(rb->impl(), name)) {
            return false;
        }
        worldAttached = true;
        if (!m_rigid_bodies.insert(rb).second) {
            m_impl->remove_rigid_body(rb->impl());
            return false;
        }
    } catch (...) {
        if (worldAttached) {
            m_impl->remove_rigid_body(rb->impl());
        }
        return false;
    }
    return true;
}

bool dbSolver_t::add_soft_body(
    const dbSoftBody_t::pointer& sb, const char* name)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !sb || !sb->valid() || !sb->impl() || name == nullptr) {
        return false;
    }
    if (m_soft_bodies.find(sb) != m_soft_bodies.end()) {
        return true;
    }
    bool worldAttached = false;
    try {
        if (!m_impl->add_soft_body(sb->impl(), name)) {
            return false;
        }
        worldAttached = true;
        if (!m_soft_bodies.insert(sb).second) {
            m_impl->remove_soft_body(sb->impl());
            return false;
        }
    } catch (...) {
        if (worldAttached) {
            m_impl->remove_soft_body(sb->impl());
        }
        return false;
    }
    return true;
}


void dbSolver_t::remove_rigid_body(dbRigidBody_t::pointer& rb)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!rb) {
        return;
    }

    // A constraint owns strong references to its bodies while each Bullet body
    // keeps a non-owning implementation pointer back to the constraint. Remove
    // every relationship before taking the body out of the world so neither
    // side can retain or dereference an obsolete object.
    for (;;) {
        dbConstraint_t::pointer attachedConstraint;
        for (std::set<dbConstraint_t::pointer>::const_iterator it =
                 m_constraints.begin();
             it != m_constraints.end(); ++it) {
            if ((*it)->rigid_bodyA() == rb || (*it)->rigid_bodyB() == rb) {
                attachedConstraint = *it;
                break;
            }
        }
        if (!attachedConstraint) {
            break;
        }
        remove_constraint(attachedConstraint);
    }

    const std::set<dbRigidBody_t::pointer>::iterator registered =
        m_rigid_bodies.find(rb);
    if (registered != m_rigid_bodies.end()) {
        if (m_impl && rb->impl()) {
            m_impl->remove_rigid_body(rb->impl());
        }
        m_rigid_bodies.erase(registered);
    }
}

void dbSolver_t::remove_soft_body(dbSoftBody_t::pointer &sb)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!sb) {
        return;
    }
    const std::set<dbSoftBody_t::pointer>::iterator registered =
        m_soft_bodies.find(sb);
    if (registered != m_soft_bodies.end()) {
        if (m_impl && sb->impl()) {
            m_impl->remove_soft_body(sb->impl());
        }
        m_soft_bodies.erase(registered);
    }
}

void dbSolver_t::remove_all_rigid_bodies()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    remove_all_constraints();
    if (!m_impl) {
        m_rigid_bodies.clear();
        return;
    }
    std::set<dbRigidBody_t::pointer>::iterator it;
    for(it = m_rigid_bodies.begin(); it != m_rigid_bodies.end(); ++it) {
        if (*it && (*it)->impl()) {
            m_impl->remove_rigid_body((*it)->impl());
        }
    }
    m_rigid_bodies.clear();
}

void dbSolver_t::remove_all_soft_bodies()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl) {
        m_soft_bodies.clear();
        return;
    }
    std::set<dbSoftBody_t::pointer>::iterator it;
    for(it = m_soft_bodies.begin(); it != m_soft_bodies.end(); ++it) {
        if (*it && (*it)->impl()) {
            m_impl->remove_soft_body((*it)->impl());
        }
    }
    m_soft_bodies.clear();
}

bool dbSolver_t::reconfigure_rigid_body(
    dbRigidBody_t::pointer& rb,
    float mass,
    const vec3f& inertia,
    bool kinematic,
    const char* name)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !rb || !rb->impl() || name == nullptr ||
        m_rigid_bodies.find(rb) == m_rigid_bodies.end() ||
        !m_impl->can_set_mass_properties(rb->impl(), mass, inertia, kinematic)) {
        return false;
    }

    float previousMass;
    vec3f previousInertia;
    bool previousKinematic;
    rb->get_mass_properties(previousMass, previousInertia, previousKinematic);
    if (!m_impl->can_set_mass_properties(rb->impl(), previousMass, previousInertia, previousKinematic)) return false;
    const bool classificationChanged = previousKinematic != kinematic ||
        (previousMass == 0) != (mass == 0);
    if (!classificationChanged) return rb->set_mass_properties(mass, inertia, kinematic);

    m_impl->remove_rigid_body(rb->impl());
    try {
        if (!rb->set_mass_properties(mass, inertia, kinematic))
            throw std::runtime_error("Unsafe Bullet mass properties");
        if (!m_impl->add_rigid_body(rb->impl(), name)) {
            throw std::runtime_error("Unable to re-add Bullet rigid body");
        }
    } catch (...) {
        try {
            m_impl->remove_rigid_body(rb->impl());
            if (!rb->set_mass_properties(previousMass, previousInertia, previousKinematic) ||
                !m_impl->add_rigid_body(rb->impl(), name)) m_rigid_bodies.erase(rb);
        } catch (...) {
            m_rigid_bodies.erase(rb);
        }
        return false;
    }
    return true;
}

bool dbSolver_t::reconfigure_rigid_bodies(
    const std::vector<dbRigidBody_t::pointer>& bodies, float mass,
    const std::vector<vec3f>& inertia, bool kinematic, const char* name)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !name || bodies.size() != inertia.size()) return false;
    struct Previous { float mass; vec3f inertia; bool kinematic; };
    std::vector<Previous> previous(bodies.size());
    std::set<dbRigidBody_t::pointer> unique;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!bodies[i] || !bodies[i]->impl() || !unique.insert(bodies[i]).second ||
            m_rigid_bodies.find(bodies[i]) == m_rigid_bodies.end() ||
            !m_impl->can_set_mass_properties(bodies[i]->impl(), mass, inertia[i], kinematic)) return false;
        bodies[i]->get_mass_properties(previous[i].mass, previous[i].inertia, previous[i].kinematic);
        if (!m_impl->can_set_mass_properties(bodies[i]->impl(), previous[i].mass,
                previous[i].inertia, previous[i].kinematic)) return false;
    }
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        dbRigidBody_t::pointer body = bodies[i];
        if (!reconfigure_rigid_body(body, mass, inertia[i], kinematic, name)) {
            for (std::size_t j = 0; j < i; ++j) {
                body = bodies[j];
                (void)reconfigure_rigid_body(body, previous[j].mass, previous[j].inertia,
                                            previous[j].kinematic, name);
            }
            return false;
        }
    }
    return true;
}

bool dbSolver_t::add_constraint(
    dbConstraint_t::pointer& c, bool disableCollide)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !c || !c->impl()) {
        return false;
    }
    if (m_constraints.find(c) != m_constraints.end()) {
        return true;
    }

    DB_Constraint_t* const constraintImpl =
        dynamic_cast<DB_Constraint_t*>(c->pubImpl());
    dbRigidBody_t::pointer rigidBodyA = c->rigid_bodyA();
    dbRigidBody_t::pointer rigidBodyB = c->rigid_bodyB();
    if (constraintImpl == nullptr || !rigidBodyA ||
        !rigidBodyA->impl() ||
        (rigidBodyB && !rigidBodyB->impl()) ||
        m_rigid_bodies.find(rigidBodyA) == m_rigid_bodies.end() ||
        (rigidBodyB &&
         m_rigid_bodies.find(rigidBodyB) == m_rigid_bodies.end())) {
        return false;
    }

    bool bodyAAttached = false;
    bool bodyBAttached = false;
    bool worldAttached = false;
    try {
        rigidBodyA->add_constraint(constraintImpl, disableCollide);
        bodyAAttached = true;
        if (rigidBodyB && rigidBodyB != rigidBodyA) {
            rigidBodyB->add_constraint(constraintImpl, disableCollide);
            bodyBAttached = true;
        }

        if (!m_impl->add_constraint(c->impl(), disableCollide)) {
            throw std::runtime_error("Unable to add Bullet constraint");
        }
        worldAttached = true;
        if (!m_constraints.insert(c).second) {
            m_impl->remove_constraint(c->impl());
            if (bodyBAttached) {
                rigidBodyB->remove_constraint(constraintImpl);
            }
            if (bodyAAttached) {
                rigidBodyA->remove_constraint(constraintImpl);
            }
            return false;
        }
    } catch (...) {
        if (worldAttached) {
            m_impl->remove_constraint(c->impl());
        }
        if (bodyBAttached) {
            rigidBodyB->remove_constraint(constraintImpl);
        }
        if (bodyAAttached) {
            rigidBodyA->remove_constraint(constraintImpl);
        }
        return false;
    }
    return true;
}

void dbSolver_t::remove_constraint(dbConstraint_t::pointer& c)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!c) {
        return;
    }

    const std::set<dbConstraint_t::pointer>::iterator registered =
        m_constraints.find(c);
    if (registered != m_constraints.end()) {
        if (m_impl && c->impl()) {
            m_impl->remove_constraint(c->impl());
        }
        m_constraints.erase(registered);
    }

    DB_Constraint_t* const constraintImpl =
        dynamic_cast<DB_Constraint_t*>(c->pubImpl());
    if (constraintImpl == nullptr) {
        return;
    }

    dbRigidBody_t::pointer rigidBodyA = c->rigid_bodyA();
    dbRigidBody_t::pointer rigidBodyB = c->rigid_bodyB();
    if (rigidBodyA && rigidBodyA->impl()) {
        rigidBodyA->remove_constraint(constraintImpl);
    }
    if (rigidBodyB && rigidBodyB != rigidBodyA && rigidBodyB->impl()) {
        rigidBodyB->remove_constraint(constraintImpl);
    }
}

void dbSolver_t::remove_all_constraints()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    while (!m_constraints.empty()) {
        dbConstraint_t::pointer constraint = *m_constraints.begin();
        remove_constraint(constraint);
    }
}

bool dbSolver_t::set_gravity(vec3f const& g)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (m_impl && dommy::bullet::isFiniteVec3(g)) {
        return m_impl->set_gravity(g);
    }
    return false;
}

void dbSolver_t::set_split_impulse(bool enabled)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (m_impl) {
        m_impl->set_split_impulse(enabled);
    }
}

void dbSolver_t::destroyWorld()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
	if(!m_impl) return;
	remove_all_constraints();
	remove_all_soft_bodies();
	remove_all_rigid_bodies();
    m_impl->destroyWorld();
}

void dbSolver_t::createWorld()
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (m_impl) {
        m_impl->createWorld();
    }
}

bool dbSolver_t::can_step_simulation(float dt, float fixedSeconds, int substeps)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    return m_impl && m_impl->can_step_simulation(dt, fixedSeconds, substeps);
}

bool dbSolver_t::step_simulation(float dt, float fixedSeconds, const SoftBodyForces& forces)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (!m_impl || !m_impl->can_step_simulation(dt, fixedSeconds, 1)) return false;
    dommy::bullet::SoftBodyForces nativeForces;
    nativeForces.reserve(forces.size());
    for (const auto& item : forces) {
        if (!item.first || m_soft_bodies.count(item.first) == 0 ||
            !dommy::bullet::isFiniteVec3(item.second)) return false;
        DB_SoftBody_t* body = dynamic_cast<DB_SoftBody_t*>(item.first->impl());
        if (!body || !body->body()) return false;
        nativeForces.push_back({body->body(), btVector3(item.second[0], item.second[1], item.second[2])});
    }
    return m_impl->step_simulation(dt, fixedSeconds, nativeForces);
}


void dbSolver_t::debug_draw(int dbgMode)
{
    std::lock_guard<std::recursive_mutex> stateLock(m_mutex);
    if (m_impl) {
        m_impl->debug_draw(dbgMode);
    }
}
