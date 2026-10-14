#include "common/utility/DMayaEntryPointGuard.h"
#include "DEmitter.h"

#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointMean.h"
#include "DRandomGenerator.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDynSweptTriangle.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnDynSweptGeometryData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool checkedTriangleCenter(MDynSweptTriangle& triangle,
                           double time,
                           MVector& center)
{
    const std::array<MVector, 3> vertices = {{
        triangle.vertex(0, time), triangle.vertex(1, time),
        triangle.vertex(2, time)}};
    std::array<double, 3> mean;
    if (!dommy::numeric::checkedPointMean(vertices, mean)) return false;
    center = MVector(mean[0], mean[1], mean[2]);
    return true;
}

MStatus configureNumericAttribute(MFnNumericAttribute& attribute)
{
    MStatus status = attribute.setKeyable(true);
    if (!status) return status;
    return attribute.setStorable(true);
}

} // namespace

MTypeId DEmitter::id(0x0399A);

MObject DEmitter::samplingArea;
MObject DEmitter::timeSampling;
MObject DEmitter::timeSamples;
MObject DEmitter::timeNoise;

void* DEmitter::creator()
{
    return ::dommy::maya::createObject("DEmitter", [&]() {
        return new DEmitter();
    });
}

MStatus DEmitter::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError("DEmitter received an invalid output index.");
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
        MGlobal::displayError("DEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime <= startTime || deltaSeconds <= 0.0) {
        return finishOutput();
    }

    // In the archived implementation mRate only enabled or disabled the
    // active triangle path; each triangle still emitted exactly one sample.
    MDataHandle rateHandle = data.inputValue(mRate, &status);
    if (!status) return status;
    const double rate = rateHandle.asDouble();
    if (!std::isfinite(rate)) {
        MGlobal::displayError("DEmitter rate must be finite.");
        return MS::kFailure;
    }
    if (rate <= 0.0) return finishOutput();

    MDataHandle timeSamplingHandle = data.inputValue(timeSampling, &status);
    if (!status) return status;
    const bool sampleAcrossTime = timeSamplingHandle.asBool();

    MDataHandle timeSamplesHandle = data.inputValue(timeSamples, &status);
    if (!status) return status;
    const int rawTimeSampleCount = timeSamplesHandle.asInt();
    std::size_t timeSampleCount = 0U;
    if (!dommy::generation::checkedCount(
            rawTimeSampleCount,
            dommy::generation::kMaxGeneratedPointCount,
            timeSampleCount)) {
        MGlobal::displayError(
            "DEmitter time sample count exceeds the safety limit.");
        return MS::kFailure;
    }
    if (!sampleAcrossTime) timeSampleCount = 0U;

    MDataHandle timeNoiseHandle = data.inputValue(timeNoise, &status);
    if (!status) return status;
    const double timeNoiseAmount =
        static_cast<double>(timeNoiseHandle.asFloat());
    if (!std::isfinite(timeNoiseAmount) || timeNoiseAmount < 0.0) {
        MGlobal::displayError(
            "DEmitter time noise must be finite and non-negative.");
        return MS::kFailure;
    }

    const MPlug sweptGeometryPlug(thisMObject(), mSweptGeometry);
    const bool isConnected = sweptGeometryPlug.isConnected(&status);
    if (!status) return status;
    if (!isConnected) return finishOutput();

    MDataHandle sweptGeometryHandle =
        data.inputValue(mSweptGeometry, &status);
    if (!status) return status;
    const MObject sweptGeometryObject = sweptGeometryHandle.data();
    if (sweptGeometryObject.isNull()) return finishOutput();

    MFnDynSweptGeometryData sweptGeometry(
        sweptGeometryObject, &status);
    if (!status) return status;
    const int rawTriangleCount = sweptGeometry.triangleCount(&status);
    if (!status) return status;
    std::size_t triangleCount = 0U;
    if (!dommy::generation::checkedCount(
            rawTriangleCount,
            dommy::generation::kMaxGeneratedPointCount,
            triangleCount)) {
        MGlobal::displayError(
            "DEmitter triangle count exceeds the safety limit.");
        return MS::kFailure;
    }
    if (triangleCount == 0U) return finishOutput();

    std::size_t samplesPerTriangle = 0U;
    if (!dommy::generation::checkedSum(
            1U, timeSampleCount,
            dommy::generation::kMaxGeneratedPointCount,
            samplesPerTriangle)) {
        return MS::kFailure;
    }
    std::size_t outputCount = 0U;
    if (!dommy::generation::checkedProduct(
            triangleCount, samplesPerTriangle,
            dommy::generation::kMaxGeneratedPointCount,
            outputCount)) {
        MGlobal::displayError(
            "DEmitter output exceeds the generated-point safety limit.");
        return MS::kFailure;
    }

    MVectorArray outputPositions = outputData.vectorArray(
        "position", &status);
    if (!status) return status;
    status = outputPositions.setLength(
        static_cast<unsigned int>(outputCount));
    if (!status) return status;
    MVectorArray outputVelocities = outputData.vectorArray(
        "velocity", &status);
    if (!status) return status;
    status = outputVelocities.setLength(
        static_cast<unsigned int>(outputCount));
    if (!status) return status;

    dommy::math::RandomGenerator randomGenerator(
        5489U ^ static_cast<std::uint32_t>(logicalIndex));
    unsigned int outputIndex = 0U;
    for (std::size_t triangleIndex = 0U;
         triangleIndex < triangleCount; ++triangleIndex) {
        MDynSweptTriangle triangle = sweptGeometry.sweptTriangle(
            static_cast<int>(triangleIndex), &status);
        if (!status) return status;

        MVector currentCenter;
        if (!checkedTriangleCenter(triangle, 1.0, currentCenter)) {
            MGlobal::displayError(
                "DEmitter encountered non-finite swept geometry.");
            return MS::kFailure;
        }
        outputPositions[outputIndex] = currentCenter;
        outputVelocities[outputIndex++] = MVector::zero;

        if (timeSampleCount == 0U) continue;
        MVector previousCenter;
        if (!checkedTriangleCenter(triangle, 0.0, previousCenter)) {
            MGlobal::displayError(
                "DEmitter encountered non-finite swept geometry.");
            return MS::kFailure;
        }

        for (std::size_t sampleIndex = 0U;
             sampleIndex < timeSampleCount; ++sampleIndex) {
            const double fraction =
                static_cast<double>(sampleIndex + 1U) /
                static_cast<double>(timeSampleCount + 1U);
            MVector sample;
            if (!dommy::attribute::checkedLinearBlend(
                    currentCenter.x, previousCenter.x, fraction, sample.x) ||
                !dommy::attribute::checkedLinearBlend(
                    currentCenter.y, previousCenter.y, fraction, sample.y) ||
                !dommy::attribute::checkedLinearBlend(
                    currentCenter.z, previousCenter.z, fraction, sample.z)) {
                return MS::kFailure;
            }
            if (timeNoiseAmount > 0.0) {
                sample.x += randomGenerator.uniform(
                    -timeNoiseAmount, timeNoiseAmount);
                sample.y += randomGenerator.uniform(
                    -timeNoiseAmount, timeNoiseAmount);
                sample.z += randomGenerator.uniform(
                    -timeNoiseAmount, timeNoiseAmount);
            }
            if (!isFiniteVector(sample)) {
                MGlobal::displayError(
                    "DEmitter generated a non-finite sample.");
                return MS::kFailure;
            }
            outputPositions[outputIndex] = sample;
            outputVelocities[outputIndex++] = MVector::zero;
        }
    }

    if (static_cast<std::size_t>(outputIndex) != outputCount) {
        MGlobal::displayError(
            "DEmitter generated an unexpected sample count.");
        return MS::kFailure;
    }
    return finishOutput();
}
DOMMY_COMPUTE_CATCH("DEmitter")

MStatus DEmitter::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;

    samplingArea = numericAttribute.create(
        "samplingArea", "spa", MFnNumericData::kInt, 0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0);
    if (!status) return status;
    status = numericAttribute.setMax(
        static_cast<int>(dommy::generation::kMaxGeneratedPointCount));
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(samplingArea);
    if (!status) return status;

    timeSampling = numericAttribute.create(
        "timeSampling", "tm", MFnNumericData::kBoolean, true, &status);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(timeSampling);
    if (!status) return status;

    timeSamples = numericAttribute.create(
        "timeSample", "tms", MFnNumericData::kInt, 0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0);
    if (!status) return status;
    status = numericAttribute.setMax(
        static_cast<int>(dommy::generation::kMaxGeneratedPointCount));
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(timeSamples);
    if (!status) return status;

    timeNoise = numericAttribute.create(
        "timeNoise", "tmn", MFnNumericData::kFloat, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(timeNoise);
    if (!status) return status;

    const MObject inputs[] = {
        samplingArea, timeSampling, timeSamples, timeNoise};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }
    return MS::kSuccess;
}
