#include "curve_path_doubles.h"
#include "curve_path.inc"
#include "curve_transform_cases.h"

namespace {
DCurveData inputData;
std::vector<MVector> pointVectors(const std::string& name) {
    const auto field = std::dynamic_pointer_cast<const DPVec3fAttr>(pointOutput.getAttr(name));
    assert(field);
    std::vector<MVector> values;
    for (const auto& value : field->value()) values.push_back({value.x, value.y, value.z});
    return values;
}
const std::vector<double>& pointParameters() {
    const auto field = std::dynamic_pointer_cast<const DPDoubleAttr>(pointOutput.getAttr("param"));
    assert(field);
    return field->value();
}
void setup() {
    reset();
    assert(DCMotionPath::initialize() == MS::kSuccess);
    assert(DCDivide::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    shortAttribute = shortPointStorage = false;
    rejectPointAttribute = false;
    for (double start : {5., -9.}) {
        auto curve =
            DCItem(new DCurveInternalData(MPointArray{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}},
                                          MDoubleArray{0, 0, 0, 1, 1, 1}, 3, false));
        curve->useGeometryFixture = true;
        curve->geometryFixture.start = start;
        curve->geometryFixture.end = start + 4;
        curve->attributes["marker"] = {1, 2, 3, 4};
        inputData.curves.push_back(curve);
    }
    inputs["inDC"].plugin = &inputData;
    inputs["Speed"].number = 1;
    inputs["PathPosition"].number = .5;
    inputs["TangentVector"].vec[1] = 1;
    inputs["PositionAttribute"].string = "param";
    inputs["OffsetAttribute"].string = "offset";
    inputs["SpeedAttribute"].string = "speed";
    inputs["NormalAttribute"].string = "N";
    inputs["TangentAttribute"].string = "T";
    inputs["ParamAttribute"].string = "param";
    inputs["Count"].number = 2;
    inputs["Sampling"].number = 4;
    inputs["StepLength"].number = .1;
    reset();
}
template <class Node> MObject outputAttribute();
template <> MObject outputAttribute<DCMotionPath>() {
    return DCMotionPath::outDP;
}
template <> MObject outputAttribute<DCDivide>() {
    return DCDivide::outDC;
}
template <class Node> MStatus compute() {
    Node node;
    MDataBlock block;
    return node.compute(MPlug(node.thisMObject(), outputAttribute<Node>()), block);
}
void near(double a, double b) {
    assert(std::abs(a - b) < 1e-6);
}
bool called(const char* name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
void enableExports() {
    for (const auto* name : {"IsExportNormal", "IsExportTangent", "IsExportParam"})
        inputs[name].number = 1;
}
void checkInput();
void testOutputAttributes() {
    const char* names[] = {"NormalAttribute", "TangentAttribute", "ParamAttribute"};
    const char* flags[] = {"IsExportNormal", "IsExportTangent", "IsExportParam"};
    for (unsigned int first = 0; first < 3; ++first)
        for (unsigned int second = first + 1; second < 3; ++second)
            for (unsigned int mask = 0; mask < 8; ++mask) {
                setup();
                for (unsigned int i = 0; i < 3; ++i) inputs[flags[i]].number = (mask >> i) & 1U;
                inputs[names[first]].string = inputs[names[second]].string = "shared";
                const bool collision = (mask & (1U << first)) && (mask & (1U << second));
                assert(compute<DCMotionPath>() ==
                       (collision ? MS::kInvalidParameter : MS::kSuccess));
                if (collision) assert(!published && !cleaned && !called("plugin.create"));
                checkInput();
            }
    for (unsigned int field = 0; field < 3; ++field) {
        for (const std::string& invalid : {
                 std::string(dommy::generation::kMaxGeneratedStringByteCount + 1U, 'x')}) {
            setup(); inputs[flags[field]].number = 1; inputs[names[field]].string = invalid.c_str();
            assert(compute<DCMotionPath>() == MS::kInvalidParameter && !published);
            inputs[flags[field]].number = 0; reset();
            assert(compute<DCMotionPath>() == MS::kSuccess);
        }
        setup(); inputs[flags[field]].number = 1; inputs[names[field]].string = "";
        assert(compute<DCMotionPath>() == MS::kSuccess && pointOutput.fields.empty());
        setup(); inputs[flags[field]].number = 1; rejectPointAttribute = true;
        assert(compute<DCMotionPath>() == MS::kFailure && !published && !cleaned);
        checkInput(); rejectPointAttribute = false; reset();
        assert(compute<DCMotionPath>() == MS::kSuccess && pointOutput.fields.size() == 1);
    }
    setup(); enableExports(); inputData.curves.clear();
    assert(compute<DCMotionPath>() == MS::kSuccess && pointOutput.fields.size() == 3);
    for (const auto& field : pointOutput.fields) assert(field.second->attrSize() == 0);
    // Legacy storage semantics are not a test-side cross-type replacement.
    DPData owner;
    owner.addAttr("same", DPVec3fAttr(std::vector<Imath::V3f>{}));
    owner.addAttr("same", DPDoubleAttr(std::vector<double>{}));
    assert(owner.getAttr("same")->getAttrType() == DP_ATTR_VEC3F);
}
void testRestInputBoundary() {
    for (int index : {0, 1})
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            setup(); failAt = index; injectedStatus = code;
            DCRest node; MDataBlock data;
            assert(node.compute(MPlug({}, DCRest::outDC), data) == code);
            assert(callCount == index + 1 && !published && !cleaned);
        }
    setup(); inputData.curves[0].reset(); DCRest node; MDataBlock data;
    assert(node.compute(MPlug({}, DCRest::outDC), data) == MS::kInvalidParameter);
    setup(); inputs["inDC"].plugin = nullptr;
    assert(node.compute(MPlug({}, DCRest::outDC), data) == MS::kInvalidParameter);
    setup(); assert(node.compute(MPlug({}, DCRest::outDC), data) == MS::kSuccess);
    reset(); assert(node.compute(MPlug({}, {"other"}), data) == MS::kUnknownParameter);
    assert(callCount == 0);
}
void checkInput() {
    assert(inputData.curves.size() == 2);
    for (const auto& c : inputData.curves) {
        assert(c->degree == 3 && !c->open && c->points.size() == 4);
        assert(c->attributes.at("marker") == std::vector<float>({1, 2, 3, 4}));
        assert(c->knots == MDoubleArray({0, 0, 0, 1, 1, 1}));
        for (unsigned i = 0; i < 4; ++i)
            near(c->points[i].x, i);
    }
}
template <class Node> int failures() {
    assert(compute<Node>() == MS::kSuccess);
    const auto trace = operations;
    int tests = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            const auto result = compute<Node>();
            if (trace[i] == "curve.normal" && status == MS::kInvalidParameter) {
                assert(result == MS::kSuccess); // Undefined normal uses the stable-axis fallback.
            } else {
                assert(result != MS::kSuccess && callCount == i + 1);
                if (trace[i] != "clean")
                    assert(published == 0 && cleaned == 0);
            }
            ++tests;
        }
        reset();
        throwAt = i;
        assert(compute<Node>() != MS::kSuccess && callCount == i + 1);
        if (trace[i] != "clean")
            assert(published == 0 && cleaned == 0);
        ++tests;
        if (trace[i] == "array.resize" || trace[i] == "array.copy" || trace[i] == "batch.insert") {
            reset();
            shortAt = i;
            assert(compute<Node>() != MS::kSuccess && published == 0 && cleaned == 0);
            ++tests;
        }
        checkInput();
    }
    reset();
    assert(compute<Node>() == MS::kSuccess);
    return tests;
}
void testTransforms() {
    for (const auto& matrix : primitiveTransformCases()) {
        setup();
        for (auto& c : inputData.curves)
            c->setTransformMatrix(matrix);
        assert(compute<DCDivide>() == MS::kSuccess);
        for (const auto& c : outputData.curves)
            assertSameMatrix(c->getTransformMatrix(), matrix);
        near(outputData.curves[0]->points.front().x, 5);
        near(outputData.curves[1]->points.back().x, 9);
        for (const auto& c : inputData.curves)
            assertSameMatrix(c->getTransformMatrix(), matrix);
        checkInput();
    }
    setup();
    enableExports();
    inputData.curves[0]->primitiveMatrix[3][0] = 100;
    inputData.curves[1]->primitiveMatrix[3][0] = -50;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], 107);
    near(pointOutput.points[1][0], -57);
    near(pointParameters()[0], .5);
    near(pointVectors("T")[0].x, 1);
    setup();
    enableExports();
    const auto matrix = primitiveTransformCases()[2];
    inputData.curves[0]->setTransformMatrix(matrix);
    MMatrix secondMatrix;
    secondMatrix[0][0] = 2;
    secondMatrix[1][1] = -3;
    inputData.curves[1]->setTransformMatrix(secondMatrix);
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], 100);
    near(pointOutput.points[0][1], -6);
    near(pointOutput.points[0][2], 7);
    near(pointVectors("T")[0].y, 1);
    near(pointVectors("N")[0].x, -1);
    // The second curve has its own reflected frame: transport in world space,
    // without applying its matrix again to the already-world-space normal.
    near(pointVectors("T")[1].x, 1);
    near(pointVectors("N")[1].y, 1);
    assertSameMatrix(inputData.curves[0]->getTransformMatrix(), matrix);
    assertSameMatrix(inputData.curves[1]->getTransformMatrix(), secondMatrix);
    setup();
    enableExports();
    inputs["TangentMode"].number = 1;
    inputs["TangentVector"].vec[2] = 1;
    inputData.curves[0]->setTransformMatrix(matrix);
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointVectors("N")[0].x, -.6);
    near(pointVectors("N")[0].z, -.8);
    setup();
    enableExports();
    inputData.curves[0]->setTransformMatrix(primitiveTransformCases()[3]);
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointVectors("T")[0].x, 2 / std::sqrt(5.));
    near(pointVectors("N")[0].x, -1 / std::sqrt(5.));
    near(pointVectors("N")[0] * pointVectors("T")[0], 0);
    setup();
    enableExports();
    inputData.curves[0]->setTransformMatrix(primitiveTransformCases()[4]);
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], -14);
    near(pointVectors("T")[0].x, -1);
    setup();
    inputData.curves[0]->primitiveMatrix[0][0] = 0;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    enableExports();
    inputData.curves[0]->primitiveMatrix[1][1] = 0;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointVectors("N")[0].y, 1);
    setup();
    enableExports();
    inputs["PathPosition"].number = 0;
    inputData.curves[0]->geometryFixture.start = 0;
    inputData.curves[0]->geometryFixture.end = 4;
    inputData.curves[0]->primitiveMatrix[0][0] = 1e200;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointVectors("T")[0].x, 1);
    setup();
    inputData.curves[0]->primitiveMatrix[3][0] = 1e40;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    for (bool projective : {false, true}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = projective ? 1 : NAN;
        assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
        reset();
        assert(compute<DCDivide>() == MS::kInvalidParameter && published == 0);
    }
}
void testMotion() {
    for(double offset:{std::ldexp(1.0,60),-std::ldexp(1.0,60)}) {
        setup();
        enableExports();
        inputs["WrapMode"].number=2;
        inputs["PathPosition"].number=.25;
        inputs["Offset"].number=offset;
        assert(compute<DCMotionPath>()==MS::kSuccess);
        near(pointOutput.points[0][0],6);
        near(pointOutput.points[1][0],-8);
        for(double value:pointParameters())near(value,.25);
    }
    setup();
    enableExports();
    inputs["Offset"].number = .5;
    inputs["Speed"].number = 2;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    assert(pointOutput.numberOfPoints() == 2);
    near(pointOutput.points[0][0], 8);
    near(pointOutput.points[1][0], -6);
    for (unsigned i = 0; i < 2; ++i) {
    near(pointParameters()[i], .75);
    const auto n = pointVectors("N")[i], t = pointVectors("T")[i];
        near(n.y, 1);
        near(n.length(), 1);
        near(t.x, 1);
        near(n * t, 0);
    }
    assert(!called("array.append"));
    setup();
    inputs["WrapMode"].number = 1;
    inputs["PathPosition"].number = 2;
    inputs["Offset"].number = 10;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], 9);
    setup();
    inputs["WrapMode"].number = 2;
    inputs["PathPosition"].number = 1.25;
    inputs["Offset"].number = 1;
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], 7);
    setup();
    inputs["Type"].number = 1;
    inputData.attributes["param"] = {.25, .75};
    inputData.attributes["offset"] = {1, 0};
    inputData.attributes["speed"] = {2, 3};
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointOutput.points[0][0], 8);
    near(pointOutput.points[1][0], -6);
    for (const auto* name : {"param", "offset", "speed"}) {
        setup();
        inputs["Type"].number = 1;
        inputData.attributes[name] = {.5, .5};
        shortAttribute = true;
        assert(compute<DCMotionPath>() != MS::kSuccess && !called("curve.fromData") &&
               published == 0);
    }
    setup();
    enableExports();
    inputs["TangentMode"].number = 1;
    inputs["TangentVector"].vec[0] = 1;
    inputs["TangentVector"].vec[1] = 0;
    assert(compute<DCMotionPath>() == MS::kSuccess && !called("curve.normal"));
    near(pointVectors("N")[0].y, 1);
    setup();
    enableExports();
    inputData.curves[1]->geometryFixture.tangent = MVector(0, 1, 0);
    assert(compute<DCMotionPath>() == MS::kSuccess);
    near(pointVectors("N")[1].x, -1);
    near(pointVectors("T")[1].y, 1);
    setup();
    shortPointStorage = true;
    assert(compute<DCMotionPath>() != MS::kSuccess && published == 0);
    for (const auto* name : {"Type", "WrapMode", "TangentMode"}) {
        setup();
        inputs[name].number = -1;
        assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
        setup();
        inputs[name].number = 3;
        assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    }
    for (const auto* name : {"PathPosition", "Offset", "Speed"}) {
        setup();
        inputs[name].number = NAN;
        assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    }
    setup();
    inputs["TangentVector"].vec[0] = INFINITY;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter);
    setup();
    inputData.curves[0]->geometryFixture.tangent = MVector();
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0]->geometryFixture.end = 5;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0]->geometryFixture.length = 0;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    inputs["Offset"].number = 1e308;
    inputs["Speed"].number = 2;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0]->geometryFixture.matrix[3][0] = 1e40;
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves.resize(dommy::generation::kMaxGeneratedCurveCount + 1, inputData.curves[0]);
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && !called("array.resize") &&
           !called("plugin.create"));
    setup();
    auto source = inputData.curves[0];
    source->points.resize(1001, MPoint(0, 0, 0));
    source->knots.resize(1003, 1);
    source->attributes.clear();
    inputData.curves.assign(10000, source);
    assert(compute<DCMotionPath>() == MS::kInvalidParameter && !called("array.resize") &&
           !called("plugin.create"));
}
void testDivide() {
    setup();
    assert(compute<DCDivide>() == MS::kSuccess);
    assert(outputData.curves.size() == 4 && outputData.getTotalCvs() == 16);
    const double starts[] = {5, 7, -9, -7};
    for (unsigned c = 0; c < 4; ++c) {
        const auto& curve = *outputData.curves[c];
        assert(curve.open && curve.degree == 3);
        for (unsigned i = 0; i < 4; ++i)
            near(curve.points[i].x, starts[c] + 2. * i / 3.);
    }
    assert(std::count(operations.begin(), operations.end(), "batch.insert") == 1);
    setup();
    inputs["SpaceLength"].number = .2;
    inputs["IsEdgeSpace"].number = 1;
    assert(compute<DCDivide>() == MS::kSuccess);
    near(outputData.curves[0]->points.front().x, 5.4);
    near(outputData.curves[0]->points.back().x, 6.8);
    setup();
    inputs["DivideType"].number = 1;
    inputs["Count"].number = 5;
    inputs["StepLength"].number = .4;
    inputs["SpaceLength"].number = .1;
    assert(compute<DCDivide>() == MS::kSuccess && outputData.curves.size() == 4);
    near(outputData.curves[0]->points.back().x, 6.6);
    near(outputData.curves[1]->points.front().x, 7);
    setup();
    inputs["SpaceLength"].number = 10;
    assert(compute<DCDivide>() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["Offset"].number = .25;
    assert(compute<DCDivide>() == MS::kSuccess);
    near(outputData.curves[0]->points.front().x, 5.25);
    for (const auto* name : {"Count", "Sampling"}) {
        setup();
        inputs[name].number = -1;
        assert(compute<DCDivide>() == MS::kInvalidParameter && !called("plugin.create"));
    }
    setup();
    inputs["Sampling"].number = 3;
    assert(compute<DCDivide>() == MS::kInvalidParameter && published == 0);
    setup();
    inputs["Count"].number = 1000000;
    inputs["Sampling"].number = 1000000;
    assert(compute<DCDivide>() == MS::kInvalidParameter && !called("plugin.create"));
    setup();
    inputData.curves[0]->geometryFixture.matrix[3][0] = 1e40;
    assert(compute<DCDivide>() != MS::kSuccess && published == 0);
}
template <class Node> void testCommon() {
    setup();
    inputData.curves.clear();
    assert(compute<Node>() == MS::kSuccess && published == 1);
    if (std::is_same<Node, DCMotionPath>::value)
        assert(pointOutput.points.empty());
    else
        assert(outputData.curves.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute<Node>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0].reset();
    assert(compute<Node>() == MS::kInvalidParameter && published == 0);
    setup();
    Node node;
    MDataBlock block;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"unknown"}), block) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto schema = schemaTrace("DCMotionPath", DCMotionPath::initialize) +
                        schemaTrace("DCDivide", DCDivide::initialize);
    std::ifstream file(argv[1]);
    assert(file);
    const std::string expected((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    assert(schema == expected);
    const int initFailures = testFailures("DCMotionPath", DCMotionPath::initialize, 109) +
                             testFailures("DCDivide", DCDivide::initialize, 45);
    testTransforms();
    testMotion();
    testOutputAttributes();
    testRestInputBoundary();
    testDivide();
    testCommon<DCMotionPath>();
    testCommon<DCDivide>();
    setup();
    int computeFailures = failures<DCMotionPath>();
    setup();
    enableExports();
    computeFailures += failures<DCMotionPath>();
    setup();
    inputs["Type"].number = 1;
    inputData.attributes["param"] = {.25, .75};
    inputData.attributes["offset"] = {0, 0};
    inputData.attributes["speed"] = {1, 1};
    computeFailures += failures<DCMotionPath>();
    setup();
    computeFailures += failures<DCDivide>();
    std::cout << "curve path safety: " << initFailures << " initializer failures, "
              << computeFailures << " compute status/exception/short-array cases passed\n";
}
