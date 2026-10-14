#pragma once

#include <maya/MDataHandle.h>
#include <maya/MFnPluginData.h>
#include <maya/MObject.h>
#include <maya/MPxData.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>

namespace dommy {
namespace maya {

namespace detail {

template <int Priority>
struct PluginDataValidationPriority
    : PluginDataValidationPriority<Priority - 1> {};

template <>
struct PluginDataValidationPriority<0> {};

template <typename DataType>
auto validatePluginDataImpl(
        const DataType& data,
        PluginDataValidationPriority<4>)
    -> decltype(data.hasValidPointTopology(), bool()) {
    return data.hasValidPointTopology();
}

template <typename DataType>
auto validatePluginDataImpl(
        const DataType& data,
        PluginDataValidationPriority<3>)
    -> decltype(data.hasValidCurveTopology(), bool()) {
    return data.hasValidCurveTopology();
}

template <typename DataType>
auto validatePluginDataImpl(
        const DataType& data,
        PluginDataValidationPriority<2>)
    -> decltype(data.hasValidMeshTopology(), bool()) {
    return data.hasValidMeshTopology();
}

template <typename DataType>
auto validatePluginDataImpl(
        const DataType& data,
        PluginDataValidationPriority<1>)
    -> decltype(data.hasValidData(), bool()) {
    return data.hasValidData();
}

template <typename DataType>
bool validatePluginDataImpl(
        const DataType&,
        PluginDataValidationPriority<0>) {
    return true;
}

} // namespace detail

// Known dommy data types expose a topology or payload validator. Other Maya
// plug-in data types remain compatible and are accepted by the fallback.
template <typename DataType>
bool validatePluginData(const DataType& data) {
    return detail::validatePluginDataImpl(
        data, detail::PluginDataValidationPriority<4>());
}

// Returns a checked, non-owning view of Maya-owned plug-in data.
template <typename DataType>
MStatus getPluginData(MFnPluginData& pluginData, DataType*& outputData)
{
    outputData = NULL;
    MStatus status;
    MPxData* const rawData = pluginData.data(&status);
    if (!status) return status;

    outputData = dynamic_cast<DataType*>(rawData);
    return outputData ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

template <typename DataType>
MStatus getPluginData(const MFnPluginData& pluginData,
                      const DataType*& outputData)
{
    outputData = NULL;
    MStatus status;
    const MPxData* const rawData = pluginData.constData(&status);
    if (!status) return status;

    outputData = dynamic_cast<const DataType*>(rawData);
    return outputData ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

// Create a Maya-owned plug-in data object and verify its dynamic C++ type
// before returning a non-owning pointer to the caller.
template <typename DataType>
MStatus createPluginData(MFnPluginData& pluginData,
                         const MTypeId& typeId,
                         DataType*& outputData)
{
    outputData = NULL;
    MStatus status;
    const MObject object = pluginData.create(typeId, &status);
    if (!status || object.isNull()) {
        return status ? MStatus(MS::kFailure) : status;
    }
    return getPluginData(pluginData, outputData);
}

// Expression-compatible adapter for legacy output construction. It preserves
// the historical MFnPluginData lifetime while replacing unchecked static_cast
// calls with dynamic type validation.
template <typename DataType>
DataType* pluginDataPointer(MFnPluginData& pluginData, MStatus* outputStatus)
{
    DataType* outputData = NULL;
    const MStatus status = getPluginData(pluginData, outputData);
    if (outputStatus) *outputStatus = status;
    return status ? outputData : NULL;
}

// MDataHandle::asPluginData() does not report status. Validate both Maya's
// runtime type id and the C++ dynamic type before exposing the pointer.
template <typename DataType>
MStatus getPluginDataFromHandle(const MDataHandle& handle,
                                const MTypeId& expectedTypeId,
                                const DataType*& outputData)
{
    outputData = NULL;
    if (handle.typeId() != expectedTypeId) return MS::kFailure;

    outputData = dynamic_cast<const DataType*>(handle.asPluginData());
    return outputData ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
}

template <typename DataType>
MStatus getPluginDataFromObject(const MObject& object,
                                const MTypeId& expectedTypeId,
                                const DataType*& outputData)
{
    outputData = NULL;
    if (object.isNull()) return MS::kFailure;

    MStatus status;
    const MFnPluginData pluginData(object, &status);
    if (!status) return status;
    const MTypeId actualTypeId = pluginData.typeId(&status);
    if (!status) return status;
    if (actualTypeId != expectedTypeId) return MS::kFailure;
    return getPluginData(pluginData, outputData);
}

// Keep dataObject alive in the caller for as long as outputData is used.
template <typename DataType>
MStatus getPluginDataFromPlug(const MPlug& plug,
                              const MTypeId& expectedTypeId,
                              MObject& dataObject,
                              const DataType*& outputData)
{
    dataObject = MObject::kNullObj;
    outputData = NULL;

    MStatus status = plug.getValue(dataObject);
    if (!status) return status;
    return getPluginDataFromObject(dataObject, expectedTypeId, outputData);
}

} // namespace maya
} // namespace dommy
