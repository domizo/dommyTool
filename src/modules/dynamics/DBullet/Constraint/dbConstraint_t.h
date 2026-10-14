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
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/constraint.h

//
//  DB_Constraint_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/27.
//

#pragma once



#include "shared_ptr.h"
#include "constraint_impl.h"

class dbRigidBody_t;

//constraint_t
class dbConstraint_t
{
public:
    //typedefs
    typedef shared_ptr<dbConstraint_t> pointer;
    
public:
    virtual ~dbConstraint_t() {}
    
    bool getPivotChanged()
    {
        constraint_impl_t* constr_impl = dynamic_cast<constraint_impl_t*>(impl());
        return constr_impl->getPivotChanged();
    }
    void setPivotChanged(bool isChanged)
    {
        constraint_impl_t* constr_impl = dynamic_cast<constraint_impl_t*>(impl());
        return constr_impl->setPivotChanged(isChanged);
    }
    constraint_impl_t* pubImpl()            { return m_impl.get(); }
    shared_ptr<dbRigidBody_t> rigid_bodyA() const { return m_rigid_bodyA; }
    shared_ptr<dbRigidBody_t> rigid_bodyB() const { return m_rigid_bodyB; }
    
    virtual void register_name(solver_impl_t* solver, const char* objectName) = 0;
    
protected:
    friend class dbSolver_t;
    
    dbConstraint_t(const shared_ptr<constraint_impl_t>& impl,
                   const shared_ptr<dbRigidBody_t>& rigidBodyA,
                   const shared_ptr<dbRigidBody_t>& rigidBodyB =
                       shared_ptr<dbRigidBody_t>())
        : m_impl(impl),
          m_rigid_bodyA(rigidBodyA),
          m_rigid_bodyB(rigidBodyB) {}
    
    constraint_impl_t* impl()              { return m_impl.get(); }
    constraint_impl_t const* impl() const  { return m_impl.get(); }
    
protected:
    shared_ptr<constraint_impl_t> m_impl;
    shared_ptr<dbRigidBody_t> m_rigid_bodyA;
    shared_ptr<dbRigidBody_t> m_rigid_bodyB;
};

