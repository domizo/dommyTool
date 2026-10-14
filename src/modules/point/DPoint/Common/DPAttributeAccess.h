#pragma once

#include "DPData.h"
#include "DType.h"
#include "common/utility/DMayaAttributeArray.h"

#include <maya/MDoubleArray.h>
#include <maya/MIntArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MStringArray.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

// Narrow access declarations for callers that do not need the legacy
// DPAttributeCommon template and DCurve dependency graph.
void getDScalarArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    DScalarDataList& newArray);

// Checked accessors for evaluation code. Validation and staging precede
// publication. A Maya copy failure is reported, but Maya does not promise
// rollback of a failed copy into the destination.
namespace dommy {
namespace point {
namespace detail {

template <typename ValueType>
inline bool copyCheckedScalarAttribute(const DPData& inputData,
                                       const std::string& name,
                                       std::size_t expectedCount,
                                       DScalarDataList& output)
{
    return attribute::convertTypedArray<ValueType>(
        inputData.getAttr(name), expectedCount, output);
}

template <typename ValueType>
inline bool copyCheckedVectorAttribute(const DPData& inputData,
                                       const std::string& name,
                                       std::size_t expectedCount,
                                       MVectorArray& output)
{
    return attribute::copyTypedMayaArray<ValueType, Imath::V3d>(
        inputData.getAttr(name), expectedCount,
        [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
        output) == MS::kSuccess;
}

template <typename ValueType>
inline bool copyCheckedDoubleAttribute(const DPData& inputData,
                                       const std::string& name,
                                       std::size_t expectedCount,
                                       MDoubleArray& output)
{
    return attribute::copyTypedMayaArray<ValueType, double>(
        inputData.getAttr(name), expectedCount, output) == MS::kSuccess;
}

template <typename ValueType>
inline bool copyCheckedIntAttribute(const DPData& inputData,
                                    const std::string& name,
                                    std::size_t expectedCount,
                                    MIntArray& output)
{
    return attribute::copyTypedMayaArray<ValueType, int>(
        inputData.getAttr(name), expectedCount, output) == MS::kSuccess;
}

inline bool copyCheckedStringAttribute(const DPData& inputData,
                                       const std::string& name,
                                       std::size_t expectedCount,
                                       MStringArray& output)
{
    return attribute::copyStringMayaArray(
        inputData.getAttr(name), expectedCount, output) == MS::kSuccess;
}

inline bool copyCheckedMatrixAttribute(const DPData& inputData,
                                       const std::string& name,
                                       std::size_t expectedCount,
                                       MMatrixArray& output)
{
    return attribute::copyTypedMayaArray<MMatrix, MMatrix>(
        inputData.getAttr(name), expectedCount, output) == MS::kSuccess;
}

} // namespace detail
} // namespace point
} // namespace dommy

inline bool getCheckedDScalarArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    DScalarDataList& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            return dommy::point::detail::copyCheckedScalarAttribute<int32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_UINT32:
            return dommy::point::detail::copyCheckedScalarAttribute<uint32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_INT64:
            return dommy::point::detail::copyCheckedScalarAttribute<int64_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_FLOAT:
            return dommy::point::detail::copyCheckedScalarAttribute<float>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_DOUBLE:
            return dommy::point::detail::copyCheckedScalarAttribute<double>(
                inputData, name, expectedCount, newArray);
        default:
            return false;
    }
}

inline bool getCheckedMVectorArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MVectorArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_VEC3I:
            return dommy::point::detail::copyCheckedVectorAttribute<Imath::V3i>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_VEC3F:
            return dommy::point::detail::copyCheckedVectorAttribute<Imath::V3f>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_VEC3D:
            return dommy::point::detail::copyCheckedVectorAttribute<Imath::V3d>(
                inputData, name, expectedCount, newArray);
        default:
            return false;
    }
}

inline bool getCheckedMDoubleArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MDoubleArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_FLOAT:
            return dommy::point::detail::copyCheckedDoubleAttribute<float>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_DOUBLE:
            return dommy::point::detail::copyCheckedDoubleAttribute<double>(
                inputData, name, expectedCount, newArray);
        default:
            return false;
    }
}

// The legacy real-valued controls accept integer fields as well as Float and
// Double. Keep the narrower MDouble accessor's policy unchanged for its other
// callers while admitting the same five numeric kinds for these controls.
inline bool getCheckedMRealArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MDoubleArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_INT:
            return dommy::point::detail::copyCheckedDoubleAttribute<int32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_UINT32:
            return dommy::point::detail::copyCheckedDoubleAttribute<uint32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_INT64:
            return dommy::point::detail::copyCheckedDoubleAttribute<int64_t>(
                inputData, name, expectedCount, newArray);
        default:
            return getCheckedMDoubleArrayFromAttribute(
                inputData, name, expectedCount, newArray);
    }
}

inline bool getCheckedMIntArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MIntArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_BOOL:
            return dommy::point::detail::copyCheckedIntAttribute<bool>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_INT:
            return dommy::point::detail::copyCheckedIntAttribute<int32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_UINT32:
            return dommy::point::detail::copyCheckedIntAttribute<uint32_t>(
                inputData, name, expectedCount, newArray);
        case DP_ATTR_INT64:
            return dommy::point::detail::copyCheckedIntAttribute<int64_t>(
                inputData, name, expectedCount, newArray);
        default:
            return false;
    }
}

inline bool getCheckedMStringArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MStringArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount ||
        attribute->getAttrType() != DP_ATTR_STRING) {
        return false;
    }
    return dommy::point::detail::copyCheckedStringAttribute(
        inputData, name, expectedCount, newArray);
}

inline bool getCheckedMMatrixArrayFromAttribute(
    const DPData& inputData,
    const std::string& name,
    std::size_t expectedCount,
    MMatrixArray& newArray)
{
    const DPAttributeData::ConstPtr attribute = inputData.getAttr(name);
    if (!attribute || attribute->attrSize() != expectedCount ||
        attribute->getAttrType() != DP_ATTR_MATRIX) {
        return false;
    }
    return dommy::point::detail::copyCheckedMatrixAttribute(
        inputData, name, expectedCount, newArray);
}
