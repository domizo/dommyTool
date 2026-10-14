//
//  icosahedronNode.cpp
//  dommyTool
//
//  Created by yuzo on 3/29/13.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "icosahedronNode.h"

#include "DIcosahedronTopology.h"
#include "common/utility/DComputeGuard.h"

#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFloatPointArray.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>

#include <cmath>
#include <cstddef>


MObject icosahedronNode::frequency;
MObject icosahedronNode::radius;
MObject icosahedronNode::outPutMesh;

const MTypeId icosahedronNode::typeId(0x00330);


void* icosahedronNode::creator()
{
    return ::dommy::maya::createObject("icosahedronNode", [&]() {
        return new icosahedronNode();
    });
}


MStatus icosahedronNode::compute(const MPlug& plug, MDataBlock& block) try
{
    if (plug != outPutMesh) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle frequencyValueHandle = block.inputValue(frequency, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int frequencyValue = frequencyValueHandle.asInt();
    const MDataHandle radiusValueHandle = block.inputValue(radius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float radiusValue = radiusValueHandle.asFloat();
    if (!std::isfinite(radiusValue) || radiusValue <= 0.0f) {
        MGlobal::displayError("icosahedronNode: radius must be finite and positive.");
        return MS::kFailure;
    }

    dommy::geometry::IcosahedronTopology topology;
    if (!dommy::geometry::buildHistoricalIcosahedron(frequencyValue, topology)) {
        MGlobal::displayError(
            "icosahedronNode: frequency is invalid or exceeds the geometry safety limit.");
        return MS::kFailure;
    }

    MFloatPointArray vertices;
    MIntArray polygonCounts;
    MIntArray polygonConnections;
    for (std::vector<dommy::geometry::IcosahedronPoint>::const_iterator it =
             topology.points.begin(); it != topology.points.end(); ++it) {
        vertices.append(MFloatPoint(
            it->x * radiusValue, it->y * radiusValue, it->z * radiusValue));
    }
    for (std::vector<dommy::geometry::IcosahedronTriangle>::const_iterator it =
             topology.triangles.begin(); it != topology.triangles.end(); ++it) {
        polygonCounts.append(3);
        polygonConnections.append(it->first);
        polygonConnections.append(it->second);
        polygonConnections.append(it->third);
    }

    MFnMeshData meshDataFn;
    const MObject outputMeshData = meshDataFn.create(&status);
    if (status != MS::kSuccess || outputMeshData.isNull()) {
        return status == MS::kSuccess ? MStatus(MS::kFailure) : status;
    }

    MFnMesh meshFn;
    const MObject mesh = meshFn.create(
        static_cast<int>(topology.points.size()),
        static_cast<int>(topology.triangles.size()),
        vertices,
        polygonCounts,
        polygonConnections,
        outputMeshData,
        &status);
    if (status != MS::kSuccess || mesh.isNull()) {
        return status == MS::kSuccess ? MStatus(MS::kFailure) : status;
    }

    MDataHandle output = block.outputValue(outPutMesh, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = output.set(outputMeshData);
    if (status != MS::kSuccess) {
        return status;
    }

    return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("icosahedronNode")


MStatus icosahedronNode::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;

    frequency = numericAttribute.create(
        "frequency", "fq", MFnNumericData::kInt, 1, &status);
    if (status != MS::kSuccess) return status;
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(10));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));

    radius = numericAttribute.create(
        "radius", "r", MFnNumericData::kFloat, 1.0f, &status);
    if (status != MS::kSuccess) return status;
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0001f));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));

    outPutMesh = genericAttribute.create("outPutMesh", "opm", &status);
    if (status != MS::kSuccess) return status;
    status = genericAttribute.addDataAccept(MFnData::kMesh);
    if (status != MS::kSuccess) return status;
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));

    status = addAttribute(frequency);
    if (status != MS::kSuccess) return status;
    status = addAttribute(radius);
    if (status != MS::kSuccess) return status;
    status = addAttribute(outPutMesh);
    if (status != MS::kSuccess) return status;
    status = attributeAffects(frequency, outPutMesh);
    if (status != MS::kSuccess) return status;
    return attributeAffects(radius, outPutMesh);
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "icosahedronNode", "initialize", std::current_exception());
}
