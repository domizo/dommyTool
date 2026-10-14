#include "vdb_fill_doubles.h"
#include "vdb_fill_compute.inc"

const float infinity = std::numeric_limits<float>::infinity();
const float notNumber = std::numeric_limits<float>::quiet_NaN();
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
    publications = gridCopies = invalidReads = fillCalls = 0;
    nullCopy = invalidNativeOutput = wrongCopy = nullOutput = invalidOutputPayload = false;
    meshConnected = meshValid = true;
    meshBounds = MBoundingBox();
    meshBounds.low.x = meshBounds.low.y = meshBounds.low.z = -1;
    meshBounds.high.x = meshBounds.high.y = meshBounds.high.z = 1;
    lateFillFailure = -1;
}
MDataBlock block(int mode = 0) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "source";
    data.inputs["aBounds"].integer = mode;
    data.inputs["aActive"].integer = data.inputs["aSparse"].integer = 1;
    for (int axis = 0; axis < 3; ++axis) {
        data.inputs["aMinCoord"].ints[axis] = -1;
        data.inputs["aMaxCoord"].ints[axis] = 1;
        data.inputs["aMinWorldCoord"].floats[axis] = -1;
        data.inputs["aMaxWorldCoord"].floats[axis] = 1;
        data.inputs["aValue"].floats[axis] = 2.75f + axis;
    }
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBFill().compute(MPlug(DOpenVDBFill::aVdbOutput), data);
}
void reject(MDataBlock& data, bool beforeCopy = true) {
    assert(!compute(data) && publications == 0 && invalidReads == 0);
    assert(outputData.grids.size() == 1 && outputData.grids[0]->name == "previous");
    if (beforeCopy)
        assert(gridCopies == 0 && fillCalls == 0);
}
template <typename T> void typedFill() {
    for (int sparse = 0; sparse < 2; ++sparse)
        for (int active = 0; active < 2; ++active) {
            setup();
            inputData.grids[0] = grid<T>("source");
            auto data = block();
            data.inputs["aSparse"].integer = sparse;
            data.inputs["aActive"].integer = active;
            assert(compute(data));
            assert(gridCopies == 1 && fillCalls == 1 && publications == 1);
            assert(outputData.grids.size() == 2 && outputData.grids[0]->name == "keep");
            const auto result = std::dynamic_pointer_cast<const T>(outputData.grids[1]);
            assert(result && result->filled && result->wasSparse == bool(sparse));
            assert(result->wasActive == bool(active) && !source<T>()->filled);
            typename T::ValueType expected;
            assert(FillValue<typename T::ValueType>::convert({2.75, 3.75, 4.75}, expected));
            assert(result->fillValue == expected);
            for (int axis = 0; axis < 3; ++axis)
                assert(result->fillBox.min()[axis] == -1 && result->fillBox.max()[axis] == 1);
        }
}
void valuesAndInputs() {
    typedFill<openvdb::FloatGrid>();
    typedFill<openvdb::DoubleGrid>();
    typedFill<openvdb::Int32Grid>();
    typedFill<openvdb::Int64Grid>();
    typedFill<openvdb::Vec3SGrid>();
    typedFill<openvdb::Vec3DGrid>();
    typedFill<openvdb::Vec3IGrid>();
    typedFill<openvdb::BoolGrid>();
    bool boolean = false;
    assert(FillValue<bool>::convert({-2, notNumber, infinity}, boolean) && boolean);
    assert(FillValue<bool>::convert({0, 1, 2}, boolean) && !boolean);
    assert(!FillValue<bool>::convert({infinity, 1, 2}, boolean));
    int integer = 0;
    assert(FillValue<int>::convert({-2.75, notNumber, infinity}, integer) && integer == -2);
    long long wide = 0;
    assert(!FillValue<long long>::convert({std::ldexp(1., 63), 0, 0}, wide));
    assert(FillValue<long long>::convert({-std::ldexp(1., 63), 0, 0}, wide));
    assert(wide == std::numeric_limits<long long>::min());
    for (short mode : {-1, 3, 99}) {
        setup();
        auto data = block(mode);
        reject(data);
    }
    for (int kind = 0; kind < 6; ++kind) {
        setup();
        auto data = block();
        if (kind == 0)
            data.inputs["aVdbInput"].type = 42;
        if (kind == 1)
            data.inputs["aVdbInput"].payload = nullptr;
        if (kind == 2)
            inputData.valid = false;
        if (kind == 3)
            inputData.grids[0] = nullptr;
        if (kind == 4)
            data.inputs["aVdbSelectedGridNames"].text = "missing";
        if (kind == 5)
            inputData.grids.clear();
        reject(data);
    }
    for (float value : {infinity, -infinity, notNumber, 3e9f}) {
        setup();
        inputData.grids[0] = grid<openvdb::Int32Grid>("source");
        auto data = block();
        data.inputs["aValue"].floats[0] = value;
        reject(data);
        setup();
        inputData.grids[0] = grid<openvdb::Vec3IGrid>("source");
        data = block();
        data.inputs["aValue"].floats[2] = value;
        reject(data);
    }
    setup();
    auto data = block();
    data.inputs["aValue"].floats[1] = notNumber;
    assert(compute(data));
    setup();
    data = block();
    source()->values[0] = infinity;
    reject(data);
    setup();
    data = block();
    source()->bg = notNumber;
    reject(data);
    setup();
    data = block();
    assert(DOpenVDBFill().compute(MPlug({"unrelated"}), data) == MS::kUnknownParameter);
    assert(callCount == 0 && publications == 0);
    setup();
    data = block();
    data.inputs["aVdbSelectedGridNames"].text = "*";
    assert(compute(data) && gridCopies == 2 && outputData.grids.size() == 2);
    assert(outputData.grids[0]->name == "source" && outputData.grids[1]->name == "keep");
}
void boundsAndTransforms() {
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        auto data = block(mode);
        assert(compute(data));
        setup();
        data = block(mode);
        data.inputs["aMinCoord"].ints[0] = 2;
        data.inputs["aMinWorldCoord"].floats[0] = 2;
        meshBounds.low.x = 2;
        reject(data);
    }
    for (int kind = 0; kind < 8; ++kind) {
        setup();
        auto data = block(1);
        if (kind == 0)
            source()->xform.linear = false;
        if (kind == 1)
            source()->dx[0] = 0;
        if (kind == 2)
            source()->dx[0] = infinity;
        if (kind == 3)
            source()->xform.inverseShift[0] = 3e9;
        if (kind == 4)
            source()->xform.inverseShift[0] = infinity;
        if (kind == 5)
            source()->xform.inverse[0][1] = notNumber;
        if (kind == 6)
            data.inputs["aMinWorldCoord"].floats[0] = notNumber;
        if (kind == 7)
            data.inputs["aMaxWorldCoord"].floats[0] = infinity;
        reject(data);
    }
    for (int kind = 0; kind < 3; ++kind) {
        setup();
        auto data = block(2);
        if (kind == 0)
            meshConnected = false;
        if (kind == 1)
            meshValid = false;
        if (kind == 2)
            meshBounds.low.z = notNumber;
        reject(data);
    }
    setup();
    auto data = block(0);
    source()->xform.linear = false;
    data.inputs.erase("aMinWorldCoord");
    data.inputs.erase("aMaxWorldCoord");
    assert(compute(data));
    setup();
    data = block(1);
    source()->xform.inverse[0][0] = 0;
    source()->xform.inverse[0][1] = -1;
    source()->xform.inverse[1][0] = 1;
    source()->xform.inverse[1][1] = 0;
    source()->xform.inverse[2][2] = .5;
    source()->xform.inverseShift[0] = 2;
    for (int axis = 0; axis < 3; ++axis) {
        data.inputs["aMinWorldCoord"].floats[axis] = -1.2f - axis;
        data.inputs["aMaxWorldCoord"].floats[axis] = 1.2f + axis;
    }
    assert(compute(data));
    auto result = std::dynamic_pointer_cast<const openvdb::FloatGrid>(outputData.grids[1]);
    assert(result->fillBox.min()[0] == -1 && result->fillBox.max()[0] == 5);
    assert(result->fillBox.min()[1] == -2 && result->fillBox.max()[1] == 2);
    assert(result->fillBox.min()[2] == -2 && result->fillBox.max()[2] == 2);
    setup();
    data = block(1);
    source()->xform.inverse[0][1] = -2;
    assert(compute(data));
    result = std::dynamic_pointer_cast<const openvdb::FloatGrid>(outputData.grids[1]);
    assert(result->fillBox.min()[0] == -3 && result->fillBox.max()[0] == 3);
    setup();
    data = block(2);
    meshBounds = MBoundingBox();
    assert(compute(data));
    result = std::dynamic_pointer_cast<const openvdb::FloatGrid>(outputData.grids[1]);
    assert(result->fillBox.min()[0] == 0 && result->fillBox.max()[0] == 0);
    for (int coordinate : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        setup();
        data = block();
        data.inputs["aMinCoord"].ints[0] = data.inputs["aMaxCoord"].ints[0] = coordinate;
        reject(data);
    }
    for (int coordinate :
         {std::numeric_limits<int>::min() + 4096, std::numeric_limits<int>::max() - 4096}) {
        setup();
        data = block();
        data.inputs["aMinCoord"].ints[0] = data.inputs["aMaxCoord"].ints[0] = coordinate;
        assert(compute(data));
    }
}
void budgetsAndTransactions() {
    std::size_t count = 0;
    openvdb::CoordBBox box;
    box.reset({-8, -8, -8}, {-1, -1, -1});
    assert(treeBlockCount(box, 8, count) && count == 1);
    box.reset({-1, -1, -1}, {0, 0, 0});
    assert(treeBlockCount(box, 8, count) && count == 8);
    assert(!treeBlockCount(box, 0, count));
    FillBudget budget;
    assert(!budget.addBytes(std::numeric_limits<std::size_t>::max(), 2));
    assert(!budget.addWork(std::numeric_limits<std::size_t>::max(), 2));
    budget.work = dommy::generation::kMaxWorkItemCount;
    assert(!chargeTreeNode<openvdb::FillLeaf<float>>(box, budget));
    for (int sparse = 0; sparse < 2; ++sparse) {
        setup();
        auto data = block();
        data.inputs["aSparse"].integer = sparse;
        for (int axis = 0; axis < 3; ++axis) {
            data.inputs["aMinCoord"].ints[axis] = 0;
            data.inputs["aMaxCoord"].ints[axis] = 10000;
        }
        reject(data);
    }
    setup();
    auto data = block();
    source()->memory = dommy::vdb::kMaxVdbPayloadBytes / 4;
    reject(data);
    setup();
    data = block();
    data.inputs["aVdbSelectedGridNames"].text = "*";
    inputData.grids.clear();
    for (int i = 0; i < 40; ++i)
        inputData.grids.push_back(grid("source"));
    reject(data);
    setup();
    data = block();
    data.inputs["aVdbSelectedGridNames"].text = "*";
    inputData.grids[1] = grid<openvdb::Int32Grid>("last");
    data.inputs["aValue"].floats[0] = 3e9f;
    reject(data);
    for (int kind = 0; kind < 5; ++kind) {
        setup();
        data = block();
        if (kind == 0)
            nullCopy = true;
        if (kind == 1)
            wrongCopy = true;
        if (kind == 2)
            invalidNativeOutput = true;
        if (kind == 3)
            nullOutput = true;
        if (kind == 4)
            invalidOutputPayload = true;
        reject(data, false);
        assert(!source()->filled);
    }
    setup();
    data = block();
    inputData.grids[1] = grid("last");
    data.inputs["aVdbSelectedGridNames"].text = "*";
    lateFillFailure = 1;
    reject(data, false);
    assert(fillCalls == 2 && gridCopies == 2 && !source()->filled);
}
void failures() {
    int tested = 0;
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        auto data = block(mode);
        assert(compute(data));
        const int calls = callCount;
        for (int failure = 0; failure < calls; ++failure)
            for (int kind = 0; kind < 3; ++kind) {
                setup();
                data = block(mode);
                if (kind == 2)
                    attributeThrowAt = failure;
                else {
                    failAt = failure;
                    injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(!compute(data) && invalidReads == 0 && callCount == failure + 1);
                assert(!source()->filled);
                if (failure == calls - 1)
                    assert(publications == 1);
                else
                    assert(publications == 0 && outputData.grids[0]->name == "previous");
                ++tested;
            }
    }
    std::cout << "Fill compute: " << tested << " injected statuses/exceptions passed\n";
}
int main() {
    valuesAndInputs();
    boundsAndTransforms();
    budgetsAndTransactions();
    failures();
    std::cout << "Fill: 8 types, numeric/coordinate limits, aggregate budgets, copy-before-write "
                 "and publication passed\n";
}
