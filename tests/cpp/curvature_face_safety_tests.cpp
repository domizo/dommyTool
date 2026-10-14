#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

// Compatibility only for executing archived, pre-fix methods in mutation checks.
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
#define CHECK_MSTATUS(expression)                                                                  \
    do {                                                                                           \
        const MStatus result = (expression);                                                       \
        if (!result)                                                                               \
            result.pAPIerror(__FILE__, __LINE__);                                                  \
    } while (false)

namespace {
int shortAt = -1, throwAt = -1, accessesAfterFault = 0;
bool faultReached = false;
bool earlyIterator = false, stuckIterator = false, nullCreated = false, nullCopied = false;
bool earlySmoothing = false;
int rampAttempts = 0, rampWrites = 0, colorWrites = 0, outputWrites = 0, cleanCalls = 0;
unsigned int existingRampEntries = 0;
std::vector<float> rampSamples;

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
    accessesAfterFault = 0;
    faultReached = false;
    earlyIterator = stuckIterator = nullCreated = nullCopied = false;
    earlySmoothing = false;
    rampAttempts = rampWrites = colorWrites = outputWrites = cleanCalls = 0;
    existingRampEntries = 0;
    rampSamples.clear();
}
} // namespace

struct MColor {
    float r, g, b, a;
    MColor(float r = 0, float g = 0, float b = 0) : r(r), g(g), b(b), a(1) {}
};
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    MVector operator/(double divisor) const {
        return {x / divisor, y / divisor, z / divisor};
    }
    double angle(const MVector& other) const {
        return std::acos(std::max(-1.0, std::min(1.0, (x * other.x + y * other.y + z * other.z) /
                                                          (length() * other.length()))));
    }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z), w(1) {}
    MVector operator-(const MPoint& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
};

template <class Value> const char* arrayName();
template <> const char* arrayName<double>() {
    return "double";
}
template <> const char* arrayName<float>() {
    return "float";
}
template <> const char* arrayName<int>() {
    return "int";
}
template <> const char* arrayName<MColor>() {
    return "color";
}
template <> const char* arrayName<MPoint>() {
    return "point";
}
template <class Value> Value uninitializedValue() {
    return Value();
}
template <> double uninitializedValue<double>() {
    return std::numeric_limits<double>::quiet_NaN();
}

template <class Value> struct Array {
    std::vector<Value> values;
    Array() = default;
    // Simulated failed/short legacy constructors; not Maya allocation evidence.
    Array(unsigned int count, const Value& initial = Value()) {
        if (setLength(count))
            std::fill(values.begin(), values.end(), initial);
    }
    Array(const Array& other) {
        copy(other);
    }
    MStatus setLength(unsigned int count) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<Value>()) + ".setLength");
        if (status)
            values.assign(index == shortAt && count ? count - 1 : count,
                          uninitializedValue<Value>());
        return status;
    }
    MStatus copy(const Array& other) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<Value>()) + ".copy");
        if (status) {
            values = other.values;
            if (index == shortAt && !values.empty())
                values.pop_back();
        }
        return status;
    }
    MStatus append(const Value& value) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<Value>()) + ".append");
        if (status && index != shortAt)
            values.push_back(value);
        return status;
    }
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    Value& operator[](unsigned int index) {
        if (faultReached)
            ++accessesAfterFault;
        return values.at(index);
    }
    const Value& operator[](unsigned int index) const {
        if (faultReached)
            ++accessesAfterFault;
        return values.at(index);
    }
};
using MDoubleArray = Array<double>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;
using MColorArray = Array<MColor>;
using MPointArray = Array<MPoint>;
struct MStringArray : std::vector<MString> {
    unsigned int length() const { return static_cast<unsigned int>(size()); }
};
namespace MSpace {
enum Space { kObject };
}

struct MPlug {
    std::string name;
    MPlug(const MObject&, const MObject& attribute) : name(attribute.name) {}
    bool operator!=(const MObject& attribute) const {
        return name != attribute.name;
    }
};

struct ColorRamp : MRampAttribute {
    enum { kSpline = 3 };
    ColorRamp(const MPlug&, MStatus* status) {
        *status = api("ramp.bind");
    }
    unsigned int getNumEntries(MStatus* status) {
        *status = api("ramp.entries");
        return existingRampEntries;
    }
    MStatus setRamp(const MColorArray& colors, const MFloatArray& positions,
                    const MIntArray& interpolation) {
        ++rampAttempts;
        const MStatus status = api("ramp.set");
        assert(colors.length() == 3 && positions.length() == 3 && interpolation.length() == 3);
        assert(positions.values == std::vector<float>({1.0f, 0.5f, 0.0f}));
        assert(interpolation.values == std::vector<int>({kSpline, kSpline, kSpline}));
        for (unsigned int i = 0; i < 3; ++i) {
            const auto& color = colors.values[i];
            assert(color.r == (i == 0) && color.g == (i == 1) && color.b == (i == 2) &&
                   color.a == 1);
        }
        if (status)
            ++rampWrites;
        return status;
    }
    void getColorAtPosition(float position, MColor& color, MStatus* status) {
        *status = api("ramp.sample");
        if (*status) {
            rampSamples.push_back(position);
            // Observe the production position; do not emulate Maya interpolation.
            color = MColor(position, 0, 0);
        }
    }
};
#define MRampAttribute ColorRamp

namespace {
struct Geometry {
    std::vector<MPoint> points = std::vector<MPoint>(5);
    std::vector<MVector> normals = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, 0, 0}, {0, 0, 1}};
    std::vector<std::vector<int>> faces = {{0}, {0}, {0}, {0}, {}};
    std::vector<std::vector<int>> neighbors = {{1, 2}, {0}, {0}, {}, {}};
    MPoint center = {0, 0, 1};
    int reportedCount = 5;
} geometry;
} // namespace

struct MDataHandle {
    bool valid;
    int value;
    MObject asMesh() const {
        assert(valid);
        return {"inputMesh"};
    }
    short asShort() const {
        assert(valid);
        return static_cast<short>(value);
    }
    int asInt() const {
        assert(valid);
        return value;
    }
    bool asBool() const {
        assert(valid);
        return value != 0;
    }
    MStatus set(const MObject& object) {
        assert(valid && object.name == "outputMesh");
        const MStatus status = api("output.set");
        if (status)
            ++outputWrites;
        return status;
    }
};
struct MDataBlock {
    int iterations = 1, calculationType = 0;
    bool adjust = false;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = api("input " + attribute.name);
        return {bool(*status), attribute.name == "smIterate"  ? iterations
                               : attribute.name == "CalcType" ? calculationType
                                                              : int(adjust)};
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = api("output.handle");
        return {bool(*status), 0};
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = api("output.clean");
        if (status)
            ++cleanCalls;
        return status;
    }
};
struct MFnMeshData {
    MObject create(MStatus* status) {
        *status = api("meshData.create");
        return *status && !nullCreated ? MObject{"outputMesh"} : MObject::kNullObj;
    }
};
struct MFnMesh {
    MFnMesh() = default;
    MFnMesh(const MObject&, MStatus* status) {
        *status = api("mesh.bind");
    }
    int numVertices(MStatus* status) {
        *status = api("mesh.count");
        return geometry.reportedCount;
    }
    MObject copy(const MObject&, const MObject&, MStatus* status) {
        *status = api("mesh.copy");
        return *status && !nullCopied ? MObject{"outputMesh"} : MObject::kNullObj;
    }
    MStatus getPoints(MPointArray& points, MSpace::Space) {
        const MStatus status = api("mesh.points");
        if (status)
            points.values = geometry.points;
        return status;
    }
    MStatus setDisplayColors(bool enabled) {
        assert(enabled);
        return MS::kFailure; // The real API rejects detached mesh data.
    }
    bool hasColorChannels(const MString&) {
        return false;
    }
    MStatus createColorSetDataMesh(const MString& name) {
        assert(name == MString("curvature"));
        return api("mesh.createColorSet");
    }
    MStatus setCurrentColorSetName(const MString&) {
        return MS::kFailure;
    }
    int numPolygons(MStatus* status) { *status = api("mesh.faces"); return 1; }
    int numFaceVertices(MStatus* status) { *status = api("mesh.corners"); return int(geometry.points.size()); }
    MStatus getVertices(MIntArray& counts, MIntArray& ids) {
        const auto status = api("mesh.topology");
        if (status) {
            counts.values = {int(geometry.points.size())};
            for (unsigned int i = 0; i < geometry.points.size(); ++i) ids.values.push_back(int(i));
        } return status;
    }
    MStatus getColorSetNames(MStringArray&) { return api("mesh.colorSets"); }
    MStatus clearColors(const MString* name) { assert(name && *name == MString("curvature")); return api("mesh.clearColors"); }
    MStatus setColors(const MColorArray& colors, const MString* name) {
        assert(name && *name == MString("curvature"));
        const auto status = api("mesh.setColors");
        assert(colors.length() == geometry.points.size());
        for (unsigned int i = 0; i < colors.length(); ++i) assert(colors.values[i].r == rampSamples.at(i));
        if (status) ++colorWrites;
        return status;
    }
    MStatus assignColors(const MIntArray& ids, const MString* name) {
        assert(name && *name == MString("curvature"));
        const auto status = api("mesh.assignColors");
        for (unsigned int i = 0; i < ids.length(); ++i) assert(ids.values[i] == int(i));
        return status;
    }
    MStatus setVertexColors(const MColorArray& colors, const MIntArray& ids) {
        const MStatus status = api("mesh.setColors");
        assert(colors.length() == geometry.points.size() && colors.length() == ids.length());
        for (unsigned int i = 0; i < ids.length(); ++i) {
            assert(ids.values[i] == static_cast<int>(i));
            assert(colors.values[i].r == rampSamples.at(i));
        }
        if (status)
            ++colorWrites;
        return status;
    }
};
struct MItMeshVertex {
    unsigned int current = 0;
    MItMeshVertex(const MObject&, MStatus* status) {
        *status = api("vertices.bind");
    }
    bool isDone(MStatus* status) const {
        *status = api("vertices.done");
        return current >= (earlyIterator ? 1U : geometry.points.size());
    }
    MStatus next() {
        if (!stuckIterator) ++current;
        return api("vertices.next");
    }
    MStatus reset() {
        current = 0;
        if (earlySmoothing) earlyIterator = true;
        return api("vertices.reset");
    }
    int index(MStatus* status) {
        *status = api("vertices.index");
        return static_cast<int>(current);
    }
    MStatus getNormal(MVector& normal, MSpace::Space) {
        const MStatus status = api("vertices.normal");
        if (status)
            normal = geometry.normals.at(current);
        return status;
    }
    MStatus getConnectedFaces(MIntArray& ids) {
        const MStatus status = api("vertices.faces");
        if (status)
            ids.values = geometry.faces.at(current);
        return status;
    }
    MStatus getConnectedVertices(MIntArray& ids) {
        const MStatus status = api("vertices.neighbors");
        if (status)
            ids.values = geometry.neighbors.at(current);
        return status;
    }
};
struct MItMeshPolygon {
    MItMeshPolygon(const MObject&, MStatus* status) {
        *status = api("polygons.bind");
    }
    MStatus setIndex(int index, int&) {
        assert(index == 0);
        return api("polygons.index");
    }
    MPoint center(MSpace::Space, MStatus* status) {
        *status = api("polygons.center");
        return geometry.center;
    }
};

struct DCurvatureData : AttributeNode {
    static MObject inMesh, aCalcType, aCurveMap, smIterate, aIsAdjustRange, outMesh;
    static MStatus initialize();
    MObject thisMObject() const {
        return {"node"};
    }
    void postConstructor();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DPolyFaceInfo : AttributeNode {
    static MObject inMesh, faceIndex, outTranslate, outRotate;
    static MStatus initialize();
};

#include "curvature_face_methods.inc"

namespace {
MStatus compute(MDataBlock& data) {
    DCurvatureData node;
    return node.compute(MPlug(node.thisMObject(), DCurvatureData::outMesh), data);
}

void expectSamples(int iterations, bool adjust, const std::vector<float>& expected) {
    resetRun();
    MDataBlock data;
    data.iterations = iterations;
    data.adjust = adjust;
    assert(compute(data) == MS::kSuccess);
    assert(rampSamples.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        assert(std::abs(rampSamples[i] - expected[i]) < 1.0e-6f);
    assert(colorWrites == 1 && outputWrites == 1 && cleanCalls == 1);
}

void testRamp() {
    DCurvatureData node;
    resetRun();
    node.postConstructor();
    assert(callCount == 12 && rampAttempts == 1 && rampWrites == 1);
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < 12; ++failure) {
            resetRun(failure);
            node.postConstructor();
            assert(callCount == failure + 1 && rampWrites == 0);
            assert(rampAttempts == (failure == 11));
        }
    }
    for (int failure = 0; failure < 12; ++failure) {
        resetRun();
        throwAt = failure;
        node.postConstructor(); // No exception escapes the Maya callback.
        assert(callCount == failure + 1 && rampWrites == 0);
        assert(rampAttempts == (failure == 11));
    }
    for (int failure = 2; failure < 11; ++failure) {
        resetRun();
        shortAt = failure;
        node.postConstructor();
        assert(callCount == 11 && rampAttempts == 0 && accessesAfterFault == 0);
    }
    resetRun();
    existingRampEntries = 4;
    node.postConstructor();
    assert(callCount == 2 && rampAttempts == 0);
    resetRun();
    node.postConstructor();
    assert(rampWrites == 1);
}

void testCompute() {
    expectSamples(0, false, {1, .5f, 0, 0, 0});
    expectSamples(1, false, {.25f, 1, 1, 0, 0});
    expectSamples(2, false, {1, .25f, .25f, 0, 0});
    expectSamples(1, true, {.25f, 1, 1, 0, 0});
    // Use a nontrivial range: only the connected, non-degenerate vertices.
    geometry.points.resize(3);
    geometry.reportedCount = 3;
    expectSamples(1, true, {0, 1, 1});
    geometry = Geometry();
    const auto savedNormals = geometry.normals;
    geometry.normals.assign(5, MVector());
    expectSamples(1, true, {0, 0, 0, 0, 0});
    geometry.normals = savedNormals;

    resetRun();
    MDataBlock data;
    assert(compute(data) == MS::kSuccess);
    const auto baseline = operations;
    std::vector<int> arrayCalls;
    for (std::size_t i = 0; i < baseline.size(); ++i)
        if (baseline[i] == "double.setLength" || baseline[i] == "double.copy" ||
            baseline[i] == "color.setLength" || baseline[i] == "int.setLength")
            arrayCalls.push_back(static_cast<int>(i));
    assert(arrayCalls.size() == 4);
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < static_cast<int>(baseline.size()); ++failure) {
            resetRun(failure);
            assert(compute(data) != MS::kSuccess);
            assert(callCount == failure + 1 && cleanCalls == 0);
            assert(accessesAfterFault == 0);
            if (failure < static_cast<int>(baseline.size()) - 2)
                assert(outputWrites == 0);
        }
        for (int failure : arrayCalls) {
            resetRun(failure);
            assert(compute(data) == status);
            assert(colorWrites == 0 && outputWrites == 0 && accessesAfterFault == 0);
        }
    }
    for (int failure : arrayCalls) {
        resetRun();
        shortAt = failure;
        assert(compute(data) == MS::kFailure);
        assert(callCount == failure + 1 && accessesAfterFault == 0);
        assert(colorWrites == 0 && outputWrites == 0 && cleanCalls == 0);
        resetRun();
        throwAt = failure;
        assert(compute(data) == MS::kFailure);
        assert(callCount == failure + 1 && accessesAfterFault == 0);
        assert(colorWrites == 0 && outputWrites == 0 && cleanCalls == 0);
        expectSamples(1, false, {.25f, 1, 1, 0, 0});
    }
    for (int mode = 0; mode < 5; ++mode) {
        resetRun();
        earlyIterator = mode == 0;
        stuckIterator = mode == 1;
        nullCreated = mode == 2;
        nullCopied = mode == 3;
        earlySmoothing = mode == 4;
        assert(compute(data) != MS::kSuccess && outputWrites == 0 && cleanCalls == 0);
    }
    resetRun();
    data.iterations = -1;
    assert(compute(data) == MS::kInvalidParameter && outputWrites == 0);
    resetRun();
    data.iterations = 1;
    data.calculationType = 1;
    assert(compute(data) == MS::kInvalidParameter && outputWrites == 0);
    resetRun();
    data.calculationType = 0;
    geometry.reportedCount = -1;
    assert(compute(data) == MS::kFailure && outputWrites == 0);
    resetRun();
    geometry.reportedCount = 10000001;
    assert(compute(data) == MS::kFailure && outputWrites == 0);
    resetRun();
    geometry.reportedCount = 5;
    data.iterations = 20000001;
    assert(compute(data) == MS::kFailure && outputWrites == 0);
    resetRun();
    data.iterations = 1;
    geometry.points.clear();
    assert(compute(data) == MS::kFailure && outputWrites == 0);
    resetRun();
    geometry.reportedCount = 0;
    assert(compute(data) == MS::kSuccess && outputWrites == 1 && cleanCalls == 1);
    assert(colorWrites == 0 && rampSamples.empty());
    resetRun();
    DCurvatureData node;
    assert(node.compute(MPlug(node.thisMObject(), DCurvatureData::inMesh), data) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
    geometry = Geometry();
    expectSamples(1, false, {.25f, 1, 1, 0, 0});
    std::cout << "Curvature: " << baseline.size() * 2
              << " compute API failures, 4 checked array sites; ";
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DCurvatureData", DCurvatureData::initialize) +
                              schemaTrace("DPolyFaceInfo", DPolyFaceInfo::initialize);
    if (std::string(argv[1]) == "--print-schema") {
        std::cout << trace;
        return 0;
    }
    if (std::string(argv[1]) == "--normal-only") {
        expectSamples(0, false, {1, .5f, 0, 0, 0});
        expectSamples(1, false, {.25f, 1, 1, 0, 0});
        expectSamples(2, false, {1, .25f, .25f, 0, 0});
        resetRun();
        DCurvatureData node;
        node.postConstructor();
        assert(rampWrites == 1);
        std::cout << "Legacy normal paths: OK\n";
        return 0;
    }
    std::ifstream fixture(argv[1]);
    assert(fixture);
    const std::string expected((std::istreambuf_iterator<char>(fixture)),
                               std::istreambuf_iterator<char>());
    assert(trace == expected);
    const int initializerFailures = testFailures("DCurvatureData", DCurvatureData::initialize, 27) +
                                    testFailures("DPolyFaceInfo", DPolyFaceInfo::initialize, 26);
    for (int at = 0; at < 27; ++at) {
        resetCalls(); attributeThrowAt = at;
        assert(!DCurvatureData::initialize() && callCount == at + 1);
    }
    testRamp();
    testCompute();
    std::cout << initializerFailures
              << " initializer failures; ramp preparation and recovery: OK\n";
}
