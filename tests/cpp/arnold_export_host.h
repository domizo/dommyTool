#pragma once

// Failure-injectable host shapes exercise source control flow and ownership.
// Matrix/plug/data behavior here does not establish native Maya compatibility.
#include <cassert>
#include <cmath>
#include <exception>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
#include <ImathVec.h>
#include <maya/MStatus.h>
#include "arnold_export_api.h"
#include "common/string/DStreamFormat.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"

using uint = unsigned int;
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;

#ifdef DOMMY_SDK_CHECK
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnPluginData.h>
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MDagPath.h>
#include <maya/MDoubleArray.h>
#include <maya/MIntArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MPoint.h>
#include <maya/MBoundingBox.h>
#include <maya/MPlug.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVectorArray.h>
#include <maya/MTypeId.h>
#include "common/utility/DComputeGuard.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DMayaValueValidation.h"
#else
#define CHECK_MSTATUS(expression) do { const MStatus status = (expression); (void)status; } while (false)
int failAttribute = -1, failOperation = -1, throwOperation = -1;
MS::MStatusCode injectedApiStatus = MS::kInvalidParameter;
int exceptionKind = 0, reports = 0, publications = 0, cleanCalls = 0;
int writeResult = AI_SUCCESS, writes = 0, sessions = 0, liveNodes = 0, liveArrays = 0;
bool connected = true, validInput = true, fileExists = false, pathValid = true;
bool lastBinary = false;
std::string outputInfo;
std::vector<std::string> operations;
std::vector<float> numericSubmissions;
void api(const char* name) {
    operations.push_back(name);
    if (int(operations.size()) - 1 == throwOperation) {
        if (exceptionKind) throw std::bad_alloc();
        throw std::runtime_error("injected export failure");
    }
}
MStatus apiStatus(const char* name) {
    api(name);
    return int(operations.size()) - 1 == failOperation
        ? injectedApiStatus : MS::kSuccess;
}
void number(float value) { assert(std::isfinite(value)); numericSubmissions.push_back(value); }
using MTypeId = int;
using double2 = double[2];
using double3 = double[3];
struct MObject {
    int id;
    explicit MObject(int value = 0) : id(value) {}
    bool isNull() const { return id == 0; }
};
struct MPlug {
    MObject attribute;
    explicit MPlug(MObject value) : attribute(value) {}
    MPlug(MObject, MObject value) : attribute(value) { api("plug.construct"); }
    bool isConnected(MStatus* status = nullptr) const {
        const MStatus queryStatus = apiStatus("plug.connected");
        if (status) *status = queryStatus;
        // The boolean must not be consumed when the API reports failure.
        return connected;
    }
    bool isNull() const { api("plug.null"); return false; }
    bool operator==(MObject value) const { return attribute.id == value.id; }
    bool operator!=(MObject value) const { return !(*this == value); }
};
struct MDagPath {
    MObject node(MStatus* status = nullptr) const {
        const MStatus result = apiStatus("path.node");
        if (status) *status = result;
        return MObject(1);
    }
};
struct MStringArray;
struct MString {
    std::string text;
    MString(const char* value = "") : text(value) {}
    const char* asChar() const { return text.c_str(); }
    uint length() const { return uint(text.size()); }
    MStatus split(char, MStringArray&) const;
};
template <typename T> struct Array {
    std::vector<T> values;
    explicit Array(std::size_t count = 0) : values(count) {}
    Array(std::size_t count, const T& value) : values(count, value) {}
    uint length() const { return uint(values.size()); }
    T& operator[](std::size_t index) { assert(index < values.size()); return values[index]; }
    const T& operator[](std::size_t index) const { assert(index < values.size()); return values[index]; }
    MStatus append(const T& value) {
        const MStatus status = apiStatus("array.append");
        if (status) values.push_back(value);
        return status;
    }
    MStatus setLength(uint count) {
        const MStatus status = apiStatus("array.resize");
        if (status) values.resize(count);
        return status;
    }
    MStatus copy(const Array& source) {
        const MStatus status = apiStatus("array.copy");
        if (status) values = source.values;
        return status;
    }
    void clear() { values.clear(); }
};
struct MStringArray : Array<MString> {};
MStatus MString::split(char delimiter, MStringArray& result) const {
    const MStatus status = apiStatus("string.split");
    std::istringstream stream(text); std::string token;
    while (std::getline(stream, token, delimiter))
        if (!token.empty()) result.values.push_back(MString(token.c_str()));
    return status; // A failure may still leave a partial result.
}
struct MQuaternion { double x = 0, y = 0, z = 0, w = 1; };
struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double operator[](std::size_t axis) const { return axis == 0 ? x : axis == 1 ? y : z; }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    MQuaternion rotateTo(const MVector&) const { api("vector.rotate"); return {}; }
    MVector operator+(const MVector& b) const { return {x+b.x, y+b.y, z+b.z}; }
    MVector operator-(const MVector& b) const { return {x-b.x, y-b.y, z-b.z}; }
};
struct MPoint {
    double x, y, z, w = 1;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
};
struct MBoundingBox {
    MPoint first, second;
    MBoundingBox(MPoint a = {}, MPoint b = {}) : first(a), second(b) {}
    MPoint min() const { return first; }
    MPoint max() const { return second; }
    double width() const { return second.x-first.x; }
    double height() const { return second.y-first.y; }
    double depth() const { return second.z-first.z; }
};
struct MMatrix {
    double cells[4][4]{};
    MMatrix() { for (uint i = 0; i < 4; ++i) cells[i][i] = 1; }
    double* operator[](uint row) { return cells[row]; }
    const double* operator[](uint row) const { return cells[row]; }
    MMatrix operator*(const MMatrix& other) const {
        MMatrix result;
        for (uint i = 0; i < 4; ++i) for (uint j = 0; j < 4; ++j) {
            result[i][j] = 0;
            for (uint k = 0; k < 4; ++k) result[i][j] += cells[i][k]*other[k][j];
        }
        return result;
    }
};
namespace MSpace { enum Space { kWorld }; }
struct MTransformationMatrix {
    MMatrix matrix;
    MTransformationMatrix(const MMatrix& value = {}) : matrix(value) {}
    MMatrix asMatrix() const { api("transform.matrix"); return matrix; }
    MStatus addScale(const double* value, MSpace::Space) {
        const MStatus status = apiStatus("transform.scale"); if (!status) return status;
        for (uint i = 0; i < 3; ++i) matrix[i][i] *= value[i];
        return MS::kSuccess;
    }
    MStatus addTranslation(const MVector& value, MSpace::Space) {
        const MStatus status = apiStatus("transform.translate"); if (!status) return status;
        matrix[3][0] += value.x;
        matrix[3][1] += value.y; matrix[3][2] += value.z; return MS::kSuccess;
    }
    MStatus addRotationQuaternion(double, double, double, double, MSpace::Space) {
        return apiStatus("transform.rotation");
    }
    MVector getTranslation(MSpace::Space, MStatus* status = nullptr) const {
        const MStatus result = apiStatus("transform.getTranslation"); if (status) *status = result;
        return {matrix[3][0],matrix[3][1],matrix[3][2]};
    }
};
using MDoubleArray = Array<double>;
using MIntArray = Array<int>;
using MVectorArray = Array<MVector>;
using MMatrixArray = Array<MMatrix>;
struct Value {
    double scalar = 0;
    double3 components{0,0,0};
    MString string;
};
struct MDataHandle {
    Value* value;
    explicit MDataHandle(Value* source = nullptr) : value(source) {}
    bool asBool() const { return value->scalar != 0; }
    short asShort() const { return short(value->scalar); }
    int asInt() const { return int(value->scalar); }
    double asDouble() const { return value->scalar; }
    float asFloat() const { return float(value->scalar); }
    MVector asFloatVector() const { return {value->components[0],value->components[1],value->components[2]}; }
    const double2& asDouble2() const { return *reinterpret_cast<const double2*>(value->components); }
    const double3& asDouble3() const { return value->components; }
    MString asString() const { return value->string; }
    void set(const MString& text) {
        api("output.string"); ++publications; outputInfo = text.text;
        value->string = text;
    }
    void set(bool state) { api("output.bool"); ++publications; value->scalar = state ? 1 : 0; }
};
struct MDataBlock {
    std::map<int, Value> values;
    MDataHandle inputValue(MObject attr, MStatus* status = nullptr) {
        api("input"); if (status) *status = attr.id == failAttribute ? injectedApiStatus : MS::kSuccess;
        return MDataHandle(&values[attr.id]);
    }
    MDataHandle outputValue(MObject attr, MStatus* status = nullptr) {
        const MStatus result = apiStatus("output"); if (status) *status = result;
        return MDataHandle(&values[attr.id]);
    }
    MStatus setClean(MObject) { ++cleanCalls; return apiStatus("clean"); }
    MStatus setClean(const MPlug& plug) { return setClean(plug.attribute); }
};
struct MTime { double value() const { return 1; } };
struct MFileObject {
    MString path;
    MStatus setRawFullName(const char* text) {const auto status=apiStatus("file.path");path=text;return status;}
    MString resolvedFullName() const {return path;}
    bool exists() const { api("file.exists"); return fileExists; }
};
struct MGlobal {
    static void displayWarning(const char*) { api("warning"); }
    static void displayError(const char*) { api("error"); }
};
#endif

struct DPAttributeData {
    using ConstPtr = std::shared_ptr<const DPAttributeData>;
    virtual ~DPAttributeData() = default;
    virtual std::size_t attrSize() const = 0;
    std::string type;
    std::string typeName() const { return type; }
};
template <typename T> struct Attribute final : DPAttributeData {
    std::vector<T> values;
    std::size_t attrSize() const override { return values.size(); }
};
struct AttributeOwner {
    std::map<std::string, DPAttributeData::ConstPtr> attrs;
    bool isExistAttr(const std::string& name) const { return attrs.count(name) != 0; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const { return attrs.at(name); }
    std::string getAttrTypeName(const std::string& name) const { return getAttr(name)->type; }
    std::string getAttributeType(const std::string& name) const { return getAttrTypeName(name); }
    std::size_t numberOfAttributes() const { return attrs.size(); }
    auto beginAttr() const -> decltype(attrs.begin()) { return attrs.begin(); }
    auto endAttr() const -> decltype(attrs.end()) { return attrs.end(); }
    std::vector<std::string> getAllAttributeList() const {
        std::vector<std::string> result;
        for (const auto& value : attrs) result.push_back(value.first);
        return result;
    }
    template <typename T> std::vector<T> attrListValue(const std::string& name) const {
        const auto field = std::dynamic_pointer_cast<const Attribute<T>>(getAttr(name));
        assert(field); return field->values;
    }
    template <typename T> void field(const std::string& name, const char* type, std::vector<T> values) {
        auto attribute = std::make_shared<Attribute<T>>();
        attribute->type = type; attribute->values = std::move(values); attrs[name] = attribute;
    }
};
struct Group { std::size_t numberOfIds() const { return 0; } };
struct DPData : AttributeOwner {
    static const MTypeId id;
    bool topology = true;
    bool hasValidPointTopology() const { return topology; }
    DPointList points{DPoint(1,2,3)};
    std::map<std::string,std::shared_ptr<Group>> groups;
    uint numberOfPoints() const { return uint(points.size()); }
    std::size_t numberOfAttributes() const { return attrs.size(); }
    std::size_t numberOfGroups() const { return groups.size(); }
    auto beginAttr() const -> decltype(attrs.begin()) { return attrs.begin(); }
    auto endAttr() const -> decltype(attrs.end()) { return attrs.end(); }
    auto beginGrp() const -> decltype(groups.begin()) { return groups.begin(); }
    auto endGrp() const -> decltype(groups.end()) { return groups.end(); }
    std::vector<std::string> getAllGroupList() const {
        std::vector<std::string> result;
        for (const auto& value : groups) result.push_back(value.first);
        return result;
    }
    const DPointList& getPositionConst() const { return points; }
    const DPoint& positionAt(std::size_t i) const { return points.at(i); }
    void setDPData(const DPData& value) { *this = value; }
    MBoundingBox getBoundingBox() const;
};
struct DCurveInternalData : AttributeOwner {
    DPointList points{DPoint(0,0,0),DPoint(1,2,3),DPoint(2,3,4),DPoint(3,4,5)};
    std::size_t numberOfPoints() const { return points.size(); }
    const DPointList* getPositionPtrConst() const { return &points; }
};
struct DCurveData : AttributeOwner {
    static const MTypeId id;
    bool topology = true;
    bool hasValidCurveTopology() const { return topology; }
    std::vector<std::shared_ptr<DCurveInternalData>> curves{std::make_shared<DCurveInternalData>()};
    std::map<std::string,std::shared_ptr<Group>> groups;
    std::vector<std::string> getAllGroupList() const {
        std::vector<std::string> result;
        for (const auto& value : groups) result.push_back(value.first);
        return result;
    }
    std::size_t getTotalCvs() const { return curves.size()*4U; }
    std::size_t numberOfPointAttribute(bool) const { return 0; }
    std::vector<std::string> getPointAttributeInfo() const { return {}; }
    std::size_t numberOfGroups() const { return groups.size(); }
    auto beginGrp() const -> decltype(groups.begin()) { return groups.begin(); }
    auto endGrp() const -> decltype(groups.end()) { return groups.end(); }
    std::size_t numberOfCompGroup(int) const { return 0; }
    std::vector<std::string> getCompGroupInfo(int) const { return {}; }
    std::size_t numberOfCurves() const { return curves.size(); }
    auto beginCurves() const -> decltype(curves.begin()) { return curves.begin(); }
    auto endCurves() const -> decltype(curves.end()) { return curves.end(); }
    MStatus copyFrom(const DCurveData& other) { *this = other; return MS::kSuccess; }
    MBoundingBox getBoundingBox() const;
};
const DPData* getInputDP(const MObject&, MDataBlock&);
MStatus getValidInputDP(const MObject&, MDataBlock&, const DPData*&);
const DPData* getInputDPfromPlug(const MPlug&, MObject&);
const DCurveData* getInputDC(const MObject&, MDataBlock&);
MStatus getValidInputDC(const MObject&, MDataBlock&, const DCurveData*&);
void getMDoubleArrayFromAttribute(const DPData&, const std::string&, MDoubleArray&);
void getMIntArrayFromAttribute(const DPData&, const std::string&, MIntArray&);
void getMVectorArrayFromAttribute(const DPData&, const std::string&, MVectorArray&);
void getMVector2DArrayFromAttribute(const DPData&, const std::string&, MVectorArray&);
void getMMatrixArrayFromAttribute(const DPData&, const std::string&, MMatrixArray&);
bool getCheckedMRealArrayFromAttribute(const DPData&, const std::string&, std::size_t, MDoubleArray&);
bool getCheckedMVectorArrayFromAttribute(const DPData&, const std::string&, std::size_t, MVectorArray&);
bool getCheckedMMatrixArrayFromAttribute(const DPData&, const std::string&, std::size_t, MMatrixArray&);

#ifndef DOMMY_SDK_CHECK
// These full-callback tests keep attribute storage/array copying as explicit
// host seams. The separate renderer-control suite executes the real accessors.
inline bool getCheckedMRealArrayFromAttribute(const DPData& points,const std::string& name,std::size_t count,MDoubleArray& output) {
    const auto field=points.getAttr(name);
    if(!field || field->attrSize()!=count)return false;
    const auto& type=field->type;
    if(type!="int" && type!="uint32" && type!="int64" && type!="float" && type!="double")return false;
    getMDoubleArrayFromAttribute(points,name,output);return output.length()==count;
}
inline bool getCheckedMVectorArrayFromAttribute(const DPData& points,const std::string& name,std::size_t count,MVectorArray& output) {
    const auto field=points.getAttr(name);
    if(!field || field->attrSize()!=count)return false;
    if(field->type!="vec3" && field->type!="vec3i" && field->type!="vec3f" && field->type!="vec3d")return false;
    getMVectorArrayFromAttribute(points,name,output);return output.length()==count;
}
inline bool getCheckedMMatrixArrayFromAttribute(const DPData& points,const std::string& name,std::size_t count,MMatrixArray& output) {
    const auto field=points.getAttr(name);
    if(!field || field->attrSize()!=count || field->type!="matrix")return false;
    getMMatrixArrayFromAttribute(points,name,output);return output.length()==count;
}
#endif
bool getSequenceFileName(const MString&, double, MString&);
void EchoWarning(const char*, const char*);
void EchoWarning(const std::string&, const char*);
void EchoInfo(const char*, const char*);

#ifndef DOMMY_SDK_CHECK
struct MFnPluginData {
    std::unique_ptr<DPData> point;
    std::unique_ptr<DCurveData> curve;
    void create(MTypeId id, MStatus* status) {
        *status = apiStatus("data.create");
        if (id == DPData::id) point.reset(new DPData); else curve.reset(new DCurveData);
    }
    MObject object() const { return MObject(1); }
};
#endif
namespace dommy { namespace maya {
MStatus getEvaluationTime(MDataBlock&, MTime&);
template <typename T> MStatus getPluginData(const MFnPluginData&, T*&);
template <typename T> MStatus getPluginDataFromHandle(const MDataHandle&, MTypeId, const T*&);
template <typename T> MStatus publishPluginDataObject(MDataHandle&, const MObject&, const T&);
template <typename T> MStatus publishTypedPluginDataObject(MDataHandle&, const MObject&, MTypeId);
#ifndef DOMMY_SDK_CHECK
MStatus reportComputeException(const char*, const std::exception_ptr&) noexcept { ++reports; return MS::kFailure; }
void reportViewportCallbackException(const char*, const char*, const std::exception_ptr&) noexcept { ++reports; }
inline bool isFiniteVector(const MVector& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
#endif
}}
