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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_box_shape.h

//
//  DB_BoxShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//



#pragma once


#include "box_shape_impl.h"
#include "drawUtils.h"
#include "DB_CollisionShape_t.h"

// Apply absolute dimensions without Bullet's division by the previous scale.
class DB_AbsoluteBoxShape : public btBoxShape
{
public:
    explicit DB_AbsoluteBoxShape(const btVector3& halfExtents) : btBoxShape(halfExtents) {}
    void applyDimensions(const btVector3& halfExtents, const btVector3& scale, btScalar margin) {
        m_localScaling = scale.absolute();
        m_collisionMargin = margin;
        m_implicitShapeDimensions = halfExtents - btVector3(margin,margin,margin);
    }
};

class DB_BoxShape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override
    {
        //      std::cout << "DB_BoxShape_t::draw" << std::endl;
        btBoxShape *box_shape = static_cast<btBoxShape*>(shape());
        btVector3 const& e = box_shape->getHalfExtentsWithoutMargin();
        glPushMatrix();
        glScalef(2 * e.x(), 2 * e.y(), 2 * e.z());
        if(draw_style & dbCollisionShape_t::kDSSolid) {
            solid_cube();
        } else {
            wire_cube();
        }
        glPopMatrix();
    }
    
    scale_update prepare_scale(vec3f const& s, bool = false) override {
        if (!shape() || !dommy::bullet::isFiniteVector(s)) return scale_update();
        vec3f scaled;
        if (!dommy::bullet::scaledCollisionPoint(m_sourceHalfExtents, s, scaled)) return scale_update();
        const btVector3 half(std::abs(scaled[0]),std::abs(scaled[1]),std::abs(scaled[2]));
        const float margin = btMin(m_sourceMargin, btMin(half.x(),btMin(half.y(),half.z())));
        const btVector3 core = half - btVector3(margin,margin,margin);
        float volume;
        vec3f inertia;
        if (!dommy::bullet::boxMassFromDimensions(vec3f(2*core.x(),2*core.y(),2*core.z()), volume, inertia)) return scale_update();
        scale_update update(new collision_shape_scale_update([this, s, half, margin, volume, inertia]() {
            static_cast<DB_AbsoluteBoxShape*>(shape())->applyDimensions(half, btVector3(s[0],s[1],s[2]), margin);
            m_cachedScaling = s;
            m_volume = volume;
            m_local_inertia = inertia;
        }));
        update->minimum = vec3f(-half.x(),-half.y(),-half.z());
        update->maximum = vec3f(half.x(),half.y(),half.z());
        update->volume = volume;
        update->inertia = inertia;
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
    
    DB_BoxShape_t(vec3f const& halfExtents):
    DB_CollisionShape_t(), m_sourceHalfExtents(halfExtents), m_sourceMargin(0),
    m_cachedScaling(1,1,1), m_volume(0), m_center(0,0,0),
    m_rotation(mvl::qidentity<float>()), m_local_inertia(0,0,0)
    {
        float volume;
        vec3f inertia;
        if (!dommy::bullet::boxMassFromDimensions(2.0f * halfExtents, volume, inertia)) return;
        set_shape(new DB_AbsoluteBoxShape(btVector3(halfExtents[0], halfExtents[1], halfExtents[2])));
        m_sourceMargin = shape()->getMargin();
        set_scale(vec3f(1,1,1));
    }
    
private:
    vec3f m_sourceHalfExtents;
    float m_sourceMargin;
    vec3f m_cachedScaling;
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
};
