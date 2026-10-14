//
//  DCurvatureData.cpp
//  dommyTool
//
//  Created by yuzo on 6/3/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaMeshColorOutput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "DCurvatureData.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

const double kRadiansToDegrees = 57.2957795130823208768;

MStatus setOutputMesh(MDataBlock& data, const MPlug& plug, const MObject& meshObject) {
    MStatus status;
    MDataHandle outputHandle = data.outputValue(DCurvatureData::outMesh, &status);
    if (!status)
        return status;
    status = outputHandle.set(meshObject);
    if (!status)
        return status;
    return data.setClean(plug);
}

MStatus initializeDefaultColorRamp(const MObject& node) {
    MStatus status;
    MPlug rampPlug(node, DCurvatureData::aCurveMap);
    MRampAttribute ramp(rampPlug, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int entryCount = ramp.getNumEntries(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (entryCount != 0U)
        return MS::kSuccess;

    // Prepare all entries before changing the ramp on the Maya node.
    MFloatArray positions;
    DOMMY_RETURN_IF_FAILURE(positions.append(1.0f));
    DOMMY_RETURN_IF_FAILURE(positions.append(0.5f));
    DOMMY_RETURN_IF_FAILURE(positions.append(0.0f));

    MColorArray colors;
    DOMMY_RETURN_IF_FAILURE(colors.append(MColor(1.0f, 0.0f, 0.0f)));
    DOMMY_RETURN_IF_FAILURE(colors.append(MColor(0.0f, 1.0f, 0.0f)));
    DOMMY_RETURN_IF_FAILURE(colors.append(MColor(0.0f, 0.0f, 1.0f)));

    MIntArray interpolation;
    DOMMY_RETURN_IF_FAILURE(interpolation.append(MRampAttribute::kSpline));
    DOMMY_RETURN_IF_FAILURE(interpolation.append(MRampAttribute::kSpline));
    DOMMY_RETURN_IF_FAILURE(interpolation.append(MRampAttribute::kSpline));
    if (positions.length() != 3U || colors.length() != 3U || interpolation.length() != 3U)
        return MS::kFailure;
    return ramp.setRamp(colors, positions, interpolation);
}

} // namespace

const MTypeId DCurvatureData::typeId(0x4004A);

MObject DCurvatureData::inMesh;
MObject DCurvatureData::aCalcType;
MObject DCurvatureData::aCurveMap;
MObject DCurvatureData::smIterate;
MObject DCurvatureData::aIsAdjustRange;
MObject DCurvatureData::outMesh;

void* DCurvatureData::creator() {
    return ::dommy::maya::createObject("DCurvatureData", [&]() {
        return new DCurvatureData();
    });
}

void DCurvatureData::postConstructor() try {
    const MStatus status = initializeDefaultColorRamp(thisMObject());
    if (!status)
        status.pAPIerror("DCurvatureData::postConstructor", __LINE__);
} catch (...) {
    (void)dommy::maya::reportEntryPointException("DCurvatureData", "postConstructor",
                                                 std::current_exception());
}

MStatus DCurvatureData::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outMesh)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inMesh, &status);
    if (!status)
        return status;

    MObject inputMeshObject = inputHandle.asMesh();
    if (inputMeshObject.isNull())
        return MS::kInvalidParameter;

    MFnMesh inputMesh(inputMeshObject, &status);
    if (!status)
        return status;

    const int rawVertexCount = inputMesh.numVertices(&status);
    if (!status)
        return status;

    std::size_t vertexCount = 0;
    if (!dommy::generation::checkedCount(rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
                                         vertexCount)) {
        MGlobal::displayError("DCurvatureData: mesh vertex count exceeds the safety budget.");
        return MS::kFailure;
    }

    const MDataHandle calcTypeHandle = data.inputValue(aCalcType, &status);
    if (!status)
        return status;
    const short calculationType = calcTypeHandle.asShort();
    if (calculationType != 0) {
        MGlobal::displayError("DCurvatureData: unsupported curvature calculation type.");
        return MS::kInvalidParameter;
    }

    const MDataHandle smoothingHandle = data.inputValue(smIterate, &status);
    if (!status)
        return status;
    const int rawSmoothingIterations = smoothingHandle.asInt();
    if (rawSmoothingIterations < 0) {
        MGlobal::displayError("DCurvatureData: smoothing iterations cannot be negative.");
        return MS::kInvalidParameter;
    }

    const std::size_t smoothingIterations = static_cast<std::size_t>(rawSmoothingIterations);
    std::size_t smoothingWorkItems = 0;
    if (!dommy::generation::checkedProduct(vertexCount, smoothingIterations,
                                           dommy::generation::kMaxWorkItemCount,
                                           smoothingWorkItems)) {
        MGlobal::displayError("DCurvatureData: smoothing work exceeds the safety budget.");
        return MS::kFailure;
    }

    const MDataHandle adjustHandle = data.inputValue(aIsAdjustRange, &status);
    if (!status)
        return status;
    const bool adjustRange = adjustHandle.asBool();

    MFnMeshData meshData;
    const MObject outputMeshObject = meshData.create(&status);
    if (!status)
        return status;

    if (outputMeshObject.isNull()) return MS::kFailure;
    MFnMesh outputMesh;
    const MObject copiedMesh = outputMesh.copy(inputMeshObject, outputMeshObject, &status);
    if (!status) return status;
    if (copiedMesh.isNull()) return MS::kFailure;

    if (vertexCount == 0U) {
        return setOutputMesh(data, plug, outputMeshObject);
    }

    MPointArray points;
    status = inputMesh.getPoints(points, MSpace::kObject);
    if (!status)
        return status;
    if (points.length() != vertexCount)
        return MS::kFailure;

    for (unsigned int i = 0U; i < points.length(); ++i)
        if (!dommy::maya::isFinitePoint(points[i])) return MS::kInvalidParameter;

    MPlug colorRampPlug(thisMObject(), aCurveMap);
    MRampAttribute colorRamp(colorRampPlug, &status);
    if (!status)
        return status;

    MItMeshVertex vertexIterator(inputMeshObject, &status);
    if (!status)
        return status;
    MItMeshPolygon polygonIterator(inputMeshObject, &status);
    if (!status)
        return status;

    MDoubleArray curvatureValues;
    DOMMY_RETURN_IF_FAILURE(curvatureValues.setLength(static_cast<unsigned int>(vertexCount)));
    if (curvatureValues.length() != vertexCount)
        return MS::kFailure;
    // setLength does not initialize new elements. Isolated vertices retain zero.
    for (unsigned int index = 0U; index < curvatureValues.length(); ++index) {
        curvatureValues[index] = 0.0;
    }

    status = dommy::maya::visitMeshElements(vertexIterator, vertexCount, [&](MItMeshVertex&) -> MStatus {
        const int vertexIndex = vertexIterator.index(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= vertexCount) {
            return MS::kFailure;
        }

        MVector vertexNormal;
        status = vertexIterator.getNormal(vertexNormal, MSpace::kObject);
        if (!status)
            return status;

        MVector normalizedNormal;
        if (!dommy::maya::normalizedVector(vertexNormal, normalizedNormal)) {
            curvatureValues[static_cast<unsigned int>(vertexIndex)] = 0.0;
            return MS::kSuccess;
        }

        MIntArray faceIds;
        status = vertexIterator.getConnectedFaces(faceIds);
        if (!status)
            return status;

        double curvatureSum = 0.0;
        std::size_t validFaceCount = 0U;
        for (unsigned int face = 0U; face < faceIds.length(); ++face) {
            const int faceId = faceIds[face];
            if (faceId < 0)
                return MS::kFailure;

            int previousFaceId = 0;
            status = polygonIterator.setIndex(faceId, previousFaceId);
            if (!status)
                return status;

            const MPoint faceCenter = polygonIterator.center(MSpace::kObject, &status);
            if (!status || !dommy::maya::isFinitePoint(faceCenter)) {
                return status ? MStatus(MS::kFailure) : status;
            }

            MVector faceDirection;
            if (!dommy::maya::normalizedVector(
                    faceCenter - points[static_cast<unsigned int>(vertexIndex)], faceDirection)) {
                continue;
            }

            const double angle = normalizedNormal.angle(faceDirection);
            if (!std::isfinite(angle))
                continue;

            const double legacyAlignment =
                std::max(0.0, std::min(1.0, 1.0 - (angle * kRadiansToDegrees / 180.0)));
            curvatureSum += legacyAlignment;
            ++validFaceCount;
        }

        if (validFaceCount > 0U) {
            curvatureValues[static_cast<unsigned int>(vertexIndex)] =
                curvatureSum / static_cast<double>(validFaceCount);
        }
        return MS::kSuccess;
    });
    if (!status) return status;

    MDoubleArray finalCurvature;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(curvatureValues, finalCurvature));
    for (std::size_t iteration = 0U; iteration < smoothingIterations; ++iteration) {
        MDoubleArray previousCurvature;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(finalCurvature, previousCurvature));
        status = vertexIterator.reset();
        if (!status)
            return status;

        status = dommy::maya::visitMeshElements(vertexIterator, vertexCount, [&](MItMeshVertex&) -> MStatus {
            const int vertexIndex = vertexIterator.index(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (vertexIndex < 0 ||
                static_cast<std::size_t>(vertexIndex) >= vertexCount) {
                return MS::kFailure;
            }

            MIntArray connectedVertices;
            status = vertexIterator.getConnectedVertices(connectedVertices);
            if (!status)
                return status;
            if (connectedVertices.length() == 0U)
                return MS::kSuccess;

            double sum = 0.0;
            std::size_t validNeighbourCount = 0U;
            for (unsigned int neighbour = 0U; neighbour < connectedVertices.length(); ++neighbour) {
                const int neighbourIndex = connectedVertices[neighbour];
                if (neighbourIndex < 0 || static_cast<std::size_t>(neighbourIndex) >= vertexCount) {
                    return MS::kFailure;
                }
                const double value = previousCurvature[static_cast<unsigned int>(neighbourIndex)];
                if (!std::isfinite(value))
                    continue;
                sum += value;
                ++validNeighbourCount;
            }

            if (validNeighbourCount > 0U) {
                finalCurvature[static_cast<unsigned int>(vertexIndex)] =
                    sum / static_cast<double>(validNeighbourCount);
            }
            return MS::kSuccess;
        });
        if (!status) return status;
    }

    double minimumCurvature = std::numeric_limits<double>::infinity();
    double maximumCurvature = -std::numeric_limits<double>::infinity();
    for (unsigned int index = 0U; index < finalCurvature.length(); ++index) {
        const double value = finalCurvature[index];
        if (!std::isfinite(value))
            return MS::kFailure;
        minimumCurvature = std::min(minimumCurvature, value);
        maximumCurvature = std::max(maximumCurvature, value);
    }

    MColorArray outputColors;
    DOMMY_RETURN_IF_FAILURE(outputColors.setLength(static_cast<unsigned int>(vertexCount)));
    if (outputColors.length() != vertexCount)
        return MS::kFailure;
    for (unsigned int index = 0U; index < finalCurvature.length(); ++index) {
        float rampPosition = static_cast<float>(finalCurvature[index]);
        if (adjustRange) {
            const double span = maximumCurvature - minimumCurvature;
            if (std::isfinite(span) && span > std::numeric_limits<double>::epsilon()) {
                rampPosition =
                    static_cast<float>((finalCurvature[index] - minimumCurvature) / span);
            } else {
                rampPosition = 0.0f;
            }
        }
        if (!std::isfinite(rampPosition))
            return MS::kFailure;

        MColor color;
        colorRamp.getColorAtPosition(rampPosition, color, &status);
        if (!status || !dommy::maya::isFiniteColor(color)) {
            return status ? MStatus(MS::kFailure) : status;
        }
        outputColors[index] = color;
    }

    status = dommy::maya::setMeshDataVertexColors(
        outputMesh, outputColors, MString("curvature"));
    if (!status)
        return status;
    return setOutputMesh(data, plug, outputMeshObject);
}
DOMMY_COMPUTE_CATCH("DCurvatureData")

MStatus DCurvatureData::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnEnumAttribute enumAttribute;

    inMesh = typedAttribute.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));

    // Value zero is retained for compatibility with historical scene files.
    // The implementation is an alignment heuristic, not differential mean
    // curvature, so the UI names the actual legacy contract explicitly.
    aCalcType = enumAttribute.create("CalcType", "caty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Legacy Normal-Face Alignment", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCalcType));

    aCurveMap = MRampAttribute::createColorRamp("curvatureMap", "cm", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveMap));

    smIterate = numericAttribute.create("smIterate", "sit", MFnNumericData::kInt, 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(smIterate));

    aIsAdjustRange =
        numericAttribute.create("IsAdjustRange", "iar", MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAdjustRange));

    outMesh = typedAttribute.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCalcType, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurveMap, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(smIterate, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAdjustRange, outMesh));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurvatureData", "initialize", std::current_exception());
}
