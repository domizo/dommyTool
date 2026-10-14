#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DProceduralMeshPlans.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

bool operator!(MStatus status) {
    return status != MS::kSuccess;
}

namespace MSpace {
enum Space { kWorld };
}
const double kMFnNurbsEpsilon = 1.0e-3;

// Host substitutes: ordinary Z-axis rotation and deterministic query samples,
// not Maya's rotation implementation or a NURBS geometry kernel.
struct MVector {
    enum Axis { kZaxis };
    double x, y, z;
    MVector(double x, double y, double z) : x(x), y(y), z(z) {}
    MVector rotateBy(Axis axis, double radians) const {
        assert(axis == kZaxis);
        const double c = std::cos(radians), s = std::sin(radians);
        return MVector(x * c - y * s, x * s + y * c, z);
    }
};
template <typename Value> struct Point {
    Value x, y, z, w;
    Point(Value x = 0, Value y = 0, Value z = 0) : x(x), y(y), z(z), w(1) {}
};
using MPoint = Point<double>;
using MFloatPoint = Point<float>;

template <typename Value> struct Array {
    std::vector<Value> values;
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    MStatus setLength(unsigned int length) {
        const MStatus status = operation("array.resize");
        if (status == MS::kSuccess)
            values.resize(length);
        return status;
    }
    MStatus append(const Value& value) {
        const MStatus status = operation("array.append");
        if (status == MS::kSuccess)
            values.push_back(value);
        return status;
    }
    Value& operator[](unsigned int index) {
        return values.at(index);
    }
    const Value& operator[](unsigned int index) const {
        return values.at(index);
    }
};
using MPointArray = Array<MPoint>;
using MFloatPointArray = Array<MFloatPoint>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;

namespace {
std::string nullFactory;
bool throwQuery = false, overrideParameter = false, invalidPoint = false;
double parameterValue = 0.0, sourceLength = 4.0, domainStart = 0.0, domainEnd = 4.0;
int sourceDegree = 3;
unsigned int createdDegree = 0;
std::vector<double> sampledParameters;
std::vector<MPoint> editPoints;
std::vector<MFloatPoint> meshVertices;
std::vector<int> meshCounts, meshConnections, meshUvIds;
std::vector<float> meshU, meshV;
int geometryCreations = 0;

MObject factory(const std::string& name, MStatus* status) {
    *status = operation(name);
    return *status == MS::kSuccess && nullFactory != name ? MObject{name} : MObject::kNullObj;
}
} // namespace

struct MFnMeshData {
    MObject create(MStatus* status) {
        return factory("mesh.data", status);
    }
};
struct MFnMesh {
    bool valid = false;
    MObject create(int vertices, int faces, const MFloatPointArray& positions,
                   const MIntArray& counts, const MIntArray& connections, const MFloatArray& u,
                   const MFloatArray& v, const MObject& parent, MStatus* status) {
        assert(parent.name == "mesh.data");
        assert(vertices == static_cast<int>(positions.length()) &&
               faces == static_cast<int>(counts.length()));
        assert(u.length() == positions.length() && v.length() == positions.length());
        assert(connections.length() == counts.length() * 4U);
        for (const auto& point : positions.values)
            assert(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                   point.w == 1);
        for (int count : counts.values)
            assert(count == 4);
        for (int index : connections.values)
            assert(index >= 0 && index < vertices);
        for (float value : u.values)
            assert(std::isfinite(value));
        for (float value : v.values)
            assert(std::isfinite(value));
        const MObject result = factory("mesh.create", status);
        valid = !result.isNull();
        if (valid) {
            meshVertices = positions.values;
            meshCounts = counts.values;
            meshConnections = connections.values;
            meshU = u.values;
            meshV = v.values;
            ++geometryCreations;
        }
        return result;
    }
    MStatus assignUVs(const MIntArray& counts, const MIntArray& ids) {
        assert(valid && counts.values == meshCounts && ids.values == meshConnections);
        const MStatus status = operation("mesh.assignUVs");
        if (status == MS::kSuccess)
            meshUvIds = ids.values;
        return status;
    }
};

struct MFnNurbsCurveData {
    static constexpr MFnData::Type kNurbsCurve = MFnData::kNurbsCurve;
    MObject create(MStatus* status) {
        return factory("curve.data", status);
    }
};
struct MFnNurbsCurve {
    enum Form { kOpen };
    bool valid = false;
    MFnNurbsCurve() = default;
    MFnNurbsCurve(const MObject& curve, MStatus* status) {
        assert(curve.name == "source");
        *status = operation("curve.bind");
        valid = *status == MS::kSuccess;
    }
    double length(double tolerance, MStatus* status) const {
        assert(valid && tolerance == kMFnNurbsEpsilon);
        if (throwQuery)
            throw std::runtime_error("injected query exception");
        *status = operation("curve.length");
        return sourceLength;
    }
    MStatus getKnotDomain(double& start, double& end) const {
        assert(valid);
        const MStatus status = operation("curve.domain");
        start = domainStart;
        end = domainEnd;
        return status;
    }
    double findParamFromLength(double length, MStatus* status) const {
        assert(valid && std::isfinite(length));
        *status = operation("curve.parameter");
        return overrideParameter ? parameterValue
                                 : domainStart + length / sourceLength * (domainEnd - domainStart);
    }
    MStatus getPointAtParam(double parameter, MPoint& point, MSpace::Space space) const {
        assert(valid && space == MSpace::kWorld && std::isfinite(parameter));
        const MStatus status = operation("curve.sample");
        if (status == MS::kSuccess) {
            sampledParameters.push_back(parameter);
            point = MPoint(parameter, 2 * parameter, -parameter);
            if (invalidPoint)
                point.x = std::numeric_limits<double>::quiet_NaN();
        }
        return status;
    }
    int degree(MStatus* status) const {
        assert(valid);
        *status = operation("curve.degree");
        return sourceDegree;
    }
    MObject createWithEditPoints(const MPointArray& points, unsigned int degree, Form form,
                                 bool is2d, bool rational, bool uniform, MObject& parent,
                                 MStatus* status) {
        assert(parent.name == "curve.data" && form == kOpen && !is2d && rational && uniform);
        assert(points.length() >= 3 && degree >= 1 && degree < points.length());
        const MObject result = factory("curve.create", status);
        if (!result.isNull()) {
            editPoints = points.values;
            createdDegree = degree;
            ++geometryCreations;
        }
        return result;
    }
};

struct MPlug {
    std::string name;
    bool operator!=(const MObject& object) const {
        return name != object.name;
    }
};
struct MDataBlock;
struct MDataHandle {
    bool valid;
    double value;
    MObject object;
    MFnData::Type dataType;
    MDataBlock* output;
    int asInt() const {
        assert(valid);
        return static_cast<int>(value);
    }
    float asFloat() const {
        assert(valid);
        return static_cast<float>(value);
    }
    MFnData::Type type() const {
        assert(valid);
        return dataType;
    }
    MObject data() const {
        assert(valid);
        return object;
    }
    MStatus set(const MObject& value);
};
struct MDataBlock {
    std::map<std::string, double> inputs = {
        {"divide", 10},    {"radius", 5},      {"width", 1},
        {"startAngle", 0}, {"endAngle", 45},   {"offsetAngle", 0},
        {"sampling", 10},  {"curveLength", 1}, {"offset", 0}};
    MObject source = {"source"}, published = {"oldOutput"};
    MFnData::Type inputType = MFnData::kNurbsCurve;
    int publications = 0;
    bool clean = false;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input." + attribute.name);
        const double value = attribute.name == "inputCurve" ? 0.0 : inputs.at(attribute.name);
        return {*status == MS::kSuccess, value, source, inputType, nullptr};
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "outputMesh" || attribute.name == "outputCurve");
        *status = operation("output.handle");
        return {*status == MS::kSuccess, 0.0, {}, inputType, this};
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = operation("output.clean");
        if (status == MS::kSuccess)
            clean = true;
        return status;
    }
};
MStatus MDataHandle::set(const MObject& value) {
    assert(valid && output && (value.name == "mesh.data" || value.name == "curve.data"));
    const MStatus status = operation("output.set");
    if (status == MS::kSuccess) {
        output->published = value;
        ++output->publications;
    }
    return status;
}

struct DArcCurveNode : AttributeNode {
    static MObject divide, radius, width, startAngle, endAngle, offsetAngle, outputMesh;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct remakeCurveNode : AttributeNode {
    static MObject inputCurve, sampling, curveLength, offset, outputCurve;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};

#include "arc_remake_methods.inc"

namespace {
void testInitialization(const char* fixture) {
    std::ifstream input(fixture);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string actual = schemaTrace("DArcCurveNode", DArcCurveNode::initialize) +
                               schemaTrace("remakeCurveNode", remakeCurveNode::initialize);
    if (actual != expected.str()) {
        std::cerr << "Arc/Remake declaration trace changed:\n" << actual;
        std::abort();
    }
    assert(testFailures("DArcCurveNode", DArcCurveNode::initialize, 38) == 76);
    assert(testFailures("remakeCurveNode", remakeCurveNode::initialize, 23) == 46);
}

MDataBlock defaults(int failure = -1) {
    resetCalls(failure);
    nullFactory.clear();
    throwQuery = overrideParameter = invalidPoint = false;
    parameterValue = 0;
    sourceLength = domainEnd = 4;
    domainStart = 0;
    sourceDegree = 3;
    createdDegree = 0;
    sampledParameters.clear();
    editPoints.clear();
    meshVertices.clear();
    meshCounts.clear();
    meshConnections.clear();
    meshUvIds.clear();
    meshU.clear();
    meshV.clear();
    geometryCreations = 0;
    return MDataBlock();
}
void assertUnpublished(const MDataBlock& block) {
    assert(block.publications == 0 && block.published.name == "oldOutput" && !block.clean);
}

template <typename Node> int testFailuresAndRecovery(const char* output) {
    Node node;
    auto block = defaults();
    assert(node.compute({"unrelated"}, block) == MS::kUnknownParameter && callCount == 0);
    assertUnpublished(block);
    assert(node.compute({output}, block) == MS::kSuccess && block.clean && block.publications == 1);
    const auto baseline = operations;
    const int count = callCount;
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < count; ++failure) {
            block = defaults(failure);
            assert(node.compute({output}, block) == status);
            assert(callCount == failure + 1);
            assert(std::equal(operations.begin(), operations.end(), baseline.begin()));
            if (failure == count - 1)
                assert(block.publications == 1 && !block.clean); // setClean cannot undo set().
            else
                assertUnpublished(block);
            block = defaults();
            assert(node.compute({output}, block) == MS::kSuccess && block.clean);
        }
    }
    return count * 2;
}

void testArcGeometry() {
    DArcCurveNode node;
    const double pi = std::acos(-1.0);
    for (int division : {0, 1, 10, 73}) {
        for (bool reversed : {false, true}) {
            auto block = defaults();
            block.inputs["divide"] = division;
            block.inputs["startAngle"] = reversed ? 80 : -20;
            block.inputs["endAngle"] = reversed ? -20 : 80;
            block.inputs["offsetAngle"] = 30;
            assert(node.compute({"outputMesh"}, block) == MS::kSuccess);
            const unsigned int samples = static_cast<unsigned int>(division + 2);
            assert(meshVertices.size() == samples * 2U && meshCounts.size() == samples - 1U);
            assert(meshUvIds == meshConnections);
            for (unsigned int i = 0; i < samples; ++i) {
                const double angle = (10.0 + 100.0 * i / (samples - 1U)) * pi / 180.0;
                for (unsigned int side = 0; side < 2; ++side) {
                    const double radius = side == 0 ? 6 : 5;
                    const unsigned int point = 2 * i + side;
                    assert(std::abs(meshVertices[point].x - radius * std::sin(angle)) < 1.0e-5);
                    assert(std::abs(meshVertices[point].y - radius * std::cos(angle)) < 1.0e-5);
                    assert(meshVertices[point].z == 0 && meshVertices[point].w == 1);
                    assert(std::abs(meshU[point] - (radius / 6 * std::sin(angle) * 0.5 + 0.5)) <
                           1.0e-6);
                    assert(std::abs(meshV[point] - (radius / 6 * std::cos(angle) * 0.5 + 0.5)) <
                           1.0e-6);
                }
            }
            for (unsigned int face = 0; face < samples - 1U; ++face) {
                const int vertex = static_cast<int>(face * 2U);
                const int expected[] = {vertex, vertex + 1, vertex + 3, vertex + 2};
                for (unsigned int corner = 0; corner < 4; ++corner)
                    assert(meshConnections[4U * face + corner] == expected[corner]);
            }
        }
    }
    // Outer radius exceeds FLT_MAX, but the sampled rotated XYZ all fit.
    auto block = defaults();
    block.inputs["radius"] = block.inputs["width"] = 2.0e38f;
    block.inputs["startAngle"] = 40;
    block.inputs["endAngle"] = 50;
    assert(node.compute({"outputMesh"}, block) == MS::kSuccess && !meshVertices.empty());
    for (const auto& point : meshVertices)
        assert(std::isfinite(point.x) && std::isfinite(point.y));
    block = defaults();
    block.inputs["radius"] = block.inputs["width"] = 2.0e38f;
    assert(node.compute({"outputMesh"}, block) == MS::kInvalidParameter);
    assertUnpublished(block);
    assert(geometryCreations == 0);
}

void testArcInvalidInputs() {
    DArcCurveNode node;
    for (const auto& invalid : std::vector<std::pair<std::string, double>>{
             {"divide", -1}, {"divide", 1000001}, {"radius", -1}, {"width", -1}, {"endAngle", 0}}) {
        auto block = defaults();
        block.inputs[invalid.first] = invalid.second;
        assert(node.compute({"outputMesh"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
        assert(geometryCreations == 0);
    }
    for (const char* attribute : {"radius", "width", "startAngle", "endAngle", "offsetAngle"}) {
        for (double invalid :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            auto block = defaults();
            block.inputs[attribute] = invalid;
            assert(node.compute({"outputMesh"}, block) == MS::kInvalidParameter);
            assertUnpublished(block);
        }
    }
    for (const char* failedFactory : {"mesh.data", "mesh.create"}) {
        auto block = defaults();
        nullFactory = failedFactory;
        assert(node.compute({"outputMesh"}, block) == MS::kFailure);
        assertUnpublished(block);
    }
}

void testRemakeSampling() {
    remakeCurveNode node;
    for (int sampling : {2, 4, 10}) {
        for (float length : {1.0f, 4.0f, 8.0f}) {
            for (float offset : {-2.0f, 0.0f, 2.0f}) {
                auto block = defaults();
                block.inputs["sampling"] = sampling;
                block.inputs["curveLength"] = length;
                block.inputs["offset"] = offset;
                assert(node.compute({"outputCurve"}, block) == MS::kSuccess);
                assert(editPoints.size() == static_cast<std::size_t>(sampling + 1));
                assert(createdDegree == static_cast<unsigned int>(std::min(3, sampling)));
                for (int i = 0; i <= sampling; ++i) {
                    const double expected =
                        std::max(0.0, std::min(4.0, std::min(4.0, static_cast<double>(length)) * i /
                                                            sampling +
                                                        offset));
                    assert(std::abs(sampledParameters[i] - expected) < 1.0e-12);
                    assert(editPoints[i].x == sampledParameters[i] &&
                           editPoints[i].y == 2 * sampledParameters[i]);
                }
            }
        }
    }
}

void testRemakeInvalidInputs() {
    remakeCurveNode node;
    for (double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto block = defaults();
        overrideParameter = true;
        parameterValue = invalid;
        assert(node.compute({"outputCurve"}, block) == MS::kFailure);
        assertUnpublished(block);
        assert(sampledParameters.empty() && geometryCreations == 0);
        for (const char* attribute : {"curveLength", "offset"}) {
            block = defaults();
            block.inputs[attribute] = invalid;
            assert(node.compute({"outputCurve"}, block) == MS::kInvalidParameter);
            assertUnpublished(block);
        }
    }
    for (const auto& invalid : std::vector<std::pair<std::string, double>>{{"sampling", -1},
                                                                           {"sampling", 1},
                                                                           {"sampling", 10000000},
                                                                           {"curveLength", 0},
                                                                           {"curveLength", -1}}) {
        auto block = defaults();
        block.inputs[invalid.first] = invalid.second;
        assert(node.compute({"outputCurve"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
    }
    for (int scenario = 0; scenario < 10; ++scenario) {
        auto block = defaults();
        if (scenario == 0)
            block.source = MObject::kNullObj;
        if (scenario == 1)
            block.inputType = MFnData::kMesh;
        if (scenario == 2)
            sourceLength = 0;
        if (scenario == 3)
            sourceLength = std::numeric_limits<double>::infinity();
        if (scenario == 4)
            domainStart = 5;
        if (scenario == 5)
            domainEnd = std::numeric_limits<double>::quiet_NaN();
        if (scenario == 6)
            sourceDegree = 0;
        if (scenario == 7)
            invalidPoint = true;
        if (scenario == 8)
            nullFactory = "curve.data";
        if (scenario == 9)
            nullFactory = "curve.create";
        assert(node.compute({"outputCurve"}, block) ==
               (scenario < 4 ? MS::kInvalidParameter : MS::kFailure));
        assertUnpublished(block);
    }
    auto block = defaults();
    throwQuery = true;
    assert(node.compute({"outputCurve"}, block) == MS::kFailure);
    assertUnpublished(block);
    block = defaults();
    assert(node.compute({"outputCurve"}, block) == MS::kSuccess && block.clean);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testInitialization(argv[1]);
    const int failures = testFailuresAndRecovery<DArcCurveNode>("outputMesh") +
                         testFailuresAndRecovery<remakeCurveNode>("outputCurve");
    testArcGeometry();
    testArcInvalidInputs();
    testRemakeSampling();
    testRemakeInvalidInputs();
    std::cout << "Arc/Remake safety passed: 122 initializer failures, " << failures
              << " compute failures, geometry buffers, query validation and recovery\n";
}
