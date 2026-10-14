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
plugin.  It is based on rigid_body.cpp
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/soft_body_t.cpp

//
//  dbSoftBody_t.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "dbSoftBody_t.h"

#include "BulletSoftBody/btSoftBody.h"

dbSoftBody_t::dbSoftBody_t(void):
m_impl(0)
{
}


dbSoftBody_t::dbSoftBody_t(btSoftBodyWorldInfo &worldInfo, const std::vector<float> &triVertCoords,
                         const std::vector<int> &triVertIndices,
                         const dommy::bullet::SoftBodyParameters& parameters,
                         const DBulletSoftBodyWorldReferencePtr& worldReference)
{
    m_impl.reset(new DB_SoftBody_t(worldInfo, triVertCoords, triVertIndices, parameters, worldReference));
    if (!m_impl->body()) {
        m_impl.reset();
    }
}

bool dbSoftBody_t::valid() const
{
    return m_impl && m_impl->body();
}

void dbSoftBody_t::gl_draw(std::size_t draw_type)
{
    if (m_impl) this->m_impl->gl_draw(draw_type);
}

bool dbSoftBody_t::set_mass(float mass, bool fromFaces)
{
    return m_impl && m_impl->set_mass(mass, fromFaces);
}

bool dbSoftBody_t::set_parameters(const dommy::bullet::SoftBodyParameters& parameters)
{
    return m_impl && m_impl->set_parameters(parameters);
}

void dbSoftBody_t::set_transform(vec3f const& position, quatf const& rotation)
{
    if (m_impl) m_impl->set_transform(position, rotation);
}

void dbSoftBody_t::get_transform(vec3f& position, quatf& rotation) const
{
    if (m_impl) m_impl->get_transform(position, rotation);
}
void dbSoftBody_t::get_transform(mat4x4f& xform) const
{
    if (m_impl) m_impl->get_transform(xform);
}

void dbSoftBody_t::get_mesh(std::vector<int> &triIndices, std::vector<vec3f> &triCoords)
{
    if (m_impl) {
        this->m_impl->get_mesh(triIndices, triCoords);
    } else {
        triIndices.clear();
        triCoords.clear();
    }
}

void dbSoftBody_t::set_dynamic_friction_coeff(float dfc)
{
    if (m_impl) this->m_impl->set_dynamic_friction_coeff(dfc);
}

vec3f dbSoftBody_t::get_center(void)
{
    return m_impl ? this->m_impl->get_center() : vec3f(0.0f, 0.0f, 0.0f);
}

vec3f dbSoftBody_t::get_average_velocity(void)
{
    return m_impl ? this->m_impl->get_average_velocity() : vec3f(0.0f, 0.0f, 0.0f);
}

void dbSoftBody_t::apply_central_force(const vec3f& force)
{
    if (m_impl) this->m_impl->apply_central_force(force);
}


float dbSoftBody_t::get_total_mass(void)
{
    return m_impl ? this->m_impl->get_total_mass() : 0.0f;
}

void dbSoftBody_t::set_gravity(const vec3f & grav)
{
    if (m_impl) this->m_impl->set_gravity(grav);
}

bool dbSoftBody_t::set_num_clusters(std::size_t num)
{
    return m_impl && m_impl->set_num_clusters(num);
}

void dbSoftBody_t::set_collision_margin(float margin)
{
    if (m_impl) this->m_impl->set_collision_margin(margin);
}

dbSoftBody_t::~dbSoftBody_t(void)
{
}
