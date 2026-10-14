#include "curve_emitter_doubles.h"
#include "curve_emitter.inc"

namespace {
DCurveData inputData;
void setup(int mode = D_CURVE_SAMPLING_ALL, bool native = false) {
    reset();
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    pointOutput = DPData();
    badRadiusRamp = false;
    noiseValue = .5;
    for (int i = 0; i < 2; ++i) {
        auto curve = DCItem(new DCurveInternalData(MPointArray{{0, 0, 0}, {1, 0, 0}}, 1, true));
        inputData.curves.push_back(curve);
    }
    geometries["source"] = Geometry();
    inputs["sampling"].number = 2;
    inputs["samplingType"].number = mode;
    inputs["curveLength"].number = 1;
    inputs["stepSize"].number = .2;
    inputs["aFillLength"].number = .5;
    inputs["aFillSampling"].number = 6;
    inputs["aRadius"].number = 1;
    inputs["aOuterCircleSampling"].number = 4;
    inputs["aOuterRandomSampling"].number = 4;
    inputs["aSeed"].number = 1;
    inputs["aAmplitube"].number = 1;
    inputs["aOctave"].number = 2;
    inputs["aFrequency"].number = 1;
    inputs["aLacunarity"].number = 2;
    inputs["aPersistence"].number = .5;
    inputs["aNoiseScale"].number = 1;
    inputs["aTangentVector"].dvec[1] = 1;
    inputs["aInTangentCurve"].connected = true;
    inputs["inCurve"].connected = native;
    inputs["inDC"].connected = !native;
    inputs["inDC"].plugin = &inputData;
    reset();
}
MStatus compute() {
    DPointCurveEmit node;
    EmitterBlock data;
    return node.compute(MPlug({}, DPointCurveEmit::outDP), data);
}
bool called(const char* s) {
    return std::find(operations.begin(), operations.end(), s) != operations.end();
}
void pyroPlacementContract() {
    struct Field {
        double result = 2.0, coordinates[3] = {};
        int calls = 0;
        double GetValue(double x, double y, double z) {
            ++calls; coordinates[0] = x; coordinates[1] = y; coordinates[2] = z;
            return result;
        }
    } field;
    DPointList points;
    const MPoint center(1, 2, 3);
    const MVector horizontal(1, 0, 0), vertical(0, 1, 0), offset(4, 5, 6);
    const auto sample = [&](float x, float y, float radius, float ramp, float displacement,
                            float density) {
        appendPyroDiskCandidate(points, field, center, horizontal, vertical, 8,
                                x, y, radius, ramp, 2, offset, displacement, density);
    };
    sample(.3f, .4f, 2, .5f, 3, 1);
    assert(points.size() == 1 && std::fabs(points[0][0] - 3.1) < 1e-6 &&
           std::fabs(points[0][1] - 4.8) < 1e-6 && points[0][2] == 3);
    assert(field.calls == 1 && field.coordinates[0] == double(.3f) / 2 + 4 &&
           field.coordinates[1] == double(.4f) / 2 + 5 && field.coordinates[2] == 10);
    sample(1, 1, 2, .5f, 3, 1);
    assert(points.size() == 1 && field.calls == 1); // Rejection precedes noise lookup.
    sample(0, 0, 2, .5f, 3, 1);
    assert(points.back()[0] == center[0] && points.back()[1] == center[1] &&
           points.back()[2] == center[2]); // Zero radial direction is not divided.
    sample(.5f, 0, 0, 1, 3, 1);
    assert(points.back()[0] == 7); // Radius does not multiply displacement.
    sample(.5f, 0, 2, -1, 3, 1);
    assert(points.back()[0] == -6); // Negative finite ramps retain their sign.
    const auto saved = points;
    const auto unchangedPoints = [&] {
        assert(points.size() == saved.size());
        for (size_t row = 0; row != points.size(); ++row)
            for (int axis = 0; axis != 3; ++axis) assert(points[row][axis] == saved[row][axis]);
    };
    sample(.3f, .4f, 2, 1, 3, 0);
    sample(.3f, .4f, 2, 1, 3, -1);
    field.result = -2;
    sample(.3f, .4f, 2, 1, 3, 1);
    unchangedPoints();
    field.result = NAN;
    bool rejected = false;
    try { sample(.3f, .4f, 2, 1, 3, 1); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    unchangedPoints();
}
void regression() {
    setup();
    assert(compute() == MS::kSuccess && published == 1);
    const auto trace = operations;
    for (unsigned i = 0; i < trace.size(); ++i) {
        if (trace[i] == "array.append" || trace[i] == "input inDC" || trace[i] == "plugin.read") {
            setup();
            failAt = i;
            assert(compute() != MS::kSuccess && published == 0);
        }
    }
}
struct Scenario {
    int mode;
    bool native;
    int sampling;
    int subtype;
};
std::vector<Scenario> scenarios() {
    std::vector<Scenario> result;
    for (int mode = 0; mode < D_CURVE_SAMPLING_TYPE_COUNT; ++mode)
        for (bool native : {false, true})
            result.push_back({mode, native, 2, 0});
    result.push_back({D_CURVE_SAMPLING_ALL, false, 1, 0});
    result.push_back({D_CURVE_SAMPLING_ALL, false, 4, 0});
    result.push_back({D_CURVE_SAMPLING_TUBE_OUTER, false, 3, 1});
    result.push_back({D_CURVE_SAMPLING_PYRO, false, 3, 1});
    result.push_back({D_CURVE_SAMPLING_PYRO, false, 3, 2});
    return result;
}
void setup(const Scenario& s) {
    setup(s.mode, s.native);
    inputs["sampling"].number = s.sampling;
    if (s.mode == D_CURVE_SAMPLING_TUBE_OUTER)
        inputs["aOuterType"].number = s.subtype;
    if (s.mode == D_CURVE_SAMPLING_PYRO)
        inputs["aTangentMode"].number = s.subtype;
}
void unchanged() {
    assert(inputData.numberOfCurves() == 2);
    for (const auto& c : inputData.curves) {
        assert(c->points.size() == 2 && c->points[0].x == 0 && c->points[1].x == 1);
        assert(c->attributes.empty());
    }
}
std::string normalContract() {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    for (const auto& s : scenarios()) {
        setup(s);
        assert(compute() == MS::kSuccess && published == 1);
        unchanged();
        // Standard-library random distributions are not bitwise portable.
        // Check repeatability within the same runtime instead of freezing its RNG.
        if (s.mode == D_CURVE_SAMPLING_TUBE || s.mode == D_CURVE_SAMPLING_PYRO ||
            (s.mode == D_CURVE_SAMPLING_TUBE_OUTER && s.subtype == 1)) {
            const auto points = pointOutput.points;
            setup(s);
            assert(compute() == MS::kSuccess && pointOutput.points == points);
            continue;
        }
        out << s.mode << ' ' << s.native << ' ' << s.sampling << ' ' << s.subtype << ' '
            << pointOutput.points.size() << '\n';
        for (const auto& p : pointOutput.points)
            out << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
    }
    return out.str();
}
int failures() {
    int count = 0;
    for (const auto& s : scenarios()) {
        setup(s);
        assert(compute() == MS::kSuccess);
        const auto trace = operations;
        for (unsigned i = 0; i < trace.size(); ++i) {
            for (auto failure : {MS::kFailure, MS::kInvalidParameter}) {
                setup(s);
                failAt = i;
                injectedStatus = failure;
                assert(compute() != MS::kSuccess);
                assert(published == (trace[i] == "clean" ? 1 : 0));
                unchanged();
                ++count;
            }
            setup(s);
            throwAt = i;
            assert(compute() != MS::kSuccess);
            assert(published == (trace[i] == "clean" ? 1 : 0));
            unchanged();
            ++count;
            if (trace[i] == "array.resize" || trace[i] == "array.copy" || trace[i] == "curve.cvs" ||
                trace[i] == "ramp.sample") {
                setup(s);
                shortAt = i;
                assert(compute() != MS::kSuccess && published == 0);
                assert(shortApplied);
                unchanged();
                ++count;
            }
        }
        setup(s);
        assert(compute() == MS::kSuccess && published == 1);
    }
    return count;
}
void boundaries() {
    setup();
    inputs["sampling"].number = 0;
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    assert(!called("plug.connected inCurve"));
    setup();
    inputs["inDC"].connected = false;
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup();
    inputData.curves.clear();
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute() == MS::kInvalidParameter && published == 0);
    setup(0, true);
    inputs["inCurve"].object = MObject::kNullObj;
    assert(compute() == MS::kInvalidParameter && published == 0);
    setup(0, true);
    inputs["inCurve"].type = MFnData::kAny;
    assert(compute() == MS::kInvalidParameter && published == 0);
    setup();
    inputs["sampling"].number = -1;
    assert(compute() != MS::kSuccess && published == 0);
    setup();
    inputs["sampling"].number = 10000001;
    assert(compute() != MS::kSuccess && published == 0);
    setup(D_CURVE_SAMPLING_TUBE);
    badRadiusRamp = true;
    assert(compute() != MS::kSuccess && published == 0);
    setup(D_CURVE_SAMPLING_PYRO);
    noiseValue = NAN;
    assert(compute() != MS::kSuccess && published == 0);
    setup();
    inputs["curveLength"].number = NAN;
    assert(compute() != MS::kSuccess && published == 0);
    setup(D_CURVE_SAMPLING_LENGTH_FILL);
    inputs["aFillLength"].number = 0;
    assert(compute() != MS::kSuccess && published == 0);
    setup();
    DPointCurveEmit node;
    EmitterBlock data;
    assert(node.compute(MPlug({}, MObject{"other"}), data) == MS::kUnknownParameter &&
           operations.empty());
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto contract = normalContract();
    if (std::string(argv[1]) == "--dump") {
        std::cout << contract;
        return 0;
    }
    std::ifstream fixture(argv[1]);
    assert(fixture.good());
    const std::string expected((std::istreambuf_iterator<char>(fixture)), {});
    assert(contract == expected);
    regression();
    pyroPlacementContract();
    const int count = failures();
    boundaries();
    std::cout << "Curve emitter checks: " << scenarios().size() << " normal paths, " << count
              << " compute failure cases passed\n";
}
