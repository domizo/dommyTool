// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Herbert Law
 
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
 
Written by: Herbert Law <Herbert.Law@gmail.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/slider_constraint.h

//
//  dbSlider_constraint.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//




#pragma once


#include "shared_ptr.h"
#include "dbRigidBody_t.h"
#include "mathUtils.h"

#include "dbConstraint_t.h"
#include "slider_constraint_impl.h"


class dbSlider_constraint_t: public dbConstraint_t
{
public:
    //typedefs
    typedef shared_ptr<dbSlider_constraint_t> pointer;
    
    //
    void set_damping(float d) {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        slider_impl->set_damping(d);
    }
    
    float damping() const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        return slider_impl->damping();
    }
    
    void set_breakThreshold(float d) {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        slider_impl->set_breakThreshold(d);
    }
    
    void set_enabled(bool e) {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        slider_impl->set_enabled(e);
    }
    
    void set_LinLimit(float lower, float upper) {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        slider_impl->set_LinLimit(lower, upper);
    }
    
    void set_AngLimit(float lower, float upper) {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        slider_impl->set_AngLimit(lower, upper);
    }
    
    void get_frameA(vec3f& p, quatf& r) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->get_frameA(p, r);
    }
    void get_frameB(vec3f& p, quatf& r) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->get_frameB(p, r);
    }
    void get_invFrameA(vec3f& p, quatf& r) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->get_invFrameA(p, r);
    }
    void get_invFrameB(vec3f& p, quatf& r) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->get_invFrameB(p, r);
    }
    void worldToA(vec3f& w, vec3f& p) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->worldToA(w, p);
    }
    void worldFromB(vec3f& p, vec3f& w) const {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->worldFromB(p, w);
    }
    void get_local_frameA(vec3f& p, quatf& r) const
    {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl ->get_local_frameA(p,r);
    }
    
    void get_local_frameB(vec3f& p, quatf& r) const
    {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl ->get_local_frameB(p,r);
    }
    
    bool set_world(vec3f const& p, quatf const& r)
    {
        slider_constraint_impl_t* slider_impl = dynamic_cast<slider_constraint_impl_t*>(impl());
        return slider_impl->set_world(p, r);
    }
    
    //local space pivot
    void get_world(vec3f& p, quatf& r) const  {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->get_world(p, r);
    }
    
    
    virtual void register_name(solver_impl_t* solver, const char* objectName)
    {
        slider_constraint_impl_t const* slider_impl = dynamic_cast<slider_constraint_impl_t const*>(impl());
        slider_impl->register_name(solver,objectName);
    }
    
public:
    virtual ~dbSlider_constraint_t() {};
    
protected:
    friend class dbSolver_t;
    dbSlider_constraint_t(const shared_ptr<slider_constraint_impl_t>& impl, dbRigidBody_t::pointer& rigid_body):
    dbConstraint_t(impl, rigid_body)
    {
    }
    dbSlider_constraint_t(const shared_ptr<slider_constraint_impl_t>& impl, dbRigidBody_t::pointer& rigid_bodyA, dbRigidBody_t::pointer& rigid_bodyB):
    dbConstraint_t(impl, rigid_bodyA, rigid_bodyB)
    {
    }
};


