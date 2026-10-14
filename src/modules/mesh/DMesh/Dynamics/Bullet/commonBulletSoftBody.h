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
 
This file was written by Francisco Gochez <fjgochez@gmail.com>, and was not part of the original
plugin.  It is based on bt_rigid_body.cpp
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
 
This file was written by Francisco Gochez <fjgochez@gmail.com>, and was not part of the original
plugin.  It is based on bt_rigid_body.h
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_soft_body.cpp
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_soft_body.h

//
//  commonBulletSoftBody.h
//  dommyTool
//
//  Created by yuzo on 2017/10/10.
//

#pragma once

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "btBulletCollisionCommon.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"

#include "shared_ptr.h"
#include "mathUtils.h"
#include "DBulletUserData.h"
#include "SoftBody/DSoftBodyPreparation.h"

#include <memory>




struct btSoftParams
{
    btSoftParams()
    : m_mass(1.0)
    , m_volumeMass(100)
    , m_stiffness(0.5)
    , m_angular_stiffness(0.5)
    , m_volume_stiffness(0.5)
    , m_is_bend_constraint(false)
    , m_is_cluster(false)
    , m_rest_length_scale(1.0)
    , m_restitution(0.1)
    , m_friction(0.5)
    , m_rolling_friction(0.0)
    , m_spinning_friction(0.0)
    , m_aeromodel(0)
    //    , m_linearDamping(0.3)
    //    , m_angularDamping(0.3)
    , m_VelocityCorrectionFactor(1)
    , m_dynamicFrictionCoeff(0.2)
    , m_Damping(0)
    , m_Drag(0)
    , m_Lift(0)
    , m_Pressure(0)
    , m_VolumeConversation(0)
    , m_PoseMatch(0)
    , m_RigidContactHardness(1.0)
    , m_KineticContactHardness(0.1)
    , m_SoftContactHardness(1.0)
    , m_AnchorContactHardness(0.7)
    , m_rigid_impulse_split(0.5)
    , m_kinetict_impulse_split(0.5)
    , m_soft_impulse_split(0.5)
    , m_set_pose_frame(false)
    , m_set_pose_volume(false)
    , m_isRandomizeConstraints(true)
    , m_bendConstraintDistance(2)
    , m_numClusters(100)
    , m_collide_rigid(true)
    , m_collide_soft(true)
    , m_collide_self(true)
    , m_collisionMargin(0.04)
    , m_positionIterations(3)
    , m_velocityIterations(0)
    , m_driftIterations(0)
    , m_clusterIterations(4)
    , m_max_volume(1.0)
    , m_scale(1.0)
    { }
    
    float m_mass;
    float m_volumeMass;
    
    double m_stiffness;
    double m_angular_stiffness;
    double m_volume_stiffness;
    
    bool m_is_bend_constraint;
    bool m_is_cluster;
    
    double m_rest_length_scale;
    double m_restitution;
    double m_friction;
    double m_rolling_friction;
    double m_spinning_friction;
    
    
    short m_aeromodel;
    
    //    double m_linearDamping;
    //    double m_angularDamping;
    
    //    MVector m_initPos;
    //    MVector m_initRot;
    //    MVector m_initVel;
    //    MVector m_initSpin;
    
    
    double m_VelocityCorrectionFactor;
    double m_dynamicFrictionCoeff;
    double m_Damping;
    double m_Drag;
    double m_Lift;
    double m_Pressure;
    double m_VolumeConversation;
    double m_PoseMatch;
    
    double m_RigidContactHardness;
    double m_KineticContactHardness;
    double m_SoftContactHardness;
    double m_AnchorContactHardness;
    
    double m_rigid_impulse_split;
    double m_kinetict_impulse_split;
    double m_soft_impulse_split;
    
    
    bool m_set_pose_frame;
    bool m_set_pose_volume;
    bool m_isRandomizeConstraints;
    
    
    int m_bendConstraintDistance;
    int m_numClusters;
    
    bool m_collide_rigid;
    bool m_collide_soft;
    bool m_collide_self;
    
    
    float m_collisionMargin;
    int m_positionIterations;
    int m_velocityIterations;
    int m_driftIterations;
    int m_clusterIterations;
    
    double m_max_volume;
    float m_scale;
    //    MVector m_collisionScale;
};





class db_softbody_base
{
public:
    db_softbody_base(btSoftBodyWorldInfo &worldInfo, const std::vector<float> &triVertexCoords,
                     const std::vector<int> &triVertexIndices ) {
        if (triVertexCoords.empty() || triVertexCoords.size() % 3U != 0U ||
            triVertexIndices.empty() || triVertexIndices.size() % 3U != 0U ||
            triVertexCoords.size() / 3U >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            triVertexIndices.size() / 3U >
                static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument(
                "Soft-body mesh requires finite vertices and complete triangles.");
        }

        const std::size_t vertexCount = triVertexCoords.size() / 3U;
        std::size_t denseEntries;
        if (vertexCount > dommy::bullet::kMaxSoftBodyVertices ||
            triVertexIndices.size() / 3 > dommy::bullet::kMaxSoftBodyFaces ||
            !dommy::generation::checkedProduct(vertexCount, vertexCount,
                dommy::bullet::kMaxSoftBodyWork, denseEntries))
            throw std::invalid_argument("Soft-body triangle helper budget exceeded.");
        std::vector<double> weights;
        if (!dommy::bullet::softBodyMassWeights(triVertexCoords, triVertexIndices, weights))
            throw std::invalid_argument("Soft-body mesh has degenerate faces or unused vertices.");
        
        this->numTriangles = static_cast<int>(triVertexIndices.size() / 3U);
        this->numVertices = static_cast<int>(triVertexCoords.size() / 3U);
        
        this->triVertCoords.resize(triVertexCoords.size());
        this->triVertIndices.resize(triVertexIndices.size());
        
        // copy the data from vectors to arrays
        for(std::size_t i = 0; i < triVertexCoords.size(); ++i)
        {
            this->triVertCoords[i] = static_cast<btScalar>(triVertexCoords[i]);
        }
        
        for(std::size_t i = 0; i < triVertexIndices.size(); ++i)
        {
            this->triVertIndices[i] = triVertexIndices[i];
        }
        
        this->m_body.reset(btSoftBodyHelpers::CreateFromTriMesh(
            worldInfo,
            this->triVertCoords.empty() ? 0 : &this->triVertCoords[0],
            this->triVertIndices.empty() ? 0 : &this->triVertIndices[0],
            this->numTriangles));
        if (!this->m_body || this->m_body->m_nodes.size() == 0) {
            throw std::runtime_error(
                "Bullet failed to create a soft body from the mesh.");
        }
        
    }
    
    ~db_softbody_base(void)
    {
        if(m_body.get()) {
            m_body->setUserPointer(0);
        }
    }

    void set_user_data(customDBData* userData)
    {
        m_userData.reset(userData);
        if(m_body.get()) {
            m_body->setUserPointer(m_userData.get());
        }
    }
    
    
    void get_transform(mat4x4f &xform) const
    {
        float m[16];
        m_body->getWorldTransform().getOpenGLMatrix(m);
        xform = mvl::trans(mvl::cmat<float, 4, 4>(m));
    }
    
    void set_num_clusters(std::size_t num)
    {
        std::size_t work = 0;
        if (!m_body || !dommy::bullet::softBodyClusterWorkBudget(
                m_body->m_nodes.size(), m_body->m_faces.size(), num, work)) return;
        m_body->generateClusters(static_cast<int>(num),
            dommy::bullet::kMaxSoftClusterGenerationIterations);
    }
    
    
    void get_transform(vec3f &position, quatf &rotation) const
    {
        const btTransform& btxform = m_body->getWorldTransform();
        btQuaternion q = btxform.getRotation();
        btVector3 p = btxform.getOrigin();
        position = vec3f(p.x(), p.y(), p.z());
//        rotation = quatf(q.x(), q.y(), q.z(, q.w()));
        rotation = quatf(q.w(), q.x(), q.y(), q.z());
    }
    
    void set_transform(vec3f const &position, quatf const &rotation)
    {
        vec3f tp = position;
        quatf tr = rotation;
        btTransform xform(btQuaternion(tr[1], tr[2], tr[3], tr[0]),
                               btVector3(tp[0], tp[1], tp[2]));
//        btTransform xform(btQuaternion(tr[0], tr[1], tr[2], tr[3]),
//                          btVector3(tp[0], tp[1], tp[2]));
        // m_body->setWorldTransform(xform);
        m_body->transform(xform);
        
    }
    
    
    void set_mass(float mass, bool fromFaces)
    {
        this->m_body->setTotalMass(mass, fromFaces);
    }
    
    void set_dynamic_friction_coeff(float f)
    {
        if(f >= 0.0f && f <= 1.0f)
        {
            this->m_body->m_cfg.kDF = f;
        }
    }
    
    
    btSoftBody* body()
    {
        return this->m_body.get();
    }

    // Initialization only. Bullet 2.88 unconditionally inverts the covariance
    // in setPose(), even with both features disabled or with zero total mass.
    void initialize_pose(bool volume, bool frame)
    {
        if (m_body->getTotalMass() == 0 || (!volume && !frame)) return;
        if (frame) {
            const btScalar total = m_body->getTotalMass();
            btVector3 center(0, 0, 0);
            std::vector<btScalar> weights(m_body->m_nodes.size());
            for (int i = 0; i < m_body->m_nodes.size(); ++i) {
                const btScalar product = m_body->m_nodes[i].m_im * total;
                if (!dommy::bullet::isSafeInverseOperand(product) || product == 0)
                    throw std::invalid_argument("Soft-body pose mass is outside the safe range.");
                weights[i] = btScalar(1) / product;
                center += m_body->m_nodes[i].m_x * weights[i];
            }
            btMatrix3x3 covariance(0, 0, 0, 0, 0, 0, 0, 0, 0);
            for (int i = 0; i < m_body->m_nodes.size(); ++i) {
                const btVector3 q = m_body->m_nodes[i].m_x - center;
                const btVector3 weighted = weights[i] * q;
                for (int axis = 0; axis < 3; ++axis) covariance[axis] += weighted[axis] * q;
            }
            const btScalar determinant = covariance.determinant();
            if (!dommy::bullet::isSafeInverseOperand(determinant) || determinant == 0)
                throw std::invalid_argument("Soft-body frame pose requires nonsingular 3D geometry.");
            const btMatrix3x3 inverse = covariance.inverse();
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    if (!std::isfinite(inverse[row][column]))
                        throw std::invalid_argument("Soft-body pose inverse is outside the safe range.");
            m_body->setPose(volume, true);
        } else {
            // A volume-only pose does not need the frame covariance inverse.
            m_body->m_pose.m_bvolume = volume;
            m_body->m_pose.m_volume = volume ? m_body->getVolume() : 0;
        }
    }
    
    void get_mesh(std::vector<int> &triIndices, std::vector<vec3f> &triCoords)
    {
        triIndices.resize(this->numTriangles * 3);
        for(int i = 0; i < this->numTriangles * 3; i++)
        {
            triIndices[i] = this->triVertIndices[i];
        }
        
        btSoftBody::tNodeArray &nodes = this->m_body->m_nodes;
        triCoords.resize(nodes.size());
        for(int i = 0; i < nodes.size(); i ++)
        {
            vec3f pt(nodes[i].m_x[0], nodes[i].m_x[1], nodes[i].m_x[2] );
            triCoords[i] = pt;
        }
    }
    
    
    vec3f get_center()
    {
        vec3f total(0,0,0);
        btSoftBody::tNodeArray &nodes = this->m_body->m_nodes;
        
        // sum and average over current node coordinates
        for(int i = 0; i < nodes.size(); i++)
        {
            total[0] += nodes[i].m_x[0];
            total[1] += nodes[i].m_x[1];
            total[2] += nodes[i].m_x[2];
        }
        return nodes.size() == 0 ? vec3f(0,0,0) : (total / nodes.size());
    }
    
    vec3f get_average_velocity()
    {
        vec3f total(0,0,0);
        btSoftBody::tNodeArray &nodes = this->m_body->m_nodes;
        
        for(int i = 0; i < nodes.size(); i++)
        {
            total[0] += nodes[i].m_v[0];
            total[1] += nodes[i].m_v[1];
            total[2] += nodes[i].m_v[2];
        }
        return nodes.size() == 0 ? vec3f(0,0,0) : (total / nodes.size());
        
    }
    
    float get_total_mass(void)
    {
        return this->m_body->getTotalMass();
    }
    
    void apply_central_force(const vec3f &force)
    {
        btVector3 btForce(force[0], force[1], force[2]);
        this->m_body->addForce(btForce);
    }
    
    void set_gravity(const vec3f& grav)
    {
        btVector3 btGrav(grav[0], grav[1], grav[2]);
        btSoftBodyWorldInfo* wInfo = this->m_body->getWorldInfo();
        wInfo->m_gravity = btGrav;
        
    }
    
    
    void set_collision_margin(float margin)
    {
        if(dommy::bullet::isSafeSoftBodyCollisionMargin(margin))
        {
            this->m_body->getCollisionShape()->setMargin(margin);
        }
        
    }
    
public:
    
    
    
    
protected:
private:
    db_softbody_base(const db_softbody_base&) = delete;
    db_softbody_base& operator=(const db_softbody_base&) = delete;
    shared_ptr<btSoftBody> m_body;
    std::unique_ptr<customDBData> m_userData;
    
    std::vector<btScalar> triVertCoords;
    std::vector<int> triVertIndices;
    
    int numTriangles;
    int numVertices;
};





inline vec3f getDBSoftBodyCenter(btSoftBody *m_body)
{
    vec3f total(0,0,0);
    btSoftBody::tNodeArray &nodes = m_body->m_nodes;
    
    // sum and average over current node coordinates
    for(int i = 0; i < nodes.size(); i++)
    {
        total[0] += nodes[i].m_x[0];
        total[1] += nodes[i].m_x[1];
        total[2] += nodes[i].m_x[2];
    }
    return(total / nodes.size());
}

inline vec3f getDBSoftBodyAverageVelocity(btSoftBody *m_body)
{
    vec3f total(0,0,0);
    btSoftBody::tNodeArray &nodes = m_body->m_nodes;
    
    for(int i = 0; i < nodes.size(); i++)
    {
        total[0] += nodes[i].m_v[0];
        total[1] += nodes[i].m_v[1];
        total[2] += nodes[i].m_v[2];
    }
    return(total / nodes.size());
    
}
