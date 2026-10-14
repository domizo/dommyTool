#include "vdb_vector_merge_doubles.h"
#include "vdb_vector_merge_compute.inc"

template <typename T> std::shared_ptr<T> grid(const std::string& name, int value) {
    auto result = std::make_shared<T>();
    result->name = name;
    result->sample = value;
    return result;
}
template <typename T = openvdb::FloatGrid> void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    missingInput = nullCreated = nullMerged = failedDispatch = false;
    publications = merges = invalidReads = 0;
    inputData.grids = {grid<T>("left.x", 1), grid<T>("middle.y", 2), grid<T>("right.z", 3),
                       grid<T>("left.x", 4), grid<T>("keep", 5)};
    stagedData.grids.clear();
    outputData.grids = {grid<T>("previous", 999)};
}
MDataBlock block() {
    MDataBlock data;
    data.inputs["aXGroup"].text = "@name=*.x";
    data.inputs["aYGroup"].text = "@name=*.y";
    data.inputs["aZGroup"].text = "@name=*.z";
    data.inputs["aMergedName"].text = "merged#";
    data.inputs["aUseXName"].number = 1;
    data.inputs["aRemoveSource"].number = 1;
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBVectorMerge().compute(MPlug(DOpenVDBVectorMerge::aVdbOutput), data);
}
void unchanged() {
    assert(inputData.grids[0]->getName() == "left.x");
    assert(inputData.grids[1]->getName() == "middle.y");
    assert(inputData.grids[2]->getName() == "right.z");
    assert(inputData.grids[3]->getName() == "left.x");
    assert(invalidReads == 0);
}
template <typename T> void validCases() {
    for (int remove = 0; remove < 2; ++remove)
        for (int inactive = 0; inactive < 2; ++inactive)
            for (int vectorType = 0; vectorType < openvdb::NUM_VEC_TYPES; ++vectorType) {
                setup<T>();
                auto data = block();
                data.inputs["aRemoveSource"].number = remove;
                data.inputs["aCopyInactiveValue"].number = inactive;
                data.inputs["aVectorType"].number = vectorType;
                assert(compute(data) && publications == 1 && merges == 1);
                assert(outputData.grids.size() == (remove ? 3U : 6U));
                assert(outputData.grids[0]->getName() == "left");
                assert(outputData.grids[0]->vectorType == vectorType);
                assert((outputData.grids[0]->values == std::array<long double, 3>{{1, 2, 3}}));
                assert(lastCopyInactive == bool(inactive));
                for (int i = 0; i < 3; ++i)
                    assert(mergedPointers[i] == inputData.grids[i].get());
                // A fourth, unselected grid has the same name as X and must survive.
                assert(outputData.grids[outputData.grids.size() - 2] == inputData.grids[3]);
                unchanged();
            }
}
void testSelections() {
    assert(containsGrid({}, "any", 7));
    assert(containsGrid({"", ""}, "any", 7));
    assert(containsGrid({"*"}, "any", 7));
    assert(containsGrid({"7"}, "other", 7));
    assert(!containsGrid({"7"}, "7", 0));
    assert(containsGrid({"@name=7"}, "7", 0));
    assert(!containsGrid({"@name=7"}, "other", 7));
    assert(containsGrid({"@name=*.x"}, "velocity.x", 0));
    assert(containsGrid({"*.x"}, "velocity.x", 0));
    assert(containsGrid({"missing", "@name=*.x"}, "velocity.x", 0));
    assert(containsGrid({"@name="}, "", 0));
    assert(!containsGrid({"@name="}, "velocity.x", 0));
    assert(!containsGrid({"@name=*.x"}, "velocity.y", 0));
    assert(!containsGrid({"missing"}, "velocity.x", 0));
    setup();
    auto data = block();
    data.inputs["aXGroup"].text = "  0  ";
    data.inputs["aYGroup"].text = "1";
    data.inputs["aZGroup"].text = "2";
    assert(compute(data) && outputData.grids.size() == 3);
    setup();
    data = block();
    data.inputs["aYGroup"].text = "missing";
    assert(compute(data) && !merges && outputData.grids == inputData.grids);
    setup();
    data = block();
    inputData.grids.clear();
    assert(compute(data) && outputData.grids.empty() && !merges);
    setup();
    data = block();
    for (auto attr : {"aXGroup", "aYGroup", "aZGroup"})
        data.inputs[attr].text = "0";
    assert(compute(data) && outputData.grids.size() == 5);
    assert((outputData.grids[0]->values == std::array<long double, 3>{{1, 1, 1}}));
    // Empty, duplicate names can be selected by index without deleting the fourth grid.
    setup();
    data = block();
    for (auto& value : inputData.grids)
        const_cast<Grid*>(value.get())->name.clear();
    data.inputs["aXGroup"].text = "0";
    data.inputs["aYGroup"].text = "1";
    data.inputs["aZGroup"].text = "2";
    assert(compute(data) && outputData.grids.size() == 3);
    assert(outputData.grids[0]->getName() == "merged1");
}
void testNames() {
    for (const std::string& pattern : {"", "plain", "merged#", "a###b##"}) {
        setup();
        auto data = block();
        data.inputs["aUseXName"].number = 0;
        data.inputs["aMergedName"].text = pattern;
        assert(compute(data));
        assert(outputData.grids[0]->getName() ==
               std::regex_replace(pattern, std::regex("#+"), "1"));
    }
}
void testRejections() {
    setup();
    auto malformed = block();
    inputData.grids.push_back(GridCPtr());
    assert(!compute(malformed) && !publications && !merges);
    for (int axis = 0; axis < 3; ++axis) {
        setup();
        auto data = block();
        inputData.grids[axis] = grid<openvdb::DoubleGrid>(inputData.grids[axis]->getName(), 1);
        assert(!compute(data) && !merges && !publications);
    }
    setup<openvdb::BoolGrid>();
    auto data = block();
    assert(!compute(data) && !merges && !publications);
    setup();
    data = block();
    auto forged = grid<openvdb::DoubleGrid>("middle.y", 2);
    forged->kind = DVdbGridType::kFloat;
    inputData.grids[1] = forged;
    // Even a stale external type tag must fail before the unchecked native cast.
    assert(!compute(data) && !publications);
    for (int mode : {-1, openvdb::NUM_VEC_TYPES, 30000}) {
        setup();
        data = block();
        data.inputs["aVectorType"].number = mode;
        assert(!compute(data) && !merges && !publications);
    }
    for (int bad = 0; bad < 4; ++bad) {
        setup();
        data = block();
        missingInput = bad == 0;
        nullCreated = bad == 1;
        nullMerged = bad == 2;
        failedDispatch = bad == 3;
        const MStatus result = compute(data);
        assert(!result && !publications);
        if (bad == 3)
            assert(result == MS::kInvalidParameter);
        assert(outputData.grids[0]->getName() == "previous");
        unchanged();
    }
    setup();
    data = block();
    assert(DOpenVDBVectorMerge().compute(MPlug({"unrelated"}), data) == MS::kUnknownParameter);
    assert(callCount == 0 && !publications);
    auto f = grid<openvdb::FloatGrid>("f", 1);
    auto d = grid<openvdb::DoubleGrid>("d", 1);
    assert(!validMergeInputs(nullptr, f.get(), f.get()));
    assert(!validMergeInputs(f.get(), nullptr, f.get()));
    assert(!validMergeInputs(f.get(), f.get(), nullptr));
    assert(checkedMergeTree<openvdb::FloatGrid>(nullptr) == nullptr);
    for (int axis = 0; axis < 3; ++axis) {
        const Grid* pointers[3] = {f.get(), f.get(), f.get()};
        pointers[axis] = d.get();
        bool caught = false;
        try {
            readMergeTrees<openvdb::FloatGrid>(pointers);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        assert(caught);
    }
}
void testFailures() {
    setup();
    auto data = block();
    assert(compute(data));
    const int count = callCount;
    const auto baseline = operations;
    int tested = 0;
    for (int mode = 0; mode < 3; ++mode)
        for (int i = 0; i < count; ++i) {
            setup();
            data = block();
            if (mode == 2)
                attributeThrowAt = i;
            else {
                failAt = i;
                injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
            }
            const MStatus result = compute(data);
            assert(!result);
            if (mode != 2 && i != 0 && baseline[i] != "native.merge" &&
                baseline[i] != "grid.name" && baseline[i] != "grid.vectorType" &&
                baseline[i] != "data.insert")
                assert(result == injectedStatus);
            assert(callCount == i + 1);
            assert(publications == (i == count - 1 ? 1 : 0));
            if (!publications)
                assert(outputData.grids[0]->getName() == "previous");
            unchanged();
            ++tested;
            resetCalls();
            assert(compute(data));
            unchanged();
        }
    std::cout << "VectorMerge: 80 typed/control cases, selection/naming/source identity, " << tested
              << " injected compute failures/exceptions with recovery passed\n";
}
int main() {
    validCases<openvdb::FloatGrid>();
    validCases<openvdb::DoubleGrid>();
    validCases<openvdb::Int32Grid>();
    validCases<openvdb::Int64Grid>();
    testSelections();
    testNames();
    testRejections();
    testFailures();
}
