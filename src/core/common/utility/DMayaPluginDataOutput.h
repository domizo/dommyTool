#pragma once

#include "DMayaPluginData.h"
#include "DStatusOperation.h"

#include <maya/MDataBlock.h>

namespace dommy {
namespace maya {

// Creates an empty Maya-owned plug-in data object and returns a non-owning
// mutable view for staged construction. Keep dataObject alive until outputData
// is no longer used, then publish dataObject rather than the raw pointer.
template <typename DataType>
MStatus createPluginDataObject(const MTypeId& dataTypeId,
                               MObject& dataObject,
                               DataType*& outputData)
{
    outputData = NULL;
    MStatus status;
    MFnPluginData pluginData;
    const MObject candidate = pluginData.create(dataTypeId, &status);
    if (!status) return status;
    if (candidate.isNull()) return MS::kFailure;

    status = getPluginData(pluginData, outputData);
    if (!status) return status;
    dataObject = candidate;
    return MS::kSuccess;
}

// Creates and populates a Maya-owned plug-in data object without publishing
// it. Callers that need to update more than one output can prepare every object
// first and then commit the complete set.
template <typename DataType, typename Populate>
MStatus createPluginDataObject(const MTypeId& dataTypeId,
                               Populate populate,
                               MObject& dataObject)
{
    MStatus status;
    MFnPluginData pluginData;
    const MObject candidate = pluginData.create(dataTypeId, &status);
    if (!status) return status;
    if (candidate.isNull()) return MS::kFailure;

    DataType* outputData = nullptr;
    status = getPluginData(pluginData, outputData);
    if (!status) return status;

    try {
        status = invokeStatusOperation(populate, *outputData);
        if (status != MS::kSuccess) return status;
        if (!validatePluginData(*outputData)) {
            return MS::kInvalidParameter;
        }
    } catch (...) {
        return MS::kFailure;
    }
    dataObject = candidate;
    return MS::kSuccess;
}

// Validates staged plug-in data immediately before publishing it to Maya.
// This is the commit path for callers that use the mutable-pointer overload of
// createPluginDataObject().
template <typename DataType>
MStatus publishPluginDataObject(MDataHandle& output,
                                const MObject& dataObject,
                                const DataType& data)
{
    if (dataObject.isNull() || !validatePluginData(data)) {
        return MS::kInvalidParameter;
    }
    MStatus status = output.set(dataObject);
    if (!status) return status;
    output.setClean();
    return MS::kSuccess;
}

// Publishes an already-created object when the construction code does not
// retain a typed pointer. The dynamic Maya data type and its final invariants
// are both checked before the output handle is modified.
template <typename DataType>
MStatus publishTypedPluginDataObject(MDataHandle& output,
                                     const MObject& dataObject,
                                     const MTypeId& expectedTypeId)
{
    const DataType* data = nullptr;
    const MStatus status = getPluginDataFromObject(
        dataObject, expectedTypeId, data);
    if (!status || !data) {
        return status ? MStatus(MS::kFailure) : status;
    }
    return publishPluginDataObject(output, dataObject, *data);
}

// Creates a Maya-owned plug-in data object, populates it while the function
// set and MObject are alive, and publishes it only after every API operation
// has succeeded.
template <typename DataType, typename Populate>
MStatus setPluginDataOutput(MDataBlock& block,
                            const MObject& outputAttribute,
                            const MTypeId& dataTypeId,
                            Populate populate)
{
    MStatus status;
    MObject dataObject;
    status = createPluginDataObject<DataType>(dataTypeId, populate, dataObject);
    if (!status) return status;

    MDataHandle output = block.outputValue(outputAttribute, &status);
    if (!status) {
        return status;
    }
    return publishTypedPluginDataObject<DataType>(
        output, dataObject, dataTypeId);
}

} // namespace maya
} // namespace dommy
