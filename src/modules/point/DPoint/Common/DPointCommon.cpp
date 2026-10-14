//
//  DpointCommon.cpp
//  dommyTool
//
//  Created by yuzo on 2016/03/01.
//
//

#include "DPointCommon.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DStatusOperation.h"

#include <type_traits>

#include "dpoint_ids.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>

MStatus getValidInputDP(const MObject& object, MDataBlock& data, const DPData*& inputData) {
    inputData = nullptr;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(object, &status);
    if (!status) return status;

    const DPData* candidate = nullptr;
    status = dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, candidate);
    if (!status) return status;
    if (!candidate || !candidate->hasValidPointTopology()) return MS::kInvalidParameter;
    inputData = candidate;
    return MS::kSuccess;
}

const DPData* getInputDP(const MObject& object, MDataBlock& data) {
    const DPData* inputData = nullptr;
    return getValidInputDP(object, data, inputData) ? inputData : nullptr;
}

MStatus getOptionalInputDP(const MObject& node, const MObject& attribute,
                           MDataBlock& data, const DPData*& inputData)
{
    inputData = nullptr;
    MStatus status;
    MDataHandle inputHandle = data.inputValue(attribute, &status);
    if (!status) return status;

    if (inputHandle.data().isNull()) {
        const MPlug inputPlug(node, attribute);
        const bool connected = inputPlug.isConnected(&status);
        if (!status) return status;
        return connected ? MS::kFailure : MS::kSuccess;
    }

    const DPData* candidate = nullptr;
    status = dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, candidate);
    if (!status) return status;
    if (!candidate || !candidate->hasValidPointTopology()) return MS::kInvalidParameter;
    inputData = candidate;
    return MS::kSuccess;
}

const DPData* getInputDPfromPlug(const MPlug& plg, MObject& obj) {
    const DPData* inputData = NULL;
    const MStatus status = dommy::maya::getPluginDataFromPlug(plg, DPData::id, obj, inputData);
    return status && inputData && inputData->hasValidPointTopology() ? inputData : NULL;
}

const DPData* getInputDPfromHandle(MDataHandle hnd) {
    const DPData* inputData = NULL;
    const MStatus status = dommy::maya::getPluginDataFromHandle(hnd, DPData::id, inputData);
    return status && inputData && inputData->hasValidPointTopology() ? inputData : NULL;
}

MStatus DOutputDPData(MDataBlock& data, const MObject& dirtPlug, const MObject& output,
                      const DPointList& pointlist) {
    const MStatus status = dommy::maya::setPluginDataOutput<DPData>(
        data, output, DPData::id, [&pointlist](DPData& outputData) {
            outputData.setPosition(pointlist);
            outputData.updateBBox();
        });
    return status ? data.setClean(dirtPlug) : status;
}

// get for type MFn
MFn::Type getDpointType(const MObject& node, const MObject& plug, MObject& pointData,
                        MStatus& stat) {
    MPlug targetPlug(node, plug);
    stat = targetPlug.getValue(pointData);

    return pointData.apiType();
}

namespace {

template <typename Array>
MStatus readPlugArrayInput(const MObject& node, const MObject& attribute, Array& output,
                          MStatus (*read)(const MObject&, Array&)) {
    MStatus status;
    MObject object;
    getDpointType(node, attribute, object, status);
    return status ? read(object, output) : status;
}

template <typename Array>
MStatus readBlockArrayInput(MDataBlock& block, const MObject& attribute, Array& output,
                           MStatus (*read)(const MObject&, Array&)) {
    MStatus status;
    MDataHandle input = block.inputValue(attribute, &status);
    if (!status) return status;
    return read(input.data(), output);
}

// Preserve each legacy reader's append/replace contract. Stage conversion before
// copying to the destination; callers must discard it if the final Maya copy fails.
template <typename FunctionSet, typename Array, typename Convert>
MStatus readArrayData(const MObject& object, bool append, Array& output, Convert convert) {
    MStatus status;
    FunctionSet functionSet(object, &status);
    if (!status)
        return status;
    const auto source = functionSet.array(&status);
    if (!status)
        return status;
    using Element = typename std::remove_reference<decltype(output[0U])>::type;
    using SourceElement = typename std::remove_reference<decltype(source[0U])>::type;
    const std::size_t prefix = append ? output.length() : 0U;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(prefix, source.length(),
                                       dommy::generation::kMaxGeneratedPointCount, count) ||
        !dommy::attribute::validArrayConversionBudget(count,
                                                      sizeof(Element) + sizeof(SourceElement)))
        return MS::kInvalidParameter;
    return dommy::maya::buildCheckedArray(
        count,
        [&](unsigned int index, Element& value) -> MStatus {
            if (index < prefix) {
                value = output[index];
                return MS::kSuccess;
            }
            return convert(source[static_cast<unsigned int>(index - prefix)], value);
        },
        output);
}

} // namespace

MStatus getVectorArrayFromData(const MObject& object, MVectorArray& vArray) {
    if (object.isNull()) return MS::kFailure;
    const MFn::Type type = object.apiType();
    switch (type) {
    case MFn::kPointArrayData:
        return readArrayData<MFnPointArrayData>(
            object, true, vArray, [](const MPoint& source, MVector& value) -> MStatus {
                if (!dommy::maya::isFinitePoint(source))
                    return MS::kInvalidParameter;
                value = MVector(source);
                return dommy::maya::isFiniteVector(value) ? MStatus(MS::kSuccess)
                                                          : MStatus(MS::kInvalidParameter);
            });
    case MFn::kVectorArrayData:
        return readArrayData<MFnVectorArrayData>(
            object, true, vArray, [](const MVector& source, MVector& value) -> MStatus {
                if (!dommy::maya::isFiniteVector(source))
                    return MS::kInvalidParameter;
                value = MVector(source);
                return dommy::maya::isFiniteVector(value) ? MStatus(MS::kSuccess)
                                                          : MStatus(MS::kInvalidParameter);
            });
    default:
        return MS::kUnknownParameter;
    }
}

MStatus getPointArrayFromData(const MObject& object, MPointArray& pArray) {
    if (object.isNull()) return MS::kFailure;
    const MFn::Type type = object.apiType();
    switch (type) {
    case MFn::kPointArrayData:
        return readArrayData<MFnPointArrayData>(
            object, true, pArray, [](const MPoint& source, MPoint& value) -> MStatus {
                if (!dommy::maya::isFinitePoint(source))
                    return MS::kInvalidParameter;
                value = MPoint(source);
                return dommy::maya::isFinitePoint(value) ? MStatus(MS::kSuccess)
                                                         : MStatus(MS::kInvalidParameter);
            });
    case MFn::kVectorArrayData:
        return readArrayData<MFnVectorArrayData>(
            object, true, pArray, [](const MVector& source, MPoint& value) -> MStatus {
                if (!dommy::maya::isFiniteVector(source))
                    return MS::kInvalidParameter;
                value = MPoint(source);
                return dommy::maya::isFinitePoint(value) ? MStatus(MS::kSuccess)
                                                         : MStatus(MS::kInvalidParameter);
            });
    default:
        return MS::kUnknownParameter;
    }
}

MStatus getMatrixArrayFromData(const MObject& object, MMatrixArray& matArray) {
    if (object.isNull()) return MS::kFailure;
    const MFn::Type type = object.apiType();
    switch (type) {
    case MFn::kMatrixArrayData:
        return readArrayData<MFnMatrixArrayData>(
            object, true, matArray, [](const MMatrix& source, MMatrix& value) -> MStatus {
                if (!dommy::maya::isFiniteMatrix(source))
                    return MS::kInvalidParameter;
                value = MMatrix(source);
                return dommy::maya::isFiniteMatrix(value) ? MStatus(MS::kSuccess)
                                                          : MStatus(MS::kInvalidParameter);
            });
    default:
        return MS::kUnknownParameter;
    }
}

MStatus getDoubleArrayFromData(const MObject& object, DScalarDataList& output) {
    MDoubleArray source;
    const MStatus status = getDoubleArrayFromData(object, source);
    if (!status)
        return status;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(output.size(), source.length(),
                                       dommy::generation::kMaxGeneratedPointCount, count) ||
        !dommy::attribute::validArrayConversionBudget(count, sizeof(double) + sizeof(DScalarData)))
        return MS::kInvalidParameter;
    DScalarDataList candidate(output);
    const std::size_t prefix = candidate.size();
    candidate.resize(count);
    for (unsigned int index = 0U; index < source.length(); ++index) {
        if (!dommy::numeric::checkedFloatCast(source[index], candidate[prefix + index]))
            return MS::kInvalidParameter;
    }
    output.swap(candidate);
    return MS::kSuccess;
}

MStatus getDoubleArrayFromData(const MObject& object, MDoubleArray& output) {
    if (object.isNull()) return MS::kFailure;
    const MFn::Type type = object.apiType();
    switch (type) {
    case MFn::kDoubleArrayData:
        return readArrayData<MFnDoubleArrayData>(
            object, false, output, [](double source, double& value) -> MStatus {
                return dommy::numeric::checkedNumericCast(source, value)
                           ? MStatus(MS::kSuccess)
                           : MStatus(MS::kInvalidParameter);
            });
    case MFn::kFloatArrayData:
        return readArrayData<MFnFloatArrayData>(
            object, true, output, [](float source, double& value) -> MStatus {
                return dommy::numeric::checkedNumericCast(source, value)
                           ? MStatus(MS::kSuccess)
                           : MStatus(MS::kInvalidParameter);
            });
    default:
        return MS::kInvalidParameter;
    }
}

MStatus getFloatArrayFromData(const MObject& object, MFloatArray& output) {
    if (object.isNull()) return MS::kFailure;
    const MFn::Type type = object.apiType();
    switch (type) {
    case MFn::kFloatArrayData:
        return readArrayData<MFnFloatArrayData>(
            object, false, output, [](float source, float& value) -> MStatus {
                return dommy::numeric::checkedNumericCast(source, value)
                           ? MStatus(MS::kSuccess)
                           : MStatus(MS::kInvalidParameter);
            });
    case MFn::kDoubleArrayData:
        return readArrayData<MFnDoubleArrayData>(
            object, false, output, [](double source, float& value) -> MStatus {
                return dommy::numeric::checkedNumericCast(source, value)
                           ? MStatus(MS::kSuccess)
                           : MStatus(MS::kInvalidParameter);
            });
    default:
        return MS::kInvalidParameter;
    }
}

MStatus getVectorArrayFromData(const MObject& node, const MObject& plug, MVectorArray& output) {
    return readPlugArrayInput(node, plug, output, getVectorArrayFromData);
}
MStatus getPointArrayFromData(const MObject& node, const MObject& plug, MPointArray& output) {
    return readPlugArrayInput(node, plug, output, getPointArrayFromData);
}
MStatus getMatrixArrayFromData(const MObject& node, const MObject& plug, MMatrixArray& output) {
    return readPlugArrayInput(node, plug, output, getMatrixArrayFromData);
}
MStatus getDoubleArrayFromData(const MObject& node, const MObject& plug, DScalarDataList& output) {
    return readPlugArrayInput(node, plug, output, getDoubleArrayFromData);
}
MStatus getDoubleArrayFromData(const MObject& node, const MObject& plug, MDoubleArray& output) {
    return readPlugArrayInput(node, plug, output, getDoubleArrayFromData);
}
MStatus getFloatArrayFromData(const MObject& node, const MObject& plug, MFloatArray& output) {
    return readPlugArrayInput(node, plug, output, getFloatArrayFromData);
}
MStatus getVectorArrayFromData(MDataBlock& block, const MObject& plug, MVectorArray& output) {
    return readBlockArrayInput(block, plug, output, getVectorArrayFromData);
}
MStatus getPointArrayFromData(MDataBlock& block, const MObject& plug, MPointArray& output) {
    return readBlockArrayInput(block, plug, output, getPointArrayFromData);
}
MStatus getMatrixArrayFromData(MDataBlock& block, const MObject& plug, MMatrixArray& output) {
    return readBlockArrayInput(block, plug, output, getMatrixArrayFromData);
}
MStatus getDoubleArrayFromData(MDataBlock& block, const MObject& plug, DScalarDataList& output) {
    return readBlockArrayInput(block, plug, output, getDoubleArrayFromData);
}
MStatus getDoubleArrayFromData(MDataBlock& block, const MObject& plug, MDoubleArray& output) {
    return readBlockArrayInput(block, plug, output, getDoubleArrayFromData);
}
MStatus getFloatArrayFromData(MDataBlock& block, const MObject& plug, MFloatArray& output) {
    return readBlockArrayInput(block, plug, output, getFloatArrayFromData);
}

#pragma mark DPAttribute

void checkDPAttribute(const std::string attrName, DPData& outputDPData, bool delAttr) {

    DPAttributeData::ConstPtr existAttr = outputDPData.getAttr(attrName);

    if (existAttr.get() && delAttr) {
        outputDPData.removeAttr(attrName);
    }
}

MStatus DPAddAttrScalar(DPData& dpdata, const bool& isOutAttr, const bool& isDeleteOrg,
                        const std::string& outAttrName, const std::string& originalAttrName,
                        const DScalarDataList& result) {
    return DPublishAttributeValues(dpdata, isOutAttr, isDeleteOrg, outAttrName, originalAttrName,
                                   result);
}

MStatus DPAddAttrVec2(DPData& dpdata, const bool& isOutAttr, const bool& isDeleteOrg,
                      const std::string& outAttrName, const std::string& originalAttrName,
                      const DVec2DataList& result) {
    return DPublishAttributeValues(dpdata, isOutAttr, isDeleteOrg, outAttrName, originalAttrName,
                                   result);
}

MStatus DPAddAttrVec3(DPData& dpdata, const bool& isOutAttr, const bool& isDeleteOrg,
                      const std::string& outAttrName, const std::string& originalAttrName,
                      const DVec3DataList& result) {
    return DPublishAttributeValues(dpdata, isOutAttr, isDeleteOrg, outAttrName, originalAttrName,
                                   result);
}
