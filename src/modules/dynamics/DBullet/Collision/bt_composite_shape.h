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
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_composite_shape.h

//
//  bt_composite_shape.h
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//

#pragma once

#include "composite_shape_impl.h"
#include "drawUtils.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"

#include <vector>
#include <memory>

class bt_composite_shape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override
    {
        (void)draw_style;
    }
    
    scale_update prepare_scale(vec3f const& s, bool force = false) override {
        if (!shape() || shape()->getShapeType() != COMPOUND_SHAPE_PROXYTYPE ||
            !dommy::bullet::isFiniteVector(s)) return scale_update();
        btCompoundShape* compound = static_cast<btCompoundShape*>(shape());
        const std::size_t count = m_childCollisionShapes.size();
        if (count == 0 || count != static_cast<std::size_t>(compound->getNumChildShapes()) ||
            count != m_sourceChildTransforms.size()) return scale_update();
        struct Staged {
            std::vector<scale_update> children;
            std::vector<btTransform> transforms;
        };
        const std::shared_ptr<Staged> staged(new Staged());
        staged->children.reserve(count);
        staged->transforms.reserve(count);
        btVector3 minimum(FLT_MAX,FLT_MAX,FLT_MAX), maximum(-FLT_MAX,-FLT_MAX,-FLT_MAX);
        for (std::size_t i = 0; i < count; ++i) {
            if (compound->getChildShape(static_cast<int>(i)) != m_childCollisionShapes[i]->getBulletCollisionShape()) return scale_update();
            const vec3f& originalScale = m_sourceChildScales[i];
            const vec3f childScale(originalScale[0]*s[0], originalScale[1]*s[1], originalScale[2]*s[2]);
            scale_update child = m_childCollisionShapes[i]->prepare_scale(childScale, force);
            if (!child) return scale_update();
            btTransform transform = m_sourceChildTransforms[i];
            const btVector3& origin = transform.getOrigin();
            vec3f scaledOrigin;
            if (!dommy::bullet::scaledCollisionPoint(vec3f(origin.x(),origin.y(),origin.z()), s, scaledOrigin)) return scale_update();
            transform.setOrigin(btVector3(scaledOrigin[0],scaledOrigin[1],scaledOrigin[2]));
            btVector3 childMin, childMax;
            btTransformAabb(btVector3(child->minimum[0],child->minimum[1],child->minimum[2]),
                            btVector3(child->maximum[0],child->maximum[1],child->maximum[2]),
                            compound->getMargin(), transform, childMin, childMax);
            for (int axis = 0; axis < 3; ++axis) {
                if (!dommy::bullet::isSafeCollisionCoordinate(childMin[axis]) ||
                    !dommy::bullet::isSafeCollisionCoordinate(childMax[axis])) return scale_update();
            }
            minimum.setMin(childMin);
            maximum.setMax(childMax);
            staged->children.push_back(std::move(child));
            staged->transforms.push_back(transform);
        }
        // Native AABB reconstruction and float inertia arithmetic can round
        // above an analytical double result. Reserve outward rounding headroom
        // before this bound is used to admit a mass-weighted inertia product.
        for (int axis = 0; axis < 3; ++axis) {
            const float magnitude = btMax(btFabs(minimum[axis]), btFabs(maximum[axis]));
            const float padding = 32 * std::numeric_limits<float>::epsilon() * magnitude;
            minimum[axis] = std::nextafter(minimum[axis] - padding, -FLT_MAX);
            maximum[axis] = std::nextafter(maximum[axis] + padding, FLT_MAX);
            if (!dommy::bullet::isSafeCollisionCoordinate(minimum[axis]) ||
                !dommy::bullet::isSafeCollisionCoordinate(maximum[axis])) return scale_update();
        }
        float volume = 0;
        vec3f inertia(0,0,0);
        const btVector3 extent = maximum - minimum;
        if (!dommy::bullet::isSingularScale(s) &&
            !dommy::bullet::boxMassFromDimensions(vec3f(extent.x(),extent.y(),extent.z()), volume, inertia)) return scale_update();
        scale_update result(new collision_shape_scale_update([this, staged, compound, s]() {
            // Every child is prepared before any live child is changed.
            for (std::size_t i = 0; i < staged->children.size(); ++i) {
                staged->children[i]->commit();
                compound->updateChildTransform(static_cast<int>(i), staged->transforms[i], false);
            }
            compound->recalculateLocalAabb();
            m_cachedScaling = s;
            update();
            if (dommy::bullet::isSingularScale(s)) {
                m_volume = 0;
                m_local_inertia = vec3f(0,0,0);
            }
        }));
        result->minimum = vec3f(minimum.x(),minimum.y(),minimum.z());
        result->maximum = vec3f(maximum.x(),maximum.y(),maximum.z());
        result->volume = dommy::bullet::isSingularScale(s) ? 0 : volume;
        for (int axis = 0; axis < 3; ++axis)
            inertia[axis] = std::nextafter(inertia[axis], FLT_MAX);
        result->inertia = dommy::bullet::isSingularScale(s) ? vec3f(0,0,0) : inertia;
        return result;
    }
    
    virtual void get_scale(vec3f& s) override {
        s = m_cachedScaling;
    }
    
    virtual float volume() override { return m_volume;  }
    virtual vec3f const& local_inertia() override { return m_local_inertia;  }
    virtual vec3f const& center() override { return m_center; }
    virtual quatf const& rotation() override { return m_rotation;  }
    
    virtual void getCenterOfMassTransformShift(class btTransform& shiftTransform) override
    {
        shiftTransform.setIdentity();
        shiftTransform.setOrigin(btVector3(center()[0],center()[1],center()[2]));
        shiftTransform.setRotation(btQuaternion(rotation()[1],rotation()[2],rotation()[3],rotation()[0]));
    }
    
protected:
    friend class DB_BulletSolver_t;
    
    std::vector<dbCollisionShape_t::pointer> m_childCollisionShapes;
    std::vector< vec3f> m_childPosition;
    std::vector< quatf> m_childOrientations;
    
    bt_composite_shape_t(
                         dbCollisionShape_t::pointer* childShapes,
                         vec3f* childPositions,
                         quatf* childOrientations,
                         int numChildren):
    DB_CollisionShape_t(),
    m_cachedScaling(1,1,1),
    m_volume(0.0f),
    m_center(0.0f, 0.0f, 0.0f),
    m_rotation(mvl::qidentity<float>()),
    m_local_inertia(0.0f, 0.0f, 0.0f)
    {
        std::unique_ptr<btCompoundShape> compound(new btCompoundShape());
        if (!childShapes || !childPositions || !childOrientations || numChildren <= 0) {
            set_shape(compound.release());
            return;
        }

        // Node-created children are fresh, private shapes. Reject duplicate or
        // already-shared children instead of mutating another body's geometry.
        for (int i = 0; i < numChildren; ++i) {
            if (!childShapes[i] || childShapes[i].use_count() != 1 ||
                !childShapes[i]->getBulletCollisionShape()) return;
        }

        for (int i=0;i<numChildren;i++)
        {
            m_childCollisionShapes.push_back(childShapes[i]);
            m_childPosition.push_back(childPositions[i]);
            m_childOrientations.push_back(childOrientations[i]);
        }
        
        for (int i=0;i<numChildren;i++)
        {
            if (!childShapes[i]) continue;

            btTransform childTrans;
            childTrans.setIdentity();
            childTrans.setOrigin(btVector3(childPositions[i][0],childPositions[i][1],childPositions[i][2]));
            childTrans.setRotation(btQuaternion(childOrientations[i][1],childOrientations[i][2],childOrientations[i][3],childOrientations[i][0]));
            btCollisionShape* childShape = childShapes[i]->getBulletCollisionShape();
            if (childShape)
            {
                compound->addChildShape(childTrans, childShape);
            }
        }
        
        const int compoundChildCount = compound->getNumChildShapes();
        if (compoundChildCount == 0) {
            set_shape(compound.release());
            return;
        }

        std::vector<btScalar> masses(static_cast<size_t>(compoundChildCount), btScalar(1.0));

        // Static triangle meshes do not implement calculateLocalInertia in
        // Bullet. Use bounded box proxies only for that principal-frame query;
        // the published compound keeps the original triangle collision shape.
        std::vector<std::unique_ptr<btBoxShape> > inertiaProxies;
        std::unique_ptr<btCompoundShape> principalCompound;
        for (int i = 0; i < compoundChildCount; ++i) {
            if (compound->getChildShape(i)->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE) {
                principalCompound.reset(new btCompoundShape());
                break;
            }
        }
        if (principalCompound) {
            for (int i = 0; i < compoundChildCount; ++i) {
                btCollisionShape* child = compound->getChildShape(i);
                if (child->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE) {
                    btVector3 minimum, maximum;
                    child->getAabb(btTransform::getIdentity(), minimum, maximum);
                    const btVector3 extent = maximum - minimum;
                    float volume;
                    vec3f inertia;
                    if (!dommy::bullet::boxMassFromDimensions(vec3f(extent.x(),extent.y(),extent.z()), volume, inertia)) return;
                    inertiaProxies.emplace_back(new btBoxShape(extent * 0.5f));
                    child = inertiaProxies.back().get();
                }
                principalCompound->addChildShape(compound->getChildTransform(i), child);
            }
        }
        
        btTransform principal;
        btVector3 inertia;
        btCompoundShape* principalSource = principalCompound ? principalCompound.get() : compound.get();
        principalSource->calculatePrincipalAxisTransform(masses.data(), principal, inertia);
        
        m_center = vec3f(principal.getOrigin().x(),principal.getOrigin().y(),principal.getOrigin().z());
        m_rotation = quatf(principal.getRotation().w(),principal.getRotation().x(),principal.getRotation().y(),principal.getRotation().z());
        
        for (int i=0;i<compoundChildCount;i++)
        {
            btTransform newChildTrans = principal.inverse() * compound->getChildTransform(i);
            compound->updateChildTransform(i,newChildTrans,false);
            m_sourceChildTransforms.push_back(newChildTrans);
            vec3f scale;
            m_childCollisionShapes[i]->get_scale(scale);
            m_sourceChildScales.push_back(scale);
        }
        compound->recalculateLocalAabb();
        set_shape(compound.release());
        
        update();
    }
    
    virtual ~bt_composite_shape_t() { }
    
    void update()
    {
        btCompoundShape* compound_shape = static_cast<btCompoundShape*>(shape());
        btTransform tr;
        tr.setIdentity();
        btVector3 aabbMin,aabbMax,localInertia;
        compound_shape->getAabb(tr, aabbMin,aabbMax);
        compound_shape->calculateLocalInertia(1.f,localInertia);
        
        btVector3 e = aabbMax-aabbMin;
        m_volume = e.x() * e.y() * e.z();
        m_local_inertia = vec3f(localInertia.x(),localInertia.y(),localInertia.z());
        
    }
    
private:
    std::vector<btTransform> m_sourceChildTransforms;
    std::vector<vec3f> m_sourceChildScales;
    vec3f m_cachedScaling;
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
};
