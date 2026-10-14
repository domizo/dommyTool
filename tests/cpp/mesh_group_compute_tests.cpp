#include "mesh_group_compute_doubles.h"
#include "mesh_group_compute.inc"

static bool validGeneratedGroupBudget(const DMeshData& input,
                                     const std::vector<std::string>& names) {
    return dommy::group::validGeneratedGroupBudget(input, input.numberOfMesh(), names);
}

static std::string snapshot(const DMeshData& data) {
    std::string result;
    const auto append = [&](const Groups& groups) {
        for (const auto& group : groups) {
            result += group.first + ":";
            for (bool bit : group.second->ids) result += bit ? "1" : "0";
            result += ";";
        }
    };
    append(data.groups);
    for (const auto& mesh : data.meshes) {
        append(mesh->points); append(mesh->faces); append(mesh->edges);
    }
    return result;
}

static void fixture(std::size_t count = 3U) {
    resetCompute();
    groupInput = DMeshData();
    groupOutput = DMeshData();
    targetInput = DPData();
    nearestSelections.clear();
    distances.assign(count, 0.0);
    for (std::size_t i = 0U; i < count; ++i) {
        auto mesh = std::make_shared<GroupMesh>();
        mesh->center = {{double(i), 0, 0}};
        const auto mask = std::make_shared<GroupMask>(DPIdList(1U, true));
        mesh->points["remove"] = mesh->faces["remove"] = mesh->edges["remove"] = mask;
        mesh->points["keep"] = mesh->faces["keep"] = mesh->edges["keep"] = mask;
        groupInput.meshes.push_back(mesh);
        targetInput.points.push_back(mesh->center);
        nearestSelections.push_back(i);
    }
    if (count) {
        groupInput.groups["piece0"] = std::make_shared<GroupMask>(DPIdList(count, true));
        groupInput.groups["keep"] = std::make_shared<GroupMask>(DPIdList(count, true));
    }
}
static MDataBlock inputs(bool deletion) {
    MDataBlock data;
    data.inputs["aGroupName"].string = "piece";
    data.inputs["aDistance"].number = 1.0;
    data.inputs["aIsEnable"].number = 1.0;
    data.inputs["aPrimitiveGroupName"].string = "piece0 missing piece0";
    for (const auto& name : {"aPointGroupName", "aFaceGroupName", "aEdgeGroupName"})
        data.inputs[name].string = "  remove missing  remove ";
    (void)deletion;
    return data;
}
static MStatus compute(bool deletion, MDataBlock& data) {
    return deletion ? DMGroupDelete().compute(MPlug(DMGroupDelete::outDM), data)
                    : DMGroupFromDP().compute(MPlug(DMGroupFromDP::outDM), data);
}
static void semantics() {
    fixture();
    nearestSelections = {2U, 0U, 2U};
    auto data = inputs(false);
    const std::string before = snapshot(groupInput);
    assert(compute(false, data) && publications == 1 && additions == 2);
    assert(groupOutput.groups.at("piece0")->ids == DPIdList({false, true, false}));
    assert(groupOutput.groups.at("piece1")->ids == DPIdList({true, false, true}));
    assert(groupOutput.groups.count("keep") && snapshot(groupInput) == before);
    fixture();
    distances = {0.0, 1.0, 2.0};
    data = inputs(false);
    data.inputs["aUseDistance"].number = 1.0;
    assert(compute(false, data) && additions == 2); // inclusive distance boundary
    assert(!groupOutput.groups.count("piece2"));
    fixture();
    data = inputs(false);
    data.inputs["aGroupName"].string = u8"部品";
    assert(compute(false, data) && groupOutput.groups.count(u8"部品2"));
    fixture();
    data = inputs(false);
    data.inputs["aGroupName"].string = "";
    assert(compute(false, data) && groupOutput.groups.count("0"));
    fixture();
    data = inputs(true);
    const std::string original = snapshot(groupInput);
    assert(compute(true, data) && publications == 1);
    assert(!groupOutput.groups.count("piece0") && groupOutput.groups.count("keep"));
    for (const auto& mesh : groupOutput.meshes) {
        assert(mesh->points.size() == 1U && mesh->points.count("keep"));
        assert(mesh->faces.size() == 1U && mesh->faces.count("keep"));
        assert(mesh->edges.size() == 1U && mesh->edges.count("keep"));
    }
    assert(snapshot(groupInput) == original);
    fixture();
    data = inputs(true);
    data.inputs["aIsEnable"].number = 0.0;
    assert(compute(true, data) && snapshot(groupInput) == snapshot(groupOutput));
    assert(std::find(operations.begin(), operations.end(), "string.split") == operations.end());
    fixture();
    targetInput.points.clear();
    data = inputs(false);
    assert(compute(false, data) && snapshot(groupInput) == snapshot(groupOutput));
    for (bool deletion : {false, true}) {
        fixture(0U);
        data = inputs(deletion);
        assert(compute(deletion, data) && publications == 1 && additions == 0);
        resetCompute();
        const MStatus result = deletion ? DMGroupDelete().compute(MPlug(MObject{"other"}), data)
                                       : DMGroupFromDP().compute(MPlug(MObject{"other"}), data);
        assert(result == MS::kUnknownParameter && callCount == 0);
    }
}
static void budgets() {
    // Former quadratic allocation fails before output copying or adding masks.
    fixture(10001U);
    auto data = inputs(false);
    assert(!compute(false, data) && copies == 0 && additions == 0 && publications == 0);
    fixture(10000U);
    data = inputs(false);
    assert(!compute(false, data) && copies == 0 && additions == 0 && publications == 0);
    // A high mesh count remains legal when the actual grouping is sparse.
    fixture(10001U);
    std::fill(nearestSelections.begin(), nearestSelections.end(), 0U);
    data = inputs(false);
    assert(compute(false, data) && additions == 1);
    // Long repeated prefixes are rejected before constructing generated names.
    fixture(501U);
    data = inputs(false);
    data.inputs["aGroupName"].string = MString(std::string(250000U, 'x').c_str());
    rejectNameBuild = true;
    assert(!compute(false, data) && copies == 0 && additions == 0 && nameBuilds == 0);

    // Scalar geometry count and shared masks keep an exact 100M-item boundary
    // cheap. Attribute validation below is production, not a count-only double.
    fixture(1U);
    groupInput.reportedCount = 1000000U;
    groupInput.groups.clear();
    const auto mask = std::make_shared<GroupMask>(DPIdList(1000000U, false));
    for (unsigned int i = 0U; i < 99U; ++i)
        groupInput.groups["piece" + std::to_string(i)] = mask;
    assert(validGeneratedGroupBudget(groupInput, std::vector<std::string>{"piece0"}));
    assert(!validGeneratedGroupBudget(groupInput, std::vector<std::string>{"new"}));
    groupInput.attributes["attribute"] = std::make_shared<TypedDPAttributeData<bool>>(
        DPIdList(1000000U, false));
    assert(!validGeneratedGroupBudget(groupInput, std::vector<std::string>{"piece0"}));
    groupInput.groups.erase("piece98");
    assert(validGeneratedGroupBudget(groupInput, std::vector<std::string>{"piece0"}));
    groupInput.attributes["attribute"].reset();
    assert(!validGeneratedGroupBudget(groupInput, std::vector<std::string>{"piece0"}));

    fixture(1U);
    groupInput.reportedCount = 1000000U;
    groupInput.groups.clear();
    data = inputs(true);
    std::string names;
    for (int i = 0; i < 101; ++i) names += "remove ";
    data.inputs["aPointGroupName"].string = MString(names.c_str());
    assert(!compute(true, data) && copies == 0 && publications == 0);
}
static void malformed() {
    for (bool deletion : {false, true}) {
        for (int variant = 0; variant < 5; ++variant) {
            fixture();
            if (variant == 0) nullInput = true;
            if (variant == 1) groupInput.valid = false;
            if (variant == 2) nullOutput = true;
            if (variant == 3) badCopy = true;
            if (variant == 4) badFinal = true;
            auto data = inputs(deletion);
            const auto original = snapshot(groupInput);
            assert(!compute(deletion, data) && publications == 0);
            assert(snapshot(groupInput) == original);
        }
    }
    for (int variant = 0; variant < 9; ++variant) {
        fixture();
        auto data = inputs(false);
        if (variant == 0) nullTarget = true;
        if (variant == 1) targetInput.valid = false;
        if (variant == 2) badNearest = true;
        if (variant == 3) distances[0] = NAN;
        if (variant == 4) distances[0] = -1.0;
        if (variant == 5) groupInput.meshes[0]->center[0] = INFINITY;
        if (variant == 6) targetInput.points[0][1] = NAN;
        if (variant == 7) shortCenters = true;
        if (variant == 8) targetInput.reportedCount = targetInput.points.size() + 1U;
        assert(!compute(false, data) && publications == 0 && copies == 0);
    }
    for (double distance : {-1.0, double(NAN), double(INFINITY)}) {
        fixture();
        auto data = inputs(false);
        data.inputs["aDistance"].number = distance;
        assert(!compute(false, data) && publications == 0 && copies == 0);
    }
}
static void failures() {
    unsigned int tested = 0U;
    for (bool deletion : {false, true}) {
        fixture();
        auto data = inputs(deletion);
        assert(compute(deletion, data));
        const auto trace = operations;
        for (std::size_t i = 0U; i < trace.size(); ++i) {
            for (int kind = 0; kind < 3; ++kind) {
                fixture();
                data = inputs(deletion);
                const std::string before = snapshot(groupInput);
                if (kind == 2) throwAt = int(i);
                else {
                    failAt = int(i);
                    injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(!compute(deletion, data));
                assert(callCount == int(i) + 1 && !invalidRead);
                assert(snapshot(groupInput) == before);
                if (trace[i] != "output.clean") assert(publications == 0);
                ++tested;
                fixture();
                data = inputs(deletion);
                assert(compute(deletion, data));
            }
        }
    }
    std::cout << "Mesh group compute: budgets, replacement, deletion and input immutability; "
              << tested << " status/exception injections passed\n";
}
int main() {
    semantics();
    budgets();
    malformed();
    failures();
}
