#include "curve_copy_doubles.h"
#include "curve_copy.inc"
#include "dccopy_compute_tests.h"

namespace {
void setup(bool surface, bool connected, bool switchUv = false, int reference = 0) {
    reset();
    assert(DCFitCopy::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    geometries["source"] = Geometry();
    geometries["target"] = Geometry();
    geometries["reference"] = Geometry();
    const auto scalar = [](MObject a, double v) { inputs[a.name].number = v; };
    scalar(DCFitCopy::aCountU, 2);
    scalar(DCFitCopy::aCountV, 3);
    scalar(DCFitCopy::aConnectType, connected);
    scalar(DCFitCopy::aConnectUVSwitch, switchUv);
    scalar(DCFitCopy::aSeed, 1);
    scalar(DCFitCopy::aUniformScale, 1);
    scalar(DCFitCopy::aTangentMode, reference ? 2 : 0);
    inputs[DCFitCopy::aScale.name].vec[1] = 1;
    inputs[DCFitCopy::aTangentVector.name].vec[1] = 1;
    inputs[DCFitCopy::inCopyObject.name].connected = true;
    auto& target = inputs[DCFitCopy::inObject.name];
    target.connected = true;
    target.object = {"target"};
    target.type = surface ? MFnData::kNurbsSurface : MFnData::kNurbsCurve;
    auto& ref = inputs[DCFitCopy::inReferenceObject.name];
    ref.connected = reference != 0;
    ref.object = {"reference"};
    ref.type = reference == 2 ? MFnData::kNurbsSurface : MFnData::kNurbsCurve;
    reset();
}
MStatus compute() {
    DCFitCopy node;
    MDataBlock block;
    return node.compute(MPlug(node.thisMObject(), DCFitCopy::outDC), block);
}
void testParameters() {
    double result = 99;
    for (double start : {0.0, 5.0, -7.0}) {
        for (size_t i = 0; i < 2; ++i)
            for (double coordinate : {0.0, 0.25, 1.0}) {
                assert(dommy::curve::checkedCopyParameter(start, start + 4, 2, i, 0, 0, coordinate,
                                                          result));
                assert(result == start + 2 * i + 2 * coordinate);
            }
    }
    assert(dommy::curve::checkedCopyParameter(5, 9, 2, 0, 0, -100, 0, result) && result == 5);
    assert(dommy::curve::checkedCopyParameter(5, 9, 2, 1, 0, -100, 1, result) && result == 9);
    assert(dommy::curve::checkedCopyParameter(5, 9, 2, 0, 0.5, 0, 0, result) && result == 5.5);
    assert(dommy::curve::checkedCopyParameter(5, 9, 2, 0, 0.5, 0, 1, result) && result == 6.5);
    for (auto args : {std::array<double, 3>{{5, 5, 0}},
                      {{9, 5, 0}},
                      {{0, 1, std::numeric_limits<double>::infinity()}}}) {
        result = 99;
        assert(!dommy::curve::checkedCopyParameter(args[0], args[1], 2, 0, args[2], 0, 0, result));
        assert(result == 99);
    }
    result = 99;
    assert(!dommy::curve::checkedCopyParameter(0, 1, 0, 0, 0, 0, 0, result) && result == 99);
    assert(!dommy::curve::checkedCopyParameter(0, 1, 2, 2, 0, 0, 0, result) && result == 99);
    assert(!dommy::curve::checkedCopyParameter(0, 1e308, 1, 0, 1e308, 0, 0, result) &&
           result == 99);
    std::vector<std::array<double, 2>> uv;
    const double large = static_cast<double>(2e38f);
    assert(dommy::curve::normalizedCopyCoordinates(MPointArray{{-large, 0, 2}, {large, 0, 2}}, uv));
    assert(uv[0][0] == 0 && uv[1][0] == 1 && uv[0][1] == 0 && uv[1][1] == 0);
    assert(dommy::curve::normalizedCopyCoordinates(MPointArray{{1e8, 0, 0}, {1e8 + 1, 0, 1}}, uv));
    assert(uv[0][0] == 0 && uv[1][0] == 1);
    const auto prior = uv;
    assert(!dommy::curve::normalizedCopyCoordinates(MPointArray{{NAN, 0, 0}}, uv) && uv == prior);
    assert(!dommy::curve::normalizedCopyCoordinates(MPointArray{}, uv) && uv == prior);
}
int testConnections() {
    int failures = 0;
    for (auto run : {connectFit})
        for (auto dimensions :
             {std::array<unsigned, 2>{{2, 3}}, {{3, 2}}, {{1, 3}}, {{3, 1}}, {{1, 1}}}) {
            reset();
            DCurveOutputBatch batch;
            const auto rows = dimensions[0], cols = dimensions[1];
            assert(run(rows, cols, batch) == MS::kSuccess);
            const int calls = callCount;
            DCurveData output;
            assert(batch.publishTo(output));
            assert(output.curves.size() == rows);
            for (unsigned i = 0; i < rows; ++i) {
                const auto& points = output.curves[i]->points;
                assert(points.length() == 4 * cols + 2 * (cols - 1));
                unsigned index = 0;
                for (unsigned u = 0; u < cols; ++u)
                    for (unsigned p = 0; p < 4; ++p) {
                        const unsigned repeat = 1 + (u > 0 && p == 0) + (u + 1 < cols && p == 3);
                        for (unsigned r = 0; r < repeat; ++r) {
                            assert(points[index].x == 1 + i * cols + u);
                            assert(points[index++].y == 100 * i + 10 * u + p);
                        }
                    }
            }
            for (int i = 0; i < calls; ++i)
                for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                    reset(i);
                    injectedStatus = status;
                    DCurveOutputBatch failed;
                    assert(run(rows, cols, failed) != MS::kSuccess);
                    assert(callCount == i + 1);
                    ++failures;
                }
        }
    return failures;
}
int testCompute() {
    int cases = 0;
    for (bool surface : {false, true})
        for (bool connected : {false, true})
            for (bool switchUv : {false, true})
                for (int reference : {0, 1, 2}) {
                    setup(surface, connected, switchUv, reference);
                    assert(compute() == MS::kSuccess && published == 1 && cleaned == 2);
                    const auto expected = outputData;
                    const auto trace = operations;
                    const int calls = callCount;
                    assert(expected.curves.size() ==
                           (surface ? (connected ? (switchUv ? 3 : 2) : 6) : (connected ? 1 : 2)));
                    for (int i = 0; i < calls; ++i) {
                        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                            reset(i);
                            injectedStatus = status;
                            const auto result = compute();
                            assert(result != MS::kSuccess);
                            // Knot construction and the batch adapter deliberately map
                            // their bool/exception boundary to kFailure. Direct Maya calls
                            // must retain the exact failure code, not substitute validation.
                            if (trace[i].find("array.") != 0 && trace[i] != "batch.insert")
                                assert(result == status);
                            assert(callCount == i + 1);
                            if (trace[i] != "clean")
                                assert(published == 0 && cleaned == 0);
                            ++cases;
                        }
                        reset();
                        throwAt = i;
                        assert(compute() != MS::kSuccess && callCount == i + 1);
                        if (trace[i] != "clean")
                            assert(published == 0 && cleaned == 0);
                        ++cases;
                        if (trace[i] == "array.copy" || trace[i] == "array.resize") {
                            reset();
                            shortAt = i;
                            assert(compute() != MS::kSuccess && published == 0);
                            ++cases;
                        }
                    }
                    reset();
                    assert(compute() == MS::kSuccess && published == 1);
                    assert(outputData.getTotalCvs() == expected.getTotalCvs());
                }
    setup(false, false);
    assert(compute() == MS::kSuccess);
    assert(outputData.curves[0]->points.front().x == 5);
    assert(outputData.curves[0]->points.back().x == 7);
    assert(outputData.curves[1]->points.front().x == 7);
    assert(outputData.curves[1]->points.back().x == 9);
    setup(true, true);
    inputs[DCFitCopy::aCountV.name].number = 0;
    assert(compute() == MS::kSuccess && outputData.curves.empty());
    setup(false, false);
    inputs[DCFitCopy::inObject.name].object = MObject::kNullObj;
    assert(compute() != MS::kSuccess && published == 0);
    setup(false, false);
    geometries["target"].end = 5;
    assert(compute() != MS::kSuccess && published == 0);
    setup(false, false);
    inputs[DCFitCopy::aTangentMode.name].number = 1;
    assert(compute() == MS::kSuccess);
    for (bool switchUv : {false, true}) {
        setup(true, false, switchUv);
        assert(compute() == MS::kSuccess);
        for (unsigned index = 0; index < 6; ++index) {
            const unsigned u = switchUv ? index % 2 : index / 3;
            const unsigned v = switchUv ? index / 2 : index % 3;
            for (unsigned p = 0; p < 4; ++p) {
                const auto& point = outputData.curves[index]->points[p];
                const double profileHeight = (p == 1 || p == 2) ? 1 : 0;
                assert(std::fabs(point.x - (5 + 2 * u + 2 * p / 3.0)) < 1e-12);
                assert(std::fabs(point.y - (-4 + 2 * v / 3.0 + (p >= 2 ? 2 / 3.0 : 0) +
                                            profileHeight)) < 1e-12);
            }
        }
    }
    for (bool surface : {false, true})
        for (bool switchUv : {false, true}) {
            setup(surface, false, switchUv);
            const auto source = geometries.at("source").points;
            auto& matrix = inputs[DCFitCopy::inObject.name].matrix;
            matrix[0][0] = 0;
            matrix[0][1] = 2;
            matrix[1][0] = -3;
            matrix[1][1] = 0;
            matrix[2][2] = 4;
            matrix[3][0] = 10;
            matrix[3][1] = 20;
            matrix[3][2] = 30;
            assert(compute() == MS::kSuccess);
            const auto& first = outputData.curves.front()->points.front();
            assert(first.x == (surface ? 22 : 10) && first.y == 30 && first.z == 30);
            for (unsigned i = 0; i < source.length(); ++i) {
                assert(geometries.at("source").points[i].x == source[i].x);
                assert(geometries.at("source").points[i].y == source[i].y);
            }
            setup(surface, false, switchUv);
            inputs[DCFitCopy::inObject.name].matrix[0][0] = NAN;
            assert(compute() != MS::kSuccess && published == 0);
        }
    setup(false, false, true, 2);
    inputs[DCFitCopy::inReferenceObject.name].object = MObject::kNullObj;
    assert(compute() != MS::kSuccess && published == 0);
    setup(false, false);
    inputs[DCFitCopy::aCountU.name].number = 1000001;
    assert(compute() != MS::kSuccess && published == 0);
    setup(false, false);
    reset();
    DCFitCopy node;
    MDataBlock data;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"unknown"}), data) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
    return cases;
}
} // namespace
int main(int argc, char** argv) {
    const auto schema = schemaTrace("DCFitCopy", DCFitCopy::initialize);
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    assert(argc == 3);
    std::ifstream file(argv[1]);
    assert(file);
    const std::string frozen{std::istreambuf_iterator<char>(file), {}};
    assert(schema == frozen);
    const int init = testFailures("DCFitCopy", DCFitCopy::initialize, 118);
    testParameters();
    const int connections = testConnections();
    const int computeCases = testCompute();
    testDCCopy(argv[2]);
    std::cout << "Curve copy: " << init << " initializer, " << connections << " connection, "
              << computeCases << " compute failure/exception/short-array cases passed.\n";
}
