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
// Archive member: dynamica/trunk/Extras/MayaPlugin/collision_shape.h

//
//  DB_CollisionShape.h
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#pragma once



#include "shared_ptr.h"
#include "collision_shape_impl.h"

//collision_shape.h collision_shape_t
class dbCollisionShape_t
{
public:
    //typedefs
    typedef shared_ptr<dbCollisionShape_t> pointer;
    
    //enums
    enum DrawStyle {
        kDSWireframe = 0x0001,
        kDSSolid = 0x0002
    };
    
    virtual void gl_draw(size_t draw_style = kDSSolid) { m_impl->gl_draw(draw_style); }
    
    bool set_scale(vec3f const& s) { return m_impl->set_scale(s); }
    collision_shape_impl_t::scale_update prepare_scale(vec3f const& s, bool force = false) {
        return m_impl->prepare_scale(s, force);
    }
    virtual void get_scale(vec3f& s)        { m_impl->get_scale(s);  }
    
    virtual float volume()               { return m_impl->volume();  }
    //for the inertia is assumed that the mass is 1.0. just multiply by the mass
    virtual vec3f const& local_inertia() { return m_impl->local_inertia();  }
    virtual vec3f const& center()        { return m_impl->center();   }
    virtual quatf const& rotation()      { return m_impl->rotation();   }
    
public:
    virtual ~dbCollisionShape_t() {}
    
    class btCollisionShape* getBulletCollisionShape()
    {
        return m_impl->getBulletCollisionShape();
    }
protected:
    friend class dbSolver_t;
    
    dbCollisionShape_t(const shared_ptr<collision_shape_impl_t>& impl): m_impl(impl) { }
    
    collision_shape_impl_t* impl()              { return m_impl.get(); }
    
private:
    shared_ptr<collision_shape_impl_t> m_impl;
};

