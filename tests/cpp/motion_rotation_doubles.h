#pragma once

// Host doubles for source-extracted animation-node control flow. Rotation APIs
// transport distinct components; they do NOT implement Maya rotation mathematics.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"
#include "math/DFrameAlignment.h"
#include <ImathVec.h>
#include <array>
#include <fstream>
#include <iostream>
#include <map>

// Match Maya's logging-only macro for archived-body compatibility checks.
#define CHECK_MSTATUS(expression)                                                                  \
    {                                                                                              \
        const MStatus s = (expression);                                                            \
        if (!s)                                                                                    \
            s.pAPIerror(__FILE__, __LINE__);                                                       \
    }

namespace {
int shortAt = -1, throwAt = -1, publications = 0, cleanCalls = 0;
int matrixCopies = 0, arrayCopies = 0, workerCopies = 0, outOfBounds = 0;
int dataCopies = 0;
int boundsUpdates = 0;
bool shortAttribute = false, throwDataCopy = false, failReplacement = false;
bool shortApplied = false;
bool nullCreated = false;
bool rejectAttribute = false;
bool readingAttribute = false;
double sampledTranslationScale = 1.0;
std::vector<double> sampledTimes;
MStatus api(const std::string& name) {
    const int index = callCount;
    const MStatus status = operation(readingAttribute ? "attribute " + name : name);
    if (index == throwAt)
        throw std::runtime_error("injected API exception");
    return status;
}
void resetRun(int failure = -1) {
    resetCalls(failure);
    shortAt = throwAt = -1;
    publications = cleanCalls = matrixCopies = arrayCopies = workerCopies = outOfBounds = 0;
    dataCopies = 0;
    boundsUpdates = 0;
    shortAttribute = throwDataCopy = failReplacement = false;
    shortApplied = false;
    nullCreated = false;
    rejectAttribute = false;
    readingAttribute = false;
    sampledTranslationScale = 1.0;
    sampledTimes.clear();
}
} // namespace

using uint = unsigned int;
using double3 = double[3];
enum {
    ROTATE_ORDER_XYZ,
    ROTATE_ORDER_YZX,
    ROTATE_ORDER_ZXY,
    ROTATE_ORDER_XZY,
    ROTATE_ORDER_YXZ,
    ROTATE_ORDER_ZYX
};
namespace MSpace {
enum Space { kWorld, kTransform, kObject };
}
struct MQuaternion {
    double x = 0, y = 0, z = 0, w = 1;
};
struct MMatrix {
    double values[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    MMatrix() = default;
    MMatrix(const MMatrix& other) {
        *this = other;
        ++matrixCopies;
    }
    MMatrix& operator=(const MMatrix& other) {
        for (unsigned r = 0; r < 4; ++r)
            for (unsigned c = 0; c < 4; ++c)
                values[r][c] = other[r][c];
        return *this;
    }
    double* operator[](unsigned i) {
        return values[i];
    }
    const double* operator[](unsigned i) const {
        return values[i];
    }
};
// The test build shadows maya/MMatrix.h with an empty header: the transport
// matrix above is the sole native-math seam. Attribute classes remain real.
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
#endif
inline MMatrix operator*(const MMatrix& a, const MMatrix& b) {
    MMatrix result;
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column) {
            result[row][column] = 0;
            for (unsigned k = 0; k < 4; ++k)
                result[row][column] += a[row][k] * b[k][column];
        }
    return result;
}
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    explicit MVector(const double* p) : x(p[0]), y(p[1]), z(p[2]) {}
    double operator[](unsigned i) const {
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    MVector normal() const {
        const double n = length();
        return n ? MVector(x / n, y / n, z / n) : *this;
    }
    MQuaternion rotateTo(const MVector& other) const {
        MQuaternion q;
        q.x = other.x - x;
        q.y = other.y - y;
        q.z = other.z - z;
        return q;
    }
    MVector rotateBy(const MQuaternion& q) const {
        return {x + q.x, y + q.y, z + q.z};
    }
    MVector rotateBy(const double* r, int order) const {
        return {x + r[0] * (order + 1), y + r[1] * (order + 1), z + r[2] * (order + 1)};
    }
    MVector operator+(const MVector& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }
    MVector operator*(const MMatrix& m) const {
        // Actual linear algebra plus the legacy Euler/quaternion trace payload.
        return {x * m[0][0] + y * m[1][0] + z * m[2][0] + m[0][3],
                x * m[0][1] + y * m[1][1] + z * m[2][1] + m[1][3],
                x * m[0][2] + y * m[1][2] + z * m[2][2] + m[2][3]};
    }
};
struct MPoint : MVector {
    using MVector::MVector;
};
struct MTransformationMatrix {
    enum RotationOrder { kInvalid = -1, kXYZ, kYZX, kZXY, kXZY, kYXZ, kZYX };
    MMatrix matrix;
    MTransformationMatrix() = default;
    explicit MTransformationMatrix(const MMatrix& m) : matrix(m) {}
    MTransformationMatrix& operator=(const MMatrix& m) {
        matrix = m;
        return *this;
    }
    MStatus addScale(const double* scale, MSpace::Space) {
        const MStatus status = api("transform.scale");
        if (status)
            for (unsigned i = 0; i < 3; ++i)
                matrix[i][i] *= scale[i];
        return status;
    }
    MStatus addTranslation(const MVector& value, MSpace::Space) {
        const MStatus status = api("transform.translation.add");
        if (status)
            for (unsigned i = 0; i < 3; ++i)
                matrix[3][i] += value[i];
        return status;
    }
    MMatrix asMatrix() const {
        return matrix;
    }
    MMatrix asRotateMatrix() const {
        return matrix;
    }
    MVector getTranslation(MSpace::Space, MStatus* status = nullptr) const {
        const MStatus result = api("transform.translation");
        if (status)
            *status = result;
        return {matrix[3][0], matrix[3][1], matrix[3][2]};
    }
    MStatus addRotationQuaternion(double x, double y, double z, double w, MSpace::Space) {
        const MStatus status = api("transform.quaternion");
        if (status) {
            matrix[0][3] += x * w;
            matrix[1][3] += y * w;
            matrix[2][3] += z * w;
        }
        return status;
    }
    MStatus addRotation(const double* r, RotationOrder order, MSpace::Space) {
        const MStatus status = api("transform.euler");
        if (status)
            for (unsigned i = 0; i < 3; ++i)
                matrix[i][3] += r[i] * (order + 1);
        return status;
    }
    MStatus getRotation(double* r, RotationOrder&, MSpace::Space) const {
        const MStatus status = api("transform.getRotation");
        if (status)
            for (unsigned i = 0; i < 3; ++i)
                r[i] = matrix[i][3];
        return status;
    }
};
MStatus getAlignSpaceMatrix(const MVector&, const MVector&, const MVector&,
                           const MVector&, MMatrix&);

template <class T> const char* arrayName();
template <> const char* arrayName<MVector>() {
    return "vector";
}
template <> const char* arrayName<MMatrix>() {
    return "matrix";
}
template <> const char* arrayName<double>() {
    return "double";
}
template <class T> struct Array {
    std::vector<T> values;
    Array() = default;
    explicit Array(unsigned n, const T& initial = T()) {
        if (setLength(n))
            std::fill(values.begin(), values.end(), initial);
    }
    Array(const Array& a) {
        copy(a);
    }
    MStatus setLength(unsigned n) {
        const int index = callCount;
        const MStatus s = api(std::string(arrayName<T>()) + ".allocate");
        if (s) {
            const bool shorten = index == shortAt && n;
            shortApplied |= shorten;
            values.resize(shorten ? n - 1 : n);
        }
        return s;
    }
    MStatus copy(const Array& a) {
        ++arrayCopies;
        const int index = callCount;
        const MStatus s = api(std::string(arrayName<T>()) + ".copy");
        if (s) {
            values = a.values;
            if (index == shortAt && !values.empty()) {
                shortApplied = true;
                values.pop_back();
            }
        }
        return s;
    }
    unsigned length() const {
        return static_cast<unsigned>(values.size());
    }
    T& operator[](unsigned i) {
        if (i >= values.size())
            ++outOfBounds;
        return values.at(i);
    }
    const T& operator[](unsigned i) const {
        if (i >= values.size())
            ++outOfBounds;
        return values.at(i);
    }
};
using MVectorArray = Array<MVector>;
using MMatrixArray = Array<MMatrix>;
using MDoubleArray = Array<double>;
using DMatrixArray = std::vector<MMatrix>;
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
#ifndef DOMMY_TYPED_ROTATION_ATTRIBUTES
// Older coordinate/scale/boundary suites explicitly model attribute transport.
// The motion/rotation command enables the stricter single-map mode below.
struct DPVec3dAttr {
    std::vector<Imath::V3d> values;
    DPVec3dAttr(const std::vector<Imath::V3d>& v, const Imath::V3d&) : values(v) {}
};
struct DPMat4Attr {
    std::vector<MMatrix> values;
    DPMat4Attr(const std::vector<MMatrix>& v, const MMatrix&) : values(v) {}
};
#endif
struct DPData {
    // Only aliases the custom type for initializer status flow, not SDK type checking.
    static constexpr MFnData::Type id = MFnData::kAny;
    DPointList points = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
    DPAttrList fields;
    DPAttrIter beginAttr() { return fields.begin(); }
    DPAttrIter endAttr() { return fields.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    template<class T> const std::vector<T>& values(const std::string& name) const {
        const auto field = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(getAttr(name));
        assert(field);
        return field->value();
    }
#else
    std::map<std::string, std::vector<Imath::V3d>> vectors;
    std::map<std::string, std::vector<MMatrix>> matrices;
    std::map<std::string, std::vector<double>> scalars;
#endif
    // Explicit metadata-cost seam; real typed/default/group payloads are
    // exercised by the point batch and coordinate-output suites.
    std::size_t metadataBytes = 0U;
    struct Group { std::vector<bool> ids; const std::vector<bool>& getGroupIds()const{return ids;} };
    std::map<std::string, std::shared_ptr<Group>> groups;
    auto beginGrp() const -> decltype(groups.begin()) {return groups.begin();}
    auto endGrp() const -> decltype(groups.end()) {return groups.end();}
    std::size_t fakeCount = 0;
    bool valid = true;
    bool hasValidPointTopology() const {
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
        dommy::attribute::ValidationBudget budget;
        return valid && dommy::attribute::validateAttributeList(fields, points.size(), budget);
#else
        return valid;
#endif
    }
    std::size_t numberOfPoints() const {
        return fakeCount ? fakeCount : points.size();
    }
    const DPointList& getPositionConst() const {
        return points;
    }
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    void setDPData(const DPData& d) {
        ++dataCopies;
        if (throwDataCopy)
            throw std::bad_alloc();
        *this = d;
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
        for (auto& field : fields) field.second = field.second->copy();
#endif
    }
    bool replacePositions(const DPointList& p) {
        if (failReplacement)
            return false;
        points = p;
        return true;
    }
    template<class Placement>
    MStatus copyRepeatedFrom(const DPData& source, std::size_t copies, Placement placement) {
        ++dataCopies;
        assert(copies == 1U);
        if (throwDataCopy) throw std::bad_alloc();
        if (failReplacement) return MS::kFailure;
        DPData candidate(source);
        for (std::size_t index = 0U; index < candidate.points.size(); ++index) {
            const MStatus status = placement(candidate.points[index], 0U, index);
            if (!status) return status;
        }
        candidate.updateBBox();
        *this = candidate;
        return MS::kSuccess;
    }
    void updateBBox() { ++boundsUpdates; }
    bool isExistAttr(const std::string& s) const {
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
        return bool(getAttr(s));
#else
        return vectors.count(s) || matrices.count(s) || scalars.count(s);
#endif
    }
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
    void addAttr(const std::string& s, const DPAttributeData& a) {
        if (!api("attribute write")) throw std::runtime_error("attribute write failed");
        if (!rejectAttribute) AddAttributeList(fields, s, a);
    }
#else
    void addAttr(const std::string& s, const DPVec3dAttr& a) { vectors[s] = a.values; }
    void addAttr(const std::string& s, const DPMat4Attr& a) { matrices[s] = a.values; }
#endif
    void addAttrWithValues(const std::string& s, const std::vector<MMatrix>& values) {
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
        addAttr(s, DPMat4Attr(values));
#else
        matrices[s] = values;
#endif
    }
};
namespace {
DPData inputData, stagedData, publishedData;
}
inline void checkDPAttribute(const std::string& s, DPData& d, bool) {
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
    d.fields.erase(s);
#else
    d.vectors.erase(s);
    d.matrices.erase(s);
    d.scalars.erase(s);
#endif
}
inline void getMVectorArrayFromAttribute(const DPData& d, const std::string& s, MVectorArray& out) {
#ifdef DOMMY_TYPED_ROTATION_ATTRIBUTES
    const auto field = std::dynamic_pointer_cast<const DPVec3dAttr>(d.getAttr(s));
    if (!field)
        return;
    const auto& values = field->value();
#else
    const auto found = d.vectors.find(s);
    if (found == d.vectors.end()) return;
    const auto& values = found->second;
#endif
    struct AttributeScope {
        AttributeScope() {
            readingAttribute = true;
        }
        ~AttributeScope() {
            readingAttribute = false;
        }
    } attributeScope;
    MStatus status = dommy::maya::buildCheckedArray(
        values.size(),
        [&](unsigned i, MVector& v) -> MStatus {
            const auto& a = values.at(i);
            v = {a.x, a.y, a.z};
            return MS::kSuccess;
        },
        out);
    if (!status)
        throw std::runtime_error("attribute copy failed");
    if (shortAttribute && !out.values.empty())
        out.values.pop_back();
}
struct Input {
    double number = 0;
    double vector[3] = {0, 0, 0};
    MMatrix matrix;
    MString string;
    std::vector<double> scalars;
    bool scalarInput = false;
    bool connected = false, childConnected = false;
};
struct MPlug {
    std::string name;
    bool childPlug = false;
    MPlug(const MObject&, const MObject& a) : name(a.name) {}
    bool operator==(const MObject& a) const {
        return name == a.name;
    }
    bool operator!=(const MObject& a) const {
        return name != a.name;
    }
    bool isConnected(MStatus* status = nullptr) const;
    unsigned numChildren(MStatus* status = nullptr) const {
        const auto s = api("plug.children");
        if (status)
            *status = s;
        return 3;
    }
    MPlug child(unsigned, MStatus* status = nullptr) const {
        const auto s = api("plug.child");
        if (status)
            *status = s;
        MPlug p = *this;
        p.childPlug = true;
        return p;
    }
};
namespace {
std::map<std::string, Input> inputs;
}
inline bool MPlug::isConnected(MStatus* status) const {
    const auto s = api("plug.connected " + name);
    if (status) *status = s;
    return s && (childPlug ? inputs.at(name).childConnected : inputs.at(name).connected);
}
struct MDataHandle {
    Input* input = nullptr;
    bool valid = true;
    void check() const {
        assert(valid);
    }
    MTime asTime() const {
        check();
        return MTime(input->number);
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    float asFloat() const {
        check();
        return static_cast<float>(input->number);
    }
    short asShort() const {
        check();
        return static_cast<short>(input->number);
    }
    bool asBool() const {
        check();
        return input->number != 0;
    }
    double asDouble() const {
        check();
        return input->number;
    }
    MVector asVector() const {
        check();
        return MVector(input->vector);
    }
    const double3& asDouble3() const {
        check();
        return input->vector;
    }
    MString asString() const {
        check();
        return input->string;
    }
    MMatrix asMatrix() const {
        check();
        return input->matrix;
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a, MStatus* status = nullptr) {
        const auto s = api("input " + a.name);
        if (status)
            *status = s;
        MDataHandle h;
        h.input = &inputs[a.name];
        h.valid = bool(s);
        return h;
    }
    MDataHandle outputValue(const MObject& a, MStatus* status = nullptr) {
        const auto s = api("output " + a.name);
        if (status)
            *status = s;
        MDataHandle h;
        h.valid = bool(s);
        return h;
    }
    MStatus setClean(const MPlug&) {
        const auto s = api("clean");
        if (s)
            ++cleanCalls;
        return s;
    }
};
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const auto s = operation(std::string("string default ") + value);
        if (status)
            *status = s;
        return s ? MObject{"default"} : MObject::kNullObj;
    }
};
struct AnimationNode : AttributeNode {
    MObject thisMObject() const {
        return MObject{"node"};
    }
};
inline const DPData* getInputDP(const MObject& a, MDataBlock& b) {
    MStatus status;
    b.inputValue(a, &status);
    return status && inputData.valid ? &inputData : nullptr;
}
namespace dommy {
namespace maya {
inline bool isFiniteVector(const MVector& v);
inline bool isFloatRepresentableVector(const MVector& v);
template <class T> MStatus getPluginDataFromHandle(MDataHandle& h, MFnData::Type, const T*& p) {
    h.check();
    const auto s = api("plugin.read");
    if (s)
        p = &inputData;
    return s;
}
template <class T> MStatus createPluginDataObject(MFnData::Type, MObject& object, T*& p) {
    const auto s = api("plugin.create");
    if (s) {
        object = MObject{"data"};
        p = nullCreated ? nullptr : &stagedData;
    }
    return s;
}
template <class T> MStatus publishPluginDataObject(MDataHandle& h, const MObject&, const T& d) {
    h.check();
    const auto s = api("publish");
    if (s) {
        publishedData = d;
        ++publications;
    }
    return s;
}
inline MStatus getEvaluationTime(MDataBlock&, MTime& t) {
    const auto s = api("time.current");
    if (s)
        t = MTime(12);
    return s;
}
inline MStatus sampleMatrixPlugAtTime(const MPlug&, const MTime& t, MMatrix& m) {
    const auto s = api("sample.matrix");
    if (s) {
        sampledTimes.push_back(t.value());
        m[3][0] = t.value() * sampledTranslationScale;
        m[3][1] = 2;
        m[3][2] = -3;
        m[0][3] = 0.2;
    }
    return s;
}
inline MStatus sampleVectorPlugAtTime(const MPlug&, const MTime& t, MVector& v) {
    const auto s = api("sample.vector");
    if (s) {
        sampledTimes.push_back(t.value());
        v = {t.value(), 2, -3};
    }
    return s;
}
} // namespace maya
namespace math {
double degreesToRadians(double v);
}
} // namespace dommy
inline MStatus getTimeOffsetAndStretch(const DPData& d, MDataBlock&, const MObject&, const MObject&,
                                       const MObject&, const MObject&, const MObject&, int offset,
                                       int stretch, MDoubleArray& offsets,
                                       MDoubleArray& stretches) {
    const auto s = api("time.arrays");
    if (!s)
        return s;
    if (offset < 0 || offset > 2 || stretch < 0 || stretch > 2)
        return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
        d.numberOfPoints(),
        [&](unsigned i, double& v) -> MStatus {
            v = offset ? 0.5 * i : 0;
            return MS::kSuccess;
        },
        offsets));
    return dommy::maya::buildCheckedArray(
        d.numberOfPoints(),
        [&](unsigned i, double& v) -> MStatus {
            v = stretch ? (i ? 2 : 0) : 1;
            return MS::kSuccess;
        },
        stretches);
}
inline std::size_t DGetGlobalProcessNum(std::size_t) {
    return 1;
}
namespace tbb {
template <class T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b, T) : first(a), last(b) {}
    T begin() const {
        return first;
    }
    T end() const {
        return last;
    }
};
template <class T, class Body> void parallel_for(const blocked_range<T>& range, const Body& body) {
    const int before = matrixCopies + arrayCopies;
    const Body taskCopy = body;
    workerCopies += matrixCopies + arrayCopies - before;
    // Split disjoint ranges, but no real TBB scheduling/concurrency emulation.
    const T middle = range.begin() + (range.end() - range.begin()) / 2;
    body(blocked_range<T>(range.begin(), middle, 1));
    taskCopy(blocked_range<T>(middle, range.end(), 1));
}
} // namespace tbb
