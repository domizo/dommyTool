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

This file was added to the original plugin by Francisco Gochez <fjgochez@gmail.com>
It is based in large part on rigidBodyNode.cpp

*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/softBodyNode.cpp

//
//  DBSoftBodyNode.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#include "DBSoftBodyNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DLegacyDrawGuard.h"
#include "common/utility/DMayaUserNode.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DNumericValidation.h"
#include "DCollisionMeshInput.h"
#include "dbSolver_t.h"




#include "BulletSoftBody/btSoftBody.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"
#include "DComputeGuard.h"
#include <cmath>
#include <vector>

#include "dpoint_ids.h"


MTypeId DBSoftBodyNode::typeId(ID_DB_CONSTRAINT_SOFTBODY) ;
const MString DBSoftBodyNode::typeName("DBSoftBodyNode");

// declare object attributes

MObject DBSoftBodyNode::ia_solver;
MObject DBSoftBodyNode::ca_solver;
MObject DBSoftBodyNode::ca_softBody;
MObject DBSoftBodyNode::inputMesh;
MObject DBSoftBodyNode::outputMesh;
MObject DBSoftBodyNode::ia_mass;

MObject DBSoftBodyNode::ia_initialPosition;
MObject DBSoftBodyNode::ia_initialRotation;
MObject DBSoftBodyNode::ia_initialVelocity;
MObject DBSoftBodyNode::ia_initialSpin;
MObject DBSoftBodyNode::ca_softBodyParam;
MObject DBSoftBodyNode::ia_dynamicFrictionCoeff;
MObject DBSoftBodyNode::ia_collisionMargin;
MObject DBSoftBodyNode::ia_numClusters;

// default mass
const double DEFAULT_MASS = 100.0f;
// default dynamic friction coefficient
const double DEFAULT_DFC = 0.2f;
// default collision margin.  1.75 is an adjustment for Maya-Bullet discrepancy
const float DEFAULT_CMARGIN = 1.75 * 0.04;
// default number of collision clusters
const int DEFAULT_CLUSTERS = 100;
const int MAX_CLUSTERS = 8192;

DBSoftBodyNode::DBSoftBodyNode(void):
m_soft_body(0)
{
}

DBSoftBodyNode::~DBSoftBodyNode(void)
{
    destroySoftBody();
}

void* DBSoftBodyNode::creator()
{
    return ::dommy::maya::createObject("DBSoftBodyNode", [&]() {
        return new DBSoftBodyNode();
    });
}


void DBSoftBodyNode::draw(
    M3dView& view, const MDagPath&, M3dView::DisplayStyle,
    M3dView::DisplayStatus) try
{
    dbSolver_t::LockedSolverAccess solverAccess = dbSolver_t::lock_solver();
    if (!solverAccess) return;
    
    dommy::maya::ScopedLegacyGL drawing(view, GL_ALL_ATTRIB_BITS);
    // size of the soft-body "icon"
    const float lineOffset = 0.25f;
    if(m_soft_body) {
        // draw a simple cross
        glBegin(GL_LINES);
        glVertex3f(-lineOffset, 0.0, 0.0);
        glVertex3f(lineOffset, 0.0, 0.0);
        
        glVertex3f(0.0, -lineOffset, 0.0);
        glVertex3f(0.0, lineOffset, 0.0);
        
        glVertex3f(0.0, 0.0, -lineOffset);
        glVertex3f(0.0, 0.0, lineOffset);
        glEnd();
    } // end if m_soft_body
}
DOMMY_DRAW_CATCH("DBSoftBodyNode")



MStatus DBSoftBodyNode::initialize()
try {
    MFnMessageAttribute fnMsgAttr;
    MStatus stat = MStatus::kSuccess;
    
    ia_solver = fnMsgAttr.create("solver", "solv", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_solver));
    
    
    MFnNumericAttribute fnNumericAttr;
    ca_solver = fnNumericAttr.create("ca_solver", "caso", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_solver));
    
    
    ca_softBody = fnNumericAttr.create("ca_softBody", "casb", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_softBody));
    
    
    inputMesh = fnMsgAttr.create("inMesh", "inmsh", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputMesh));
    
    
    outputMesh = fnMsgAttr.create("outMesh", "outmsh", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputMesh));
    
    /*
     Note that these initial* attributes are not in use for soft bodies at the moment
     */
    ia_initialPosition = fnNumericAttr.createPoint("initialPosition", "inpo", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialPosition));
    
    
    ia_initialRotation = fnNumericAttr.createPoint("initialRotation", "inro", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialRotation));
    
    
    ia_initialVelocity = fnNumericAttr.createPoint("initialVelocity", "inve", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialVelocity));
    
    
    ia_initialSpin = fnNumericAttr.createPoint("initialSpin", "insp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_initialSpin));
    
    
    // total soft body mass
    ia_mass = fnNumericAttr.create("mass", "ma", MFnNumericData::kDouble, DEFAULT_MASS, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_mass));
    
    
    // create dynamic friction coefficient attribute
    ia_dynamicFrictionCoeff = fnNumericAttr.create("dynamicfrictioncoeff", "dfc", MFnNumericData::kDouble, DEFAULT_DFC, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_dynamicFrictionCoeff));
    
    
    // create collision margin attribute
    ia_collisionMargin = fnNumericAttr.create("collisionmargin", "cmargin", MFnNumericData::kFloat, DEFAULT_CMARGIN, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_collisionMargin));
    
    
    // collision clusters attribute
    ia_numClusters = fnNumericAttr.create("numclusters", "nclust", MFnNumericData::kInt, DEFAULT_CLUSTERS, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setMax(MAX_CLUSTERS));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ia_numClusters));
    
    
    ca_softBodyParam = fnNumericAttr.create("ca_softBodyParam", "casbp", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(fnNumericAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ca_softBodyParam));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputMesh, ca_softBody));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputMesh, ca_softBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_solver, ca_solver));
    // connect soft body parameters to ca_softBodyParam connection
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_mass, ca_softBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_dynamicFrictionCoeff, ca_softBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_collisionMargin, ca_softBodyParam));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(ia_numClusters, ca_softBodyParam));
    
    return stat;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSoftBodyNode", "initialize", std::current_exception());
}



bool DBSoftBodyNode::setInternalValueInContext(
    const MPlug&, const MDataHandle&, MDGContext&)
{
    return false;
}

MStatus DBSoftBodyNode::compute(const MPlug &plug, MDataBlock& data) try {
    if(plug == ca_softBody)
    {
        return computeSoftBody(plug, data);
    }
    else if(plug == ca_softBodyParam)
    {
        return computeSoftBodyParam(plug, data);
    }
    else if(plug == ca_solver)
    {
        MStatus status;
        data.inputValue(ia_solver, &status);
        if (!status) return status;
        MDataHandle output =
            data.outputValue(ca_solver, &status);
        if (!status) return status;
        output.set(true);
        output.setClean();
        return data.setClean(plug);
    }
    return MStatus::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DBSoftBodyNode")

void DBSoftBodyNode::destroySoftBody()
{
    if (m_soft_body) {
        dbSolver_t::remove_soft_body(m_soft_body);
    }
    m_soft_body = 0;
}

MStatus DBSoftBodyNode::computeSoftBody(const MPlug &plug, MDataBlock &data)
{
    MObject thisObject(thisMObject());
    MPlug plgInputMesh(thisObject, inputMesh);

    MObject update;
    //force evaluation of the shape
    MStatus status = plgInputMesh.getValue(update);
    if (!status) return status;
    const bool inputConnected = plgInputMesh.isConnected(&status);
    if (!status) return status;
    if (!inputConnected) return MS::kFailure;

    MPlugArray connections;
    plgInputMesh.connectedTo(connections, true, false, &status);
    if (!status) return status;
    if (connections.length() == 0 ||
        !connections[0].node().hasFn(MFn::kMesh)) {
        return MS::kFailure;
    }

    MFnMesh meshFn(connections[0].node(), &status);
    if (status != MS::kSuccess) {
        return status;
    }

    std::vector<int> triVertIndices;
    std::vector<float> triVertCoords;
    status = createHelperMesh(meshFn, triVertIndices, triVertCoords, MSpace::kTransform);
    if (status != MS::kSuccess) {
        return status;
    }

    dommy::bullet::SoftBodyParameters parameters;
    status = readSoftBodyParameters(parameters, &data);
    if (!status) return status;
    dbSoftBody_t::pointer newSoftBody =
        dbSolver_t::create_soft_body(triVertCoords, triVertIndices, parameters);
    if (!newSoftBody) {
        return MS::kFailure;
    }

    MDataHandle output =
        data.outputValue(ca_softBody, &status);
    if (!status) return status;

    if (!dbSolver_t::add_soft_body(
            newSoftBody, name().asChar())) {
        return MS::kFailure;
    }
    destroySoftBody();
    m_soft_body = newSoftBody;

    output.set(true);
    output.setClean();
    return data.setClean(plug);
}

MStatus DBSoftBodyNode::evaluate_soft_body(
    dbSoftBody_t::pointer& softBody)
{
    softBody.reset();
    const MObject thisObject(thisMObject());
    MObject updateToken;
    MStatus status =
        MPlug(thisObject, ca_softBody).getValue(updateToken);
    if (!status) {
        return status;
    }
    status = MPlug(thisObject, ca_softBodyParam).getValue(updateToken);
    if (!status) {
        return status;
    }
    if (!m_soft_body) {
        return MS::kFailure;
    }
    softBody = m_soft_body;
    return MS::kSuccess;
}

MStatus DBSoftBodyNode::createHelperMesh(MFnMesh& mayaMesh,
                                        std::vector<int>& triIndices,
                                        std::vector<float>& triVertices,
                                        MSpace::Space space)
{
    triIndices.clear();
    triVertices.clear();

    MStatus status;
    const int rawPointCount = mayaMesh.numVertices(&status);
    if (!status) return status;
    std::size_t pointCount = 0U, coordinateCount = 0U;
    if (!dommy::generation::checkedCount(
            rawPointCount, dommy::bullet::kMaxSoftBodyVertices, pointCount) ||
        pointCount < 3U ||
        !dommy::generation::checkedProduct(
            pointCount,
            3U,
            dommy::generation::kMaxWorkItemCount,
            coordinateCount)) {
        return MS::kInvalidParameter;
    }
    const MObject meshObject = mayaMesh.object(&status);
    if (!status) return status;
    std::size_t faceCount = 0U, triangleCount = 0U;
    status = dommy::maya::preflightMeshTriangles(
        mayaMesh, meshObject, faceCount, triangleCount);
    if (!status) return status;
    if (triangleCount == 0U || triangleCount>dommy::bullet::kMaxSoftBodyFaces) return MS::kInvalidParameter;

    MFloatPointArray ptArray;
    status = mayaMesh.getPoints(ptArray, space);
    if (!status) return status;
    if (ptArray.length() != pointCount) return MS::kFailure;
    MIntArray tCounts;
    MIntArray tVerts;
    status = mayaMesh.getTriangles(tCounts, tVerts);
    if (!status) return status;
    if (!dommy::maya::validMeshTriangleArrays(
            ptArray, tCounts, tVerts, pointCount, faceCount, triangleCount)) {
        return MS::kFailure;
    }

    // Publish both flattened buffers together only after validating the mesh.
    std::vector<float> coordinates;
    coordinates.reserve(coordinateCount);
    for (unsigned int i = 0U; i < ptArray.length(); ++i) {
        MFloatPoint point = ptArray[i];
        point.cartesianize();
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            return MS::kFailure;
        }
        coordinates.push_back(point.x);
        coordinates.push_back(point.y);
        coordinates.push_back(point.z);
    }
    std::vector<int> indices(tVerts.length());
    for (unsigned int i = 0U; i < tVerts.length(); ++i) indices[i] = tVerts[i];
    if (!dommy::bullet::hasValidSoftBodyMeshInput(coordinates, indices))
        return MS::kInvalidParameter;
    triVertices.swap(coordinates);
    triIndices.swap(indices);
    return MS::kSuccess;
}

MStatus DBSoftBodyNode::update()
{
    MObject thisObject(this->thisMObject());
    MObject update;
    MStatus status = MPlug(thisObject, inputMesh).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_softBody).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_softBodyParam).getValue(update);
    if (!status) return status;
    status = MPlug(thisObject, ca_solver).getValue(update);
    return status;
}


void DBSoftBodyNode::nodeRemoved(MObject& node, void*)
{
    MFnDependencyNode fnNode(node);
    DBSoftBodyNode* softBodyNode =
        dommy::maya::checkedUserNode<DBSoftBodyNode>(
            fnNode, DBSoftBodyNode::typeId);
    if (softBodyNode) softBodyNode->destroySoftBody();
}

MStatus DBSoftBodyNode::readSoftBodyParameters(
    dommy::bullet::SoftBodyParameters& parameters, MDataBlock* data) const
{
    double mass = 0, friction = 0, margin = 0;
    int clusters = 0;
    MStatus status;
    if (data) {
        const MDataHandle massHandle = data->inputValue(ia_mass, &status);
        if (!status) return status;
        const MDataHandle frictionHandle = data->inputValue(ia_dynamicFrictionCoeff, &status);
        if (!status) return status;
        const MDataHandle marginHandle = data->inputValue(ia_collisionMargin, &status);
        if (!status) return status;
        const MDataHandle clustersHandle = data->inputValue(ia_numClusters, &status);
        if (!status) return status;
        mass = massHandle.asDouble();
        friction = frictionHandle.asDouble();
        margin = marginHandle.asFloat();
        clusters = clustersHandle.asInt();
    } else {
        // StartTime reconstruction must not depend on the parameter token being dirty.
        const MObject node(thisMObject());
        status = MPlug(node, ia_mass).getValue(mass);
        if (!status) return status;
        status = MPlug(node, ia_dynamicFrictionCoeff).getValue(friction);
        if (!status) return status;
        status = MPlug(node, ia_collisionMargin).getValue(margin);
        if (!status) return status;
        status = MPlug(node, ia_numClusters).getValue(clusters);
        if (!status) return status;
    }
    return dommy::bullet::prepareSoftBodyParameters(mass, friction, margin, clusters, parameters)
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

MStatus DBSoftBodyNode::computeSoftBodyParam(const MPlug &plug, MDataBlock &data)
{
    MObject thisObject(thisMObject());
    MObject update;

    // update mass
    MStatus status;
    status = MPlug(thisObject, ca_softBody).getValue(update);
    if (!status) return status;
    if (!m_soft_body) return MS::kFailure;

    dommy::bullet::SoftBodyParameters parameters;
    status = readSoftBodyParameters(parameters, &data);
    if (!status) return status;

    auto solverAccess=dbSolver_t::lock_solver();
    if (!solverAccess) return MS::kFailure;

    MDataHandle output =
        data.outputValue(ca_softBodyParam, &status);
    if (!status) return status;

    if (!m_soft_body->set_parameters(parameters)) return MS::kInvalidParameter;

    output.set(true);
    output.setClean();
    return data.setClean(plug);
}

// The native body owns its parameter state; rebinding needs no node-side cache.
void DBSoftBodyNode::set_soft_body(dbSoftBody_t::pointer sb)
{
    this->m_soft_body = sb;
}
