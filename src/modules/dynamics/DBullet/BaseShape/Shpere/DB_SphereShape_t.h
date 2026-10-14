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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_sphere_shape.h

//
//  DB_SphereShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once

#ifdef WIN32//for glut.h
#include <windows.h>
#endif

//think different
#if defined(__APPLE__) && !defined (VMDMESA)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#endif


#include "sphere_shape_impl.h"
#include "DB_CollisionShape_t.h"
#include "dbCollisionShape_t.h"
#include "drawUtils.h"

class DB_SphereShape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override {
        btSphereShape *sphere_shape = static_cast<btSphereShape*>(shape());
        glPushMatrix();
        glScalef(sphere_shape->getRadius(), sphere_shape->getRadius(), sphere_shape->getRadius());
        if(draw_style & dbCollisionShape_t::kDSSolid) {
            solid_sphere();
        } else {
            wire_sphere();
        }
        glPopMatrix();
    }
    
    scale_update prepare_scale(vec3f const& s, bool = false) override {
        if (!shape() || !dommy::bullet::isFiniteVector(s)) return scale_update();
        vec3f scaled;
        if (!dommy::bullet::scaledCollisionPoint(vec3f(m_sourceRadius,m_sourceRadius,m_sourceRadius), s, scaled)) return scale_update();
        // Preserve Bullet's sphere contract: only the X magnitude sets radius.
        // Nonuniform scaling does not turn this primitive into an ellipsoid.
        const double radius = std::abs(static_cast<double>(scaled[0]));
        const double volume = (4.0 * 3.1415926 * radius * radius * radius) / 3.0;
        const double inertia = 0.4 * radius * radius;
        if (!std::isfinite(volume) || volume > std::numeric_limits<float>::max() ||
            !std::isfinite(inertia) || inertia > std::numeric_limits<float>::max() ||
            (volume != 0 && static_cast<float>(volume) == 0) ||
            (inertia != 0 && static_cast<float>(inertia) == 0)) return scale_update();
        scale_update update(new collision_shape_scale_update([this, s, volume, inertia]() {
            shape()->setLocalScaling(btVector3(s[0],s[1],s[2]));
            m_cachedScaling = s;
            m_volume = static_cast<float>(volume);
            m_local_inertia = vec3f(inertia,inertia,inertia);
        }));
        update->minimum = vec3f(-radius,-radius,-radius);
        update->maximum = vec3f(radius,radius,radius);
        update->volume = static_cast<float>(volume);
        update->inertia = vec3f(inertia,inertia,inertia);
        return update;
    }
    
    virtual void get_scale(vec3f& s) override {
        s = m_cachedScaling;
    }
    
    virtual float volume() override { return m_volume;  }
    virtual vec3f const& local_inertia() override { return m_local_inertia;  }
    virtual vec3f const& center() override { return m_center; }
    virtual quatf const& rotation() override { return m_rotation;  }
    
protected:
    friend class DB_BulletSolver_t;
    
    DB_SphereShape_t(float radius):
    DB_CollisionShape_t(), m_sourceRadius(radius), m_cachedScaling(1,1,1),
    m_volume(0), m_center(0,0,0), m_rotation(mvl::qidentity<float>()), m_local_inertia(0,0,0)
    {
        if (!dommy::bullet::isSafeCollisionCoordinate(radius) || radius <= 0) return;
        set_shape(new btSphereShape(radius));
        set_scale(vec3f(1,1,1));
    }
    
private:
    float m_sourceRadius;
    vec3f m_cachedScaling;
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
    
};
