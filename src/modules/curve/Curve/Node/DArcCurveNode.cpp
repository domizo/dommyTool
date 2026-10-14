//
//  DArcCurveNode.cpp
//  dommyTool
//
//  Created by yuzo on 2015/01/20.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DArcCurveNode.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DProceduralMeshPlans.h"

#include "dommyMath.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MVector.h>

#include <cstddef>

const MTypeId DArcCurveNode::typeId(0x0047A);

MObject DArcCurveNode::divide;
MObject DArcCurveNode::radius;
MObject DArcCurveNode::width;

MObject DArcCurveNode::startAngle;
MObject DArcCurveNode::endAngle;
MObject DArcCurveNode::offsetAngle;

MObject DArcCurveNode::outputMesh;

void* DArcCurveNode::creator() {
    return ::dommy::maya::createObject("DArcCurveNode", [&]() {
        return new DArcCurveNode();
    });
}

MStatus DArcCurveNode::compute(const MPlug& plug, MDataBlock& block) try {
    if (plug != outputMesh)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle divideHandle = block.inputValue(divide, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int divideValue = divideHandle.asInt();
    const MDataHandle radiusHandle = block.inputValue(radius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double radiusValue = radiusHandle.asFloat();
    const MDataHandle widthHandle = block.inputValue(width, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double widthValue = widthHandle.asFloat();
    const MDataHandle startAngleHandle = block.inputValue(startAngle, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double startAngleValue = startAngleHandle.asFloat();
    const MDataHandle endAngleHandle = block.inputValue(endAngle, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double endAngleValue = endAngleHandle.asFloat();
    const MDataHandle offsetAngleHandle = block.inputValue(offsetAngle, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double offsetAngleValue = offsetAngleHandle.asFloat();

    dommy::generation::ArcMeshPlan plan;
    if (!dommy::generation::buildArcMeshPlan(divideValue, radiusValue, widthValue, startAngleValue,
                                             endAngleValue, offsetAngleValue, plan)) {
        MGlobal::displayError(
            "DArcCurveNode: inputs are invalid or exceed the mesh generation budget.");
        return MS::kInvalidParameter;
    }

    MFloatPointArray vertices;
    MIntArray polygonCounts;
    MIntArray polygonConnections;
    MFloatArray uCoordinates;
    MFloatArray vCoordinates;
    MIntArray uvIds;
    MIntArray uvCounts;

    // Counts are already bounded by the generation plan. Allocate once and
    // stop on any failure before indexing or handing arrays to Maya's mesh API.
    const unsigned int vertexCount = static_cast<unsigned int>(plan.vertexCount);
    const unsigned int faceCount = static_cast<unsigned int>(plan.faceCount);
    const unsigned int connectionCount = static_cast<unsigned int>(plan.connectionCount);
    DOMMY_RETURN_IF_FAILURE(vertices.setLength(vertexCount));
    DOMMY_RETURN_IF_FAILURE(polygonCounts.setLength(faceCount));
    DOMMY_RETURN_IF_FAILURE(polygonConnections.setLength(connectionCount));
    DOMMY_RETURN_IF_FAILURE(uCoordinates.setLength(vertexCount));
    DOMMY_RETURN_IF_FAILURE(vCoordinates.setLength(vertexCount));
    DOMMY_RETURN_IF_FAILURE(uvIds.setLength(connectionCount));
    DOMMY_RETURN_IF_FAILURE(uvCounts.setLength(faceCount));

    for (std::size_t index = 0U; index < plan.sampleCount; ++index) {
        MVector innerVector(0.0, radiusValue, 0.0);
        MVector outerVector(0.0, plan.uvScale, 0.0);
        const double angle =
            plan.startDegrees + plan.stepDegrees * static_cast<double>(index) + offsetAngleValue;
        outerVector = outerVector.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-angle));
        innerVector = innerVector.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(-angle));

        const MVector samplePositions[] = {outerVector, innerVector};
        for (unsigned int side = 0U; side < 2U; ++side) {
            const MVector& position = samplePositions[side];
            MFloatPoint point;
            float u = 0.0f, v = 0.0f;
            if (!dommy::numeric::checkedFloatCast(position.x, point.x) ||
                !dommy::numeric::checkedFloatCast(position.y, point.y) ||
                !dommy::numeric::checkedFloatCast(position.z, point.z) ||
                !dommy::numeric::checkedFloatCast((position.x / plan.uvScale) * 0.5 + 0.5, u) ||
                !dommy::numeric::checkedFloatCast((position.y / plan.uvScale) * 0.5 + 0.5, v)) {
                MGlobal::displayError(
                    "DArcCurveNode: generated coordinates exceed the finite float range.");
                return MS::kInvalidParameter;
            }
            const unsigned int pointIndex = static_cast<unsigned int>(index * 2U) + side;
            vertices[pointIndex] = point;
            uCoordinates[pointIndex] = u;
            vCoordinates[pointIndex] = v;
        }
    }

    for (std::size_t face = 0U; face < plan.faceCount; ++face) {
        const int offsetId = static_cast<int>(face * 2U);
        const unsigned int faceIndex = static_cast<unsigned int>(face);
        polygonCounts[faceIndex] = 4;
        uvCounts[faceIndex] = 4;
        const int quad[] = {offsetId, offsetId + 1, offsetId + 3, offsetId + 2};
        for (unsigned int corner = 0U; corner < 4U; ++corner) {
            const unsigned int connectionIndex = faceIndex * 4U + corner;
            polygonConnections[connectionIndex] = quad[corner];
            uvIds[connectionIndex] = quad[corner];
        }
    }

    MFnMeshData meshDataFunction;
    const MObject newMeshData = meshDataFunction.create(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (newMeshData.isNull())
        return MS::kFailure;

    MFnMesh meshFunction;
    const MObject newMesh = meshFunction.create(
        static_cast<int>(plan.vertexCount), static_cast<int>(plan.faceCount), vertices,
        polygonCounts, polygonConnections, uCoordinates, vCoordinates, newMeshData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (newMesh.isNull())
        return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(meshFunction.assignUVs(uvCounts, uvIds));

    MDataHandle outputHandle = block.outputValue(outputMesh, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(outputHandle.set(newMeshData));
    return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DArcCurveNode")

MStatus DArcCurveNode::initialize() {
    MStatus status;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;

    divide = nAttr.create("divide", "dv", MFnNumericData::kInt, 10, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(dommy::generation::kMaxArcDivisionCount));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    radius = nAttr.create("radius", "r", MFnNumericData::kFloat, 5.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    width = nAttr.create("width", "w", MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    startAngle = nAttr.create("startAngle", "sa", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    endAngle = nAttr.create("endAngle", "ea", MFnNumericData::kFloat, 45.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    offsetAngle = nAttr.create("offsetAngle", "oa", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    outputMesh = gAttr.create("outputMesh", "opm", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(divide));
    DOMMY_RETURN_IF_FAILURE(addAttribute(radius));
    DOMMY_RETURN_IF_FAILURE(addAttribute(width));

    DOMMY_RETURN_IF_FAILURE(addAttribute(startAngle));
    DOMMY_RETURN_IF_FAILURE(addAttribute(endAngle));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetAngle));

    DOMMY_RETURN_IF_FAILURE(addAttribute(outputMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(divide, outputMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(radius, outputMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(width, outputMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(startAngle, outputMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(endAngle, outputMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetAngle, outputMesh));

    return MS::kSuccess;
}
