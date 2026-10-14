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

Modified by Francisco Gochez <fjgochez@gmail.com>
Nov 2011 - Dec 2011 : Added logic for soft bodies
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/solver.h

//
//  dbSolver_t.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#pragma once


#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "mathUtils.h"
#include "shared_ptr.h"

#include "dbRigidBody_t.h"
#include "dbSphere_shape_t.h"
#include "dbPlane_shape_t.h"
#include "dbBox_shape_t.h"
#include "convex_hull_shape.h"
#include "dbMeshShape_t.h"
#include "solver_impl.h"
#include "dbSoftBody_t.h"
#include "composite_shape_t.h"


#include "dbHinge_constraint_t.h"
#include "dbNail_constraint_t.h"
#include "dbSixdof_constraint_t.h"
#include "dbSlider_constraint_t.h"


//solver.h solver_t
///basic class for all bullet solvers
class dbSolver_t
{
public:
    class LockedSolverAccess final {
    public:
        solver_impl_t* get() const noexcept { return m_solver.get(); }
        explicit operator bool() const noexcept { return m_solver != nullptr; }

        LockedSolverAccess(LockedSolverAccess&&) = default;
        LockedSolverAccess& operator=(LockedSolverAccess&&) = default;
        LockedSolverAccess(const LockedSolverAccess&) = delete;
        LockedSolverAccess& operator=(const LockedSolverAccess&) = delete;

    private:
        friend class dbSolver_t;
        LockedSolverAccess(std::recursive_mutex& mutex,
                           const shared_ptr<solver_impl_t>& solver)
            : m_lock(mutex), m_solver(solver) {}

        std::unique_lock<std::recursive_mutex> m_lock;
        shared_ptr<solver_impl_t> m_solver;
    };

    static void initialize();
    static void cleanup();
    
    
    
    //creation methods
    static dbSphere_shape_t::pointer create_sphere_shape(float radius = 1.0);
    
    static dbPlane_shape_t::pointer create_plane_shape(vec3f const& normal = vec3f(0, 1, 0), float d = 0);
    
    static dbBox_shape_t::pointer create_box_shape(vec3f const& halfExtents = vec3f(0.5f, 0.5f, 0.5f));
    
    static convex_hull_shape_t::pointer create_convex_hull_shape(vec3f const* vertices, size_t num_vertices,
                                                                 vec3f const* normals,
                                                                 unsigned int const *indices, size_t num_indices);
    
    static dbMeshShape_t::pointer create_mesh_shape(vec3f const* vertices, size_t num_vertices,
                                                   vec3f const* normals,
                                                   unsigned int const *indices, size_t num_indices, bool dynamicMesh);
    static dbHacdShape_t::pointer create_hacd_shape(vec3f const* vertices, size_t num_vertices,
                                                   vec3f const* normals,
                                                   unsigned int const *indices, size_t num_indices, bool dynamicMesh);
    static composite_shape_t::pointer create_composite_shape(
                                                             dbCollisionShape_t::pointer* childShapes,
                                                             vec3f* childPositions,
                                                             quatf* childOrientations,
                                                             int numChildren);
    
    static dbRigidBody_t::pointer create_rigid_body(dbCollisionShape_t::pointer& cs);
    
    static dbSoftBody_t::pointer create_soft_body(const std::vector<float> &triVertexCoords,
        const std::vector<int> &triVertexIndices,
        const dommy::bullet::SoftBodyParameters& parameters = dommy::bullet::SoftBodyParameters());
    
    static dbNail_constraint_t::pointer create_nail_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot);
    static dbNail_constraint_t::pointer create_nail_constraint(dbRigidBody_t::pointer& rbA, dbRigidBody_t::pointer& rbB, vec3f const& pivotInA, vec3f const& pivotInB);
    static dbHinge_constraint_t::pointer create_hinge_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot);
    static dbHinge_constraint_t::pointer create_hinge_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB);
    static dbSlider_constraint_t::pointer create_slider_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot);
    static dbSlider_constraint_t::pointer create_slider_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB);
    static dbSixdof_constraint_t::pointer create_sixdof_constraint(dbRigidBody_t::pointer& rb, vec3f const& pivot, quatf const& rot);
    static dbSixdof_constraint_t::pointer create_sixdof_constraint(dbRigidBody_t::pointer& rbA, vec3f const& pivotA, quatf const& rotA, dbRigidBody_t::pointer& rbB, vec3f const& pivotB, quatf const& rotB);
    
    //add/remove from world
    static bool add_rigid_body(
        const dbRigidBody_t::pointer& rb, const char* name);
    static bool add_soft_body(
        const dbSoftBody_t::pointer& sb, const char* name);
    static void remove_rigid_body(dbRigidBody_t::pointer& rb);
    static void remove_soft_body(dbSoftBody_t::pointer &sb);
    static void remove_all_rigid_bodies();
    static void remove_all_soft_bodies();
    static bool reconfigure_rigid_body(
        dbRigidBody_t::pointer& rb,
        float mass,
        const vec3f& inertia,
        bool kinematic,
        const char* name);
    static bool reconfigure_rigid_bodies(
        const std::vector<dbRigidBody_t::pointer>& bodies, float mass,
        const std::vector<vec3f>& inertia, bool kinematic, const char* name);
    
    //add/remove from world
    static bool add_constraint(
        dbConstraint_t::pointer& c, bool disableCollide = false);
    static void remove_constraint(dbConstraint_t::pointer& c);
    static void remove_all_constraints();
    
    //
    static bool set_gravity(vec3f const& g);
    
    //
    static void set_split_impulse(bool enabled);
    
    //
    static void set_collision_margin(float cm); //mb
    
    typedef std::vector<std::pair<dbSoftBody_t::pointer, vec3f>> SoftBodyForces;
    static bool can_step_simulation(float dt, float fixedSeconds, int substeps);
    static bool step_simulation(float dt, float fixedSeconds, const SoftBodyForces& forces);
    
    static void createWorld();
    
    static void destroyWorld();
    
    static void debug_draw(int dbgMode);
    
    static LockedSolverAccess lock_solver();
    
private:
    
    static std::recursive_mutex m_mutex;
    static shared_ptr<solver_impl_t> m_impl;
    static std::set<dbRigidBody_t::pointer> m_rigid_bodies;
    static std::set<dbSoftBody_t::pointer> m_soft_bodies;
    static std::set<dbConstraint_t::pointer> m_constraints;
    
};
