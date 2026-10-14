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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_collision_shape.h

//
//  DB_CollisionShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//



#pragma once


#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"
#include "collision_shape_impl.h"
#include "DShapeFrameMath.h"
#include "LinearMath/btAabbUtil2.h"

class DB_CollisionShape_t  : public collision_shape_impl_t
{
public:
    
protected:
    friend class DB_BulletSolver_t;
    
    DB_CollisionShape_t() { }

    scale_update unchanged_scale() {
        if (!shape()) return scale_update();
        scale_update update(new collision_shape_scale_update([]() {}));
        btVector3 minimum, maximum;
        shape()->getAabb(btTransform::getIdentity(), minimum, maximum);
        update->minimum = vec3f(minimum.x(), minimum.y(), minimum.z());
        update->maximum = vec3f(maximum.x(), maximum.y(), maximum.z());
        update->volume = volume();
        update->inertia = local_inertia();
        return update;
    }

    static bool prepare_bounds(collision_shape_scale_update& update,
                               const std::vector<vec3f>& vertices,
                               const btTransform& transform, float margin,
                               bool quantized = false) {
        if (vertices.empty()) return false;
        btVector3 minimum(vertices[0][0], vertices[0][1], vertices[0][2]);
        btVector3 maximum = minimum;
        for (const vec3f& vertex : vertices) {
            minimum.setMin(btVector3(vertex[0], vertex[1], vertex[2]));
            maximum.setMax(btVector3(vertex[0], vertex[1], vertex[2]));
        }
        if (quantized) {
            // Quantization may round outward by one cell. Include the build
            // domain's two-unit padding and leave a second cell of headroom.
            const btVector3 extent = maximum - minimum;
            margin += (btMax(extent.x(), btMax(extent.y(), extent.z())) +
                       2 * margin + 2) / 32767.0f;
        }
        btVector3 transformedMin, transformedMax;
        btTransformAabb(minimum, maximum, margin, transform, transformedMin, transformedMax);
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::bullet::isSafeCollisionCoordinate(transformedMin[axis]) ||
                !dommy::bullet::isSafeCollisionCoordinate(transformedMax[axis])) return false;
        }
        update.minimum = vec3f(transformedMin.x(), transformedMin.y(), transformedMin.z());
        update.maximum = vec3f(transformedMax.x(), transformedMax.y(), transformedMax.z());
        return true;
    }
    
    btCollisionShape* shape()               { return m_shape.get(); }
    virtual btCollisionShape* getBulletCollisionShape()               { return m_shape.get(); }
    virtual void getCenterOfMassTransformShift(class btTransform& shiftTransform)
    {
        shiftTransform.setIdentity();
    }
    
    void set_shape(btCollisionShape *shape) { return m_shape.reset(shape); }
    
public:
    friend class DB_RigidBody_t;
    virtual ~DB_CollisionShape_t() { }
    
private:
    shared_ptr<btCollisionShape> m_shape;
};
