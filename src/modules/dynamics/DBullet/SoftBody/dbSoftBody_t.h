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
plugin.  It is based on rigid_body.h
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/soft_body_t.h

//
//  dbSoftBody_t.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once


#include "shared_ptr.h"
#include "mathUtils.h"
#include "soft_body_impl_t.h"

// This adapter deliberately exposes btSoftBodyWorldInfo at its construction
// boundary. Direct construction without a world reference borrows worldInfo;
// the caller must keep ALL world resources alive for the body's lifetime.
// The solver factory always supplies a managed generation reference.
#include "BulletSoftBody/btSoftBody.h"
#include "DB_SoftBody_t.h"
#include <vector>

//dbSoftBody_t
class dbSoftBody_t
{
public:
    // typedefs
    
    // shared pointer type
    typedef shared_ptr<dbSoftBody_t> pointer;
    dbSoftBody_t(btSoftBodyWorldInfo &worldInfo, const std::vector<float> &triVertCoords,
                 const std::vector<int> &triVertIndices,
                 const dommy::bullet::SoftBodyParameters& parameters = dommy::bullet::SoftBodyParameters(),
                 const DBulletSoftBodyWorldReferencePtr& worldReference = DBulletSoftBodyWorldReferencePtr());
    virtual ~dbSoftBody_t(void);
    bool valid() const;
    std::size_t vertex_count() const {
        return valid() ? static_cast<std::size_t>(m_impl->body()->m_nodes.size()) : 0;
    }
    bool set_mass(float mass, bool fromFaces = false);
    bool set_parameters(const dommy::bullet::SoftBodyParameters& parameters);
    float get_total_mass();
    void apply_central_force(const vec3f& force);
    
    void set_transform(vec3f const& position, quatf const& rotation);
    void get_transform(vec3f& position, quatf& rotation) const;
    void get_transform(mat4x4f& xform) const;
    void gl_draw(std::size_t draw_type);
    void set_dynamic_friction_coeff(float dfc = 0.2f);
    void set_gravity(const vec3f & grav);
    void get_mesh(std::vector<int> &triIndices, std::vector<vec3f> &triCoords);
    /** \brief set the soft body's collision margin
     \param margin collision margin. The function does nothing if margin < 0
     */
    void set_collision_margin(float margin = 0.04);
    vec3f get_center(void);
    /** \brief Get the average velocity of each node (vertex)
     */
    vec3f get_average_velocity(void);
    
    /** \brief Set number of collision clusters.
     
     \param num Positive cluster count within the adapter's safety budgets.
     */
    bool set_num_clusters(size_t num);
    
protected:
    friend class dbSolver_t;
    soft_body_impl_t* impl(void)
    {
        return(m_impl.get());
    }
private:
    
    dbSoftBody_t(void);
    shared_ptr<DB_SoftBody_t> m_impl;
    
    
};
