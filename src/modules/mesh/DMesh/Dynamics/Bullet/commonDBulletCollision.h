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
// Archive member: dynamica/trunk/Extras/MayaPlugin/collisionShapeNode.cpp

//
//  commonDBulletCollision.h
//  dommyTool
//
//  Created by yuzo on 2017/10/04.
//

#pragma once


#include "btBulletCollisionCommon.h"
#include "mathUtils.h"
#include "shared_ptr.h"

#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DBulletDataHeaders.h"

#include "db_collision_base_shapes.h"
#include "db_collision_mesh_shapes.h"
#include "../../../../dynamics/DBullet/Collision/DCollisionMeshInput.h"

#pragma mark -
#pragma mark colision_shape

template <typename MeshItemPtr>
inline db_collision_shape_base* createCollisionMeshShape(
    DBulletCollisionType colType, const MeshItemPtr* msh)
{
    if(!msh || !msh->get()) {
        return 0;
    }

    MStatus stat;
    MObject mObj;
    stat = generateMMeshFromDmesh(*msh->get(), mObj);
    if(stat != MS::kSuccess) {
        return 0;
    }
    
    MFnMesh meshFn(mObj, &stat);
    if (stat != MS::kSuccess) {
        return 0;
    }
//    MFnMesh meshFn;
//    MObject newObj = MFnMeshData().create();
//
//    meshFn.create((*msh)->numberOfPoints(), (*msh)->numFace(), (*msh)->getMPosition(), (*msh)->getFaceList(), (*msh)->getConnectList(), newObj, &stat);
//    CHECK_MSTATUS(stat);
    
    MFloatPointArray mpoints;
    MFloatVectorArray mnormals;
    MIntArray mtrianglecounts;
    MIntArray mtrianglevertices;
    
    stat = meshFn.getPoints(mpoints);
    if (stat == MS::kSuccess) stat = meshFn.getNormals(mnormals);
    if (stat == MS::kSuccess) {
        stat = meshFn.getTriangles(mtrianglecounts, mtrianglevertices);
    }
    if (stat != MS::kSuccess) {
        return 0;
    }

    if(mpoints.length() == 0 || mtrianglevertices.length() < 3) {
        return 0;
    }
    
    std::vector<vec3f> vertices(mpoints.length());
    std::vector<vec3f> normals(mpoints.length());
    std::vector<unsigned int> indices(mtrianglevertices.length());
    
    for(size_t i = 0; i < vertices.size(); ++i) {
        vertices[i] = vec3f(mpoints[i].x, mpoints[i].y, mpoints[i].z);
        normals[i] = i < mnormals.length()
            ? vec3f(mnormals[i].x, mnormals[i].y, mnormals[i].z)
            : vec3f(0.0f, 0.0f, 0.0f);
    }
    for(size_t i = 0; i < indices.size(); ++i) {
        if (mtrianglevertices[i] < 0) {
            return 0;
        }
        indices[i] = static_cast<unsigned int>(mtrianglevertices[i]);
    }

    if (!dommy::bullet::hasValidCollisionMeshInput(
            vertices.data(), vertices.size(), normals.data(),
            indices.data(), indices.size())) {
        return 0;
    }
    
    db_collision_shape_base* colShape = nullptr;
    
    
    switch (colType) {
        case D_COL_CONVEX_HULL:
        {
            colShape = new db_collision_convex_hull(vertices.data(), vertices.size(), normals.data(), indices.data(), indices.size());
        }
            break;
        case D_COL_MESH:
        {
            colShape = new db_collision_mesh(vertices.data(), vertices.size(), normals.data(), indices.data(), indices.size(), true);
        }
            break;
        case D_COL_BVH_MESH:
        {
            colShape = new db_collision_mesh(vertices.data(), vertices.size(), normals.data(), indices.data(), indices.size(), false);
        }
            break;
        case D_COL_HACD:
        {
            colShape = new db_collision_hacd(vertices.data(), vertices.size(), normals.data(), indices.data(), indices.size(), true);
        }
            break;
        default:
            break;
    }
    
    if (colShape && !colShape->shape()) {
        delete colShape;
        colShape = 0;
    }
    return colShape;
}

template <typename MeshItemPtr>
inline db_collision_shape_base* createCollisionShape(
    DBulletCollisionType colType, const MeshItemPtr* msh)
{
    db_collision_shape_base* colShape = nullptr;
    
    switch(colType)
    {
        case D_COL_BOX:
        {
            //box
            colShape = new db_collision_box();
        }
            break;
        case D_COL_SPHERE:
        {
            //sphere
            colShape = new db_collision_sphere(1.0);
        }
            break;
        case D_COL_PLANE:
        {
            //plane
            colShape = new db_collision_plane(vec3f(0.0, 1.0, 0.0), 0);
        }
            break;
        case D_COL_CONVEX_HULL:
        case D_COL_MESH:
        case D_COL_BVH_MESH:
        case D_COL_HACD:
        {
            colShape = createCollisionMeshShape(colType, msh);
        }
            break;
        default:
            break;
    }
    
    return colShape;
}

template <typename CurveItemPtr>
inline db_collision_shape_base* create2DCollisionShape(
    const CurveItemPtr* crv)
{
    db_collision_shape_base* colShape = nullptr;
    
    //create convex 2d
    
    if(!crv || !crv->get()) {
        return 0;
    }

    const DPointList *ptsPtr = (*crv)->getPositionPtrConst();
    
    size_t count = ptsPtr->size();
    if(count < 3) {
        return 0;
    }

    std::vector<vec3f> vertices(count);
    
    for(size_t i = 0; i < count; ++i) {
//        vertices[i] = vec3f((*ptsPtr)[i][0], (*ptsPtr)[i][1], (*ptsPtr)[i][2]);
        vertices[i] = vec3f((*ptsPtr)[i][0], (*ptsPtr)[i][1], 0.0);
    }
    
    
    colShape = new db_collision_convex_2d(vertices.data(), vertices.size());
    
    return colShape;
}
