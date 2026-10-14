#include "common/utility/DMayaEntryPointGuard.h"
#include "DRadialGeometryEmitter.h"
#include "DMeshEmitterSampling.h"

#include "PerlinNoise.h"
#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "DRandomGenerator.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
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
#include <cstdint>
#include <limits>
#include <vector>

namespace {

const int kMaximumNoiseOctaveCount = 16;

typedef std::array<int, 4> QuadVertexIndices;

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool isValidVertexIndex(int index, unsigned int vertexCount)
{
    return index >= 0 && static_cast<unsigned int>(index) < vertexCount;
}

MStatus setKeyableAndStorable(MFnAttribute& attribute)
{
    MStatus status = attribute.setKeyable(true);
    if (!status) return status;
    return attribute.setStorable(true);
}

MStatus setSamplePoint(const MPoint& point,
                       MPointArray& points,
                       unsigned int& outputIndex)
{
    if (outputIndex >= points.length()) {
        MGlobal::displayError(
            "DRadialGeometryEmitter exceeded its sample allocation.");
        return MS::kFailure;
    }
    if (!dommy::maya::isFinitePoint(point)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter produced a non-finite sample point.");
        return MS::kFailure;
    }

    points[outputIndex++] = point;
    return MS::kSuccess;
}

MStatus collectCurveSamples(MFnNurbsCurve& curve,
                            bool sampleCurve,
                            int rawSampling,
                            MPointArray& points)
{
    MStatus status;
    if (!sampleCurve) {
        const int rawCvCount = curve.numCVs(&status);
        if (!status) return status;
        std::size_t cvCount = 0U;
        if (!dommy::generation::checkedCount(
                rawCvCount,
                dommy::generation::kMaxGeneratedPointCount,
                cvCount)) {
            MGlobal::displayError(
                "DRadialGeometryEmitter curve exceeds the CV safety limit.");
            return MS::kFailure;
        }
        status = curve.getCVs(points, MSpace::kObject);
        if (!status) return status;
        return points.length() == cvCount ? MS::kSuccess : MS::kFailure;
    }

    std::size_t sampleCount = 0U;
    if (!dommy::generation::checkedCount(
            rawSampling,
            dommy::generation::kMaxGeneratedPointCount,
            sampleCount) ||
        sampleCount < 2U) {
        MGlobal::displayError(
            "DRadialGeometryEmitter curve sampling must be at least two and bounded.");
        return MS::kFailure;
    }

    const double totalLength = curve.length(kMFnNurbsEpsilon, &status);
    if (!status) return status;
    if (!std::isfinite(totalLength) || totalLength < 0.0) {
        MGlobal::displayError(
            "DRadialGeometryEmitter curve length must be finite.");
        return MS::kFailure;
    }

    status = points.setLength(static_cast<unsigned int>(sampleCount));
    if (!status) return status;
    unsigned int outputIndex = 0U;
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        // Preserve the archival rule: sample the start and equally spaced
        // interior lengths, but do not duplicate the curve end.
        const double partLength = totalLength *
            (static_cast<double>(index) /
             static_cast<double>(sampleCount));
        const double parameter = curve.findParamFromLength(
            partLength, &status);
        if (!status) return status;
        if (!std::isfinite(parameter)) return MS::kFailure;

        MPoint point;
        status = curve.getPointAtParam(
            parameter, point, MSpace::kObject);
        if (!status) return status;
        status = setSamplePoint(point, points, outputIndex);
        if (!status) return status;
    }
    return outputIndex == points.length() ? MS::kSuccess : MS::kFailure;
}

MStatus collectSurfaceSamples(MFnNurbsSurface& surface,
                              bool sampleSurface,
                              int rawSampling,
                              MPointArray& points)
{
    MStatus status;
    if (!sampleSurface) {
        const int rawCvCountU = surface.numCVsInU(&status);
        if (!status) return status;
        const int rawCvCountV = surface.numCVsInV(&status);
        if (!status) return status;
        std::size_t cvCountU = 0U;
        std::size_t cvCountV = 0U;
        std::size_t cvCount = 0U;
        if (!dommy::generation::checkedCount(
                rawCvCountU,
                dommy::generation::kMaxGeneratedPointCount,
                cvCountU) ||
            !dommy::generation::checkedCount(
                rawCvCountV,
                dommy::generation::kMaxGeneratedPointCount,
                cvCountV) ||
            !dommy::generation::checkedProduct(
                cvCountU, cvCountV,
                dommy::generation::kMaxGeneratedPointCount,
                cvCount)) {
            MGlobal::displayError(
                "DRadialGeometryEmitter surface exceeds the CV safety limit.");
            return MS::kFailure;
        }
        status = surface.getCVs(points, MSpace::kObject);
        if (!status) return status;
        return points.length() == cvCount ? MS::kSuccess : MS::kFailure;
    }

    std::size_t intervalCount = 0U;
    std::size_t axisSampleCount = 0U;
    std::size_t totalSampleCount = 0U;
    if (!dommy::generation::checkedCount(
            rawSampling,
            dommy::generation::kMaxGeneratedPointCount,
            intervalCount) ||
        intervalCount < 2U ||
        !dommy::generation::checkedSum(
            intervalCount, 1U,
            dommy::generation::kMaxGeneratedPointCount,
            axisSampleCount) ||
        !dommy::generation::checkedProduct(
            axisSampleCount, axisSampleCount,
            dommy::generation::kMaxGeneratedPointCount,
            totalSampleCount)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter surface sampling exceeds the point safety limit.");
        return MS::kFailure;
    }

    double startU = 0.0;
    double endU = 0.0;
    double startV = 0.0;
    double endV = 0.0;
    status = surface.getKnotDomain(startU, endU, startV, endV);
    if (!status) return status;
    if (!std::isfinite(startU) || !std::isfinite(endU) ||
        !std::isfinite(startV) || !std::isfinite(endV) ||
        endU < startU || endV < startV) {
        MGlobal::displayError(
            "DRadialGeometryEmitter surface has an invalid parameter domain.");
        return MS::kFailure;
    }
    const double extentU = endU - startU;
    const double extentV = endV - startV;
    if (!std::isfinite(extentU) || !std::isfinite(extentV)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter surface parameter extent overflowed.");
        return MS::kFailure;
    }

    status = points.setLength(
        static_cast<unsigned int>(totalSampleCount));
    if (!status) return status;
    unsigned int outputIndex = 0U;
    for (std::size_t u = 0U; u < axisSampleCount; ++u) {
        const double parameterU = u == intervalCount ? endU :
            startU + extentU *
                (static_cast<double>(u) /
                 static_cast<double>(intervalCount));
        for (std::size_t v = 0U; v < axisSampleCount; ++v) {
            const double parameterV = v == intervalCount ? endV :
                startV + extentV *
                    (static_cast<double>(v) /
                     static_cast<double>(intervalCount));
            MPoint point;
            status = surface.getPointAtParam(
                parameterU, parameterV, point, MSpace::kObject);
            if (!status) return status;
            status = setSamplePoint(point, points, outputIndex);
            if (!status) return status;
        }
    }
    return outputIndex == points.length() ? MS::kSuccess : MS::kFailure;
}

MStatus collectMeshSamples(MFnMesh& mesh,
                           bool emitVertices,
                           bool emitEdges,
                           std::size_t edgeSampleCount,
                           bool emitFaces,
                           std::size_t faceSampleCount,
                           MPointArray& points)
{
    if (!emitVertices && !emitEdges && !emitFaces) {
        return points.clear();
    }

    MStatus status;
    const int rawVertexCount = mesh.numVertices(&status);
    if (!status) return status;
    std::size_t checkedVertexCount = 0U;
    if (!dommy::generation::checkedCount(
            rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
            checkedVertexCount)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter mesh exceeds the vertex safety limit.");
        return MS::kFailure;
    }
    if (checkedVertexCount == 0U) {
        return points.clear();
    }
    const unsigned int vertexCount = static_cast<unsigned int>(checkedVertexCount);

    const int rawEdgeCount = mesh.numEdges(&status);
    if (!status) return status;
    const int rawPolygonCount = mesh.numPolygons(&status);
    if (!status) return status;
    if (rawEdgeCount < 0 || rawPolygonCount < 0) return MS::kFailure;
    const std::size_t edgeCount =
        static_cast<std::size_t>(rawEdgeCount);
    const std::size_t polygonCount =
        static_cast<std::size_t>(rawPolygonCount);
    if ((emitEdges &&
         edgeCount > dommy::generation::kMaxWorkItemCount) ||
        (emitFaces &&
         polygonCount > dommy::generation::kMaxGeneratedFaceCount)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter mesh exceeds the topology safety limits.");
        return MS::kFailure;
    }

    MPointArray vertices;
    status = mesh.getPoints(vertices, MSpace::kObject);
    if (!status) return status;
    if (vertices.length() != checkedVertexCount) return MS::kFailure;
    for (unsigned int index = 0U; index < vertexCount; ++index) {
        if (!dommy::maya::isFinitePoint(vertices[index])) {
            MGlobal::displayError(
                "DRadialGeometryEmitter mesh contains a non-finite point.");
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
                        "DRadialGeometryEmitter mesh has an invalid face index.");
                    return MS::kFailure;
                }
                quad[corner] = vertexIndex;
            }
            quadFaces.push_back(quad);
        }
    }

    std::size_t totalSampleCount =
        emitVertices ? static_cast<std::size_t>(vertexCount) : 0U;
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
            "DRadialGeometryEmitter edge output exceeds the point safety limit.");
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
            "DRadialGeometryEmitter face output exceeds the point safety limit.");
        return MS::kFailure;
    }

    status = points.setLength(
        static_cast<unsigned int>(totalSampleCount));
    if (!status) return status;
    unsigned int outputIndex = 0U;
    if (emitVertices) {
        for (unsigned int index = 0U; index < vertexCount; ++index) {
            status = setSamplePoint(vertices[index], points, outputIndex);
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
                    "DRadialGeometryEmitter mesh has an invalid edge index.");
                return MS::kFailure;
            }

            const MPoint& first = vertices[edgeVertices[0]];
            const MPoint& second = vertices[edgeVertices[1]];
            for (std::size_t sampleIndex = 1U;
                 sampleIndex <= edgeSampleCount; ++sampleIndex) {
                MPoint sample;
                if (!dommy::particle::blendMeshSample(
                        first, second, static_cast<double>(sampleIndex) / divisor,
                        sample)) return MS::kFailure;
                status = setSamplePoint(sample, points, outputIndex);
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
            const MPoint& point0 = vertices[quad[0]];
            if (faceSampleCount == 1U) {
                MPoint sample;
                if (!dommy::particle::blendMeshSample(
                        point0, vertices[quad[2]], 0.5, sample)) return MS::kFailure;
                status = setSamplePoint(sample, points, outputIndex);
                if (!status) return status;
                continue;
            }

            for (std::size_t x = 1U; x <= faceSampleCount; ++x) {
                const double fractionX = static_cast<double>(x) / divisor;
                for (std::size_t y = 1U; y <= faceSampleCount; ++y) {
                    const double fractionY =
                        static_cast<double>(y) / divisor;
                    MPoint sample;
                    if (!dommy::particle::quadMeshSample(
                            point0, vertices[quad[1]], vertices[quad[3]],
                            fractionX, fractionY, sample)) return MS::kFailure;
                    status = setSamplePoint(sample, points, outputIndex);
                    if (!status) return status;
                }
            }
        }
    }

    return outputIndex == points.length() ? MS::kSuccess : MS::kFailure;
}

bool isSafePerlinCoordinate(double value, int octaveCount)
{
    if (!std::isfinite(value) || octaveCount < 1) return false;
    double maximumMagnitude =
        static_cast<double>(std::numeric_limits<std::int32_t>::max());
    for (int octave = 1; octave < octaveCount; ++octave) {
        maximumMagnitude *= 0.5;
    }
    return value >= -maximumMagnitude && value <= maximumMagnitude;
}

bool checkedNoiseCoordinate(double position,
                            double scale,
                            double offset,
                            int octaveCount,
                            double& result)
{
    result = position * scale + offset;
    return isSafePerlinCoordinate(result, octaveCount);
}

} // namespace

MTypeId DRadialGeometryEmitter::id(0x0191A);

MObject DRadialGeometryEmitter::inputGeometry;
MObject DRadialGeometryEmitter::transformMatrix;
MObject DRadialGeometryEmitter::emitType;
MObject DRadialGeometryEmitter::emitSpeedMultiply;
MObject DRadialGeometryEmitter::emitSpeedRandom;
MObject DRadialGeometryEmitter::emitPositionBias;
MObject DRadialGeometryEmitter::emitSourcePosition;
MObject DRadialGeometryEmitter::emitSourcePositionX;
MObject DRadialGeometryEmitter::emitSourcePositionY;
MObject DRadialGeometryEmitter::emitSourcePositionZ;
MObject DRadialGeometryEmitter::emitDirection;
MObject DRadialGeometryEmitter::emitDirectionX;
MObject DRadialGeometryEmitter::emitDirectionY;
MObject DRadialGeometryEmitter::emitDirectionZ;
MObject DRadialGeometryEmitter::isNurbsSampling;
MObject DRadialGeometryEmitter::sampling;
MObject DRadialGeometryEmitter::isVertexEmit;
MObject DRadialGeometryEmitter::isEdgeEmit;
MObject DRadialGeometryEmitter::edgeSampling;
MObject DRadialGeometryEmitter::isFaceEmit;
MObject DRadialGeometryEmitter::faceSampling;
MObject DRadialGeometryEmitter::nSeed;
MObject DRadialGeometryEmitter::nFrequency;
MObject DRadialGeometryEmitter::directionNoise;
MObject DRadialGeometryEmitter::directionNoiseX;
MObject DRadialGeometryEmitter::directionNoiseY;
MObject DRadialGeometryEmitter::directionNoiseZ;
MObject DRadialGeometryEmitter::noiseScale;
MObject DRadialGeometryEmitter::noiseScaleX;
MObject DRadialGeometryEmitter::noiseScaleY;
MObject DRadialGeometryEmitter::noiseScaleZ;
MObject DRadialGeometryEmitter::noisePositionOffset;
MObject DRadialGeometryEmitter::noisePositionOffsetX;
MObject DRadialGeometryEmitter::noisePositionOffsetY;
MObject DRadialGeometryEmitter::noisePositionOffsetZ;

void* DRadialGeometryEmitter::creator()
{
    return ::dommy::maya::createObject("DRadialGeometryEmitter", [&]() {
        return new DRadialGeometryEmitter();
    });
}

MStatus DRadialGeometryEmitter::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError(
            "DRadialGeometryEmitter received an invalid output index.");
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
            "DRadialGeometryEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime <= startTime || deltaSeconds <= 0.0) {
        return finishOutput();
    }

    MDataHandle rateHandle = data.inputValue(mRate, &status);
    if (!status) return status;
    const double rate = rateHandle.asDouble();
    if (!std::isfinite(rate)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter rate must be finite.");
        return MS::kFailure;
    }
    // Preserve the archival behavior: any nonzero rate enables one particle
    // per geometry sample; rate is not used as a particle-count multiplier.
    if (rate == 0.0) return finishOutput();

    MDataHandle speedHandle = data.inputValue(mSpeed, &status);
    if (!status) return status;
    const double speed = speedHandle.asDouble();
    MDataHandle emitTypeHandle = data.inputValue(emitType, &status);
    if (!status) return status;
    const int emitTypeValue = emitTypeHandle.asInt();
    MDataHandle speedMultiplyHandle = data.inputValue(
        emitSpeedMultiply, &status);
    if (!status) return status;
    const double speedMultiply = speedMultiplyHandle.asDouble();
    MDataHandle speedRandomHandle = data.inputValue(
        emitSpeedRandom, &status);
    if (!status) return status;
    const double speedRandom = speedRandomHandle.asDouble();
    MDataHandle positionBiasHandle = data.inputValue(
        emitPositionBias, &status);
    if (!status) return status;
    const double positionBias = positionBiasHandle.asDouble();

    MDataHandle sourcePositionHandle = data.inputValue(
        emitSourcePosition, &status);
    if (!status) return status;
    const double3& rawSourcePosition = sourcePositionHandle.asDouble3();
    const MVector sourcePosition(
        rawSourcePosition[0], rawSourcePosition[1], rawSourcePosition[2]);
    MDataHandle directionHandle = data.inputValue(emitDirection, &status);
    if (!status) return status;
    const double3& rawDirection = directionHandle.asDouble3();
    const MVector fixedDirection(
        rawDirection[0], rawDirection[1], rawDirection[2]);

    MDataHandle nurbsSamplingHandle = data.inputValue(
        isNurbsSampling, &status);
    if (!status) return status;
    const bool sampleNurbs = nurbsSamplingHandle.asBool();
    MDataHandle samplingHandle = data.inputValue(sampling, &status);
    if (!status) return status;
    const int rawNurbsSampling = samplingHandle.asInt();

    MDataHandle vertexModeHandle = data.inputValue(isVertexEmit, &status);
    if (!status) return status;
    const bool emitVertices = vertexModeHandle.asBool();
    MDataHandle edgeModeHandle = data.inputValue(isEdgeEmit, &status);
    if (!status) return status;
    const bool emitEdges = edgeModeHandle.asBool();
    MDataHandle edgeSamplingHandle = data.inputValue(edgeSampling, &status);
    if (!status) return status;
    const int rawEdgeSampling = edgeSamplingHandle.asInt();
    MDataHandle faceModeHandle = data.inputValue(isFaceEmit, &status);
    if (!status) return status;
    const bool emitFaces = faceModeHandle.asBool();
    MDataHandle faceSamplingHandle = data.inputValue(faceSampling, &status);
    if (!status) return status;
    const int rawFaceSampling = faceSamplingHandle.asInt();

    MDataHandle seedHandle = data.inputValue(nSeed, &status);
    if (!status) return status;
    const int seed = seedHandle.asInt();
    MDataHandle frequencyHandle = data.inputValue(nFrequency, &status);
    if (!status) return status;
    const int noiseOctaveCount = frequencyHandle.asInt();
    MDataHandle directionNoiseHandle = data.inputValue(
        directionNoise, &status);
    if (!status) return status;
    const double3& rawDirectionNoise = directionNoiseHandle.asDouble3();
    const MVector directionNoiseValue(
        rawDirectionNoise[0], rawDirectionNoise[1], rawDirectionNoise[2]);
    MDataHandle noiseScaleHandle = data.inputValue(noiseScale, &status);
    if (!status) return status;
    const double3& rawNoiseScale = noiseScaleHandle.asDouble3();
    const MVector noiseScaleValue(
        rawNoiseScale[0], rawNoiseScale[1], rawNoiseScale[2]);
    MDataHandle noiseOffsetHandle = data.inputValue(
        noisePositionOffset, &status);
    if (!status) return status;
    const double3& rawNoiseOffset = noiseOffsetHandle.asDouble3();
    const MVector noiseOffsetValue(
        rawNoiseOffset[0], rawNoiseOffset[1], rawNoiseOffset[2]);

    MDataHandle matrixHandle = data.inputValue(transformMatrix, &status);
    if (!status) return status;
    const MMatrix worldMatrix = matrixHandle.asMatrix();

    const double maximumHalf =
        0.5 * std::numeric_limits<double>::max();
    if ((emitTypeValue != D_RADIAL_EMIT_SOURCE &&
         emitTypeValue != D_RADIAL_EMIT_DIRECTION) ||
        !std::isfinite(speed) ||
        !std::isfinite(speedMultiply) || speedMultiply < 0.0 ||
        !std::isfinite(speedRandom) || speedRandom < 0.0 ||
        speedRandom > maximumHalf ||
        !std::isfinite(positionBias) || positionBias < 0.0 ||
        !isFiniteVector(sourcePosition) ||
        !isFiniteVector(fixedDirection) ||
        !isFiniteVector(directionNoiseValue) ||
        !isFiniteVector(noiseScaleValue) ||
        !isFiniteVector(noiseOffsetValue) ||
        !dommy::maya::isFiniteMatrix(worldMatrix) ||
        seed < 1 || noiseOctaveCount < 1 ||
        noiseOctaveCount > kMaximumNoiseOctaveCount) {
        MGlobal::displayError(
            "DRadialGeometryEmitter has invalid emission or noise parameters.");
        return MS::kFailure;
    }
    const double baseSpeed = speed * speedMultiply;
    if (!std::isfinite(baseSpeed) ||
        (speedRandom > 0.0 &&
         std::fabs(baseSpeed) >
             std::numeric_limits<double>::max() - speedRandom)) {
        MGlobal::displayError(
            "DRadialGeometryEmitter speed range overflowed.");
        return MS::kFailure;
    }

    MDataHandle geometryHandle = data.inputValue(inputGeometry, &status);
    if (!status) return status;
    const MObject geometryObject = geometryHandle.data();
    if (geometryObject.isNull()) {
        MGlobal::displayWarning(
            "DRadialGeometryEmitter requires geometry input.");
        return finishOutput();
    }

    MPointArray samplePoints;
    if (geometryHandle.type() == MFnData::kNurbsCurve) {
        MFnNurbsCurve curve(geometryObject, &status);
        if (!status) return status;
        status = collectCurveSamples(
            curve, sampleNurbs, rawNurbsSampling, samplePoints);
        if (!status) return status;
    } else if (geometryHandle.type() == MFnData::kNurbsSurface) {
        MFnNurbsSurface surface(geometryObject, &status);
        if (!status) return status;
        status = collectSurfaceSamples(
            surface, sampleNurbs, rawNurbsSampling, samplePoints);
        if (!status) return status;
    } else if (geometryHandle.type() == MFnData::kMesh) {
        std::size_t edgeSampleCount = 0U;
        if (emitEdges &&
            (!dommy::generation::checkedCount(
                 rawEdgeSampling,
                 dommy::generation::kMaxGeneratedPointCount,
                 edgeSampleCount) ||
             edgeSampleCount == 0U)) {
            MGlobal::displayError(
                "DRadialGeometryEmitter edge sampling must be positive and bounded.");
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
                "DRadialGeometryEmitter face sampling must be positive and bounded.");
            return MS::kFailure;
        }

        MFnMesh mesh(geometryObject, &status);
        if (!status) return status;
        status = collectMeshSamples(
            mesh, emitVertices, emitEdges, edgeSampleCount,
            emitFaces, faceSampleCount, samplePoints);
        if (!status) return status;
    } else {
        // Vector-array and unsupported legacy inputs intentionally emit no
        // particles; the kAny attribute is retained for scene compatibility.
        return finishOutput();
    }

    const std::size_t totalSampleCount = samplePoints.length();
    if (totalSampleCount == 0U) return finishOutput();
    if (totalSampleCount > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kFailure;
    }
    for (unsigned int index = 0U; index < samplePoints.length(); ++index) {
        if (!dommy::maya::isFinitePoint(samplePoints[index])) {
            MGlobal::displayError(
                "DRadialGeometryEmitter geometry contains a non-finite point.");
            return MS::kFailure;
        }
    }

    // Signed amplitudes reverse an axis; only an all-zero vector disables noise.
    const bool noiseEnabled = directionNoiseValue.x != 0.0 ||
        directionNoiseValue.y != 0.0 || directionNoiseValue.z != 0.0;
    if (noiseEnabled) {
        std::size_t octaveWorkCount = 0U;
        std::size_t noiseWorkCount = 0U;
        if (!dommy::generation::checkedProduct(
                totalSampleCount,
                static_cast<std::size_t>(noiseOctaveCount),
                dommy::generation::kMaxWorkItemCount,
                octaveWorkCount) ||
            !dommy::generation::checkedProduct(
                octaveWorkCount, 3U,
                dommy::generation::kMaxWorkItemCount,
                noiseWorkCount)) {
            MGlobal::displayError(
                "DRadialGeometryEmitter noise exceeds the work safety limit.");
            return MS::kFailure;
        }
    }

    MVectorArray outputPositions = outputData.vectorArray(
        "position", &status);
    if (!status) return status;
    status = outputPositions.setLength(
        static_cast<unsigned int>(totalSampleCount));
    if (!status) return status;
    MVectorArray outputVelocities = outputData.vectorArray(
        "velocity", &status);
    if (!status) return status;
    status = outputVelocities.setLength(
        static_cast<unsigned int>(totalSampleCount));
    if (!status) return status;

    noise::PerlinOctave perlinNoise(
        noiseOctaveCount, static_cast<std::uint32_t>(seed));
    dommy::math::RandomGenerator randomGenerator(
        static_cast<std::uint32_t>(seed));
    for (unsigned int index = 0U; index < samplePoints.length(); ++index) {
        MVector target(
            samplePoints[index].x,
            samplePoints[index].y,
            samplePoints[index].z);
        if (noiseEnabled) {
            double x0 = 0.0;
            double y0 = 0.0;
            double z0 = 0.0;
            double y1 = 0.0;
            double z1 = 0.0;
            double x1 = 0.0;
            double z2 = 0.0;
            double x2 = 0.0;
            double y2 = 0.0;
            if (!checkedNoiseCoordinate(
                    target.x, noiseScaleValue.x, noiseOffsetValue.x,
                    noiseOctaveCount, x0) ||
                !checkedNoiseCoordinate(
                    target.y, noiseScaleValue.x, noiseOffsetValue.x,
                    noiseOctaveCount, y0) ||
                !checkedNoiseCoordinate(
                    target.z, noiseScaleValue.x, noiseOffsetValue.x,
                    noiseOctaveCount, z0) ||
                !checkedNoiseCoordinate(
                    target.y, noiseScaleValue.y, noiseOffsetValue.y,
                    noiseOctaveCount, y1) ||
                !checkedNoiseCoordinate(
                    target.z, noiseScaleValue.y, noiseOffsetValue.y,
                    noiseOctaveCount, z1) ||
                !checkedNoiseCoordinate(
                    target.x, noiseScaleValue.y, noiseOffsetValue.y,
                    noiseOctaveCount, x1) ||
                !checkedNoiseCoordinate(
                    target.z, noiseScaleValue.z, noiseOffsetValue.z,
                    noiseOctaveCount, z2) ||
                !checkedNoiseCoordinate(
                    target.x, noiseScaleValue.z, noiseOffsetValue.z,
                    noiseOctaveCount, x2) ||
                !checkedNoiseCoordinate(
                    target.y, noiseScaleValue.z, noiseOffsetValue.z,
                    noiseOctaveCount, y2)) {
                MGlobal::displayError(
                    "DRadialGeometryEmitter noise coordinates exceed the safe range.");
                return MS::kFailure;
            }

            const MVector noiseVector(
                perlinNoise.noise(x0, y0, z0) * directionNoiseValue.x,
                perlinNoise.noise(y1, z1, x1) * directionNoiseValue.y,
                perlinNoise.noise(z2, x2, y2) * directionNoiseValue.z);
            if (!isFiniteVector(noiseVector)) {
                MGlobal::displayError(
                    "DRadialGeometryEmitter produced non-finite noise.");
                return MS::kFailure;
            }
            target += noiseVector;
            if (!isFiniteVector(target)) return MS::kFailure;
        }

        MVector localPosition;
        MVector localDirection;
        if (emitTypeValue == D_RADIAL_EMIT_SOURCE) {
            localPosition = (target - sourcePosition) * positionBias;
            // Preserve the archival distance-scaled radial velocity rule.
            localDirection = target - sourcePosition;
        } else {
            localPosition = target;
            localDirection = fixedDirection;
        }
        if (!isFiniteVector(localPosition) ||
            !isFiniteVector(localDirection)) {
            return MS::kFailure;
        }

        double speedVariation = 0.0;
        if (speedRandom > 0.0) {
            speedVariation = randomGenerator.uniform(
                -speedRandom, speedRandom);
            if (baseSpeed + speedVariation < 0.0) {
                speedVariation = -baseSpeed;
            }
        }
        const double particleSpeed = baseSpeed + speedVariation;
        if (!std::isfinite(particleSpeed)) return MS::kFailure;

        MPoint worldPoint(
            localPosition.x, localPosition.y, localPosition.z);
        worldPoint *= worldMatrix;
        if (!dommy::maya::isFinitePoint(worldPoint) ||
            std::fabs(worldPoint.w) <=
                std::numeric_limits<double>::epsilon()) {
            MGlobal::displayError(
                "DRadialGeometryEmitter produced an invalid world point.");
            return MS::kFailure;
        }
        worldPoint.cartesianize();
        const MVector worldVelocity =
            (localDirection * particleSpeed) * worldMatrix;
        if (!dommy::maya::isFinitePoint(worldPoint) ||
            !isFiniteVector(worldVelocity)) {
            return MS::kFailure;
        }

        outputPositions[index] = MVector(
            worldPoint.x, worldPoint.y, worldPoint.z);
        outputVelocities[index] = worldVelocity;
    }

    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DRadialGeometryEmitter")

MStatus DRadialGeometryEmitter::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;
    inputGeometry = typedAttribute.create(
        "inputGeometry", "igo", MFnData::kAny,
        MObject::kNullObj, &status);
    if (!status) return status;
    status = typedAttribute.setStorable(false);
    if (!status) return status;
    status = typedAttribute.setHidden(true);
    if (!status) return status;
    status = addAttribute(inputGeometry);
    if (!status) return status;

    MFnMatrixAttribute matrixAttribute;
    transformMatrix = matrixAttribute.create(
        "transformMatrix", "tm", MFnMatrixAttribute::kDouble, &status);
    if (!status) return status;
    status = matrixAttribute.setStorable(true);
    if (!status) return status;
    status = matrixAttribute.setKeyable(false);
    if (!status) return status;
    status = matrixAttribute.setReadable(true);
    if (!status) return status;
    status = matrixAttribute.setWritable(true);
    if (!status) return status;
    status = matrixAttribute.setCached(false);
    if (!status) return status;
    status = addAttribute(transformMatrix);
    if (!status) return status;

    MFnEnumAttribute enumAttribute;
    emitType = enumAttribute.create(
        "emitType", "et", D_RADIAL_EMIT_SOURCE, &status);
    if (!status) return status;
    status = enumAttribute.addField(
        "Source", D_RADIAL_EMIT_SOURCE);
    if (!status) return status;
    status = enumAttribute.addField(
        "Direction", D_RADIAL_EMIT_DIRECTION);
    if (!status) return status;
    status = setKeyableAndStorable(enumAttribute);
    if (!status) return status;
    status = addAttribute(emitType);
    if (!status) return status;

    MFnNumericAttribute numericAttribute;
    emitSpeedMultiply = numericAttribute.create(
        "emitSpeedMultiply", "esm", MFnNumericData::kDouble,
        1.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(emitSpeedMultiply);
    if (!status) return status;

    emitSpeedRandom = numericAttribute.create(
        "emitSpeedRandom", "esr", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(emitSpeedRandom);
    if (!status) return status;

    emitPositionBias = numericAttribute.create(
        "emitPositionBias", "epb", MFnNumericData::kDouble,
        1.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(emitPositionBias);
    if (!status) return status;

    emitSourcePositionX = numericAttribute.create(
        "emitSourcePositionX", "esx", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitSourcePositionY = numericAttribute.create(
        "emitSourcePositionY", "esy", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitSourcePositionZ = numericAttribute.create(
        "emitSourcePositionZ", "esz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitSourcePosition = numericAttribute.create(
        "emitSourcePosition", "es",
        emitSourcePositionX, emitSourcePositionY,
        emitSourcePositionZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(0.0, 0.0, 0.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(emitSourcePosition);
    if (!status) return status;

    emitDirectionX = numericAttribute.create(
        "emitDirectionX", "edx", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitDirectionY = numericAttribute.create(
        "emitDirectionY", "edy", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitDirectionZ = numericAttribute.create(
        "emitDirectionZ", "edz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    emitDirection = numericAttribute.create(
        "emitDirection", "ed", emitDirectionX,
        emitDirectionY, emitDirectionZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(1.0, 0.0, 0.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(emitDirection);
    if (!status) return status;

    isNurbsSampling = numericAttribute.create(
        "isNurbsSampling", "ins", MFnNumericData::kBoolean,
        false, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(isNurbsSampling);
    if (!status) return status;

    sampling = numericAttribute.create(
        "sampling", "sa", MFnNumericData::kInt, 10, &status);
    if (!status) return status;
    status = numericAttribute.setMin(2);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(sampling);
    if (!status) return status;

    isVertexEmit = numericAttribute.create(
        "isVertexEmit", "ive", MFnNumericData::kBoolean,
        true, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(isVertexEmit);
    if (!status) return status;

    isEdgeEmit = numericAttribute.create(
        "isEdgeEmit", "iee", MFnNumericData::kBoolean,
        false, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(isEdgeEmit);
    if (!status) return status;

    edgeSampling = numericAttribute.create(
        "edgeSampling", "esa", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(edgeSampling);
    if (!status) return status;

    isFaceEmit = numericAttribute.create(
        "isFaceEmit", "ife", MFnNumericData::kBoolean,
        false, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(isFaceEmit);
    if (!status) return status;

    faceSampling = numericAttribute.create(
        "faceSampling", "fsa", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(faceSampling);
    if (!status) return status;

    nSeed = numericAttribute.create(
        "nSeed", "nse", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(nSeed);
    if (!status) return status;

    nFrequency = numericAttribute.create(
        "nFrequency", "nfr", MFnNumericData::kInt, 2, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setMax(kMaximumNoiseOctaveCount);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    status = addAttribute(nFrequency);
    if (!status) return status;

    directionNoiseX = numericAttribute.create(
        "directionNoiseX", "dnx", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    directionNoiseY = numericAttribute.create(
        "directionNoiseY", "dny", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    directionNoiseZ = numericAttribute.create(
        "directionNoiseZ", "dnz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    directionNoise = numericAttribute.create(
        "directionNoise", "dn", directionNoiseX,
        directionNoiseY, directionNoiseZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(0.0, 0.0, 0.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(directionNoise);
    if (!status) return status;

    noiseScaleX = numericAttribute.create(
        "noiseScaleX", "nsx", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noiseScaleY = numericAttribute.create(
        "noiseScaleY", "nsy", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noiseScaleZ = numericAttribute.create(
        "noiseScaleZ", "nsz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noiseScale = numericAttribute.create(
        "noiseScale", "ns", noiseScaleX,
        noiseScaleY, noiseScaleZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(1.0, 1.0, 1.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(noiseScale);
    if (!status) return status;

    noisePositionOffsetX = numericAttribute.create(
        "noisePositionOffsetX", "nox", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noisePositionOffsetY = numericAttribute.create(
        "noisePositionOffsetY", "noy", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noisePositionOffsetZ = numericAttribute.create(
        "noisePositionOffsetZ", "noz", MFnNumericData::kDouble,
        0.0, &status);
    if (!status) return status;
    status = setKeyableAndStorable(numericAttribute);
    if (!status) return status;
    noisePositionOffset = numericAttribute.create(
        "noisePositionOffset", "npo", noisePositionOffsetX,
        noisePositionOffsetY, noisePositionOffsetZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(0.0, 0.0, 0.0);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(noisePositionOffset);
    if (!status) return status;

    const MObject inputs[] = {
        inputGeometry,
        transformMatrix,
        emitType,
        emitSpeedMultiply,
        emitSpeedRandom,
        emitPositionBias,
        emitSourcePosition,
        emitSourcePositionX,
        emitSourcePositionY,
        emitSourcePositionZ,
        emitDirection,
        emitDirectionX,
        emitDirectionY,
        emitDirectionZ,
        isNurbsSampling,
        sampling,
        isVertexEmit,
        isEdgeEmit,
        edgeSampling,
        isFaceEmit,
        faceSampling,
        nSeed,
        nFrequency,
        directionNoise,
        directionNoiseX,
        directionNoiseY,
        directionNoiseZ,
        noiseScale,
        noiseScaleX,
        noiseScaleY,
        noiseScaleZ,
        noisePositionOffset,
        noisePositionOffsetX,
        noisePositionOffsetY,
        noisePositionOffsetZ};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }

    return MS::kSuccess;
}
