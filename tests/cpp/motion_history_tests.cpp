#include "motion_history_doubles.h"
#include <fstream>
#include <functional>

namespace {
std::vector<MPointArray> pointResult;
std::vector<std::vector<HistoryPoints>> meshResult;

void fixture() {
    reset(); inputs.clear(); emitterMeshes.clear(); topologies.clear();
    meshSnapshots.clear(); snapshots.clear(); queriedTimes.clear(); meshSpaces.clear();
    invalidPointCount = false;
    activeTime = MTime(2, MTime::kNTSCFrame);
    selectedGeometry = 0;
    for (int time = 0; time < 3; ++time) {
        const MObject object{"sample" + std::to_string(time)};
        auto& mesh = emitterMeshes[object.name];
        mesh.points = {{double(time),0,0}, {double(time),1,0}, {double(time),0,1}};
        topologies[object.name] = HistoryTopology();
        auto item = std::make_shared<HistoryMesh>();
        item->points.assign(mesh.points.begin(), mesh.points.end());
        meshSnapshots[time].items.push_back(item);
        snapshots[time].object = object;
        snapshots[time].type = MFnData::kMesh;
        snapshots[time].plugin = &meshSnapshots[time];
    }
    inputs["Sampling"].number = 3;
    inputs["TimeRange"].number = 2;
    inputs["Amplitude"].number = 1;
    inputs["Weight"].number = 1;
    inputs["TimeSample"].number = 3;
    inputs["StartFrame"].number = 0;
    inputs["EndFrame"].number = 2;
    pointResult = {{{99,99,99}}};
    meshResult.clear();
    meshResult.emplace_back();
    meshResult.back().emplace_back();
    meshResult.back().back().push_back({99,99,99});
}

MStatus guarded(const std::function<MStatus()>& call) {
    try { return call(); }
    catch (...) { return MS::kFailure; }
}

MStatus sample(int source, long count = -1, double range = 2,
               MTime time = MTime(2, MTime::kNTSCFrame), unsigned points = 3) {
    return guarded([&]() -> MStatus {
        if (source == 0) {
            DMotionDeformer node;
            return node.getPlugTimeValue(pointResult, count < 0 ? 2 : count, range, time, 7, points);
        }
        if (source == 1) {
            DMMotionDeform node;
            return node.getDMeshFromTime(meshResult, {points}, points, count < 0 ? 3 : count, range, time);
        }
        return sampleMeshPoints(pointResult, HistoryPlug({}, DMTimeShapeGen::inMesh),
            count < 0 ? 3 : count, MTime(time.value()-range, time.unit()),
            count > 1 ? range / (count-1) : range/2, points);
    });
}

void unchanged(int source) {
    if (source == 1)
        assert(meshResult.size() == 1 && meshResult[0].size() == 1 &&
               meshResult[0][0].size() == 1 && meshResult[0][0][0].x == 99);
    else
        assert(pointResult.size() == 1 && pointResult[0].size() == 1 && pointResult[0][0].x == 99);
    assert(activeTime.value() == 2 && activeTime.unit() == MTime::kNTSCFrame);
}

std::size_t calls(const std::string& operation) {
    return std::count(operations.begin(), operations.end(), operation);
}

void normal() {
    for (int source = 0; source < 3; ++source) {
        fixture(); assert(sample(source) == MS::kSuccess);
        const std::vector<double> times = source == 0 ? std::vector<double>{1,0} :
            source == 1 ? std::vector<double>{2,1,0} : std::vector<double>{0,1,2};
        assert(queriedTimes.size() == times.size());
        for (unsigned i = 0; i < times.size(); ++i) {
            assert(queriedTimes[i].value() == times[i] && queriedTimes[i].unit() == MTime::kNTSCFrame);
            if (source == 1) assert(meshResult[i][0][0].x == times[i]);
            else assert(pointResult[i][0].x == times[i]);
        }
        if (source == 0) assert(selectedGeometry == 7);
        for (const auto& query : meshSpaces)
            assert(query.second == (source == 0 ? MSpace::kObject : MSpace::kWorld));
        assert(activeTime.value() == 2 && activeTime.unit() == MTime::kNTSCFrame);
        for (long count : {2L,3L}) {
            fixture(); assert(sample(source, count, 0) == MS::kSuccess);
            assert(queriedTimes.size() == std::size_t(count));
            for (const auto& query : queriedTimes) assert(query.value() == 2);
        }
    }
    fixture(); assert(sample(0, 1, 0) == MS::kSuccess && queriedTimes.size() == 1);
}

int failures() {
    int tested = 0;
    for (int source = 0; source < 3; ++source) {
        fixture(); assert(sample(source) == MS::kSuccess); const auto trace = operations;
        for (unsigned i = 0; i < trace.size(); ++i) {
            for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
                fixture(); failAt = i; injectedStatus = status;
                const auto result = sample(source);
                assert(result != MS::kSuccess && callCount == int(i)+1);
                if (source == 1 && trace[i] == "meshData.read") assert(result == status);
                unchanged(source); ++tested;
            }
            fixture(); throwAt = i;
            assert(sample(source) != MS::kSuccess); unchanged(source); ++tested;
            if (trace[i] == "mesh.points" || trace[i] == "array.copy") {
                fixture(); shortAt = i;
                assert(sample(source) != MS::kSuccess && shortApplied); unchanged(source); ++tested;
            }
        }
    }
    for (const auto reader : controlReaders) {
        fixture(); HistoryBlock block;
        assert(reader(block) == MS::kSuccess && callCount == 1);
        for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
            fixture(); failAt = 0; injectedStatus = status;
            assert(reader(block) == status && callCount == 1); ++tested;
        }
        fixture(); throwAt = 0;
        assert(guarded([&] { return reader(block); }) != MS::kSuccess); ++tested;
    }
    return tested;
}

void boundaries() {
    using dommy::sampling::validTimeHistoryStorage;
    assert(validTimeHistoryStorage(3, 1, 3));
    assert(!validTimeHistoryStorage(3, 1, 0));
    assert(!validTimeHistoryStorage(0, 1, 1000001));
    assert(!validTimeHistoryStorage(0, 1000000, 3));
    assert(!validTimeHistoryStorage(800000, 1, 3));
    assert(!validTimeHistoryStorage(std::numeric_limits<std::size_t>::max(), 1, 2));
    assert(validTimeHistoryStorage(2097150, 1, 1));
    assert(!validTimeHistoryStorage(2097151, 1, 1));
    for (int source = 0; source < 3; ++source) {
        for (const long count : {0L, source == 2 ? 1000001L : 4097L}) {
            fixture(); assert(sample(source, count) != MS::kSuccess);
            unchanged(source); assert(queriedTimes.empty());
        }
        fixture(); assert(sample(source, 3, 2, activeTime, 800000) != MS::kSuccess);
        unchanged(source); assert(queriedTimes.empty());
        for (const double time : {NAN, INFINITY, -INFINITY}) {
            fixture(); assert(sample(source, 3, 2, MTime(time, MTime::kNTSCFrame)) != MS::kSuccess);
            unchanged(source); assert(queriedTimes.empty());
        }
        // Changed historical layout must fail before copying that snapshot.
        fixture();
        emitterMeshes["sample1"].pointCount = 100000000;
        meshSnapshots[1].items[0]->points.reported = 100000000;
        assert(sample(source) != MS::kSuccess); unchanged(source);
        assert(calls("positions.copy") == (source == 1 ? 1U : 0U));
        assert(calls("mesh.points") == (source == 2 ? 1U : 0U));
        // Empty snapshots remain a valid fixed layout.
        fixture();
        for (int t = 0; t < 3; ++t) {
            emitterMeshes["sample"+std::to_string(t)].points.clear();
            meshSnapshots[t].items[0]->points.clear();
        }
        assert(sample(source, -1, 2, activeTime, 0) == MS::kSuccess);
    }
    for (int source = 0; source < 2; ++source) {
        fixture();
        const double maximum = std::numeric_limits<double>::max();
        assert(sample(source, source == 0 ? 3 : 4, maximum,
                      MTime(-maximum/2, MTime::kNTSCFrame)) != MS::kSuccess);
        unchanged(source); assert(queriedTimes.empty());
    }
    fixture(); meshSnapshots[1].reported = 100000000;
    assert(sample(1) != MS::kSuccess && calls("positions.copy") == 1); unchanged(1);
    fixture(); meshSnapshots[1].items[0]->valid = false;
    assert(sample(1) != MS::kSuccess); unchanged(1);
    for (int source : {0,2}) {
        fixture(); emitterMeshes["sample1"].points[0].x = NAN;
        assert(sample(source) != MS::kSuccess); unchanged(source);
        fixture(); snapshots[1].type = MFnData::kNurbsCurve;
        assert(sample(source) != MS::kSuccess); unchanged(source);
    }
    fixture(); emitterMeshes["sample1"].points[0].x = std::numeric_limits<double>::max();
    assert(sample(2) != MS::kSuccess); unchanged(2);
    fixture(); DMMotionDeform node;
    assert(node.getDMeshFromTime(meshResult, {2}, 3, 3, 2, activeTime) != MS::kSuccess);
    unchanged(1); assert(queriedTimes.empty());
    fixture();
    const double maximum = std::numeric_limits<double>::max();
    assert(sampleMeshPoints(pointResult, HistoryPlug({}, DMTimeShapeGen::inMesh), 3,
        MTime(maximum, MTime::kNTSCFrame), maximum, 3) != MS::kSuccess);
    unchanged(2); assert(queriedTimes.empty());
    // Same total vertex count is insufficient: each ordered group must match.
    fixture();
    for (int t = 0; t < 3; ++t) {
        meshSnapshots[t].items[0]->points.resize(1);
        auto second = std::make_shared<HistoryMesh>(); second->points.resize(2);
        meshSnapshots[t].items.push_back(second);
    }
    assert(node.getDMeshFromTime(meshResult, {1,2}, 3, 3, 2, activeTime) == MS::kSuccess);
    assert(meshResult.size() == 3 && meshResult[1].size() == 2);
    meshSnapshots[1].items[0]->points.resize(2);
    meshSnapshots[1].items[1]->points.resize(1);
    reset(); meshResult.clear(); meshResult.emplace_back(); meshResult.back().emplace_back();
    meshResult.back().back().push_back({99,99,99});
    assert(node.getDMeshFromTime(meshResult, {1,2}, 3, 3, 2, activeTime) != MS::kSuccess);
    unchanged(1); assert(calls("positions.copy") == 2);
}

MStatus shapePreflight(std::size_t count = 3) {
    HistoryMeshFn mesh; mesh.object = {"sample2"}; mesh.valid = true;
    return guarded([&] { return preflightShape(mesh, count); });
}

int preflights() {
    fixture(); assert(shapePreflight() == MS::kSuccess); const auto trace = operations;
    int tested = 0;
    for (unsigned i = 0; i < trace.size(); ++i) {
        for (const auto status : {MS::kFailure,MS::kInvalidParameter}) {
            fixture(); failAt = i; injectedStatus = status;
            assert(shapePreflight() == status && callCount == int(i)+1); ++tested;
        }
        fixture(); throwAt = i; assert(shapePreflight() != MS::kSuccess); ++tested;
        if (trace[i] == "mesh.vertices" || trace[i] == "mesh.uvs" || trace[i] == "mesh.uvAssignments") {
            fixture(); shortAt = i; assert(shapePreflight() != MS::kSuccess && shortApplied); ++tested;
        }
    }
    for (int field = 0; field < 4; ++field) {
        fixture();
        if (field == 0) emitterMeshes["sample2"].pointCount = 800000;
        if (field == 1) emitterMeshes["sample2"].faceCount = 10000000;
        if (field == 2) topologies["sample2"].connectionCount = 100000000;
        if (field == 3) topologies["sample2"].uvCount = 100000000;
        assert(shapePreflight() != MS::kSuccess && calls("mesh.vertices") == 0);
    }
    fixture(); topologies["sample2"].u.clear(); topologies["sample2"].v.clear();
    assert(shapePreflight() == MS::kSuccess && calls("mesh.uvs") == 0 && calls("mesh.uvAssignments") == 0);
    fixture(); assert(shapePreflight(1000001) != MS::kSuccess && calls("mesh.vertices") == 0);
    fixture(); assert(preflightMotion(&meshSnapshots[2], 3, 2, 1, 1) == MS::kSuccess);
    for (int count : {0,1})
        assert(preflightMotion(&meshSnapshots[2], count, NAN, NAN, NAN) == MS::kSuccess);
    for (const double value : {double(NAN),double(INFINITY),std::numeric_limits<double>::max()}) {
        fixture(); assert(preflightMotion(&meshSnapshots[2], 3, 2, 1, value) != MS::kSuccess);
    }
    fixture(); assert(preflightMotion(&meshSnapshots[2], 3, 2, NAN, 1) != MS::kSuccess);
    fixture(); meshSnapshots[2].reported = 100000000;
    assert(preflightMotion(&meshSnapshots[2], 3, 2, 1, 1) != MS::kSuccess);
    fixture(); meshSnapshots[2].items[0]->points.reported = 800000;
    assert(preflightMotion(&meshSnapshots[2], 3, 2, 1, 1) != MS::kSuccess);
    return tested;
}
} // namespace

int main(int argc, char** argv) {
    const auto first = schemaTrace("DMotionDeformer", DMotionDeformer::initialize); const int calls1 = callCount;
    const auto second = schemaTrace("DMMotionDeform", DMMotionDeform::initialize); const int calls2 = callCount;
    const auto third = schemaTrace("DMTimeShapeGen", DMTimeShapeGen::initialize); const int calls3 = callCount;
    const auto schema = first + second + third;
    if (argc == 2 && std::string(argv[1]) == "--print-schema") { std::cout << schema; return 0; }
    assert(argc == 2); std::ifstream file(argv[1]); assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)),{}); assert(schema == expected);
    int tested = testFailures("DMotionDeformer", DMotionDeformer::initialize, calls1);
    tested += testFailures("DMMotionDeform", DMMotionDeform::initialize, calls2);
    tested += testFailures("DMTimeShapeGen", DMTimeShapeGen::initialize, calls3);
    normal(); boundaries(); tested += failures(); tested += preflights();
    std::cout << "Motion history contracts, preflight budgets, layout/time bounds and "
              << tested << " injected failures passed\n";
}
