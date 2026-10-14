#pragma once

#include "DMayaValueValidation.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MDataHandle.h>
#include <maya/MFnMatrixData.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace dommy {
namespace maya {

inline bool validSampleOffsets(const std::size_t count,
                               const std::vector<double>& randomOffsets)
{
    if (!randomOffsets.empty() && randomOffsets.size() != count) {
        return false;
    }
    for (std::size_t index = 0U; index < randomOffsets.size(); ++index) {
        if (!std::isfinite(randomOffsets[index])) return false;
    }
    return true;
}

inline bool historicalSampleTime(const MTime& origin,
                                 const double firstOffset,
                                 const double step,
                                 const double randomOffset,
                                 const std::size_t index,
                                 MTime& sampleTime)
{
    if (!std::isfinite(origin.value()) || !std::isfinite(firstOffset) ||
        !std::isfinite(step) || !std::isfinite(randomOffset)) {
        return false;
    }

    const double value =
        origin.value() + firstOffset +
        step * static_cast<double>(index) + randomOffset;
    if (!std::isfinite(value)) return false;
    sampleTime = MTime(value, origin.unit());
    return true;
}

inline MStatus sampleMatrixPlugAtTime(const MPlug& source,
                                      const MTime& sampleTime,
                                      MMatrix& matrix)
{
    if (source.isNull() || !std::isfinite(sampleTime.value())) {
        return MS::kInvalidParameter;
    }

    const MDGContext context(sampleTime);
    const MDGContextGuard contextGuard(context);
    MStatus status;
    MObject matrixObject;
    status = source.getValue(matrixObject);
    if (!status || matrixObject.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }

    MFnMatrixData matrixData(matrixObject, &status);
    if (!status) return status;
    const MMatrix candidate = matrixData.matrix(&status);
    if (!status || !isFiniteMatrix(candidate)) {
        return status ? MStatus(MS::kInvalidParameter) : status;
    }
    matrix = candidate;
    return MS::kSuccess;
}

inline MStatus sampleVectorPlugAtTime(const MPlug& source,
                                      const MTime& sampleTime,
                                      MVector& vector)
{
    if (source.isNull() || !std::isfinite(sampleTime.value())) {
        return MS::kInvalidParameter;
    }

    const MDGContext context(sampleTime);
    const MDGContextGuard contextGuard(context);
    dommy::maya::ScopedPlugDataHandle handleOwner;
    MDataHandle& handle = handleOwner.handle();
    const MStatus status = handleOwner.read(source);
    if (!status) return status;

    const MVector candidate = handle.asVector();
    if (!isFiniteVector(candidate)) return MS::kInvalidParameter;
    vector = candidate;
    return MS::kSuccess;
}

inline MStatus sampleScalarPlugSeries(
    std::vector<double>& values,
    const MPlug& source,
    const std::size_t count,
    const MTime& origin,
    const double firstOffset,
    const double step,
    const std::vector<double>& randomOffsets)
{
    if (source.isNull() ||
        !validSampleOffsets(count, randomOffsets)) {
        return MS::kFailure;
    }

    std::vector<double> sampled;
    sampled.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const double randomOffset =
            randomOffsets.empty() ? 0.0 : randomOffsets[index];
        MTime sampleTime;
        if (!historicalSampleTime(
                origin, firstOffset, step, randomOffset,
                index, sampleTime)) {
            return MS::kFailure;
        }

        const MDGContext context(sampleTime);
        const MDGContextGuard contextGuard(context);
        double value = 0.0;
        const MStatus status = source.getValue(value);
        if (!status || !std::isfinite(value)) {
            return status ? MStatus(MS::kFailure) : status;
        }
        sampled.push_back(value);
    }

    values.swap(sampled);
    return MS::kSuccess;
}

inline MStatus sampleMatrixPlugSeries(
    std::vector<MMatrix>& matrices,
    const MPlug& source,
    const std::size_t count,
    const MTime& origin,
    const double firstOffset,
    const double step,
    const std::vector<double>& randomOffsets)
{
    if (source.isNull() ||
        !validSampleOffsets(count, randomOffsets)) {
        return MS::kFailure;
    }

    std::vector<MMatrix> sampled;
    sampled.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const double randomOffset =
            randomOffsets.empty() ? 0.0 : randomOffsets[index];
        MTime sampleTime;
        if (!historicalSampleTime(
                origin, firstOffset, step, randomOffset,
                index, sampleTime)) {
            return MS::kFailure;
        }

        MMatrix matrix;
        const MStatus status =
            sampleMatrixPlugAtTime(source, sampleTime, matrix);
        if (!status) return status;
        sampled.push_back(matrix);
    }

    matrices.swap(sampled);
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
