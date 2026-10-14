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
12/24/2009 : Nail constraint improvements

*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_plane_shape.h

//
//  DB_PlaneShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once

#include "plane_shape_impl.h"
#include "drawUtils.h"

class DB_PlaneShape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override
    {
        //    std::cout << "bt_plane_shape_t::draw" << std::endl;
        glPushMatrix();
        glScalef(100.0f, 0.001f, 100.0f);
        if(draw_style & dbCollisionShape_t::kDSSolid) {
            solid_cube();
        } else {
            wire_cube();
        }
        glPopMatrix();
    }
    
    scale_update prepare_scale(vec3f const& s, bool = false) override {
        if (!shape() || !dommy::bullet::isFiniteVector(s)) return scale_update();
        scale_update update(new collision_shape_scale_update([this, s]() {
            shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
        }));
        update->inertia = m_local_inertia;
        update->minimum = vec3f(-FLT_MAX,-FLT_MAX,-FLT_MAX);
        update->maximum = vec3f(FLT_MAX,FLT_MAX,FLT_MAX);
        return update;
    }
    
    virtual void get_scale(vec3f& s) override {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    virtual float volume() override { return m_volume;  }
    virtual vec3f const& local_inertia() override { return m_local_inertia;  }
    virtual vec3f const& center() override { return m_center; }
    virtual quatf const& rotation() override { return m_rotation;  }
    
protected:
    friend class DB_BulletSolver_t;
    
    DB_PlaneShape_t(vec3f const& normal, float d):
    DB_CollisionShape_t(),
    m_volume(0),
    m_center(0, 0, 0),
    m_rotation(mvl::qidentity<float>()),
    m_local_inertia(1, 1, 1)
    {
        set_shape(new btStaticPlaneShape(btVector3(normal[0], normal[1], normal[2]), d));
        //  shape()->setMargin(0.1);
    }
    
private:
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
    
};
