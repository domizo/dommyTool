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
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_convex_hull_shape.h

//
//  bt_convex_hull_shape.h
//  dommyTool
//
//  Created by yuzo on 2017/09/29.
//

#pragma once


#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "convex_hull_shape_impl.h"
#include "DB_CollisionShape_t.h"
#include "DShapeFrameMath.h"

class bt_convex_hull_shape_t: public DB_CollisionShape_t
{
public:
    void gl_draw(size_t draw_style) override {
        
        if(m_vertices.empty() || m_indices.empty()) return;
        
        glPushMatrix();
        glTranslatef(m_center[0], m_center[1], m_center[2]);
        float angle;
        vec3f axis;
        q_to_axis_angle(m_rotation, axis, angle);
        if (angle != 0.0f) glRotatef(rad2deg(angle), axis[0], axis[1], axis[2]);
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
    }
    
    scale_update prepare_scale(vec3f const& s1, bool force = false) override {
        if (!shape() || !m_ch_shape ||
            shape()->getShapeType() != COMPOUND_SHAPE_PROXYTYPE ||
            !std::isfinite(s1[0]) || !std::isfinite(s1[1]) ||
            !std::isfinite(s1[2])) {
            return scale_update();
        }
        btCompoundShape* compound =
            static_cast<btCompoundShape*>(shape());
        if (compound->getNumChildShapes() != 1 ||
            compound->getChildShape(0) != m_ch_shape.get() ||
            m_sourceVertices.size() !=
                static_cast<std::size_t>(m_ch_shape->getNumPoints())) {
            return scale_update();
        }

        const btVector3 s(s1[0], s1[1], s1[2]);
        if (!force && s == m_cachedScaling) return unchanged_scale();

        // Stage all allocations, transformed coordinates and mass properties
        // before touching the live Bullet points or the displayed geometry.
        const btTransform tr = compound->getChildTransform(0);
        const btTransform inverse = tr.inverse();
        struct Staged { std::vector<vec3f> vertices, normals; };
        const std::shared_ptr<Staged> staged(new Staged());
        std::vector<vec3f>& vertices = staged->vertices;
        vertices.resize(m_sourceVertices.size());
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const vec3f& source = m_sourceVertices[i];
            const btVector3 parent = tr * btVector3(source[0], source[1], source[2]);
            vec3f scaled;
            if (!dommy::bullet::scaledCollisionPoint(
                    vec3f(parent.x(), parent.y(), parent.z()), s1, scaled)) return scale_update();
            const btVector3 local = inverse * btVector3(scaled[0], scaled[1], scaled[2]);
            if (!dommy::bullet::scaledCollisionPoint(
                    vec3f(local.x(), local.y(), local.z()), vec3f(1,1,1), vertices[i])) return scale_update();
        }
        dommy::bullet::MassProperties properties;
        vec3f inertia(0,0,0);
        if (!dommy::bullet::isSingularScale(s1) &&
            (!dommy::bullet::computeMassProperties(vertices.data(), vertices.size(),
                 m_indices.data(), m_indices.size(), properties) ||
             !dommy::bullet::inertiaDiagonalInFrame(properties.inertia, m_rotation, inertia))) return scale_update();
        mat3x3f parentToLocal;
        q_to_mat(qconj(m_rotation), parentToLocal);
        std::vector<vec3f>& normals = staged->normals;
        normals.resize(m_sourceNormals.size());
        for (std::size_t i = 0; i < normals.size(); ++i) {
            normals[i] = prod(parentToLocal,
                dommy::bullet::scaledDisplayNormal(m_sourceNormals[i], s1));
        }

        scale_update update(new collision_shape_scale_update(
            [this, staged, compound, tr, s, properties, inertia]() {
                const std::vector<vec3f>& points = staged->vertices;
                for (std::size_t i = 0; i < points.size(); ++i) {
                    m_ch_shape->getUnscaledPoints()[i] =
                        btVector3(points[i][0], points[i][1], points[i][2]);
                }
                m_ch_shape->recalcLocalAabb();
                // Refresh the child tree as well as the parent AABB.
                compound->updateChildTransform(0, tr);
                m_vertices.swap(staged->vertices);
                m_normals.swap(staged->normals);
                m_cachedScaling = s;
                m_volume = properties.volume;
                m_local_inertia = inertia;
            }));
        update->volume = properties.volume;
        update->inertia = inertia;
        if (!prepare_bounds(*update, vertices, tr, 2 * m_ch_shape->getMargin())) return scale_update();
        return update;
    }
    
    void get_scale(vec3f& s) override {
        s = vec3f(m_cachedScaling.x(), m_cachedScaling.y(), m_cachedScaling.z());
    }
    
    float volume() override                  { return m_volume;  }
    vec3f const& local_inertia() override    { return m_local_inertia;  }
    vec3f const& center() override           { return m_center; }
    quatf const& rotation() override         { return m_rotation;  }
    
protected:
    friend class DB_BulletSolver_t;
    
    bt_convex_hull_shape_t(vec3f const* vertices, size_t num_vertices,
                           vec3f const* normals,
                           unsigned int const *indices, size_t num_indices):
    DB_CollisionShape_t(),
    m_sourceNormals(normals, normals + num_vertices),
    m_normals(normals, normals + num_vertices),
    m_indices(indices, indices + num_indices),
    m_volume(0.0f),
    m_center(0.0f, 0.0f, 0.0f),
    m_rotation(mvl::qidentity<float>()),
    m_local_inertia(0.0f, 0.0f, 0.0f),
    m_cachedScaling(1.0f, 1.0f, 1.0f)
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

        // The compound restores input-mesh axes; the body uses those axes too.
        m_local_inertia = diag(I);

        m_vertices.resize(num_vertices);
        for(size_t i = 0; i < m_vertices.size(); ++i)
        {
            m_vertices[i] = prod(Qinv, vertices[i] - m_center);
            m_normals[i] = prod(Qinv, dommy::bullet::scaledDisplayNormal(
                m_sourceNormals[i], vec3f(1.0f, 1.0f, 1.0f)));
        }
        
        if (num_vertices >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return;
        }
        m_sourceVertices = m_vertices;
        m_ch_shape.reset(new btConvexHullShape(
            &(m_vertices[0][0]), static_cast<int>(num_vertices),
            sizeof(vec3f)));
        std::unique_ptr<btCompoundShape> compound_shape(new btCompoundShape);
        compound_shape->addChildShape(btTransform(btQuaternion(m_rotation[1],
                                                               m_rotation[2],
                                                               m_rotation[3],
                                                               m_rotation[0]),
                                                  btVector3(m_center[0],
                                                            m_center[1],
                                                            m_center[2])),
                                      m_ch_shape.get());
        set_shape(compound_shape.release());
    }
    
private:
    shared_ptr<btConvexHullShape>   m_ch_shape;
    std::vector<vec3f>              m_sourceVertices;
    std::vector<vec3f>              m_sourceNormals;
    std::vector<vec3f>              m_vertices;
    std::vector<vec3f>              m_normals;
    std::vector<unsigned int>       m_indices;
    
    
    float                           m_volume;
    vec3f                           m_center;
    quatf                           m_rotation;
    vec3f                           m_local_inertia;
    btVector3                        m_cachedScaling;
};
