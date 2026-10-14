#include "mesh_emitter_doubles.h"

// Analytic rotation and parameter-trace surface are host boundaries, not a Maya
// geometry emulator. Production sampling expressions and random engines run.
struct EmitterVector : MVector {
    using MVector::MVector;
    enum { kYaxis };
    EmitterVector rotateBy(int, double angle) const {
        assert(std::isfinite(angle));
        return {x * std::cos(angle) + z * std::sin(angle), y,
                z * std::cos(angle) - x * std::sin(angle)};
    }
};
namespace {
MPointArray surfaceCVs;
int declaredU = 2, declaredV = 2;
double uStart = 2, uEnd = 4, vStart = -3, vEnd = -1;
double surfaceHeight = 5;
} // namespace
struct EmitterSurface {
    bool valid = false;
    MStatus setObject(MObject object) {
        const auto s = api("surface.bind");
        valid = bool(s) && !object.isNull();
        return s ? (valid ? MS::kSuccess : MS::kInvalidParameter) : s;
    }
    int numCVsInU(MStatus* status) const {
        assert(valid);
        *status = api("surface.countU");
        return declaredU;
    }
    int numCVsInV(MStatus* status) const {
        assert(valid);
        *status = api("surface.countV");
        return declaredV;
    }
    MStatus getKnotDomain(double& u0, double& u1, double& v0, double& v1) const {
        assert(valid);
        const auto s = api("surface.domain");
        if (s) {
            u0 = uStart;
            u1 = uEnd;
            v0 = vStart;
            v1 = vEnd;
        }
        return s;
    }
    MStatus getPointAtParam(double u, double v, MPoint& point, MSpace::Space space) const {
        assert(valid && space == MSpace::kWorld && std::isfinite(u) && std::isfinite(v));
        const auto s = api("surface.point");
        if (s)
            point = {u, v, surfaceHeight};
        return s;
    }
    MStatus getCVs(MPointArray& points, MSpace::Space space) const {
        assert(valid && space == MSpace::kWorld);
        const int ordinal = callCount;
        const auto s = api("surface.cvs");
        if (s) {
            points = surfaceCVs;
            if (ordinal == shortAt && points.size()) {
                points.pop_back();
                shortApplied = true;
            }
        }
        return s;
    }
};
#include "circle_surface_emitter.inc"

namespace {
struct Scenario {
    bool circle;
    int mode;
    int count;
};
std::vector<Scenario> scenarios() {
    return {{true, 0, 0},  {true, 0, 1},  {true, 0, 4},  {true, 1, 0},  {true, 1, 9},
            {false, 0, 0}, {false, 0, 2}, {false, 1, 0}, {false, 1, 2}, {false, 2, 0}};
}
void setup(const Scenario& s) {
    resetAttributes();
    inputs.clear();
    pointOutput = pointStorage = DPData();
    shortPointStorage = false;
    declaredU = declaredV = 2;
    uStart = 2;
    uEnd = 4;
    vStart = -3;
    vEnd = -1;
    surfaceHeight = 5;
    surfaceCVs = {{0, 0, 0}, {1, 0, 1}, {0, 1, 2}, {1, 1, 3}};
    inputs["emitType"].number = inputs["samplingType"].number = s.mode;
    inputs["sampling"].number = inputs["samplingU"].number = s.count;
    inputs["samplingV"].number = s.count;
    inputs["radius"].number = 2;
    inputs["startAngle"].number = 10;
    inputs["endAngle"].number = 190;
    inputs["offsetAngle"].number = 5;
    inputs["aSeed"].number = 42;
    inputs["uLength"].number = 2;
    inputs["vLength"].number = -1;
    inputs["uOffset"].number = 3;
    inputs["vOffset"].number = 4;
    inputs["inSurfaceObject"].type = MFnData::kNurbsSurface;
    reset();
}
template <class Node> MStatus compute() {
    Node node;
    EmitterBlock block;
    return node.compute(MPlug({}, Node::outDP), block);
}
MStatus compute(const Scenario& s) {
    return s.circle ? compute<DPointCircleEmit>() : compute<DPointSurfaceEmit>();
}
bool called(const char* name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
std::string schema() {
    return schemaTrace("DPointCircleEmit", &DPointCircleEmit::initialize) +
           schemaTrace("DPointSurfaceEmit", &DPointSurfaceEmit::initialize);
}
std::string outputContract() {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    for (const auto& s : scenarios()) {
        setup(s);
        assert(compute(s) == MS::kSuccess && published == 1);
        if (s.circle && s.mode == D_CIRCLE_FILL) {
            // std::uniform_real_distribution is not bitwise portable.
            const auto points = pointOutput.points;
            setup(s);
            assert(compute(s) == MS::kSuccess && pointOutput.points == points);
            assert(points.size() == unsigned(s.count));
            for (const auto& p : points)
                assert(p[1] == 0 && p[0] * p[0] + p[2] * p[2] <= 4.000001);
        } else {
            out << s.circle << ' ' << s.mode << ' ' << s.count << ' ' << pointOutput.points.size()
                << '\n';
            for (const auto& p : pointOutput.points)
                out << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
        }
    }
    return out.str();
}
int failures() {
    int count = 0;
    for (const auto& s : scenarios()) {
        setup(s);
        assert(compute(s) == MS::kSuccess);
        const auto trace = operations;
        for (unsigned i = 0; i < trace.size(); ++i) {
            for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                setup(s);
                failAt = i;
                injectedStatus = status;
                const std::vector<std::array<double, 3>> previous{{{88, 99, 77}}};
                pointOutput.points = previous;
                const auto result = compute(s);
                assert(result != MS::kSuccess && callCount == int(i) + 1);
                if (trace[i] != "point.set" && trace[i] != "point.bounds")
                    assert(result == status);
                assert(published == (trace[i] == "clean" ? 1 : 0));
                if (!published)
                    assert(pointOutput.points == previous);
                ++count;
            }
            setup(s);
            throwAt = i;
            assert(compute(s) != MS::kSuccess && published == (trace[i] == "clean" ? 1 : 0));
            ++count;
            if (trace[i] == "surface.cvs") {
                setup(s);
                shortAt = i;
                assert(compute(s) != MS::kSuccess && !published && shortApplied);
                ++count;
            }
        }
        setup(s);
        assert(compute(s) == MS::kSuccess && published == 1);
    }
    return count;
}
void boundaries() {
    const Scenario cvs{false, 2, 0}, all{false, 0, 1}, range{false, 1, 1}, circle{true, 0, 4},
        fill{true, 1, 4};
    for (const auto& dimensions : {std::make_pair(-1, 2), std::make_pair(2, -1),
                                   std::make_pair(10000001, 1), std::make_pair(10000, 10000)}) {
        setup(cvs);
        declaredU = dimensions.first;
        declaredV = dimensions.second;
        assert(compute(cvs) != MS::kSuccess && !published && !called("surface.cvs"));
    }
    setup(cvs);
    surfaceCVs.pop_back();
    assert(compute(cvs) != MS::kSuccess && !published);
    setup(cvs);
    surfaceCVs.push_back({1, 2, 3});
    assert(compute(cvs) != MS::kSuccess && !published);
    setup(cvs);
    declaredU = declaredV = 0;
    surfaceCVs.clear();
    assert(compute(cvs) == MS::kSuccess && pointOutput.points.empty());
    for (const auto& s : {all, range, cvs}) {
        for (double value :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::max()}) {
            setup(s);
            surfaceHeight = value;
            surfaceCVs[2].z = value;
            assert(compute(s) != MS::kSuccess && !published);
        }
        setup(s);
        inputs["inSurfaceObject"].type = MFnData::kMesh;
        assert(compute(s) != MS::kSuccess && !published);
        setup(s);
        inputs["inSurfaceObject"].object = MObject::kNullObj;
        assert(compute(s) != MS::kSuccess && !published);
        setup(s);
        shortPointStorage = true;
        assert(compute(s) != MS::kSuccess && !published);
    }
    setup(all);
    uStart = -std::numeric_limits<double>::max();
    uEnd = std::numeric_limits<double>::max();
    assert(compute(all) != MS::kSuccess && !published && !called("surface.point"));
    setup(all);
    inputs["samplingU"].number = 10000000;
    assert(compute(all) != MS::kSuccess && !published && !called("surface.point"));
    setup(circle);
    inputs["sampling"].number = -1;
    assert(compute(circle) != MS::kSuccess && !published);
    setup(circle);
    inputs["radius"].number = -1;
    assert(compute(circle) != MS::kSuccess && !published);
    setup(circle);
    inputs["startAngle"].number = inputs["endAngle"].number;
    assert(compute(circle) != MS::kSuccess && !published);
    setup(fill);
    inputs["aSeed"].number = -1;
    assert(compute(fill) != MS::kSuccess && !published);
    setup(circle);
    shortPointStorage = true;
    assert(compute(circle) != MS::kSuccess && !published);
    setup(circle);
    inputs["startAngle"].number = std::numeric_limits<float>::max() / 2;
    inputs["endAngle"].number = inputs["offsetAngle"].number = std::numeric_limits<float>::max();
    assert(compute(circle) != MS::kSuccess && !published);
    setup(circle);
    DPointCircleEmit circleNode;
    DPointSurfaceEmit surfaceNode;
    EmitterBlock block;
    assert(circleNode.compute(MPlug({}, {"unknown"}), block) == MS::kUnknownParameter);
    assert(surfaceNode.compute(MPlug({}, {"unknown"}), block) == MS::kUnknownParameter &&
           callCount == 0);
}
std::string read(const char* path) {
    std::ifstream stream(path);
    assert(stream);
    return std::string(std::istreambuf_iterator<char>(stream), {});
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--dump-schema") {
        std::cout << schema();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--dump-output") {
        std::cout << outputContract();
        return 0;
    }
    assert(argc == 3);
    assert(schema() == read(argv[1]));
    assert(outputContract() == read(argv[2]));
    const int count = failures();
    boundaries();
    const int init = testFailures("DPointCircleEmit", &DPointCircleEmit::initialize, 33) +
                     testFailures("DPointSurfaceEmit", &DPointSurfaceEmit::initialize, 38);
    std::cout << "Circle/surface emitters: " << count << " compute failures, " << init
              << " initializer failures, 10 output scenarios OK\n";
}
