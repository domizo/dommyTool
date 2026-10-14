//
//  DNumericToFloatArray.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/11.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DNumericToFloatArray.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DNumericValidation.h"

#include "DMayaHeaders.h"
#include "general_ids.h"

#include <cstddef>
#include <utility>

namespace {

MStatus appendFloat(double value, MFloatArray& output) {
    float converted = 0.0f;
    if (!dommy::numeric::checkedFloatCast(value, converted)) {
        return MS::kInvalidParameter;
    }
    return output.append(converted);
}

bool outputFitsBudget(std::size_t valueCount, std::size_t componentCount) {
    std::size_t outputCount = 0U;
    return dommy::generation::checkedProduct(
        valueCount, componentCount, dommy::generation::kMaxGeneratedPointCount, outputCount);
}

MStatus appendNumeric(MFnNumericData::Type type, MDataHandle& handle,
                      MFloatArray& output) {
    MStatus status;
    switch (type) {
    case MFnNumericData::kBoolean:
    case MFnNumericData::kByte:
    case MFnNumericData::kChar:
    case MFnNumericData::kShort:
    case MFnNumericData::kInt:
    case MFnNumericData::kInt64:
    case MFnNumericData::kFloat:
    case MFnNumericData::kDouble: {
        long double value = 0.0L;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::readNumericScalar(handle, value));
        // Preserve the historical double intermediate before float narrowing.
        return appendFloat(static_cast<double>(value), output);
    }
    default:
        break;
    }

    if (type == MFnNumericData::kInvalid) return MS::kInvalidParameter;
    MObject numericObject = handle.data();
    if (numericObject.isNull()) return MS::kInvalidParameter;
    MFnNumericData numericData(numericObject, &status);
    if (!status)
        return status;

    switch (type) {
    case MFnNumericData::k2Short: {
        short first = 0, second = 0;
        status = numericData.getData2Short(first, second);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        return appendFloat(second, output);
    }
    case MFnNumericData::k3Short: {
        short first = 0, second = 0, third = 0;
        status = numericData.getData3Short(first, second, third);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(second, output));
        return appendFloat(third, output);
    }
    case MFnNumericData::k2Int: {
        int first = 0, second = 0;
        status = numericData.getData2Int(first, second);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        return appendFloat(second, output);
    }
    case MFnNumericData::k3Int: {
        int first = 0, second = 0, third = 0;
        status = numericData.getData3Int(first, second, third);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(second, output));
        return appendFloat(third, output);
    }
    case MFnNumericData::k2Float: {
        float first = 0.0f, second = 0.0f;
        status = numericData.getData2Float(first, second);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        return appendFloat(second, output);
    }
    case MFnNumericData::k3Float: {
        float first = 0.0f, second = 0.0f, third = 0.0f;
        status = numericData.getData3Float(first, second, third);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(second, output));
        return appendFloat(third, output);
    }
    case MFnNumericData::k2Double: {
        double first = 0.0, second = 0.0;
        status = numericData.getData2Double(first, second);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        return appendFloat(second, output);
    }
    case MFnNumericData::k3Double: {
        double first = 0.0, second = 0.0, third = 0.0;
        status = numericData.getData3Double(first, second, third);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(second, output));
        return appendFloat(third, output);
    }
    case MFnNumericData::k4Double: {
        double first = 0.0, second = 0.0, third = 0.0, fourth = 0.0;
        status = numericData.getData4Double(first, second, third, fourth);
        if (!status)
            return status;
        DOMMY_RETURN_IF_FAILURE(appendFloat(first, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(second, output));
        DOMMY_RETURN_IF_FAILURE(appendFloat(third, output));
        return appendFloat(fourth, output);
    }
    default:
        return MS::kInvalidParameter;
    }
}

MStatus appendMatrix(const MMatrix& matrix, MFloatArray& output) {
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            DOMMY_RETURN_IF_FAILURE(appendFloat(matrix[row][column], output));
        }
    }
    return MS::kSuccess;
}

template <typename Value>
MStatus appendXYZ(const Value& value, MFloatArray& output) {
    for (unsigned int component = 0U; component < 3U; ++component) {
        DOMMY_RETURN_IF_FAILURE(appendFloat(value[component], output));
    }
    return MS::kSuccess;
}

template <typename FunctionSet, typename Append>
MStatus appendNumericArray(MDataHandle handle, std::size_t components,
                           MFloatArray& output, Append append) {
    typedef decltype(std::declval<FunctionSet&>().array(nullptr)) Array;
    return dommy::maya::visitBoundedNumericArray<FunctionSet>(handle, components,
        [&](const Array& values, std::size_t count) -> MStatus {
            for (unsigned int index = 0U; index < count; ++index) {
                DOMMY_RETURN_IF_FAILURE(append(values[index], output));
            }
            return MS::kSuccess;
        });
}

} // namespace

const MTypeId DNumericToFloatArray::typeId(ID_UTILITY_NUMERIC_TO_FLOAT_ARRAY);

MObject DNumericToFloatArray::inputData;
MObject DNumericToFloatArray::outDataArray;

void* DNumericToFloatArray::creator() {
    return ::dommy::maya::createObject("DNumericToFloatArray", [&]() {
        return new DNumericToFloatArray();
    });
}

MStatus DNumericToFloatArray::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDataArray)
        return MS::kUnknownParameter;

    MFloatArray output;
    const MPlug inputPlug(thisMObject(), inputData);
    MStatus status;
    const bool connected = inputPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) {
        return DOutputDataMayaFloatArray(data, plug, outDataArray, output);
    }

    MDataHandle inputHandle = data.inputValue(inputData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MFnData::Type dataType = inputHandle.type();

    if (dataType == MFnData::kInvalid || dataType == MFnData::kNumeric) {
        bool genericNumeric = false, genericNull = false;
        const bool generic = inputHandle.isGeneric(genericNumeric, genericNull);
        if (generic && genericNull) return MS::kInvalidParameter;
        if (generic && genericNumeric) {
            long double value = 0.0L;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::readNumericScalar(inputHandle, value));
            DOMMY_RETURN_IF_FAILURE(appendFloat(static_cast<double>(value), output));
        } else {
            MFnNumericData::Type numericType = inputHandle.isNumeric()
                ? inputHandle.numericType() : MFnNumericData::kInvalid;
            if (numericType == MFnNumericData::kInvalid && dataType == MFnData::kNumeric) {
                MObject numericObject = inputHandle.data();
                MFnNumericData numericData(numericObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                numericType = numericData.numericType(&status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
            }
            DOMMY_RETURN_IF_FAILURE(appendNumeric(numericType, inputHandle, output));
        }
    } else if (dataType == MFnData::kMatrix) {
        if (!outputFitsBudget(1U, 16U))
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(appendMatrix(inputHandle.asMatrix(), output));
    } else if (dataType == MFnData::kVectorArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnVectorArrayData>(
            inputHandle, 3U, output, appendXYZ<MVector>));
    } else if (dataType == MFnData::kPointArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnPointArrayData>(
            inputHandle, 3U, output, appendXYZ<MPoint>));
    } else if (dataType == MFnData::kFloatArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnFloatArrayData>(
            inputHandle, 1U, output, appendFloat));
    } else if (dataType == MFnData::kDoubleArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnDoubleArrayData>(
            inputHandle, 1U, output, appendFloat));
    } else if (dataType == MFnData::kIntArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnIntArrayData>(
            inputHandle, 1U, output, appendFloat));
    } else if (dataType == MFnData::kMatrixArray) {
        DOMMY_RETURN_IF_FAILURE(appendNumericArray<MFnMatrixArrayData>(
            inputHandle, 16U, output, appendMatrix));
    } else {
        return MS::kInvalidParameter;
    }

    return DOutputDataMayaFloatArray(data, plug, outDataArray, output);
}
DOMMY_COMPUTE_CATCH("DNumericToFloatArray")

MStatus DNumericToFloatArray::initialize() {
    MStatus status;
    MFnGenericAttribute genericAttribute;

    inputData = genericAttribute.create("inputData", "ind", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMatrixArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kNumeric));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kBoolean));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kByte));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kChar));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kShort));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k2Short));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k3Short));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kInt));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k2Int));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k3Int));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kInt64));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kFloat));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k2Float));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k3Float));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::kDouble));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k2Double));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k3Double));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addNumericDataAccept(MFnNumericData::k4Double));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMatrix));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputData));

    outDataArray = genericAttribute.create("outDataArray", "out", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputData, outDataArray));
    return MS::kSuccess;
}
