#include "mesh_curve_copy_doubles.h"
#include "mesh_curve_copy.inc"

namespace {
void setupMesh(bool triangle = false, bool uv = false) {
    reset();
    assert(DCMeshCopy::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    meshes.clear();
    geometries["source"] = Geometry();
    geometries["source"].points = {{0, 0, 0}, {1, 1, 0}, {1, 2, 1}, {0, 3, 1}};
    meshes["target"] = MeshGeometry();
    if (triangle) {
        auto& face = meshes["target"].faces[0];
        face.ids = {0, 1, 2};
        face.u = {0, 1, 1};
        face.v = {-200, -150, -175};
    }
    for (auto a : {DCMeshCopy::aCount, DCMeshCopy::aHeightScale, DCMeshCopy::aSeed})
        inputs[a.name].number = 1;
    inputs[DCMeshCopy::aDirectionMode.name].number = uv;
    auto& copy = inputs[DCMeshCopy::inCopyObject.name];
    copy.type = MFnData::kNurbsCurve;
    copy.object = {"source"};
    copy.connected = true;
    auto& mesh = inputs[DCMeshCopy::inMesh.name];
    mesh.type = MFnData::kMesh;
    mesh.object = {"target"};
    mesh.connected = true;
    reset();
}
MStatus computeMesh() {
    DCMeshCopy node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), DCMeshCopy::outDC), data);
}
void pointEqual(const MPoint& a, const MPoint& b) {
    for (unsigned i = 0; i < 3; ++i)
        assert(std::abs(a[i] - b[i]) < 1e-6);
}
void curvesEqual(const DCurveData& a, const DCurveData& b) {
    assert(a.curves.size() == b.curves.size());
    for (unsigned i = 0; i < a.curves.size(); ++i) {
        const auto& x = *a.curves[i];
        const auto& y = *b.curves[i];
        assert(x.degree == y.degree && x.knots == y.knots && x.open == y.open);
        assert(x.points.size() == y.points.size());
        for (unsigned j = 0; j < x.points.size(); ++j)
            pointEqual(x.points[j], y.points[j]);
    }
}
bool called(const std::string& name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
void testUv() {
    unsigned top = 99;
    assert(dommy::curve::checkedCopyUvTopIndex(MFloatArray{0, 1, 1, 0}, MFloatArray{0, 0, 1, 1},
                                               top) &&
           top == 2);
    for (float shift : {-500.f, 0.f, 700.f}) {
        assert(
            dommy::curve::checkedCopyUvTopIndex(
                MFloatArray{0, 1, 1}, MFloatArray{-200 + shift, -150 + shift, -175 + shift}, top) &&
            top == 1);
        assert(dommy::curve::checkedCopyUvTopIndex(
                   MFloatArray{0, 1, 1, 0}, MFloatArray{shift, shift, 1 + shift, 1 + shift}, top) &&
               top == 2);
    }
    assert(dommy::curve::checkedCopyUvTopIndex(MFloatArray{0, 0, 0}, MFloatArray{1, 1, 1}, top) &&
           top == 0);
    const float largest = std::numeric_limits<float>::max();
    assert(dommy::curve::checkedCopyUvTopIndex(
               MFloatArray{0, 0, 0, 0}, MFloatArray{-largest, -largest, largest, largest}, top) &&
           top == 2);
    for (const auto& v :
         {MFloatArray{}, MFloatArray{0, 1}, MFloatArray{0, NAN, 1}, MFloatArray{0, INFINITY, 1}}) {
        top = 99;
        assert(!dommy::curve::checkedCopyUvTopIndex(MFloatArray{0, 1, 1}, v, top) && top == 99);
    }
    top = 99;
    assert(
        !dommy::curve::checkedCopyUvTopIndex(MFloatArray{NAN, 1, 1}, MFloatArray{0, 1, 1}, top) &&
        top == 99);
}
void testMeshGeometry() {
    const MPointArray expected[] = {
        {{0, 0, 0}, {1, 1, 0}, {1, 2, 1}, {0, 3, 1}}, // simple quad
        {{1, 0, 0}, {1, 1, 1}, {0, 2, 1}, {0, 3, 0}}, // highest-V quad edge
        {{1, 0, 1}, {1, 1, 1}, {1, 2, 0}, {0, 3, 0}}, // simple triangle top 2
        {{1, 0, 0}, {1, 1, 0}, {0, 2, 0}, {1, 3, 1}}  // negative-V triangle top 1
    };
    for (bool triangle : {false, true})
        for (bool uv : {false, true}) {
            setupMesh(triangle, uv);
            assert(computeMesh() == MS::kSuccess && published == 1 && cleaned == 2);
            assert(outputData.curves.size() == 1);
            for (unsigned i = 0; i < 4; ++i)
                pointEqual(outputData.curves[0]->points[i], expected[triangle * 2 + uv][i]);
            const auto baseline = outputData;
            reset();
            inputs[DCMeshCopy::aCount.name].number = 8;
            assert(computeMesh() == MS::kSuccess);
            curvesEqual(outputData, baseline);
            reset();
            inputs[DCMeshCopy::aCount.name].number = 0;
            assert(computeMesh() == MS::kSuccess && outputData.curves.empty() &&
                   !called("mesh.bind"));
        }
    setupMesh();
    inputs[DCMeshCopy::inMesh.name].matrix[3][0] = 10;
    inputs[DCMeshCopy::inMesh.name].matrix[3][1] = 20;
    inputs[DCMeshCopy::inCopyObject.name].matrix[3][1] = 2;
    assert(computeMesh() == MS::kSuccess);
    for (unsigned i = 0; i < 4; ++i)
        pointEqual(outputData.curves[0]->points[i], expected[0][i] + MVector(10, 22, 0));
    // A skipped face does not consume a random sample; Count remains an enable switch.
    setupMesh();
    inputs[DCMeshCopy::aRandomPadding.name].vec[0] = .3f;
    inputs[DCMeshCopy::aRandomPadding.name].vec[1] = .2f;
    inputs[DCMeshCopy::aRandomHeight.name].number = .5f;
    meshes["target"].faces.resize(2);
    assert(computeMesh() == MS::kSuccess);
    const auto randomBaseline = outputData;
    std::vector<float> u, v, h;
    dommy::random::generateValues(1U, -.3f, .3f, 2U, u, true);
    dommy::random::generateValues(2U, -.2f, .2f, 2U, v, true);
    dommy::random::generateValues(3U, -.5f, .5f, 2U, h, true);
    for (unsigned face = 0; face < 2; ++face)
        for (unsigned i = 0; i < 4; ++i) {
            const auto p = expected[0][i];
            pointEqual(outputData.curves[face]->points[i],
                       {u[face] * .5 + (1. - u[face]) * p.x, p.y * (1. + h[face]),
                        v[face] * .5 + (1. - v[face]) * p.z});
        }
    MeshFace skipped;
    skipped.hasUvs = false;
    meshes["target"].faces.insert(meshes["target"].faces.begin(), skipped);
    skipped.ids = {0, 1, 2, 3, 0};
    meshes["target"].faces.push_back(skipped);
    reset();
    assert(computeMesh() == MS::kSuccess);
    curvesEqual(outputData, randomBaseline);
    setupMesh();
    meshes["target"].faces[0].hasUvs = false;
    assert(computeMesh() == MS::kSuccess && outputData.curves.empty() && !called("mesh.points"));
    setupMesh();
    inputs[DCMeshCopy::inMesh.name].connected = false;
    assert(computeMesh() == MS::kSuccess && outputData.curves.empty());
}
template <class Compute> int injectFailures(Compute compute) {
    assert(compute() == MS::kSuccess);
    const auto trace = operations;
    const auto expected = outputData;
    int cases = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            const auto result = compute();
            assert(result != MS::kSuccess && callCount == i + 1);
            if (trace[i] != "batch.insert")
                assert(result == status);
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
        if (trace[i] == "array.copy" || trace[i] == "array.resize" || trace[i] == "mesh.vertices" ||
            trace[i] == "mesh.points" || trace[i] == "mesh.uvs" || trace[i] == "curve.cvs" ||
            trace[i] == "curve.knots") {
            reset();
            shortAt = i;
            assert(compute() != MS::kSuccess && published == 0 && cleaned == 0);
            ++cases;
        }
    }
    reset();
    assert(compute() == MS::kSuccess);
    curvesEqual(outputData, expected);
    return cases;
}
void testMeshInvalid() {
    for (int variant = 0; variant < 19; ++variant) {
        setupMesh(false, true);
        auto& mesh = meshes["target"];
        auto& face = mesh.faces[0];
        switch (variant) {
        case 0:
            face.ids[0] = -1;
            break;
        case 1:
            face.ids[0] = 4;
            break;
        case 2:
            face.cornerCount = 3;
            break;
        case 3:
            face.v.pop_back();
            break;
        case 4:
            face.v[0] = NAN;
            break;
        case 5:
            face.u[0] = INFINITY;
            break;
        case 6:
            face.normal.x = NAN;
            break;
        case 7:
            mesh.points[0].x = INFINITY;
            break;
        case 8:
            mesh.reportedPoints = 5;
            break;
        case 9:
            mesh.reportedFaces = 2;
            break;
        case 10:
            mesh.reportedFaces = 0;
            break;
        case 11:
            mesh.stuck = true;
            break;
        case 12:
            inputs[DCMeshCopy::aCount.name].number = -1;
            break;
        case 13:
            inputs[DCMeshCopy::aDirectionMode.name].number = 2;
            break;
        case 14:
            inputs[DCMeshCopy::aHeightScale.name].number = NAN;
            break;
        case 15:
            inputs[DCMeshCopy::aRandomPadding.name].vec[1] = NAN;
            break;
        case 16:
            inputs[DCMeshCopy::inMesh.name].type = MFnData::kNurbsCurve;
            break;
        case 17:
            inputs[DCMeshCopy::inMesh.name].matrix[1][0] = NAN;
            break;
        case 18:
            mesh.points[0].x = 1e100;
            break;
        }
        assert(computeMesh() != MS::kSuccess && published == 0 && cleaned == 0);
    }
    setupMesh();
    geometries["source"].points.resize(1001, MPoint(0, 0, 0));
    meshes["target"].faces.resize(10000);
    assert(computeMesh() == MS::kInvalidParameter && published == 0);
    assert(!called("mesh.points") && !called("mesh.vertices") && !called("plugin.create") &&
           !called("array.resize"));
    setupMesh();
    inputs[DCMeshCopy::inCopyObject.name].connected = false;
    assert(computeMesh() == MS::kFailure &&
           operations == std::vector<std::string>{"plug.connected inCopyObject"});
    setupMesh();
    DCMeshCopy node;
    MDataBlock block;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"other"}), block) ==
               MS::kUnknownParameter &&
           operations.empty());
}
DCurveData replicationInput;
void setupReplication(int mode = 0, int count = 2, float radius = .5f) {
    reset();
    assert(DCReplicator::initialize() == MS::kSuccess);
    inputs.clear();
    replicationInput.curves.clear();
    replicationInput.curves.push_back(
        DCItem(new DCurveInternalData(MPointArray{{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}},
                                      MDoubleArray{0, 0, 0, 1, 1, 1}, 3, false)));
    inputs[DCReplicator::inDC.name].plugin = &replicationInput;
    inputs[DCReplicator::aCount.name].number = count;
    inputs[DCReplicator::aRadius.name].number = radius;
    inputs[DCReplicator::aGenerateType.name].number = mode;
    reset();
}
MStatus computeReplication() {
    DCReplicator node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), DCReplicator::outDC), data);
}
void testReplication() {
    for (int mode : {0, 1})
        for (float radius : {0.f, .5f}) {
            setupReplication(mode, 2, radius);
            const auto source = replicationInput.curves[0]->points;
            assert(computeReplication() == MS::kSuccess && outputData.curves.size() == 2);
            std::vector<float> x, y, z;
            const std::size_t count = mode ? 8 : 2;
            dommy::random::generateValues(1U, -radius, radius, count, x, true);
            dommy::random::generateValues(2U, -radius, radius, count, y, true);
            dommy::random::generateValues(3U, -radius, radius, count, z, true);
            for (unsigned c = 0; c < 2; ++c) {
                assert(!outputData.curves[c]->open && outputData.curves[c]->degree == 3 &&
                       outputData.curves[c]->knots == replicationInput.curves[0]->knots);
                for (unsigned p = 0; p < 4; ++p) {
                    const auto index = mode ? c * 4 + p : c;
                    pointEqual(outputData.curves[c]->points[p],
                               source[p] + MVector(x[index], y[index], z[index]));
                    pointEqual(replicationInput.curves[0]->points[p], source[p]);
                }
            }
        }
    setupReplication(0, 0);
    assert(computeReplication() == MS::kSuccess && outputData.curves.empty());
    setupReplication();
    replicationInput.curves.clear();
    assert(computeReplication() == MS::kSuccess && outputData.curves.empty());
    for (int variant = 0; variant < 9; ++variant) {
        setupReplication();
        switch (variant) {
        case 0:
            inputs[DCReplicator::aCount.name].number = -1;
            break;
        case 1:
            inputs[DCReplicator::aRadius.name].number = NAN;
            break;
        case 2:
            inputs[DCReplicator::aRadius.name].number = -1;
            break;
        case 3:
            inputs[DCReplicator::aGenerateType.name].number = 2;
            break;
        case 4:
            inputs[DCReplicator::inDC.name].plugin = nullptr;
            break;
        case 5:
            replicationInput.curves[0].reset();
            break;
        case 6:
            replicationInput.curves[0]->points[0].x = NAN;
            break;
        case 7:
            inputs[DCReplicator::aCount.name].number = 1000001;
            break;
        case 8:
            inputs[DCReplicator::aRadius.name].number = std::numeric_limits<float>::max();
            break;
        }
        assert(computeReplication() != MS::kSuccess && published == 0 && cleaned == 0);
    }
    setupReplication(0, 1000000);
    replicationInput.curves.push_back(replicationInput.curves[0]);
    assert(computeReplication() == MS::kInvalidParameter && !called("plugin.create"));
    setupReplication(0, 1000000);
    replicationInput.curves[0]->points.resize(11, MPoint(0, 0, 0));
    replicationInput.curves[0]->knots.resize(13, 1);
    assert(computeReplication() == MS::kInvalidParameter && !called("plugin.create"));
    setupReplication();
    DCReplicator node;
    MDataBlock data;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"other"}), data) ==
               MS::kUnknownParameter &&
           operations.empty());
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string schema = schemaTrace("DCMeshCopy", DCMeshCopy::initialize) +
                               schemaTrace("DCReplicator", DCReplicator::initialize) +
                               schemaTrace("DCNormal", DCNormal::initialize);
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(schema ==
           std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
    int initializationFailures = testFailures("DCMeshCopy", DCMeshCopy::initialize, 70) +
                                 testFailures("DCReplicator", DCReplicator::initialize, 32) +
                                 testFailures("DCNormal", DCNormal::initialize, 53);
    testUv();
    testMeshGeometry();
    testMeshInvalid();
    testReplication();
    int failures = 0;
    for (bool triangle : {false, true})
        for (bool uv : {false, true}) {
            setupMesh(triangle, uv);
            failures += injectFailures(computeMesh);
        }
    for (int mode : {0, 1}) {
        setupReplication(mode);
        failures += injectFailures(computeReplication);
    }
    std::cout << "Mesh/curve copy checks passed: " << initializationFailures
              << " initializer failures, " << failures
              << " compute failure/exception/short-array cases.\n";
}
