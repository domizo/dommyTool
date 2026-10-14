#include "vdb_clip_doubles.h"

// Host, tree allocation and Poisson are recording boundaries. Production
// settings, preflight, transaction, pairing and projection bodies run unchanged.
static int solverCalls, receivedIterations, receivedColliderType, failSolverAt = -1;
static int badResultAt = -1;
static bool converges = true, nullPressure, wrongPressure, badPressure;
namespace openvdb {
using Index = unsigned int;
namespace util { struct NullInterrupter {}; }
namespace math { namespace pcg {
struct State { bool success = true; int iterations = 0; double absoluteError = 0, relativeError = 0; };
template<typename T> State terminationDefaults() { return State(); }
}}
}
namespace tbb {
template<typename T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b) : first(a), last(b) {}
    T begin() const { return first; } T end() const { return last; }
};
}

#include "vdb_divergence_compute.inc"

template<typename G> bool processGrid(SolverParms& parms) {
    nativeCall("native.solver");
    const int index = solverCalls++;
    if (index == failSolverAt) throw std::runtime_error("late solver failure");
    receivedIterations = parms.iterations;
    receivedColliderType = parms.colliderType;
    using Scalar = typename G::template ValueConverter<typename G::ValueType::ValueType>::Type;
    auto pressure = std::make_shared<Scalar>();
    pressure->name = parms.velocityGrid->getName() + "_pressure";
    if (badPressure) pressure->values[0] = std::numeric_limits<typename Scalar::ValueType>::infinity();
    parms.pressureGrid = nullPressure ? nullptr : pressure;
    if (wrongPressure) parms.pressureGrid = std::make_shared<openvdb::GridBase>();
    auto& velocity = static_cast<G&>(*parms.velocityGrid);
    velocity.values[0] = typename G::ValueType(index == badResultAt ?
        std::numeric_limits<typename G::ValueType::ValueType>::infinity() : 2);
    return converges;
}

template<typename T> struct ProjectionLeafOf {
    using Value = openvdb::math::Vec3<T>;
    bool active = true;
    openvdb::Coord location;
    Value value{1};
    explicit ProjectionLeafOf(int x = 0) : location(x, 0, 0) {}
    bool isInactive() const { return !active; }
    openvdb::Coord origin() const { return location; }
    struct Buffer { Value* p; Value* data() { return p; } const Value* data() const { return p; } };
    Buffer buffer() { return {&value}; }
    Buffer buffer() const { return {const_cast<Value*>(&value)}; }
    struct ValueOnIter {
        bool active;
        explicit operator bool() const { return active; }
        ValueOnIter& operator++() { active = false; return *this; }
        unsigned int pos() const { return 0; }
    };
    ValueOnIter beginValueOn() { return {active}; }
};
template<typename T> struct ProjectionTreeOf {
    using LeafNodeType = ProjectionLeafOf<T>;
    using ValueType = typename LeafNodeType::Value;
    std::vector<LeafNodeType*> leaves;
    bool mismatchedOrigin = false;
    void getNodes(std::vector<LeafNodeType*>& out) { out = leaves; }
    const LeafNodeType* probeConstLeaf(openvdb::Coord coord) const {
        for (const auto* leaf : leaves)
            if (mismatchedOrigin || !(leaf->origin() != coord)) return leaf;
        return nullptr;
    }
};
using ProjectionLeaf = ProjectionLeafOf<float>;
using ProjectionTree = ProjectionTreeOf<float>;
template<typename F> void throws(F function) {
    bool caught = false;
    try { function(); } catch (const std::exception&) { caught = true; }
    assert(caught);
}
void projection() {
    SolverParms parms;
    auto grid = std::make_shared<openvdb::Vec3SGrid>();
    parms.velocityGrid = grid;
    ProjectionLeaf inactive(16), first(8), second(0), gradientA(0), gradientB(8);
    inactive.active = false; inactive.value = ProjectionLeaf::Value(99);
    gradientA.value = ProjectionLeaf::Value(2);
    gradientB.value = ProjectionLeaf::Value(3);
    ProjectionTree velocity, gradient;
    velocity.leaves = {&inactive, &first, &second};
    gradient.leaves = {&gradientA, &gradientB};
    const auto pairs = pressureLeafPairs(velocity, gradient);
    assert(pairs.size() == 2 && pairs[0].velocity == &first && pairs[0].gradient == &gradientB &&
           pairs[1].velocity == &second && pairs[1].gradient == &gradientA);
    PressureProjectionOp<ProjectionTree>(parms, pairs, false)({0, pairs.size()});
    assert(first.value[0] == -11 && second.value[0] == -7 && inactive.value[0] == 99);
    first.value = ProjectionLeaf::Value(1);
    PressureProjectionOp<ProjectionTree>(parms, pairs, true)({0, 1});
    assert(first.value[0] == -2);
    gradient.leaves = {&gradientA};
    throws([&] { pressureLeafPairs(velocity, gradient); });
    gradient.mismatchedOrigin = true;
    throws([&] { pressureLeafPairs(velocity, gradient); });
    velocity.leaves = {nullptr};
    throws([&] { pressureLeafPairs(velocity, gradient); });
    throws([&] { PressureProjectionOp<ProjectionTree>(parms, pairs, false)({0, 3}); });
    first.value = ProjectionLeaf::Value(1e38f); gradientB.value = ProjectionLeaf::Value(-1e38f);
    throws([&] { PressureProjectionOp<ProjectionTree>(parms, pairs, false)({0, 1}); });
    first.value = ProjectionLeaf::Value(std::numeric_limits<float>::quiet_NaN());
    throws([&] { PressureProjectionOp<ProjectionTree>(parms, pairs, true)({0, 1}); });
    grid->xform.forward[0][0] = 1e100;
    throws([&] { PressureProjectionOp<ProjectionTree>(parms, pairs, false)({0, 1}); });
    grid->xform.forward[0][0] = 0;
    throws([&] { PressureProjectionOp<ProjectionTree>(parms, pairs, false)({0, 1}); });
    parms.velocityGrid = std::make_shared<openvdb::Vec3DGrid>();
    ProjectionLeafOf<double> dv, dg;
    dv.value = openvdb::math::Vec3<double>(2e200);
    dg.value = openvdb::math::Vec3<double>(1e200);
    ProjectionTreeOf<double> vt, gt; vt.leaves = {&dv}; gt.leaves = {&dg};
    const auto dp = pressureLeafPairs(vt, gt);
    PressureProjectionOp<ProjectionTreeOf<double>>(parms, dp, false)({0, 1});
    assert(std::fabs(dv.value[0] / -2e200 - 1) < 1e-15);
    dv.value = openvdb::math::Vec3<double>(1e308); dg.value = openvdb::math::Vec3<double>(-1e308);
    throws([&] { PressureProjectionOp<ProjectionTreeOf<double>>(parms, dp, false)({0, 1}); });
}

static DOpenVDBData colliderData;
void setup() {
    resetCalls();
    inputData = DOpenVDBData(); colliderData = DOpenVDBData();
    stagedData = DOpenVDBData(); outputData = DOpenVDBData();
    inputData.grids = {std::make_shared<openvdb::Vec3SGrid>()};
    colliderData.grids = {std::make_shared<openvdb::BoolGrid>()};
    publications = invalidReads = solverCalls = maskCalls = receivedIterations = gridCopies = 0;
    failSolverAt = badResultAt = -1; converges = true;
    nullPressure = wrongPressure = badPressure = false;
    emptyInteriorMask = false;
    nullCopy = wrongCopy = nullOutput = invalidOutputPayload = false;
    activeWorldBox = openvdb::BBoxd();
}
std::shared_ptr<openvdb::Vec3SGrid> source() {
    return std::const_pointer_cast<openvdb::Vec3SGrid>(
        std::dynamic_pointer_cast<const openvdb::Vec3SGrid>(inputData.grids.front()));
}
MDataBlock block(bool collider = false, int type = 0) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "*";
    data.inputs["aUseiterations"].integer = 1; data.inputs["aIterations"].integer = 50;
    data.inputs["aUsetolerance"].integer = 1; data.inputs["aTolerance"].number = 1e-5f;
    data.inputs["aPressure"].integer = 1;
    data.inputs["aUsecollider"].integer = collider;
    data.inputs["aCollidertype"].integer = type;
    data.inputs["aInvertCollider"].integer = 0;
    data.inputs["aCollideVdbName"].text = "*";
    data.inputs["aCollideVdb"].payload = &colliderData;
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBRemoveDivergence().compute(MPlug(DOpenVDBRemoveDivergence::aVdbOutput), data);
}
void reject(MDataBlock& data) {
    assert(!compute(data));
    assert(publications == 0 && invalidReads == 0 && solverCalls == 0 && gridCopies == 0 && maskCalls == 0);
}
void validCases() {
    for (int kind = 0; kind < 5; ++kind) {
        setup(); auto data = block(kind > 0, kind == 4 ? 2 : std::max(0, kind - 1));
        if (kind == 3) {
            auto collider = std::make_shared<openvdb::FloatGrid>();
            collider->kind = openvdb::GRID_LEVEL_SET;
            colliderData.grids = {collider};
        }
        if (kind == 4) colliderData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
        assert(compute(data) && solverCalls == 1 && publications == 1);
        assert(outputData.grids.size() == 2 && outputData.grids[0].get() != source().get());
        assert(outputData.grids[1]->getName() == "source_pressure" && source()->values[0][0] == 1);
    }
    setup(); auto data = block(); inputData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
    assert(compute(data) && getVdbGridType(*outputData.grids[1]) == DVdbGridType::kDouble);
    setup(); data = block(); data.inputs["aPressure"].integer = 0;
    assert(compute(data) && outputData.grids.size() == 1);
    setup(); data = block(); converges = false;
    assert(compute(data) && publications == 1 && outputData.grids[0].get() == source().get());
    setup(); data = block();
    for (const char* name : {"aCollideVdb", "aCollideVdbName", "aCollidertype", "aInvertCollider"}) data.inputs.erase(name);
    data.inputs["aUsetolerance"].integer = 0; data.inputs.erase("aTolerance");
    assert(compute(data));
    setup(); data = block(); data.inputs["aUseiterations"].integer = 0; data.inputs.erase("aIterations");
    DivergenceSettings settings; assert(readDivergenceSettings(data, settings) && settings.iterations == 10000);
    resetCalls(); reject(data); // worst-case 10000-iteration work for this domain
    setup(); data = block(); data.inputs["aVdbSelectedGridNames"].text = "none";
    data.inputs.erase("aUseiterations");
    assert(compute(data) && solverCalls == 0 && gridCopies == 0 && outputData.grids.size() == 1);
    setup(); data = block(); inputData.grids = {std::make_shared<openvdb::FloatGrid>()};
    assert(compute(data) && solverCalls == 0 && outputData.grids.size() == 1);
    setup(); data = block(); source()->xform.uniform = false;
    assert(compute(data) && solverCalls == 0);
    setup(); data = block(true, 1); source()->xform.shift[0] = 1;
    assert(compute(data) && solverCalls == 0 && maskCalls == 0);
    setup(); data = block(); source()->vacant = true;
    assert(compute(data) && solverCalls == 0);
    for (bool empty : {false, true}) {
        setup(); data = block(true, 1);
        auto collider = std::make_shared<openvdb::FloatGrid>();
        collider->kind = openvdb::GRID_LEVEL_SET; collider->vacant = true;
        colliderData.grids = {collider}; emptyInteriorMask = empty;
        assert(compute(data) && maskCalls == 1 && receivedColliderType == (empty ? CT_NONE : CT_STATIC));
    }
    setup(); data = block();
    assert(DOpenVDBRemoveDivergence().compute(MPlug({"unknown"}), data) == MS::kUnknownParameter && callCount == 0);
}
void invalidCases() {
    for (int n : {-1, 0, 10001, INT32_MAX}) {
        setup(); auto data = block(); data.inputs["aIterations"].integer = n;
        DivergenceSettings settings; assert(!readDivergenceSettings(data, settings));
        resetCalls(); reject(data);
    }
    for (float value : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        setup(); auto data = block(); data.inputs["aTolerance"].number = value; reject(data);
    }
    for (int kind = 0; kind < 14; ++kind) {
        setup(); auto data = block(kind >= 4, 1);
        if (kind == 0) data.inputs["aVdbInput"].payload = nullptr;
        if (kind == 1) data.inputs["aVdbInput"].type = 999;
        if (kind == 2) inputData.valid = false;
        if (kind == 3) inputData.grids[0] = nullptr;
        if (kind == 4) data.inputs["aCollideVdb"].payload = nullptr;
        if (kind == 5) data.inputs["aCollideVdb"].type = 999;
        if (kind == 6) colliderData.valid = false;
        if (kind == 7) colliderData.grids[0] = nullptr;
        if (kind == 8) data.inputs["aCollideVdbName"].text = "missing";
        if (kind == 9) data.inputs["aCollidertype"].integer = 3;
        if (kind == 10) data.inputs["aCollidertype"].integer = -1;
        if (kind == 11) colliderData.grids = {std::make_shared<openvdb::GridBase>()};
        if (kind == 12) {
            auto grid = std::make_shared<openvdb::Int32Grid>(); grid->kind = openvdb::GRID_LEVEL_SET; colliderData.grids = {grid};
        }
        if (kind == 13) {
            auto grid = std::make_shared<openvdb::FloatGrid>(); grid->kind = openvdb::GRID_LEVEL_SET;
            grid->bg = 0; colliderData.grids = {grid};
        }
        reject(data);
    }
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 1e30f})
        for (bool background : {false, true}) {
            setup(); auto data = block();
            if (background) source()->bg[0] = value; else source()->values[0][0] = value;
            reject(data);
        }
    for (int kind = 0; kind < 8; ++kind) {
        setup(); auto data = block();
        if (kind == 0) source()->dx[0] = 0;
        if (kind == 1) source()->xform.linear = false;
        if (kind == 2) source()->xform.shift[0] = std::numeric_limits<double>::infinity();
        if (kind == 3) source()->xform.forward[0][0] = 1e7;
        if (kind == 4) source()->xform.inverse[0][0] = 1e7;
        if (kind == 5) source()->storedBox.reset({INT32_MAX-7,0,0}, {INT32_MAX,0,0});
        if (kind == 6) source()->storedBox.reset({INT32_MIN,0,0}, {INT32_MIN+7,0,0});
        if (kind == 7) source()->memory = dommy::vdb::kMaxVdbPayloadBytes;
        reject(data);
    }
    setup(); auto data = block(); source()->tile = true; source()->storedBox.reset({0,0,0}, {4095,4095,4095}); reject(data);
    setup(); data = block(); source()->storedBox.reset({0,0,0}, {63,63,63}); reject(data);
    setup(); data = block(true, 0); activeWorldBox = openvdb::BBoxd(openvdb::Vec3R(0), openvdb::Vec3R(3e9)); reject(data);
    setup(); data = block(true, 0); activeWorldBox.min()[0] = std::numeric_limits<double>::quiet_NaN(); reject(data);
    setup(); data = block(true, 2);
    auto collider = std::make_shared<openvdb::Vec3DGrid>(); collider->values[0][0] = 1e60;
    colliderData.grids = {collider}; reject(data);
    setup(); data = block(); auto bad = std::make_shared<openvdb::Vec3SGrid>();
    bad->values[0][0] = std::numeric_limits<float>::quiet_NaN(); inputData.grids.push_back(bad); reject(data);
    setup(); data = block(); inputData.grids.resize(4, source()); reject(data); // aggregate tree budget
}
void failures() {
    int injected = 0;
    for (int kind = 0; kind < 5; ++kind) {
        setup(); auto data = block(kind != 0, kind >= 3 ? 2 : kind - 1);
        if (kind == 3) colliderData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
        if (kind == 4) {
            auto collider = std::make_shared<openvdb::FloatGrid>(); collider->kind = openvdb::GRID_LEVEL_SET;
            colliderData.grids = {collider};
        }
        assert(compute(data)); const auto trace = operations;
        for (int mode = 0; mode < 3; ++mode) for (int failure = 0; failure < int(trace.size()); ++failure) {
            setup(); data = block(kind != 0, kind >= 3 ? 2 : kind - 1);
            if (kind == 3) colliderData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
            if (kind == 4) {
                auto collider = std::make_shared<openvdb::FloatGrid>(); collider->kind = openvdb::GRID_LEVEL_SET;
                colliderData.grids = {collider};
            }
            if (mode == 2) attributeThrowAt = failure;
            else { failAt = failure; injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter; }
            assert(!compute(data) && invalidReads == 0);
            assert(publications == (trace[failure] == "output.clean" ? 1 : 0)); ++injected;
        }
    }
    for (int kind = 0; kind < 7; ++kind) {
        setup(); auto data = block();
        if (kind == 0) nullCopy = true;
        if (kind == 1) wrongCopy = true;
        if (kind == 2) nullPressure = true;
        if (kind == 3) wrongPressure = true;
        if (kind == 4) badPressure = true;
        if (kind == 5) badResultAt = 0;
        if (kind == 6) nullOutput = true;
        assert(!compute(data) && publications == 0 && source()->values[0][0] == 1);
    }
    for (bool bad : {false, true}) {
        setup(); auto data = block(); inputData.grids.push_back(std::make_shared<openvdb::Vec3SGrid>());
        for (const auto& grid : inputData.grids)
            const_cast<openvdb::Vec3SGrid&>(static_cast<const openvdb::Vec3SGrid&>(*grid)).storedBox.reset({8,8,8}, {15,15,15});
        if (bad) badResultAt = 1; else failSolverAt = 1;
        assert(!compute(data) && solverCalls == 2 && publications == 0 && source()->values[0][0] == 1);
    }
    std::cout << "Remove Divergence: " << injected << " failure injections, preflight, transaction and projection cases passed\n";
}
int main() { projection(); validCases(); invalidCases(); failures(); }
