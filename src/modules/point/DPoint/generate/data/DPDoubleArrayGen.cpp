//
//  DPDoubleArrayGen.cpp
//  dommyTool
//
//  Created by yuzo on 3/5/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDoubleArrayGen.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DTimeSampling.h"
#include "math/DLinearInterpolation.h"
#include "dpoint_ids.h"

#include <maya/MDGContextGuard.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MRampAttribute.h>

#include <algorithm>
#include <cmath>
#include <vector>

const MTypeId DPDoubleArrayGen::typeId(ID_DP_DOUBLE_ARRAY);

MObject DPDoubleArrayGen::generateType;
MObject DPDoubleArrayGen::value;
MObject DPDoubleArrayGen::valueSize;
MObject DPDoubleArrayGen::aSeed;
MObject DPDoubleArrayGen::minRange;
MObject DPDoubleArrayGen::maxRange;
MObject DPDoubleArrayGen::aShiftRange;
MObject DPDoubleArrayGen::aValueRamp;
MObject DPDoubleArrayGen::aRampRange;
MObject DPDoubleArrayGen::aGlobalOffset;
MObject DPDoubleArrayGen::aInOffsetArray;
MObject DPDoubleArrayGen::aInStretchArray;
MObject DPDoubleArrayGen::outDataArray;

void* DPDoubleArrayGen::creator() {
    return ::dommy::maya::createObject("DPDoubleArrayGen", [&]() {
        return new DPDoubleArrayGen();
    });
}

MStatus DPDoubleArrayGen::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDataArray)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle generationTypeHandle = data.inputValue(generateType, &status);
    if (!status)
        return status;
    const short generationType = generationTypeHandle.asShort();
    const MDataHandle baseValueHandle = data.inputValue(value, &status);
    if (!status)
        return status;
    const float baseValue = baseValueHandle.asFloat();
    const MDataHandle rawValueCountHandle = data.inputValue(valueSize, &status);
    if (!status)
        return status;
    const int rawValueCount = rawValueCountHandle.asInt();

    std::size_t valueCount = 0U;
    if (generationType < 0 || generationType > 4 || !std::isfinite(baseValue) ||
        !dommy::generation::checkedCount(rawValueCount, dommy::generation::kMaxGeneratedPointCount,
                                         valueCount)) {
        return MS::kInvalidParameter;
    }

    MFloatArray values;
    switch (generationType) {
    case 0:
        status = values.setLength(static_cast<unsigned int>(valueCount));
        if (!status)
            return status;
        if (values.length() != valueCount)
            return MS::kFailure;
        for (unsigned int index = 0U; index < values.length(); ++index) {
            values[index] = baseValue;
        }
        break;

    case 1: {
        const MDataHandle seedHandle = data.inputValue(aSeed, &status);
        if (!status)
            return status;
        const int seed = seedHandle.asInt();
        const MDataHandle minimumHandle = data.inputValue(minRange, &status);
        if (!status)
            return status;
        const float minimum = minimumHandle.asFloat();
        const MDataHandle maximumHandle = data.inputValue(maxRange, &status);
        if (!status)
            return status;
        const float maximum = maximumHandle.asFloat();
        if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
            return MS::kInvalidParameter;
        }

        status =
            dommy::maya::generateRandomArray(seed, minimum, maximum, valueCount, values, false);
        if (!status)
            return status;
        break;
    }

    case 2: {
        const MDataHandle rangeHandle = data.inputValue(aShiftRange, &status);
        if (!status)
            return status;
        const float2& range = rangeHandle.asFloat2();
        if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
            return MS::kInvalidParameter;
        }
        const float minimum = std::min(range[0], range[1]);
        const float maximum = std::max(range[0], range[1]);

        status = values.setLength(static_cast<unsigned int>(valueCount));
        if (!status)
            return status;
        if (values.length() != valueCount)
            return MS::kFailure;
        if (valueCount == 1U) {
            values[0] = minimum;
        } else if (valueCount > 1U) {
            const double step =
                (static_cast<double>(maximum) - minimum) / static_cast<double>(valueCount - 1U);
            for (std::size_t index = 0U; index < valueCount; ++index) {
                float& output = values[static_cast<unsigned int>(index)];
                // Keep both endpoints exact, including wide ranges whose span
                // loses the smaller endpoint in double arithmetic.
                if (index == 0U) {
                    output = minimum;
                    continue;
                }
                if (index + 1U == valueCount) {
                    output = maximum;
                    continue;
                }
                const double generated = minimum + step * index;
                // Preserve ordinary rounding. Repair only an inadmissible
                // intermediate with the shared checked interpolation.
                if (!dommy::numeric::checkedFloatCast(generated, output)) {
                    const double position = static_cast<double>(index) /
                        static_cast<double>(valueCount - 1U);
                    if (!dommy::math::checkedLinearInterpolation(
                            minimum, maximum, position, output))
                        return MS::kFailure;
                }
            }
        }
        break;
    }

    case 3: {
        const MDataHandle rangeHandle = data.inputValue(aRampRange, &status);
        if (!status)
            return status;
        const float2& range = rangeHandle.asFloat2();
        if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
            return MS::kInvalidParameter;
        }
        const double minimum = std::min(range[0], range[1]);
        const double maximum = std::max(range[0], range[1]);
        const double rangeSize = maximum - minimum;

        MRampAttribute ramp(MPlug(thisMObject(), aValueRamp), &status);
        if (!status)
            return status;
        status = values.setLength(static_cast<unsigned int>(valueCount));
        if (!status)
            return status;
        if (values.length() != valueCount)
            return MS::kFailure;
        for (std::size_t index = 0U; index < valueCount; ++index) {
            const float position =
                valueCount <= 1U ? 1.0f
                                 : static_cast<float>(index) / static_cast<float>(valueCount - 1U);
            float rampValue = 0.0f;
            ramp.getValueAtPosition(position, rampValue, &status);
            if (!status)
                return status;
            if (!std::isfinite(rampValue))
                return MS::kFailure;
            // Select by the sampled value, not the array index. Preserve the
            // endpoints without cancellation and keep ordinary extrapolation.
            const double generated =
                rampValue == 0.0f ? minimum :
                rampValue == 1.0f ? maximum : minimum + rangeSize * rampValue;
            if (!dommy::numeric::checkedFloatCast(generated,
                                                  values[static_cast<unsigned int>(index)])) {
                return MS::kFailure;
            }
        }
        break;
    }

    case 4: {
        status = values.setLength(static_cast<unsigned int>(valueCount));
        if (!status)
            return status;
        if (values.length() != valueCount)
            return MS::kFailure;
        for (unsigned int index = 0U; index < values.length(); ++index) {
            values[index] = baseValue;
        }
        const MPlug valuePlug(thisMObject(), value);
        const bool connected = valuePlug.isConnected(&status);
        if (!status) return status;
        if (!connected || valueCount == 0U)
            break;

        MDoubleArray offsets;
        MDoubleArray stretches;
        status = dommy::maya::readTimeOffsetAndStretch(
            data, thisMObject(), aInOffsetArray, aInStretchArray, valueCount, offsets, stretches);
        if (!status)
            return status;

        const MDataHandle globalOffsetHandle = data.inputValue(aGlobalOffset, &status);
        if (!status)
            return status;
        const double globalOffset = globalOffsetHandle.asDouble();
        if (!std::isfinite(globalOffset)) {
            return MS::kInvalidParameter;
        }
        MTime currentTime;
        status = dommy::maya::getEvaluationTime(data, currentTime);
        if (!status)
            return status;

        for (std::size_t index = 0U; index < valueCount; ++index) {
            double sampleTime = 0.0;
            if (!dommy::maya::computeSampleTime(currentTime.value(), offsets[index],
                                                stretches[index], globalOffset, sampleTime)) {
                return MS::kInvalidParameter;
            }
            const MDGContext context(MTime(sampleTime, MTime::uiUnit()));
            MDGContextGuard contextGuard(context);
            dommy::maya::ScopedPlugDataHandle sampledValueOwner;
            MDataHandle& sampledValue = sampledValueOwner.handle();
            status = sampledValueOwner.read(valuePlug);
            if (!status)
                return status;
            const float sampled = sampledValue.asFloat();
            if (!std::isfinite(sampled))
                return MS::kInvalidParameter;
            values[static_cast<unsigned int>(index)] = sampled;
        }
        break;
    }
    }

    return DOutputDataMayaFloatArray(data, plug, outDataArray, values);
}
DOMMY_COMPUTE_CATCH("DPDoubleArrayGen")

MStatus DPDoubleArrayGen::initialize() {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;

    MFnEnumAttribute enumAttribute;
    generateType = enumAttribute.create("generateType", "gt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Single Value", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Random Range", 1));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("ID Shift", 2));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Ramp Map", 3));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Animation", 4));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(generateType));

    value = numericAttribute.create("value", "val", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(value));

    valueSize = numericAttribute.create("valueSize", "vs", MFnNumericData::kInt, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(valueSize));

    aSeed = numericAttribute.create("Seed", "s", MFnNumericData::kInt, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    minRange = numericAttribute.create("minRange", "mnr", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(minRange));

    maxRange = numericAttribute.create("maxRange", "mxr", MFnNumericData::kFloat, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maxRange));

    aShiftRange =
        numericAttribute.create("ShiftRange", "shr", MFnNumericData::k2Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShiftRange));

    aValueRamp = MRampAttribute::createCurveRamp("ValueRamp", "vramp", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRamp));

    aRampRange = numericAttribute.create("RampRange", "rar", MFnNumericData::k2Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRampRange));

    aGlobalOffset =
        numericAttribute.create("GlobalOffset", "gof", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalOffset));

    aInOffsetArray = genericAttribute.create("InOffsetArray", "inoff", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInOffsetArray));

    aInStretchArray = genericAttribute.create("InStretchArray", "instr", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInStretchArray));

    outDataArray = typedAttribute.create("outDataArray", "out", MFnData::kFloatArray,
                                         MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setCached(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    const MObject inputs[] = {generateType, value,         valueSize,      aSeed,
                              minRange,     maxRange,      aShiftRange,    aValueRamp,
                              aRampRange,   aGlobalOffset, aInOffsetArray, aInStretchArray};
    for (std::size_t index = 0U; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(inputs[index], outDataArray));
    }
    return MS::kSuccess;
}
