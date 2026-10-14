#include "common/utility/DMayaEntryPointGuard.h"
#include "curveEmitter.h"

#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "perlin.h"
#include "DRandomGenerator.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDynSweptLine.h>
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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

const int kMaximumNoiseFrequency = 1000000;

bool isFiniteVector(const MVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

MStatus configureNumericAttribute(MFnNumericAttribute& attribute)
{
    MStatus status = attribute.setKeyable(true);
    if (!status) return status;
    return attribute.setStorable(true);
}

MStatus appendSample(const MVector& sample,
                     MVectorArray& positions,
                     MVectorArray& velocities,
                     unsigned int& outputIndex)
{
    if (outputIndex >= positions.length() ||
        outputIndex >= velocities.length()) {
        MGlobal::displayError(
            "curveEmitter exceeded its precomputed sample count.");
        return MS::kFailure;
    }
    if (!isFiniteVector(sample)) {
        MGlobal::displayError(
            "curveEmitter generated a non-finite sample.");
        return MS::kFailure;
    }

    positions[outputIndex] = sample;
    velocities[outputIndex++] = MVector::zero;
    return MS::kSuccess;
}

} // namespace

MTypeId curveEmitter::id(0x0188A);

MObject curveEmitter::startPosition;
MObject curveEmitter::endPosition;
MObject curveEmitter::samplingArea;
MObject curveEmitter::curveRadius;
MObject curveEmitter::radiusNoise;
MObject curveEmitter::rNoisefrequency;
MObject curveEmitter::rNoiseScale;

void* curveEmitter::creator()
{
    return ::dommy::maya::createObject("curveEmitter", [&]() {
        return new curveEmitter();
    });
}

MStatus curveEmitter::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != mOutput) return MS::kUnknownParameter;

    MStatus status;
    const int rawLogicalIndex = plug.logicalIndex(&status);
    if (!status) return status;
    if (rawLogicalIndex < 0) {
        MGlobal::displayError(
            "curveEmitter received an invalid output index.");
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
        MGlobal::displayError("curveEmitter time inputs must be finite.");
        return MS::kFailure;
    }
    if (currentTime <= startTime || deltaSeconds <= 0.0) {
        return finishOutput();
    }

    MDataHandle rateHandle = data.inputValue(mRate, &status);
    if (!status) return status;
    const double rate = rateHandle.asDouble();
    if (!std::isfinite(rate)) {
        MGlobal::displayError("curveEmitter rate must be finite.");
        return MS::kFailure;
    }
    if (rate <= 0.0) return finishOutput();
    const double roundedRate = std::ceil(rate);
    if (!std::isfinite(roundedRate) ||
        roundedRate > static_cast<double>(
            dommy::generation::kMaxGeneratedPointCount)) {
        MGlobal::displayError(
            "curveEmitter rate exceeds the generated-point safety limit.");
        return MS::kFailure;
    }
    const std::size_t rateCount =
        static_cast<std::size_t>(roundedRate);

    MDataHandle startHandle = data.inputValue(startPosition, &status);
    if (!status) return status;
    const double startValue = startHandle.asDouble();
    MDataHandle endHandle = data.inputValue(endPosition, &status);
    if (!status) return status;
    const double endValue = endHandle.asDouble();
    MDataHandle radiusHandle = data.inputValue(curveRadius, &status);
    if (!status) return status;
    const double radius = radiusHandle.asDouble();
    MDataHandle samplingHandle = data.inputValue(samplingArea, &status);
    if (!status) return status;
    const int rawAreaSampleCount = samplingHandle.asInt();
    MDataHandle radiusNoiseHandle = data.inputValue(radiusNoise, &status);
    if (!status) return status;
    const double radiusNoiseAmount = radiusNoiseHandle.asDouble();
    MDataHandle noiseFrequencyHandle =
        data.inputValue(rNoisefrequency, &status);
    if (!status) return status;
    const int noiseFrequency = noiseFrequencyHandle.asInt();
    MDataHandle noiseScaleHandle = data.inputValue(rNoiseScale, &status);
    if (!status) return status;
    const double noiseScale = noiseScaleHandle.asDouble();

    if (!std::isfinite(startValue) || !std::isfinite(endValue) ||
        startValue < 0.0 || startValue > 1.0 ||
        endValue < 0.0 || endValue > 1.0 ||
        !std::isfinite(radius) || radius < 0.0 ||
        !std::isfinite(radiusNoiseAmount) || radiusNoiseAmount < 0.0 ||
        noiseFrequency < 0 || noiseFrequency > kMaximumNoiseFrequency ||
        !std::isfinite(noiseScale)) {
        MGlobal::displayError(
            "curveEmitter received an invalid sampling or noise value.");
        return MS::kFailure;
    }
    std::size_t areaSampleCount = 0U;
    if (!dommy::generation::checkedCount(
            rawAreaSampleCount,
            dommy::generation::kMaxGeneratedPointCount,
            areaSampleCount)) {
        MGlobal::displayError(
            "curveEmitter area sampling exceeds the safety limit.");
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
    const int rawLineCount = sweptGeometry.lineCount(&status);
    if (!status) return status;
    std::size_t lineCount = 0U;
    if (!dommy::generation::checkedCount(
            rawLineCount,
            dommy::generation::kMaxGeneratedPointCount,
            lineCount)) {
        MGlobal::displayError(
            "curveEmitter line count exceeds the safety limit.");
        return MS::kFailure;
    }
    if (lineCount == 0U) return finishOutput();

    const std::size_t firstIndex = std::min(
        static_cast<std::size_t>(
            std::floor(static_cast<double>(lineCount) * startValue)),
        lineCount - 1U);
    const std::size_t lastIndex = std::min(
        static_cast<std::size_t>(
            std::floor(static_cast<double>(lineCount) * endValue)),
        lineCount - 1U);
    const std::size_t minimumIndex = std::min(firstIndex, lastIndex);
    const std::size_t maximumIndex = std::max(firstIndex, lastIndex);
    const std::size_t selectedLineCount =
        maximumIndex - minimumIndex + 1U;
    const std::size_t areaLineCount =
        selectedLineCount - (minimumIndex == 0U ? 1U : 0U);

    std::size_t primarySampleCount = 0U;
    std::size_t areaSamplesPerLine = 0U;
    std::size_t totalAreaSampleCount = 0U;
    std::size_t outputCount = 0U;
    if (!dommy::generation::checkedProduct(
            selectedLineCount, rateCount,
            dommy::generation::kMaxGeneratedPointCount,
            primarySampleCount) ||
        !dommy::generation::checkedProduct(
            areaSampleCount, rateCount,
            dommy::generation::kMaxGeneratedPointCount,
            areaSamplesPerLine) ||
        !dommy::generation::checkedProduct(
            areaLineCount, areaSamplesPerLine,
            dommy::generation::kMaxGeneratedPointCount,
            totalAreaSampleCount) ||
        !dommy::generation::checkedSum(
            primarySampleCount, totalAreaSampleCount,
            dommy::generation::kMaxGeneratedPointCount,
            outputCount)) {
        MGlobal::displayError(
            "curveEmitter output exceeds the generated-point safety limit.");
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

    noise::module::Perlin perlinNoise;
    perlinNoise.SetSeed(1);
    perlinNoise.SetFrequency(static_cast<double>(noiseFrequency));
    perlinNoise.SetOctaveCount(2);
    dommy::math::RandomGenerator randomGenerator(
        5489U ^ static_cast<std::uint32_t>(logicalIndex));

    unsigned int outputIndex = 0U;
    for (std::size_t lineIndex = minimumIndex;
         lineIndex <= maximumIndex; ++lineIndex) {
        MDynSweptLine line = sweptGeometry.sweptLine(
            static_cast<int>(lineIndex), &status);
        if (!status) return status;
        const MVector currentPoint = line.vertex(0);
        if (!isFiniteVector(currentPoint)) {
            MGlobal::displayError(
                "curveEmitter encountered non-finite swept geometry.");
            return MS::kFailure;
        }

        MVector previousPoint;
        if (lineIndex > 0U && areaSampleCount > 0U) {
            MDynSweptLine previousLine = sweptGeometry.sweptLine(
                static_cast<int>(lineIndex - 1U), &status);
            if (!status) return status;
            previousPoint = previousLine.vertex(0);
            if (!isFiniteVector(previousPoint)) {
                MGlobal::displayError(
                    "curveEmitter encountered non-finite swept geometry.");
                return MS::kFailure;
            }
        }

        for (std::size_t emissionIndex = 0U;
             emissionIndex < rateCount; ++emissionIndex) {
            if (lineIndex > 0U) {
                for (std::size_t sampleIndex = 0U;
                     sampleIndex < areaSampleCount; ++sampleIndex) {
                    const double fraction =
                        static_cast<double>(sampleIndex + 1U) /
                        static_cast<double>(areaSampleCount + 1U);
                    MVector sample;
                    if (!dommy::attribute::checkedLinearBlend(
                            previousPoint.x, currentPoint.x, fraction, sample.x) ||
                        !dommy::attribute::checkedLinearBlend(
                            previousPoint.y, currentPoint.y, fraction, sample.y) ||
                        !dommy::attribute::checkedLinearBlend(
                            previousPoint.z, currentPoint.z, fraction, sample.z)) {
                        return MS::kFailure;
                    }
                    status = appendSample(
                        sample, outputPositions, outputVelocities,
                        outputIndex);
                    if (!status) return status;
                }
            }

            MVector sample = currentPoint;
            if (radius > 0.0 || radiusNoiseAmount > 0.0) {
                const double noiseX = currentPoint.x * noiseScale;
                const double noiseY = currentPoint.y * noiseScale;
                const double noiseZ = currentPoint.z * noiseScale;
                if (!std::isfinite(noiseX) || !std::isfinite(noiseY) ||
                    !std::isfinite(noiseZ)) {
                    MGlobal::displayError(
                        "curveEmitter noise coordinates must be finite.");
                    return MS::kFailure;
                }
                const double noiseValue = radiusNoiseAmount > 0.0
                    ? perlinNoise.GetValue(noiseX, noiseY, noiseZ)
                    : 0.0;
                if (!std::isfinite(noiseValue)) return MS::kFailure;

                // curveRadius historically represented a diameter. Modulate
                // it with the noise amplitude, clamp a negative radius, then
                // preserve the archived axis-aligned jitter style.
                const double scatterRadius = std::max(
                    0.0, radius + radiusNoiseAmount * noiseValue) * 0.5;
                if (!std::isfinite(scatterRadius)) {
                    MGlobal::displayError(
                        "curveEmitter generated a non-finite radius.");
                    return MS::kFailure;
                }
                sample.x += randomGenerator.uniform(
                    -scatterRadius, scatterRadius);
                sample.y += randomGenerator.uniform(
                    -scatterRadius, scatterRadius);
                sample.z += randomGenerator.uniform(
                    -scatterRadius, scatterRadius);
            }
            status = appendSample(
                sample, outputPositions, outputVelocities, outputIndex);
            if (!status) return status;
        }
    }

    if (static_cast<std::size_t>(outputIndex) != outputCount) {
        MGlobal::displayError(
            "curveEmitter generated an unexpected sample count.");
        return MS::kFailure;
    }
    return finishOutput();
}
DOMMY_COMPUTE_CATCH("curveEmitter")

MStatus curveEmitter::initialize()
{
    MStatus status;
    MFnNumericAttribute numericAttribute;

    startPosition = numericAttribute.create(
        "startPosition", "stp", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setMax(1.0);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(startPosition);
    if (!status) return status;

    endPosition = numericAttribute.create(
        "endPosition", "edp", MFnNumericData::kDouble, 1.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = numericAttribute.setMax(1.0);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(endPosition);
    if (!status) return status;

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

    curveRadius = numericAttribute.create(
        "curveRadius", "crr", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(curveRadius);
    if (!status) return status;

    radiusNoise = numericAttribute.create(
        "radiusNoise", "rno", MFnNumericData::kDouble, 0.0, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0.0);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(radiusNoise);
    if (!status) return status;

    rNoisefrequency = numericAttribute.create(
        "rNoisefrequency", "rnf", MFnNumericData::kInt, 1, &status);
    if (!status) return status;
    status = numericAttribute.setMin(0);
    if (!status) return status;
    status = numericAttribute.setMax(kMaximumNoiseFrequency);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(rNoisefrequency);
    if (!status) return status;

    rNoiseScale = numericAttribute.create(
        "rNoiseScale", "rns", MFnNumericData::kDouble, 1.0, &status);
    if (!status) return status;
    status = configureNumericAttribute(numericAttribute);
    if (!status) return status;
    status = addAttribute(rNoiseScale);
    if (!status) return status;

    const MObject inputs[] = {
        startPosition, endPosition, samplingArea, curveRadius,
        radiusNoise, rNoisefrequency, rNoiseScale};
    for (std::size_t index = 0U;
         index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        status = attributeAffects(inputs[index], mOutput);
        if (!status) return status;
    }
    return MS::kSuccess;
}
