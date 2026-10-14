#include "mesh_emitter_doubles.h"
#include "mesh_emitter.inc"

namespace {
DMeshData inputData;
void setup(int mode = 0, bool native = true) {
    resetAttributes();
    inputs.clear();
    emitterMeshes.clear();
    meshSpaces.clear();
    pointOutput = DPData();
    pointStorage = DPData();
    inputData.items.clear();
    shortPointStorage = invalidPointCount = uvNoHit = badUVPoint = badConversion = false;
    emitterMeshes["source"] = EmitterMesh();
    for (int i = 0; i < 2; ++i) {
        auto mesh = std::make_shared<MeshItem>();
        mesh->object = i == 0 ? "first" : "second";
        emitterMeshes[mesh->object] = EmitterMesh();
        if (i)
            for (auto& p : emitterMeshes[mesh->object].points)
                p.z += 3;
        inputData.items.push_back(mesh);
    }
    inputs["inMesh"].connected = native;
    inputs["inMesh"].type = MFnData::kMesh;
    inputs["inDM"].connected = !native;
    inputs["inDM"].plugin = &inputData;
    inputs["isVertexEmit"].number = mode == 0 || mode == 7;
    inputs["isEdgeEmit"].number = mode == 1 || mode == 2 || mode == 7;
    inputs["edgeEmitType"].number = mode == 2;
    inputs["edgeSampling"].number = 3;
    inputs["stepWidth"].number = .75;
    inputs["isFinishEdgeLength"].number = true;
    inputs["isFaceEmit"].number = mode == 3 || mode == 4 || mode == 7;
    inputs["faceEmitType"].number = mode == 4;
    inputs["faceSampling"].number = 1;
    inputs["aIsFromUV"].number = mode == 5 || mode == 6 || mode == 7;
    inputs["aUVSamplingType"].number = mode == 6;
    inputs["aUVUniformCount"].number = 2;
    inputs["aUVRandomCount"].number = 3;
    inputs["aUVIsRelax"].number = true;
    inputs["aUVRelaxIteration"].number = 1;
    inputs["aUVTolerance"].number = .001;
    inputs["aURange"].vec[1] = inputs["aVRange"].vec[1] = 1;
    reset();
}
MStatus compute() {
    DPointMeshEmit node;
    EmitterBlock data;
    return node.compute(MPlug({}, DPointMeshEmit::outDP), data);
}
void unchanged() {
    assert(inputData.items.size() == 2);
    assert(emitterMeshes.at("first").points.front().z == 0);
    assert(emitterMeshes.at("second").points.front().z == 3);
}
std::string outputContract() {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    for (int mode = 0; mode <= 7; ++mode)
        for (bool native : {false, true}) {
            setup(mode, native);
            assert(compute() == MS::kSuccess && published == 1);
            unchanged();
            out << mode << ' ' << native << ' ' << pointOutput.points.size() << '\n';
            for (const auto& p : pointOutput.points)
                out << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
            for (const auto& query : meshSpaces)
                assert(query.second == (native ? MSpace::kWorld : MSpace::kObject));
        }
    return out.str();
}
void regression() {
    setup(3, false);
    assert(compute() == MS::kSuccess);
    const auto trace = operations;
    for (const char* target : {"input inDM", "face.center", "input isVertexEmit", "output"}) {
        const auto found = std::find(trace.begin(), trace.end(), target);
        assert(found != trace.end());
        setup(3, false);
        failAt = int(found - trace.begin());
        assert(compute() != MS::kSuccess && published == 0);
    }
}
int failures() {
    int count = 0;
    for (int mode = 0; mode <= 7; ++mode)
        for (bool native : {false, true}) {
            setup(mode, native);
            assert(compute() == MS::kSuccess);
            const auto trace = operations;
            for (unsigned i = 0; i < trace.size(); ++i) {
                for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                    setup(mode, native);
                    failAt = i;
                    injectedStatus = status;
                    const std::vector<std::array<double, 3>> previous{{{88, 99, 77}}};
                    pointOutput.points = previous;
                    const auto result = compute();
                    assert(result != MS::kSuccess);
                    assert(callCount == int(i) + 1);
                    if (trace[i].find("input ") == 0 && trace[i] != "input aURange" &&
                        trace[i] != "input aVRange" && trace[i].find("input aUV") != 0 &&
                        trace[i] != "input aSeed")
                        assert(result == status);
                    if (trace[i] == "output" || trace[i].find("face.") == 0)
                        assert(result == status);
                    assert(published == (trace[i] == "clean" ? 1 : 0));
                    if (!published)
                        assert(pointOutput.points == previous);
                    unchanged();
                    ++count;
                }
                setup(mode, native);
                throwAt = i;
                assert(compute() != MS::kSuccess && published == (trace[i] == "clean" ? 1 : 0));
                unchanged();
                ++count;
                if (trace[i] == "mesh.points" || trace[i] == "mesh.triangles" ||
                    trace[i] == "mesh.uv" || trace[i] == "uv.relax") {
                    setup(mode, native);
                    shortAt = i;
                    assert(compute() != MS::kSuccess && !published && shortApplied);
                    ++count;
                }
            }
            setup(mode, native);
            assert(compute() == MS::kSuccess && published == 1);
        }
    return count;
}
void boundaries() {
    setup();
    inputs["inMesh"].connected = false;
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup(0, false);
    inputData.items.clear();
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup();
    inputs["inDM"].connected = true;
    inputs["inDM"].plugin = nullptr;
    assert(compute() == MS::kSuccess && pointOutput.points.size() == 3);
    setup(0, false);
    inputs["inDM"].plugin = nullptr;
    assert(compute() == MS::kInvalidParameter && !published);
    setup(0, false);
    inputData.items[1].reset();
    assert(compute() == MS::kInvalidParameter && !published);
    setup();
    inputs["inMesh"].object = MObject::kNullObj;
    assert(compute() != MS::kSuccess && !published);
    setup();
    inputs["inMesh"].type = MFnData::kNurbsCurve;
    assert(compute() != MS::kSuccess && !published);
    setup();
    invalidPointCount = true;
    assert(compute() != MS::kSuccess && !published);
    setup();
    shortPointStorage = true;
    assert(compute() != MS::kSuccess && !published);
    setup(0, false);
    badConversion = true;
    assert(compute() != MS::kSuccess && !published);
    for (bool native : {false, true}) {
        const char* object = native ? "source" : "first";
        setup(3, native);
        emitterMeshes[object].faceCount = 2;
        assert(compute() != MS::kSuccess && !published);
        setup(3, native);
        emitterMeshes[object].centers.push_back({1, 2, 3});
        assert(compute() == MS::kSuccess);
        setup(3, native);
        emitterMeshes[object].stuck = true;
        assert(compute() != MS::kSuccess && !published && callCount < 100);
        setup(3, native);
        emitterMeshes[object].centers[0].x = NAN;
        assert(compute() != MS::kSuccess && !published);
        setup(3, native);
        emitterMeshes[object].centers[0].x = std::numeric_limits<double>::max();
        assert(compute() != MS::kSuccess && !published);
        setup(3, native);
        emitterMeshes[object].faceCount = 0;
        emitterMeshes[object].centers.clear();
        assert(compute() == MS::kSuccess);
        setup(3, native);
        emitterMeshes[object].faceCount = int(dommy::generation::kMaxGeneratedFaceCount) + 1;
        assert(compute() != MS::kSuccess && !published && callCount < 100);
        setup(3, native);
        emitterMeshes[object].faceCount = 1;
        emitterMeshes[object].centers.push_back({1, 2, 3});
        assert(compute() != MS::kSuccess && !published);
    }
    setup(1);
    emitterMeshes["source"].edges[0][1] = 999;
    assert(compute() != MS::kSuccess && !published);
    setup(2);
    inputs["stepWidth"].number = 0;
    assert(compute() != MS::kSuccess && !published);
    setup(4);
    inputs["faceSampling"].number = 100;
    assert(compute() != MS::kSuccess && !published && callCount < 100);
    setup(5);
    uvNoHit = true;
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup(5);
    badUVPoint = true;
    assert(compute() != MS::kSuccess && !published);
    setup(5);
    inputs["aURange"].vec[0] = 2;
    assert(compute() != MS::kSuccess && !published);
    setup(5, false);
    for (auto& item : emitterMeshes)
        item.second.hasUV = false;
    assert(compute() == MS::kSuccess && pointOutput.points.empty());
    setup();
    DPointMeshEmit node;
    EmitterBlock block;
    assert(node.compute(MPlug({}, {"unknown"}), block) == MS::kUnknownParameter && callCount == 0);
}
std::string schema() {
    return schemaTrace("DPointCurveEmit", &DPointCurveEmit::initialize) +
           schemaTrace("DPointMeshEmit", &DPointMeshEmit::initialize);
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
    regression();
    const int count = failures();
    boundaries();
    const int init = testFailures("DPointCurveEmit", &DPointCurveEmit::initialize, 137) +
                     testFailures("DPointMeshEmit", &DPointMeshEmit::initialize, 116);
    std::cout << "Mesh emitter: " << count << " compute failures, " << init
              << " initializer failures, 16 frozen output scenarios OK\n";
}
