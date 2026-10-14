#include "motion_rotation_doubles.h"
#define CHECK_MSTATUS_AND_RETURN_IT(x) DOMMY_RETURN_IF_FAILURE(x)
struct DCurveData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
std::string controlTrace;
#include "curve_boundary_controls.inc"

namespace {
template <class T> std::string normalControls(int scenario) {
    resetRun();
    assert(T::initialize() == MS::kSuccess);
    inputs.clear();
    for (const auto& operation : operations) {
        if (operation.find("create ") == 0) {
            const auto end = operation.find(' ', 7);
            Input& input = inputs[operation.substr(7, end - 7)];
            input.number = scenario ? -1.25 : 2.125;
            input.string = scenario ? "custom" : "N";
        }
    }
    resetRun();
    T node;
    MDataBlock data;
    assert(node.readControls(MPlug(node.thisMObject(), T::outDC), data) == MS::kSuccess);
    return controlTrace;
}
template <class T> int failControls(int expectedCalls) {
    const auto trace = normalControls<T>(0);
    assert(callCount == expectedCalls);
    int cases = 0;
    T node;
    MDataBlock data;
    const MPlug output(node.thisMObject(), T::outDC);
    for (int index = 0; index < expectedCalls; ++index) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            resetRun(index);
            injectedStatus = code;
            controlTrace = "unchanged";
            assert(node.readControls(output, data) == code);
            assert(callCount == index + 1 && controlTrace == "unchanged");
            assert(normalControls<T>(0) == trace);
            ++cases;
        }
        resetRun();
        throwAt = index;
        controlTrace = "unchanged";
        assert(node.readControls(output, data) == MS::kFailure);
        assert(callCount == index + 1 && controlTrace == "unchanged");
        ++cases;
    }
    resetRun();
    assert(node.readControls(MPlug(node.thisMObject(), MObject{"unknown"}), data) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
    return cases;
}
std::string normalTraces() {
    std::string result;
    for (int scenario : {0, 1}) {
        result +=
            "curve " + std::to_string(scenario) + "\n" + normalControls<DCurveFromCurve>(scenario);
        result += "surface " + std::to_string(scenario) + "\n" +
                  normalControls<DCurveFromSurface>(scenario);
        result += "cloth " + std::to_string(scenario) + "\n" +
                  normalControls<DCurveClothFromSurface>(scenario);
    }
    resetRun();
    MMatrix m;
    assert(transformSample(m) == MS::kSuccess);
    std::ostringstream s;
    s << std::setprecision(17);
    for (const auto& row : m.values)
        for (double v : row)
            s << v << ' ';
    return result + "transform " + s.str() + "\n";
}
std::string readFile(const char* path) {
    std::ifstream f(path);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace
int main(int argc, char** argv) {
    const auto schema = schemaTrace("DCurveFromCurve", DCurveFromCurve::initialize) +
                        schemaTrace("DCurveFromSurface", DCurveFromSurface::initialize) +
                        schemaTrace("DCurveClothFromSurface", DCurveClothFromSurface::initialize);
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--trace") {
        std::cout << normalTraces();
        return 0;
    }
    assert(argc == 3 && schema == readFile(argv[1]));
    assert(normalTraces() == readFile(argv[2]));
    const int initializers =
        testFailures("DCurveFromCurve", DCurveFromCurve::initialize, 109) +
        testFailures("DCurveFromSurface", DCurveFromSurface::initialize, 76) +
        testFailures("DCurveClothFromSurface", DCurveClothFromSurface::initialize, 48);
    const int controls = failControls<DCurveFromCurve>(14) + failControls<DCurveFromSurface>(11) +
                         failControls<DCurveClothFromSurface>(7);
    for (int index = 0; index < 3; ++index)
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            resetRun(index);
            injectedStatus = code;
            MMatrix matrix;
            matrix[3][0] = 99;
            assert(transformSample(matrix) == code);
            assert(callCount == index + 1 && matrix[3][0] == 99);
        }
    std::cout << "Curve controls: " << initializers << " initializer, " << controls
              << " input and 6 transform failures; three schemas and seven normal traces passed.\n";
}
