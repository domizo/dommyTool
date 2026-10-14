#include "common/utility/DMayaEntryPointGuard.h"
#include "DMotionDeformer.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DMayaDeformerGeometry.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaHistoricalSampling.h"
#include "common/utility/DTimeSamplingBudget.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MFloatArray.h>
#include <maya/MFnData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItGeometry.h>
#include <maya/MPlug.h>
#include <maya/MRampAttribute.h>
#include <maya/MTime.h>

#include <algorithm>
#include <cmath>
#include <vector>

MTypeId DMotionDeformer::typeId(0x0090A);
MString DMotionDeformer::typeName("DMotionDeformer");

MObject DMotionDeformer::timeRange;
MObject DMotionDeformer::sampling;
MObject DMotionDeformer::amplitude;
MObject DMotionDeformer::amplitudeRamp;

void* DMotionDeformer::creator() {
    return ::dommy::maya::createObject("DMotionDeformer", [&]() {
        return new DMotionDeformer();
    });
}

void DMotionDeformer::postConstructor() {
    dommy::maya::initializeNodeRamps("DMotionDeformer", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), amplitudeRamp, {0.0f, 1.0f}, {0.0f, 1.0f},
            {MRampAttribute::kLinear, MRampAttribute::kLinear}));
        return MS::kSuccess;
    });
}

MStatus DMotionDeformer::getPlugTimeValue(std::vector<MPointArray>& dataArray, long sampleCount,
                                          double timeRangeValue, const MTime& currentTime,
                                          unsigned int geomIndex,
                                          unsigned int expectedVertexCount) {
    if (!dommy::deformer::validSamplingRequest(sampleCount, timeRangeValue) ||
        !dommy::deformer::validSampleStorageRequest(sampleCount, expectedVertexCount,
                                                    dommy::deformer::kMaxMotionSampleCount,
                                                    dommy::deformer::kMaxMotionPointSamples) ||
        !dommy::sampling::validTimeHistoryStorage(expectedVertexCount, 1U, sampleCount)) {
        return MS::kFailure;
    }

    const double stepTime = timeRangeValue / static_cast<double>(sampleCount);
    MTime endpoint;
    if (!dommy::maya::historicalSampleTime(currentTime, 0.0, -stepTime, 0.0,
            static_cast<std::size_t>(sampleCount), endpoint)) return MS::kInvalidParameter;
    std::vector<MPointArray> candidate;
    candidate.reserve(static_cast<std::size_t>(sampleCount));
    dommy::sampling::TimeSamplingBudget budget;

    for (long sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
        MTime sampleTime;
        if (!dommy::maya::historicalSampleTime(currentTime, 0.0, -stepTime, 0.0,
                static_cast<std::size_t>(sampleIndex), sampleTime)) return MS::kInvalidParameter;
        if (!budget.readPoints(expectedVertexCount) || !budget.addPoints(expectedVertexCount))
            return MS::kInvalidParameter;

        // Evaluate the historical input through Maya's current DG context.
        // Do not mutate the MDataBlock supplied to deform(); it belongs to the
        // active evaluation and may be shared by Maya's evaluation machinery.
        const MDGContext sampleContext(sampleTime);
        const MDGContextGuard contextGuard(sampleContext);
        MDataBlock sampleData = forceCache();

        MStatus status;
        MArrayDataHandle inputHandle = sampleData.inputArrayValue(input, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        DOMMY_RETURN_IF_FAILURE(inputHandle.jumpToElement(geomIndex));
        MDataHandle geometryHandleElement = inputHandle.inputValue(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        MDataHandle geometryHandle = geometryHandleElement.child(inputGeom);
        if (geometryHandle.type() != MFnData::kMesh) {
            return MS::kFailure;
        }

        MFnMesh meshFn(geometryHandle.data(), &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int pointCount = meshFn.numVertices(&status);
        if (!status) return status;
        if (pointCount < 0 || static_cast<unsigned int>(pointCount) != expectedVertexCount)
            return MS::kFailure;
        MPointArray points;
        DOMMY_RETURN_IF_FAILURE(meshFn.getPoints(points, MSpace::kObject));
        if (points.length() != expectedVertexCount) return MS::kFailure;
        for (unsigned int index = 0U; index < points.length(); ++index)
            if (!dommy::maya::isFinitePoint(points[index])) return MS::kInvalidParameter;
        candidate.emplace_back();
        DOMMY_RETURN_IF_FAILURE(candidate.back().copy(points));
        if (candidate.back().length() != expectedVertexCount) return MS::kFailure;
    }

    dataArray.swap(candidate);
    return MS::kSuccess;
}

MStatus DMotionDeformer::deform(MDataBlock& data, MItGeometry& iter, const MMatrix& localToWorld,
                                unsigned int geomIndex) try {
    (void)localToWorld;
    MStatus status;

    MDataHandle envHandle = data.inputValue(envelope, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0f) {
        return MS::kSuccess;
    }

    MArrayDataHandle inputHandle = data.inputArrayValue(input, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(inputHandle.jumpToElement(geomIndex));
    MDataHandle geometryHandleElement = inputHandle.inputValue(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle geometryHandle = geometryHandleElement.child(inputGeom);
    if (geometryHandle.type() != MFnData::kMesh) {
        MGlobal::displayWarning("DMotionDeformer supports mesh input only.");
        return MS::kUnknownParameter;
    }

    MFnMesh meshFn(geometryHandle.data(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int vertexCount = meshFn.numVertices(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (vertexCount < 0) {
        return MS::kFailure;
    }

    MDataHandle timeRangeValueHandle = data.inputValue(timeRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double timeRangeValue = timeRangeValueHandle.asDouble();
    MDataHandle sampleCountHandle = data.inputValue(sampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const long sampleCount = sampleCountHandle.asInt();
    MDataHandle amplitudeValueHandle = data.inputValue(amplitude, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double amplitudeValue = amplitudeValueHandle.asDouble();
    if (!dommy::deformer::validSamplingRequest(sampleCount, timeRangeValue) ||
        !dommy::deformer::validSampleStorageRequest(
            sampleCount, static_cast<std::size_t>(vertexCount),
            dommy::deformer::kMaxMotionSampleCount, dommy::deformer::kMaxMotionPointSamples) ||
        !dommy::sampling::validTimeHistoryStorage(vertexCount, 1U, sampleCount) ||
        !std::isfinite(amplitudeValue)) {
        return MS::kFailure;
    }

    MRampAttribute amplitudeRampAttribute(MPlug(thisMObject(), amplitudeRamp), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MTime evaluationTime;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, evaluationTime));

    std::vector<MPointArray> sampledPoints;
    DOMMY_RETURN_IF_FAILURE(
        getPlugTimeValue(sampledPoints, sampleCount, timeRangeValue, evaluationTime, geomIndex,
                         static_cast<unsigned int>(vertexCount)));
    if (sampledPoints.empty()) {
        return MS::kSuccess;
    }
    for (std::size_t sampleIndex = 0; sampleIndex < sampledPoints.size(); ++sampleIndex) {
        if (sampledPoints[sampleIndex].length() != static_cast<unsigned int>(vertexCount)) {
            MGlobal::displayWarning(
                "DMotionDeformer skipped evaluation because sampled topology changed.");
            return MS::kFailure;
        }
    }

    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int pointIndex, const MPoint& point, MPoint& result) -> MStatus {
            if (pointIndex >= static_cast<unsigned int>(vertexCount)) {
                return MS::kFailure;
            }

            const float pointWeight = weightValue(data, geomIndex, pointIndex);
            if (!std::isfinite(pointWeight)) {
                return MS::kFailure;
            }
            if (pointWeight == 0.0f) {
                return MS::kSuccess;
            }

            const MVector movement = point - sampledPoints[0][pointIndex];
            MVector movementDirection(movement);
            if (!dommy::deformer::normalizeVector(movementDirection))
                return MS::kInvalidParameter;
            if (movementDirection.length() <= 1.0e-12)
                return MS::kSuccess;

            MVector normal = iter.normal(MSpace::kObject, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!dommy::deformer::normalizeVector(normal))
                return MS::kInvalidParameter;
            if (normal.length() <= 1.0e-12) {
                return MS::kSuccess;
            }
            const double dot = movementDirection * normal;
            if (!std::isfinite(dot) || dot > 0.0) {
                return MS::kSuccess;
            }

            float rampValue = 0.0f;
            amplitudeRampAttribute.getValueAtPosition(static_cast<float>(std::fabs(dot)), rampValue,
                                                      &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(rampValue)) {
                return MS::kFailure;
            }
            rampValue = std::max(0.0f, std::min(1.0f, rampValue));

            // State zero is the current point. Historical sample N is stored at
            // sampledPoints[N - 1], so the conceptual state count includes one
            // more entry than the sampled array.
            dommy::deformer::MotionSampleBlend blend;
            if (!dommy::deformer::motionSampleBlend(static_cast<double>(rampValue),
                                                    sampledPoints.size() + 1U, blend)) {
                return MS::kFailure;
            }

            const MPoint lowerPoint =
                blend.lowerIndex == 0U ? point : sampledPoints[blend.lowerIndex - 1U][pointIndex];
            const MPoint upperPoint =
                blend.upperIndex == 0U ? point : sampledPoints[blend.upperIndex - 1U][pointIndex];
            if (!dommy::maya::isFinitePoint(lowerPoint) ||
                !dommy::maya::isFinitePoint(upperPoint)) {
                return MS::kFailure;
            }

            MPoint sampledPoint;
            double strength = 0.0;
            if (!dommy::deformer::blendPoint(lowerPoint, upperPoint, blend.fraction,
                                             sampledPoint) ||
                !dommy::numeric::scaledProduct(
                    std::array<double, 3>{{amplitudeValue, double(pointWeight), double(env)}},
                    strength) ||
                !dommy::deformer::blendPoint(point, sampledPoint, strength, result))
                return MS::kInvalidParameter;
            return MS::kSuccess;
        });
}
DOMMY_DEFORMER_CATCH("DMotionDeformer")

MStatus DMotionDeformer::initialize() try
{
    MStatus stat;
    MFnNumericAttribute numericAttribute;

    timeRange = numericAttribute.create(
        "timeRange", "tmr", MFnNumericData::kDouble, 5.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeRange));

    sampling = numericAttribute.create(
        "sampling", "sa", MFnNumericData::kInt, 4, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    amplitude = numericAttribute.create(
        "amplitude", "amp", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(amplitude));

    amplitudeRamp = MRampAttribute::createCurveRamp("amplitudeRamp", "aramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(amplitudeRamp));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(timeRange, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(amplitude, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(amplitudeRamp, outputGeom));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMotionDeformer", "initialize", std::current_exception());
}
