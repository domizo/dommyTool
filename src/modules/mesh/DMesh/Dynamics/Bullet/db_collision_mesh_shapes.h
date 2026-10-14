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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_mesh_shape.h
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_convex_hull_shape.h

//
//  db_collision_mesh_shapes.h
//  dommyTool
//
//  Created by yuzo on 2017/10/05.
//

#pragma once


#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "convex_decomposition_hacd.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "BulletCollision/CollisionShapes/btConvex2dShape.h"

#include "mathUtils.h"
#include "shared_ptr.h"

#include "db_collision_shape_base.h"
#include "../../../../dynamics/DBullet/Collision/DMassProperties.h"

#include <memory>
#include <utility>


// Convex hull

class db_collision_convex_hull: public db_collision_shape_base
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
    
    db_collision_convex_hull(vec3f const* vertices, size_t num_vertices,
                           vec3f const* normals,
                           unsigned int const *indices, size_t num_indices):
    db_collision_shape_base(),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices)
    {
        dommy::bullet::MassProperties properties;
        if (dommy::bullet::computeMassProperties(
                vertices, num_vertices, indices, num_indices, properties)) {
            m_volume = properties.volume;
        }
        
        {
            m_center = vec3f(0.f,0.f,0.f);
            m_local_inertia = vec3f(0.f,0.f,0.f);
            m_rotation = quatf(mvl::qidentity<float>());
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i)
            {
                m_vertices[i] = vertices[i];
            }
        }
        std::unique_ptr<btConvexHullShape> convexShape(
            new btConvexHullShape());
        for (size_t vertex = 0; vertex < m_vertices.size(); ++vertex) {
            convexShape->addPoint(btVector3(
                m_vertices[vertex][0], m_vertices[vertex][1],
                m_vertices[vertex][2]), false);
        }
        convexShape->recalcLocalAabb();
        set_shape(std::move(convexShape));
        m_cachedScaling[0] = m_cachedScaling[1] = m_cachedScaling[2] = 1.f;
    }
    
    void update()
    {
        m_local_inertia = vec3f(0,0,0);
        m_rotation = quatf(mvl::qidentity<float>());
        m_center = vec3f(0,0,0);
    }
    
private:
    
    std::vector<vec3f>              m_vertices;
    std::vector<vec3f>              m_normals;
    std::vector<unsigned int>       m_indices;
    
    btVector3                        m_cachedScaling;
};






// Triangle mesh

class db_collision_mesh: public db_collision_shape_base
{
public:
    
    virtual void set_scale(vec3f const& s) {
        if (!m_concaveShapeObserver) return;
        m_concaveShapeObserver->setLocalScaling(btVector3(s[0], s[1], s[2]));
        if (m_concaveShapeObserver->getShapeType() == GIMPACT_SHAPE_PROXYTYPE)
        {
            btGImpactMeshShape *gi_shape = static_cast<btGImpactMeshShape*>(m_concaveShapeObserver);
            gi_shape->updateBound();
        }
        
        update();
    }
    
    virtual void get_scale(vec3f& s) {
        if (!m_concaveShapeObserver) {
            s = vec3f(1.0f, 1.0f, 1.0f);
            return;
        }
        const btVector3& scale = m_concaveShapeObserver->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
public:
    
    db_collision_mesh(vec3f const* vertices,
                   size_t num_vertices,
                   vec3f const* normals,
                   unsigned int const *indices,
                   size_t num_indices,
                   bool dynamicMesh):
    db_collision_shape_base(),
    m_gimpactShape(),
    m_concaveShapeObserver(nullptr),
    m_vertices(),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices),
    m_bulletIndices(indices, indices + num_indices),
    m_dynamicMesh(dynamicMesh),
    m_tiva()
    {
        
        if (m_dynamicMesh)
        {
            dommy::bullet::MassProperties properties;
            if (!dommy::bullet::computeMassProperties(
                    vertices, num_vertices, indices, num_indices, properties)) {
                return;
            }
            m_volume = properties.volume;
            m_center = properties.center;
            const mat3x3f& I = properties.inertia;
            m_rotation = diagonalizer(I);
            
            mat3x3f Q, Qinv;
            q_to_mat(m_rotation, Q);
            q_to_mat(qconj(m_rotation), Qinv);
            
            //D = trans(Q) * I * Q;
            m_local_inertia = diag(prod(trans(Q), mat3x3f(prod(I, Q))));
            
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i) {
                m_vertices[i] = prod(Qinv, vertices[i] - m_center);
            }
            
            m_tiva.reset(new btTriangleIndexVertexArray(
                static_cast<int>(num_indices / 3U), &m_bulletIndices[0],
                3 * sizeof(int), static_cast<int>(num_vertices),
                &m_vertices[0][0], sizeof(vec3f)));
            
            
            
            std::unique_ptr<btGImpactMeshShape> gimpact(
                new btGImpactMeshShape(m_tiva.get()));
            m_concaveShapeObserver = gimpact.get();
            m_concaveShapeObserver->setLocalScaling(btVector3(1.0f,1.0f,1.0f));
            gimpact->updateBound();
            std::unique_ptr<btCompoundShape> compoundShape(
                new btCompoundShape());
            compoundShape->addChildShape(
                btTransform(
                    btQuaternion(
                        m_rotation[1], m_rotation[2], m_rotation[3],
                        m_rotation[0]),
                    btVector3(m_center[0], m_center[1], m_center[2])),
                m_concaveShapeObserver);
            m_gimpactShape.reset(gimpact.release());
            set_shape(std::move(compoundShape));
        }
        else
        {
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i) {
                m_vertices[i] = vertices[i];
            }
            
            m_tiva.reset(new btTriangleIndexVertexArray(
                static_cast<int>(num_indices / 3U), &m_bulletIndices[0],
                3 * sizeof(int), static_cast<int>(num_vertices),
                &m_vertices[0][0], sizeof(vec3f)));
            
            std::unique_ptr<btBvhTriangleMeshShape> bvh(
                new btBvhTriangleMeshShape(m_tiva.get(), true));
            m_concaveShapeObserver = bvh.get();
            m_concaveShapeObserver->setLocalScaling(btVector3(1.0f,1.0f,1.0f));
            set_shape(std::move(bvh));
            m_gimpactShape.reset();
        }
        
        
        
        
        
        // gimpactShape->setMargin(0.05);
    }
    
    void update()
    {
        if (!m_concaveShapeObserver || m_vertices.empty() || m_indices.empty()) return;
        //apply the scaling
        btVector3 const& scale = m_concaveShapeObserver->getLocalScaling();
        
        dommy::bullet::MassProperties properties;
        if (!dommy::bullet::computeScaledMassProperties(
                m_vertices, m_indices,
                vec3f(scale.x(), scale.y(), scale.z()), properties)) {
            m_volume = 0.0f;
            m_local_inertia = vec3f(0.0f, 0.0f, 0.0f);
            return;
        }
        m_volume = properties.volume;
        const mat3x3f& I = properties.inertia;
        //   std::cout << I << std::endl;
        //m_rotation = diagonalizer(I);
        //std::cout << rotation << std::endl;
        //the rotation shouldn't change from scaling
        
        mat3x3f Q;
        q_to_mat(m_rotation, Q);
        
        //D = Q * I * trans(Q);
        m_local_inertia = diag(prod(trans(Q), mat3x3f(prod(I, Q))));
    }
    
private:
    shared_ptr<btConcaveShape>                m_gimpactShape;
    // Non-owning alias. The dynamic shape is owned by m_gimpactShape and the
    // static shape by db_collision_shape_base::m_shape.
    btConcaveShape*                          m_concaveShapeObserver;
    std::vector<vec3f>                      m_vertices;
    std::vector<vec3f>                      m_normals;
    std::vector<unsigned int>               m_indices;
    std::vector<int>                        m_bulletIndices;
    bool                                    m_dynamicMesh;
    shared_ptr<btTriangleIndexVertexArray>  m_tiva;
    
};


// HACD decomposition


class db_collision_hacd : public db_collision_shape_base
{
public:
    
    virtual void set_scale(vec3f const& s) {
        if (!shape()) return;
        shape()->setLocalScaling(btVector3(s[0], s[1], s[2]));
        update();
    }
    
    virtual void get_scale(vec3f& s) {
        if (!shape()) {
            s = vec3f(1.0f, 1.0f, 1.0f);
            return;
        }
        const btVector3& scale = shape()->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
public:
    
    db_collision_hacd(vec3f const* vertices,
                   size_t num_vertices,
                   vec3f const* normals,
                   unsigned int const *indices,
                   size_t num_indices,
                   bool dynamicMesh):
    db_collision_shape_base(),
    m_vertices(),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices)
    {
        (void)dynamicMesh;
        dommy::bullet::MassProperties properties;
        if (!dommy::bullet::computeMassProperties(
                vertices, num_vertices, indices, num_indices, properties)) {
            return;
        }
        m_volume = properties.volume;
        m_center = properties.center;
        const mat3x3f& I = properties.inertia;
        m_rotation = diagonalizer(I);
        
        mat3x3f Q, Qinv;
        q_to_mat(m_rotation, Q);
        q_to_mat(qconj(m_rotation), Qinv);
        
        //D = trans(Q) * I * Q;
        m_local_inertia = diag(prod(trans(Q), mat3x3f(prod(I, Q))));
        
        m_vertices.resize(num_vertices);
        for(size_t i = 0; i < m_vertices.size(); ++i) {
            m_vertices[i] = vertices[i];
        }
        
        const int numHacdVertices = static_cast<int>(num_vertices);
        std::vector<float> hacdVertices(numHacdVertices*3);
        
        for(int i = 0; i < numHacdVertices; ++i) {
            int ii = i*3;
            hacdVertices[ii] = vertices[i][0];
            hacdVertices[ii+1] = vertices[i][1];
            hacdVertices[ii+2] = vertices[i][2];
        }
        
        
        convex_decomposition_hacd hacdTest;
        std::unique_ptr<btCompoundShape> compoundShape(
            hacdTest.ConvexDecomp(
                numHacdVertices, hacdVertices.data(),
                static_cast<int>(num_indices), indices));
        
        if (compoundShape && compoundShape->getNumChildShapes() > 0)
        {
            set_shape(std::move(compoundShape));
        }
        
        
        ////////////////////////////////////////////
        
        
        
        //std::cout << "construtor: " << m_center << std::endl;
        
        // gimpactShape->setMargin(0.05);
    }
    
    void update()
    {
        //apply the scaling
        if (shape() && !m_vertices.empty() && !m_indices.empty())
        {
            btVector3 const& scale = shape()->getLocalScaling();
            
            std::vector<vec3f> vertices(m_vertices.size());
            for(size_t i = 0; i < vertices.size(); ++i) {
                vertices[i] = vec3f(scale.x() * m_vertices[i][0], scale.y() * m_vertices[i][1], scale.z() * m_vertices[i][2]);
            }
            dommy::bullet::MassProperties properties;
            if (!dommy::bullet::computeMassProperties(
                    &vertices[0], vertices.size(), &m_indices[0],
                    m_indices.size(), properties)) {
                m_volume = 0.0f;
                m_local_inertia = vec3f(0.0f, 0.0f, 0.0f);
                return;
            }
            m_volume = properties.volume;
            const mat3x3f& I = properties.inertia;
            //   std::cout << I << std::endl;
            //m_rotation = diagonalizer(I);
            //std::cout << rotation << std::endl;
            //the rotation shouldn't change from scaling
            
            mat3x3f Q, Qinv;
            q_to_mat(m_rotation, Q);
            q_to_mat(qconj(m_rotation), Qinv);
            
            //D = Q * I * trans(Q);
            m_local_inertia = diag(prod(trans(Q), mat3x3f(prod(I, Q))));
        }
    }
    
private:
    std::vector<vec3f>                      m_vertices;
    std::vector<vec3f>                      m_normals;
    std::vector<unsigned int>               m_indices;
    
};








// 2D convex hull

class db_collision_convex_2d: public db_collision_shape_base
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
    
    db_collision_convex_2d(vec3f const* vertices, size_t num_vertices):
    db_collision_shape_base()
    {
        m_volume = 0.f;
        
        {
            m_center = vec3f(0.f,0.f,0.f);
            m_local_inertia = vec3f(0.f,0.f,0.f);
            m_rotation = quatf(mvl::qidentity<float>());
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i)
            {
                m_vertices[i] = btVector3(vertices[i][0],vertices[i][1],vertices[i][2]);
            }
        }
        
        std::unique_ptr<btConvexHullShape> childShape(
            new btConvexHullShape());
        for(size_t i = 0; i < m_vertices.size(); ++i) {
            childShape->addPoint(m_vertices[i], false);
        }
        childShape->recalcLocalAabb();
        m_childShape.reset(childShape.release());
        set_shape(std::unique_ptr<btCollisionShape>(
            new btConvex2dShape(m_childShape.get())));
        
        m_cachedScaling[0] = m_cachedScaling[1] = m_cachedScaling[2] = 1.f;
    }
    
    void update()
    {
        {
            m_local_inertia = vec3f(0,0,0);
            m_rotation = quatf(mvl::qidentity<float>());
            m_center = vec3f(0,0,0);
        }
        
    }
    
private:
    
    std::vector<btVector3>              m_vertices;
    shared_ptr<btConvexShape>           m_childShape;
    btVector3                        m_cachedScaling;
};
