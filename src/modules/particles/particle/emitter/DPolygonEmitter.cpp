#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolygonEmitter.h"
#include "DMeshEmitterSampling.h"

#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnData.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

typedef std::array<int, 4> QuadVertexIndices;

bool isValidVertexIndex(int index, unsigned int vertexCount)
{
    return index >= 0 && static_cast<unsigned int>(index) < vertexCount;
}

MStatus appendPoint(const MPoint& point,
                    MVectorArray& positions,
                    unsigned int& outputIndex)
{
    if (outputIndex >= positions.length()) {
        MGlobal::displayError(
            "DPolygonEmitter exceeded its precomputed sample count.");
        return MS::kFailure;
    }
    if (!dommy::maya::isFinitePoint(point)) {
        MGlobal::displayError(
            "DPolygonEmitter produced a non-finite sample point.");
        return MS::kFailure;
    }

    positions[outputIndex++] = MVector(point.x, point.y, point.z);
    return MS::kSuccess;
}

} // namespace

MTypeId DPolygonEmitter::id(0x0189A);

MObject DPolygonEmitter::inMeshObject;
MObject DPolygonEmitter::isVertexEmit;
MObject DPolygonEmitter::isEdgeEmit;
MObject DPolygonEmitter::edgeSampling;
MObject DPolygonEmitter::isFaceEmit;
MObject DPolygonEmitter::faceSampling;

void* DPolygonEmitter::creator()
{
    return ::dommy::maya::createObject("DPolygonEmitter", [&]() {
        return new DPolygonEmitter();
    });
}

MStatus DPolygonEmitter::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError(
            "DPolygonEmitter received an invalid output index.");
        return MS::kFailure;
    }
    const unsigned int logicalIndex =
        static_cast<unsigned int>(rawLogicalIndex);

    MArrayDataHandle outputArray = data.outputArrayValue(mOutput, &status);
    if (!status) return status;
    MArrayDataBuilder outputBuilder = outputArray.builder(&status);
    if (!status) return status;
    MDataHandle outputHandle = outputBuilder.addElement(
        logicalIndex, &status);
    if (!status) return status;

    MFnArrayAttrsData outputData;
    const MObject outputObject = outputData.create(&status);
    if (!status) return status;
    const auto finishOutput = [&]() -> MStatus {
        MStatus finishStatus = outputHandle.set(outputObject);
        if (!finishStatus) return finishStatus;
        return data.setClean(plug);
    };

    bool targetIsFull = false;
    status = dommy::particle::readArrayBoolInput(
        data, mIsFull, logicalIndex, targetIsFull);
    if (!status) return status;
    if (targetIsFull) return finishOutput();

    MDataHandle currentTimeHandle = data.inputValue(mCurrentTime, &status);
    if (!status) return status;
    const MTime currentTime = currentTimeHandle.asTime();
    MTime startTime;
    status = dommy::particle::readArrayTimeInput(
        data, mStartTime, logicalIndex, startTime);
    if (!status) return status;
    MTime deltaTime;
    status = dommy::particle::readArrayTimeInput(
        data, mDeltaTime, logicalIndex, deltaTime);
    if (!status) return status;

    const double currentSeconds = currentTime.as(MTime::kSeconds);
    const double startSeconds = startTime.as(MTime::kSeconds);
    const double deltaSeconds = deltaTime.as(MTime::kSeconds);
    if (!std::isfinite(currentSeconds) || !std::isfinite(startSeconds) ||
        !std::isfinite(deltaSeconds)) {
        MGlobal::displayError(
            "DPolygonEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime <= startTime || deltaSeconds <= 0.0) {
        return finishOutput();
    }

    MDataHandle vertexModeHandle = data.inputValue(isVertexEmit, &status);
    if (!status) return status;
    const bool emitVertices = vertexModeHandle.asBool();
    MDataHandle edgeModeHandle = data.inputValue(isEdgeEmit, &status);
    if (!status) return status;
    const bool emitEdges = edgeModeHandle.asBool();
    MDataHandle faceModeHandle = data.inputValue(isFaceEmit, &status);
    if (!status) return status;
    const bool emitFaces = faceModeHandle.asBool();
    if (!emitVertices && !emitEdges && !emitFaces) return finishOutput();

    MDataHandle edgeSamplingHandle = data.inputValue(edgeSampling, &status);
    if (!status) return status;
    const int rawEdgeSampling = edgeSamplingHandle.asInt();
    MDataHandle faceSamplingHandle = data.inputValue(faceSampling, &status);
    if (!status) return status;
    const int rawFaceSampling = faceSamplingHandle.asInt();

    std::size_t edgeSampleCount = 0U;
    if (emitEdges &&
        (!dommy::generation::checkedCount(
             rawEdgeSampling,
             dommy::generation::kMaxGeneratedPointCount,
             edgeSampleCount) ||
         edgeSampleCount == 0U)) {
        MGlobal::displayError(
            "DPolygonEmitter edge sampling must be positive and bounded.");
        return MS::kFailure;
    }
    std::size_t faceSampleCount = 0U;
    if (emitFaces &&
        (!dommy::generation::checkedCount(
             rawFaceSampling,
             dommy::generation::kMaxGeneratedPointCount,
             faceSampleCount) ||
         faceSampleCount == 0U)) {
        MGlobal::displayError(
            "DPolygonEmitter face sampling must be positive and bounded.");
        return MS::kFailure;
    }

    MDataHandle meshHandle = data.inputValue(inMeshObject, &status);
    if (!status) return status;
    if (meshHandle.type() != MFnData::kMesh || meshHandle.data().isNull()) {
        MGlobal::displayWarning(
            "DPolygonEmitter requires a valid mesh input.");
        return finishOutput();
    }

    MFnMesh mesh(meshHandle.data(), &status);
    if (!status) return status;
    const int rawVertexCount = mesh.numVertices(&status);
    if (!status) return status;
    std::size_t checkedVertexCount = 0U;
    if (!dommy::generation::checkedCount(
            rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
            checkedVertexCount)) {
        MGlobal::displayError(
            "DPolygonEmitter mesh exceeds the vertex safety limit.");
        return MS::kFailure;
    }
    if (checkedVertexCount == 0U) return finishOutput();
    const unsigned int vertexCount = static_cast<unsigned int>(checkedVertexCount);

    const int rawEdgeCount = mesh.numEdges(&status);
    if (!status) return status;
    const int rawPolygonCount = mesh.numPolygons(&status);
    if (!status) return status;
    if (rawEdgeCount < 0 || rawPolygonCount < 0) {
        MGlobal::displayError(
            "DPolygonEmitter mesh has invalid topology counts.");
        return MS::kFailure;
    }
    const std::size_t edgeCount =
        static_cast<std::size_t>(rawEdgeCount);
    const std::size_t polygonCount =
        static_cast<std::size_t>(rawPolygonCount);
    if ((emitEdges &&
         edgeCount > dommy::generation::kMaxWorkItemCount) ||
        (emitFaces &&
         polygonCount > dommy::generation::kMaxGeneratedFaceCount)) {
        MGlobal::displayError(
            "DPolygonEmitter mesh exceeds the topology safety limits.");
        return MS::kFailure;
    }

    MPointArray meshPoints;
    status = mesh.getPoints(meshPoints, MSpace::kObject);
    if (!status) return status;
    if (meshPoints.length() != checkedVertexCount) return MS::kFailure;
    for (unsigned int index = 0U; index < vertexCount; ++index) {
        if (!dommy::maya::isFinitePoint(meshPoints[index])) {
            MGlobal::displayError(
                "DPolygonEmitter mesh contains a non-finite point.");
            return MS::kFailure;
        }
    }

    std::vector<QuadVertexIndices> quadFaces;
    if (emitFaces) {
        quadFaces.reserve(polygonCount);
        for (std::size_t polygonIndex = 0U;
             polygonIndex < polygonCount; ++polygonIndex) {
            MIntArray polygonVertices;
            status = mesh.getPolygonVertices(
                static_cast<int>(polygonIndex), polygonVertices);
            if (!status) return status;
            if (polygonVertices.length() != 4U) continue;

            QuadVertexIndices quad = {{0, 0, 0, 0}};
            for (unsigned int corner = 0U; corner < 4U; ++corner) {
                const int vertexIndex = polygonVertices[corner];
                if (!isValidVertexIndex(vertexIndex, vertexCount)) {
                    MGlobal::displayError(
                        "DPolygonEmitter mesh has an invalid face index.");
                    return MS::kFailure;
                }
                quad[corner] = vertexIndex;
            }
            quadFaces.push_back(quad);
        }
    }

    std::size_t totalSampleCount = 0U;
    if (emitVertices &&
        !dommy::generation::checkedSum(
            totalSampleCount, static_cast<std::size_t>(vertexCount),
            dommy::generation::kMaxGeneratedPointCount,
            totalSampleCount)) {
        MGlobal::displayError(
            "DPolygonEmitter vertex output exceeds the point safety limit.");
        return MS::kFailure;
    }

    std::size_t edgeOutputCount = 0U;
    if (emitEdges &&
        (!dommy::generation::checkedProduct(
             edgeCount, edgeSampleCount,
             dommy::generation::kMaxGeneratedPointCount,
             edgeOutputCount) ||
         !dommy::generation::checkedSum(
             totalSampleCount, edgeOutputCount,
             dommy::generation::kMaxGeneratedPointCount,
             totalSampleCount))) {
        MGlobal::displayError(
            "DPolygonEmitter edge output exceeds the point safety limit.");
        return MS::kFailure;
    }

    std::size_t samplesPerQuad = 0U;
    std::size_t faceOutputCount = 0U;
    if (emitFaces &&
        (!dommy::generation::checkedProduct(
             faceSampleCount, faceSampleCount,
             dommy::generation::kMaxGeneratedPointCount,
             samplesPerQuad) ||
         !dommy::generation::checkedProduct(
             quadFaces.size(), samplesPerQuad,
             dommy::generation::kMaxGeneratedPointCount,
             faceOutputCount) ||
         !dommy::generation::checkedSum(
             totalSampleCount, faceOutputCount,
             dommy::generation::kMaxGeneratedPointCount,
             totalSampleCount))) {
        MGlobal::displayError(
            "DPolygonEmitter face output exceeds the point safety limit.");
        return MS::kFailure;
    }
    if (totalSampleCount == 0U) return finishOutput();

    MVectorArray outputPositions = outputData.vectorArray(
        "position", &status);
    if (!status) return status;
    status = outputPositions.setLength(
        static_cast<unsigned int>(totalSampleCount));
    if (!status) return status;
    unsigned int outputIndex = 0U;

    if (emitVertices) {
        for (unsigned int index = 0U; index < vertexCount; ++index) {
            status = appendPoint(
                meshPoints[index], outputPositions, outputIndex);
            if (!status) return status;
        }
    }

    if (emitEdges) {
        const double divisor =
            static_cast<double>(edgeSampleCount) + 1.0;
        for (std::size_t edgeIndex = 0U;
             edgeIndex < edgeCount; ++edgeIndex) {
            int edgeVertices[2] = {0, 0};
            status = mesh.getEdgeVertices(
                static_cast<int>(edgeIndex), edgeVertices);
            if (!status) return status;
            if (!isValidVertexIndex(edgeVertices[0], vertexCount) ||
                !isValidVertexIndex(edgeVertices[1], vertexCount)) {
                MGlobal::displayError(
                    "DPolygonEmitter mesh has an invalid edge index.");
                return MS::kFailure;
            }

            const MPoint& first = meshPoints[edgeVertices[0]];
            const MPoint& second = meshPoints[edgeVertices[1]];
            for (std::size_t sampleIndex = 1U;
                 sampleIndex <= edgeSampleCount; ++sampleIndex) {
                MPoint sample;
                if (!dommy::particle::blendMeshSample(
                        first, second, static_cast<double>(sampleIndex) / divisor,
                        sample)) return MS::kFailure;
                status = appendPoint(
                    sample, outputPositions, outputIndex);
                if (!status) return status;
            }
        }
    }

    if (emitFaces) {
        const double divisor =
            static_cast<double>(faceSampleCount) + 1.0;
        for (std::size_t faceIndex = 0U;
             faceIndex < quadFaces.size(); ++faceIndex) {
            const QuadVertexIndices& quad = quadFaces[faceIndex];
            const MPoint& point0 = meshPoints[quad[0]];

            if (faceSampleCount == 1U) {
                // Preserve the original single-sample diagonal midpoint.
                MPoint sample;
                if (!dommy::particle::blendMeshSample(
                        point0, meshPoints[quad[2]], 0.5, sample)) return MS::kFailure;
                status = appendPoint(
                    sample, outputPositions, outputIndex);
                if (!status) return status;
                continue;
            }

            // Preserve the historical quad rule: samples span the two edges
            // from vertex 0, rather than using bilinear interpolation.
            for (std::size_t x = 1U; x <= faceSampleCount; ++x) {
                const double fractionX = static_cast<double>(x) / divisor;
                for (std::size_t y = 1U; y <= faceSampleCount; ++y) {
                    const double fractionY =
                        static_cast<double>(y) / divisor;
                    MPoint sample;
                    if (!dommy::particle::quadMeshSample(
                            point0, meshPoints[quad[1]], meshPoints[quad[3]],
                            fractionX, fractionY, sample)) return MS::kFailure;
                    status = appendPoint(
                        sample, outputPositions, outputIndex);
                    if (!status) return status;
                }
            }
        }
    }

    if (static_cast<std::size_t>(outputIndex) != totalSampleCount) {
        MGlobal::displayError(
            "DPolygonEmitter generated an unexpected sample count.");
        return MS::kFailure;
    }

    MVectorArray outputVelocities = outputData.vectorArray(
        "velocity", &status);
    if (!status) return status;
    status = outputVelocities.setLength(outputIndex);
    if (!status) return status;
    for (unsigned int index = 0U; index < outputIndex; ++index) {
        outputVelocities[index] = MVector::zero;
    }

    // The archived implementation intentionally emits one particle for each
    // selected mesh sample and never used mRate as a multiplier or gate.
    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DPolygonEmitter")

MStatus DPolygonEmitter::initialize()
{
    MStatus status;
    MFnGenericAttribute genericAttribute;
    inMeshObject = genericAttribute.create(
        "inMeshObject", "imo", &status);
    if (!status) return status;
    status = genericAttribute.addDataAccept(MFnData::kMesh);
    if (!status) return status;
    status = genericAttribute.setStorable(false);
    if (!status) return status;
    status = addAttribute(inMeshObject);
    if (!status) return status;

    MFnNumericAttribute numericAttribute;
    isVertexEmit = numericAttribute.create(
        "isVertexEmit", "iv", MFnNumericData::kBoolean, true, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(isVertexEmit);
    if (!status) return status;

    isEdgeEmit = numericAttribute.create(
        "isEdgeEmit", "ie", MFnNumericData::kBoolean, false, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(isEdgeEmit);
    if (!status) return status;

    edgeSampling = numericAttribute.create(
        "edgeSampling", "es", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(edgeSampling);
    if (!status) return status;

    isFaceEmit = numericAttribute.create(
        "isFaceEmit", "if", MFnNumericData::kBoolean, false, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(isFaceEmit);
    if (!status) return status;

    faceSampling = numericAttribute.create(
        "faceSampling", "fs", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(faceSampling);
    if (!status) return status;

    const MObject inputs[] = {
        inMeshObject,
        isVertexEmit,
        isEdgeEmit,
        edgeSampling,
        isFaceEmit,
        faceSampling};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }

    return MS::kSuccess;
}
