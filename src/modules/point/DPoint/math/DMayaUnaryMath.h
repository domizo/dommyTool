#pragma once

#include "DUnaryMath.h"
#include "DMathArrayConversion.h"
#include "common/utility/DAttributeMathInput.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MPlug.h>

template <typename Owner, typename Values>
MStatus DReadUnaryMathAttribute(const Owner& owner, const std::string& name,
                                std::size_t count, Values& values)
{
    return dommy::math::validMathBudget(count, 0U) &&
        dommy::attribute::readNumericAttributeValues(owner, name, count, values)
        ? MS::kSuccess : MS::kInvalidParameter;
}

namespace dommy {
namespace maya {

template <typename FunctionSet, typename Values>
MStatus readUnaryMathArray(MDataHandle input, Values& values)
{
    MStatus status;
    FunctionSet data(input.data(), &status);
    if (!status) return status;
    const unsigned int count = data.length(&status);
    if (!status) return status;
    if (!math::validMathBudget(count, 0U)) return MS::kInvalidParameter;
    // Reuse the checked boundary copy; no Maya-owned array copy is necessary.
    return numeric::checkedScalarArrayCopy(data, count, count, values)
        ? MS::kSuccess : MS::kInvalidParameter;
}

// MVector/MPoint expose three coordinates but are not serialized Imath values.
template <typename FunctionSet>
MStatus readUnaryMathVectors(MDataHandle input, DMathVec3DataList& values,
                             std::size_t expectedCount = kInferInputCount,
                             std::size_t operands = 0U)
{
    MStatus status;
    FunctionSet data(input.data(), &status);
    if (!status) return status;
    const unsigned int count = data.length(&status);
    if (!status) return status;
    if ((expectedCount != kInferInputCount && count != expectedCount) ||
        !math::validMathBudget(count, operands)) return MS::kInvalidParameter;
    return numeric::checkedVectorArrayCopy(data, count, count, values)
        ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Operation>
MStatus mapMayaUnaryArray(MDataBlock& data, const MPlug& plug, const MObject& inputAttribute,
                          const MObject& outputAttribute, Operation operation,
                          bool vectorsOnly = false, bool vectorOutput = false)
{
    MStatus status;
    MDataHandle input = data.inputValue(inputAttribute, &status);
    if (!status) return status;
    const MFnData::Type type = input.type();
    if (type == MFnData::kVectorArray || type == MFnData::kPointArray) {
        DMathVec3DataList values;
        status = type == MFnData::kVectorArray
            ? readUnaryMathVectors<MFnVectorArrayData>(input, values)
            : readUnaryMathVectors<MFnPointArrayData>(input, values);
        if (!status) return status;
        if (!operation(values)) return MS::kInvalidParameter;
        const auto read = [&](std::size_t i) -> const DMathVec3Data& { return values[i]; };
        if (vectorOutput || type == MFnData::kVectorArray) {
            MVectorArray output;
            DOMMY_RETURN_IF_FAILURE((math::writeVectorMathArray<MVector>(values.size(), read, output)));
            return DOutputDataMayaVectorArray(data, plug, outputAttribute, output);
        }
        MPointArray output;
        DOMMY_RETURN_IF_FAILURE((math::writeVectorMathArray<MPoint>(values.size(), read, output)));
        return DOutputDataMayaPointArray(data, plug, outputAttribute, output);
    }
    if (vectorsOnly) return MS::kInvalidParameter;
    DMathScalarDataList values;
    switch (type) {
    case MFnData::kIntArray: status = readUnaryMathArray<MFnIntArrayData>(input, values); break;
    case MFnData::kFloatArray: status = readUnaryMathArray<MFnFloatArrayData>(input, values); break;
    case MFnData::kDoubleArray: status = readUnaryMathArray<MFnDoubleArrayData>(input, values); break;
    default: return MS::kInvalidParameter;
    }
    if (!status) return status;
    if (!operation(values)) return MS::kInvalidParameter;
    const auto read = [&](std::size_t i) { return values[i]; };
    switch (type) {
    case MFnData::kIntArray: {
        MIntArray output;
        DOMMY_RETURN_IF_FAILURE((math::writeScalarMathArray<int>(values.size(), read, output)));
        return DOutputDataMayaIntArray(data, plug, outputAttribute, output);
    }
    case MFnData::kFloatArray: {
        MFloatArray output;
        DOMMY_RETURN_IF_FAILURE((math::writeScalarMathArray<float>(values.size(), read, output)));
        return DOutputDataMayaFloatArray(data, plug, outputAttribute, output);
    }
    default: {
        MDoubleArray output;
        DOMMY_RETURN_IF_FAILURE((math::writeScalarMathArray<double>(values.size(), read, output)));
        return DOutputDataMayaDoubleArray(data, plug, outputAttribute, output);
    }
    }
}

} // namespace maya
} // namespace dommy
