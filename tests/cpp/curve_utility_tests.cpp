#include "curve_utility_doubles.h"
#include "curve_utility.inc"
#include "curve_transform_cases.h"

namespace {
DCurveData inputData;
void setup() {
    reset();
    assert(DCReform::initialize() == MS::kSuccess);
    assert(DCResample::initialize() == MS::kSuccess);
    assert(DCCharactor::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    shortAttribute = badRamp = false;
    rebuiltReportedCount = -1;
    auto curve =
        DCItem(new DCurveInternalData(MPointArray{{5, 0, 0}, {6, 0, 0}, {8, 0, 0}, {9, 0, 0}},
                                      MDoubleArray{5, 5, 5, 9, 9, 9}, 3, false));
    curve->attributes["marker"] = {10, 20, 30, 40};
    curve->useGeometryFixture = true;
    curve->geometryFixture.mapClosestToDomain = true;
    curve->geometryFixture.points = curve->points;
    curve->geometryFixture.open = false;
    inputData.curves.push_back(curve);
    inputs["inDC"].plugin = &inputData;
    inputs["Length"].number = 1;
    inputs["Sampling"].number = 4;
    inputs["isClamp"].number = 1;
    inputs["SampleType"].number = 1;
    inputs["NumSpan"].number = 4;
    inputs["Degree"].number = 1;
    inputs["KeepRange"].number = 1;
    inputs["Tolerance"].number = .01;
    inputs["LengthAttributeName"].string = "len";
    inputs["WidthAttributeName"].string = "width";
    inputs["ParamAttributeName"].string = "param";
    inputs["BaseWidth"].number = .05;
    inputs["TipWidth"].number = .01;
    inputs["WidthScale"].number = 1;
    inputs["WidthRange"].dvec[1] = .1;
    inputs["Seed"].number = 1;
    inputs["IsNormalizedParam"].number = 1;
    reset();
}
template <class Node> MStatus compute() {
    Node node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), Node::outDC), data);
}
void near(double a, double b) {
    assert(std::abs(a - b) < 1e-6);
}
bool called(const char* name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
void same(const DCurveData& a, const DCurveData& b) {
    assert(a.attributes == b.attributes && a.curves.size() == b.curves.size());
    for (unsigned c = 0; c < a.curves.size(); ++c) {
        const auto& x = *a.curves[c];
        const auto& y = *b.curves[c];
        assert(x.degree == y.degree && x.open == y.open && x.knots == y.knots &&
               x.attributes == y.attributes);
        assert(x.points.size() == y.points.size());
        assertSameMatrix(x.getTransformMatrix(), y.getTransformMatrix());
        for (unsigned i = 0; i < x.points.size(); ++i)
            for (unsigned axis = 0; axis < 3; ++axis)
                near(x.points[i][axis], y.points[i][axis]);
    }
}
DCurveData snapshot(const DCurveData& input) {
    DCurveData copy;
    copy.attributes = input.attributes;
    for (const auto& c : input.curves)
        copy.curves.push_back(DCItem(new DCurveInternalData(*c)));
    return copy;
}
void enableCharacter() {
    for (const auto* name :
         {"AddLengthAttribute", "AddWidthAttribute", "AddParamAttribute", "IsChangeKnot"})
        inputs[name].number = 1;
}
template <class Node> int failures() {
    assert(compute<Node>() == MS::kSuccess);
    const auto expected = outputData, source = snapshot(inputData);
    const auto trace = operations;
    int tests = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            assert(compute<Node>() != MS::kSuccess && callCount == i + 1);
            if (trace[i] != "clean")
                assert(published == 0 && cleaned == 0);
            ++tests;
        }
        reset();
        throwAt = i;
        assert(compute<Node>() != MS::kSuccess && callCount == i + 1);
        if (trace[i] != "clean")
            assert(published == 0 && cleaned == 0);
        ++tests;
        if (trace[i] == "array.copy" || trace[i] == "array.resize" || trace[i] == "curve.cvs" ||
            trace[i] == "curve.knots" || trace[i] == "ramp.sample") {
            reset();
            shortAt = i;
            assert(compute<Node>() != MS::kSuccess && published == 0 && cleaned == 0);
            ++tests;
        }
        same(inputData, source);
    }
    reset();
    assert(compute<Node>() == MS::kSuccess);
    same(outputData, expected);
    return tests;
}
void testReform() {
    for (const auto& matrix : primitiveTransformCases()) {
        setup();
        inputData.curves[0]->setTransformMatrix(matrix);
        assert(compute<DCReform>() == MS::kSuccess);
        assertSameMatrix(outputData.curves[0]->getTransformMatrix(), matrix);
        near(outputData.curves[0]->points.front().x, 5);
        near(outputData.curves[0]->points.back().x, 9);
        assertSameMatrix(inputData.curves[0]->getTransformMatrix(), matrix);
        // The existing resample replacement contract must continue retaining it.
        reset();
        assert(compute<DCResample>() == MS::kSuccess);
        assertSameMatrix(outputData.curves[0]->getTransformMatrix(), matrix);
    }
    for (bool projective : {false, true}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = projective ? 1 : NAN;
        assert(compute<DCReform>() == MS::kInvalidParameter && published == 0);
    }
    setup();
    assert(compute<DCReform>() == MS::kSuccess);
    assert(outputData.curves.size() == 1 && outputData.curves[0]->points.size() == 4);
    for (unsigned i = 0; i < 4; ++i)
        near(outputData.curves[0]->points[i].x, 5. + 4. * i / 3.);
    assert(outputData.curves[0]->open); // Preserve the historical open output.
    setup();
    inputs["Length"].number = 0;
    assert(compute<DCReform>() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["Offset"].number = 10;
    assert(compute<DCReform>() == MS::kSuccess);
    for (const auto& p : outputData.curves[0]->points)
        near(p.x, 9);
    setup();
    inputs["isClamp"].number = 0;
    inputs["Offset"].number = 1;
    assert(compute<DCReform>() == MS::kSuccess);
    near(outputData.curves[0]->points.back().x, 10);
    setup();
    inputData.attributes["len"] = {.5f};
    assert(compute<DCReform>() == MS::kSuccess);
    near(outputData.curves[0]->points.back().x, 7);
    setup();
    inputData.attributes["len"] = {.5f};
    shortAttribute = true;
    assert(compute<DCReform>() != MS::kSuccess && published == 0 && !called("curve.fromData"));
    setup();
    inputs["Sampling"].number = 1000000;
    inputData.curves.resize(11, inputData.curves[0]);
    assert(compute<DCReform>() == MS::kInvalidParameter && !called("curve.fromData"));
}
void testResample() {
    for (int mode : {0, 1, 2}) {
        setup();
        inputs["SampleType"].number = mode;
        assert(compute<DCResample>() == MS::kSuccess);
        const auto& c = *outputData.curves[0];
        assert(!c.open && c.degree == 3);
        if (mode == 2) {
            assert(c.points.size() == 6 && c.attributes.count("marker"));
            const double expected[] = {5, 6, 6 + 2. / 3., 6 + 4. / 3., 8, 9};
            for (unsigned i = 0; i < 6; ++i)
                near(c.points[i].x, expected[i]);
            assert(c.attributes.at("marker") == std::vector<float>({10, 20, 20, 20, 30, 40}));
        } else
            assert(c.points.size() == 7 && c.attributes.empty());
    }
    setup();
    inputs["Degree"].number = 0;
    inputs["KeepCV"].number = 1;
    assert(compute<DCResample>() == MS::kSuccess && outputData.curves[0]->degree == 1 &&
           outputData.curves[0]->points.size() == 4);
    setup();
    rebuiltReportedCount = 10000001;
    assert(compute<DCResample>() == MS::kInvalidParameter && !called("curve.cvs") &&
           published == 0);
    setup();
    rebuiltReportedCount = 8;
    assert(compute<DCResample>() != MS::kSuccess && published == 0);
}
void testCharacter() {
    for (double start : {0., 5., -9., 1e40}) {
        setup();
        enableCharacter();
        auto& g = inputData.curves[0]->geometryFixture;
        g.start = start;
        g.end = start == 1e40 ? 5e40 : start + 4;
        assert(compute<DCCharactor>() == MS::kSuccess);
        const auto& params = outputData.curves[0]->attributes.at("param");
        const double expected[] = {0, .25, .75, 1};
        for (unsigned i = 0; i < 4; ++i)
            near(params[i], expected[i]);
        near(outputData.attributes.at("len")[0], 4);
        const auto& widths = outputData.curves[0]->attributes.at("width");
        for (unsigned i = 0; i < 4; ++i)
            near(widths[i], .05 + (.01 - .05) * i / 3.);
        assert(inputData.curves[0]->attributes.count("marker") &&
               !inputData.curves[0]->attributes.count("param"));
    }
    setup();
    enableCharacter();
    inputs["IsNormalizedParam"].number = 0;
    assert(compute<DCCharactor>() == MS::kSuccess);
    assert(outputData.curves[0]->attributes.at("param") == std::vector<float>({5, 6, 8, 9}));
    setup();
    enableCharacter();
    inputs["WidthType"].number = 1;
    assert(compute<DCCharactor>() == MS::kSuccess);
    for (unsigned i = 0; i < 4; ++i)
        near(outputData.curves[0]->attributes.at("width")[i], double(i) / 3);
    setup();
    enableCharacter();
    inputs["IsRandom"].number = 1;
    inputData.curves.push_back(DCItem(new DCurveInternalData(*inputData.curves[0])));
    assert(compute<DCCharactor>() == MS::kSuccess);
    std::mt19937 engine(1);
    std::uniform_real_distribution<double> distribution(0, .1);
    for (const auto& c : outputData.curves)
        for (unsigned i = 0; i < 4; ++i)
            near(c->attributes.at("width")[i], .05 + (.01 - .05) * i / 3. + distribution(engine));
    setup();
    enableCharacter();
    inputs["IsRandom"].number = 1;
    inputs["WidthRange"].dvec[0] = inputs["WidthRange"].dvec[1] = 1e300;
    assert(compute<DCCharactor>() == MS::kSuccess);
    near(outputData.curves[0]->attributes.at("width")[0], .05);
    for (int variant = 0; variant < 10; ++variant) {
        setup();
        enableCharacter();
        switch (variant) {
        case 0:
            inputs["WidthScale"].number = 1e40;
            break;
        case 1:
            inputData.curves[0]->geometryFixture.length = 1e40;
            break;
        case 2:
            inputs["IsNormalizedParam"].number = 0;
            inputData.curves[0]->geometryFixture.start = 1e40;
            break;
        case 3:
            inputData.curves[0]->geometryFixture.end = 5;
            break;
        case 4:
            inputs["WidthType"].number = 2;
            break;
        case 5:
            inputs["KnotType"].number = 2;
            break;
        case 6:
            inputs["WidthType"].number = 1;
            badRamp = true;
            break;
        case 7:
            inputs["Seed"].number = -1;
            break;
        case 8:
            inputs["IsRandom"].number = 1;
            inputs["WidthRange"].dvec[0] = -1e308;
            inputs["WidthRange"].dvec[1] = 1e308;
            break;
        case 9:
            inputs["WidthScale"].number = NAN;
            break;
        }
        assert(compute<DCCharactor>() != MS::kSuccess && published == 0);
    }
}
template <class Node> void testCommon() {
    setup();
    inputData.curves.clear();
    assert(compute<Node>() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute<Node>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0].reset();
    assert(compute<Node>() == MS::kInvalidParameter && published == 0);
    setup();
    Node node;
    MDataBlock data;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"other"}), data) ==
               MS::kUnknownParameter &&
           operations.empty());
}
void testInvalidControls() {
    for (const auto& value : std::vector<std::pair<const char*, double>>{
             {"Sampling", -1}, {"Sampling", 1}, {"Length", -1}, {"Offset", NAN}}) {
        setup();
        inputs[value.first].number = value.second;
        assert(compute<DCReform>() == MS::kInvalidParameter && published == 0 &&
               !called("curve.fromData"));
    }
    for (const auto& value : std::vector<std::pair<const char*, double>>{{"SampleType", -1},
                                                                         {"SampleType", 3},
                                                                         {"Length", 0},
                                                                         {"Length", NAN},
                                                                         {"NumSpan", 0},
                                                                         {"NumSpan", 1000001},
                                                                         {"Degree", 2},
                                                                         {"KeepRange", 3},
                                                                         {"EndKnots", 2},
                                                                         {"Tolerance", -1},
                                                                         {"Tolerance", INFINITY}}) {
        setup();
        inputs[value.first].number = value.second;
        assert(compute<DCResample>() == MS::kInvalidParameter && published == 0 &&
               !called("curve.fromData"));
    }
    setup();
    inputData.curves[0]->geometryFixture.end = 5;
    assert(compute<DCReform>() == MS::kInvalidParameter && published == 0);
    setup();
    inputData.curves[0]->geometryFixture.start = 1e40;
    inputData.curves[0]->geometryFixture.end = 5e40;
    assert(compute<DCReform>() == MS::kInvalidParameter && published == 0);
}
int testDefaultRamp() {
    setup();
    rampEntries = 0;
    defaultRamp = MFloatArray();
    DCCharactor node;
    node.postConstructor();
    assert(defaultRamp == MFloatArray({0, 1}) && rampEntries == 2);
    const auto trace = operations;
    int tests = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            rampEntries = 0;
            defaultRamp = MFloatArray();
            node.postConstructor();
            assert(callCount == i + 1 && defaultRamp.empty());
            ++tests;
        }
        reset();
        throwAt = i;
        rampEntries = 0;
        defaultRamp = MFloatArray();
        node.postConstructor();
        assert(callCount == i + 1 && defaultRamp.empty());
        ++tests;
        if (trace[i] == "array.copy" || trace[i] == "array.resize") {
            reset();
            shortAt = i;
            rampEntries = 0;
            defaultRamp = MFloatArray();
            node.postConstructor();
            assert(defaultRamp.empty() && !called("defaultRamp.set"));
            ++tests;
        }
    }
    reset();
    rampEntries = 3;
    defaultRamp = {.2f, .3f, .4f};
    node.postConstructor();
    assert(defaultRamp.size() == 3 && !called("defaultRamp.set"));
    reset();
    return tests;
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream fixture(argv[1]);
    assert(fixture.good());
    const auto schema = schemaTrace("DCReform", DCReform::initialize) +
                        schemaTrace("DCResample", DCResample::initialize) +
                        schemaTrace("DCCharactor", DCCharactor::initialize);
    assert(schema ==
           std::string(std::istreambuf_iterator<char>(fixture), std::istreambuf_iterator<char>()));
    const int initializerFailures = testFailures("DCReform", DCReform::initialize, 44) +
                                    testFailures("DCResample", DCResample::initialize, 72) +
                                    testFailures("DCCharactor", DCCharactor::initialize, 108);
    testReform();
    testResample();
    testCharacter();
    testCommon<DCReform>();
    testCommon<DCResample>();
    testCommon<DCCharactor>();
    testInvalidControls();
    setup();
    inputData.attributes["len"] = {1};
    int count = failures<DCReform>();
    for (int mode : {0, 1, 2}) {
        setup();
        inputs["SampleType"].number = mode;
        count += failures<DCResample>();
    }
    for (int width : {0, 1})
        for (int normalized : {0, 1}) {
            setup();
            enableCharacter();
            inputs["WidthType"].number = width;
            inputs["IsNormalizedParam"].number = normalized;
            count += failures<DCCharactor>();
        }
    const int rampFailures = testDefaultRamp();
    std::cout << "Curve utility checks passed: " << initializerFailures << " initializer failures, "
              << count << " compute failure/exception/short-array cases, " << rampFailures
              << " default-ramp failure cases.\n";
}
