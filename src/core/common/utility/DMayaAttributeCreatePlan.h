#pragma once

#include "DAttributeCreatePlan.h"
#include "DStatusOperation.h"
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MString.h>
#include <maya/MVector.h>

namespace dommy {
namespace maya {

struct AttributeCreateControls {
    MObject list, type, scope, name, size, precision, action, input;
};

inline std::size_t manualCreatedAttributeWidth(short type, int size, short precision) {
    switch (type) {
    case DA_CREATE_FLOAT:
        return (size == 1 ? 1U : size == 2 ? 2U : 3U) *
            (precision == 0 ? sizeof(float) : sizeof(double));
    case DA_CREATE_INT: return (size == 1 ? 1U : size == 2 ? 2U : 3U) * sizeof(int32_t);
    case DA_CREATE_VECTOR: return precision == 0 ? sizeof(Imath::V3f) : sizeof(Imath::V3d);
    case DA_CREATE_STRING: return sizeof(std::string);
    case DA_CREATE_MATRIX: return sizeof(MMatrix);
    case DA_CREATE_BOOL: return sizeof(bool);
    default: return 0U; // Preserve the legacy no-op for an unused enum value.
    }
}

inline MStatus createdStringBytes(const MString& value, std::size_t& bytes) {
    if (value.length() > generation::kMaxGeneratedStringByteCount) return MS::kInvalidParameter;
    const char* text = value.asChar();
    if (!text) return MS::kFailure;
    bytes = 0U;
    while (bytes <= generation::kMaxGeneratedStringByteCount && text[bytes] != '\0') ++bytes;
    return bytes <= generation::kMaxGeneratedStringByteCount
        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
}

template <typename FunctionSet>
MStatus createdArrayCount(const MObject& object, std::size_t& count) {
    MStatus status;
    const FunctionSet array(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    count = array.length(&status);
    return status;
}

// Binding and length inspection precede any native array() or STL payload
// copy. The actual compute keeps its historical mismatch fallback/skip policy.
inline MStatus preflightAttributeCreateRows(MDataBlock& data, const MObject& node,
                                           const AttributeCreateControls& controls,
                                           bool collection, attribute::AttributeCreatePlan& plan) {
    MStatus status;
    MArrayDataHandle rows = data.inputArrayValue(controls.list, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int count = rows.elementCount(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (count > generation::kMaxGeneratedStringCount) return MS::kInvalidParameter;
    plan.addWork(count);
    for (unsigned int row = 0U; row < count; ++row) {
        DOMMY_RETURN_IF_FAILURE(rows.jumpToArrayElement(row));
        MDataHandle handle = rows.inputValue(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MString nativeName = handle.child(controls.name).asString();
        std::size_t nameBytes = 0U;
        DOMMY_RETURN_IF_FAILURE(createdStringBytes(nativeName, nameBytes));
        if (nameBytes == 0U) continue;
        plan.addWork(nameBytes);
        plan.addScratchBytes(nameBytes);
        const std::string name(nativeName.asChar());
        const bool pointScope = collection && handle.child(controls.scope).asShort() == DP_ATTR_CLASS_POINT;
        if (collection && !pointScope &&
            handle.child(controls.scope).asShort() != DP_ATTR_CLASS_PRIMITIVE)
            return MS::kInvalidParameter;
        const bool replace = handle.child(controls.action).asShort() != 1;
        const std::size_t first = pointScope ? 1U : 0U;
        const std::size_t last = pointScope ? plan.size() : 1U;
        bool active = false;
        std::size_t expected = 0U;
        for (std::size_t slot = first; slot < last; ++slot) {
            if (!generation::checkedSum(expected, plan.count(slot),
                generation::kMaxGeneratedPointCount, expected)) return MS::kInvalidParameter;
            active = plan.shouldCreate(slot, name, replace) || active;
        }
        if (!active) continue; // Not Add still ignores unused controls.
        const short type = handle.child(controls.type).asShort();
        const short precision = handle.child(controls.precision).asShort();
        std::size_t width = manualCreatedAttributeWidth(type, handle.child(controls.size).asInt(), precision);
        std::size_t repeatedStrings = 0U, arrayCount = 0U, rawWidth = 0U;
        bool arrayInput = false, stringArray = false;
        MObject arrayObject;
        const MPlug inputPlug(node, controls.input);
        const bool connected = inputPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (connected) {
            MDataHandle input = handle.child(controls.input);
            const MFnData::Type inputType = input.type();
            arrayObject = input.data();
            switch (inputType) {
            case MFnData::kNumeric:
                if (type == DA_CREATE_INT) width = sizeof(int32_t);
                else if (type == DA_CREATE_FLOAT) width = precision == 0 ? sizeof(float) : sizeof(double);
                else if (type == DA_CREATE_BOOL) width = sizeof(bool);
                break;
            case MFnData::kString:
                width = sizeof(std::string);
                DOMMY_RETURN_IF_FAILURE(createdStringBytes(input.asString(), repeatedStrings));
                plan.addWork(repeatedStrings);
                plan.addScratchBytes(repeatedStrings);
                break;
            case MFnData::kMatrix: width = sizeof(MMatrix); break;
            case MFnData::kVectorArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnVectorArrayData>(arrayObject, arrayCount));
                width = precision == 0 ? sizeof(Imath::V3f) : sizeof(Imath::V3d);
                rawWidth = sizeof(MVector); arrayInput = true; break;
            case MFnData::kPointArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnPointArrayData>(arrayObject, arrayCount));
                width = precision == 0 ? sizeof(Imath::V3f) : sizeof(Imath::V3d);
                rawWidth = sizeof(MPoint); arrayInput = true; break;
            case MFnData::kFloatArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnFloatArrayData>(arrayObject, arrayCount));
                width = rawWidth = sizeof(float); arrayInput = true; break;
            case MFnData::kDoubleArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnDoubleArrayData>(arrayObject, arrayCount));
                width = rawWidth = sizeof(double); arrayInput = true; break;
            case MFnData::kIntArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnIntArrayData>(arrayObject, arrayCount));
                width = rawWidth = sizeof(int); arrayInput = true; break;
            case MFnData::kStringArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnStringArrayData>(arrayObject, arrayCount));
                width = sizeof(std::string); rawWidth = sizeof(MString);
                arrayInput = stringArray = true; break;
            case MFnData::kMatrixArray:
                DOMMY_RETURN_IF_FAILURE(createdArrayCount<MFnMatrixArrayData>(arrayObject, arrayCount));
                width = rawWidth = sizeof(MMatrix); arrayInput = true; break;
            default: break;
            }
        }
        std::size_t stringScratch = 0U;
        if (arrayInput) {
            plan.addScratch(arrayCount, rawWidth + 2U * width);
            if (stringArray) {
                const MFnStringArrayData strings(arrayObject, &status);
                DOMMY_RETURN_IF_FAILURE(status);
                for (std::size_t i = 0U; i < arrayCount; ++i) {
                    std::size_t bytes = 0U;
                    DOMMY_RETURN_IF_FAILURE(createdStringBytes(strings[static_cast<unsigned int>(i)], bytes));
                    plan.addWork(bytes);
                    if (!generation::checkedSum(stringScratch, bytes,
                        generation::kMaxGeneratedStringByteCount, stringScratch)) return MS::kInvalidParameter;
                }
                plan.addScratch(arrayCount, rawWidth + 2U * width, stringScratch * 2U);
            }
            if (arrayCount == 0U || arrayCount != expected) {
                if (collection) continue;
                width = manualCreatedAttributeWidth(type, handle.child(controls.size).asInt(), precision);
                stringArray = false;
            }
        }
        if (width == 0U) continue;
        std::size_t offset = 0U;
        for (std::size_t slot = first; slot < last; ++slot) {
            const std::size_t points = plan.count(slot);
            if (plan.shouldCreate(slot, name, replace)) {
                std::size_t strings = 0U;
                if (stringArray) {
                    const MFnStringArrayData values(arrayObject, &status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    for (std::size_t i = 0U; i < points; ++i) {
                        std::size_t bytes = 0U;
                        DOMMY_RETURN_IF_FAILURE(createdStringBytes(values[static_cast<unsigned int>(offset + i)], bytes));
                        plan.addWork(bytes);
                        if (!generation::checkedSum(strings, bytes,
                            generation::kMaxGeneratedStringByteCount, strings)) return MS::kInvalidParameter;
                    }
                } else if (!generation::checkedProduct(points, repeatedStrings,
                    generation::kMaxGeneratedStringByteCount, strings)) return MS::kInvalidParameter;
                plan.addField(slot, name, width, strings);
            }
            offset += points; // Skipped point owners still consume flattened input.
        }
    }
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
