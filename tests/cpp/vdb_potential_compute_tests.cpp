#include "vdb_clip_doubles.h"

// Allocation, sparse topology, sampling and native PCG are recording boundaries.
// The actual compatibility wrapper, boundary arithmetic and complete compute run.
static int domainCalls, neumannCalls, solverCalls, flowCalls, resampleCalls;
static int receivedDilation, receivedIterations;
static double receivedTolerance;
static bool converges = true, emptyDomain, nullDomain, nullNeumann, nullPotential, nullFlow;
static bool badNeumann, badPotential, badFlow, badState;
static bool receivedBackground, receivedStaggered;
namespace openvdb {
using Vec3f = math::Vec3<float>;
struct TopologyCopy {};
namespace util { struct NullInterrupter {}; }
namespace math { namespace pcg {
struct State { bool success = true; int iterations = 0; double absoluteError = 0, relativeError = 0; };
template<typename T> State terminationDefaults() { return State(); }
}}
template<typename G> typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr& grid) {
    return std::dynamic_pointer_cast<const G>(grid);
}
namespace tools {
struct PointSampler {};
template<typename G> BoolGrid::Ptr interiorMask(const G& grid) {
    nativeCall("native.mask");
    auto result = std::make_shared<BoolGrid>();
    result->xform = grid.xform;
    return result;
}
template<typename S, typename A, typename B> void resampleToMatch(const A&, B&) {
    nativeCall("native.resample"); ++resampleCalls;
}
template<typename G, typename M> typename M::Ptr createPotentialFlowMask(const G& grid, int dilation) {
    nativeCall("native.domain"); ++domainCalls; receivedDilation = dilation;
    if (nullDomain) return nullptr;
    auto result = std::make_shared<M>();
    result->xform = grid.xform; result->dx = grid.dx; result->vacant = emptyDomain;
    return result;
}
template<typename V, typename G, typename M>
typename G::template ValueConverter<V>::Type::Ptr createPotentialFlowNeumannVelocities(
    const G&, const M&, const typename G::template ValueConverter<V>::Type::ConstPtr, const V&) {
    nativeCall("native.neumann"); ++neumannCalls;
    if (nullNeumann) return nullptr;
    auto result = std::make_shared<typename G::template ValueConverter<V>::Type>();
    if (badNeumann) result->values[0] = InvalidFillValue<V>::get();
    return result;
}
namespace poisson {
template<typename T, typename Boundary, typename Interrupt>
typename T::Ptr solveWithBoundaryConditions(const T&, const Boundary& boundary,
    math::pcg::State& state, Interrupt&, bool staggered) {
    nativeCall("native.solve"); ++solverCalls;
    receivedStaggered = staggered; receivedIterations = state.iterations;
    receivedTolerance = state.absoluteError;
    state.success = converges;
    if (badState) state.absoluteError = std::numeric_limits<double>::quiet_NaN();
    double source = 0, diagonal = 0;
    // Also invoke the actual boundary through its copied, solver-facing object.
    auto copy = boundary;
    copy(Coord(10, 10, 10), Coord(9, 10, 10), source, diagonal);
    if (nullPotential) return nullptr;
    auto tree = std::make_shared<T>();
    if (badPotential) tree->sample = InvalidFillValue<typename T::ValueType>::get();
    return tree;
}
}
template<typename S, typename G> typename G::Ptr computePotentialFlow(
    const S&, const G&, const typename G::ValueType& background) {
    nativeCall("native.flow"); ++flowCalls;
    receivedBackground = background[0] != 0;
    if (nullFlow) return nullptr;
    auto result = std::make_shared<G>();
    if (badFlow) result->values[0] = InvalidFillValue<typename G::ValueType>::get();
    return result;
}
}}

#include "vdb_potential_compute.inc"

static DOpenVDBData velocityData, maskData;
static std::shared_ptr<openvdb::FloatGrid> solid;
void setup() {
    resetCalls();
    inputData = DOpenVDBData(); outputData = DOpenVDBData(); stagedData = DOpenVDBData();
    velocityData = DOpenVDBData(); maskData = DOpenVDBData();
    solid = std::make_shared<openvdb::FloatGrid>();
    solid->kind = openvdb::GRID_LEVEL_SET;
    solid->storedBox.reset({32, 32, 32}, {33, 33, 33});
    inputData.grids = {solid};
    auto velocity = std::make_shared<openvdb::Vec3fGrid>();
    velocity->storedBox = solid->storedBox;
    velocityData.grids = {velocity};
    auto mask = std::make_shared<openvdb::BoolGrid>();
    mask->storedBox = solid->storedBox;
    maskData.grids = {mask};
    socketConnections.clear(); meshConnected = true;
    publications = gridCopies = invalidReads = 0;
    nullCopy = nullOutput = invalidOutputPayload = false;
    domainCalls = neumannCalls = solverCalls = flowCalls = resampleCalls = 0;
    receivedDilation = receivedIterations = 0;
    converges = true;
    emptyDomain = nullDomain = nullNeumann = nullPotential = nullFlow = false;
    badNeumann = badPotential = badFlow = badState = false;
}
MDataBlock block() {
    MDataBlock b;
    b.inputs["aVdbInput"].payload = &inputData;
    b.inputs["aVdbSelectedGridNames"].text = "*";
    b.inputs["aVelocityVdb"].payload = &velocityData;
    b.inputs["aVelocityGridName"].text = "*";
    b.inputs["aMaskVdb"].payload = &maskData;
    b.inputs["aMaskGridName"].text = "*";
    b.inputs["aMaskType"].integer = 0;
    b.inputs["aUseWorldSpace"].integer = 1;
    b.inputs["aDilation"].number = 1;
    b.inputs["aDilationVoxels"].integer = 1;
    b.inputs["aUseIterations"].integer = 1;
    b.inputs["aIterations"].integer = 50;
    b.inputs["aUseTolerance"].integer = 1;
    b.inputs["aTolerance"].number = 1e-5f;
    b.inputs["aUseBackgroundVelocity"].integer = 1;
    b.inputs["aBackgroundVelocity"].floats[0] = 1;
    b.inputs["aApplyBackgroundVelocity"].integer = 1;
    b.inputs["aOutputPotential"].integer = 1;
    return b;
}
MStatus compute(MDataBlock& b) {
    return DVDBPotentialFlow().compute(MPlug(DVDBPotentialFlow::aVdbOutput), b);
}
void rejected(MDataBlock& b, bool preflight = true) {
    assert(!compute(b) && publications == 0 && invalidReads == 0 && gridCopies == 0);
    if (preflight) assert(domainCalls == 0 && solverCalls == 0 && flowCalls == 0);
}
template<typename F> void throws(F f) {
    bool caught = false;
    try { f(); } catch (const std::exception&) { caught = true; }
    assert(caught);
}
template<typename G> void boundaryCases() {
    G velocity;
    velocity.values[0] = typename G::ValueType(2, 3, 4);
    openvdb::BoolGrid domain;
    domain.dx = openvdb::Vec3d(2);
    dommy::vdb::PotentialBoundary<G> boundary(velocity, domain.voxelSize()[0]);
    const auto copy = boundary;
    domain.dx = openvdb::Vec3d(99);
    double source = 10, diagonal = -6;
    copy({1, 2, 3}, {0, 2, 3}, source, diagonal);
    assert(source == 14 && diagonal == -6);
    copy({1, 2, 3}, {1, 3, 3}, source, diagonal);
    assert(source == 8);
    copy({1, 2, 3}, {1, 2, 2}, source, diagonal);
    assert(source == 16);
    velocity.active = false;
    copy({}, {}, source, diagonal);
    assert(source == 16 && diagonal == -7);
    velocity.active = true;
    velocity.values[0] = openvdb::InvalidFillValue<typename G::ValueType>::get();
    throws([&] { copy({}, {}, source, diagonal); });
    throws([&] { dommy::vdb::PotentialBoundary<G> invalid(velocity, 0); });
    velocity.values[0] = typename G::ValueType(1);
    source = 0;
    copy({INT32_MAX, 0, 0}, {INT32_MIN, 0, 0}, source, diagonal);
    assert(source == 2.0 * (double(INT32_MAX) - INT32_MIN));
}
int main() {
    boundaryCases<openvdb::Vec3fGrid>();
    boundaryCases<openvdb::Vec3dGrid>();
    for (int count : {0, 257, INT32_MAX}) {
        setup(); auto data = block(); PotentialSettings settings;
        data.inputs["aUseWorldSpace"].integer = 0;
        data.inputs["aDilationVoxels"].integer = count;
        assert(!readPotentialSettings(data, 1, settings));
    }
    setup();
    openvdb::FloatGrid scaled;
    scaled.xform.forward[0][0] = 1000;
    scaled.xform.inverse[0][0] = .001;
    assert(!potentialSampleBox(openvdb::BBoxd(openvdb::Vec3d(32), openvdb::Vec3d(33)),
                               scaled, *solid));
    setup(); auto b = block();
    assert(compute(b) && publications == 1 && solverCalls == 1 && receivedStaggered &&
           receivedDilation == 1 && receivedIterations == 50 && receivedBackground && gridCopies == 0);
    assert(outputData.grids.size() == 2 && outputData.grids[0]->getName() == "flowvel" &&
           outputData.grids[1]->getName() == "potential");
    const auto trace = operations;
    int injected = 0;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        setup(); b = block(); failAt = int(i);
        assert(!compute(b) && invalidReads == 0);
        assert(publications == (trace[i] == "output.clean" ? 1 : 0));
        ++injected;
        setup(); b = block(); attributeThrowAt = int(i);
        assert(!compute(b) && invalidReads == 0);
        assert(publications == (trace[i] == "output.clean" ? 1 : 0));
        ++injected;
    }
    for (const auto& name : {"aDilation", "aTolerance"}) {
        for (float value : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            setup(); b = block(); b.inputs[name].number = value; rejected(b);
        }
    }
    setup(); b = block(); b.inputs["aDilation"].number = 1e10f; rejected(b);
    for (int value : {0, 257, INT32_MAX}) {
        setup(); b = block(); b.inputs["aUseWorldSpace"].integer = 0;
        b.inputs["aDilationVoxels"].integer = value; rejected(b);
    }
    for (int value : {0, 10001, INT32_MAX}) {
        setup(); b = block(); b.inputs["aIterations"].integer = value; rejected(b);
    }
    for (int value : {-1, 2, 42}) {
        setup(); b = block(); b.inputs["aMaskType"].integer = value; rejected(b);
    }
    for (int which = 0; which < 3; ++which) {
        setup(); b = block();
        (which == 0 ? inputData : which == 1 ? velocityData : maskData).valid = false; rejected(b);
    }
    setup(); b = block(); solid->values[0] = std::numeric_limits<float>::quiet_NaN(); rejected(b);
    setup(); b = block(); solid->values[0] = 1e20f; rejected(b);
    setup(); b = block(); solid->bg = -1; rejected(b);
    setup(); b = block(); solid->xform.linear = false; rejected(b);
    setup(); b = block(); solid->xform.uniform = false; rejected(b);
    setup(); b = block(); solid->xform.forward[0][0] = 1e100; rejected(b);
    setup(); b = block(); solid->dx = openvdb::Vec3d(0); rejected(b);
    setup(); b = block(); solid->memory = dommy::vdb::kMaxVdbPayloadBytes; rejected(b);
    setup(); b = block(); solid->storedBox.reset({0, 0, 0}, {100, 100, 100}); rejected(b);
    setup(); b = block(); solid->storedBox.reset({INT32_MAX - 10, 0, 0}, {INT32_MAX - 2, 0, 0}); rejected(b);
    setup(); b = block(); b.inputs["aBackgroundVelocity"].floats[1] = std::numeric_limits<float>::infinity(); rejected(b);
    setup(); b = block(); b.inputs["aBackgroundVelocity"].floats[0] = 1e30f; rejected(b);
    setup(); b = block(); b.inputs["aVelocityGridName"].text = "missing"; rejected(b);
    setup(); b = block(); b.inputs["aMaskGridName"].text = "missing"; rejected(b);
    setup(); b = block();
    auto velocity = std::make_shared<openvdb::Vec3fGrid>();
    velocity->values[0][0] = std::numeric_limits<float>::infinity();
    velocityData.grids = {velocity}; rejected(b);
    setup(); b = block();
    auto mask = std::make_shared<openvdb::FloatGrid>();
    mask->xform.forward[0][0] = 1000; mask->xform.inverse[0][0] = .001;
    maskData.grids = {mask}; rejected(b);
    setup(); b = block(); mask = std::make_shared<openvdb::FloatGrid>();
    mask->kind = openvdb::GRID_LEVEL_SET; mask->bg = -3; maskData.grids = {mask}; rejected(b);
    for (bool* flag : {&nullDomain, &nullNeumann, &nullPotential, &nullFlow, &badNeumann,
                       &badPotential, &badFlow, &badState, &nullCopy, &nullOutput, &invalidOutputPayload}) {
        setup(); b = block(); *flag = true; rejected(b, false);
    }
    setup(); b = block(); converges = false;
    assert(compute(b) && publications == 1 && flowCalls == 1); // Historical finite warning policy.
    setup(); b = block(); emptyDomain = true;
    assert(compute(b) && solverCalls == 0 && flowCalls == 0 && outputData.grids.size() == 2);
    setup(); b = block(); velocityData.grids = {std::make_shared<openvdb::Vec3dGrid>()};
    assert(compute(b) && getVdbGridType(*outputData.grids[0]) == DVdbGridType::kVec3d &&
           getVdbGridType(*outputData.grids[1]) == DVdbGridType::kDouble);
    setup(); b = block(); b.inputs["aOutputPotential"].integer = 0;
    b.inputs["aApplyBackgroundVelocity"].integer = 0;
    assert(compute(b) && outputData.grids.size() == 1 && !receivedBackground);
    setup(); b = block(); b.inputs["aDilation"].number = 1.1f;
    assert(compute(b) && receivedDilation == 2);
    setup(); b = block(); b.inputs["aUseWorldSpace"].integer = 0;
    b.inputs.erase("aDilation"); assert(compute(b));
    setup(); b = block(); b.inputs["aUseIterations"].integer = 0; b.inputs.erase("aIterations");
    assert(compute(b) && receivedIterations == 10000);
    setup(); b = block(); b.inputs["aUseTolerance"].integer = 0; b.inputs.erase("aTolerance");
    assert(compute(b) && receivedTolerance == static_cast<float>(1e-20));
    setup(); b = block(); b.inputs["aUseBackgroundVelocity"].integer = 0;
    b.inputs.erase("aBackgroundVelocity"); b.inputs.erase("aApplyBackgroundVelocity");
    assert(compute(b) && !receivedBackground);
    setup(); b = block(); socketConnections["aVelocityVdb"] = socketConnections["aMaskVdb"] = false;
    for (const auto& name : {"aVelocityVdb", "aVelocityGridName", "aMaskVdb", "aMaskGridName", "aMaskType"})
        b.inputs.erase(name);
    assert(compute(b) && getVdbGridType(*outputData.grids[0]) == DVdbGridType::kVec3f);
    setup(); b = block(); b.inputs["aMaskType"].integer = 1;
    assert(compute(b) && std::find(operations.begin(), operations.end(), "native.difference") != operations.end());
    setup(); b = block(); mask = std::make_shared<openvdb::FloatGrid>();
    mask->storedBox = solid->storedBox;
    mask->xform.shift[0] = 1; mask->xform.inverseShift[0] = -1; maskData.grids = {mask};
    assert(compute(b) && resampleCalls == 1);
    setup(); b = block(); maskData.grids = {std::make_shared<openvdb::points::PointDataGrid>()};
    assert(compute(b));
    setup(); b = block(); maskData.grids = {std::make_shared<openvdb::points::PointIndexGrid>()};
    assert(compute(b));
    setup(); b = block(); mask = std::make_shared<openvdb::FloatGrid>();
    mask->values.clear(); mask->vacant = true;
    mask->xform.forward[0][0] = 1000; mask->xform.inverse[0][0] = .001;
    maskData.grids = {mask}; rejected(b); // Empty inputs still initialize native resamplers.
    setup(); b = block(); b.inputs["aVelocityGridName"].text = "";
    b.inputs["aMaskGridName"].text = "";
    b.inputs.erase("aVelocityVdb"); b.inputs.erase("aMaskVdb"); b.inputs.erase("aMaskType");
    assert(compute(b));
    setup(); b = block(); solid->tile = true; solid->active = false; solid->values[0] = -3;
    assert(compute(b)); // An inactive negative tile still contributes interior bounds.
    setup(); b = block(); solid->storedBox.reset({32, 32, 32}, {33, 33, 33});
    b.inputs["aDilation"].number = 10; b.inputs["aIterations"].integer = 1000;
    assert(compute(b)); // Typical legacy defaults remain admissible for a small domain.
    setup(); b = block(); b.inputs["aUseWorldSpace"].integer = 0;
    assert(compute(b));
    const auto voxelTrace = operations;
    const auto voxelRead = std::find(voxelTrace.begin(), voxelTrace.end(), "input aDilationVoxels");
    assert(voxelRead != voxelTrace.end());
    setup(); b = block(); b.inputs["aUseWorldSpace"].integer = 0;
    failAt = int(voxelRead - voxelTrace.begin()); rejected(b);
    setup(); b = block();
    assert(DVDBPotentialFlow().compute(MPlug(MObject{"other"}), b) == MS::kUnknownParameter && operations.empty());
    std::cout << "Potential Flow compute: boundary ownership, guards, compatibility and "
              << injected << " status/exception injections passed\n";
}
