#pragma once

#include "DGenerationLimits.h"

#include <maya/MDataBlock.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFloatArray.h>
#include <maya/MPlug.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace maya {

namespace detail {

inline MStatus copyScalarInput(MDataBlock& data, const MObject& node, const MObject& attribute,
                               MDoubleArray& values) {
    const MPlug plug(node, attribute);
    if (plug.isNull())
        return MS::kSuccess;

    MStatus status;
    const bool connected = plug.isConnected(&status);
    if (!status) return status;
    if (!connected) return MS::kSuccess;
    MDataHandle handle = data.inputValue(attribute, &status);
    if (!status)
        return status;

    const MFnData::Type type = handle.type();
    if (type == MFnData::kNumeric) {
        const double value = handle.asDouble();
        if (!std::isfinite(value))
            return MS::kInvalidParameter;
        for (unsigned int index = 0U; index < values.length(); ++index) {
            values[index] = value;
        }
        return MS::kSuccess;
    }

    if (type == MFnData::kFloatArray) {
        MFnFloatArrayData arrayData(handle.data(), &status);
        if (!status)
            return status;
        const MFloatArray source = arrayData.array(&status);
        if (!status)
            return status;
        const unsigned int copyCount = std::min(values.length(), source.length());
        for (unsigned int index = 0U; index < copyCount; ++index) {
            if (!std::isfinite(source[index]))
                return MS::kInvalidParameter;
            values[index] = source[index];
        }
        return MS::kSuccess;
    }

    if (type == MFnData::kDoubleArray) {
        MFnDoubleArrayData arrayData(handle.data(), &status);
        if (!status)
            return status;
        const MDoubleArray source = arrayData.array(&status);
        if (!status)
            return status;
        const unsigned int copyCount = std::min(values.length(), source.length());
        for (unsigned int index = 0U; index < copyCount; ++index) {
            if (!std::isfinite(source[index]))
                return MS::kInvalidParameter;
            values[index] = source[index];
        }
        return MS::kSuccess;
    }

    return MS::kInvalidParameter;
}

} // namespace detail

inline MStatus readTimeOffsetAndStretch(MDataBlock& data, const MObject& node,
                                        const MObject& offsetAttribute,
                                        const MObject& stretchAttribute, std::size_t count,
                                        MDoubleArray& offsets, MDoubleArray& stretches) {
    if (count > generation::kMaxGeneratedPointCount ||
        count > std::numeric_limits<unsigned int>::max()) {
        return MS::kInvalidParameter;
    }

    // These caller-owned scratch arrays must be discarded on failure.
    // Check both allocation status and length before any indexed write.
    MStatus status = offsets.setLength(static_cast<unsigned int>(count));
    if (!status)
        return status;
    if (offsets.length() != count)
        return MS::kFailure;
    status = stretches.setLength(static_cast<unsigned int>(count));
    if (!status)
        return status;
    if (stretches.length() != count)
        return MS::kFailure;
    for (unsigned int index = 0U; index < count; ++index) {
        offsets[index] = 0.0;
        stretches[index] = 1.0;
    }

    status = detail::copyScalarInput(data, node, offsetAttribute, offsets);
    if (!status)
        return status;
    return detail::copyScalarInput(data, node, stretchAttribute, stretches);
}

inline bool computeSampleTime(double currentTime, double offset, double stretch,
                              double globalOffset, double& sampleTime) {
    if (!std::isfinite(currentTime) || !std::isfinite(offset) || !std::isfinite(stretch) ||
        !std::isfinite(globalOffset)) {
        return false;
    }

    sampleTime = currentTime;
    if (stretch != 0.0)
        sampleTime /= stretch;
    sampleTime -= offset;
    sampleTime += globalOffset;
    return std::isfinite(sampleTime);
}

} // namespace maya
} // namespace dommy
