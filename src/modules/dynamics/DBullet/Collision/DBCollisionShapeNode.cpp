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
//  DBCollisionShapeNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBCollisionShapeNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DComputeGuard.h"
#include "common/utility/DMayaMeshTraversal.h"


#include <iterator>
#include <cmath>
#include <limits>
#include <vector>

#include "dbSolver_t.h"
#include "DCollisionMeshInput.h"

#include "dpoint_ids.h"

#include "DBulletDataHeaders.h"


namespace {

struct MeshCollisionData
{
    std::vector<vec3f> vertices;
    std::vector<vec3f> normals;
    std::vector<unsigned int> indices;
};

bool readMeshCollisionData(const MObject& node, MeshCollisionData& result)
{
    if (!node.hasFn(MFn::kMesh)) {
        return false;
    }

    MDagPath dagPath;
    MStatus status = MDagPath::getAPathTo(node, dagPath);
    if (status != MS::kSuccess) {
        return false;
    }

    MFnMesh mesh(dagPath, &status);
    if (status != MS::kSuccess) {
        return false;
    }

    const int rawVertexCount = mesh.numVertices(&status);
    std::size_t vertexCount = 0U;
    if (!status || !dommy::generation::checkedCount(
            rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
            vertexCount) || vertexCount < 3U) return false;
    std::size_t faceCount = 0U, triangleCount = 0U;
    if (!dommy::maya::preflightMeshTriangles(
            mesh, node, faceCount, triangleCount) || triangleCount == 0U) return false;

    MFloatPointArray mayaPoints;
    MFloatVectorArray mayaNormals;
    MIntArray triangleCounts;
    MIntArray triangleVertices;
    if (mesh.getPoints(mayaPoints, MSpace::kWorld) != MS::kSuccess ||
        mayaPoints.length() != vertexCount ||
        mesh.getTriangles(triangleCounts, triangleVertices) != MS::kSuccess ||
        !dommy::maya::validMeshTriangleArrays(
            mayaPoints, triangleCounts, triangleVertices,
            vertexCount, faceCount, triangleCount)) {
        return false;
    }

    // The display buffers store one normal per vertex, not Maya normal-pool IDs.
    // Keep normals optional for collision creation, but never consume a failed
    // or incomplete bulk read. Hard-edge normals are averaged per vertex.
    status = mesh.getVertexNormals(false, mayaNormals, MSpace::kWorld);
    const bool hasVertexNormals = status && mayaNormals.length() == vertexCount;

    MeshCollisionData candidate;
    candidate.vertices.resize(vertexCount);
    candidate.normals.assign(vertexCount, vec3f(0.0f, 0.0f, 0.0f));
    candidate.indices.resize(triangleVertices.length());

    for (unsigned int i = 0; i < mayaPoints.length(); ++i) {
        if (!std::isfinite(mayaPoints[i].x) ||
            !std::isfinite(mayaPoints[i].y) ||
            !std::isfinite(mayaPoints[i].z)) {
            return false;
        }
        candidate.vertices[i] = vec3f(mayaPoints[i].x, mayaPoints[i].y, mayaPoints[i].z);
        if (hasVertexNormals &&
            std::isfinite(mayaNormals[i].x) &&
            std::isfinite(mayaNormals[i].y) &&
            std::isfinite(mayaNormals[i].z)) {
            candidate.normals[i] = vec3f(mayaNormals[i].x, mayaNormals[i].y, mayaNormals[i].z);
        }
    }

    for (unsigned int i = 0; i < triangleVertices.length(); ++i) {
        candidate.indices[i] = static_cast<unsigned int>(triangleVertices[i]);
    }

    if (!dommy::bullet::hasValidCollisionMeshInput(
            candidate.vertices.data(), candidate.vertices.size(), candidate.normals.data(),
            candidate.indices.data(), candidate.indices.size())) return false;
    result.vertices.swap(candidate.vertices);
    result.normals.swap(candidate.normals);
    result.indices.swap(candidate.indices);
    return true;
}

} // namespace


MTypeId DBCollisionShapeNode::typeId(ID_DB_COLLISION_SHAPE);
const MString DBCollisionShapeNode::typeName("DBCollisionShapeNode");

MObject DBCollisionShapeNode::ia_shape;
MObject DBCollisionShapeNode::ia_type;
MObject DBCollisionShapeNode::ia_scale;
MObject DBCollisionShapeNode::ca_collisionShape;
MObject DBCollisionShapeNode::ca_collisionShapeParam;
MObject DBCollisionShapeNode::oa_collisionShape;



MStatus DBCollisionShapeNode::initialize()
try {
    MStatus stat;
    MFnMessageAttribute fnMsgAttr;
    MFnNumericAttribute fnNumericAttr;
    MFnEnumAttribute fnEnumAttr;
    
    ia_type = fnEnumAttr.create("type", "tp", D_COL_BVH_MESH, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("Convex Hull", D_COL_CONVEX_HULL));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("Mesh", D_COL_MESH));
//    fnEnumAttr.addField("Cylinder", D_COL_CYLINDER);
//    fnEnumAttr.addField("Capsule", D_COL_CAPSULE);
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("Box", D_COL_BOX));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("Sphere", D_COL_SPHERE));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("Plane", D_COL_PLANE));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("BvhMesh", D_COL_BVH_MESH));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.addField("HACD", D_COL_HACD));
    DOMMY_RETURN_IF_FAILURE(fnEnumAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_type));
    
    
    ia_scale = fnNumericAttr.createPoint("scale", "sc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_scale));
    
    
    oa_collisionShape = fnMsgAttr.create("outCollisionShape", "oucs", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(oa_collisionShape));
    
    
    ia_shape = fnMsgAttr.create("inShape", "insh", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_shape));
    
    
    ca_collisionShape = fnNumericAttr.create("ca_collisionShape", "ccs", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setWorldSpace(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_collisionShape));
    
    
    ca_collisionShapeParam = fnNumericAttr.create("collisionShapeParam", "cspm", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_collisionShapeParam));
    
    
    //
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_shape, oa_collisionShape));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_type, oa_collisionShape));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_scale, oa_collisionShape));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_shape, ca_collisionShape));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_type, ca_collisionShape));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_shape, ca_collisionShapeParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_scale, ca_collisionShapeParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_type, ca_collisionShapeParam));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBCollisionShapeNode", "initialize", std::current_exception());
}


void* DBCollisionShapeNode::creator()
{
    return ::dommy::maya::createObject("DBCollisionShapeNode", [&]() {
        return new DBCollisionShapeNode();
    });
}


DBCollisionShapeNode::DBCollisionShapeNode() = default;

DBCollisionShapeNode::~DBCollisionShapeNode()
{
}


bool DBCollisionShapeNode::setInternalValueInContext ( const  MPlug & plug,
                                                    const  MDataHandle & dataHandle,
                                                    MDGContext & ctx)
{
    (void)plug;
    (void)dataHandle;
    (void)ctx;
    return false; //setInternalValueInContext(plug,dataHandle,ctx);
}

MStatus DBCollisionShapeNode::compute(const MPlug& plug, MDataBlock& data) try {
    if(plug == oa_collisionShape) {
        return computeOutputShape(plug, data);
    } else if(plug == ca_collisionShape) {
        return computeCollisionShape(plug, data);
    } else  if(plug == ca_collisionShapeParam) {
        return computeCollisionShapeParam(plug, data);
    }
    return MStatus::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBCollisionShapeNode")

MStatus DBCollisionShapeNode::evaluate_collision_shape(
    dbCollisionShape_t::pointer& collisionShape)
{
    collisionShape.reset();
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, oa_collisionShape).getValue(updateToken);
    if (!status) {
        return status;
    }
    status =
        MPlug(thisObject, ca_collisionShapeParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    if (!m_collision_shape) {
        return MS::kFailure;
    }
    collisionShape = m_collision_shape;
    return MS::kSuccess;
}

static MStatus isCompound(const MPlug& plgInShape, bool& compound)
{
    compound = false;
    MPlugArray plgaConnectedTo;
    MStatus status;
    const bool connected = plgInShape.connectedTo(
        plgaConnectedTo, true, true, &status);
    if (!status) {
        return status;
    }
    const unsigned int numSelectedShapes = plgaConnectedTo.length();
    
    if(connected && numSelectedShapes > 0U) {
        
        MObject node = plgaConnectedTo[0].node();
        
        MDagPath dagPath;
        status = MDagPath::getAPathTo(node, dagPath);
        if (!status) {
            return status;
        }
        const unsigned int numChildren = dagPath.childCount();
        
        if (node.hasFn(MFn::kTransform))
        {
            for (unsigned int i = 0U; i < numChildren; ++i)
            {
                MObject child = dagPath.child(i);
                if(child.hasFn(MFn::kMesh))
                {
                    return MS::kSuccess;
                }
                
                if(child.hasFn(MFn::kTransform))
                {
                    MDagPath childPath;
                    status = MDagPath::getAPathTo(child, childPath);
                    if (!status) {
                        return status;
                    }
                    
                    const unsigned int numGrandChildren =
                        childPath.childCount();
                    for (unsigned int grandchildIndex = 0U;
                         grandchildIndex < numGrandChildren;
                         ++grandchildIndex)
                    {
                        MObject grandchild =
                            childPath.child(grandchildIndex);
                        if(grandchild.hasFn(MFn::kMesh))
                        {
                            compound = true;
                            return MS::kSuccess;
                        }
                    }
                }
            }
        }
    }
    return MS::kSuccess;
}

dbCollisionShape_t::pointer DBCollisionShapeNode::createCompositeShape(const MPlug& plgInShape)
{
    std::vector<dbCollisionShape_t::pointer> childCollisionShapes;
    std::vector<vec3f> childPositions;
    std::vector<quatf> childOrientations;
    
    MPlugArray plgaConnectedTo;
    MStatus connectionStatus;
    const bool connected = plgInShape.connectedTo(
        plgaConnectedTo, true, true, &connectionStatus);
    if (!connectionStatus || !connected ||
        plgaConnectedTo.length() == 0U) {
        return dbCollisionShape_t::pointer();
    }

    const MObject node = plgaConnectedTo[0].node();
    if (!node.hasFn(MFn::kTransform)) {
        return dbCollisionShape_t::pointer();
    }

    MDagPath rootPath;
    if (MDagPath::getAPathTo(node, rootPath) != MS::kSuccess) {
        return dbCollisionShape_t::pointer();
    }

    for (unsigned int childIndex = 0; childIndex < rootPath.childCount(); ++childIndex) {
        const MObject child = rootPath.child(childIndex);
        if (child.hasFn(MFn::kMesh)) {
            return dbCollisionShape_t::pointer();
        }
        if (!child.hasFn(MFn::kTransform)) {
            continue;
        }

        MDagPath childPath;
        if (MDagPath::getAPathTo(child, childPath) != MS::kSuccess) {
            continue;
        }

        MStatus status;
        MFnTransform childTransform(childPath, &status);
        if (status != MS::kSuccess) {
            continue;
        }

        const MVector translation = childTransform.getTranslation(MSpace::kTransform, &status);
        if (status != MS::kSuccess) {
            continue;
        }

        MQuaternion rotation;
        status = childTransform.getRotation(rotation, MSpace::kTransform);
        if (status != MS::kSuccess) {
            continue;
        }

        const vec3f childPosition(
            static_cast<float>(translation.x),
            static_cast<float>(translation.y),
            static_cast<float>(translation.z));
        const quatf childOrientation(
            static_cast<float>(rotation.w),
            static_cast<float>(rotation.x),
            static_cast<float>(rotation.y),
            static_cast<float>(rotation.z));

        for (unsigned int grandchildIndex = 0;
             grandchildIndex < childPath.childCount();
             ++grandchildIndex) {
            const MObject grandchild = childPath.child(grandchildIndex);
            if (!grandchild.hasFn(MFn::kMesh)) {
                continue;
            }

            dbCollisionShape_t::pointer childShape = createCollisionShape(grandchild);
            if (childShape) {
                childCollisionShapes.push_back(childShape);
                childPositions.push_back(childPosition);
                childOrientations.push_back(childOrientation);
            }
        }
    }

    if (childCollisionShapes.empty() ||
        childCollisionShapes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return dbCollisionShape_t::pointer();
    }

    return dbSolver_t::create_composite_shape(
        childCollisionShapes.data(),
        childPositions.data(),
        childOrientations.data(),
        static_cast<int>(childCollisionShapes.size()));
}



dbCollisionShape_t::pointer DBCollisionShapeNode::createCollisionShape(const MObject& node)
{
    MObject thisObject(thisMObject());
    MPlug plgType(thisObject, ia_type);
    DBulletCollisionType type;
    if (plgType.getValue(type) != MS::kSuccess) {
        return dbCollisionShape_t::pointer();
    }

    MeshCollisionData meshData;
    if (!readMeshCollisionData(node, meshData)) {
        return dbCollisionShape_t::pointer();
    }

    switch(type) {
        case D_COL_CONVEX_HULL:
            return dbSolver_t::create_convex_hull_shape(
                meshData.vertices.data(), meshData.vertices.size(),
                meshData.normals.data(), meshData.indices.data(), meshData.indices.size());
        case D_COL_MESH:
            return dbSolver_t::create_mesh_shape(
                meshData.vertices.data(), meshData.vertices.size(),
                meshData.normals.data(), meshData.indices.data(), meshData.indices.size(), true);
        case D_COL_CYLINDER:
            //cylinder
            break;
        case D_COL_CAPSULE:
            //capsule
            break;
            
        case D_COL_BVH_MESH:
            return dbSolver_t::create_mesh_shape(
                meshData.vertices.data(), meshData.vertices.size(),
                meshData.normals.data(), meshData.indices.data(), meshData.indices.size(), false);
        case D_COL_HACD:
            return dbSolver_t::create_hacd_shape(
                meshData.vertices.data(), meshData.vertices.size(),
                meshData.normals.data(), meshData.indices.data(), meshData.indices.size(), false);
        default:
            return dbCollisionShape_t::pointer();
    }

    return dbCollisionShape_t::pointer();
}

MStatus DBCollisionShapeNode::computeCollisionShape(const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject(thisMObject());
    const MPlug plgInShape(thisObject, ia_shape);
    dbCollisionShape_t::pointer replacement;

    bool compound = false;
    MStatus status = isCompound(plgInShape, compound);
    if (!status) {
        return status;
    }
    if (compound)
    {
        replacement = createCompositeShape(plgInShape);
    } else
    {
        const MDataHandle typeHandle =
            data.inputValue(ia_type, &status);
        if (!status) {
            return status;
        }
        const DBulletCollisionType type =
            static_cast<DBulletCollisionType>(typeHandle.asShort());
        
        switch(type)
        {
            case D_COL_BOX:
                replacement = dbSolver_t::create_box_shape();
                break;
            case D_COL_SPHERE:
                replacement = dbSolver_t::create_sphere_shape();
                break;
            case D_COL_PLANE:
                replacement = dbSolver_t::create_plane_shape();
                break;
                
            default:
            {
                MPlugArray plgaConnectedTo;
                const bool connected = plgInShape.connectedTo(
                    plgaConnectedTo, true, true, &status);
                if (!status) {
                    return status;
                }
                if (!connected || plgaConnectedTo.length() != 1U) {
                    return MS::kInvalidParameter;
                }
                replacement =
                    createCollisionShape(plgaConnectedTo[0].node());
            }
        }
    }

    if (!replacement) {
        return MS::kFailure;
    }

    MDataHandle output =
        data.outputValue(ca_collisionShape, &status);
    if (!status) {
        return status;
    }
    m_collision_shape = replacement;
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}


MStatus DBCollisionShapeNode::computeCollisionShapeParam(const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_collisionShape).getValue(updateToken);
    if (!status) {
        return status;
    }
    const MDataHandle scaleHandle =
        data.inputValue(ia_scale, &status);
    if (!status) {
        return status;
    }
    const float3& scale = scaleHandle.asFloat3();

    if (!m_collision_shape ||
        !std::isfinite(scale[0]) ||
        !std::isfinite(scale[1]) ||
        !std::isfinite(scale[2])) {
        return m_collision_shape ? MStatus(MS::kInvalidParameter)
                                 : MStatus(MS::kFailure);
    }
    MDataHandle output =
        data.outputValue(ca_collisionShapeParam, &status);
    if (!status) {
        return status;
    }
    auto solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;
    if (!m_collision_shape->set_scale(vec3f(scale[0], scale[1], scale[2]))) {
        return MS::kInvalidParameter;
    }
    output.setBool(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBCollisionShapeNode::computeOutputShape(const MPlug& plug, MDataBlock& data)
{
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_collisionShape).getValue(updateToken);
    if (!status) {
        return status;
    }
    status =
        MPlug(thisObject, ca_collisionShapeParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    if (!m_collision_shape) {
        return MS::kFailure;
    }

    return data.setClean(plug);
}
