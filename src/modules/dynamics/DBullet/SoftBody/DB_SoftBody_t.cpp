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
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/bt_soft_body.cpp

//
//  DB_SoftBody_t.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DB_SoftBody_t.h"
#include "dbCollisionShape_t.h"
#include "Collision/DCollisionMeshInput.h"
#include "DSoftBodyPreparation.h"

#ifdef WIN32//for glut.h
#include <windows.h>
#endif

namespace {
std::vector<float> softBodyCoordinates(const btSoftBody& body)
{
    std::vector<float> coordinates(3*body.m_nodes.size());
    for (int i=0;i<body.m_nodes.size();++i)
        for (int axis=0;axis<3;++axis) coordinates[3*i+axis]=body.m_nodes[i].m_x[axis];
    return coordinates;
}

}
//think different
#if defined(__APPLE__) && !defined (VMDMESA)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#endif



DB_SoftBody_t::DB_SoftBody_t(void):
m_body(0),
numTriangles(0),
numVertices(0)
{
}


DB_SoftBody_t::DB_SoftBody_t(btSoftBodyWorldInfo &worldInfo, const std::vector<float> &triVertexCoords,
                           const std::vector<int> &triVertexIndices, float collisionMargin  )
    : DB_SoftBody_t(worldInfo, triVertexCoords, triVertexIndices,
                   dommy::bullet::SoftBodyParameters(100, .5f, collisionMargin, 100))
{
}

DB_SoftBody_t::DB_SoftBody_t(btSoftBodyWorldInfo &worldInfo, const std::vector<float> &triVertexCoords,
                           const std::vector<int> &triVertexIndices,
                           const dommy::bullet::SoftBodyParameters& parameters,
                           const DBulletSoftBodyWorldReferencePtr& worldReference)
    : m_body(0), m_worldReference(worldReference), m_managedWorld(bool(worldReference)),
      numTriangles(0), numVertices(0)
{
    if (worldReference && worldReference->get() != &worldInfo) return;
    std::size_t clusterWork = 0;
    if (!dommy::bullet::hasValidSoftBodyMeshInput(
            triVertexCoords, triVertexIndices) ||
        !dommy::bullet::validSoftBodyParameters(parameters) ||
        (parameters.mass > 0 && !dommy::bullet::softBodyClusterWorkBudget(
            triVertexCoords.size()/3, triVertexIndices.size()/3,
            parameters.clusters, clusterWork))) {
        return;
    }
    dommy::bullet::SoftBodyTopology topology;
    if (!dommy::bullet::prepareSoftBodyTopology(triVertexCoords,triVertexIndices,topology)) return;
    this->numTriangles = static_cast<int>(triVertexIndices.size() / 3U);
    this->numVertices = static_cast<int>(triVertexCoords.size() / 3U);
    
    this->triVertCoords.resize(triVertexCoords.size());
    this->triVertIndices.resize(triVertexIndices.size());
    
    // copy the data from vectors to arrays
    for(std::size_t i = 0; i < triVertexCoords.size(); i ++)
    {
        this->triVertCoords[i] = static_cast<btScalar>(triVertexCoords[i]);
    }
    
    for(std::size_t i = 0; i < triVertexIndices.size(); i ++)
    {
        this->triVertIndices[i] = triVertexIndices[i];
    }
    
    std::vector<btVector3> positions(numVertices);
    for (int i=0;i<numVertices;++i)
        positions[i]=btVector3(triVertexCoords[3*i],triVertexCoords[3*i+1],triVertexCoords[3*i+2]);
    this->m_body.reset(new btSoftBody(&worldInfo,numVertices,positions.data(),nullptr));
    if (!this->m_body) {
        this->triVertCoords.clear();
        this->triVertIndices.clear();
        this->numTriangles = 0;
        this->numVertices = 0;
        return;
    }
    m_massWeights.swap(topology.weights);
    for (const auto& edge:topology.edges) m_body->appendLink(edge.first,edge.second);
    for (std::size_t i=0;i<triVertIndices.size();i+=3)
        m_body->appendFace(triVertIndices[i],triVertIndices[i+1],triVertIndices[i+2]);
    // Keep both historical randomization passes and distance-2 link ordering.
    m_body->randomizeConstraints();
    
    btSoftBody::Material*    pm=this->m_body->appendMaterial();
    pm->m_kLST                =    0.5;
    pm->m_flags                -=    btSoftBody::fMaterial::DebugDraw;
    for (const auto& edge:topology.bends) {
        m_body->appendLink(edge.second,edge.first,pm);
        m_body->m_links[m_body->m_links.size()-1].m_bbending=-1; // Signed one-bit flag in Bullet 2.88.
    }
    this->m_body->m_cfg.piterations    =    3;
    this->m_body->m_cfg.kDF            =    0.5;
    
    // enable cluster collisions with other soft bodies and self
    
    this->m_body->m_cfg.collisions += (btSoftBody::fCollision::CL_SS + btSoftBody::fCollision::CL_SELF);
    // this->m_body->m_cfg.collisions += btSoftBody::fCollision::VF_SS;
    
    // Configure the candidate before it can be registered with a solver.
    if (!set_parameters(parameters)) {
        m_body.reset();
        return;
    }
    
    this->m_body->randomizeConstraints();

    // Establish rest geometry once, before publication. Runtime mass/cluster
    // edits must not ask predictMotion() to reset it to a deformed shape.
    m_body->updateConstants();
    for (int i=0;i<m_body->m_links.size();++i) {
        const btSoftBody::Link& link=m_body->m_links[i];
        if (!std::isfinite(link.m_c0) || !std::isnormal(link.m_c1)) {
            m_body.reset();
            return;
        }
    }
    m_body->m_fdbvt.clear();
    if (m_body->m_cfg.collisions & btSoftBody::fCollision::VF_SS)
        m_body->initializeFaceTree();
    m_body->m_bUpdateRtCst=false;
}

DB_SoftBody_t::~DB_SoftBody_t(void)
{
}

void DB_SoftBody_t::get_transform(mat4x4f &xform) const
{
    if (!body()) return;
    float m[16];
    m_body->getWorldTransform().getOpenGLMatrix(m);
    xform = mvl::trans(mvl::cmat<float, 4, 4>(m));
}

bool DB_SoftBody_t::set_num_clusters(std::size_t num)
{
    return set_mass_and_clusters(m_totalMass, num, false);
}

bool DB_SoftBody_t::set_parameters(const dommy::bullet::SoftBodyParameters& parameters)
{
    if (!dommy::bullet::validSoftBodyParameters(parameters) ||
        !set_mass_and_clusters(parameters.mass, parameters.clusters, false)) return false;
    set_dynamic_friction_coeff(parameters.friction);
    set_collision_margin(parameters.collisionMargin);
    return true;
}

void DB_SoftBody_t::get_transform(vec3f &position, quatf &rotation) const
{
    if (!body()) return;
    const btTransform& btxform = m_body->getWorldTransform();
    btQuaternion q = btxform.getRotation();
    btVector3 p = btxform.getOrigin();
    position = vec3f(p.x(), p.y(), p.z());
    rotation = quatf(q.w(), q.x(), q.y(), q.z());
}

void DB_SoftBody_t::set_transform(vec3f const &position, quatf const &rotation)
{
    if (!body() || !dommy::bullet::isFiniteVec3(position) ||
        !dommy::bullet::hasValidQuaternion(rotation)) return;
    vec3f tp = position;
    quatf tr = rotation;
    btTransform xform(btQuaternion(tr[1], tr[2], tr[3], tr[0]),
                      btVector3(tp[0], tp[1], tp[2]));
    // m_body->setWorldTransform(xform);
    m_body->transform(xform);
    
}


bool DB_SoftBody_t::set_mass(float mass, bool fromFaces)
{
    return set_mass_and_clusters(mass, m_clusterCount, fromFaces);
}

bool DB_SoftBody_t::set_mass_and_clusters(float mass, std::size_t clusters, bool fromFaces)
{
    if (!body() || !dommy::bullet::isSafeInverseOperand(mass) ||
        clusters < 1 || clusters > 8192) return false;
    if (mass==m_totalMass && clusters==m_clusterCount && !fromFaces) return true;
    const bool massChanged=m_clusterCount==0 || mass!=m_totalMass || fromFaces;
    std::size_t clusterWork = 0;
    if (mass > 0 && !dommy::bullet::softBodyClusterWorkBudget(
            m_body->m_nodes.size(), triVertIndices.size()/3, clusters, clusterWork)) return false;
    std::vector<double> weights=m_massWeights;
    const std::vector<float> coordinates=softBodyCoordinates(*m_body);
    if (fromFaces) {
        if (!dommy::bullet::softBodyMassWeights(coordinates,triVertIndices,weights)) return false;
    }
    std::vector<float> values;
    if (!dommy::bullet::prepareSoftBodyMass(mass,weights,values) ||
        values.size()!=std::size_t(m_body->m_nodes.size())) return false;
    if (mass>0 && !dommy::bullet::softBodyClusterMassRange(coordinates,triVertIndices,values)) return false;
    if (massChanged) {
        // Match Bullet's scalar arithmetic before changing any inverse mass.
        // Only the mass-dependent link coefficient is refreshed at runtime.
        for (int i=0;i<m_body->m_links.size();++i) {
            const btSoftBody::Link& link=m_body->m_links[i];
            if (!link.m_material || !std::isfinite(link.m_material->m_kLST) ||
                link.m_material->m_kLST<=0) return false;
            const btScalar a=values[link.m_n[0]-&m_body->m_nodes[0]];
            const btScalar b=values[link.m_n[1]-&m_body->m_nodes[0]];
            const btScalar inverseA=a>0 ? btScalar(1)/a : btScalar(0);
            const btScalar inverseB=b>0 ? btScalar(1)/b : btScalar(0);
            if (!std::isfinite((inverseA+inverseB)/link.m_material->m_kLST)) return false;
        }
    }
    std::vector<btScalar> previous(values.size());
    for (std::size_t i=0;i<values.size();++i) previous[i]=m_body->m_nodes[int(i)].m_im;
    const bool pendingInitialization=m_body->m_bUpdateRtCst;
    if (massChanged) {
        for (std::size_t i=0;i<values.size();++i) m_body->setMass(int(i),values[i]);
        // setMass marks *all* rest constants dirty. Preserve only an existing
        // initialization request, including on a rejected cluster update.
        m_body->m_bUpdateRtCst=pendingInitialization;
    }
    if (mass==0) {
        // Fixed vertices must not retain dynamic collision clusters.
        m_body->releaseClusters();
        for (int i=0;i<m_body->m_nodes.size();++i) {
            btSoftBody::Node& node=m_body->m_nodes[i];
            node.m_v.setZero();
            node.m_f.setZero();
            node.m_q=node.m_x;
        }
    } else {
        m_body->generateClusters(static_cast<int>(clusters),
            dommy::bullet::kMaxSoftClusterGenerationIterations);
        if (!dommy::bullet::validSoftBodyClusters(*m_body)) {
            for (std::size_t i=0;i<previous.size();++i) m_body->m_nodes[int(i)].m_im=previous[i];
            m_body->releaseClusters();
            if (m_totalMass>0 && m_clusterCount) m_body->generateClusters(static_cast<int>(m_clusterCount),
                dommy::bullet::kMaxSoftClusterGenerationIterations);
            return false;
        }
    }
    if (massChanged) m_body->updateLinkConstants();
    m_massWeights.swap(weights);
    m_totalMass=mass;
    m_clusterCount=clusters;
    return true;
}

void DB_SoftBody_t::set_dynamic_friction_coeff(float f)
{
    if(body() && std::isfinite(static_cast<double>(f)) && f >= 0.0f && f <= 1.0f)
    {
        this->m_body->m_cfg.kDF = f;
    }
}


btSoftBody* DB_SoftBody_t::body() const
{
    if (m_managedWorld) {
        const DBulletSoftBodyWorldReferencePtr reference = m_worldReference.lock();
        if (!reference || !reference->get()) return nullptr;
    }
    return this->m_body.get();
}

bool DB_SoftBody_t::belongs_to_world(const DBulletSoftBodyWorldReferencePtr& reference) const
{
    return body() && m_managedWorld && reference && m_worldReference.lock() == reference;
}

void DB_SoftBody_t::get_mesh(std::vector<int> &triIndices, std::vector<vec3f> &triCoords)
{
    if (!body()) {
        triIndices.clear();
        triCoords.clear();
        return;
    }
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

vec3f DB_SoftBody_t::get_center()
{
    vec3f total(0,0,0);
    if (!body() || m_body->m_nodes.size() == 0) return total;
    btSoftBody::tNodeArray &nodes = this->m_body->m_nodes;
    
    // sum and average over current node coordinates
    for(int i = 0; i < nodes.size(); i++)
    {
        total[0] += nodes[i].m_x[0];
        total[1] += nodes[i].m_x[1];
        total[2] += nodes[i].m_x[2];
    }
    return(total / nodes.size());
}

vec3f DB_SoftBody_t::get_average_velocity()
{
    vec3f total(0,0,0);
    if (!body() || m_body->m_nodes.size() == 0) return total;
    btSoftBody::tNodeArray &nodes = this->m_body->m_nodes;
    
    for(int i = 0; i < nodes.size(); i++)
    {
        total[0] += nodes[i].m_v[0];
        total[1] += nodes[i].m_v[1];
        total[2] += nodes[i].m_v[2];
    }
    return(total / nodes.size());
    
}

float DB_SoftBody_t::get_total_mass(void)
{
    return body() ? this->m_body->getTotalMass() : 0.0f;
}

void DB_SoftBody_t::apply_central_force(const vec3f &force)
{
    if (!body() || !dommy::bullet::isFiniteVec3(force)) return;
    btVector3 btForce(force[0], force[1], force[2]);
    this->m_body->addForce(btForce);
}

void DB_SoftBody_t::set_gravity(const vec3f& grav)
{
    if (!body() || !dommy::bullet::isFiniteVec3(grav)) return;
    btVector3 btGrav(grav[0], grav[1], grav[2]);
    btSoftBodyWorldInfo* wInfo = this->m_body->getWorldInfo();
    wInfo->m_gravity = btGrav;
    
}

void DB_SoftBody_t::gl_draw(std::size_t draw_style) {
    if (!body()) return;
    glPushMatrix();
    
    if(draw_style & dbCollisionShape_t::kDSSolid) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < this->numTriangles; i ++)
    {
        float x[3];
        float y[3];
        float z[3];
        // current index into the vertex list (i * 3 since each triangle has 3 indices)
        int j = i * 3;
        
        // retrieve coordinates from current soft body node set
        x[0] = this->m_body->m_nodes[this->triVertIndices[j]].m_x[0];
        y[0] = this->m_body->m_nodes[ this->triVertIndices[j]].m_x[1];
        z[0] = this->m_body->m_nodes[ this->triVertIndices[j]].m_x[2];
        
        x[1] = this->m_body->m_nodes[ this->triVertIndices[j + 1]].m_x[0];
        y[1] = this->m_body->m_nodes[ this->triVertIndices[j + 1]].m_x[1];
        z[1] = this->m_body->m_nodes[ this->triVertIndices[j + 1]].m_x[2];
        
        x[2] = this->m_body->m_nodes[ this->triVertIndices[j + 2]].m_x[0];
        y[2] = this->m_body->m_nodes[ this->triVertIndices[j + 2]].m_x[1];
        z[2] = this->m_body->m_nodes[ this->triVertIndices[j + 2]].m_x[2];
        
        glVertex3f(x[0], y[0], z[0]);
        glVertex3f(x[1], y[1], z[1]);
        glVertex3f(x[2], y[2], z[2]);
        
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();
    
}


void DB_SoftBody_t::set_collision_margin(float margin)
{
    if(body() && dommy::bullet::isSafeSoftBodyCollisionMargin(margin))
    {
        this->m_body->getCollisionShape()->setMargin(margin);
    }
    
}
