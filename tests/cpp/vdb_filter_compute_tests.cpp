#include "vdb_filter_doubles.h"
#include "vdb_filter_compute.inc"

using namespace dommy::vdb;
const double inf = std::numeric_limits<double>::infinity();
const double nanValue = std::numeric_limits<double>::quiet_NaN();

template <typename T = openvdb::FloatGrid> std::shared_ptr<T> grid(const std::string& name) {
    auto result = std::make_shared<T>();
    result->name = name;
    return result;
}
void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    inputData = DOpenVDBData();
    maskData = DOpenVDBData();
    stagedData = DOpenVDBData();
    outputData = DOpenVDBData();
    inputData.grids = {grid("source"), grid<Grid>("keep")};
    maskData.grids = {grid("mask")};
    outputData.grids = {grid<Grid>("previous")};
    publications = gridCopies = invalidReads = 0;
    nullCopy = invalidNativeOutput = false;
    filterCalls.clear();
    lastMask = nullptr;
}
MDataBlock block(bool level, bool masked = false) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "*";
    if (!level) {
        data.inputs["aFilter"].integer = 0;
        data.inputs["aRadius"].integer = 1;
        data.inputs["aIterations"].integer = 1;
        data.inputs["aOffset"].number = 0;
    } else {
        data.inputs["iterations"].integer = 1;
        data.inputs["halfWidth"].integer = 3;
        data.inputs["halfWidthWorld"].number = 0.3f;
        data.inputs["stencilWidth"].integer = 0;
        data.inputs["stencilWidthWorld"].number = 0.1f;
        data.inputs["voxelOffset"].number = 0;
        data.inputs["minMask"].number = 0;
        data.inputs["maxMask"].number = 1;
        data.inputs["isInvert"].integer = 0;
        data.inputs["isWorldSpaceUnits"].integer = 0;
        data.inputs["filterType"].integer = 1;
        data.inputs["accuracyType"].integer = 0;
        data.inputs["isMask"].integer = masked;
        data.inputs["maskVdbInput"].payload = masked ? &maskData : nullptr;
        data.inputs["maskName"].text = "*";
    }
    return data;
}
MStatus compute(MDataBlock& data, bool level) {
    return level ? DOpenVDBFilterLevelSet().compute(MPlug(DOpenVDBFilterLevelSet::aVdbOutput), data)
                 : DOpenVDBFilter().compute(MPlug(DOpenVDBFilter::aVdbOutput), data);
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> source() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(inputData.grids[0]));
}
void reject(MDataBlock& data, bool level, bool beforeCopy = true) {
    assert(!compute(data, level) && publications == 0 && invalidReads == 0);
    assert(outputData.grids[0]->name == "previous");
    if (beforeCopy)
        assert(gridCopies == 0);
}
void testPolicy() {
    FilterPlan p;
    p.radius = 9;
    int width = 7;
    assert(filterWidth(.3, .1, width) && width == 2); // truncation, not rounding
    assert(filterWidth(.01, 1, width) && width == 1);
    assert(filterWidth(64, 1, width) && width == 64);
    for (double bad : {inf, nanValue, 0.0, -1.0, std::ldexp(1.0, -31)}) {
        width = 7;
        assert(!filterWidth(1, bad, width) && width == 7);
    }
    assert(!filterWidth(65, 1, width));
    FilterSettings g;
    for (int op = 0; op <= 3; ++op) {
        g.operation = op;
        assert(makeFilterPlan(g, p));
    }
    g.operation = 4;
    assert(!makeFilterPlan(g, p));
    g = FilterSettings();
    g.radius = 65;
    assert(!makeFilterPlan(g, p));
    g = FilterSettings();
    g.iterations = 0;
    assert(!makeFilterPlan(g, p));
    g.iterations = 1025;
    assert(!makeFilterPlan(g, p));
    g = FilterSettings();
    g.offset = float(inf);
    assert(!makeFilterPlan(g, p));
    LevelSetFilterSettings s;
    for (int op = -1; op <= 11; ++op) {
        s.operation = op;
        assert(makeLevelSetFilterPlan(s, 1, p));
    }
    s = LevelSetFilterSettings();
    s.iterations = 0;
    assert(makeLevelSetFilterPlan(s, 1, p));
    s.operation = 5;
    s.offset = 2048;
    assert(makeLevelSetFilterPlan(s, 1, p));
    s.offset = 2049;
    assert(!makeLevelSetFilterPlan(s, 1, p));
    s.offset = std::numeric_limits<float>::max();
    assert(!makeLevelSetFilterPlan(s, 2, p));
    s = LevelSetFilterSettings();
    s.worldUnits = true;
    for (int op : {1, 2, 10, 11}) {
        s.operation = op;
        assert(!makeLevelSetFilterPlan(s, std::ldexp(1.0, -31), p));
    }
    s = LevelSetFilterSettings();
    s.maskMin = s.maskMax = 1;
    assert(!validLevelSetFilterSettings(s, true) && validLevelSetFilterSettings(s, false));
    s.maskMin = float(nanValue);
    assert(!validLevelSetFilterSettings(s, true));
    s = LevelSetFilterSettings();
    s.maskMax = std::numeric_limits<float>::denorm_min();
    assert(!validLevelSetFilterSettings(s, true));
    s = LevelSetFilterSettings();
    s.accuracy = 5;
    assert(!validLevelSetFilterSettings(s, false));
    std::size_t work = 0;
    p.workPerVoxel = 10;
    assert(addFilterWork(10000000, p, work) && work == 100000000);
    assert(!addFilterWork(1, p, work) && work == 100000000);
    work = 1;
    assert(!addFilterWork(std::numeric_limits<std::uint64_t>::max(), p, work) && work == 1);
    const int lo = std::numeric_limits<int>::min(), hi = std::numeric_limits<int>::max();
    assert(filterCoordinate(double(lo) + 64, 64) && filterCoordinate(double(hi) - 64, 64));
    assert(!filterCoordinate(double(lo) + 63, 64) && !filterCoordinate(double(hi) - 63, 64));
}
void testCompute() {
    setup();
    Grid wrongType;
    FilterSettings generic;
    LevelSetFilterSettings levelSettings;
    FilterPlan plan;
    assert(!preflightFilter<openvdb::FloatGrid>(wrongType, plan));
    assert(!applyFilter<openvdb::FloatGrid>(wrongType, generic));
    assert(!preflightLevelSetFilter<openvdb::FloatGrid>(wrongType, levelSettings, nullptr, plan));
    assert(!applyLevelSetFilter<openvdb::FloatGrid>(wrongType, levelSettings, plan, nullptr));
    assert(filterCalls.empty());
    for (bool level : {false, true}) {
        setup();
        auto data = block(level);
        assert(compute(data, level) && publications == 1 && gridCopies == 1);
        assert(outputData.grids.size() == 2 && outputData.grids[1] == inputData.grids[1]);
        assert(outputData.grids[0] != inputData.grids[0] && source()->values[0] == 1);
        setup();
        data = block(level);
        inputData.grids[0] = grid<openvdb::DoubleGrid>("double");
        assert(compute(data, level));
        setup();
        data = block(level);
        data.inputs["aVdbSelectedGridNames"].text = "missing";
        assert(compute(data, level) && gridCopies == 0 && outputData.grids.size() == 2);
        setup();
        data = block(level);
        data.inputs["aVdbSelectedGridNames"].text = "source";
        assert(compute(data, level) && outputData.grids[0] == inputData.grids[1]);
        for (double bad : {inf, nanValue}) {
            setup();
            data = block(level);
            source()->values[0] = float(bad);
            reject(data, level);
            setup();
            data = block(level);
            source()->bg = float(bad);
            reject(data, level);
        }
        setup();
        data = block(level);
        source()->bounds.high = openvdb::Coord(std::numeric_limits<int>::max(), 0, 0);
        reject(data, level);
        setup();
        data = block(level);
        inputData.valid = false;
        reject(data, level);
        setup();
        data = block(level);
        inputData.grids[0] = nullptr;
        reject(data, level);
        setup();
        data = block(level);
        data.inputs["aVdbInput"].payload = nullptr;
        reject(data, level);
        setup();
        data = block(level);
        data.inputs["aVdbInput"].type = 999;
        reject(data, level);
        setup();
        data = block(level);
        MPxData wrongPayload;
        data.inputs["aVdbInput"].payload = &wrongPayload;
        reject(data, level);
        setup();
        data = block(level);
        stagedData.valid = false;
        reject(data, level, false);
        setup();
        data = block(level);
        nullCopy = true;
        reject(data, level, false);
        setup();
        data = block(level);
        invalidNativeOutput = true;
        reject(data, level, false);
        setup();
        data = block(level);
        inputData.grids.push_back(grid("large"));
        source()->activeCount = level ? 400000 : 6000000;
        std::const_pointer_cast<Grid>(inputData.grids.back())->activeCount = source()->activeCount;
        reject(data, level);
    }
    for (int op = 0; op <= 3; ++op) {
        setup();
        auto data = block(false);
        data.inputs["aFilter"].integer = op;
        data.inputs["aRadius"].integer = 2;
        data.inputs["aIterations"].integer = 3;
        data.inputs["aOffset"].number = -2;
        assert(compute(data, false));
        const char* expected[] = {"mean:2:3", "gaussian:2:3", "median:2:3", "offset:-2"};
        assert(filterCalls == std::vector<std::string>{expected[op]});
    }
    for (int op = -1; op <= 11; ++op) {
        setup();
        auto data = block(true);
        data.inputs["filterType"].integer = op;
        data.inputs["voxelOffset"].number = 2;
        data.inputs["iterations"].integer = 2;
        assert(compute(data, true));
        std::vector<std::string> expected{"temporal:0", "spatial:2"};
        const std::vector<std::vector<std::string>> endings = {
            {},
            {"norm:2", "temporal:1", "normalize", "prune"},
            {"mean:1", "mean:1"},
            {"median:1", "median:1"},
            {"curvature", "curvature"},
            {"laplacian", "laplacian"},
            {"offset:-2"},
            {"offset:2"},
            {"offset:2", "offset:-2"},
            {"offset:-2", "offset:2"},
            {"track", "track"},
            {"gaussian:1", "gaussian:1"},
            {"norm:1", "resize:3"}};
        expected.insert(expected.end(), endings[op + 1].begin(), endings[op + 1].end());
        assert(filterCalls == expected);
    }
    setup();
    auto data = block(true);
    source()->kind = openvdb::GRID_UNKNOWN;
    assert(compute(data, true) && gridCopies == 0 && outputData.grids[0] == inputData.grids[0]);
    for (double dx : {0.0, -1.0, inf, nanValue, std::numeric_limits<double>::denorm_min()}) {
        setup();
        data = block(true);
        source()->dx = {dx, dx, dx};
        reject(data, true);
    }
    setup();
    data = block(true);
    source()->uniform = false;
    reject(data, true);
    setup();
    data = block(true);
    source()->bg = 65;
    reject(data, true);
    for (int op : {1, 2, 10, 11}) {
        setup();
        data = block(true);
        data.inputs["filterType"].integer = op;
        data.inputs["isWorldSpaceUnits"].integer = 1;
        source()->dx = {std::ldexp(1.0, -31), std::ldexp(1.0, -31), std::ldexp(1.0, -31)};
        source()->bg = float(3 * source()->dx[0]);
        reject(data, true);
    }
    setup();
    data = block(true);
    data.inputs["isWorldSpaceUnits"].integer = 1;
    data.inputs["stencilWidthWorld"].number = 2.9f;
    assert(compute(data, true));
    assert(filterCalls.back() == "mean:2");
    setup();
    data = block(true);
    data.inputs["iterations"].integer = 0;
    assert(compute(data, true) && filterCalls.size() == 2);
}
void testMasks() {
    for (bool useDouble : {false, true}) {
        setup();
        auto data = block(true, true);
        if (useDouble)
            inputData.grids[0] = grid<openvdb::DoubleGrid>("source");
        maskData.grids.push_back(grid("last"));
        maskData.grids.push_back(grid<Grid>("unsupported"));
        assert(compute(data, true) && lastMask == maskData.grids[1].get() && gridCopies == 1);
    }
    setup();
    auto data = block(true, true);
    data.inputs["maskName"].text = "ma*";
    reject(data, true);
    setup();
    data = block(true, true);
    data.inputs["maskName"].text = "mask";
    assert(compute(data, true));
    setup();
    data = block(true, true);
    data.inputs["maskVdbInput"].payload = nullptr;
    reject(data, true);
    setup();
    data = block(true, true);
    maskData.grids = {grid<openvdb::DoubleGrid>("mask")};
    reject(data, true);
    setup();
    data = block(true, true);
    maskData.valid = false;
    reject(data, true);
    setup();
    data = block(true, true);
    data.inputs["minMask"].number = 1;
    reject(data, true);
    setup();
    data = block(true, true);
    data.inputs["maxMask"].number = float(inf);
    reject(data, true);
    setup();
    data = block(true, true);
    std::dynamic_pointer_cast<openvdb::FloatGrid>(std::const_pointer_cast<Grid>(maskData.grids[0]))
        ->bg = float(nanValue);
    reject(data, true);
    setup();
    data = block(true, true);
    std::const_pointer_cast<Grid>(maskData.grids[0])->xform.inverseShift[0] = inf;
    reject(data, true);
    setup();
    data = block(true, true);
    std::const_pointer_cast<Grid>(maskData.grids[0])->xform.linear = false;
    reject(data, true);
    setup();
    data = block(true, false);
    data.inputs.erase("maskVdbInput");
    data.inputs.erase("maskName");
    assert(compute(data, true)); // disabled optional input is not evaluated
}
void testFailures() {
    for (bool level : {false, true}) {
        setup();
        auto data = block(level, level);
        assert(compute(data, level));
        const int count = callCount;
        const auto baseline = operations;
        for (int i = 0; i < count; ++i) {
            for (const auto failure : {MS::kFailure, MS::kInvalidParameter}) {
                setup();
                data = block(level, level);
                resetCalls(i);
                injectedStatus = failure;
                const MStatus result = compute(data, level);
                const bool hostStatus = baseline[i].compare(0, 6, "input ") == 0 ||
                                        baseline[i].compare(0, 7, "output.") == 0;
                assert(result == (hostStatus ? failure : MS::kFailure));
                assert(invalidReads == 0 && callCount == i + 1);
                assert(publications == (baseline[i] == "output.clean" ? 1 : 0));
            }
            setup();
            data = block(level, level);
            attributeThrowAt = i;
            assert(!compute(data, level) && invalidReads == 0 && callCount == i + 1);
        }
        setup();
        data = block(level);
        const MStatus status = level
                                   ? DOpenVDBFilterLevelSet().compute(MPlug(MObject{"other"}), data)
                                   : DOpenVDBFilter().compute(MPlug(MObject{"other"}), data);
        assert(status == MS::kUnknownParameter && callCount == 0);
    }
}
int main() {
    testPolicy();
    testCompute();
    testMasks();
    testFailures();
    std::cout << "VDB filters: numeric policy, all dispatch modes, masks, pass-through, batch and "
                 "failures passed\n";
}
