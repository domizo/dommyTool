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
// Archive member: dynamica/trunk/Extras/MayaPlugin/collision_shape_impl.h

//
//  collision_shape_impl.h
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#pragma once



#include "shared_ptr.h"
#include "mathUtils.h"
#include <functional>
#include <memory>
#include <utility>

// A prepared change owns all fallible C++ staging allocations. Discarding it
// leaves the live shape intact. Commit while holding the solver's existing
// lock, with no intervening shape mutation. The transaction below may also
// prepare a forced restoration of the current state before its provisional
// commit. The shape must outlive both prepared updates.
struct collision_shape_scale_update
{
    explicit collision_shape_scale_update(std::function<void()> action)
        : volume(0), inertia(0,0,0), minimum(0,0,0), maximum(0,0,0),
          m_action(std::move(action)) {}
    collision_shape_scale_update(const collision_shape_scale_update&) = delete;
    collision_shape_scale_update& operator=(const collision_shape_scale_update&) = delete;
    void commit() {
        std::function<void()> action;
        action.swap(m_action); // A prepared update can be committed only once.
        if (action) action();
    }
    float volume;
    vec3f inertia; // Exact unit inertia, or a conservative bound for compounds.
    vec3f minimum, maximum; // Conservative local-space bounds after commit.
private:
    std::function<void()> m_action;
};

class collision_shape_impl_t
{
public:
    //
    virtual void gl_draw(size_t draw_style) = 0;
    
    typedef std::unique_ptr<collision_shape_scale_update> scale_update;
    virtual scale_update prepare_scale(vec3f const& s, bool force = false) = 0;
    bool set_scale(vec3f const& s) {
        scale_update update = prepare_scale(s);
        if (!update) return false;
        update->commit();
        return true;
    }
    virtual void get_scale(vec3f& s) = 0;
    
    virtual float volume() = 0;
    virtual vec3f const& local_inertia() = 0;
    virtual vec3f const& center() = 0;
    virtual quatf const& rotation() = 0;
    
public:
    virtual ~collision_shape_impl_t() {};
    virtual class btCollisionShape* getBulletCollisionShape() = 0;
    virtual void getCenterOfMassTransformShift(class btTransform& shiftTransform) = 0;
    
};

// Prepare restoration before mutation, then inspect the actual inertia while
// holding the solver lock. Native Bullet OOM recovery is not guaranteed.
template<class Shape>
class collision_shape_scale_transaction
{
public:
    collision_shape_scale_transaction(Shape& shape, const vec3f& scale) {
        auto update = shape.prepare_scale(scale);
        if (!update) return;
        vec3f previous;
        shape.get_scale(previous);
        auto restore = shape.prepare_scale(previous, true);
        if (!restore) return;
        update->commit();
        m_restore = std::move(restore);
        m_valid = true;
    }
    ~collision_shape_scale_transaction() { if (m_restore) m_restore->commit(); }
    explicit operator bool() const { return m_valid; }
    void accept() { m_restore.reset(); }
    collision_shape_scale_transaction(const collision_shape_scale_transaction&) = delete;
    collision_shape_scale_transaction& operator=(const collision_shape_scale_transaction&) = delete;
private:
    collision_shape_impl_t::scale_update m_restore;
    bool m_valid = false;
};
