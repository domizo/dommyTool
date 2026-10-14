#include "animation_boundary_doubles.h"
#include "animation_boundary_methods.inc"

namespace {
void configure(int scenario) {
    resetRun();
    shortScalarInput = false;
    inputs.clear();
    inputData = DPData();
    stagedData = DPData();
    publishedData = DPData();
    inputs["CurrentTime"].number = 12.25;
    inputs["globalOffset"].number = 0.125;
    inputs["AnimAttributeName"].string = "amat";
    inputs["OffsetAttribute"].string = "offset";
    inputs["StretchAttribute"].string = "stretch";
    inputs["OffsetType"].number = scenario % 3;
    inputs["StretchType"].number = (scenario / 3) % 3;
    inputs["InOffsetArray"].connected = scenario != 9;
    inputs["InStretchArray"].connected = scenario != 9;
    inputs["InOffsetArray"].scalars = {0.1, -2.25, 3.5};
    inputs["InStretchArray"].scalars = {0.3, 0, -2.5};
    inputs["animationMatrix"].connected = scenario != 10;
    inputData.scalars["offset"] = {0.1, -2.25, 3.5};
    inputData.scalars["stretch"] = {0.3, 0, -2.5};
    inputData.vectors["custom"] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    inputData.matrices["amat"] = std::vector<MMatrix>(3);
    if (scenario == 9)
        inputs["OffsetType"].number = inputs["StretchType"].number = 2;
    if (scenario == 11 || scenario == 12) {
        inputs["OffsetType"].number = inputs["StretchType"].number = 1;
        inputs["OffsetAttribute"].string = scenario == 11 ? "missing" : "custom";
        inputs["StretchAttribute"].string = scenario == 11 ? "missing" : "amat";
    }
    if (scenario == 13) {
        inputs["OffsetType"].number = inputs["StretchType"].number = 1;
        inputs["AnimAttributeName"].string = "offset";
    }
    if (scenario == 14) {
        inputs["OffsetType"].number = inputs["StretchType"].number = 2;
        inputs["InOffsetArray"].scalarInput = inputs["InStretchArray"].scalarInput = true;
        inputs["InOffsetArray"].number = -0.1;
        inputs["InStretchArray"].number = 1.3;
    }
    if (scenario == 15) {
        inputData.points.clear();
        inputData.scalars.clear();
        inputData.vectors.clear();
        inputData.matrices.clear();
    }
    resetRun();
}
MStatus compute() {
    MDataBlock block;
    DPAnimMatrix node;
    return node.compute(MPlug(node.thisMObject(), DPAnimMatrix::outDP), block);
}
std::string trace() {
    std::ostringstream s;
    s << std::setprecision(17);
    for (const auto& p : publishedData.points)
        s << "p " << p.x << ' ' << p.y << ' ' << p.z << '\n';
    for (const auto& a : publishedData.scalars) {
        s << "s " << a.first;
        for (double v : a.second)
            s << ' ' << v;
        s << '\n';
    }
    for (const auto& a : publishedData.vectors) {
        s << "v " << a.first << '\n';
        for (const auto& v : a.second)
            s << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    for (const auto& a : publishedData.matrices) {
        s << "m " << a.first << '\n';
        for (const auto& m : a.second) {
            for (const auto& row : m.values)
                for (double v : row)
                    s << v << ' ';
            s << '\n';
        }
    }
    s << "times";
    for (double t : sampledTimes)
        s << ' ' << t;
    return s.str() + '\n';
}
std::string normalTraces() {
    std::string result;
    for (int scenario = 0; scenario < 16; ++scenario) {
        configure(scenario);
        assert(compute() == MS::kSuccess);
        assert(publications == 1 && cleanCalls == 1);
        result += "animation " + std::to_string(scenario) + "\n" + trace();
    }
    return result;
}
void noResult() {
    assert(publications == 0 && cleanCalls == 0 && outOfBounds == 0);
}
int failureCases(int scenario) {
    configure(scenario);
    assert(compute() == MS::kSuccess);
    const auto baseline = operations;
    const auto expected = trace();
    int tested = 0;
    for (int i = 0; i < static_cast<int>(baseline.size()); ++i) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            configure(scenario);
            failAt = i;
            injectedStatus = code;
            assert(compute() == (baseline[i].find("attribute ") == 0 ? MS::kFailure : code));
            assert(callCount == i + 1);
            if (baseline[i] == "clean")
                assert(publications == 1 && cleanCalls == 0);
            else
                noResult();
            configure(scenario);
            assert(compute() == MS::kSuccess && trace() == expected);
            ++tested;
        }
        configure(scenario);
        throwAt = i;
        assert(compute() == MS::kFailure);
        if (baseline[i] == "clean")
            assert(publications == 1 && cleanCalls == 0);
        else
            noResult();
        ++tested;
    }
    return tested;
}
void invalidCases() {
    configure(0);
    inputData.valid = false;
    assert(compute() == MS::kInvalidParameter);
    noResult();
    configure(0);
    inputData.fakeCount = dommy::generation::kMaxGeneratedTransformCount + 1;
    assert(compute() == MS::kInvalidParameter);
    noResult();
    assert(dataCopies == 0);
    assert(std::find(operations.begin(), operations.end(), "plugin.create") == operations.end());
    configure(0);
    throwDataCopy = true;
    assert(compute() == MS::kFailure);
    noResult();
    for (const char* name : {"OffsetType", "StretchType"})
        for (double value : {-1., 3.}) {
            configure(0);
            inputs[name].number = value;
            assert(compute() == MS::kInvalidParameter);
            noResult();
        }
    for (const char* name : {"CurrentTime", "globalOffset"})
        for (double value :
             {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            configure(0);
            inputs[name].number = value;
            assert(compute() == MS::kInvalidParameter);
            noResult();
        }
    configure(0);
    inputs["AnimAttributeName"].string = "";
    assert(compute() == MS::kInvalidParameter);
    noResult();
    for (const char* name : {"InOffsetArray", "InStretchArray"}) {
        for (std::size_t count : {2U, 4U}) {
            configure(8);
            inputs[name].scalars.resize(count);
            assert(compute() == MS::kInvalidParameter);
            noResult();
        }
        for (double value : {1e100, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
            configure(8);
            inputs[name].scalars[0] = value;
            assert(compute() == MS::kInvalidParameter);
            noResult();
        }
    }
    for (const char* name : {"offset", "stretch"}) {
        configure(4);
        inputData.scalars[name].pop_back();
        assert(compute() == MS::kFailure);
        noResult();
        configure(4);
        inputData.scalars[name][0] = std::numeric_limits<double>::quiet_NaN();
        assert(compute() == MS::kFailure);
        noResult();
    }
    configure(4);
    shortAttribute = true;
    assert(compute() == MS::kFailure);
    noResult();
    configure(8);
    shortScalarInput = true;
    assert(compute() == MS::kFailure);
    noResult();
    configure(4);
    inputs["CurrentTime"].number = std::numeric_limits<double>::max();
    inputData.scalars["stretch"][0] = 0.1;
    assert(compute() == MS::kInvalidParameter);
    noResult();
    configure(15);
    inputs["OffsetType"].number = inputs["StretchType"].number = -1;
    inputs["globalOffset"].number = std::numeric_limits<double>::quiet_NaN();
    inputs["AnimAttributeName"].string = "";
    assert(compute() == MS::kSuccess); // Empty data still passes through unused controls.
    configure(0);
    MDataBlock block;
    DPAnimMatrix node;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"unknown"}), block) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
}
std::string readFile(const char* path) {
    std::ifstream f(path);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace

int main(int argc, char** argv) {
    const std::string schema = schemaTrace("DPAnimMatrix", DPAnimMatrix::initialize) +
                               schemaTrace("DPSignal", DPSignal::initialize) +
                               schemaTrace("DPSignal3D", DPSignal3D::initialize);
    if (argc == 2 && std::string(argv[1]) == "--budget") {
        configure(0);inputData.metadataBytes=3000U;resetRun();
        assert(compute()==MS::kFailure && dataCopies==0 && publications==0);
        assert(std::find(operations.begin(),operations.end(),"plugin.create")==operations.end());
        assert(inputData.points[0].x==1 && inputData.metadataBytes==3000U);
        inputData.metadataBytes=0U;resetRun();assert(compute()==MS::kSuccess && publications==1);
        publishedData.points[0].x=99;assert(inputData.points[0].x==1);
        std::cout<<"Matrix animation aggregate budget, pre-copy rejection and retry: OK\n";return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--trace") {
        std::cout << normalTraces();
        return 0;
    }
    assert(argc == 3);
    assert(schema == readFile(argv[1]));
    assert(normalTraces() == readFile(argv[2]));
    const int initializationCases = testFailures("DPAnimMatrix", DPAnimMatrix::initialize, 65) +
                                    testFailures("DPSignal", DPSignal::initialize, 138) +
                                    testFailures("DPSignal3D", DPSignal3D::initialize, 194);
    int computeCases = 0;
    for (int scenario : {0, 4, 8, 9, 10, 11, 12, 13, 14, 15})
        computeCases += failureCases(scenario);
    invalidCases();
    std::cout << "Animation boundaries: " << initializationCases << " initializer and "
              << computeCases
              << " compute failure cases; three schemas and 16 normal traces passed.\n";
}
