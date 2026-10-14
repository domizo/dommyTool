// Runs complete production initialize() bodies with a failure-injectable host.
// The frozen trace protects successful schemas; this does not emulate Maya DG.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"

namespace fixture {
int calls = 0, failAt = 0, nextId = 1;
bool throwAtFailure = false;
std::uint64_t hash = 14695981039346656037ULL;
void append(const std::string& value) {
    for (unsigned char byte : value) { hash ^= byte; hash *= 1099511628211ULL; }
    hash ^= 255U; hash *= 1099511628211ULL;
}
void reset(int failure = 0, bool throwing = false) {
    calls = 0; failAt = failure; nextId = 1; throwAtFailure = throwing;
    hash = 14695981039346656037ULL;
}
MStatus operation(const char* label) {
    append(label);
    if (++calls == failAt) {
        if (throwAtFailure) throw std::runtime_error("injected host exception");
        return MS::kFailure;
    }
    return MS::kSuccess;
}
template <typename T> void argument(const T& value) {
    std::ostringstream text;
    text << std::setprecision(17) << static_cast<double>(value);
    append(text.str());
}
void argument(const char* value) { append(value); }
void argument(const MString& value) { append(value.asChar()); }
void arguments() {}
template <typename T, typename... Rest> void arguments(const T& value, const Rest&... rest) {
    argument(value); arguments(rest...);
}
template <typename... Args> MStatus checked(const char* method, const Args&... args) {
    arguments(args...); return operation(method);
}
}

struct MObject { int value; MObject(int v = 0) : value(v) {}
    explicit operator double() const { return value; } static const MObject kNullObj; };
const MObject MObject::kNullObj;
struct MMatrix { static const int identity = -1; };
struct MFnNumericData { enum Type { kBoolean, kInt, kFloat, kDouble, k2Int, k3Int,
    k2Float, k3Float, k2Double, k3Double }; };
struct MFnData { enum Type { kAny, kString, kStringArray, kNumeric, kIntArray,
    kFloatArray, kDoubleArray, kPointArray, kVectorArray, kMatrix, kMatrixArray,
    kMesh, kNurbsCurve, kNurbsSurface }; };
struct DPData { static const int id = 101; };
struct DCurveData { static const int id = 102; };
struct DMeshData { static const int id = 103; };
struct DOpenVDBData { static const int id = 104; };
const int DPData::id;
const int DCurveData::id;
const int DMeshData::id;
const int DOpenVDBData::id;
const int MMatrix::identity;
namespace dommy { namespace generation { const int kMaxGeneratedTransformCount = 1000000; } }

struct Attribute {
    int object = 0;
    template <typename... Args> MObject make(const char* method, MStatus* status, const Args&... args) {
        const MStatus result = fixture::checked(method, args...);
        if (status) *status = result;
        object = fixture::nextId++;
        return MObject(object);
    }
#define METHOD(name) template <typename... Args> MStatus name(const Args&... args) { \
        return fixture::checked(#name, object, args...); }
    METHOD(setConnectable) METHOD(setDefault) METHOD(setKeyable) METHOD(setReadable)
    METHOD(setSoftMax) METHOD(setSoftMin) METHOD(setStorable) METHOD(setWritable)
    METHOD(setArray) METHOD(setUsesArrayDataBuilder) METHOD(setMax) METHOD(setMin)
    METHOD(setCached) METHOD(setAffectsAppearance) METHOD(setUsedAsColor)
    METHOD(setHidden) METHOD(setWorldSpace) METHOD(addAccept) METHOD(addField)
    METHOD(addChild) METHOD(addDataAccept) METHOD(addNumericDataAccept)
#undef METHOD
};
struct MFnNumericAttribute : Attribute {
    MObject create(const char* a, const char* b, MFnNumericData::Type type,
                   double value = 0, MStatus* status = nullptr) {
        return make("numeric", status, a, b, type, value);
    }
    MObject create(const char* a, const char* b, MObject x, MObject y,
                   MObject z = MObject(), MStatus* status = nullptr) {
        return make("numericChildren", status, a, b, x.value, y.value, z.value);
    }
    MObject createColor(const char* a, const char* b, MStatus* status = nullptr) {
        return make("color", status, a, b);
    }
    MObject createPoint(const char* a, const char* b, MStatus* status = nullptr) {
        return make("point", status, a, b);
    }
};
struct MFnTypedAttribute : Attribute {
    template <typename Type> MObject create(const char* a, const char* b, Type type,
                   MObject value = MObject(), MStatus* status = nullptr) {
        return make("typed", status, a, b, type, value.value);
    }
};
struct MFnGenericAttribute : Attribute {
    MObject create(const char* a, const char* b, MStatus* status = nullptr) {
        return make("generic", status, a, b);
    }
};
struct MFnMessageAttribute : Attribute {
    MObject create(const char* a, const char* b, MStatus* status = nullptr) {
        return make("message", status, a, b);
    }
};
struct MFnCompoundAttribute : Attribute {
    MObject create(const char* a, const char* b, MStatus* status = nullptr) {
        return make("compound", status, a, b);
    }
};
struct MFnEnumAttribute : Attribute {
    MObject create(const char* a, const char* b, short value = 0, MStatus* status = nullptr) {
        return make("enum", status, a, b, value);
    }
};
struct MFnMatrixAttribute : Attribute {
    enum Type { kDouble };
    MObject create(const char* a, const char* b, Type type = kDouble, MStatus* status = nullptr) {
        return make("matrix", status, a, b, type);
    }
};
struct MFnUnitAttribute : Attribute {
    enum Type { kTime };
    MObject create(const char* a, const char* b, Type type,
                   double value = 0, MStatus* status = nullptr) {
        return make("unit", status, a, b, type, value);
    }
    MObject create(const char* a, const char* b, const MTime& value, MStatus* status = nullptr) {
        return make("unitTime", status, a, b, value.value(), value.unit());
    }
};
struct MFnStringData : Attribute {
    MObject create(const char* value, MStatus* status = nullptr) {
        return make("stringData", status, value);
    }
};
struct MFnStringArrayData : Attribute {
    MObject create(MStatus* status = nullptr) { return make("stringArrayData", status); }
};
struct MFnVectorArrayData { static const MFnData::Type kVectorArray = MFnData::kVectorArray; };
struct MRampAttribute {
    static MObject createCurveRamp(const char* a, const char* b, MStatus* status = nullptr) {
        Attribute attr; return attr.make("curveRamp", status, a, b);
    }
};
struct AttributeNode {
    static const MObject outputGeom;
    static const MObject message;
    static const MObject mOutputForce;
    static MStatus addAttribute(MObject object) { return fixture::checked("addAttribute", object.value); }
    static MStatus attributeAffects(MObject in, MObject out) {
        return fixture::checked("attributeAffects", in.value, out.value);
    }
};
const MObject AttributeNode::outputGeom(-1);
const MObject AttributeNode::message(-2);
const MObject AttributeNode::mOutputForce(-3);
#define CHECK_MSTATUS(expression) do { (void)(expression); } while (false);
#define CHECK_MSTATUS_AND_RETURN_IT(expression) DOMMY_RETURN_IF_FAILURE(expression)
#include "plugin_initializers.inc"

int main(int argc, char** argv) {
    const bool schemaOnly = argc > 1 && std::strcmp(argv[1], "--schema") == 0;
    int total = 0;
    for (const auto& node : initializers) {
        fixture::reset();
        assert(node.initialize() == MS::kSuccess);
        const int operations = fixture::calls;
        if (schemaOnly) {
            std::printf("%s %016llx %d\n", node.name,
                static_cast<unsigned long long>(fixture::hash), operations);
            continue;
        }
        assert(fixture::hash == node.schema && operations == node.operations);
        for (int point = 1; point <= operations; ++point) {
            for (bool throwing : {false, true}) {
                fixture::reset(point, throwing);
                if (node.initialize() == MS::kSuccess || fixture::calls != point) {
                    std::fprintf(stderr, "%s swallowed %s at operation %d (calls %d)\n",
                        node.name, throwing ? "exception" : "failure", point, fixture::calls);
                    return 1;
                }
            }
        }
        total += operations;
    }
    if (!schemaOnly) std::printf("92 initializers: %d status failures and %d exceptions checked\n", total, total);
}
