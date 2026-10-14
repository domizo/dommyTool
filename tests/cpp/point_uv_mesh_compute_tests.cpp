#include "point_uv_mesh_compute_doubles.h"
#include "point_uv_mesh_compute.inc"

namespace {
void setup(std::size_t count = 3U) {
    failAt = throwAt = -1; calls = 0; trace.clear(); statusSites.clear();
    exports = reads = invalidReads = publications = cleans = 0;
    intArrayCount = exportDelta = 0; syntheticCount = 0U; generationType = 0;
    missingInput = invalidTopology = nonfiniteExport = false;
    nullOwner = nullGeometry = wrongInputType = false; dagCreates = 0;
    inputData.points.clear(); staged = MeshResult(); published = MeshResult();
    for (std::size_t i = 0U; i < count; ++i)
        inputData.points.push_back(MPoint(i + 0.25, -double(i), 2.0));
    published.points.push_back(MPoint(99, 99, 99));
}
MStatus compute() {
    DPToMesh node; MDataBlock block;
    return node.compute(MPlug(DPToMesh::outMesh), block);
}
void unchanged() {
    assert(publications == 0 && cleans == 0);
    assert(published.points.size() == 1U && published.points[0].x == 99.0);
}
void expected(std::size_t count) {
    const auto used = count - count % 3U;
    assert(published.points.size() == used);
    assert(published.faces == std::vector<int>(used / 3U, 3));
    assert(published.corners.size() == used);
    for (std::size_t i = 0U; i < used; ++i) {
        assert(published.corners[i] == static_cast<int>(i));
        assert(published.points[i].x == i + 0.25 && published.points[i].y == -double(i));
        assert(published.points[i].z == 2.0 && published.points[i].w == 1.0);
    }
    assert(inputData.points.size() == count && inputData.points.back().x == count - 0.75);
}
void ordinary() {
    setup();
    for (std::size_t count : {3U, 4U, 5U, 6U, 8U, 9U, 3U}) {
        inputData.points.clear();
        for (std::size_t i = 0U; i < count; ++i)
            inputData.points.push_back(MPoint(i + 0.25, -double(i), 2.0));
        publications = cleans = intArrayCount = 0;
        assert(compute() == MS::kSuccess && publications == 1 && cleans == 1);
        expected(count);
    }
}
void admission() {
    // Independent oracle for shared mesh staging widths, with the full input
    // charged for export even when one or two trailing points are discarded.
    const auto bytes = [](std::size_t count) {
        return 128U * count + 32U * (count / 3U) + 32U * (count - count % 3U) + 128U;
    };
    std::size_t low = 3U, high = 10000000U;
    while (low < high) {
        const auto mid = low + (high - low + 1U) / 2U;
        if (bytes(mid) <= 256U * 1024U * 1024U) low = mid; else high = mid - 1U;
    }
    setup(); syntheticCount = low;
    assert(!compute() && exports == 1); unchanged();
    // Accepted synthetic cardinality reaches export, whose tiny fixture then
    // fails the cardinality check. No giant allocation is performed here.
    setup(); syntheticCount = low + 1U;
    assert(!compute() && exports == 0); unchanged();
    for (std::size_t count : {10000001U, 6000003U, 2000001U}) {
        setup(); syntheticCount = count;
        assert(!compute() && exports == 0); unchanged();
    }
    for (std::size_t count : {0U, 1U, 2U}) {
        setup(count); assert(!compute() && exports == 0); unchanged();
    }
    for (short mode : {short(-1), short(1), short(2), short(32767)}) {
        setup(); generationType = mode;
        assert(!compute() && exports == 0); unchanged();
    }
    for (int delta : {-1, 1}) {
        setup(); exportDelta = delta;
        assert(!compute() && exports == 1); unchanged();
    }
    for (bool topology : {false, true}) {
        setup(); missingInput = !topology; invalidTopology = topology;
        assert(!compute() && reads == 0 && exports == 0); unchanged();
    }
    setup(4); nonfiniteExport = true; // The discarded tail is still validated.
    assert(!compute()); unchanged();
    setup(); DPToMesh node; MDataBlock block;
    assert(node.compute(MPlug(MObject{"other"}), block) == MS::kUnknownParameter && calls == 0);
    std::cout << "DPToMesh: exact synthetic admission edge " << low
              << ", tail, malformed input and retired-mode checks passed\n";
}
void failures(std::size_t count) {
    setup(count); assert(compute());
    const auto baseline = trace; const auto sites = statusSites;
    std::size_t tested = 0U;
    for (std::size_t at = 0U; at < baseline.size(); ++at) {
        for (int mode = 0; mode < 3; ++mode) {
            if (mode < 2 && !sites[at]) continue;
            setup(count); injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
            if (mode == 2) throwAt = static_cast<int>(at); else failAt = static_cast<int>(at);
            const auto result = compute();
            assert(!result && calls == static_cast<int>(at) + 1 && invalidReads == 0);
            assert(std::equal(trace.begin(), trace.end(), baseline.begin()));
            if (mode < 2) assert(result == injectedStatus);
            if (baseline[at] != "output.clean" && baseline[at] != "block.clean") unchanged();
            assert(inputData.points.size() == count && inputData.points[0].x == 0.25);
            failAt = throwAt = -1; calls = publications = cleans = intArrayCount = 0;
            assert(compute() && publications == 1 && cleans == 1); expected(count);
            ++tested;
        }
    }
    std::cout << "DPToMesh " << count << " points: " << tested
              << " status/exception cases with recovery passed\n";
}
void nullOutputs() {
    for (int kind = 0; kind < 3; ++kind) {
        setup(); nullOwner = kind == 0; nullGeometry = kind == 1; wrongInputType = kind == 2;
        assert(!compute()); unchanged(); assert(dagCreates == 0);
        if (nullOwner)
            assert(std::find(trace.begin(), trace.end(), "mesh.create") == trace.end());
        nullOwner = nullGeometry = wrongInputType = false;
        calls = publications = cleans = intArrayCount = 0;
        assert(compute()); expected(3U);
    }
    std::cout << "DPToMesh: null owner/geometry and wrong input reject without DAG creation, with retry\n";
}
void meshConverters() {
    DMeshInternalData mesh;
    mesh.points.values = {MPoint(0,0,0), MPoint(1,0,0), MPoint(0,1,0)};
    mesh.faces.values = {3}; mesh.corners.values = {0,1,2};
    for (bool uv : {false, true}) {
        mesh.uv = uv;
        for (bool transformed : {false, true}) {
            setup(); MObject output{"old"};
            const auto space = transformed ? DMeshPointSpace::Transformed : DMeshPointSpace::Local;
            assert(generateMMeshFromDmeshWithUV(mesh, output, space) && !output.isNull());
            const auto baseline = trace;
            for (std::size_t at = 0U; at < baseline.size(); ++at) {
                for (const auto code : {MS::kFailure, MS::kInvalidParameter}) {
                    setup(); failAt = static_cast<int>(at); injectedStatus = code; output = {"old"};
                    assert(generateMMeshFromDmeshWithUV(mesh, output, space) == code);
                    assert(output.isNull() && dagCreates == 0 && calls == static_cast<int>(at) + 1);
                }
            }
            for (bool owner : {false, true}) {
                setup(); nullOwner = owner; nullGeometry = !owner; output = {"old"};
                assert(!generateMMeshFromDmeshWithUV(mesh, output, space));
                assert(output.isNull() && dagCreates == 0);
                nullOwner = nullGeometry = false; calls = 0;
                assert(generateMMeshFromDmeshWithUV(mesh, output, space) && !output.isNull());
            }
        }
    }
    groupMeshes = {std::make_shared<DMeshInternalData>(mesh)};
    for (MStatus (*query)() : {groupFaceMesh, groupEdgeMesh}) {
        setup(); assert(query());
        const auto baseline = trace;
        for (std::size_t at = 0U; at < baseline.size(); ++at) {
            setup(); failAt = static_cast<int>(at); injectedStatus = MS::kInvalidParameter;
            assert(query() == injectedStatus && calls == static_cast<int>(at) + 1 && dagCreates == 0);
        }
        for (bool owner : {false, true}) {
            setup(); nullOwner = owner; nullGeometry = !owner;
            assert(!query() && dagCreates == 0);
            nullOwner = nullGeometry = false; calls = 0; assert(query());
        }
    }
    std::cout << "Mesh conversion/common and DMGroup face/edge owner blocks: status/null checks passed\n";
}
} // namespace

int main() {
    ordinary(); admission(); failures(3U); failures(5U);
    nullOutputs();
    meshConverters();
    std::cout << "DPToMesh: ordered triangles, early budgets and transactional staging passed\n";
}
