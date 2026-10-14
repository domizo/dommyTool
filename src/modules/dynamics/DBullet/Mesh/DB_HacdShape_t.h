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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_hacd_shape.h

//
//  DB_HacdShape_t.h
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//

#pragma once

#include <vector>
#include <memory>

#include "mesh_shape_impl.h"
#include "DB_CollisionShape_t.h"
#include "../Collision/DShapeFrameMath.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "convex_decomposition_hacd.h"

class DB_HacdShape_t: public DB_CollisionShape_t
{
public:
    virtual void gl_draw(size_t draw_style) override {
        
        if (!shape() || m_vertices.empty() || m_indices.empty()) return;
        
        const vec3f& scale = m_cachedScaling;
        glPushAttrib(GL_ENABLE_BIT);
        glEnable(GL_NORMALIZE);
        glPushMatrix();
        // HACD and the display buffer both retain input-mesh coordinates.
        glScalef(scale[0], scale[1], scale[2]);
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
        btCollisionShape* collisionShape = shape();
        if (!collisionShape || collisionShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE ||
            !dommy::bullet::isFiniteVector(s)) {
            return scale_update();
        }
        if (!force && s[0] == m_cachedScaling[0] && s[1] == m_cachedScaling[1] &&
            s[2] == m_cachedScaling[2]) return unchanged_scale();
        btCompoundShape* compound = static_cast<btCompoundShape*>(collisionShape);
        if (m_sourceChildOrigins.size() != static_cast<std::size_t>(compound->getNumChildShapes())) return scale_update();
        std::vector<vec3f> vertices, hullVertices;
        const std::shared_ptr<std::vector<vec3f> > origins(new std::vector<vec3f>());
        if (!dommy::bullet::prepareScaledCollisionVertices(m_vertices, s, vertices) ||
            !dommy::bullet::prepareScaledCollisionVertices(m_sourceChildOrigins, s, *origins)) return scale_update();
        dommy::bullet::MassProperties properties;
        if (!dommy::bullet::isSingularScale(s) &&
            !dommy::bullet::computeMassProperties(vertices.data(), vertices.size(),
                m_indices.data(), m_indices.size(), properties)) return scale_update();
        float margin = 0;
        // Validate every decomposed hull, not just the original display mesh.
        for (int i = 0; i < compound->getNumChildShapes(); ++i) {
            btCollisionShape* child = compound->getChildShape(i);
            if (child->getShapeType() != CONVEX_HULL_SHAPE_PROXYTYPE) return scale_update();
            const btConvexHullShape* hull = static_cast<btConvexHullShape*>(child);
            margin = btMax(margin, 2 * hull->getMargin());
            for (int j = 0; j < hull->getNumPoints(); ++j) {
                const btVector3& point = hull->getUnscaledPoints()[j];
                vec3f scaled;
                if (!dommy::bullet::scaledCollisionPoint(
                        vec3f(point.x(), point.y(), point.z()), s, scaled)) return scale_update();
                for (int axis = 0; axis < 3; ++axis) {
                    if (!dommy::bullet::isSafeCollisionCoordinate(
                            static_cast<double>(scaled[axis]) + (*origins)[i][axis])) return scale_update();
                }
                hullVertices.push_back(scaled + (*origins)[i]);
            }
        }
        // Keep the compound's own scale at identity. Bullet's compound setter
        // divides by its previous scale and cannot recover from a zero axis.
        // HACD children have identity bases and immutable unscaled hull points.
        scale_update update(new collision_shape_scale_update([this, compound, origins, s, properties]() {
            for (int i = 0; i < compound->getNumChildShapes(); ++i) {
                compound->getChildShape(i)->setLocalScaling(btVector3(s[0], s[1], s[2]));
                btTransform transform = compound->getChildTransform(i);
                const vec3f& origin = (*origins)[i];
                transform.setOrigin(btVector3(origin[0], origin[1], origin[2]));
                compound->updateChildTransform(i, transform, false);
            }
            compound->recalculateLocalAabb();
            m_cachedScaling = s;
            m_volume = properties.volume;
            m_local_inertia = diag(properties.inertia);
        }));
        update->volume = properties.volume;
        update->inertia = diag(properties.inertia);
        if (!prepare_bounds(*update, hullVertices, btTransform::getIdentity(), margin)) return scale_update();
        return update;
    }
    
    virtual void get_scale(vec3f& s) override {
        if (!shape()) {
            s = vec3f(1.0f, 1.0f, 1.0f);
            return;
        }
        s = m_cachedScaling;
    }
    
    virtual float volume() override { return m_volume;  }
    virtual vec3f const& local_inertia() override { return m_local_inertia;  }
    virtual vec3f const& center() override { return m_center; }
    virtual quatf const& rotation() override { return m_rotation;  }
    
protected:
    friend class DB_BulletSolver_t;

    DB_HacdShape_t(vec3f const* vertices,
                    size_t num_vertices,
                    vec3f const* normals,
                    unsigned int const *indices,
                    size_t num_indices,
                    bool dynamicMesh):
    DB_CollisionShape_t(),
    m_vertices(),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices),
    m_cachedScaling(1.0f, 1.0f, 1.0f),
    m_volume(0.0f),
    m_center(0.0f, 0.0f, 0.0f),
    m_rotation(mvl::qidentity<float>()),
    m_local_inertia(0.0f, 0.0f, 0.0f)
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
        
        m_local_inertia = diag(I);
        
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
        
        
        convex_decomposition_hacd decomposition;
        std::unique_ptr<btCompoundShape> compound_shape(decomposition.ConvexDecomp(
            numHacdVertices, hacdVertices.data(),
            static_cast<int>(num_indices), indices));
        
        if (compound_shape && compound_shape->getNumChildShapes() > 0)
        {
            m_sourceChildOrigins.reserve(static_cast<std::size_t>(compound_shape->getNumChildShapes()));
            for (int i = 0; i < compound_shape->getNumChildShapes(); ++i) {
                const btVector3& origin = compound_shape->getChildTransform(i).getOrigin();
                m_sourceChildOrigins.push_back(vec3f(origin.x(), origin.y(), origin.z()));
            }
            set_shape(compound_shape.release());
        }
    }
    
private:
    std::vector<vec3f>                      m_vertices;
    std::vector<vec3f>                      m_normals;
    std::vector<unsigned int>               m_indices;
    std::vector<vec3f>                      m_sourceChildOrigins;
    vec3f                                  m_cachedScaling;

    float m_volume;
    vec3f m_center;
    quatf m_rotation;
    vec3f m_local_inertia;
};
