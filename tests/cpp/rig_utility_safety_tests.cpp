#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DProceduralMeshPlans.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

#include "rig_utility_types.inc"

bool operator!(MStatus status) {
    return status != MS::kSuccess;
}
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)

namespace {
bool shortCopy = false, throwCopy = false;
unsigned int pointReads = 0;
std::vector<double> observed;
std::size_t observedBudget = 0;
} // namespace
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z), w(1) {}
    bool operator==(const MPoint& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};
struct MPointArray {
    std::vector<MPoint> values;
    MPointArray() = default;
    // Keep a failure-injectable constructor to catch restoration of the old copy.
    MPointArray(const MPointArray& other) {
        copy(other);
    }
    MStatus copy(const MPointArray& other) {
        const MStatus status = operation("points.copy");
        if (throwCopy)
            throw std::runtime_error("injected copy exception");
        if (status == MS::kSuccess && !shortCopy)
            values = other.values;
        return status;
    }
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    MPoint& operator[](unsigned int index) {
        ++pointReads;
        return values.at(index);
    }
    const MPoint& operator[](unsigned int index) const {
        ++pointReads;
        return values.at(index);
    }
};
using double3 = double[3];
struct MDataHandle {
    bool valid;
    double value;
    double3* vector;
    int asInt() const {
        assert(valid);
        return static_cast<int>(value);
    }
    double asDouble() const {
        assert(valid);
        return value;
    }
    bool asBool() const {
        assert(valid);
        return value != 0;
    }
    double3& asDouble3() const {
        assert(valid && vector);
        return *vector;
    }
};
struct MDataBlock {
    std::map<std::string, double> values = {{"shapeType", D_CTRL_CROSS_ARROW},
                                            {"localScale", 2},
                                            {"division", 8},
                                            {"offset", 0},
                                            {"localRotate", 0},
                                            {"keepCenter", 1},
                                            {"ratio", 1.25},
                                            {"dirAxis", D_X_AXIS_MINUS},
                                            {"taperAxis", D_Y_AXIS},
                                            {"taperAmount", 0.4},
                                            {"bendAmount", 30},
                                            {"bendAxis", D_Y_AXIS},
                                            {"arrowReverse", 1},
                                            {"arrowRatio", 0.75},
                                            {"arrowWidth", 0.25}};
    double3 offset = {1, 2, 3}, rotation = {10, 20, 30};
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        return {*status == MS::kSuccess, values.at(attribute.name),
                attribute.name == "offset" ? &offset : &rotation};
    }
};
struct MPlug {
    std::string name;
    bool operator==(const MObject& object) const {
        return name == object.name;
    }
};
struct MFnNurbsCurveData {
    static constexpr MFnData::Type kNurbsCurve = MFnData::kNurbsCurve;
};
struct DRigController : AttributeNode {
    static MObject shapeType, localScale, division, offset, offsetX, offsetY, offsetZ, localRotate,
        localRotateX, localRotateY, localRotateZ, keepCenter, ratio, dirAxis, taperAxis,
        taperAmount, bendAmount, bendAxis, arrowReverse, arrowRatio, arrowWidth, outputCurve;
    static MStatus initialize();
    MStatus inputBoundary(const MPlug&, MDataBlock&);
    bool taperDeform(MPointArray&, double, double, double, D_AXIS_TYPE);
    bool bendDeform(MPointArray&, double, double, double, D_AXIS_TYPE);
    bool crossBendDeform(MPointArray&, double, double, double);
};
struct DMatToVec : AttributeNode {
    static MObject inMatrix, aType, aSpace, outVector;
    static MStatus initialize();
};
struct DNumericToFloatArray : AttributeNode {
    static MObject inputData, outDataArray;
    static MStatus initialize();
};

#include "rig_utility_methods.inc"

namespace {
void testInitialization(const char* fixture) {
    std::ifstream input(fixture);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string actual =
        schemaTrace("DRigController", DRigController::initialize) +
        schemaTrace("DMatToVec", DMatToVec::initialize) +
        schemaTrace("DNumericToFloatArray", DNumericToFloatArray::initialize);
    if (actual != expected.str()) {
        std::cerr << "Rig/Utility declaration trace changed:\n" << actual;
        std::abort();
    }
    assert(testFailures("DRigController", DRigController::initialize, 131) == 262);
    assert(testFailures("DMatToVec", DMatToVec::initialize, 27) == 54);
    assert(testFailures("DNumericToFloatArray", DNumericToFloatArray::initialize, 36) == 72);
}
void resetProbe(int failure = -1) {
    resetCalls(failure);
    observed.clear();
    observedBudget = pointReads = 0;
    shortCopy = throwCopy = false;
}
void testInputFailures() {
    DRigController node;
    MDataBlock block;
    resetProbe();
    assert(node.inputBoundary({"unrelated"}, block) == MS::kUnknownParameter && callCount == 0);
    assert(node.inputBoundary({"outputCurve"}, block) == MS::kSuccess && callCount == 15);
    const auto baseline = operations;
    const std::vector<double> expected = {7, 2,    1, 8, 1,   2,  3, 10, 20,   30,
                                          1, 1.25, 3, 1, 0.4, 30, 1, 1,  0.75, 0.25};
    assert(observed == expected && observedBudget == 192);
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < 15; ++failure) {
            resetProbe(failure);
            assert(node.inputBoundary({"outputCurve"}, block) == status);
            assert(callCount == failure + 1 && observed.empty() && observedBudget == 0);
            assert(std::equal(operations.begin(), operations.end(), baseline.begin()));
            resetProbe();
            assert(node.inputBoundary({"outputCurve"}, block) == MS::kSuccess);
            assert(observed == expected && operations == baseline);
        }
    }
    for (int shape = 0; shape < 9; ++shape) {
        for (int axis = 0; axis < 6; ++axis) {
            resetProbe();
            block.values["shapeType"] = shape;
            block.values["dirAxis"] = block.values["taperAxis"] = axis;
            assert(node.inputBoundary({"outputCurve"}, block) == MS::kSuccess);
            assert(observed[0] == shape && observed[12] == axis && observed[13] == axis);
        }
    }
}
void testInputValidation() {
    DRigController node;
    for (const auto& test : std::vector<std::pair<std::string, double>>{{"shapeType", -1},
                                                                        {"shapeType", 9},
                                                                        {"division", 2},
                                                                        {"division", 100001},
                                                                        {"ratio", -0.01},
                                                                        {"ratio", 2.01},
                                                                        {"arrowRatio", -0.01},
                                                                        {"arrowRatio", 1.01},
                                                                        {"arrowWidth", -0.01},
                                                                        {"arrowWidth", 1.01},
                                                                        {"dirAxis", -1},
                                                                        {"dirAxis", 6},
                                                                        {"taperAxis", -1},
                                                                        {"taperAxis", 6},
                                                                        {"bendAxis", -1},
                                                                        {"bendAxis", 2}}) {
        resetProbe();
        MDataBlock block;
        block.values[test.first] = test.second;
        assert(node.inputBoundary({"outputCurve"}, block) == MS::kInvalidParameter);
        assert(observed.empty());
    }
    for (double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        for (const char* attribute :
             {"localScale", "ratio", "taperAmount", "bendAmount", "arrowRatio", "arrowWidth"}) {
            resetProbe();
            MDataBlock block;
            block.values[attribute] = value;
            assert(node.inputBoundary({"outputCurve"}, block) == MS::kInvalidParameter);
            assert(observed.empty());
        }
        for (int component = 0; component < 6; ++component) {
            resetProbe();
            MDataBlock block;
            (component < 3 ? block.offset : block.rotation)[component % 3] = value;
            assert(node.inputBoundary({"outputCurve"}, block) == MS::kInvalidParameter);
            assert(observed.empty());
        }
    }
}
MStatus crossBoundary(DRigController& node, MPointArray& points) try {
    return node.crossBendDeform(points, 1, -1, 0.2) ? MS::kSuccess : MS::kFailure;
}
DOMMY_COMPUTE_CATCH("Rig cross bend test boundary")
void testCopyFailures() {
    DRigController node;
    const std::vector<MPoint> original = {{0.2, 0.5, 0.8}, {-0.3, -0.6, 0.4}};
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        resetProbe(0);
        injectedStatus = status;
        MPointArray points;
        points.values = original;
        assert(crossBoundary(node, points) == MS::kFailure);
        assert(points.values == original && callCount == 1 && pointReads == 0);
    }
    for (bool exception : {false, true}) {
        resetProbe();
        shortCopy = !exception;
        throwCopy = exception;
        MPointArray points;
        points.values = original;
        assert(crossBoundary(node, points) == MS::kFailure);
        assert(points.values == original && callCount == 1 && pointReads == 0);
    }
    resetProbe();
    MPointArray points;
    points.values = original;
    assert(crossBoundary(node, points) == MS::kSuccess && points.values != original &&
           callCount == 1);
}
void testDeformCompatibility() {
    DRigController node;
    // Positive axes retain the archive's taper formula, including bound clamps.
    for (int axis = 0; axis < 3; ++axis) {
        for (double curvature : {-0.7, 0.0, 0.7}) {
            resetProbe();
            MPointArray points;
            points.values = {{0.2, 0.5, 0.8}, {2, -3, 4}, {-2, 3, -4}};
            const auto original = points.values;
            assert(node.taperDeform(points, 1, -1, curvature, static_cast<D_AXIS_TYPE>(axis)));
            for (std::size_t i = 0; i < points.values.size(); ++i) {
                const double input[] = {original[i].x, original[i].y, original[i].z};
                const double actual[] = {points.values[i].x, points.values[i].y,
                                         points.values[i].z};
                const double rate = std::max(-1.0, std::min(1.0, input[axis])) / 2 * curvature;
                for (int component = 0; component < 3; ++component)
                    assert(std::abs(actual[component] -
                                    (component == axis
                                         ? input[component]
                                         : input[component] + input[component] * rate)) < 1e-12);
            }
        }
    }
    // Deterministic two-plane composition checks Cross Arrow's successful copy.
    for (double curvature : {-0.7, 0.0, 0.7}) {
        resetProbe();
        MPointArray points;
        points.values = {{0.2, 0.5, 0.8}, {-0.3, -0.6, 0.4}};
        const auto original = points.values;
        MPointArray first;
        first.values = original;
        assert(node.bendDeform(first, 1, -1, curvature, D_Z_AXIS));
        assert(node.crossBendDeform(points, 1, -1, curvature));
        for (std::size_t i = 0; i < original.size(); ++i) {
            // crossBend's second plane uses x as axial and z as radial coordinate.
            const double r = curvature == 0 ? 0 : 1 / curvature;
            const double angle = std::acos(-1.0) - original[i].x * curvature;
            const double c = std::cos(angle), s = std::sin(angle);
            const double px = curvature == 0 ? original[i].x : r * s - original[i].z * s;
            const double pz = curvature == 0 ? original[i].z : r * c + r - original[i].z * c;
            assert(std::abs(points.values[i].x - (px + first.values[i].x - original[i].x)) < 1e-12);
            assert(std::abs(points.values[i].y - first.values[i].y) < 1e-12);
            assert(std::abs(points.values[i].z - (pz + first.values[i].z - original[i].z)) < 1e-12);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testInitialization(argv[1]);
    testInputFailures();
    testInputValidation();
    testCopyFailures();
    testDeformCompatibility();
    std::cout << "Rig/Utility safety passed: 388 initializer failures, 30 input failures, "
                 "copy failure/length/exception handling and deformation compatibility\n";
}
