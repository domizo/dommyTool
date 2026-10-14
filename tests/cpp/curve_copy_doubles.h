#pragma once

// Failure-injectable host boundaries. Curves/surfaces are simple parameter
// traces, and rotations are payload transport, not a Maya geometry emulator.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaCurveKnots.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DCurveSampling.h"
#include "math/DFrameAlignment.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCurveBatchBudget.h"
#include "common/utility/DCurveEvaluation.h"
#include "modules/curve/DCurve/Utility/DCurveCopyBudget.h"
#include "modules/curve/DCurve/Utility/DCurveCopyMath.h"
#include <maya/MMatrix.h>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#ifdef DOMMY_TYPED_PATH_ATTRIBUTES
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
static bool rejectPointAttribute = false;
#endif
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
static bool rejectTypedAttribute = false;
#endif

#define CHECK_MSTATUS_AND_RETURN_IT(x) DOMMY_RETURN_IF_FAILURE(x)
#define DOMMY_ENABLE_DEBUG_LOGS 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
using uint = unsigned int;
using float2 = float[2];
using float3 = float[3];
using double3 = double[3];
using double2 = double[2];
using DScalarDataList = std::vector<float>;
constexpr double kMFnNurbsEpsilon = 1e-3;
namespace MSpace {
enum Space { kObject, kWorld };
}
namespace {
int throwAt = -1, shortAt = -1, published = 0, cleaned = 0;
bool shortApplied = false;
MStatus api(const std::string& name) {
    const int index = callCount;
    const auto status = operation(name);
    if (index == throwAt)
        throw std::runtime_error("injected failure");
    return status;
}
void reset(int fail = -1) {
    resetCalls(fail);
    throwAt = shortAt = -1;
    published = cleaned = 0;
    shortApplied = false;
}
} // namespace
struct MQuaternion {
    double x = 0, y = 0, z = 0, w = 1;
};
struct MVector {
    static const MVector zero, xAxis, yAxis;
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    explicit MVector(const float* v) : x(v[0]), y(v[1]), z(v[2]) {}
    explicit MVector(const double* v) : x(v[0]), y(v[1]), z(v[2]) {}
    double& operator[](unsigned int i) {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    const double& operator[](unsigned int i) const {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    MStatus normalize() {
        const double n = length();
        if (n > 1e-10) {
            x /= n;
            y /= n;
            z /= n;
        }
        return MS::kSuccess;
    }
    MQuaternion rotateTo(const MVector&) const {
        return MQuaternion();
    }
    MVector rotateBy(const MQuaternion&) const {
        return *this;
    }
    MVector rotateBy(const double* r, int) const {
        return {x + r[0], y + r[1], z + r[2]};
    }
    MVector operator^(const MVector& v) const {
        return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
    }
    double operator*(const MVector& v) const {
        return x * v.x + y * v.y + z * v.z;
    }
    MVector operator*(double s) const {
        return {x * s, y * s, z * s};
    }
    MVector operator-(const MVector& v) const {
        return {x - v.x, y - v.y, z - v.z};
    }
    MVector operator+(const MVector& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }
    MVector operator-() const {
        return {-x, -y, -z};
    }
    MVector& operator-=(const MVector& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    MVector& operator/=(double s) {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
};
const MVector MVector::zero{0, 0, 0};
const MVector MVector::xAxis{1, 0, 0};
const MVector MVector::yAxis{0, 1, 0};
struct MPoint : MVector {
    double w = 1;
    using MVector::MVector;
    MPoint() = default;
    MPoint(const MVector& v) : MVector(v) {}
    MPoint operator*(const MMatrix& m) const {
        return {x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0],
                x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1],
                x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2]};
    }
    MVector operator-(const MPoint& p) const {
        return {x - p.x, y - p.y, z - p.z};
    }
    MPoint operator+(const MVector& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }
    double distanceTo(const MPoint& p) const {
        return (*this - p).length();
    }
    bool operator!=(const MPoint& p) const {
        return x != p.x || y != p.y || z != p.z;
    }
    void setCast(const MVector& v) {
        x = v.x;
        y = v.y;
        z = v.z;
    }
};
using MFloatPoint = MPoint;
struct MTransformationMatrix {
    enum RotationOrder { kXYZ, kYZX };
    MMatrix matrix;
    MStatus addRotationQuaternion(double, double, double, double, MSpace::Space) {
        return api("transform.rotation");
    }
    MStatus addTranslation(const MVector& p, MSpace::Space) {
        const auto status = api("transform.translation");
        if (status) {
            matrix[3][0] = p.x;
            matrix[3][1] = p.y;
            matrix[3][2] = p.z;
        }
        return status;
    }
    MMatrix asMatrix() const {
        return matrix;
    }
};
template <class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    T& operator[](std::size_t i) {
        return this->at(i);
    }
    const T& operator[](std::size_t i) const {
        return this->at(i);
    }
    unsigned int length() const {
        return this->size();
    }
    MStatus append(const T& value) {
        const auto status = api("array.append");
        if (status)
            this->push_back(value);
        return status;
    }
    MStatus setLength(unsigned n) {
        const int index = callCount;
        const auto status = api("array.resize");
        if (status) {
            this->resize(n && index == shortAt ? n - 1 : n);
            if (n && index == shortAt)
                shortApplied = true;
        }
        return status;
    }
    MStatus copy(const Array& a) {
        const int index = callCount;
        const auto status = api("array.copy");
        if (status) {
            *this = a;
            if (index == shortAt && this->size()) {
                this->pop_back();
                shortApplied = true;
            }
        }
        return status;
    }
    MStatus clear() {
        const auto status = api("array.clear");
        if (status)
            std::vector<T>::clear();
        return status;
    }
};
using MPointArray = Array<MPoint>;
using MDoubleArray = Array<double>;
using MVectorArray = Array<MVector>;
using MDoubleArrayPtr = std::shared_ptr<MDoubleArray>;
void GenerateDCurveKnots(const size_t& n, const int& d, MDoubleArray& knots, bool loop = false) {
    if (!dommy::maya::buildCurveKnots(n, d, loop, knots))
        throw std::runtime_error("knots failed");
}
struct Geometry {
    MPointArray points = {{0, 0, 0}, {1, 1, 0}, {2, 1, 1}, {3, 0, 1}};
    double start = 5, end = 9, startV = -4, endV = -2;
    MMatrix matrix;
    MVector tangent{1, 0, 0}, normal{0, 1, 0};
    double length = 4;
    int degree = 3, reportedCvCount = -1;
    bool open = true;
    bool mapClosestToDomain = false;
    // Optional straight-line fixture for transformed geometry, not NURBS.
    bool linearSamples = false;
};
namespace {
std::map<std::string, Geometry> geometries;
}
struct MFnNurbsCurve {
    enum Form { kOpen, kClosed };
    MObject object;
    MFnNurbsCurve() = default;
    MFnNurbsCurve(MObject o, MStatus* s) {
        *s = setObject(o);
    }
    MStatus setObject(MObject o) {
        const auto s = api("curve.bind");
        if (!s)
            return s;
        assert(!o.isNull() && geometries.count(o.name));
        object = o;
        return s;
    }
    int degree(MStatus* s) const {
        *s = api("curve.degree");
        return geometries.at(object.name).degree;
    }
    Form form(MStatus* s) const {
        *s = api("curve.form");
        return geometries.at(object.name).open ? kOpen : kClosed;
    }
    MObject rebuild(int, unsigned int, short, short, bool, bool, bool, double, MStatus*);
    int numCVs(MStatus* s) const {
        *s = api("curve.cvCount");
        const auto& g = geometries.at(object.name);
        return g.reportedCvCount < 0 ? int(g.points.length()) : g.reportedCvCount;
    }
    MStatus getCVs(MPointArray& p, MSpace::Space space) const {
        assert(space == MSpace::kObject);
        const int index = callCount;
        const auto s = api("curve.cvs");
        if (s) {
            p = geometries.at(object.name).points;
            if (index == shortAt && p.size()) {
                p.pop_back();
                shortApplied = true;
            }
        }
        return s;
    }
    MStatus setCVs(const MPointArray& p, MSpace::Space space) {
        assert(space == MSpace::kObject);
        const auto s = api("curve.setCVs");
        if (s)
            geometries.at(object.name).points = p;
        return s;
    }
    MStatus updateCurve() {
        return api("curve.update");
    }
    double sampleLength() const {
        const auto& g = geometries.at(object.name);
        return g.linearSamples ? g.points.front().distanceTo(g.points.back()) : g.length;
    }
    MStatus getKnots(MDoubleArray& k) const {
        const int index = callCount;
        const auto s = api("curve.knots");
        if (s) {
            const auto& g = geometries.at(object.name);
            k.assign(g.points.size() + g.degree - 1, 1);
            for (unsigned i = 0; i < unsigned(g.degree) && i < k.size(); ++i)
                k[i] = 0;
            if (index == shortAt && k.size()) {
                k.pop_back();
                shortApplied = true;
            }
        }
        return s;
    }
    MStatus getKnotDomain(double& a, double& b) const {
        const auto s = api("curve.domain");
        if (s) {
            a = geometries.at(object.name).start;
            b = geometries.at(object.name).end;
        }
        return s;
    }
    double length(double, MStatus* s) const {
        *s = api("curve.length");
        return sampleLength();
    }
    double findParamFromLength(double distance, MStatus* s) const {
        *s = api("curve.parameter");
        const auto& g = geometries.at(object.name);
        return g.start + (g.end - g.start) * distance / sampleLength();
    }
    MStatus getPointAtParam(double t, MPoint& p, MSpace::Space space) const {
        assert(space == MSpace::kObject && std::isfinite(t));
        const auto s = api("curve.point");
        if (s) {
            const auto& g = geometries.at(object.name);
            p = g.linearSamples ? g.points.front() + (g.points.back() - g.points.front()) *
                                                         ((t - g.start) / (g.end - g.start))
                                : MPoint(t, 0, 0) * g.matrix;
        }
        return s;
    }
    MVector tangent(double, MSpace::Space space, MStatus* s) const {
        assert(space == MSpace::kObject);
        *s = api("curve.tangent");
        const auto& g = geometries.at(object.name);
        MVector value = g.linearSamples ? g.points.back() - g.points.front() : g.tangent;
        if (g.linearSamples)
            value.normalize();
        return value;
    }
    MVector normal(double, MSpace::Space space, MStatus* s) const {
        assert(space == MSpace::kObject);
        *s = api("curve.normal");
        return geometries.at(object.name).normal;
    }
    MPoint closestPoint(const MPoint& p, double* t, double, MSpace::Space, MStatus* s) const {
        *s = api("curve.closest");
        if (*s) {
            const auto& g = geometries.at(object.name);
            *t = g.mapClosestToDomain ? g.start + (g.end - g.start) * (p.x - 5.0) / 4.0 : p.x;
        }
        return p;
    }
    double findLengthFromParam(double p, MStatus* s) const {
        *s = api("curve.partialLength");
        const auto& g = geometries.at(object.name);
        return (p - g.start) / (g.end - g.start) * sampleLength();
    }
};
struct MFnNurbsSurface {
    MObject object;
    MStatus setObject(MObject o) {
        const auto s = api("surface.bind");
        if (!s)
            return s;
        assert(!o.isNull() && geometries.count(o.name));
        object = o;
        return s;
    }
    MStatus getKnotDomain(double& a, double& b, double& c, double& d) const {
        const auto s = api("surface.domain");
        if (s) {
            const auto& g = geometries.at(object.name);
            a = g.start;
            b = g.end;
            c = g.startV;
            d = g.endV;
        }
        return s;
    }
    MStatus getDerivativesAtParm(double u, double v, MPoint& p, MVector& a, MVector& b,
                                 MSpace::Space space) const {
        assert(space == MSpace::kObject && std::isfinite(u) && std::isfinite(v));
        const auto s = api("surface.derivatives");
        if (s) {
            p = MPoint(u, v, 0) * geometries.at(object.name).matrix;
            a = geometries.at(object.name).tangent;
            b = {0, 0, 1};
        }
        return s;
    }
    MVector normal(double, double, MSpace::Space, MStatus* s) const {
        *s = api("surface.normal");
        return geometries.at(object.name).normal;
    }
    MStatus getTangents(double, double, MVector& u, MVector& v, MSpace::Space space) const {
        assert(space == MSpace::kObject);
        const auto s = api("surface.tangents");
        if (s) {
            u = geometries.at(object.name).tangent;
            v = {0, 0, 1};
        }
        return s;
    }
    MPoint closestPoint(const MPoint& p, double* u, double* v, bool ignore, double,
                        MSpace::Space space, MStatus* s) const {
        assert(!ignore && space == MSpace::kObject);
        *s = api("surface.closest");
        if (*s) {
            *u = p.x;
            *v = p.y;
        }
        return p;
    }
    MStatus getPointAtParam(double u, double v, MPoint& p) const {
        const auto s = api("surface.point");
        if (s)
            p = {u, v, 0};
        return s;
    }
};
struct Input {
    double number = 0;
    float vec[3] = {0, 0, 0};
    double dvec[3] = {0, 0, 0};
    std::string string;
    const void* plugin = nullptr;
    bool pluginTypeMatches = true;
    bool connected = false;
    MFnData::Type type = MFnData::kNurbsCurve;
    MObject object{"source"};
    MMatrix matrix;
};
namespace {
std::map<std::string, Input> inputs;
}
struct MDataHandle {
    Input* input = nullptr;
    bool valid = false;
    void check() const {
        assert(valid && input);
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    short asShort() const {
        check();
        return static_cast<short>(input->number);
    }
    bool asBool() const {
        check();
        return input->number != 0;
    }
    float asFloat() const {
        check();
        return input->number;
    }
    double asDouble() const {
        check();
        return input->number;
    }
    const double3& asDouble3() const {
        check();
        return input->dvec;
    }
    MVector asVector() const {
        check();
        return MVector(input->dvec);
    }
    MVector asFloatVector() const {
        check();
        return MVector(input->vec);
    }
    const double2& asDouble2() const {
        check();
        return *reinterpret_cast<const double2*>(input->dvec);
    }
    MString asString() const {
        check();
        return input->string.c_str();
    }
    const float2& asFloat2() const {
        check();
        return *reinterpret_cast<const float2*>(input->vec);
    }
    const float3& asFloat3() const {
        check();
        return input->vec;
    }
    MFnData::Type type() const {
        check();
        return input->type;
    }
    MMatrix geometryTransformMatrix() const {
        check();
        return input->matrix;
    }
    MObject transformed() const {
        check();
        if (input->object.isNull())
            return MObject::kNullObj;
        const MObject result{input->object.name + ".transformed"};
        auto candidate = geometries.at(input->object.name);
        candidate.matrix = input->matrix;
        for (auto& p : candidate.points)
            p = p * input->matrix;
        geometries[result.name] = candidate;
        return result;
    }
    MObject asNurbsCurveTransformed() const {
        return transformed();
    }
    MObject asNurbsSurfaceTransformed() const {
        return transformed();
    }
    MObject asMeshTransformed() const;
    MObject data() const {
        check();
        return input->object;
    }
    void setClean() {
        assert(valid);
        ++cleaned;
    }
};
struct MPlug {
    std::string attribute;
    MPlug(MObject, MObject a) : attribute(a.name) {}
    bool isConnected(MStatus* status = nullptr) const {
        const MStatus result = api("plug.connected " + attribute);
        if (status) *status = result;
        return result == MS::kSuccess && inputs.at(attribute).connected;
    }
    bool operator==(MObject a) const {
        return attribute == a.name;
    }
    bool operator!=(MObject a) const {
        return !(*this == a);
    }
};
struct MDataBlock {
    MDataHandle inputValue(MObject a, MStatus* status) {
        const auto s = api("input " + a.name);
        *status = s;
        MDataHandle h;
        h.input = &inputs[a.name];
        h.valid = bool(s);
        return h;
    }
    MDataHandle outputValue(MObject a, MStatus* status) {
        const auto s = api("output");
        *status = s;
        MDataHandle h;
        h.input = &inputs[a.name];
        h.valid = bool(s);
        return h;
    }
    MStatus setClean(const MPlug&) {
        const auto s = api("clean");
        if (s)
            ++cleaned;
        return s;
    }
};
enum class DCurveReplacementPolicy { ResetMetadata };
struct DCurveInternalData {
    MPointArray points;
    MDoubleArray knots;
    int degree;
    bool open;
    std::map<std::string, std::vector<float>> attributes;
    std::map<std::string, std::vector<MVector>> vectorAttributes;
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
    DPAttrList fields;
    DPAttrIter beginAttr() { return fields.begin(); }
    DPAttrIter endAttr() { return fields.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    bool isExistAttr(const std::string& name) const { return bool(getAttr(name)); }
    void removeAttr(const std::string& name) { fields.erase(name); }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        if (!api("attribute.write")) throw std::runtime_error("attribute write failed");
        if (!rejectTypedAttribute) AddAttributeList(fields, name, value);
    }
#else
    const std::vector<MVector>& getAttr(const std::string& name) const {
        return vectorAttributes.at(name);
    }
#endif
    // Independent of Geometry::matrix: production stores primitive metadata
    // separately from the local Maya data object used to sample the curve.
    MMatrix primitiveMatrix;
    MMatrix getTransformMatrix() const {
        return primitiveMatrix;
    }
    void setTransformMatrix(const MMatrix& value) {
        primitiveMatrix = value;
    }
    Geometry geometryFixture;
    bool useGeometryFixture = false;
    MPointArray getPointsInWorld() const {
        if (!api("curve.worldPoints"))
            throw std::runtime_error("world point conversion failed");
        MPointArray result;
        for (const auto& point : points) {
            std::array<float, 3> transformed;
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    if (!std::isfinite(primitiveMatrix[row][column]))
                        throw std::invalid_argument("invalid matrix");
            if (!dommy::numeric::checkedTransformPoint(point, primitiveMatrix, transformed))
                throw std::invalid_argument("invalid world point");
            result.push_back(MPoint(transformed[0], transformed[1], transformed[2]));
        }
        return result;
    }
    DCurveInternalData(const MPointArray& p, const MDoubleArray& k, int d, bool open = true)
        : points(p), knots(k), degree(d), open(open) {}
    DCurveInternalData(const MPointArray& p, int d, bool open = true)
        : points(p), degree(d), open(open) {
        GenerateDCurveKnots(p.length(), d, knots);
    }
    bool insertPoint(const MPoint& p, std::size_t index) {
        if (!api("curve.insertPoint"))
            return false;
        points.insert(points.begin() + index, p);
        GenerateDCurveKnots(points.length(), degree, knots);
        return true;
    }
    bool appendPoint(const MPoint& p) {
        if (!api("curve.appendPoint"))
            return false;
        points.push_back(p);
        GenerateDCurveKnots(points.length(), degree, knots);
        return true;
    }
    const MPointArray* getPositionPtrConst() const {
        return &points;
    }
    const MDoubleArray& getKnots() const {
        return knots;
    }
    int getDegree() const {
        return degree;
    }
    bool isCurveOpen() const {
        return open;
    }
    void addAttrWithValues(const std::string& name, const std::vector<float>& values) {
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        addAttr(name, DPFloatAttr(values));
#else
        if (!api("attribute.write"))
            throw std::runtime_error("attribute write failed");
        attributes[name] = values;
#endif
    }
    bool replaceKnots(const MDoubleArray& value) {
        if (!api("curve.replaceKnots"))
            return false;
        knots = value;
        return hasValidCurveTopology();
    }
    bool setReplaceCurveData(const MPointArray& p, const MDoubleArray& k, int d, bool o,
                             DCurveReplacementPolicy) {
        if (!api("curve.replace"))
            return false;
        points = p;
        knots = k;
        degree = d;
        open = o;
        attributes.clear();
        return hasValidCurveTopology();
    }
    bool setRemappedCurveData(const MPointArray& p, const MDoubleArray& k, int d, bool o,
                              const std::vector<std::size_t>& indices) {
        if (!api("curve.remap"))
            return false;
        for (auto& attribute : attributes) {
            std::vector<float> next;
            for (auto i : indices)
                next.push_back(attribute.second.at(i));
            attribute.second.swap(next);
        }
        points = p;
        knots = k;
        degree = d;
        open = o;
        return hasValidCurveTopology();
    }
    bool hasValidCurveTopology() const {
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        dommy::attribute::ValidationBudget budget;
        if (!dommy::attribute::validateAttributeList(fields, points.size(), budget)) return false;
#endif
        if (degree < 1 || points.size() <= static_cast<unsigned>(degree) ||
            knots.size() != points.size() + degree - 1)
            return false;
        for (const auto& p : points)
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                return false;
        return true;
    }
    size_t numberOfPoints() const {
        return points.size();
    }
    // Geometry/metadata transport boundary; complete typed cost accounting is
    // executed by curve_transactions_tests rather than this NURBS trace.
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const {
        return hasValidCurveTopology() && budget.append(points.size(),knots.size(),0U,0U);
    }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
struct TestCurveGroup {
    using ConstPtr = std::shared_ptr<const TestCurveGroup>;
    std::vector<bool> ids;
    void getActiveIndexList(std::vector<std::size_t>& result) const {
        if (!api("group.indices"))
            throw std::runtime_error("group read failed");
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (ids[i])
                result.push_back(i);
    }
    const std::vector<bool>& getGroupIds() const {
        return ids;
    }
};
struct TestCurveStringAttribute {
    std::vector<std::string> values;
    explicit TestCurveStringAttribute(const std::vector<std::string>& v) : values(v) {}
};
struct DCurveData {
    using DCurveIter = std::vector<DCItem>::iterator;
    static constexpr MFnData::Type id = MFnData::kAny;
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    std::map<std::string, std::shared_ptr<TestCurveGroup>> groups;
    std::map<std::string, std::vector<std::string>> strings;
    DCurveList::const_iterator beginPrims() const {
        return curves.begin();
    }
    DCurveList::const_iterator endPrims() const {
        return curves.end();
    }
    TestCurveGroup::ConstPtr getGrpPtr(const std::string& name) const {
        return groups.at(name);
    }
    std::map<std::string, std::shared_ptr<TestCurveGroup>>::const_iterator beginGrp() const {
        return groups.begin();
    }
    std::map<std::string, std::shared_ptr<TestCurveGroup>>::const_iterator endGrp() const {
        return groups.end();
    }
    void addGrp(const std::string& name, const std::vector<bool>& ids) {
        if (!api("group.write"))
            throw std::runtime_error("group write failed");
        auto group = std::make_shared<TestCurveGroup>();
        group->ids = ids;
        groups[name] = group;
    }
    void addAttr(const std::string& name, const TestCurveStringAttribute& attr) {
        if (!api("string.write"))
            throw std::runtime_error("string write failed");
        strings[name] = attr.values;
    }
    bool insert(const DCItem& curve) {
        return insert(DCurveList{curve});
    }
    DCItem getCurvePtr(std::size_t index) const {
        return curves.at(index);
    }
    std::map<std::string, std::vector<float>> attributes;
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
    DPAttrList fields;
    DPAttrIter beginAttr() { return fields.begin(); }
    DPAttrIter endAttr() { return fields.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    bool isExistAttr(const std::string& name) const { return bool(getAttr(name)); }
    void removeAttr(const std::string& name) { fields.erase(name); }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        if (!api("attribute.write")) throw std::runtime_error("attribute write failed");
        if (!rejectTypedAttribute) AddAttributeList(fields, name, value);
    }
#else
    bool isExistAttr(const std::string& name) const {
        return attributes.count(name) != 0;
    }
    const std::vector<float>& getAttr(const std::string& name) const {
        return attributes.at(name);
    }
#endif
    void addAttrWithValues(const std::string& name, const std::vector<float>& values) {
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        addAttr(name, DPFloatAttr(values));
#else
        if (!api("attribute.write"))
            throw std::runtime_error("attribute write failed");
        attributes[name] = values;
#endif
    }
    MStatus copyFrom(const DCurveData& input) {
        const auto s = api("data.copy");
        if (!s)
            return s;
        curves.clear();
        attributes = input.attributes;
        groups = input.groups;
        strings = input.strings;
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        fields.clear();
        for (const auto& item : input.fields) fields[item.first] = item.second->copy();
#endif
        for (const auto& curve : input.curves)
            curves.push_back(DCItem(new DCurveInternalData(*curve)));
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        for (auto& curve : curves)
            for (auto& item : curve->fields) item.second = item.second->copy();
#endif
        return MS::kSuccess;
    }
    std::size_t numberOfCurves() const {
        return curves.size();
    }
    DCItem getCurvePtr(std::size_t index) {
        return curves.at(index);
    }
    DCurveList::const_iterator beginCurves() const {
        return curves.begin();
    }
    DCurveList::const_iterator endCurves() const {
        return curves.end();
    }
    DCurveList::iterator beginCurves() {
        return curves.begin();
    }
    DCurveList::iterator endCurves() {
        return curves.end();
    }
    bool getCheckedTotalCvs(std::size_t& result) const {
        if (!api("data.total"))
            return false;
        result = getTotalCvs();
        return true;
    }
    bool insert(const DCurveList& c) {
        const int index = callCount;
        if (!api("batch.insert"))
            return false;
        curves.insert(curves.end(), c.begin(), c.end());
        if (index == shortAt && !curves.empty())
            curves.pop_back();
        return true;
    }
    bool hasValidCurveTopology() const {
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        dommy::attribute::ValidationBudget budget;
        if (!dommy::attribute::validateAttributeList(fields, curves.size(), budget)) return false;
#endif
        for (const auto& c : curves)
            if (!c || !c->hasValidCurveTopology())
                return false;
        return true;
    }
    size_t getTotalCvs() const {
        size_t n = 0;
        for (const auto& c : curves)
            n += c->numberOfPoints();
        return n;
    }
    void updateBBox() const {}
};
namespace {
DCurveData outputData;
}
struct MFnPluginData {
    DCurveData data;
    MObject object() const {
        return {"pluginData"};
    }
};
namespace dommy {
namespace maya {
inline bool isFiniteMatrix(const MMatrix& m) {
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            if (!std::isfinite(m[r][c]))
                return false;
    return true;
}
inline bool isFiniteVector(const MVector& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline bool isFinitePoint(const MPoint& p) {
    return isFiniteVector(p) && std::isfinite(p.w);
}
// A pointer only valid during compute; publication copies into persistent test output.
DCurveData* pending = nullptr;
inline MStatus createPluginData(MFnPluginData& p, MFnData::Type, DCurveData*& out) {
    const auto s = api("plugin.create");
    if (s) {
        out = &p.data;
        pending = out;
    }
    return s;
}
template <class T> MStatus publishTypedPluginDataObject(MDataHandle& h, MObject, MFnData::Type) {
    h.check();
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
    if (!pending || !pending->hasValidCurveTopology()) return MS::kInvalidParameter;
#endif
    const auto s = api("publish");
    if (s) {
        outputData = *pending;
        ++published;
    }
    return s;
}
} // namespace maya
namespace math {
inline double degreesToRadians(double d) {
    return d * 0.0174532925199432957692369076848;
}
} // namespace math
} // namespace dommy
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const auto s = operation(std::string("string default ") + value);
        if (status)
            *status = s;
        return s ? MObject{"default"} : MObject::kNullObj;
    }
};
inline void EchoWarning(const char*, const char*) {}

struct DPData {
    static constexpr MFnData::Type id = MFnData::kAny;
    std::vector<std::array<double, 3>> points;
#ifdef DOMMY_TYPED_PATH_ATTRIBUTES
    DPAttrList fields;
    DPAttrIter beginAttr() { return fields.begin(); }
    DPAttrIter endAttr() { return fields.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        if (!api("point.attribute")) throw std::runtime_error("attribute failed");
        if (!rejectPointAttribute) AddAttributeList(fields, name, value);
    }
#else
    std::map<std::string, MVectorArray> attributes;
    std::map<std::string, std::vector<double>> scalarAttributes;
#endif
    void setPosition(const MPointArray& values);
    void updateBBox();
#ifndef DOMMY_TYPED_PATH_ATTRIBUTES
    template <class T> void addAttr(const std::string& name, const T& values);
#endif
    std::size_t numberOfPoints() const {
        return points.size();
    }
    bool topologyValid = true;
    mutable std::size_t positionCopies = 0U, positionViews = 0U;
    bool hasValidPointTopology() const {
        if (!topologyValid)
            return false;
        for (const auto& p : points)
            for (double v : p)
                if (!std::isfinite(v))
                    return false;
#ifdef DOMMY_TYPED_PATH_ATTRIBUTES
        dommy::attribute::ValidationBudget budget;
        if (!dommy::attribute::validateAttributeList(fields, points.size(), budget)) return false;
#else
        for (const auto& a : attributes)
            if (a.second.size() != points.size())
                return false;
        for (const auto& a : scalarAttributes)
            if (a.second.size() != points.size())
                return false;
#endif
        return true;
    }
    std::vector<std::array<double, 3>> getPositionConst() const {
        ++positionCopies;
        return points;
    }
    const std::vector<std::array<double, 3>>* getPositionPtrConst() const {
        ++positionViews;
        return &points;
    }
};
namespace dommy {
namespace maya {
template <class T>
MStatus getPluginDataFromHandle(const MDataHandle& h, MFnData::Type, const T*& result) {
    h.check();
    const auto s = api("plugin.read");
    if (!s)
        return s;
    if (!h.input->pluginTypeMatches)
        return MS::kFailure;
    if (!h.input->plugin)
        return MS::kInvalidParameter;
    result = static_cast<const T*>(h.input->plugin);
    return MS::kSuccess;
}
} // namespace maya
} // namespace dommy
void getMVectorArrayFromAttribute(const DPData& data, const std::string& name,
                                  MVectorArray& result) {
#ifdef DOMMY_TYPED_PATH_ATTRIBUTES
    std::vector<Imath::V3f> values;
    if (!dommy::attribute::convertTypedArray<Imath::V3f>(data.getAttr(name),
                                                       data.numberOfPoints(), values)) return;
    if (!dommy::maya::buildCheckedArray(values.size(), [&](unsigned int i, MVector& value) {
            value = MVector(values[i].x, values[i].y, values[i].z);
            return MS::kSuccess;
        }, result)) throw std::runtime_error("attribute conversion failed");
#else
    const auto found = data.attributes.find(name);
    if (found == data.attributes.end())
        return;
    if (!dommy::maya::copyCheckedArray(found->second, result))
        throw std::runtime_error("attribute conversion failed");
#endif
}
MStatus generateMCurveFromDcurve(const DCItem& input, MObject& result) {
    const auto s = api("curve.fromData");
    if (!s)
        return s;
    if (!input || !input->hasValidCurveTopology())
        return MS::kInvalidParameter;
    geometries["dcurve"] = input->useGeometryFixture ? input->geometryFixture : Geometry();
    result = {"dcurve"};
    return MS::kSuccess;
}
