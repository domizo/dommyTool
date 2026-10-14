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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/nail_constraint.h

//
//  dbNail_constraint.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once


#include "shared_ptr.h"
#include "dbRigidBody_t.h"
#include "mathUtils.h"

#include "dbConstraint_t.h"
#include "nail_constraint_impl.h"


class dbNail_constraint_t: public dbConstraint_t
{
public:
    //typedefs
    typedef shared_ptr<dbNail_constraint_t> pointer;
    
    //local space pivots
    void set_pivotA(vec3f const& p) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        nail_impl->set_pivotA(p);
    }
    void get_pivotA(vec3f& p) const {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->get_pivotA(p);
    }
    void set_pivotB(vec3f const& p) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        nail_impl->set_pivotB(p);
    }
    void get_pivotB(vec3f& p) const {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->get_pivotB(p);
    }
    //world space pivots
    void get_world_pivotA(vec3f& p) const {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->get_world_pivotA(p);
    }
    void get_world_pivotB(vec3f& p) const {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->get_world_pivotB(p);
    }
    
    //
    bool set_world(vec3f const& p) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        return nail_impl->set_world(p);
    }
    
    //
    void get_world(vec3f& p) const  {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->get_world(p);
    }
    
    //
    void set_damping(float d) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        nail_impl->set_damping(d);
    }
    
    float damping() const {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        return nail_impl->damping();
    }
    
    void set_breakThreshold(float d) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        nail_impl->set_breakThreshold(d);
    }
    
    void set_enabled(bool e) {
        nail_constraint_impl_t* nail_impl = dynamic_cast<nail_constraint_impl_t*>(impl());
        nail_impl->set_enabled(e);
    }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName)
    {
        nail_constraint_impl_t const* nail_impl = dynamic_cast<nail_constraint_impl_t const*>(impl());
        nail_impl->register_name(solver,objectName);
    }
    
public:
    virtual ~dbNail_constraint_t() {};
    
protected:
    friend class dbSolver_t;
    dbNail_constraint_t(const shared_ptr<nail_constraint_impl_t>& impl, dbRigidBody_t::pointer& rigid_bodyA):
    dbConstraint_t(impl, rigid_bodyA)
    {
    }
    
    dbNail_constraint_t(const shared_ptr<nail_constraint_impl_t>& impl, dbRigidBody_t::pointer& rigid_bodyA, dbRigidBody_t::pointer& rigid_bodyB):
    dbConstraint_t(impl, rigid_bodyA, rigid_bodyB)
    {
    }
};
