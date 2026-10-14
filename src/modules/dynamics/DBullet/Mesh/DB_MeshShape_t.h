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

//
//  DB_MeshShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//



#pragma once


#include <vector>
#include <memory>
#include <iterator>

#include "mesh_shape_impl.h"
#include "DB_CollisionShape_t.h"
#include "../Collision/DShapeFrameMath.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"

namespace dommy {
namespace bullet {

// Bullet 2.88's quantized refit clamps to the original quantization domain.
// Scaling can move triangles outside that domain, so rebuild each changed part.
// Retain the mesh and part identities used by existing collision algorithms.
class RebuildingGImpactMeshPart : public btGImpactMeshShapePart
{
public:
    RebuildingGImpactMeshPart(btStridingMeshInterface* mesh, int part)
        : btGImpactMeshShapePart(mesh, part) {}

protected:
    void calcLocalAABB() override
    {
        lockChildShapes();
        m_box_set.buildSet();
        unlockChildShapes();
        m_localAABB = m_box_set.getGlobalBox();
    }
};

class RebuildingGImpactMeshShape : public btGImpactMeshShape
{
public:
    explicit RebuildingGImpactMeshShape(btStridingMeshInterface* mesh)
        : btGImpactMeshShape(mesh)
    {
        // Replace only during construction, before this shape is published.
        // The base destructor owns all installed parts, including on failure.
        for (int i = 0; i < m_mesh_parts.size(); ++i) {
            std::unique_ptr<btGImpactMeshShapePart> part(new RebuildingGImpactMeshPart(mesh, i));
            delete m_mesh_parts[i];
            m_mesh_parts[i] = part.release();
        }
    }
};

} // namespace bullet
} // namespace dommy

class DB_MeshShape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override {
        
        if (!m_concaveShapeObserver || m_vertices.empty() || m_indices.empty()) return;
        
        btVector3 const& scale = m_concaveShapeObserver->getLocalScaling();
        glPushAttrib(GL_ENABLE_BIT);
        glEnable(GL_NORMALIZE);
        glPushMatrix();
        glTranslatef(m_center[0], m_center[1], m_center[2]);
        float angle;
        vec3f axis;
        q_to_axis_angle(m_rotation, axis, angle);
        if (angle != 0.0f) glRotatef(rad2deg(angle), axis[0], axis[1], axis[2]);
        glScalef(scale.x(), scale.y(), scale.z());
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        if(draw_style & dbCollisionShape_t::kDSSolid) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        glVertexPointer(3, GL_FLOAT, 0, &(m_vertices[0]));
        glNormalPointer(GL_FLOAT, 0, &(m_normals[0]));
        glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, &(m_indices[0]));
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glPopMatrix();
        glPopAttrib();
    }
    
    scale_update prepare_scale(vec3f const& s, bool force = false) override {
        if (!m_concaveShapeObserver || !dommy::bullet::isFiniteVector(s)) {
            return scale_update();
        }
        std::vector<vec3f> vertices;
        if (!dommy::bullet::prepareScaledCollisionVertices(m_vertices, s, vertices)) return scale_update();
        dommy::bullet::MassProperties properties;
        vec3f inertia(0,0,0);
        if (!dommy::bullet::isSingularScale(s)) {
            const bool hasMass = dommy::bullet::computeMassProperties(
                vertices.data(), vertices.size(), m_indices.data(), m_indices.size(), properties);
            // Static triangle soups need not enclose a volume.
            if (!hasMass && m_dynamicMesh) return scale_update();
            if (hasMass && !dommy::bullet::inertiaDiagonalInFrame(
                    properties.inertia, m_rotation, inertia)) return scale_update();
        }
        btCompoundShape* compound = nullptr;
        if (m_dynamicMesh) {
            if (!shape() || shape()->getShapeType() != COMPOUND_SHAPE_PROXYTYPE) return scale_update();
            compound = static_cast<btCompoundShape*>(shape());
            if (compound->getNumChildShapes() != 1 ||
                compound->getChildShape(0) != m_concaveShapeObserver) return scale_update();
            const btTransform& transform = compound->getChildTransform(0);
            for (const vec3f& vertex : vertices) {
                const btVector3 point = transform * btVector3(vertex[0], vertex[1], vertex[2]);
                for (int axis = 0; axis < 3; ++axis) {
                    if (!dommy::bullet::isSafeCollisionCoordinate(point[axis])) return scale_update();
                }
            }
        }
        const btVector3 scaling(s[0], s[1], s[2]);
        const bool changed = force || scaling != m_concaveShapeObserver->getLocalScaling();
        if (!m_dynamicMesh && m_concaveShapeObserver->getShapeType() != TRIANGLE_MESH_SHAPE_PROXYTYPE) return scale_update();
        scale_update update(new collision_shape_scale_update([this, scaling, changed, compound, properties, inertia]() {
            if (changed && !m_dynamicMesh) {
                btBvhTriangleMeshShape* bvh = static_cast<btBvhTriangleMeshShape*>(m_concaveShapeObserver);
                bvh->btTriangleMeshShape::setLocalScaling(scaling);
                bvh->buildOptimizedBvh();
            } else if (changed) {
                m_concaveShapeObserver->setLocalScaling(scaling);
            }
            if (changed && m_concaveShapeObserver->getShapeType() == GIMPACT_SHAPE_PROXYTYPE) {
                btGImpactMeshShape* gi_shape = static_cast<btGImpactMeshShape*>(m_concaveShapeObserver);
                gi_shape->updateBound();
            }
            if (changed && compound) {
                const btTransform transform = compound->getChildTransform(0);
                compound->updateChildTransform(0, transform);
            }
            m_volume = properties.volume;
            m_local_inertia = inertia;
        }));
        update->volume = properties.volume;
        update->inertia = inertia;
        const btTransform transform = compound ? compound->getChildTransform(0) : btTransform::getIdentity();
        float margin = m_concaveShapeObserver->getMargin();
        if (m_dynamicMesh) {
            const btGImpactMeshShape* mesh = static_cast<btGImpactMeshShape*>(m_concaveShapeObserver);
            for (int part = 0; part < mesh->getMeshPartCount(); ++part) {
                margin = btMax(margin, mesh->getMeshPart(part)->getMargin());
            }
        }
        if (!prepare_bounds(*update, vertices, transform,
                2 * margin, m_dynamicMesh)) return scale_update();
        return update;
    }
    
    virtual void get_scale(vec3f& s) override {
        if (!m_concaveShapeObserver) {
            s = vec3f(1.0f, 1.0f, 1.0f);
            return;
        }
        const btVector3& scale = m_concaveShapeObserver->getLocalScaling();
        s = vec3f(scale.x(), scale.y(), scale.z());
    }
    
    virtual float volume() override { return m_volume;  }
    virtual vec3f const& local_inertia() override { return m_local_inertia;  }
    virtual vec3f const& center() override { return m_center; }
    virtual quatf const& rotation() override { return m_rotation;  }
    
protected:
    friend class DB_BulletSolver_t;

    DB_MeshShape_t(vec3f const* vertices,
                    size_t num_vertices,
                    vec3f const* normals,
                    unsigned int const *indices,
                    size_t num_indices,
                    bool dynamicMesh):
    DB_CollisionShape_t(),
    m_gimpactShape(),
    m_concaveShapeObserver(nullptr),
    m_vertices(),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices),
    m_bulletIndices(indices, indices + num_indices),
    m_dynamicMesh(dynamicMesh),
    m_tiva(),
    m_volume(0.0f),
    m_center(0.0f, 0.0f, 0.0f),
    m_rotation(mvl::qidentity<float>()),
    m_local_inertia(0.0f, 0.0f, 0.0f)
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
            
            mat3x3f Qinv;
            q_to_mat(qconj(m_rotation), Qinv);
            
            m_local_inertia = diag(I);
            
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i) {
                m_vertices[i] = prod(Qinv, vertices[i] - m_center);
                m_normals[i] = prod(Qinv, normals[i]);
            }
            
            m_tiva.reset(new btTriangleIndexVertexArray(
                                                        static_cast<int>(num_indices / 3), &m_bulletIndices[0], 3 * sizeof(int),
                                                        static_cast<int>(num_vertices), &m_vertices[0][0], sizeof(vec3f)));
            
            
            
            m_gimpactShape.reset(new dommy::bullet::RebuildingGImpactMeshShape(m_tiva.get()));
            btGImpactMeshShape* gimpact = static_cast<btGImpactMeshShape*>(m_gimpactShape.get());
            m_concaveShapeObserver = m_gimpactShape.get();
            m_concaveShapeObserver->setLocalScaling(btVector3(1.0f,1.0f,1.0f));
            gimpact->updateBound();
            std::unique_ptr<btCompoundShape> compound_shape(new btCompoundShape);
            compound_shape->addChildShape(btTransform(btQuaternion(m_rotation[1],
                                                                   m_rotation[2],
                                                                   m_rotation[3],
                                                                   m_rotation[0]),
                                                      btVector3(m_center[0],
                                                                m_center[1],
                                                                m_center[2])),
                                          m_concaveShapeObserver);
            set_shape(compound_shape.release());
        } else
        {
            m_vertices.resize(num_vertices);
            for(size_t i = 0; i < m_vertices.size(); ++i) {
                m_vertices[i] = vertices[i];
            }
            
            m_tiva.reset(new btTriangleIndexVertexArray(
                                                        static_cast<int>(num_indices / 3), &m_bulletIndices[0], 3 * sizeof(int),
                                                        static_cast<int>(num_vertices), &m_vertices[0][0], sizeof(vec3f)));
            
            btBvhTriangleMeshShape* bvh = new btBvhTriangleMeshShape(m_tiva.get(),true);
            m_concaveShapeObserver = bvh;
            m_concaveShapeObserver->setLocalScaling(btVector3(1.0f,1.0f,1.0f));
            set_shape(bvh);
            m_gimpactShape.reset();
        }
        // gimpactShape->setMargin(0.05);
    }
    
private:
    shared_ptr<btConcaveShape>                m_gimpactShape;
    // Non-owning alias. The dynamic shape is owned by m_gimpactShape and the
    // static shape by DB_CollisionShape_t::m_shape.
    btConcaveShape*                          m_concaveShapeObserver;
    std::vector<vec3f>                      m_vertices;
    std::vector<vec3f>                      m_normals;
    std::vector<unsigned int>               m_indices;
    std::vector<int>                        m_bulletIndices;
    bool                                    m_dynamicMesh;
    shared_ptr<btTriangleIndexVertexArray>  m_tiva;
    
    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
};
