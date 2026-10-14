#include "vdb_value_doubles.h"
#include "vdb_value_compute.inc"

using namespace dommy::vdb;
const double infinity = std::numeric_limits<double>::infinity();
const double notNumber = std::numeric_limits<double>::quiet_NaN();
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> grid(const std::string& name) {
    auto result = std::make_shared<T>();
    result->name = name;
    return result;
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> source() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(inputData.grids[0]));
}
void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    inputData = DOpenVDBData();
    stagedData = DOpenVDBData();
    outputData = DOpenVDBData();
    inputData.grids = {grid("source"), grid<Grid>("keep")};
    outputData.grids = {grid<Grid>("previous")};
    publications = gridCopies = invalidReads = rampSamples = 0;
    nullCopy = invalidNativeOutput = wrongCopy = worker = false;
    rampMode = 0;
    rampFailure = -1;
    filterCalls.clear();
}
MDataBlock block() {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "*";
    for (const char* name : {"aMode", "aTolerance", "aInputRange", "aOutputRange", "aBelowMinimum",
                             "aAboveMaximum", "aDeactivateBackgroundVoxels"})
        data.inputs[name];
    return data;
}
MStatus compute(MDataBlock& data, bool remap) {
    return remap ? DOpenVDBRemap().compute(MPlug(DOpenVDBRemap::aVdbOutput), data)
                 : DOpenVDBPrune().compute(MPlug(DOpenVDBPrune::aVdbOutput), data);
}
void reject(MDataBlock& data, bool remap, bool beforeCopy = true) {
    assert(!compute(data, remap) && publications == 0 && invalidReads == 0);
    assert(outputData.grids[0]->name == "previous");
    if (beforeCopy)
        assert(gridCopies == 0);
}
template <typename Op> void throws(Op op) {
    bool caught = false;
    try {
        op();
    } catch (const std::exception&) {
        caught = true;
    }
    assert(caught);
}
void testMath() {
    setup();
    MStatus status;
    MRampAttribute attr(MPlug({"node"}), &status);
    rampMode = 1;
    RampLookup constant(attr);
    assert(remapValue(-1e-50, constant, 1, 1, 0, 1, 0, 1) == -1e-50);
    assert(remapValue(std::numeric_limits<double>::max(), constant, 2, 2, 0, 1, 0, 1) == .5);
    assert(remapValue(-1e-50, constant, 1, 1, 1, 0, 0, 1) == -1e-50);
    const auto maxInt = std::numeric_limits<std::int64_t>::max();
    const auto minInt = std::numeric_limits<std::int64_t>::min();
    assert(remapValue(maxInt, constant, 1, 1, std::ldexp(1.f, 63), std::ldexp(1.f, 64), 0, 1) ==
           maxInt);
    assert(remapValue(minInt, constant, 1, 1, 0, 1, 0, 1) == minInt);
    assert(remapValue(5., constant, 0, 0, 0, 1, 9, 9) == 9);
    rampMode = 2;
    RampLookup extreme(attr);
    const float maxFloat = std::numeric_limits<float>::max();
    assert(extreme.sample(.5L, false) == maxFloat);
    assert(extreme.sample(.5L + 1.L / 8192, false) == 0);
    assert(remapValue(.5, extreme, 0, 0, 0, 1, 0, 1) == maxFloat);
    throws([&] { extreme.sample(notNumber, false); });
    rampMode = 0;
    RampLookup linear(attr);
    for (int below = 0; below < 3; ++below)
        for (int above = 0; above < 3; ++above) {
            assert(remapValue(-1., linear, below, above, 0, 1, 2, 4) == (below == 0   ? 2
                                                                         : below == 1 ? -1
                                                                                      : 0));
            assert(remapValue(2., linear, below, above, 0, 1, 2, 4) == (above == 0   ? 4
                                                                        : above == 1 ? 2
                                                                                     : 6));
            assert(remapValue(2., linear, below, above, 1, 0, 2, 4) == (below == 0   ? 2
                                                                        : below == 1 ? 2
                                                                                     : 0));
        }
    assert(remapValue(std::numeric_limits<double>::max(), linear, 0, 0, 0,
                      std::numeric_limits<float>::denorm_min(), 0, 1) == 1);
    throws([&] { remapValue(2.f, linear, 2, 2, 0, 1, 0, maxFloat); });
    throws([&] { remapValue(notNumber, linear, 1, 1, 0, 1, 0, 1); });
    assert(ValueComponents<std::int64_t>::matchesBackground(maxInt, maxInt, 0));
    assert(!ValueComponents<std::int64_t>::matchesBackground(maxInt, minInt, 0));
    PruneRange<std::int64_t> range;
    assert(range.add(minInt) && range.add(-1));
    assert(!range.add(0));
    PruneRange<std::int64_t> positive;
    assert(positive.add(0) && positive.add(maxInt));
    assert(!positive.add(-1));
    for (double bad : {notNumber, infinity, -0.1, 1.1, 1e30}) {
        assert(!validPruneSettings(0, bad));
        PruneOp op(0, bad);
        auto g = grid<openvdb::Int32Grid>("int");
        throws([&] { op(*g); });
    }
    assert(!validPruneSettings(-1, 0) && !validPruneSettings(3, 0));
    for (double tol : {0., .5, 1.}) {
        auto i = grid<openvdb::Int32Grid>("int");
        PruneOp op(0, tol);
        op(*i);
        assert(pruneTolerance == (tol == 1 ? 1 : 0));
        auto b = grid<openvdb::BoolGrid>("bool");
        op(*b);
        assert(pruneTolerance == (tol != 0));
    }
    std::size_t work = dommy::generation::kMaxWorkItemCount - 26;
    ValuePreflight preflight(work, 2);
    preflight(*grid("a"));
    assert(preflight.valid && work == 100000000);
    preflight(*grid("b"));
    assert(!preflight.valid && work == 100000000);
    work = 100000000 - 77;
    ValuePreflight vectorFlight(work, 2);
    vectorFlight(*grid<openvdb::Vec3DGrid>("v"));
    assert(!vectorFlight.valid);
}
template <typename GridT> void typedCompute(bool remap) {
    setup();
    inputData.grids[0] = grid<GridT>("typed");
    auto data = block();
    assert(compute(data, remap) && publications == 1 && gridCopies == 1);
    assert(outputData.grids[0] != inputData.grids[0] && outputData.grids[1] == inputData.grids[1]);
    const auto out = std::dynamic_pointer_cast<const GridT>(outputData.grids[0]);
    assert(out && out->data.leaf.values == source<GridT>()->data.leaf.values);
    assert(out->data.internal.values == source<GridT>()->data.internal.values);
    assert(out->data.tiles.values == source<GridT>()->data.tiles.values);
    assert(out->kind == (remap ? openvdb::GRID_UNKNOWN : openvdb::GRID_LEVEL_SET));
}
void testCompute() {
    for (bool remap : {false, true}) {
        typedCompute<openvdb::FloatGrid>(remap);
        typedCompute<openvdb::DoubleGrid>(remap);
        typedCompute<openvdb::Int32Grid>(remap);
        typedCompute<openvdb::Int64Grid>(remap);
        typedCompute<openvdb::Vec3SGrid>(remap);
        typedCompute<openvdb::Vec3DGrid>(remap);
        typedCompute<openvdb::Vec3IGrid>(remap);
        setup();
        auto data = block();
        nullCopy = true;
        reject(data, remap, false);
        setup();
        data = block();
        wrongCopy = true;
        reject(data, remap, false);
        setup();
        data = block();
        inputData.grids[0] = nullptr;
        reject(data, remap);
        setup();
        data = block();
        inputData.valid = false;
        reject(data, remap);
        setup();
        data = block();
        data.inputs["aVdbInput"].payload = nullptr;
        reject(data, remap);
        setup();
        data = block();
        data.inputs["aVdbInput"].type = -1;
        reject(data, remap);
        setup();
        data = block();
        source()->data.leaf.values[2] = float(notNumber);
        reject(data, remap);
        setup();
        data = block();
        source()->data.internal.values[3] = float(infinity);
        reject(data, remap);
        setup();
        data = block();
        source()->data.bg = float(infinity);
        reject(data, remap);
        setup();
        data = block();
        inputData.grids.push_back(grid<openvdb::DoubleGrid>("late"));
        std::const_pointer_cast<openvdb::DoubleGrid>(
            std::dynamic_pointer_cast<const openvdb::DoubleGrid>(inputData.grids[2]))
            ->data.bg = notNumber;
        reject(data, remap);
        setup();
        data = block();
        data.inputs["aVdbSelectedGridNames"].text = "none";
        if (remap) {
            assert(compute(data, true) && publications == 1 && gridCopies == 0 &&
                   outputData.grids == inputData.grids);
        } else
            reject(data, false);
    }
    typedCompute<openvdb::BoolGrid>(false);
    setup();
    auto data = block();
    inputData.grids[0] = grid<openvdb::BoolGrid>("bool");
    assert(compute(data, true) && gridCopies == 0 && outputData.grids == inputData.grids);
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        data = block();
        data.inputs["aMode"].integer = mode;
        assert(compute(data, false));
        assert(filterCalls == std::vector<std::string>{mode == 0   ? "prune.value"
                                                       : mode == 1 ? "prune.inactive"
                                                                   : "prune.levelset"});
    }
    for (double bad : {notNumber, infinity, -1., 2., 1e30}) {
        setup();
        data = block();
        data.inputs["aTolerance"].number = bad;
        reject(data, false);
    }
    for (int bad : {-1, 3}) {
        setup();
        data = block();
        data.inputs["aMode"].integer = bad;
        reject(data, false);
    }
    setup();
    data = block();
    data.inputs["aMode"].integer = 2;
    source()->data.bg = -1;
    reject(data, false);
    setup();
    data = block();
    inputData.grids[0] = grid<openvdb::Int64Grid>("wide");
    source<openvdb::Int64Grid>()->data.leaf.values[0] = std::numeric_limits<std::int64_t>::min();
    reject(data, false);
    setup();
    data = block();
    data.inputs["aMode"].integer = 2;
    inputData.grids[0] = grid<openvdb::Vec3IGrid>("vector");
    source<openvdb::Vec3IGrid>()->data.bg = {1, std::numeric_limits<int>::min(), 1};
    reject(data, false);
    setup();
    data = block();
    invalidNativeOutput = true;
    reject(data, false, false);
    for (const char* attr : {"aBelowMinimum", "aAboveMaximum"})
        for (int bad : {-1, 3}) {
            setup();
            data = block();
            data.inputs[attr].integer = bad;
            reject(data, true);
        }
    for (const char* attr : {"aInputRange", "aOutputRange"})
        for (int i = 0; i < 2; ++i) {
            setup();
            data = block();
            data.inputs[attr].pair[i] = float(infinity);
            reject(data, true);
        }
    setup();
    data = block();
    data.inputs["aInputRange"].pair[1] = 0;
    reject(data, true);
    for (int failure : {0, 2048, 4096}) {
        setup();
        data = block();
        rampFailure = failure;
        reject(data, true);
    }
    setup();
    data = block();
    rampMode = 3;
    reject(data, true);
    setup();
    data = block();
    data.inputs["aOutputRange"].pair[0] = 10;
    data.inputs["aOutputRange"].pair[1] = 20;
    data.inputs["aDeactivateBackgroundVoxels"].integer = 1;
    assert(compute(data, true) && rampSamples == 4097);
    auto out = std::dynamic_pointer_cast<const openvdb::FloatGrid>(outputData.grids[0]);
    for (const auto* node :
         {&out->data.leaf, static_cast<const openvdb::Node<float>*>(&out->data.internal),
          &out->data.tiles}) {
        assert((node->values == std::array<float, 4>{{10, 20, 10, 20}}));
        assert((node->active == std::array<bool, 4>{{false, true, false, false}}));
    }
    assert(out->data.bg == 10 && source()->data.bg == 0 && source()->data.leaf.active[0]);
    setup();
    data = block();
    inputData.grids[0] = grid<openvdb::Vec3DGrid>("vector");
    source<openvdb::Vec3DGrid>()->data.leaf.values[0] = {-1e-50, .25, 2};
    data.inputs["aBelowMinimum"].integer = 1;
    data.inputs["aAboveMaximum"].integer = 2;
    data.inputs["aOutputRange"].pair[0] = 2;
    data.inputs["aOutputRange"].pair[1] = 4;
    assert(compute(data, true));
    const auto vectorOut = std::dynamic_pointer_cast<const openvdb::Vec3DGrid>(outputData.grids[0]);
    assert((vectorOut->data.leaf.values[0] == openvdb::math::Vec3<double>(-1e-50, 2.5, 6)));
    setup();
    data = block();
    inputData.grids[0] = grid<openvdb::Int64Grid>("extreme");
    source<openvdb::Int64Grid>()->data.bg = std::numeric_limits<std::int64_t>::min();
    source<openvdb::Int64Grid>()->data.leaf.values[0] = std::numeric_limits<std::int64_t>::max();
    data.inputs["aBelowMinimum"].integer = data.inputs["aAboveMaximum"].integer = 1;
    data.inputs["aDeactivateBackgroundVoxels"].integer = 1;
    assert(compute(data, true));
    const auto integerOut =
        std::dynamic_pointer_cast<const openvdb::Int64Grid>(outputData.grids[0]);
    assert(integerOut->data.leaf.active[0]);
    assert(integerOut->data.bg == std::numeric_limits<std::int64_t>::min());
    // Failure in a later grid must not publish already-computed earlier copies.
    setup();
    data = block();
    data.inputs["aAboveMaximum"].integer = 2;
    data.inputs["aOutputRange"].pair[1] = std::numeric_limits<float>::max();
    auto later = grid("later");
    later->data.leaf.values[0] = 2;
    inputData.grids.push_back(later);
    reject(data, true, false);
    assert(source()->data.leaf.values[1] == 1 && later->data.leaf.values[0] == 2);
}
void testFailures() {
    for (bool remap : {false, true}) {
        setup();
        auto data = block();
        assert(compute(data, remap));
        const auto baseline = operations;
        for (std::size_t i = 0; i < baseline.size(); ++i) {
            for (bool exception : {false, true}) {
                setup();
                data = block();
                if (exception)
                    attributeThrowAt = int(i);
                else
                    failAt = int(i);
                assert(!compute(data, remap) && invalidReads == 0);
                assert(publications == (baseline[i] == "output.clean" ? 1 : 0));
                assert(callCount == int(i) + 1 && source()->data.leaf.values[1] == 1);
            }
        }
    }
}
int main() {
    testMath();
    testCompute();
    testFailures();
    std::cout << "VDB value edits: actual Prune/Remap compute, dispatch, numeric and value-loop "
                 "tests passed (native boundaries doubled)\n";
}
