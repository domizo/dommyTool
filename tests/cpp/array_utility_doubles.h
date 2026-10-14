#pragma once
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "modules/point/DPoint/dpoint_ids.h"
#include "modules/curve/DCurve/dcurve_ids.h"
#include "modules/mesh/DMesh/dmesh_ids.h"

#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>

#define CHECK_MSTATUS(expression)                                                                  \
    do {                                                                                           \
        const MStatus result = (expression);                                                       \
        if (!result)                                                                               \
            result.pAPIerror(__FILE__, __LINE__);                                                  \
    } while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(expression) DOMMY_RETURN_IF_FAILURE(expression)

namespace {
int shortAt = -1, throwAt = -1, publications = 0;
bool faultReached = false;
std::vector<std::array<double, 4>> published;
std::vector<std::string> publishedStrings;
MStatus api(const std::string& name) {
    const int index = callCount;
    const MStatus status = operation(name);
    if (!status || index == shortAt || index == throwAt)
        faultReached = true;
    if (index == throwAt)
        throw std::runtime_error("injected API exception");
    return status;
}
void resetRun(int failure = -1) {
    resetCalls(failure);
    shortAt = throwAt = -1;
    faultReached = false;
    publications = 0;
    published.clear();
    publishedStrings.clear();
}
void readValue() {
    assert(!faultReached);
}
} // namespace

struct ArrayString : MString {
    using MString::MString;
    ArrayString(const MString& value) : MString(value) {}
    const char* asUTF8(int& length) const {
        length = static_cast<int>(std::strlen(asChar()));
        return asChar();
    }
};
#define MString ArrayString
struct MTypeId {
    unsigned int value;
    explicit MTypeId(unsigned int value = 0) : value(value) {}
    unsigned int id() const {
        return value;
    }
    bool operator==(const MTypeId& other) const {
        return value == other.value;
    }
};
using MInt64 = long long;
using uint = unsigned int;
namespace MFn {
enum Type {
    kPointArrayData,
    kVectorArrayData,
    kMatrixArrayData,
    kFloatArrayData,
    kDoubleArrayData,
    kInvalid
};
}
struct Input;
struct ArrayObject : MObject {
    Input* inputOwner = nullptr;
    ArrayObject(const std::string& value = "") { name = value; }
    ArrayObject(const MObject& value) : MObject(value) {}
    MFn::Type apiType() const;
};
#define MObject ArrayObject
struct MPoint;
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    explicit MVector(const MPoint&);
    double operator[](unsigned int i)const{return i==0?x:(i==1?y:z);}
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0, double w = 1) : x(x), y(y), z(z), w(w) {}
    explicit MPoint(const MVector& v) : x(v.x), y(v.y), z(v.z), w(1) {}
    double operator[](unsigned int i)const{return i==0?x:(i==1?y:z);}
};
MVector::MVector(const MPoint& p) : x(p.x), y(p.y), z(p.z) {}
struct MMatrix {
    double values[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    double* operator[](unsigned int i) {
        return values[i];
    }
    const double* operator[](unsigned int i) const {
        return values[i];
    }
};
struct MColor {
    enum Type { kHSV };
    float r, g, b;
    MColor(float r, float g, float b) : r(r), g(g), b(b) {}
    bool get(Type, float& h, float& s, float& v) const {
        const MStatus status = api("color.get");
        if (status) {
            h = s = 0;
            v = std::max(r, std::max(g, b));
        }
        return bool(status);
    }
};
template <class T> const char* arrayName();
template <> const char* arrayName<MPoint>() {
    return "point";
}
template <> const char* arrayName<MVector>() {
    return "vector";
}
template <> const char* arrayName<MMatrix>() {
    return "matrix";
}
template <> const char* arrayName<float>() {
    return "float";
}
template <> const char* arrayName<double>() {
    return "double";
}
template <> const char* arrayName<MString>() {
    return "string";
}
template <class T> struct Array {
    std::vector<T> values;
    Array() = default;
    explicit Array(unsigned int count, const T& initial = T()) {
        if (setLength(count))
            std::fill(values.begin(), values.end(), initial);
    }
    MStatus setLength(unsigned int count) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".allocate");
        if (status)
            values.resize(index == shortAt && count ? count - 1 : count);
        return status;
    }
    MStatus copy(const Array& source) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".copy");
        if (status) {
            values = source.values;
            if (index == shortAt && !values.empty())
                values.pop_back();
        }
        return status;
    }
    MStatus append(const T& value) {
        const MStatus status = api(std::string(arrayName<T>()) + ".append");
        if (status)
            values.push_back(value);
        return status;
    }
    MStatus set(const T& value, unsigned int index) {
        const MStatus status = api(std::string(arrayName<T>()) + ".set");
        if (status)
            values.at(index) = value;
        return status;
    }
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    T& operator[](unsigned int index) {
        readValue();
        return values.at(index);
    }
    const T& operator[](unsigned int index) const {
        readValue();
        return values.at(index);
    }
};
using MPointArray = Array<MPoint>;
using MVectorArray = Array<MVector>;
using MMatrixArray = Array<MMatrix>;
using MFloatArray = Array<float>;
using MDoubleArray = Array<double>;
using MStringArray = Array<MString>;
using DScalarData = float;
using DScalarDataList = std::vector<DScalarData>;
using double3 = double[3];
struct Input {
    double number = 0;
    double3 vector = {1, 2, 3};
    MString string = "abc";
    bool connected = false, nullObject = false;
    MFn::Type arrayType = MFn::kPointArrayData;
    MFnData::Type type = MFnData::kMesh;
    MTypeId typeId = MTypeId(ID_DP_DATA);
    MPointArray points;
    MVectorArray vectors;
    MMatrixArray matrices;
    MFloatArray floats;
    MDoubleArray doubles;
};
struct CustomData {
    std::size_t count = 3, primitives = 1;
    bool valid = true, checkedValid = true, nullData = false;
    std::map<std::string, std::vector<double>> attributes;
    std::size_t numberOfPoints() const {
        readValue();
        return count;
    }
    bool hasValidPointTopology() const {
        readValue();
        return valid;
    }
    bool hasValidCurveTopology() const {
        readValue();
        return valid;
    }
    bool hasValidMeshTopology() const {
        readValue();
        return valid;
    }
    bool getCheckedTotalCvs(std::size_t& total) const {
        total = checkedValid ? count : 0;
        return checkedValid;
    }
    bool getCheckedTotalVertex(std::size_t& total) const {
        return getCheckedTotalCvs(total);
    }
    std::size_t getTotalCvs() const {
        return checkedValid ? count : 0;
    }
    void getTotalVertex(std::size_t& total) const {
        total = getTotalCvs();
    }
    std::size_t numberOfCurves() const {
        return primitives;
    }
    std::size_t numberOfMesh() const {
        return primitives;
    }
};
struct DPData : CustomData {
    static const MTypeId id;
};
struct DCurveData : CustomData {
    static const MTypeId id;
};
struct DMeshData : CustomData {
    static const MTypeId id;
};
const MTypeId DPData::id(ID_DP_DATA), DCurveData::id(ID_DC_DATA), DMeshData::id(ID_DM_DATA);
namespace {
DPData pointData;
DCurveData curveData;
DMeshData meshData;
} // namespace
struct MDataHandle {
    bool valid = true;
    Input* input = nullptr;
    std::string key;
    void check() const {
        assert(valid);
        readValue();
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    short asShort() const {
        check();
        return static_cast<short>(input->number);
    }
    double3& asDouble3() const {
        check();
        return input->vector;
    }
    MString asString() const {
        check();
        return input->string;
    }
    MFnData::Type type() const {
        check();
        return input->type;
    }
    MTypeId typeId() const {
        check();
        return input->typeId;
    }
    MObject data() {
        check();
        if (input->nullObject) return MObject::kNullObj;
        MObject result(key);
        result.inputOwner = input;
        return result;
    }
    MStatus set(const MObject&) {
        check();
        const MStatus status = api("output.set");
        if (status)
            ++publications;
        return status;
    }
    void setInt64(MInt64 count) {
        check();
        ++publications;
        published = {{{double(count), 0, 0, 0}}};
    }
    void setClean() {
        check();
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attr, MStatus* status = nullptr) {
        const MStatus result = api("input " + attr.name);
        if (status)
            *status = result;
        MDataHandle handle;
        handle.valid = bool(result);
        handle.input = &inputs[attr.name];
        handle.key = attr.name;
        return handle;
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = api("output.handle");
        MDataHandle handle;
        handle.valid = bool(*status);
        return handle;
    }
    MStatus setClean(const struct MPlug&) {
        return api("output.clean");
    }
};
namespace {
MDataBlock* activeData = nullptr;
}
MFn::Type ArrayObject::apiType() const {
    return inputOwner ? inputOwner->arrayType : activeData->inputs[name].arrayType;
}
struct MPlug {
    MObject attr;
    explicit MPlug(const MObject& attr) : attr(attr) {}
    MPlug(const MObject&, const MObject& attr) : attr(attr) {}
    bool operator==(const MObject& other) const {
        return attr.name == other.name;
    }
    bool operator!=(const MObject& other) const {
        return !(*this == other);
    }
    bool isConnected(MStatus* result = nullptr) const {
        const MStatus status = api("connected " + attr.name);
        if (result) *result = status;
        return status && activeData->inputs[attr.name].connected;
    }
    MStatus getValue(MObject& object) const {
        const MStatus status = api("plug " + attr.name);
        if (status)
            object = activeData->inputs[attr.name].nullObject ? MObject::kNullObj : attr;
        return status;
    }
};
MFn::Type getDpointType(const MObject&, const MObject& plug, MObject& object, MStatus& status) {
    status = api("plug " + plug.name);
    if (status)
        object = plug;
    return activeData->inputs[plug.name].arrayType;
}
template <class T> Array<T>& arrayInput(Input& input);
template <> MPointArray& arrayInput<MPoint>(Input& i) {
    return i.points;
}
template <> MVectorArray& arrayInput<MVector>(Input& i) {
    return i.vectors;
}
template <> MMatrixArray& arrayInput<MMatrix>(Input& i) {
    return i.matrices;
}
template <> MFloatArray& arrayInput<float>(Input& i) {
    return i.floats;
}
template <> MDoubleArray& arrayInput<double>(Input& i) {
    return i.doubles;
}
std::array<double, 4> components(float value) {
    return {{value, 0, 0, 0}};
}
std::array<double, 4> components(const MVector& value) {
    return {{value.x, value.y, value.z, 0}};
}
template <class T> struct ArrayData {
    MObject object;
    ArrayData() = default;
    ArrayData(const MObject& object, MStatus* status) : object(object) {
        *status = api(std::string(arrayName<T>()) + ".bind");
    }
    Array<T> array(MStatus* status) const {
        *status = api(std::string(arrayName<T>()) + ".source");
        return *status ? arrayInput<T>(object.inputOwner ? *object.inputOwner :
                                     activeData->inputs[object.name]) : Array<T>();
    }
    MObject create(const Array<T>& values, MStatus* status) {
        *status = api(std::string(arrayName<T>()) + ".create");
        if (*status) {
            published.clear();
            for (const T& value : values.values)
                published.push_back(components(value));
        }
        return {"result"};
    }
};
using MFnPointArrayData = ArrayData<MPoint>;
using MFnVectorArrayData = ArrayData<MVector>;
using MFnMatrixArrayData = ArrayData<MMatrix>;
using MFnFloatArrayData = ArrayData<float>;
using MFnDoubleArrayData = ArrayData<double>;
struct MFnStringArrayData {
    MObject create(const MStringArray& values, MStatus* status) {
        *status = api("string.create");
        if (*status)
            for (const auto& value : values.values)
                publishedStrings.push_back(value.asChar());
        return {"result"};
    }
};
struct MFnStringData {
    MObject create(const char* text, MStatus* status = nullptr) {
        const MStatus result = operation(std::string("string default ") + text);
        if (status)
            *status = result;
        return result ? MObject{"default"} : MObject::kNullObj;
    }
};
struct MFnNurbsCurve {
    MObject object;
    MFnNurbsCurve(const MObject& object, MStatus* status) : object(object) {
        *status = api("curve.bind");
    }
    unsigned int numCVs(MStatus* status) {
        *status = api("curve.count");
        return *status ? 3 : 0;
    }
};
struct MFnNurbsSurface {
    MFnNurbsSurface(const MObject&, MStatus* status) {
        *status = api("surface.bind");
    }
    MStatus getCVs(MPointArray& values) {
        const MStatus status = api("surface.cvs");
        if (status)
            values.values.resize(6);
        return status;
    }
};
struct MFnMesh {
    MFnMesh(const MObject&, MStatus* status) {
        *status = api("mesh.bind");
    }
    int numVertices(MStatus* status) {
        *status = api("mesh.count");
        return *status ? 4 : 0;
    }
};
namespace dommy {
namespace maya {
template <class Data>
MStatus getPluginDataFromHandle(const MDataHandle&, const MTypeId&, const Data*& result);
template <>
MStatus getPluginDataFromHandle(const MDataHandle&, const MTypeId&, const DPData*& result) {
    const MStatus s = api("custom.points");
    result = pointData.nullData ? nullptr : &pointData;
    return s && !result ? MStatus(MS::kFailure) : s;
}
template <>
MStatus getPluginDataFromHandle(const MDataHandle&, const MTypeId&, const DCurveData*& result) {
    const MStatus s = api("custom.curves");
    result = curveData.nullData ? nullptr : &curveData;
    return s && !result ? MStatus(MS::kFailure) : s;
}
template <>
MStatus getPluginDataFromHandle(const MDataHandle&, const MTypeId&, const DMeshData*& result) {
    const MStatus s = api("custom.meshes");
    result = meshData.nullData ? nullptr : &meshData;
    return s && !result ? MStatus(MS::kFailure) : s;
}
} // namespace maya
} // namespace dommy
void getMDoubleArrayFromAttribute(const DPData& data, const std::string& name,
                                  MDoubleArray& output) {
    const MStatus status = api("attribute " + name);
    if (!status)
        throw std::runtime_error("attribute reader failure");
    auto found = data.attributes.find(name);
    if (found == data.attributes.end())
        return;
    const int index = callCount;
    if (!output.setLength(output.length()))
        throw std::runtime_error("attribute allocation failure");
    if (index == shortAt)
        return;
    for (unsigned int i = 0; i < output.length() && i < found->second.size(); ++i)
        output[i] = found->second[i];
}
MStatus DOutputDataMayaVectorArray(MDataBlock& data, const MPlug& plug, const MObject& attr,
                                   const MVectorArray& values) {
    MStatus status;
    MDataHandle output = data.outputValue(attr, &status);
    if (!status)
        return status;
    MFnVectorArrayData functionSet;
    MObject object = functionSet.create(values, &status);
    if (!status)
        return status;
    status = output.set(object);
    if (!status)
        return status;
    output.setClean();
    return data.setClean(plug);
}
MStatus getDoubleArrayFromData(const MObject&, const MObject&, MDoubleArray&);
MStatus getDoubleArrayFromData(const MObject&, const MObject&, DScalarDataList&);

// Archived-method compatibility only.
const DPData* getInputDP(const MObject&, MDataBlock&) {
    return pointData.nullData ? nullptr : &pointData;
}
const DCurveData* getInputDC(const MObject&, MDataBlock&) {
    return curveData.nullData ? nullptr : &curveData;
}
const DMeshData* getInputDM(const MObject&, MDataBlock&) {
    return meshData.nullData ? nullptr : &meshData;
}
void EchoWarning(const char*, const char*) {}
struct MFnDependencyNode {
    MObject node;
    explicit MFnDependencyNode(const MObject& node) : node(node) {}
    MPlug findPlug(const MObject& attr, bool) {
        return MPlug(node, attr);
    }
    MObject object() const {
        return node;
    }
};
