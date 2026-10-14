//
//  DPVectorArrayGen.cpp
//  dommyTool
//
//  Created by yuzo on 3/11/16.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPVectorArrayGen.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DTimeSampling.h"
#include "dpoint_ids.h"

#include <maya/MDGContextGuard.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstdint>
#include <vector>

const MTypeId DPVectorArrayGen::typeId(ID_DP_VECTOR_ARRAY);

MObject DPVectorArrayGen::generateType;
MObject DPVectorArrayGen::aValueType;
MObject DPVectorArrayGen::value;
MObject DPVectorArrayGen::valueSize;
MObject DPVectorArrayGen::isNormalize;
MObject DPVectorArrayGen::seed;
MObject DPVectorArrayGen::minRange;
MObject DPVectorArrayGen::maxRange;
MObject DPVectorArrayGen::aGlobalOffset;
MObject DPVectorArrayGen::aInOffsetArray;
MObject DPVectorArrayGen::aInStretchArray;
MObject DPVectorArrayGen::outDataArray;

namespace {

bool finiteVector(const MVector& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

MVector normalizedOrZero(const MVector& vector) {
    const double length = vector.length();
    if (!std::isfinite(length) || length <= 1.0e-12) {
        return MVector::zero;
    }
    return vector / length;
}

} // namespace

void* DPVectorArrayGen::creator() {
    return ::dommy::maya::createObject("DPVectorArrayGen", [&]() {
        return new DPVectorArrayGen();
    });
}

MStatus DPVectorArrayGen::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDataArray)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle generationHandle = data.inputValue(generateType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short generationType = generationHandle.asShort();
    const MDataHandle typeHandle = data.inputValue(aValueType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short valueType = typeHandle.asShort();
    const MDataHandle valueHandle = data.inputValue(value, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float3& rawBaseValue = valueHandle.asFloat3();
    const MDataHandle countHandle = data.inputValue(valueSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawValueCount = countHandle.asInt();
    const MDataHandle normalizeHandle = data.inputValue(isNormalize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool normalize = normalizeHandle.asBool();

    MVector baseValue(rawBaseValue[0], rawBaseValue[1], rawBaseValue[2]);
    std::size_t valueCount = 0U;
    if (generationType < 0 || generationType > 2 || valueType < 0 || valueType > 1 ||
        !finiteVector(baseValue) ||
        !dommy::generation::checkedCount(rawValueCount, dommy::generation::kMaxGeneratedPointCount,
                                         valueCount)) {
        return MS::kInvalidParameter;
    }
    if (normalize)
        baseValue = normalizedOrZero(baseValue);

    MVectorArray values;
    DOMMY_RETURN_IF_FAILURE(values.setLength(static_cast<unsigned int>(valueCount)));
    if (values.length() != valueCount)
        return MS::kFailure;
    for (unsigned int index = 0U; index < values.length(); ++index) {
        values[index] = baseValue;
    }
    if (generationType == 1) {
        const MDataHandle seedHandle = data.inputValue(seed, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int seedValue = seedHandle.asInt();
        const MDataHandle minimumHandle = data.inputValue(minRange, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float3& minimum = minimumHandle.asFloat3();
        const MDataHandle maximumHandle = data.inputValue(maxRange, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const float3& maximum = maximumHandle.asFloat3();
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis])) {
                return MS::kInvalidParameter;
            }
        }

        const std::uint32_t randomSeed = static_cast<std::uint32_t>(seedValue);
        std::vector<float> xValues;
        std::vector<float> yValues;
        std::vector<float> zValues;
        dommy::random::generateFloatValues(randomSeed, minimum[0], maximum[0], valueCount, xValues);
        dommy::random::generateFloatValues(randomSeed + 1U, minimum[1], maximum[1], valueCount,
                                           yValues);
        dommy::random::generateFloatValues(randomSeed + 2U, minimum[2], maximum[2], valueCount,
                                           zValues);

        for (std::size_t index = 0U; index < valueCount; ++index) {
            MVector generated(xValues[index], yValues[index], zValues[index]);
            if (!finiteVector(generated))
                return MS::kFailure;
            if (normalize)
                generated = normalizedOrZero(generated);
            values[static_cast<unsigned int>(index)] = generated;
        }
    } else if (generationType == 2 && valueCount > 0U) {
        const MPlug valuePlug(thisMObject(), value);
        bool childConnected = false;
        const unsigned int childCount = valuePlug.numChildren(&status);
        if (!status)
            return status;
        for (unsigned int child = 0U; child < childCount; ++child) {
            const MPlug childPlug = valuePlug.child(child, &status);
            if (!status)
                return status;
            const bool connected = childPlug.isConnected(&status);
            if (!status) return status;
            if (connected) {
                childConnected = true;
                break;
            }
        }

        const bool valueConnected = valuePlug.isConnected(&status);
        if (!status) return status;
        if (valueConnected || childConnected) {
            MDoubleArray offsets;
            MDoubleArray stretches;
            status = dommy::maya::readTimeOffsetAndStretch(data, thisMObject(), aInOffsetArray,
                                                           aInStretchArray, valueCount, offsets,
                                                           stretches);
            if (!status)
                return status;

            const MDataHandle offsetHandle = data.inputValue(aGlobalOffset, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const double globalOffset = offsetHandle.asDouble();
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
                MVector generated = sampledValue.asVector();
                if (!finiteVector(generated))
                    return MS::kInvalidParameter;
                if (normalize)
                    generated = normalizedOrZero(generated);
                values[static_cast<unsigned int>(index)] = generated;
            }
        }
    }

    if (valueType == 0) {
        MPointArray points;
        DOMMY_RETURN_IF_FAILURE(points.setLength(values.length()));
        if (points.length() != values.length())
            return MS::kFailure;
        for (unsigned int index = 0U; index < values.length(); ++index) {
            points[index] = MPoint(values[index]);
        }
        return DOutputDataMayaPointArray(data, plug, outDataArray, points);
    }
    return DOutputDataMayaVectorArray(data, plug, outDataArray, values);
}
DOMMY_COMPUTE_CATCH("DPVectorArrayGen")

MStatus DPVectorArrayGen::initialize() {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;

    MFnEnumAttribute enumAttribute;
    generateType = enumAttribute.create("generateType", "gt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Single Value", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Random", 1));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Animation", 2));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(generateType));

    MFnEnumAttribute valueTypeAttribute;
    aValueType = valueTypeAttribute.create("ValueType", "vt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(valueTypeAttribute.addField("Point", 0));
    DOMMY_RETURN_IF_FAILURE(valueTypeAttribute.addField("Vector", 1));
    DOMMY_RETURN_IF_FAILURE(valueTypeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(valueTypeAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueType));

    value = numericAttribute.create("value", "val", MFnNumericData::k3Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(value));

    valueSize = numericAttribute.create("valueSize", "vs", MFnNumericData::kInt, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(valueSize));

    isNormalize =
        numericAttribute.create("isNormalize", "inm", MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(isNormalize));

    seed = numericAttribute.create("seed", "s", MFnNumericData::kInt, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));

    minRange = numericAttribute.create("minRange", "mnr", MFnNumericData::k3Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(minRange));

    maxRange = numericAttribute.create("maxRange", "mxr", MFnNumericData::k3Float, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(maxRange));

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

    outDataArray = genericAttribute.create("outDataArray", "out", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    const MObject inputs[] = {generateType,  aValueType,     value,          valueSize,
                              isNormalize,   seed,           minRange,       maxRange,
                              aGlobalOffset, aInOffsetArray, aInStretchArray};
    for (std::size_t index = 0U; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(inputs[index], outDataArray));
    }
    return MS::kSuccess;
}
