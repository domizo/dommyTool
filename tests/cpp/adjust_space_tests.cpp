#include "adjust_space_doubles.h"
#include "adjust_space.inc"

namespace {
void setup() {
    failAt = throwAt = -1;
    callCount = copies = publications = cleans = creations = bulkReads = writes = 0;
    trace.clear(); statusSites.clear();
    missingInput = nullOutput = shortRemap = false;
    radius = 1.0; retained = 100.0; iterations = 1; uniform = true;
    inputData = DPData(); stagedData = DPData(); publishedData = DPData();
    inputData.points = {{0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}};
    inputData.attributes["ids"] = std::make_shared<TypedDPAttributeData<int32_t>>(
        std::vector<int32_t>{10, 20}, -1);
    inputData.groups["selected"] = std::make_shared<Group>();
    inputData.groups.at("selected")->ids = {true, false};
    publishedData.points = {{99.0f, 99.0f, 99.0f}};
}

MStatus compute() {
    DPAdjustSpace node; MDataBlock block;
    return node.compute(MPlug(DPAdjustSpace::outDP), block);
}
void unchanged() {
    assert(publications == 0 && cleans == 0 && publishedData.points.size() == 1U);
    assert(publishedData.points[0][0] == 99.0f);
}
void noStaging() { unchanged(); assert(copies == 0 && creations == 0 && writes == 0); }

template<class Function> bool rejects(Function function) {
    try { function(); } catch (const std::exception&) { return true; }
    return false;
}

void quantization() {
    using namespace dommy::geometry;
    using regularizer_detail::quantize;
    long long result = 42;
    assert(quantize(-0.25, 1.0, result) && result == -1);
    assert(quantize(2.25, 2.0, result) && result == 4);
    const double edge = std::ldexp(1.0, 63);
    for (double value : {edge, -edge, std::nextafter(edge, INFINITY),
                         std::nextafter(-edge, -INFINITY), double(NAN), double(INFINITY)}) {
        result = 42;
        assert(!quantize(value, 1.0, result) && result == 42);
    }
    result = 42;
    assert(!quantize(std::numeric_limits<double>::max(), 2.0, result) && result == 42);
    for (double value : {std::nextafter(edge, 0.0), std::nextafter(-edge, 0.0)}) {
        assert(quantize(value, 1.0, result));
        assert(result >= std::numeric_limits<long long>::min() + 2LL);
        assert(result <= std::numeric_limits<long long>::max() - 2LL);
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            Point3 point; point[axis] = value;
            regularizer_detail::SpatialGrid grid;
            dommy::generation::WorkBudget budget(128U);
            assert(grid.build({point}, 1.0, budget));
            std::size_t visits = 0U;
            assert(grid.visitNeighbors(point, budget, [&](std::size_t id) {
                assert(id == 0U); ++visits; return true;
            }));
            assert(visits == 1U && budget.used() == 115U);
        }
    }
    for (double value : {edge, -edge}) {
        RegularizedPointSet output;
        output.points = {Point3(7, 8, 9)};
        output.sourceIndices = {99U};
        assert(!regularizePointSet({Point3(value, 0, 0)}, 100, 1, 1, true,
                                   output, 10, 1000));
        assert(output.points.size() == 1 && output.points[0].x == 7);
        assert(output.sourceIndices == std::vector<std::size_t>{99U});
    }
    // Bounded work rejection also preserves the previous independent result.
    RegularizedPointSet output; output.points = {Point3(7, 8, 9)};
    assert(!regularizePointSet({Point3(), Point3(0.5, 0, 0)}, 100, 1, 1, true,
                               output, 10, 1));
    assert(output.points.size() == 1 && output.points[0].x == 7);
    std::cout << "Regularizer: checked integer extrema, neighbor headroom and rollback passed ("
              << std::numeric_limits<long double>::digits << "-bit long double)\n";
}

void ordinary() {
    for (bool mode : {false, true}) for (double percent : {0.0, 50.0, 100.0}) {
        setup(); uniform = mode; retained = percent;
        assert(compute() == MS::kSuccess && publications == 1 && cleans == 1);
        const std::size_t count = static_cast<std::size_t>(percent / 50.0);
        assert(publishedData.points.size() == count);
        if (count) {
            const double expected = count == 1 ? (mode ? 0.025 : 0.035) : (mode ? -0.055 : 0.0);
            assert(std::fabs(publishedData.points[0][0] - expected) < 1e-7);
            assert(publishedData.points[0][1] == 0.0f && publishedData.points[0][2] == 0.0f);
        }
        if (count == 2) assert(std::fabs(publishedData.points[1][0] - (mode ? 0.555 : 0.5)) < 1e-7);
        const auto ids = std::dynamic_pointer_cast<const TypedDPAttributeData<int32_t>>(
            publishedData.attributes.at("ids"));
        assert(ids && ids->value().size() == count && ids->getDefaultValue() == -1);
        for (std::size_t i = 0; i < count; ++i)
            assert(ids->value()[i] == static_cast<int32_t>(10U * (i + 1U)));
        assert(publishedData.groups.at("selected")->ids.size() == count);
        assert(inputData.points[0][0] == 0.0f && inputData.points[1][0] == 0.5f);
        assert(inputData.attributes.at("ids")->attrSize() == 2U);
        assert(inputData.groups.at("selected")->ids.size() == 2U);
    }
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        if (mode == 0) radius = 0.0;
        if (mode == 1) iterations = 0;
        if (mode == 2) { inputData.points.clear(); inputData.attributes.clear(); inputData.groups.clear(); }
        assert(compute() && publications == 1 && writes == 0 && bulkReads == 0);
        assert(publishedData.points.size() == inputData.points.size());
    }
}

std::vector<std::size_t> selectionReference(const std::vector<dommy::geometry::Point3>& points,
                                           std::size_t target) {
    using namespace dommy::geometry::regularizer_detail;
    std::map<CellKey, std::vector<std::size_t>> cells;
    for (std::size_t i = 0; i < points.size(); ++i) {
        CellKey key; assert(cellKey(points[i], 1.0, key));
        cells[key].push_back(i);
    }
    std::vector<std::size_t> selected;
    for (std::size_t layer = 0; selected.size() < target; ++layer) {
        std::vector<std::size_t> candidates;
        for (const auto& cell : cells)
            if (layer < cell.second.size()) candidates.push_back(cell.second[layer]);
        const std::size_t count = std::min(target - selected.size(), candidates.size());
        assert(count);
        for (std::size_t i = 0; i < count; ++i)
            selected.push_back(candidates[static_cast<std::size_t>(
                static_cast<long double>(i) * candidates.size() / count)]);
    }
    std::sort(selected.begin(), selected.end());
    return selected;
}

void boundedSelection() {
    using namespace dommy::geometry;
    using dommy::generation::WorkBudget;
    // Mixed cell sizes and interleaved source order exercise partial last
    // layers as well as removal of cells exhausted at different depths.
    std::vector<Point3> points;
    for (std::size_t i = 0; i < 80U; ++i)
        points.push_back(Point3(double((i * 17U) % 13U) * 3.0, 0, 0));
    for (std::size_t target = 1; target <= points.size(); ++target) {
        std::vector<std::size_t> selected;
        WorkBudget budget;
        assert(regularizer_detail::selectPoints(points, target, 1, true, selected, budget));
        assert(selected == selectionReference(points, target));
    }
    std::size_t previous = 0U;
    for (std::size_t count : {6000U, 12000U, 24000U}) {
        const std::size_t sparse = count / 3U, target = count * 99U / 100U;
        points.assign(count - sparse, Point3());
        for (std::size_t i = 0; i < sparse; ++i) points.push_back(Point3(double((i+1U)*3U), 0, 0));
        std::vector<std::size_t> selected;
        WorkBudget budget(count * 100U);
        assert(regularizer_detail::selectPoints(points, target, 1, true, selected, budget));
        assert(selected.size() == target && budget.used() < count * 100U);
        if (previous) assert(double(budget.used()) / previous < 2.5);
        previous = budget.used();
        for (std::size_t i = 0; i < target; ++i)
            assert(selected[i] == (i < target - sparse ? i : i + count - target));
        const auto before = selected;
        WorkBudget tiny(1U);
        assert(!regularizer_detail::selectPoints(points, target, 1, true, selected, tiny));
        assert(tiny.exceeded() && selected == before);
        // The real node's memory admission accepts this fixture.
        DPData source; source.countOverride = count;
        preflightAdjustSpace(source, true, true);
        std::cout << "Biased uniform selection " << count << ": " << budget.used() << " bounded work units\n";
    }
}

bool sameRegularized(const dommy::geometry::RegularizedPointSet& a,
                     const dommy::geometry::RegularizedPointSet& b) {
    if (a.sourceIndices != b.sourceIndices || a.points.size() != b.points.size()) return false;
    for (std::size_t i = 0; i < a.points.size(); ++i)
        for (std::size_t axis = 0; axis < 3U; ++axis)
            if (a.points[i][axis] != b.points[i][axis]) return false;
    return true;
}

void exactWorkAndRollback() {
    using namespace dommy::geometry;
    using dommy::generation::WorkBudget;
    const std::vector<Point3> points{{0, 0, 0}, {.5, 0, 0}};
    RegularizedPointSet expected, before;
    before.points = {{7, 8, 9}}; before.sourceIndices = {99U};
    WorkBudget measured;
    assert(regularizePointSet(points, 50, 1, 1, true, expected, 10, measured));
    assert(measured.used() == 303U); // Freeze the staged selection/grid/comparison accounting.
    for (std::size_t limit = 0; limit < measured.used(); ++limit) {
        auto output = before;
        WorkBudget budget(limit);
        assert(!regularizePointSet(points, 50, 1, 1, true, output, 10, budget));
        assert(budget.exceeded() && sameRegularized(output, before));
        assert(!budget.consume(0U));
    }
    WorkBudget exact(measured.used());
    auto output = before;
    assert(regularizePointSet(points, 50, 1, 1, true, output, 10, exact));
    assert(exact.remaining() == 0 && sameRegularized(output, expected));
    assert(!regularizePointSet(points, 50, 1, 1, true, output, 10, exact));
    assert(exact.exceeded() && sameRegularized(output, expected));
    WorkBudget twoCalls(2U * measured.used());
    assert(regularizePointSet(points, 50, 1, 1, true, output, 10, twoCalls));
    assert(regularizePointSet(points, 50, 1, 1, true, output, 10, twoCalls));
    assert(twoCalls.remaining() == 0U);
    RegularizedPointSet compatibility;
    assert(regularizePointSet(points, 50, 1, 1, true, compatibility, 10, measured.used()));
    assert(sameRegularized(compatibility, expected));
    std::cout << "Regularizer exact work: " << measured.used() << "; exhausted stages preserve both outputs\n";
}

void nodeWorkBudget() {
    assert(dommy::generation::WorkBudget().remaining() == 1024U);
    setup(); assert(compute() && publications == 1 && cleans == 1);
    const auto previous = publishedData;
    setup(); inputData.points.assign(100U, DPoint(0, 0, 0));
    inputData.attributes.clear(); inputData.groups.clear(); publishedData = previous;
    preflightAdjustSpace(inputData, true, true);
    assert(!compute());
    assert(publications == 0 && cleans == 0 && copies == 0 && creations == 0 && writes == 0);
    assert(publishedData.points.size() == previous.points.size());
    for (std::size_t i = 0; i < previous.points.size(); ++i)
        for (std::size_t axis = 0; axis < 3U; ++axis)
            assert(publishedData.points[i][axis] == previous.points[i][axis]);
    assert(inputData.points.size() == 100U && inputData.points[0][0] == 0);
    setup(); assert(compute() && publications == 1 && cleans == 1);
    std::cout << "Node-wide regularization work failure preserves output and permits retry\n";
}

void budgets() {
    DPData synthetic;
    std::size_t low = 1U, high = dommy::generation::kMaxGeneratedPointCount;
    // Synthetic cardinalities exercise exact admission edges without allocating
    // those point arrays or spatial maps. Typed attribute/group checks are real.
    while (low < high) {
        const std::size_t middle = low + (high - low + 1U) / 2U;
        synthetic.countOverride = middle;
        if (rejects([&] { preflightAdjustSpace(synthetic, true, true); })) high = middle - 1U;
        else low = middle;
    }
    assert(low > 1000 && low < dommy::generation::kMaxGeneratedPointCount);
    // Independent accounting oracle, not just a boundary learned from the
    // function under test. Keep every retained array and map visible here.
    const std::size_t cellBytes = 3U * sizeof(long long) +
        sizeof(std::vector<std::size_t>) + sizeof(std::size_t) + 4U * sizeof(void*);
    const std::size_t perPoint = 2U * sizeof(DPoint) + 9U * sizeof(double) +
        4U * sizeof(std::size_t) + sizeof(bool) + 3U * cellBytes +
        sizeof(std::vector<std::size_t>);
    const std::size_t neighbors = 256U *
        sizeof(dommy::geometry::regularizer_detail::WeightedNeighbor);
    const std::size_t expected = (64U * 1024U * 1024U - neighbors) / perPoint;
    assert(low == expected);
    synthetic.countOverride = low;
    preflightAdjustSpace(synthetic, true, true);
    synthetic.countOverride = low + 1U;
    assert(rejects([&] { preflightAdjustSpace(synthetic, true, true); }));
    preflightAdjustSpace(synthetic, true, false); // Uniform selection scratch matters.

    synthetic.countOverride = low;
    synthetic.attributes["flag"] = std::make_shared<TypedDPAttributeData<bool>>(
        std::vector<bool>(low, true), false);
    assert(rejects([&] { preflightAdjustSpace(synthetic, true, true); }));
    synthetic.attributes.clear();
    synthetic.groups["group"] = std::make_shared<Group>();
    synthetic.groups.at("group")->ids.resize(low);
    assert(rejects([&] { preflightAdjustSpace(synthetic, true, true); }));
    synthetic.groups.clear();

    const std::size_t passCount = (64U * 1024U * 1024U) / sizeof(DPoint);
    synthetic.countOverride = passCount;
    preflightAdjustSpace(synthetic, false, true);
    synthetic.countOverride = passCount + 1U;
    assert(rejects([&] { preflightAdjustSpace(synthetic, false, true); }));
    for (bool active : {false, true}) {
        setup(); if (!active) radius = 0;
        inputData.attributes.clear(); inputData.groups.clear();
        inputData.countOverride = active ? low + 1U : passCount + 1U;
        assert(!compute()); noStaging(); assert(bulkReads == 0);
    }
    std::cout << "DPAdjustSpace: exact admission edge, real metadata/groups and pass-through budget passed\n";
}

void invalid() {
    for (int mode = 0; mode < 14; ++mode) {
        setup();
        switch (mode) {
        case 0: radius = -1; break;
        case 1: radius = NAN; break;
        case 2: radius = INFINITY; break;
        case 3: retained = -1; break;
        case 4: retained = 101; break;
        case 5: retained = NAN; break;
        case 6: iterations = -1; break;
        case 7: iterations = 101; break;
        case 8: missingInput = true; break;
        case 9: inputData.valid = false; break;
        case 10: inputData.points[0][0] = NAN; break;
        case 11: inputData.groups.at("selected")->ids.pop_back(); break;
        case 12: inputData.attributes.at("ids").reset(); break;
        case 13: inputData.countOverride = dommy::generation::kMaxGeneratedPointCount + 1U; break;
        }
        assert(!compute()); noStaging(); assert(bulkReads == 0);
    }
    for (double value : {std::ldexp(1.0, 63), -std::ldexp(1.0, 63)}) {
        setup(); inputData.points[0][0] = static_cast<float>(value);
        assert(!compute()); noStaging();
    }
    for (unsigned int axis = 0; axis < 3; ++axis) for (float sign : {-1.0f, 1.0f}) {
        setup(); const float maximum = std::numeric_limits<float>::max();
        inputData.points = {{0, 0, 0}, {0, 0, 0}};
        inputData.points[0][axis] = maximum * sign;
        inputData.points[1][axis] = (maximum / 2.0f) * sign;
        radius = maximum;
        assert(compute() == MS::kInvalidParameter); noStaging();
    }
    setup(); nullOutput = true; assert(!compute()); unchanged(); assert(copies == 0);
    setup(); shortRemap = true; assert(!compute()); unchanged(); assert(writes == 0);
    setup(); DPAdjustSpace node; MDataBlock block;
    assert(node.compute(MPlug(MObject{"other"}), block) == MS::kUnknownParameter && callCount == 0);
}

void failures(bool active) {
    setup(); if (!active) radius = 0;
    assert(compute());
    const auto baseline = trace;
    const auto sites = statusSites;
    std::size_t tested = 0;
    for (std::size_t at = 0; at < baseline.size(); ++at) {
        for (int mode = 0; mode < 3; ++mode) {
            if (mode < 2 && !sites[at]) continue;
            setup(); if (!active) radius = 0;
            injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
            if (mode == 2) throwAt = static_cast<int>(at); else failAt = static_cast<int>(at);
            const auto result = compute();
            assert(!result && callCount == static_cast<int>(at) + 1);
            assert(std::equal(trace.begin(), trace.end(), baseline.begin()));
            if (mode < 2 && baseline[at] != "data.remap")
                assert(result == injectedStatus);
            if (baseline[at] != "output.clean" && baseline[at] != "block.clean") unchanged();
            assert(inputData.points[0][0] == 0.0f && inputData.attributes.at("ids")->attrSize() == 2U);
            failAt = throwAt = -1; callCount = 0; publications = cleans = 0;
            assert(compute() && publications == 1 && cleans == 1);
            ++tested;
        }
    }
    std::cout << "DPAdjustSpace " << (active ? "regularize" : "pass-through") << ": "
              << tested << " status/exception cases with recovery passed\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--work-budget") { nodeWorkBudget(); return 0; }
    assert(argc == 1);
    quantization(); ordinary(); boundedSelection(); exactWorkAndRollback();
    budgets(); invalid(); failures(true); failures(false);
    std::cout << "DPAdjustSpace: ordinary values, metadata, bounded staging and checked float conversion passed\n";
}
