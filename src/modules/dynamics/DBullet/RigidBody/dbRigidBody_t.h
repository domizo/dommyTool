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
// Archive member: dynamica/trunk/Extras/MayaPlugin/rigid_body.h

//
//  dbRigidBody_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#pragma once




#include "shared_ptr.h"
#include "dbCollisionShape_t.h"
#include "mathUtils.h"

#include "rigid_body_impl.h"


class solver_impl_t;


//rigid_body_t
class dbRigidBody_t
{
public:
    //typedefs
    typedef shared_ptr<dbRigidBody_t> pointer;
    
    //
    dbCollisionShape_t::pointer collision_shape() { return m_collision_shape;   }
    
    //
    void set_kinematic(bool kinematic) {    m_impl->set_kinematic(kinematic);   }
    
    //
    void set_mass(float mass)               { m_impl->set_mass(mass);   }
    void set_inertia(vec3f const& I)        { m_impl->set_inertia(I);   }
    bool set_mass_properties(float mass, const vec3f& inertia, bool kinematic) {
        return m_impl->set_mass_properties(mass, inertia, kinematic);
    }
    void get_mass_properties(float& mass, vec3f& inertia, bool& kinematic) const {
        m_impl->get_mass_properties(mass, inertia, kinematic);
    }
    void set_restitution(float r)           { m_impl->set_restitution(r); }
    void set_friction(float f)              { m_impl->set_friction(f);    }
    void set_linear_damping(float d)         { m_impl->set_linear_damping(d);   }
    void set_angular_damping(float d)        { m_impl->set_angular_damping(d);   }
    
    void set_transform(vec3f const& position, quatf const& rotation)    { m_impl->set_transform(position, rotation);  }
    void get_transform(vec3f& position, quatf& rotation) const          { m_impl->get_transform(position, rotation);   }
    void get_transform(mat4x4f& xform) const                            { m_impl->get_transform(xform); }
    void set_interpolation_transform(vec3f const& position, quatf const& rotation)    { m_impl->set_interpolation_transform(position, rotation);  }
    
    float    get_mass() const { return m_impl->get_mass();}
    //
    void set_linear_velocity(vec3f const& v)                            { m_impl->set_linear_velocity(v);    }
    void get_linear_velocity(vec3f& v) const                            { m_impl->get_linear_velocity(v);    }
    
    void set_angular_velocity(vec3f const& v)                           { m_impl->set_angular_velocity(v);   }
    void get_angular_velocity(vec3f& v) const                           { m_impl->get_angular_velocity(v);   }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName) { m_impl->register_name(solver,objectName);}
    
    //
    void clear_forces()                                                 { m_impl->clear_forces(); }
    void apply_central_force(vec3f const& f)                            { m_impl->apply_central_force(f);    }
    void apply_torque(vec3f const& t)                                   { m_impl->apply_torque(t);    }
    
    void update_constraint()                                            { m_impl->update_constraint(); }
    
public:
    virtual ~dbRigidBody_t() {};
    
protected:
    friend class dbSolver_t;
    void add_constraint(DB_Constraint_t* constraint, bool disableCollide)    { m_impl->add_constraint(constraint, disableCollide); }
    void remove_constraint(DB_Constraint_t* constraint)                    { m_impl->remove_constraint(constraint); }
    dbRigidBody_t(const shared_ptr<rigid_body_impl_t>& impl, dbCollisionShape_t::pointer& shape): m_impl(impl), m_collision_shape(shape)
    {
    }
    
public:
    rigid_body_impl_t* impl() { return m_impl.get(); }
    //   rigid_body_impl_t const* impl() const { return m_impl.get(); }
    
private:
    shared_ptr<rigid_body_impl_t> m_impl;
    dbCollisionShape_t::pointer m_collision_shape;
};

