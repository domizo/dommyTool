#pragma once

#include "common/utility/DMayaPointBlend.h"
#include "common/utility/DGenerationLimits.h"

#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MObject.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace curve {

const std::size_t kMinimumCubicCurveSampleCount = 4U;

inline bool checkedCurveSampleCount(int rawCount, std::size_t& sampleCount) {
    return generation::checkedCount(rawCount, generation::kMaxCurveSampleCount, sampleCount) &&
           sampleCount >= kMinimumCubicCurveSampleCount &&
           sampleCount <= static_cast<std::size_t>(std::numeric_limits<unsigned int>::max());
}

inline bool isFiniteCurvePoint(const MPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
           std::isfinite(point.w);
}

inline bool checkedCurvePointBlend(const MPoint& base, const MPoint& target, double blend,
                                   MPoint& result) {
    return maya::checkedPointBlend(base, target, blend, result);
}

inline MStatus sampleCurveByLength(const MFnNurbsCurve& curve, std::size_t sampleCount,
                                   MPointArray& points) {
    if (sampleCount < kMinimumCubicCurveSampleCount ||
        sampleCount > generation::kMaxCurveSampleCount ||
        sampleCount > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    const double curveLength = curve.length(kMFnNurbsEpsilon, &status);
    if (status != MS::kSuccess || !std::isfinite(curveLength) || curveLength < 0.0) {
        return MS::kFailure;
    }

    status = points.setLength(static_cast<unsigned int>(sampleCount));
    if (status != MS::kSuccess) {
        return status;
    }

    const double denominator = static_cast<double>(sampleCount - 1U);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        const double fraction = static_cast<double>(index) / denominator;
        const double sampleLength = curveLength * fraction;
        if (!std::isfinite(sampleLength)) {
            return MS::kFailure;
        }

        const double parameter = curve.findParamFromLength(sampleLength, &status);
        if (status != MS::kSuccess || !std::isfinite(parameter)) {
            return MS::kFailure;
        }

        MPoint point;
        status = curve.getPointAtParam(parameter, point, MSpace::kObject);
        if (status != MS::kSuccess || !isFiniteCurvePoint(point)) {
            return MS::kFailure;
        }
        points[static_cast<unsigned int>(index)] = point;
    }

    return MS::kSuccess;
}

inline MStatus rebuildCurveWithPoints(const MObject& topologySource, const MPointArray& points,
                                      MObject& outputCurve) {
    const std::size_t sampleCount = static_cast<std::size_t>(points.length());
    if (sampleCount < kMinimumCubicCurveSampleCount ||
        sampleCount > generation::kMaxCurveSampleCount) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnNurbsCurve topologyCurve(topologySource, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MFnNurbsCurveData curveData;
    MObject curveDataObject = curveData.create(&status);
    if (status != MS::kSuccess || curveDataObject.isNull()) {
        return MS::kFailure;
    }

    MFnNurbsCurve workingCurve;
    MObject copiedCurve = workingCurve.copy(topologySource, curveDataObject, &status);
    if (status != MS::kSuccess || copiedCurve.isNull()) {
        return MS::kFailure;
    }

    const unsigned int spans = static_cast<unsigned int>(sampleCount - 3U);
    MObject rebuiltCurve =
        workingCurve.rebuild(spans, 3U, 1U, 0U, true, true, false, 0.01f, &status);
    if (status != MS::kSuccess || rebuiltCurve.isNull()) {
        return MS::kFailure;
    }

    MFnNurbsCurve outputCurveFunction(rebuiltCurve, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    const int outputCvCount = outputCurveFunction.numCVs(&status);
    if (status != MS::kSuccess || outputCvCount < 0 ||
        static_cast<std::size_t>(outputCvCount) != sampleCount) {
        return MS::kFailure;
    }

    status = outputCurveFunction.setCVs(points, MSpace::kObject);
    if (status != MS::kSuccess) {
        return status;
    }
    status = outputCurveFunction.updateCurve();
    if (status != MS::kSuccess) {
        return status;
    }

    outputCurve = rebuiltCurve;
    return MS::kSuccess;
}

inline MStatus buildLengthResampledCurve(const MObject& sourceCurve, std::size_t sampleCount,
                                         MObject& outputCurve) {
    MStatus status;
    MFnNurbsCurve sourceCurveFunction(sourceCurve, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MPointArray points;
    status = sampleCurveByLength(sourceCurveFunction, sampleCount, points);
    if (status != MS::kSuccess) {
        return status;
    }
    return rebuildCurveWithPoints(sourceCurve, points, outputCurve);
}

inline MStatus buildLengthBlendedCurve(const MObject& baseCurve, const MObject& targetCurve,
                                       double blend, std::size_t sampleCount,
                                       MObject& outputCurve) {
    if (!std::isfinite(blend)) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnNurbsCurve baseCurveFunction(baseCurve, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    MFnNurbsCurve targetCurveFunction(targetCurve, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    MPointArray basePoints;
    status = sampleCurveByLength(baseCurveFunction, sampleCount, basePoints);
    if (status != MS::kSuccess) {
        return status;
    }
    MPointArray targetPoints;
    status = sampleCurveByLength(targetCurveFunction, sampleCount, targetPoints);
    if (status != MS::kSuccess || targetPoints.length() != basePoints.length()) {
        return MS::kFailure;
    }

    MPointArray blendedPoints;
    status = blendedPoints.setLength(basePoints.length());
    if (status != MS::kSuccess) {
        return status;
    }
    for (unsigned int index = 0; index < basePoints.length(); ++index) {
        MPoint point;
        if (!checkedCurvePointBlend(basePoints[index], targetPoints[index], blend, point)) {
            return MS::kFailure;
        }
        blendedPoints[index] = point;
    }

    return rebuildCurveWithPoints(baseCurve, blendedPoints, outputCurve);
}

} // namespace curve
} // namespace dommy
