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
 
Written by: Erwin Coumans <erwin.coumans@gmail.com>
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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_composite_shape.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/collision_shape.h

//
//  db_collision_shape_base.h
//  dommyTool
//
//  Created by yuzo on 2017/10/21.
//

#pragma once

#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "mathUtils.h"
#include "shared_ptr.h"
#include "DBulletUserData.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>


class db_collision_shape_base
{
public:
    db_collision_shape_base()
        : m_volume(0.0f)
        , m_center(0.0f, 0.0f, 0.0f)
        , m_rotation(mvl::qidentity<float>())
        , m_local_inertia(0.0f, 0.0f, 0.0f)
        , m_itemIndex(std::numeric_limits<size_t>::max())
    {
    }

    virtual ~db_collision_shape_base()
    {
        if(shape()) {
            shape()->setUserPointer(0);
        }
    }

    db_collision_shape_base(const db_collision_shape_base&) = delete;
    db_collision_shape_base& operator=(const db_collision_shape_base&) = delete;

    virtual void set_scale(vec3f const& s) { (void)s; }
    virtual void get_scale(vec3f& s) { s = vec3f(1.0f, 1.0f, 1.0f); }
    
    float volume()                  { return m_volume;  }
    vec3f const& local_inertia()    { return m_local_inertia;  }
    vec3f const& center()           { return m_center; }
    quatf const& rotation()         { return m_rotation;  }
    
    size_t const& itemIndex()         { return m_itemIndex;  }
    void set_itemIndex(size_t i)         { m_itemIndex = i;  }
    
    virtual void getCenterOfMassTransformShift(class btTransform& shiftTransform)
    {
        shiftTransform.setIdentity();
    }
    
    btCollisionShape* shape() { return m_shape.get(); }
    const btCollisionShape* shape() const { return m_shape.get(); }
    void set_shape(btCollisionShape *shape)
    {
        m_shape.reset(shape);
        if(shape && m_userData) {
            shape->setUserPointer(m_userData.get());
        }
    }
    void set_shape(std::unique_ptr<btCollisionShape> shape)
    {
        set_shape(shape.release());
    }
    void set_user_data(customDBData* userData)
    {
        m_userData.reset(userData);
        if(shape()) {
            shape()->setUserPointer(m_userData.get());
        }
    }
    class btCollisionShape* getBulletCollisionShape() { return m_shape.get(); }
    
    
    
    
protected:
    shared_ptr<btCollisionShape> m_shape;
    
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
    size_t m_itemIndex;
    std::unique_ptr<customDBData> m_userData;
};






class db_composite_shape_t: public db_collision_shape_base
{
public:
    
    virtual void gl_draw(size_t draw_style)
    {
        (void)draw_style;
    }
    
    virtual void set_scale(vec3f const& s) {
        const btVector3& scale = shape()->getLocalScaling();
        if(scale.x() != s[0] || scale.y() != s[1] || scale.z() != s[2]) {
            shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
            update();
        }
    }
    
    virtual void get_scale(vec3f& s) {
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    
    
    virtual void getCenterOfMassTransformShift(class btTransform& shiftTransform)
    {
        shiftTransform.setIdentity();
        shiftTransform.setOrigin(btVector3(center()[0],center()[1],center()[2]));
        shiftTransform.setRotation(btQuaternion(rotation()[1],rotation()[2],rotation()[3],rotation()[0]));
    }
    
    
    
    
    std::vector<std::unique_ptr<db_collision_shape_base> >
        m_childCollisionShapes;
    std::vector<btTransform> m_childTransform;
    
    db_composite_shape_t(
        std::vector<std::unique_ptr<db_collision_shape_base> > childShapes,
        const std::vector<btTransform>& childTransform):
    db_collision_shape_base()
    {
        if (childShapes.empty() ||
            childShapes.size() != childTransform.size()) {
            throw std::invalid_argument(
                "Compound shapes require matching non-empty child data.");
        }

        const size_t childCount = childShapes.size();
        std::unique_ptr<btCompoundShape> compound(new btCompoundShape());
        for (size_t i=0;i<childCount;i++)
        {
            if (!childShapes[i] || !childShapes[i]->shape()) {
                throw std::invalid_argument(
                    "Compound shape contains an empty child.");
            }
            compound->addChildShape(
                childTransform[i], childShapes[i]->shape());
        }

        const size_t compoundChildCount = static_cast<size_t>(compound->getNumChildShapes());
        if(compoundChildCount != childCount) {
            throw std::runtime_error(
                "Bullet did not retain every compound child shape.");
        }

        std::vector<btScalar> masses(compoundChildCount, btScalar(1.0));
        btTransform principal;
        btVector3 inertia;
        compound->calculatePrincipalAxisTransform(
            masses.data(), principal, inertia);

        m_center = vec3f(
            principal.getOrigin().x(), principal.getOrigin().y(),
            principal.getOrigin().z());
        m_rotation = quatf(
            principal.getRotation().w(), principal.getRotation().x(),
            principal.getRotation().y(), principal.getRotation().z());

        for (size_t i=0;i<compoundChildCount;i++)
        {
            btTransform newChildTrans = principal.inverse() *
                compound->getChildTransform(static_cast<int>(i));
            compound->updateChildTransform(
                static_cast<int>(i), newChildTrans, false);
        }
        compound->recalculateLocalAabb();

        btTransform identity;
        identity.setIdentity();
        btVector3 aabbMin, aabbMax, localInertia;
        compound->getAabb(identity, aabbMin, aabbMax);
        compound->calculateLocalInertia(1.f, localInertia);
        const btVector3 extents = aabbMax - aabbMin;
        m_volume = extents.x() * extents.y() * extents.z();
        m_local_inertia = vec3f(
            localInertia.x(), localInertia.y(), localInertia.z());

        m_childTransform = childTransform;
        m_childCollisionShapes.reserve(childCount);
        set_shape(std::move(compound));
        m_childCollisionShapes = std::move(childShapes);
    }

    virtual ~db_composite_shape_t()
    {
        // btCompoundShape references, but does not own, its child shapes.
        // Destroy the compound before releasing the owning wrappers.
        set_shape(nullptr);
    }
    
    void update()
    {
        btCompoundShape* compound_shape = static_cast<btCompoundShape*>(shape());
        if(!compound_shape || compound_shape->getNumChildShapes() == 0) {
            m_volume = 0.0f;
            m_local_inertia = vec3f(0.0f, 0.0f, 0.0f);
            return;
        }
        //btVector3 e = 2 * compound_shape->getHalfExtentsWithoutMargin();
        btTransform tr;
        tr.setIdentity();
        btVector3 aabbMin,aabbMax,localInertia;
        compound_shape->getAabb(tr, aabbMin,aabbMax);
        compound_shape->calculateLocalInertia(1.f,localInertia);
        
        btVector3 e = aabbMax-aabbMin;
        m_volume = e.x() * e.y() * e.z();
        //m_center = vec3f(0,0,0);
        //m_rotation = mvl::qidentity<float>();
        m_local_inertia = vec3f(localInertia.x(),localInertia.y(),localInertia.z());
        
    }
    
private:
    
};
