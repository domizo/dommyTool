//
//  dbMeshShape.h
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//

#pragma once

#include "dbCollisionShape_t.h"
#include "collision_shape_impl.h"

class dbMeshShape_t: public dbCollisionShape_t
{
public:
    //typedefs
    typedef shared_ptr<dbMeshShape_t> pointer;
    
protected:
    friend class dbSolver_t;
    
    dbMeshShape_t(const shared_ptr<collision_shape_impl_t>& impl): dbCollisionShape_t(impl) { }
    
};

class dbHacdShape_t: public dbCollisionShape_t
{
public:
    //typedefs
    typedef shared_ptr<dbHacdShape_t> pointer;
    
protected:
    friend class dbSolver_t;
    
    dbHacdShape_t(const shared_ptr<collision_shape_impl_t>& impl): dbCollisionShape_t(impl) { }
    
};
