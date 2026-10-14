#include "vdb_transport_doubles.h"
#include "vdb_transport_compute.inc"

static DOpenVDBData velocityData, targetData;
static MDataBlock block(bool morph = false, bool masked = false) {
    MDataBlock data;
    for (const char* name : {"aTimestep",
                             "aSpatialScheme",
                             "aTemporalScheme",
                             "aSpatialRenormalization",
                             "aTemporalRenormalization",
                             "aRenormalizationSteps",
                             "aSubsteps",
                             "aAdvectionScheme",
                             "aLimiterScheme",
                             "aRespectGridClass",
                             "aVdbVelocityGridNames",
                             "aVdbSelectedGridNames",
                             "aAdvectSpatial",
                             "aAdvectTemporal",
                             "aRenormSpatial",
                             "aRenormTemporal",
                             "aSteps",
                             "aMinMask",
                             "aMaxMask",
                             "aInvertMask",
                             "aTargetVdbSelectedGridNames",
                             "aMaskName"})
        data.inputs[name];
    data.inputs["aTimestep"].number = .5;
    data.inputs["aMaxMask"].number = 1;
    data.inputs["aSubsteps"].integer = 1;
    data.inputs["aRenormalizationSteps"].integer = 3;
    data.inputs["aSteps"].integer = 3;
    data.inputs["aRespectGridClass"].integer = 1;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbVelocityInput"].payload = &velocityData;
    data.inputs["aTargetVdbInput"].payload = &targetData;
    data.inputs["aMaskVdbInput"].payload = masked ? &maskData : nullptr;
    (void)morph;
    return data;
}
static MStatus compute(MDataBlock& data, bool morph = false) {
    return morph ? DOpenVDBMorphLevelSet().compute(MPlug(DOpenVDBMorphLevelSet::aVdbOutput), data)
                 : DOpenVDBAdvect().compute(MPlug(DOpenVDBAdvect::aVdbOutput), data);
}
static void reset() {
    resetCalls();
    gridCopies = publications = invalidReads = 0;
    nullCopy = wrongCopy = invalidNativeOutput = false;
    stagedData = DOpenVDBData();
    outputData = DOpenVDBData();
    inputData = DOpenVDBData();
    velocityData = DOpenVDBData();
    targetData = DOpenVDBData();
    maskData = DOpenVDBData();
    inputData.grids = {std::make_shared<openvdb::FloatGrid>()};
    auto velocity = std::make_shared<openvdb::Vec3SGrid>();
    velocity->bg = openvdb::math::Vec3<float>(0);
    velocity->values = {openvdb::math::Vec3<float>(.1f)};
    velocityData.grids = {velocity};
    targetData.grids = {std::make_shared<openvdb::FloatGrid>()};
    maskData.grids = {std::make_shared<openvdb::FloatGrid>()};
    lastMask = nullptr;
}
static void rejected(MDataBlock& data, bool morph = false) {
    assert(!compute(data, morph));
    assert(!publications && !gridCopies && !invalidReads);
}
static void numericAndDomainTests() {
    dommy::vdb::TransportSettings settings;
    dommy::vdb::TransportPlan plan;
    settings.time = 1;
    assert(dommy::vdb::makeTransportPlan(settings, 64, 1, false, plan));
    assert(!dommy::vdb::makeTransportPlan(settings, 65, 1, false, plan));
    assert(!dommy::vdb::makeTransportPlan(settings, 1e30L, 1, false, plan));
    assert(dommy::vdb::validTransportMask(settings, 1));
    settings.maskMax = std::numeric_limits<float>::denorm_min();
    assert(!dommy::vdb::validTransportMask(settings, 1));
    settings.maskMin = -std::numeric_limits<float>::max();
    settings.maskMax = 0;
    assert(!dommy::vdb::validTransportMask(settings, std::numeric_limits<float>::max() / 64.0L));
    for (bool morph : {false, true}) {
        for (double bad : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), 1e300}) {
            reset();
            auto data = block(morph);
            data.inputs["aTimestep"].number = bad;
            rejected(data, morph);
        }
        for (const char* attribute :
             morph ? std::vector<const char*>{"aAdvectSpatial", "aAdvectTemporal", "aRenormSpatial",
                                              "aRenormTemporal"}
                   : std::vector<const char*>{"aSpatialScheme", "aTemporalScheme",
                                              "aSpatialRenormalization", "aTemporalRenormalization",
                                              "aAdvectionScheme", "aLimiterScheme"}) {
            for (int bad : {-1, 100}) {
                reset();
                auto data = block(morph);
                data.inputs[attribute].integer = bad;
                rejected(data, morph);
            }
        }
        for (int count : {-1, 1025, std::numeric_limits<int>::max()}) {
            reset();
            auto data = block(morph);
            data.inputs[morph ? "aSteps" : "aRenormalizationSteps"].integer = count;
            rejected(data, morph);
        }
        reset();
        auto data = block(morph);
        inputData.valid = false;
        rejected(data, morph);
        reset();
        data = block(morph);
        data.inputs["aVdbInput"].type = 999;
        rejected(data, morph);
        reset();
        data = block(morph);
        inputData.grids = {nullptr};
        rejected(data, morph);
        reset();
        data = block(morph);
        auto grid = std::make_shared<openvdb::FloatGrid>();
        grid->values[0] = std::numeric_limits<float>::quiet_NaN();
        inputData.grids = {grid};
        rejected(data, morph);
        reset();
        data = block(morph);
        grid = std::make_shared<openvdb::FloatGrid>();
        grid->dx = openvdb::Vec3R(0);
        inputData.grids = {grid};
        rejected(data, morph);
        reset();
        data = block(morph);
        grid = std::make_shared<openvdb::FloatGrid>();
        grid->bounds.high = openvdb::Coord(std::numeric_limits<int>::max(), 0, 0);
        inputData.grids = {grid};
        rejected(data, morph);
        reset();
        data = block(morph);
        grid = std::make_shared<openvdb::FloatGrid>();
        grid->activeCount = dommy::generation::kMaxWorkItemCount;
        inputData.grids = {grid};
        rejected(data, morph);
        reset();
        data = block(morph);
        grid = std::make_shared<openvdb::FloatGrid>();
        grid->xform.inverseShift[0] = 1e100;
        if (morph)
            targetData.grids = {grid};
        else {
            auto v = std::make_shared<openvdb::Vec3SGrid>();
            v->xform.inverseShift[0] = 1e100;
            velocityData.grids = {v};
        }
        rejected(data, morph);
        // A later bad grid must be rejected before copying the earlier good grid.
        reset();
        data = block(morph);
        grid = std::make_shared<openvdb::FloatGrid>();
        grid->uniform = false;
        inputData.grids.push_back(grid);
        rejected(data, morph);
    }
    for (int count : {0, -1, 1025, std::numeric_limits<int>::max()}) {
        reset();
        auto data = block();
        data.inputs["aSubsteps"].integer = count;
        rejected(data);
    }
    reset();
    auto data = block();
    data.inputs["aTimestep"].number = 1e6;
    rejected(data);
    reset();
    data = block();
    auto velocity = std::make_shared<openvdb::Vec3SGrid>();
    velocity->bg = openvdb::math::Vec3<float>(1e30f);
    velocityData.grids = {velocity};
    rejected(data);
    for (int bad = 0; bad < 7; ++bad) {
        reset();
        data = block(true, true);
        if (bad == 0)
            data.inputs["aMinMask"].number = 1;
        if (bad == 1)
            data.inputs["aMaxMask"].number = 1e300;
        if (bad == 2)
            data.inputs["aMaxMask"].number = std::numeric_limits<float>::denorm_min();
        if (bad == 3) {
            data.inputs["aMinMask"].number = -std::numeric_limits<float>::max();
            data.inputs["aMaxMask"].number = std::numeric_limits<float>::max();
        }
        if (bad == 4)
            data.inputs["aMaskVdbInput"].type = 999;
        if (bad == 5) {
            data.inputs["aMaskVdbInput"].payload = nullptr;
            data.inputs["aMaskVdbInput"].nonnullObject = true;
        }
        if (bad == 6)
            maskData.grids = {std::make_shared<openvdb::DoubleGrid>()};
        rejected(data, true);
    }
    // Selection remains first-match, not first-supported-match.
    reset();
    data = block();
    velocityData.grids.insert(velocityData.grids.begin(), std::make_shared<openvdb::FloatGrid>());
    rejected(data);
    reset();
    data = block(true);
    targetData.grids.insert(targetData.grids.begin(), std::make_shared<openvdb::DoubleGrid>());
    rejected(data, true);
}
static void outputTests() {
    for (bool staggered : {false, true})
        for (bool doubleVelocity : {false, true})
            for (bool level : {false, true}) {
                reset();
                auto data = block();
                if (doubleVelocity)
                    velocityData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
                if (staggered)
                    const_cast<Grid&>(*velocityData.grids[0]).kind = openvdb::GRID_STAGGERED;
                auto f = std::make_shared<openvdb::FloatGrid>();
                auto d = std::make_shared<openvdb::DoubleGrid>();
                auto vf = std::make_shared<openvdb::Vec3SGrid>();
                auto vd = std::make_shared<openvdb::Vec3DGrid>();
                if (!level) {
                    f->kind = d->kind = vf->kind = vd->kind = openvdb::GRID_UNKNOWN;
                }
                inputData.grids = {f, d, vf, vd, std::make_shared<openvdb::Int32Grid>()};
                assert(compute(data));
                assert(outputData.grids.size() == (level ? 2U : 4U));
                assert(publications == 1);
                assert(std::count(operations.begin(), operations.end(), "volume.create") ==
                       (level ? 0 : 1));
                assert(outputData.grids[0].get() != f.get() &&
                       outputData.grids[1].get() != d.get());
                assert(f->values[0] == 1 && d->values[0] == 1);
            }
    for (bool morph : {false, true})
        for (int failure = 0; failure < 3; ++failure) {
            reset();
            auto data = block(morph);
            nullCopy = failure == 0;
            wrongCopy = failure == 1;
            invalidNativeOutput = failure == 2;
            assert(!compute(data, morph));
            assert(!publications);
            assert(static_cast<const openvdb::FloatGrid&>(*inputData.grids[0]).values[0] == 1);
        }
    reset();
    auto data = block();
    data.inputs["aRespectGridClass"].integer = 0;
    invalidNativeOutput = true;
    assert(!compute(data) && !publications);
    reset();
    data = block();
    data.inputs["aVdbSelectedGridNames"].text = "absent";
    rejected(data);
    reset();
    data = block(true);
    data.inputs["aVdbSelectedGridNames"].text = "absent";
    assert(compute(data, true) && outputData.grids.empty());
    reset();
    data = block(true, true);
    data.inputs["aMinMask"].number = -1;
    assert(compute(data, true) && lastMask == maskData.grids[0].get());
    reset();
    data = block(true);
    data.inputs["aTimestep"].number = -1;
    assert(compute(data, true));
    assert(std::find(operations.begin(), operations.end(), "transport.advect") == operations.end());
    reset();
    data = block();
    data.inputs["aTimestep"].number = -.5;
    assert(compute(data));
    reset();
    data = block();
    data.inputs["aVdbSelectedGridNames"].text = "chosen";
    auto first = std::make_shared<openvdb::DoubleGrid>();
    first->name = "chosen";
    inputData.grids.push_back(first);
    assert(compute(data) && outputData.grids.size() == 1 &&
           outputData.grids[0]->isType<openvdb::DoubleGrid>());
}
static void failures() {
    for (int mode = 0; mode < 3; ++mode) {
        const bool morph = mode == 2;
        reset();
        auto data = block(morph, true);
        if (mode == 1)
            data.inputs["aRespectGridClass"].integer = 0;
        assert(compute(data, morph));
        const int count = callCount;
        const auto baseline = operations;
        for (int i = 0; i < count; ++i) {
            reset();
            data = block(morph, true);
            if (mode == 1)
                data.inputs["aRespectGridClass"].integer = 0;
            failAt = i;
            injectedStatus = MS::kInvalidParameter;
            const MStatus result = compute(data, morph);
            assert(!result && invalidReads == 0);
            if (baseline[i].find("input ") == 0 || baseline[i].find("output.") == 0)
                assert(result == injectedStatus);
            assert(callCount == i + 1);
            if (i < count - 1)
                assert(!publications);
            assert(static_cast<const openvdb::FloatGrid&>(*inputData.grids[0]).values[0] == 1);
            reset();
            data = block(morph, true);
            if (mode == 1)
                data.inputs["aRespectGridClass"].integer = 0;
            attributeThrowAt = i;
            assert(!compute(data, morph) && invalidReads == 0);
            assert(callCount == i + 1);
            if (i < count - 1)
                assert(!publications);
        }
        std::cout << (morph ? "Morph" : (mode == 1 ? "Advect Volume" : "Advect Level Set")) << ": "
                  << count << " operation failures/exceptions contained\n";
    }
}
int main() {
    numericAndDomainTests();
    outputTests();
    failures();
    std::cout << "Transport: numeric/domain/budget/selection/private-output and Float/Double "
                 "branches passed\n";
}
