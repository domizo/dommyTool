#pragma once

#include <maya/MDataHandle.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>
#include "DGenerationLimits.h"
#include "DNumericValidation.h"
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace dommy {
namespace maya {

const std::size_t kInferInputCount = std::numeric_limits<std::size_t>::max();

// Typed double controls must check acquisition before accessing the handle.
// Keep the result unchanged on either an API failure or non-finite input.
template <typename DataBlock, typename Attribute>
MStatus readFiniteDoubleInput(DataBlock& block, const Attribute& attribute, double& result) {
    MStatus status;
    const auto handle = block.inputValue(attribute, &status);
    if (status != MS::kSuccess)
        return status;
    const double candidate = handle.asDouble();
    if (!std::isfinite(candidate))
        return MS::kInvalidParameter;
    result = candidate;
    return MS::kSuccess;
}

inline bool isScalarNumericInput(const MDataHandle& handle)
{
    bool numeric = false, null = false;
    if (handle.isGeneric(numeric, null) && numeric && !null) return true;
    return handle.isNumeric() || handle.type() == MFnData::kNumeric ||
        handle.type() == MFnData::kFloatArray ||
        handle.type() == MFnData::kDoubleArray || handle.type() == MFnData::kIntArray;
}

inline MStatus readNumericScalar(const MDataHandle& handle, long double& value)
{
    bool numeric = false, null = false;
    if (handle.isGeneric(numeric, null)) {
        if (!numeric || null) return MS::kInvalidParameter;
        // Singleton generic attributes require asGeneric* accessors. Maya
        // does not expose their underlying singleton numeric subtype.
        value = handle.asGenericDouble();
    } else {
        switch (handle.numericType()) {
        case MFnNumericData::kBoolean: value = handle.asBool() ? 1.0L : 0.0L; break;
        case MFnNumericData::kByte: value = handle.asUChar(); break;
        case MFnNumericData::kChar: value = handle.asChar(); break;
        case MFnNumericData::kShort: value = handle.asShort(); break;
        case MFnNumericData::kInt: value = handle.asInt(); break;
        case MFnNumericData::kInt64: value = handle.asInt64(); break;
        case MFnNumericData::kFloat: value = handle.asFloat(); break;
        case MFnNumericData::kDouble: value = handle.asDouble(); break;
        default: return MS::kInvalidParameter;
        }
    }
    return std::isfinite(value) ? MS::kSuccess : MS::kInvalidParameter;
}

// Keep the function set alive while its potentially borrowed array is used.
// The visitor runs only after checked count/budget acquisition and cardinality
// validation. Visitors that publish output must stage their own result.
template <typename FunctionSet, typename Visitor>
MStatus visitBoundedNumericArray(MDataHandle handle, std::size_t components,
                                Visitor visitor)
{
    MStatus status;
    FunctionSet data(handle.data(), &status);
    if (!status) return status;
    const unsigned int count = data.length(&status);
    if (!status) return status;
    std::size_t valueCount = 0U;
    if (components == 0U || !generation::checkedProduct(
            count, components, generation::kMaxGeneratedPointCount, valueCount))
        return MS::kInvalidParameter;
    const auto array = data.array(&status);
    if (!status) return status;
    if (array.length() != count) return MS::kFailure;
    return visitor(array, static_cast<std::size_t>(count));
}

template <typename FunctionSet, typename T>
MStatus readScalarArray(MDataHandle handle, std::size_t expectedCount,
                         std::vector<T>& values)
{
    typedef decltype(std::declval<FunctionSet&>().array(nullptr)) Array;
    return visitBoundedNumericArray<FunctionSet>(handle, 1U,
        [&](const Array& array, std::size_t count) -> MStatus {
            const std::size_t expected = expectedCount == kInferInputCount ? count : expectedCount;
            return numeric::checkedScalarArrayCopy(array, count, expected, values)
                ? MS::kSuccess : MS::kInvalidParameter;
        });
}

template <typename T>
MStatus readScalarInput(MDataHandle handle, std::size_t expectedCount,
                        std::vector<T>& values, bool compactScalar = false)
{
    switch (handle.type()) {
    case MFnData::kFloatArray:
        return readScalarArray<MFnFloatArrayData>(handle, expectedCount, values);
    case MFnData::kDoubleArray:
        return readScalarArray<MFnDoubleArrayData>(handle, expectedCount, values);
    case MFnData::kIntArray:
        return readScalarArray<MFnIntArrayData>(handle, expectedCount, values);
    default: {
        long double raw = 0.0L;
        const MStatus status = readNumericScalar(handle, raw);
        if (!status) return status;
        T value;
        if (!numeric::checkedNumericCast(raw, value))
            return MS::kInvalidParameter;
        const std::size_t count = compactScalar || expectedCount == kInferInputCount
            ? 1U : expectedCount;
        if (count > generation::kMaxGeneratedPointCount)
            return MS::kInvalidParameter;
        std::vector<T> candidate(count, value);
        values.swap(candidate);
        return MS::kSuccess;
    }
    }
}

template <typename FunctionSet, typename Vector>
MStatus readVectorArray(MDataHandle handle, std::size_t expectedCount,
                         std::vector<Vector>& values)
{
    typedef decltype(std::declval<FunctionSet&>().array(nullptr)) Array;
    // The shared vector reader retains its historical entry-count budget;
    // float-flattening callers use three output components instead.
    return visitBoundedNumericArray<FunctionSet>(handle, 1U,
        [&](const Array& array, std::size_t count) -> MStatus {
            const std::size_t expected = expectedCount == kInferInputCount ? count : expectedCount;
            return numeric::checkedVectorArrayCopy(array, count, expected, values)
                ? MS::kSuccess : MS::kInvalidParameter;
        });
}

template <typename Vector>
MStatus readVectorInput(MDataHandle handle, std::size_t expectedCount,
                        std::vector<Vector>& values)
{
    switch (handle.type()) {
    case MFnData::kVectorArray:
        return readVectorArray<MFnVectorArrayData>(handle, expectedCount, values);
    case MFnData::kPointArray:
        return readVectorArray<MFnPointArrayData>(handle, expectedCount, values);
    default:
        return MS::kInvalidParameter;
    }
}

} // namespace maya
} // namespace dommy
