#pragma once

// Test-only attribute declarations and status injection; no Maya registration emulation.
// Used by standalone, single-translation-unit test executables.
#include <maya/MStatus.h>

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int callCount = 0, failAt = -1;
int attributeThrowAt = -1;
MS::MStatusCode injectedStatus = MS::kFailure;
std::vector<std::string> operations;

MStatus operation(const std::string& name) {
    operations.push_back(name);
    const int index = callCount++;
    if (index == attributeThrowAt) throw std::runtime_error("injected attribute failure");
    return index == failAt ? injectedStatus : MS::kSuccess;
}

template <typename Value> std::string text(const Value& value) {
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

void resetCalls(int failure = -1) {
    callCount = 0;
    failAt = failure;
    attributeThrowAt = -1;
    operations.clear();
}
std::string schemaTrace(const char* name, MStatus (*initialize)()) {
    resetCalls();
    assert(initialize() == MS::kSuccess);
    std::string trace = std::string(name) + "\n";
    for (const std::string& entry : operations)
        trace += entry + "\n";
    return trace;
}

int testFailures(const char* name, MStatus (*initialize)(), int expectedCalls) {
    const std::string baseline = schemaTrace(name, initialize);
    assert(callCount == expectedCalls);
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < expectedCalls; ++failure) {
            resetCalls(failure);
            assert(initialize() == status);
            // No later API call, including a retry of the failed expression.
            assert(callCount == failure + 1);
            assert(schemaTrace(name, initialize) == baseline);
        }
    }
    return expectedCalls * 2;
}
} // namespace

struct MObject {
    std::string name;
    static const MObject kNullObj;
    bool isNull() const {
        return name.empty() || name == "null";
    }
};
const MObject MObject::kNullObj = {"null"};

struct AttributeNode {
    static MStatus addAttribute(const MObject& attribute) {
        return operation("add " + attribute.name);
    }
    static MStatus attributeAffects(const MObject& input, const MObject& output) {
        return operation("affects " + input.name + " " + output.name);
    }
};

namespace MFnNumericData {
enum Type {
    kDouble,
    kInt,
    kBoolean,
    kFloat,
    kByte,
    kChar,
    kShort,
    k2Short,
    k3Short,
    k2Int,
    k3Int,
    kInt64,
    k2Float,
    k3Float,
    k2Double,
    k3Double,
    k4Double,
    kLong = kInt
};
inline const char* typeName(Type type) {
    switch (type) {
    case kDouble:
        return "double";
    case kInt:
        return "int";
    case kBoolean:
        return "bool";
    case kFloat:
        return "float";
    case kByte:
        return "byte";
    case kChar:
        return "char";
    case kShort:
        return "short";
    case k2Short:
        return "2short";
    case k3Short:
        return "3short";
    case k2Int:
        return "2int";
    case k3Int:
        return "3int";
    case kInt64:
        return "int64";
    case k2Float:
        return "2float";
    case k3Float:
        return "3float";
    case k2Double:
        return "2double";
    case k3Double:
        return "3double";
    case k4Double:
        return "4double";
    }
    assert(false);
    return "invalid";
}
} // namespace MFnNumericData
namespace MFnData {
enum Type {
    kNurbsCurve,
    kAny,
    kMesh,
    kNurbsSurface,
    kIntArray,
    kString,
    kComponentList,
    kInvalid,
    kVectorArray,
    kPointArray,
    kFloatArray,
    kDoubleArray,
    kMatrixArray,
    kNumeric,
    kMatrix,
    kStringArray,
    kNObject
};
inline const char* typeName(Type type) {
    switch (type) {
    case kNObject:
        return "nObject";
    case kNurbsCurve:
        return "curve";
    case kAny:
        return "geometry";
    case kMesh:
        return "mesh";
    case kNurbsSurface:
        return "surface";
    case kIntArray:
        return "intArray";
    case kString:
        return "string";
    case kStringArray:
        return "stringArray";
    case kComponentList:
        return "componentList";
    case kVectorArray:
        return "vectorArray";
    case kPointArray:
        return "pointArray";
    case kFloatArray:
        return "floatArray";
    case kDoubleArray:
        return "doubleArray";
    case kMatrixArray:
        return "matrixArray";
    case kNumeric:
        return "numeric";
    case kMatrix:
        return "matrix";
    case kInvalid:
        break;
    }
    assert(false);
    return "invalid";
}
} // namespace MFnData
namespace MFnGeometryData {
using MFnData::kAny;
}

struct Attribute {
    std::string name;

    MObject bind(const char* longName, const char* shortName, const std::string& kind,
                 MStatus* status) {
        name = longName;
        const MStatus result = operation("create " + name + " " + shortName + " " + kind);
        if (status)
            *status = result;
        return result == MS::kSuccess ? MObject{name} : MObject::kNullObj;
    }

    MStatus setDefault(double value) {
        return operation(name + " default " + text(value));
    }
    MStatus setDefault(double x, double y) {
        return operation(name + " default " + text(x) + " " + text(y));
    }
    MStatus setDefault(double x, double y, double z) {
        return operation(name + " default " + text(x) + " " + text(y) + " " + text(z));
    }
    MStatus setMin(double value) {
        return operation(name + " min " + text(value));
    }
    MStatus setMax(double value) {
        return operation(name + " max " + text(value));
    }
    MStatus setSoftMin(double value) {
        return operation(name + " softMin " + text(value));
    }
    MStatus setSoftMax(double value) {
        return operation(name + " softMax " + text(value));
    }
    MStatus setKeyable(bool value) {
        return operation(name + " keyable " + text(value));
    }
    MStatus setStorable(bool value) {
        return operation(name + " storable " + text(value));
    }
    MStatus setHidden(bool value) {
        return operation(name + " hidden " + text(value));
    }
    MStatus setReadable(bool value) {
        return operation(name + " readable " + text(value));
    }
    MStatus setWritable(bool value) {
        return operation(name + " writable " + text(value));
    }
    MStatus setArray(bool value) {
        return operation(name + " array " + text(value));
    }
    MStatus setUsesArrayDataBuilder(bool value) {
        return operation(name + " arrayBuilder " + text(value));
    }
    MStatus setCached(bool value) {
        return operation(name + " cached " + text(value));
    }
    MStatus setConnectable(bool value) {
        return operation(name + " connectable " + text(value));
    }
    MStatus setUsedAsFilename(bool value) {
        return operation(name + " filename " + text(value));
    }
};

struct MFnCompoundAttribute : Attribute {
    MObject create(const char* longName, const char* shortName, MStatus* status = nullptr) {
        return bind(longName, shortName, "compound", status);
    }
    MStatus addChild(const MObject& child) {
        return operation(name + " child " + child.name);
    }
};

struct MFnMatrixAttribute : Attribute {
    enum Type { kDouble };
    MObject create(const char* longName, const char* shortName, Type, MStatus* status = nullptr) {
        return bind(longName, shortName, "matrix double", status);
    }
};

struct MFnUnitAttribute : Attribute {
    enum Type { kTime };
    MObject create(const char* longName, const char* shortName, Type, double value = 0.0,
                   MStatus* status = nullptr) {
        return bind(longName, shortName, "time " + text(value), status);
    }
};

struct MFnGenericAttribute : Attribute {
    template <typename TypeId> MStatus addAccept(const TypeId& type) {
        return operation(name + " accepts id " + text(type.id()));
    }
    MObject create(const char* longName, const char* shortName, MStatus* status = nullptr) {
        return bind(longName, shortName, "generic", status);
    }
    MStatus addDataAccept(MFnData::Type type) {
        return operation(name + " accepts " + MFnData::typeName(type));
    }
    MStatus addNumericDataAccept(MFnNumericData::Type type) {
        return operation(name + " accepts numeric " + MFnNumericData::typeName(type));
    }
};

struct MFnNumericAttribute : Attribute {
    MObject create(const char* longName, const char* shortName, MFnNumericData::Type type,
                   double value = 0.0, MStatus* status = nullptr) {
        return bind(longName, shortName,
                    std::string(MFnNumericData::typeName(type)) + " " + text(value), status);
    }
    MObject create(const char* longName, const char* shortName, const MObject& x, const MObject& y,
                   const MObject& z, MStatus* status = nullptr) {
        return bind(longName, shortName, "compound " + x.name + " " + y.name + " " + z.name,
                    status);
    }
};

struct MFnTypedAttribute : Attribute {
    MStatus setDefault(const MObject& value) {
        return operation(name + " default " + value.name);
    }
    MObject create(const char* longName, const char* shortName, MFnData::Type type,
                   const MObject& value = MObject::kNullObj, MStatus* status = nullptr) {
        return bind(longName, shortName, std::string(MFnData::typeName(type)) + " " + value.name,
                    status);
    }
};

struct MFnEnumAttribute : Attribute {
    MObject create(const char* longName, const char* shortName, int value,
                   MStatus* status = nullptr) {
        return bind(longName, shortName, "enum " + text(value), status);
    }
    MStatus addField(const char* label, int value) {
        return operation(name + " field " + label + " " + text(value));
    }
};

struct MRampAttribute {
#ifdef DOMMY_PROCEDURAL_API_TESTS
    template<class Plug>explicit MRampAttribute(const Plug&,MStatus* status){
        *status=operation("ramp.create");
    }
    void getValueAtPosition(float position,float& value,MStatus* status)const{
        *status=operation("ramp.sample");value=position;
    }
#endif
    static MObject createColorRamp(const char* longName, const char* shortName, MStatus* status) {
        Attribute attribute;
        return attribute.bind(longName, shortName, "colorRamp", status);
    }
    static MObject createCurveRamp(const char* longName, const char* shortName, MStatus* status = nullptr) {
        Attribute attribute;
        return attribute.bind(longName, shortName, "ramp", status);
    }
};
