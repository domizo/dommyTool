#include "common/utility/DMayaEntryPointGuard.h"
#include "DMeshVolumeEmitter.h"

#include "DBounds.h"
#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "DRandomGenerator.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnData.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

const double kSurfaceTolerance = 1.0e-4;

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

MStatus isPointInsideMesh(MFnMesh& mesh,
                          const MPoint& point,
                          double thickness,
                          bool& isInside)
{
    MPoint closestPoint;
    MVector normal;
    MStatus status = mesh.getClosestPointAndNormal(
        point, closestPoint, normal, MSpace::kObject);
    if (!status) return status;
    if (!dommy::maya::isFinitePoint(closestPoint) ||
        !isFiniteVector(normal)) {
        return MS::kFailure;
    }

    const double distance = closestPoint.distanceTo(point);
    if (!std::isfinite(distance)) return MS::kFailure;
    if (thickness > 0.0 && distance >= thickness) {
        isInside = false;
        return MS::kSuccess;
    }

    const double orientation = normal * (closestPoint - point);
    if (!std::isfinite(orientation)) return MS::kFailure;
    isInside = distance < kSurfaceTolerance || orientation > 0.0;
    return MS::kSuccess;
}

} // namespace

MTypeId DMeshVolumeEmitter::id(0x0190A);

MObject DMeshVolumeEmitter::inMeshObject;
MObject DMeshVolumeEmitter::transformMatrix;
MObject DMeshVolumeEmitter::isOutKill;
MObject DMeshVolumeEmitter::density;
MObject DMeshVolumeEmitter::densityX;
MObject DMeshVolumeEmitter::densityY;
MObject DMeshVolumeEmitter::densityZ;
MObject DMeshVolumeEmitter::thickness;
MObject DMeshVolumeEmitter::noise;
MObject DMeshVolumeEmitter::nSeed;

void* DMeshVolumeEmitter::creator()
{
    return ::dommy::maya::createObject("DMeshVolumeEmitter", [&]() {
        return new DMeshVolumeEmitter();
    });
}

MStatus DMeshVolumeEmitter::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError(
            "DMeshVolumeEmitter received an invalid output index.");
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
            "DMeshVolumeEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime <= startTime || deltaSeconds <= 0.0) {
        return finishOutput();
    }

    MDataHandle rateHandle = data.inputValue(mRate, &status);
    if (!status) return status;
    const double rate = rateHandle.asDouble();
    if (!std::isfinite(rate)) {
        MGlobal::displayError("DMeshVolumeEmitter rate must be finite.");
        return MS::kFailure;
    }
    // Preserve the historical behavior: rate is an enable control and each
    // accepted grid sample contributes one particle per evaluation.
    if (rate <= 0.0) return finishOutput();

    MDataHandle killHandle = data.inputValue(isOutKill, &status);
    if (!status) return status;
    const bool killJitteredOutsidePoints = killHandle.asBool();

    MDataHandle densityHandle = data.inputValue(density, &status);
    if (!status) return status;
    const int3& rawDensity = densityHandle.asInt3();

    MDataHandle thicknessHandle = data.inputValue(thickness, &status);
    if (!status) return status;
    const double thicknessValue = thicknessHandle.asDouble();
    MDataHandle noiseHandle = data.inputValue(noise, &status);
    if (!status) return status;
    const double noiseValue = noiseHandle.asDouble();
    MDataHandle seedHandle = data.inputValue(nSeed, &status);
    if (!status) return status;
    const int seedValue = seedHandle.asInt();
    if (!std::isfinite(thicknessValue) || thicknessValue < 0.0 ||
        !std::isfinite(noiseValue) || noiseValue < 0.0 ||
        noiseValue > 0.5 * std::numeric_limits<double>::max() ||
        seedValue < 0) {
        MGlobal::displayError(
            "DMeshVolumeEmitter generation parameters are invalid.");
        return MS::kFailure;
    }

    std::size_t intervalCounts[3] = {0U, 0U, 0U};
    std::size_t sampleCounts[3] = {0U, 0U, 0U};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!dommy::generation::checkedCount(
                rawDensity[axis],
                dommy::generation::kMaxGeneratedPointCount,
                intervalCounts[axis]) ||
            intervalCounts[axis] == 0U ||
            !dommy::generation::checkedSum(
                intervalCounts[axis], 1U,
                dommy::generation::kMaxGeneratedPointCount,
                sampleCounts[axis])) {
            MGlobal::displayError(
                "DMeshVolumeEmitter density must be positive and bounded.");
            return MS::kFailure;
        }
    }

    std::size_t sampleCountXY = 0U;
    std::size_t gridSampleCount = 0U;
    if (!dommy::generation::checkedProduct(
            sampleCounts[0], sampleCounts[1],
            dommy::generation::kMaxGeneratedPointCount, sampleCountXY) ||
        !dommy::generation::checkedProduct(
            sampleCountXY, sampleCounts[2],
            dommy::generation::kMaxGeneratedPointCount,
            gridSampleCount)) {
        MGlobal::displayError(
            "DMeshVolumeEmitter density exceeds the point safety limit.");
        return MS::kFailure;
    }

    MDataHandle meshHandle = data.inputValue(inMeshObject, &status);
    if (!status) return status;
    if (meshHandle.type() != MFnData::kMesh || meshHandle.data().isNull()) {
        MGlobal::displayWarning(
            "DMeshVolumeEmitter requires a valid mesh input.");
        return finishOutput();
    }

    MFnMesh mesh(meshHandle.data(), &status);
    if (!status) return status;
    const int rawVertexCount = mesh.numVertices(&status);
    if (!status) return status;
    std::size_t vertexCount = 0U;
    if (!dommy::generation::checkedCount(
            rawVertexCount, dommy::generation::kMaxGeneratedPointCount,
            vertexCount)) return MS::kFailure;
    if (vertexCount == 0U) return finishOutput();
    const int polygonCount = mesh.numPolygons(&status);
    if (!status) return status;
    if (polygonCount <= 0) {
        return finishOutput();
    }
    if (static_cast<std::size_t>(polygonCount) >
            dommy::generation::kMaxGeneratedFaceCount) {
        MGlobal::displayError(
            "DMeshVolumeEmitter mesh exceeds the supported topology limits.");
        return MS::kFailure;
    }

    MPointArray meshPoints;
    status = mesh.getPoints(meshPoints, MSpace::kObject);
    if (!status) return status;
    if (meshPoints.length() != vertexCount) return MS::kFailure;

    dommy::math::Bounds3d bounds;
    for (unsigned int index = 0U; index < meshPoints.length(); ++index) {
        const MPoint& point = meshPoints[index];
        if (!dommy::maya::isFinitePoint(point) ||
            !bounds.add(point.x, point.y, point.z)) {
            MGlobal::displayError(
                "DMeshVolumeEmitter mesh contains a non-finite point.");
            return MS::kFailure;
        }
    }
    if (bounds.empty()) return finishOutput();

    const double extents[3] = {
        bounds.maximum(0U) - bounds.minimum(0U),
        bounds.maximum(1U) - bounds.minimum(1U),
        bounds.maximum(2U) - bounds.minimum(2U)};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!std::isfinite(extents[axis])) return MS::kFailure;
        if (extents[axis] <= 0.0) return finishOutput();
    }

    MDataHandle matrixHandle = data.inputValue(transformMatrix, &status);
    if (!status) return status;
    const MMatrix meshWorldMatrix = matrixHandle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(meshWorldMatrix)) {
        MGlobal::displayError(
            "DMeshVolumeEmitter transform matrix must be finite.");
        return MS::kFailure;
    }

    MVectorArray emittedPositions;
    status = emittedPositions.setLength(
        static_cast<unsigned int>(gridSampleCount));
    if (!status) return status;
    if (emittedPositions.length() != gridSampleCount) return MS::kFailure;
    unsigned int emittedCount = 0U;
    dommy::math::RandomGenerator randomGenerator(
        static_cast<std::uint32_t>(seedValue));

    for (std::size_t x = 0U; x < sampleCounts[0]; ++x) {
        const double positionX = bounds.minimum(0U) + extents[0] *
            (static_cast<double>(x) /
             static_cast<double>(intervalCounts[0]));
        for (std::size_t y = 0U; y < sampleCounts[1]; ++y) {
            const double positionY = bounds.minimum(1U) + extents[1] *
                (static_cast<double>(y) /
                 static_cast<double>(intervalCounts[1]));
            for (std::size_t z = 0U; z < sampleCounts[2]; ++z) {
                const double positionZ = bounds.minimum(2U) + extents[2] *
                    (static_cast<double>(z) /
                     static_cast<double>(intervalCounts[2]));
                MPoint candidate(positionX, positionY, positionZ);
                if (!dommy::maya::isFinitePoint(candidate)) {
                    return MS::kFailure;
                }

                bool isInside = false;
                status = isPointInsideMesh(
                    mesh, candidate, thicknessValue, isInside);
                if (!status) return status;
                if (!isInside) continue;

                if (noiseValue > 0.0) {
                    candidate.x += randomGenerator.uniform(
                        -noiseValue, noiseValue);
                    candidate.y += randomGenerator.uniform(
                        -noiseValue, noiseValue);
                    candidate.z += randomGenerator.uniform(
                        -noiseValue, noiseValue);
                    if (!dommy::maya::isFinitePoint(candidate)) {
                        return MS::kFailure;
                    }
                    if (killJitteredOutsidePoints) {
                        status = isPointInsideMesh(
                            mesh, candidate, 0.0, isInside);
                        if (!status) return status;
                        if (!isInside) continue;
                    }
                }

                MPoint worldPoint = candidate * meshWorldMatrix;
                if (!dommy::maya::isFinitePoint(worldPoint) ||
                    std::fabs(worldPoint.w) <=
                        std::numeric_limits<double>::epsilon()) {
                    MGlobal::displayError(
                        "DMeshVolumeEmitter produced an invalid world point.");
                    return MS::kFailure;
                }
                worldPoint.cartesianize();
                if (!dommy::maya::isFinitePoint(worldPoint)) {
                    return MS::kFailure;
                }
                emittedPositions[emittedCount++] = MVector(
                    worldPoint.x, worldPoint.y, worldPoint.z);
            }
        }
    }

    status = emittedPositions.setLength(emittedCount);
    if (!status) return status;
    if (emittedPositions.length() != emittedCount) return MS::kFailure;
    MVectorArray emittedVelocities;
    status = dommy::maya::buildCheckedArray(
        emittedCount,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        }, emittedVelocities);
    if (!status) return status;
    status = dommy::particle::copyArrayAttribute(
        outputData, "position", emittedPositions);
    if (!status) return status;
    status = dommy::particle::copyArrayAttribute(
        outputData, "velocity", emittedVelocities);
    if (!status) return status;

    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DMeshVolumeEmitter")

MStatus DMeshVolumeEmitter::initialize()
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

    MFnNumericAttribute numericAttribute;
    isOutKill = numericAttribute.create(
        "isOutKill", "iok", MFnNumericData::kBoolean, true, &status);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(isOutKill);
    if (!status) return status;

    densityX = numericAttribute.create(
        "densityX", "dex", MFnNumericData::kInt, 4, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    densityY = numericAttribute.create(
        "densityY", "dey", MFnNumericData::kInt, 4, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    densityZ = numericAttribute.create(
        "densityZ", "dez", MFnNumericData::kInt, 4, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;

    density = numericAttribute.create(
        "density", "de", densityX, densityY, densityZ, &status);
    if (!status) return status;
    status = numericAttribute.setDefault(4, 4, 4);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(density);
    if (!status) return status;

    thickness = numericAttribute.create(
        "thickness", "th", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(thickness);
    if (!status) return status;

    noise = numericAttribute.create(
        "noise", "no", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(noise);
    if (!status) return status;

    nSeed = numericAttribute.create(
        "nSeed", "nse", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(1);
    if (!status) return status;
    status = numericAttribute.setKeyable(true);
    if (!status) return status;
    status = numericAttribute.setStorable(true);
    if (!status) return status;
    status = addAttribute(nSeed);
    if (!status) return status;

    const MObject inputs[] = {
        inMeshObject,
        transformMatrix,
        isOutKill,
        density,
        densityX,
        densityY,
        densityZ,
        thickness,
        noise,
        nSeed};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }

    return MS::kSuccess;
}
