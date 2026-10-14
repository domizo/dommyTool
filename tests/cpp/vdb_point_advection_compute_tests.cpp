#include "vdb_point_advection_doubles.h"
#include "vdb_point_advection_compute.inc"

static DOpenVDBData velocityData, cptData;
static MDataBlock block(int mode = 0, bool streamlines = true) {
    MDataBlock data;
    for (const char* name :
         {"aOperation", "aIntegration", "aTimestep", "aSubsteps", "aIterations",
          "aOutputStreamlines", "aVdbVelocityGridNames", "aVdbClosestPointGridNames"})
        data.inputs[name];
    data.inputs["inDP"].payload = &pointInput;
    data.inputs["inDP"].type = DPData::id;
    data.inputs["aVdbVelocityInput"].payload = &velocityData;
    data.inputs["aVdbClosestPointInput"].payload = &cptData;
    data.inputs["aOperation"].integer = mode;
    data.inputs["aTimestep"].number = .5;
    data.inputs["aSubsteps"].integer = 1;
    data.inputs["aIterations"].integer = 1;
    data.inputs["aOutputStreamlines"].integer = streamlines;
    return data;
}
static MStatus compute(MDataBlock& data, bool curves = false) {
    return DOpenVDBAdvectPoints().compute(
        MPlug(curves ? DOpenVDBAdvectPoints::outDC : DOpenVDBAdvectPoints::outDP), data);
}
template <typename G> static void grids(bool staggered = false) {
    auto velocity = std::make_shared<G>();
    velocity->bg = typename G::ValueType(1, 0, 0);
    velocity->values = {velocity->bg};
    velocity->kind = staggered ? openvdb::GRID_STAGGERED : openvdb::GRID_UNKNOWN;
    auto cpt = std::make_shared<G>();
    cpt->bg = typename G::ValueType(2, 0, 0);
    cpt->values = {cpt->bg};
    velocityData.grids = {velocity};
    cptData.grids = {cpt};
}
static void reset() {
    resetCalls();
    pointCopies = outputCreates = curveBatches = nativeSamples = nativeSteps = projectionSteps =
        cleanCalls = 0;
    publications = invalidReads = gridCopies = 0;
    corruptStep = corruptProjection = -1;
    corruptValue = 0;
    inWorker = invalidCurve = missingOutput = false;
    pointInput = DPData();
    pointStage = DPData();
    pointOutput = DPData();
    curveStage = DCurveData();
    curveOutput = DCurveData();
    pointInput.points = {DPoint(0, 0, 0), DPoint(1, 0, 0)};
    pointInput.attributes["tag"] =
        std::make_shared<TypedDPAttributeData<int32_t>>(std::vector<int32_t>{7, 9});
    pointInput.groups["selected"] = std::make_shared<Group>();
    pointInput.groups["selected"]->ids = {true, false};
    velocityData = DOpenVDBData();
    cptData = DOpenVDBData();
    grids<openvdb::Vec3SGrid>();
}
static void unchanged() {
    assert(pointInput.points[0][0] == 0 && pointInput.points[1][0] == 1);
    assert(velocityData.grids[0]->isType<openvdb::Vec3SGrid>() ||
           velocityData.grids[0]->isType<openvdb::Vec3DGrid>());
    assert(invalidReads == 0 && gridCopies == 0);
}
static void rejected(MDataBlock& data, bool preflight = true) {
    assert(!compute(data));
    assert(publications == 0 && cleanCalls == 0);
    if (preflight)
        assert(pointCopies == 0 && outputCreates == 0 && nativeSamples == 0);
    assert(invalidReads == 0);
}

int main() {
    // Test admission directly before any mutation can launch excessive work.
    reset();
    AdvectionPointParms budget;
    budget.mStreamlines = true;
    budget.mSteps = 900000;
    std::size_t workItems = 0;
    bool memoryRejected = false;
    try {
        memoryRejected = !validateAdvectionBudget(budget, pointInput, workItems);
    } catch (const std::length_error&) {
        memoryRejected = true;
    }
    assert(memoryRejected);
    DPData manyPoints;
    manyPoints.points.resize(100000);
    budget.mStreamlines = false;
    budget.mSteps = 1000;
    assert(validateAdvectionBudget(budget, manyPoints, workItems));
    budget.mIntegrationType = INTEGRATION_TYPE_RK_4TH;
    assert(!validateAdvectionBudget(budget, manyPoints, workItems));
    for (double invalid : {double(NAN), double(INFINITY), 1e100}) {
        bool failed = false;
        try {
            (void)checkedPoint(openvdb::math::Vec3<double>(invalid, 0, 0));
        } catch (const std::domain_error&) {
            failed = true;
        }
        assert(failed);
    }
    reset();
    std::vector<MPointArray> unequal(1);
    assert(!appendLineNodes(unequal, pointInput.points));
    // Mode/precision/order/staggered/output matrix. A single substep produces two CVs.
    for (bool doubles : {false, true})
        for (int mode : {0, 1, 2})
            for (int order = 0; order < 4; ++order)
                for (bool staggered : {false, true})
                    for (bool streamlines : {false, true})
                        for (int steps : {1, 2, 3})
                            for (bool curves : {false, true}) {
                                reset();
                                if (doubles)
                                    grids<openvdb::Vec3DGrid>(staggered);
                                else
                                    grids<openvdb::Vec3SGrid>(staggered);
                                auto data = block(mode, streamlines);
                                data.inputs["aIntegration"].integer = order;
                                data.inputs["aSubsteps"].integer = steps;
                                data.inputs["aIterations"].integer = 2;
                                assert(compute(data, curves));
                                assert(publications == 1 && cleanCalls == 1 && pointCopies == 1);
                                assert(data.cleanedOutput == (curves ? "outDC" : "outDP"));
                                assert(data.requestedOutput == data.cleanedOutput);
                                assert(pointStage.attributes["tag"] !=
                                       pointInput.attributes["tag"]);
                                assert(pointStage.groups["selected"] !=
                                       pointInput.groups["selected"]);
                                assert(pointStage.groups["selected"]->ids ==
                                       pointInput.groups["selected"]->ids);
                                auto tag = std::dynamic_pointer_cast<TypedDPAttributeData<int32_t>>(
                                    pointStage.attributes["tag"]);
                                assert(tag->value() == std::vector<int32_t>({7, 9}));
                                assert(std::fabs(pointStage.points[0][0] - (mode == 0 ? .5 : 2.0)) <
                                       1e-5);
                                if (mode != 1) {
                                    assert(lastOrder == order + 1 && lastStaggered == staggered);
                                }
                                if (streamlines && mode != 1) {
                                    assert(curveBatches == 1 && curveStage.curves.size() == 2);
                                    assert(curveStage.curves[0]->points.length() ==
                                           unsigned(steps + 1));
                                    assert(curveStage.curves[0]->degree == (steps < 3 ? 1 : 3));
                                    assert(curveStage.curves[0]->points.values.front().x == 0);
                                } else
                                    assert(curveBatches == 0 && curveStage.curves.empty());
                                unchanged();
                            }
    // Exhaust every recorded Maya/native boundary in each relevant mode.
    for (int mode : {0, 1, 2}) {
        reset();
        auto baseline = block(mode);
        assert(compute(baseline));
        const int count = callCount;
        for (int fail = 0; fail < count; ++fail)
            for (bool throwing : {false, true}) {
                reset();
                auto data = block(mode);
                resetCalls(throwing ? -1 : fail);
                if (throwing)
                    attributeThrowAt = fail;
                assert(!compute(data));
                assert(callCount == fail + 1); // Stop at the first failed boundary.
                assert(invalidReads == 0 && cleanCalls == 0);
                assert(publications == (fail == count - 1 ? 1 : 0));
                unchanged();
            }
    }
    reset();
    auto unknown = block();
    assert(DOpenVDBAdvectPoints().compute(MPlug(MObject{"other"}), unknown) ==
           MS::kUnknownParameter);
    assert(callCount == 0);
    // Only the selected mode's input handles are consumed.
    reset();
    auto projection = block(1);
    for (const char* name : {"aVdbVelocityInput", "aVdbVelocityGridNames", "aTimestep", "aSubsteps",
                             "aIntegration", "aOutputStreamlines"})
        projection.inputs.erase(name);
    assert(compute(projection) && nativeSteps == 0);
    reset();
    auto advection = block();
    for (const char* name : {"aVdbClosestPointInput", "aVdbClosestPointGridNames", "aIterations"})
        advection.inputs.erase(name);
    assert(compute(advection) && projectionSteps == 0);
    for (const char* key : {"inDP", "aVdbVelocityInput", "aVdbClosestPointInput"}) {
        reset();
        auto data = block(2);
        data.inputs[key].payload = nullptr;
        rejected(data);
        reset();
        data = block(2);
        data.inputs[key].type = -5;
        rejected(data);
        reset();
        data = block(2);
        data.inputs[key].payload = key == std::string("inDP") ? static_cast<MPxData*>(&velocityData)
                                                              : static_cast<MPxData*>(&pointInput);
        rejected(data);
    }
    for (int mode : {0, 1, 2}) {
        reset();
        auto data = block(mode);
        velocityData.valid = cptData.valid = false;
        rejected(data);
        reset();
        data = block(mode);
        pointInput.valid = false;
        rejected(data);
    }
    reset();
    auto mismatch = block(2);
    cptData.grids = {std::make_shared<openvdb::Vec3DGrid>()};
    rejected(mismatch);
    for (const char* key : {"aOperation", "aIntegration"})
        for (int value : {-1, 99}) {
            reset();
            auto data = block();
            data.inputs[key].integer = value;
            rejected(data);
        }
    for (const char* key : {"aSubsteps", "aIterations"})
        for (int value : {-1, 0, 1000001}) {
            reset();
            auto data = block(2);
            data.inputs[key].integer = value;
            rejected(data);
        }
    for (double value :
         {double(NAN), double(INFINITY), double(-INFINITY), std::numeric_limits<double>::max()}) {
        reset();
        auto data = block();
        data.inputs["aTimestep"].number = value;
        rejected(data);
    }
    for (double time : {0.0, -.5}) {
        reset();
        auto data = block();
        data.inputs["aTimestep"].number = time;
        assert(compute(data) && pointOutput.points[0][0] == float(time));
    }
    for (bool doubles : {false, true})
        for (int mode : {0, 1, 2}) {
            reset();
            if (doubles)
                grids<openvdb::Vec3DGrid>();
            auto data = block(mode);
            pointInput.points[1][0] = 3e9f;
            rejected(data); // finite float, unsafe native Coord
            reset();
            if (doubles)
                grids<openvdb::Vec3DGrid>();
            data = block(mode);
            const auto grid =
                std::const_pointer_cast<Grid>(mode == 1 ? cptData.grids[0] : velocityData.grids[0]);
            grid->xform.inverseShift[0] = 3e9;
            rejected(data);
            reset();
            if (doubles)
                grids<openvdb::Vec3DGrid>();
            data = block(mode);
            std::const_pointer_cast<Grid>(mode == 1 ? cptData.grids[0] : velocityData.grids[0])
                ->xform.linear = false;
            rejected(data);
        }
    for (bool cpt : {false, true})
        for (bool background : {false, true}) {
            reset();
            auto data = block(2);
            auto grid = std::const_pointer_cast<openvdb::Vec3SGrid>(
                std::dynamic_pointer_cast<const openvdb::Vec3SGrid>(
                    (cpt ? cptData : velocityData).grids[0]));
            if (background)
                grid->bg[0] = NAN;
            else
                grid->values[0][1] = INFINITY;
            rejected(data);
        }
    // RK stages can leave the sampling domain while the starting point is valid.
    reset();
    auto envelope = block();
    envelope.inputs["aTimestep"].number = 1e9;
    rejected(envelope);
    // A later native result must be checked before another native query or narrowing.
    for (bool project : {false, true})
        for (double bad : {double(NAN), double(INFINITY), 1e100, 3e9}) {
            reset();
            grids<openvdb::Vec3DGrid>();
            auto data = block(project ? 1 : 0, false);
            data.inputs["aSubsteps"].integer = 2;
            data.inputs["aIterations"].integer = 2;
            corruptValue = bad;
            if (project)
                corruptProjection = 1;
            else
                corruptStep = 1;
            rejected(data, false);
            assert(nativeSteps + projectionSteps == 1);
        }
    reset();
    auto constrained = block(2, false);
    corruptStep = 1;
    corruptValue = 3e9;
    rejected(constrained, false);
    assert(projectionSteps == 0);
    // Large CPT world positions need not meet a velocity squared-length bound.
    reset();
    auto largeCpt = block(1, false);
    std::const_pointer_cast<openvdb::Vec3SGrid>(
        std::dynamic_pointer_cast<const openvdb::Vec3SGrid>(cptData.grids[0]))
        ->bg[0] = 1e20f;
    assert(compute(largeCpt) && pointOutput.points[0][0] == 1e20f);
    reset();
    auto badCurve = block();
    invalidCurve = true;
    rejected(badCurve, false);
    reset();
    auto missing = block();
    missingOutput = true;
    rejected(missing, false);
    // Empty point sets do not launch a million empty parallel invocations.
    reset();
    pointInput = DPData();
    auto empty = block();
    empty.inputs["aSubsteps"].integer = 999999;
    assert(compute(empty) && nativeSteps == 0 && curveBatches == 0);
    // Count, metadata and generated-curve storage all fail before a deep copy.
    reset();
    pointInput.reportedCount = 10000001;
    auto count = block();
    rejected(count);
    reset();
    pointInput.groups["selected"]->ids.pop_back();
    auto group = block();
    rejected(group);
    reset();
    auto memory = block();
    memory.inputs["aSubsteps"].integer = 900000;
    rejected(memory);
    reset();
    auto work = block(2, false);
    work.inputs["aSubsteps"].integer = 10000;
    work.inputs["aIterations"].integer = 10000;
    rejected(work);
    // First selected auxiliary grid remains the compatibility rule.
    reset();
    auto first = block();
    velocityData.grids.push_back(std::make_shared<openvdb::Vec3SGrid>());
    assert(compute(first) && pointOutput.points[0][0] == .5f);
    reset();
    auto unmatched = block();
    unmatched.inputs["aVdbVelocityGridNames"].text = "absent";
    rejected(unmatched);
    std::cout << "Point advection bodies: modes, numeric domains, budgets, transactional outputs "
                 "and injected failures passed\n";
}
