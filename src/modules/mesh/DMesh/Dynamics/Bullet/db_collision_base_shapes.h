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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_box_shape.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_sphere_shape.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_plane_shape.h

//
//  db_collision_base_shapes.h
//  dommyTool
//
//  Created by yuzo on 2017/10/05.
//

#pragma once


#include "btBulletCollisionCommon.h"
#include "mathUtils.h"
#include "shared_ptr.h"

#include "db_collision_shape_base.h"
#include "Collision/DShapeFrameMath.h"

#include <memory>
#include <stdexcept>


class db_collision_box : public db_collision_shape_base
{
public:
    
     void set_scale(vec3f const& s) {
        const btVector3& scale = shape()->getLocalScaling();
        if(scale.x() != s[0] || scale.y() != s[1] || scale.z() != s[2]) {
            shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
            update();
        }
    }
    
     void get_scale(vec3f& s) {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    
public:
    
    db_collision_box(vec3f const& halfExtents = vec3f(0.5f, 0.5f, 0.5f)):
    db_collision_shape_base()
    {
        set_shape(std::unique_ptr<btCollisionShape>(
            new btBoxShape(
                btVector3(halfExtents[0], halfExtents[1], halfExtents[2]))));
        update();
    }
    
    void update()
    {
        btBoxShape *box_shape = static_cast<btBoxShape*>(shape());
        btVector3 e = 2 * box_shape->getHalfExtentsWithoutMargin();
        m_volume = e.x() * e.y() * e.z();
        m_center = vec3f(0, 0, 0);
        m_rotation = mvl::qidentity<float>();
        m_local_inertia = vec3f((e.y() * e.y() + e.z() * e.z()) / 12.0f,
                                (e.x() * e.x() + e.z() * e.z()) / 12.0f,
                                (e.x() * e.x() + e.y() * e.y()) / 12.0f);
    }
    
    
};


class db_collision_sphere: public db_collision_shape_base
{
public:
    
    virtual void set_scale(vec3f const& s) {
        shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
        update();
    }
    
    virtual void get_scale(vec3f& s) {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
public:
    
    db_collision_sphere(float radius):
    db_collision_shape_base()
    {
        set_shape(std::unique_ptr<btCollisionShape>(
            new btSphereShape(radius)));
        update();
    }
    
    void update()
    {
        btSphereShape *sphere_shape = static_cast<btSphereShape*>(shape());
        float radius = sphere_shape->getRadius();
        m_volume = (4.0f * 3.1415926f * radius * radius * radius) / 3.0f;
        m_center = vec3f(0, 0, 0);
        m_rotation = mvl::qidentity<float>();
        m_local_inertia = vec3f(2.0f / 5.0f * radius * radius,
                                2.0f / 5.0f * radius * radius,
                                2.0f / 5.0f * radius * radius);
    }
    
};



class db_collision_plane: public db_collision_shape_base
{
public:
    virtual void set_scale(vec3f const& s) {
        shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
    }
    
    virtual void get_scale(vec3f& s) {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    
public:
    
    db_collision_plane(vec3f const& normal, float d):
    db_collision_shape_base()
    {
        m_volume=0;
        m_local_inertia = vec3f(1, 1, 1);
        m_center= vec3f(0, 0, 0);
        m_rotation = mvl::qidentity<float>();
        set_shape(std::unique_ptr<btCollisionShape>(
            new btStaticPlaneShape(
                btVector3(normal[0], normal[1], normal[2]), d)));
    }
    
    
};



class db_collision_capsule: public db_collision_shape_base
{
public:
    virtual void set_scale(vec3f const& s) {
        shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
    }
    
    virtual void get_scale(vec3f& s) {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    
public:
    
    db_collision_capsule(float radius, float height):
    db_collision_shape_base()
    {
        if (!dommy::bullet::validCapsuleDimensions(radius, height)) {
            throw std::invalid_argument("Invalid Bullet capsule radius or height.");
        }
        m_volume=0;
        m_local_inertia = vec3f(1, 1, 1);
        m_center= vec3f(0, 0, 0);
        m_rotation = mvl::qidentity<float>();
        set_shape(std::unique_ptr<btCollisionShape>(
            new btCapsuleShape(radius, height)));
    }
    
    
};
