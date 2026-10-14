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
 
This file was written by Francisco Gochez <fjgochez@gmail.com>, and was not part of the original
plugin.  It is based on bt_rigid_body.h
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_soft_body.h

//
//  DB_SoftBody_t.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#pragma once



#include <iterator>
#include <vector>

#include "soft_body_impl_t.h"
#include "shared_ptr.h"
#include "mathUtils.h"
#include "DSoftBodyPreparation.h"
#include "DBulletResourceOwnership.h"

#include "LinearMath/btAlignedObjectArray.h"
#include "BulletSoftBody/btSoftBody.h"

/** \brief Implementation of the soft_body_impl_t interface based on Bullet soft bodies
 */
class DB_SoftBody_t : public soft_body_impl_t
{
public:
    //! Smart pointer type
    typedef shared_ptr<DB_SoftBody_t> pointer;
    
    /** \brief Build a soft body using bounded sparse mesh and bending topology.
     \see dbSolver_t::create_soft_body
     
     \param worldInfo World information that must outlive a borrowed body.
     \param triVertexCoords "Unrolled" set of triangle vertex coordinates
     \param triVertexIndices Triangle integer indices into the vertex coordinates
     \param collisionMargin collision margin
     */
    DB_SoftBody_t(btSoftBodyWorldInfo &worldInfo,const std::vector<float> &triVertexCoords,
                 const std::vector<int> &triVertexIndices,
                 float collisionMargin = 0.04f);
    DB_SoftBody_t(btSoftBodyWorldInfo& worldInfo, const std::vector<float>& coordinates,
                 const std::vector<int>& indices, const dommy::bullet::SoftBodyParameters& parameters,
                 const DBulletSoftBodyWorldReferencePtr& worldReference = DBulletSoftBodyWorldReferencePtr());
    virtual ~DB_SoftBody_t(void);
    // Raw access is borrowed for the current world generation only. Callers
    // must serialize world teardown and must not cache the returned pointer.
    btSoftBody* body() const;
    bool belongs_to_world(const DBulletSoftBodyWorldReferencePtr& reference) const;
    
    virtual void get_transform(mat4x4f &xForm) const;
    
    /** \brief Set total mass
     
     \param mass Float with total mass
     \param fromFaces Recalculate the mass distribution from current face areas.
     */
    virtual bool set_mass(float mass, bool fromFaces = false);
    bool set_parameters(const dommy::bullet::SoftBodyParameters& parameters);
    /** \brief set the gravity level for this soft body.
     
     Soft bodies each seem to have their own gravity level independent of the solver's
     global setting.
     
     \param grav reference to vec3f with the gravity level
     
     */
    
    virtual void set_gravity(const vec3f & grav);
    /** \brief Set number of collision clusters
     
     \param num Positive cluster count within the adapter's safety budgets.
     \see btSoftBody::generateClusters
     
     */
    virtual bool set_num_clusters(size_t num);
    
    virtual float get_total_mass(void);
    virtual void set_dynamic_friction_coeff(float f);
    virtual void apply_central_force(const vec3f& force);
    virtual void set_transform(vec3f const& position, quatf const& rotation);
    virtual void get_transform(vec3f& position, quatf& rotation) const;
    virtual void gl_draw(std::size_t draw_style);
    virtual vec3f get_center();
    virtual vec3f get_average_velocity();
    virtual void set_collision_margin(float margin = 0.04);
    
    friend class DB_BulletSolver_t;
    /** \brief Extracts the current mesh configuration of the soft body into "generic" std::vector objects
     
     \param triIndices Reference to a std::vector<int> object which will be overwritten by triangle vertex indices
     \param triCoords Reference to a std::vector<vec3f> object which will be overwritten by triangle vertex coordinates
     
     */
    virtual void get_mesh(std::vector<int> &triIndices, std::vector<vec3f> &triCoords);
private:
    bool set_mass_and_clusters(float mass, std::size_t clusters, bool fromFaces);
    DB_SoftBody_t(void);
    DB_SoftBody_t(const DB_SoftBody_t&) = delete;
    DB_SoftBody_t& operator=(const DB_SoftBody_t&) = delete;
    shared_ptr<btSoftBody> m_body;
    std::weak_ptr<DBulletSoftBodyWorldReference> m_worldReference;
    bool m_managedWorld = false;
    std::vector<double> m_massWeights;
    float m_totalMass = 0;
    std::size_t m_clusterCount = 0;
    
    // Original mesh input; indices also preserve caller-facing output topology.
    std::vector<btScalar> triVertCoords;
    std::vector<int> triVertIndices;
    
    int numTriangles;
    int numVertices;
};
