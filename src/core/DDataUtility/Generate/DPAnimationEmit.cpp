//
//  DPAnimationEmit.cpp
//  dommyTool
//
//  Created by yuzo on 2015/03/25.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAnimationEmit.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "dpoint_ids.h"
#include "DRandomGenerator.h"

#include <maya/MDGContextGuard.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPointArray.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>


const MTypeId DPAnimationEmit::typeId(ID_DP_ANIMATION_EMIT);

MObject DPAnimationEmit::inputGeometry;
MObject DPAnimationEmit::transformMatrix;

MObject DPAnimationEmit::emitType;
MObject DPAnimationEmit::seed;
MObject DPAnimationEmit::randomCount;

MObject DPAnimationEmit::timeOffset;
MObject DPAnimationEmit::timeLength;
MObject DPAnimationEmit::timeSampling;
MObject DPAnimationEmit::timeLengthRandom;

MObject DPAnimationEmit::outputPointArray;


namespace {

MStatus finishOutput(MDataBlock& block,
                     const MPlug& plug,
                     const MObject& outputAttribute,
                     const MPointArray& points)
{
    MStatus status;
    MFnPointArrayData pointData;
    const MObject outputObject = pointData.create(points, &status);
    if (!status) return status;
    if (outputObject.isNull()) return MS::kFailure;

    MDataHandle output = block.outputValue(outputAttribute, &status);
    if (!status) return status;
    status = output.set(outputObject);
    if (!status) return status;
    output.setClean();
    return block.setClean(plug);
}

} // namespace


void* DPAnimationEmit::creator()
{
    return ::dommy::maya::createObject("DPAnimationEmit", [&]() {
        return new DPAnimationEmit();
    });
}


MStatus DPAnimationEmit::getPlugMatrices(std::vector<MMatrix>& matrices,
                                         const MPlug& sourcePlug,
                                         unsigned int count,
                                         double step,
                                         double start) const
{
    if (!std::isfinite(step) || !std::isfinite(start)) {
        return MS::kInvalidParameter;
    }

    matrices.clear();
    matrices.reserve(count);
    for (unsigned int index = 0U; index < count; ++index) {
        const double sampleTime =
            start + step * static_cast<double>(index);
        if (!std::isfinite(sampleTime)) return MS::kInvalidParameter;

        const MDGContext context(MTime(sampleTime, MTime::uiUnit()));
        MDGContextGuard contextGuard(context);

        MObject matrixObject;
        MStatus status = sourcePlug.getValue(matrixObject);
        if (!status || !matrixObject.hasFn(MFn::kMatrixData)) {
            return MS::kFailure;
        }

        MFnMatrixData matrixData(matrixObject, &status);
        if (!status) return status;
        const MMatrix matrix = matrixData.matrix(&status);
        if (!status || !dommy::maya::isFiniteMatrix(matrix)) {
            return MS::kFailure;
        }
        matrices.push_back(matrix);
    }
    return MS::kSuccess;
}


MStatus DPAnimationEmit::compute(const MPlug& plug, MDataBlock& block) try
{
    if (plug != outputPointArray) return MS::kUnknownParameter;

    MStatus status;
    dommy::maya::CheckedArray<MPointArray> outputPoints(
        static_cast<unsigned int>(dommy::generation::kMaxGeneratedPointCount));

    const MDataHandle emitTypeInputHandle = block.inputValue(emitType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DP_ANIM_EMIT_TYPE emitTypeValue = emitTypeInputHandle.asInt();
    const MDataHandle seedInputHandle = block.inputValue(seed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int seedValue = seedInputHandle.asInt();
    const MDataHandle randomCountInputHandle = block.inputValue(randomCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawRandomCount = randomCountInputHandle.asInt();
    const MDataHandle timeOffsetInputHandle = block.inputValue(timeOffset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double timeOffsetValue = timeOffsetInputHandle.asDouble();
    const MDataHandle timeLengthInputHandle = block.inputValue(timeLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double timeLengthValue = timeLengthInputHandle.asDouble();
    const MDataHandle timeSamplingInputHandle = block.inputValue(timeSampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawTimeSampling = timeSamplingInputHandle.asInt();
    const MDataHandle timeLengthRandomInputHandle = block.inputValue(timeLengthRandom, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double timeLengthRandomValue = timeLengthRandomInputHandle.asDouble();

    std::size_t randomSampleCount = 0U;
    std::size_t timeSampleCount = 0U;
    if (emitTypeValue < 0 || emitTypeValue >= DP_ANIM_EMIT_TYPE_COUNT ||
        !dommy::generation::checkedCount(rawRandomCount, dommy::generation::kMaxGeneratedPointCount,
                                         randomSampleCount) ||
        !dommy::generation::checkedCount(
            rawTimeSampling, dommy::generation::kMaxGeneratedPointCount, timeSampleCount) ||
        timeSampleCount == 0U || !std::isfinite(timeOffsetValue) ||
        !std::isfinite(timeLengthValue) || !std::isfinite(timeLengthRandomValue)) {
        return MS::kInvalidParameter;
    }

    // A matrix-only trail does not require an input geometry object.
    MPointArray sourcePoints;
    if (emitTypeValue != DP_ANIM_EMIT_MATRIX) {
        std::size_t sourcePointCount = 0U;
        const MDataHandle inputObjectHandle = block.inputValue(inputGeometry, &status);
        if (!status)
            return status;

        switch (inputObjectHandle.type()) {
        case MFnData::kNurbsCurve: {
            MFnNurbsCurve curve(inputObjectHandle.asNurbsCurve(), &status);
            if (!status)
                return status;
            const int rawCount = curve.numCVs(&status);
            if (!status) return status;
            if (!dommy::generation::checkedCount(
                    rawCount, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
                return MS::kInvalidParameter;
            status = curve.getCVs(sourcePoints, MSpace::kObject);
            break;
        }
        case MFnData::kNurbsSurface: {
            MFnNurbsSurface surface(inputObjectHandle.asNurbsSurface(), &status);
            if (!status)
                return status;
            const int rawU = surface.numCVsInU(&status);
            if (!status) return status;
            const int rawV = surface.numCVsInV(&status);
            if (!status) return status;
            std::size_t countU = 0U, countV = 0U;
            if (!dommy::generation::checkedCount(
                    rawU, dommy::generation::kMaxGeneratedPointCount, countU) ||
                !dommy::generation::checkedCount(
                    rawV, dommy::generation::kMaxGeneratedPointCount, countV) ||
                !dommy::generation::checkedProduct(
                    countU, countV, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
                return MS::kInvalidParameter;
            status = surface.getCVs(sourcePoints, MSpace::kObject);
            break;
        }
        case MFnData::kMesh: {
            MFnMesh mesh(inputObjectHandle.asMesh(), &status);
            if (!status)
                return status;
            const int rawCount = mesh.numVertices(&status);
            if (!status) return status;
            if (!dommy::generation::checkedCount(
                    rawCount, dommy::generation::kMaxGeneratedPointCount, sourcePointCount))
                return MS::kInvalidParameter;
            status = mesh.getPoints(sourcePoints, MSpace::kObject);
            break;
        }
        default:
            return finishOutput(block, plug, outputPointArray, outputPoints);
        }
        if (!status)
            return status;
        if (sourcePoints.length() != sourcePointCount) {
            return MS::kFailure;
        }
        for (unsigned int index = 0U; index < sourcePoints.length(); ++index) {
            if (!dommy::maya::isFinitePoint(sourcePoints[index])) {
                return MS::kInvalidParameter;
            }
        }
    }

    dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedValue));
    randomSampleCount = std::min<std::size_t>(randomSampleCount, sourcePoints.length());
    const std::size_t trailCount =
        emitTypeValue == DP_ANIM_EMIT_MATRIX
            ? 1U
            : (emitTypeValue == DP_ANIM_EMIT_POINT ? sourcePoints.length() : randomSampleCount);
    std::size_t expectedPointCount = 0U;
    if (!dommy::generation::checkedProduct(trailCount, timeSampleCount,
                                           dommy::generation::kMaxGeneratedPointCount,
                                           expectedPointCount)) {
        return MS::kFailure;
    }
    if (trailCount == 0U) {
        return finishOutput(block, plug, outputPointArray, outputPoints);
    }

    dommy::maya::CheckedArray<MPointArray> trailPositions(static_cast<unsigned int>(trailCount));
    trailPositions.setLength(static_cast<unsigned int>(trailCount));
    if (emitTypeValue == DP_ANIM_EMIT_MATRIX) {
        trailPositions[0U] = MPoint(0.0, 0.0, 0.0);
    } else if (emitTypeValue == DP_ANIM_EMIT_POINT) {
        for (unsigned int index = 0U; index < trailPositions.length(); ++index) {
            trailPositions[index] = sourcePoints[index];
        }
    } else {
        std::vector<unsigned int> vertexIndices(sourcePoints.length());
        for (unsigned int index = 0U; index < sourcePoints.length(); ++index) {
            vertexIndices[index] = index;
        }
        for (std::size_t index = 0U; index < randomSampleCount; ++index) {
            const std::size_t selected =
                index + randomGenerator.index(vertexIndices.size() - index);
            std::swap(vertexIndices[index], vertexIndices[selected]);
            trailPositions[static_cast<unsigned int>(index)] = sourcePoints[vertexIndices[index]];
        }
    }

    const MPlug matrixPlug(thisMObject(), transformMatrix);
    if (matrixPlug.isNull()) {
        return finishOutput(block, plug, outputPointArray, outputPoints);
    }
    const bool matrixConnected = matrixPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!matrixConnected) {
        return finishOutput(block, plug, outputPointArray, outputPoints);
    }

    const unsigned int matrixCount = static_cast<unsigned int>(timeSampleCount);
    const bool randomizeTrailLength = timeLengthRandomValue != 0.0 && trailPositions.length() > 1U;
    double minimumTrailLength = timeLengthValue;
    double maximumTrailLength = timeLengthValue;
    if (randomizeTrailLength) {
        const double halfRandomLength = std::abs(timeLengthRandomValue) * 0.5;
        minimumTrailLength -= halfRandomLength;
        maximumTrailLength += halfRandomLength;
        // Validate derived bounds before output allocation or time evaluation.
        (void)dommy::random::prepareRealBounds(minimumTrailLength, maximumTrailLength, false);
    }
    std::vector<MMatrix> matrices;
    if (!randomizeTrailLength) {
        const double step = -timeLengthValue / static_cast<double>(timeSampleCount);
        status = getPlugMatrices(matrices, matrixPlug, matrixCount, step, timeOffsetValue);
        if (!status)
            return status;
    }

    outputPoints.setLength(static_cast<unsigned int>(expectedPointCount));
    unsigned int outputIndex = 0U;
    for (unsigned int trailIndex = 0U; trailIndex < trailPositions.length(); ++trailIndex) {
        if (randomizeTrailLength) {
            const double randomizedLength = randomGenerator.uniform(
                minimumTrailLength, maximumTrailLength);
            const double step = -randomizedLength / static_cast<double>(timeSampleCount);
            status = getPlugMatrices(matrices, matrixPlug, matrixCount, step, timeOffsetValue);
            if (!status)
                return status;
        }

        for (std::size_t sampleIndex = 0U; sampleIndex < matrices.size(); ++sampleIndex) {
            if (outputIndex >= outputPoints.length())
                return MS::kFailure;
            const MPoint point = trailPositions[trailIndex] * matrices[sampleIndex];
            if (!dommy::maya::isFinitePoint(point)) {
                return MS::kInvalidParameter;
            }
            outputPoints[outputIndex++] = point;
        }
    }

    if (outputIndex != expectedPointCount) {
        return MS::kFailure;
    }
    return finishOutput(block, plug, outputPointArray, outputPoints);
}
DOMMY_COMPUTE_CATCH("DPAnimationEmit")

MStatus DPAnimationEmit::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inputGeometry =
        typedAttribute.create("inputGeometry", "igo", MFnData::kAny, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setHidden(true));

    MFnMatrixAttribute matrixAttribute;
    transformMatrix =
        matrixAttribute.create("transformMatrix", "tm", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setCached(false));

    MFnEnumAttribute enumAttribute;
    emitType = enumAttribute.create("emitType", "emt", DP_ANIM_EMIT_MATRIX, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Transform Matrix", DP_ANIM_EMIT_MATRIX));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Mesh Point", DP_ANIM_EMIT_POINT));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Mesh Point Random", DP_ANIM_EMIT_RANDOM));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setStorable(true));

    seed = numericAttribute.create("seed", "se", MFnNumericData::kInt, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    randomCount = numericAttribute.create("randomCount", "rac", MFnNumericData::kInt, 5.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    timeOffset =
        numericAttribute.create("timeOffset", "tof", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    timeLength =
        numericAttribute.create("timeLength", "tle", MFnNumericData::kDouble, 30.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    timeSampling =
        numericAttribute.create("timeSampling", "tis", MFnNumericData::kInt, 30.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    timeLengthRandom =
        numericAttribute.create("timeLengthRandom", "tlra", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));

    MPointArray defaultPoints;
    MFnPointArrayData defaultPointData;
    const MObject defaultObject = defaultPointData.create(defaultPoints, &status);
    DOMMY_RETURN_IF_FAILURE(status);

    outputPointArray = typedAttribute.create("outputPointArray", "opa", MFnData::kPointArray,
                                             defaultObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(inputGeometry));
    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrix));
    DOMMY_RETURN_IF_FAILURE(addAttribute(emitType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));
    DOMMY_RETURN_IF_FAILURE(addAttribute(randomCount));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeOffset));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeLength));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeSampling));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeLengthRandom));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputPointArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputGeometry, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(transformMatrix, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(emitType, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seed, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(randomCount, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeOffset, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeLength, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeSampling, outputPointArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeLengthRandom, outputPointArray));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPAnimationEmit", "initialize",
                                                  std::current_exception());
}
