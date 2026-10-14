#include "vdb_clip_doubles.h"
#include "vdb_clip_compute.inc"

const double infinity = std::numeric_limits<double>::infinity();
const double notNumber = std::numeric_limits<double>::quiet_NaN();
static DOpenVDBData maskData;
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> grid(const std::string& name) {
    auto result = std::make_shared<T>();
    result->name = name;
    return result;
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> source() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(inputData.grids[0]));
}
std::shared_ptr<openvdb::FloatGrid> maskSource() {
    return std::dynamic_pointer_cast<openvdb::FloatGrid>(
        std::const_pointer_cast<Grid>(maskData.grids[0]));
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
    publications = gridCopies = invalidReads = clipCalls = maskCalls = 0;
    nullClip = wrongClip = aliasClip = badClipValue = nullCopy = nullOutput = invalidOutputPayload =
        false;
    meshValid = true;
    failClipAt = -1;
    meshBounds = MBoundingBox();
    meshBounds.low.x = meshBounds.low.y = meshBounds.low.z = -1;
    meshBounds.high.x = meshBounds.high.y = meshBounds.high.z = 1;
    activeWorldBox = openvdb::BBoxd();
}
MDataBlock block(int mode = 2) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "source";
    data.inputs["aMaskVdbInput"].payload = &maskData;
    data.inputs["aMaskName"].text = "*";
    data.inputs["aClipType"].integer = mode;
    data.inputs["aClipAxis"].integer = 1;
    data.inputs["aClipPosition"].number = .5f;
    data.inputs["aClipWidth"].number = 1;
    data.inputs["aKeepInside"].integer = 1;
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBClip().compute(MPlug(DOpenVDBClip::aVdbOutput), data);
}
void reject(MDataBlock& data, bool beforeNative = true) {
    assert(!compute(data) && publications == 0 && invalidReads == 0);
    assert(outputData.grids.size() == 1 && outputData.grids[0]->name == "previous");
    if (beforeNative)
        assert(clipCalls == 0 && maskCalls == 0);
}
template <typename T> void typedModes() {
    for (int mode = 0; mode < 4; ++mode)
        for (int inside = 0; inside < 2; ++inside) {
            setup();
            inputData.grids[0] = grid<T>("source");
            auto data = block(mode);
            data.inputs["aKeepInside"].integer = inside;
            assert(compute(data) && publications == 1 && clipCalls == 1 &&
                   lastInside == bool(inside));
            assert(outputData.grids.size() == 2 && outputData.grids[0]->name == "keep");
            assert(outputData.grids[1]->name == "source_cliped" && source<T>()->name == "source");
            assert(outputData.grids[1].get() != source<T>().get());
        }
}
void contracts() {
    openvdb::CoordBBox planned;
    assert(indexGridBox(openvdb::BBoxd(openvdb::Vec3d(-.2), openvdb::Vec3d(.9)), planned));
    assert(planned.min()[0] == -1 && planned.max()[0] == 0);
    typedModes<openvdb::FloatGrid>();
    typedModes<openvdb::DoubleGrid>();
    typedModes<openvdb::Int32Grid>();
    typedModes<openvdb::Int64Grid>();
    typedModes<openvdb::BoolGrid>();
    typedModes<openvdb::Vec3SGrid>();
    typedModes<openvdb::Vec3DGrid>();
    typedModes<openvdb::Vec3IGrid>();
    setup();
    auto data = block();
    data.inputs["aVdbSelectedGridNames"].text = "missing";
    data.inputs.erase("aClipType");
    assert(compute(data) && clipCalls == 0 && outputData.grids.size() == 2);
    setup();
    data = block();
    source()->vacant = true;
    data.inputs.erase("aClipType");
    assert(compute(data) && clipCalls == 0 && outputData.grids[1]->name == "source");
    setup();
    data = block();
    auto empty = grid("empty");
    empty->vacant = true;
    inputData.grids.push_back(empty);
    inputData.grids.push_back(grid<Grid>("unsupported"));
    data.inputs["aVdbSelectedGridNames"].text = "source empty unsupported";
    assert(compute(data));
    const char* expected[] = {"keep", "empty", "source_cliped", "unsupported"};
    assert(outputData.grids.size() == 4);
    for (int i = 0; i < 4; ++i)
        assert(outputData.grids[i]->name == expected[i]);
    setup();
    data = block();
    meshBounds.low.x = -.2;
    meshBounds.high.x = .9;
    assert(compute(data));
    assert(lastClipBox.min()[0] == -1 && lastClipBox.max()[0] == 0);
    setup();
    data = block(0);
    data.inputs.erase("aClipWidth");
    data.inputs.erase("aMaskVdbInput");
    assert(compute(data) && lastClipBox.min()[1] == 0 && lastClipBox.max()[1] == 3);
    setup();
    data = block(1);
    data.inputs["aClipAxis"].integer = 2;
    assert(compute(data));
    assert(lastClipBox.min()[2] == -1 && lastClipBox.max()[2] == 0);
    setup();
    data = block(2);
    data.inputs.erase("aClipAxis");
    data.inputs.erase("aClipPosition");
    data.inputs.erase("aClipWidth");
    assert(compute(data));
    setup();
    data = block(3);
    maskSource()->kind = openvdb::GRID_LEVEL_SET;
    assert(compute(data) && maskCalls == 1);
    setup();
    data = block(3);
    maskData.grids.insert(maskData.grids.begin(), grid<Grid>("unsupported"));
    assert(compute(data) && clipCalls == 0 && outputData.grids[1]->name == "source");
    setup();
    data = block(3);
    auto unsupported = grid<Grid>("unsupported");
    unsupported->kind = openvdb::GRID_LEVEL_SET;
    maskData.grids.insert(maskData.grids.begin(), unsupported);
    reject(data);
    setup();
    data = block();
    assert(DOpenVDBClip().compute(MPlug({"unknown"}), data) == MS::kUnknownParameter);
    assert(callCount == 0);
}
void invalidInputs() {
    for (int mode : {-1, 4, 99}) {
        setup();
        auto data = block(mode);
        reject(data);
    }
    for (int kind = 0; kind < 10; ++kind) {
        setup();
        auto data = block(kind >= 6 ? 3 : 2);
        if (kind == 0)
            data.inputs["aVdbInput"].type = 42;
        if (kind == 1)
            data.inputs["aVdbInput"].payload = nullptr;
        if (kind == 2)
            inputData.valid = false;
        if (kind == 3)
            inputData.grids[0] = nullptr;
        if (kind == 4)
            meshValid = false;
        if (kind == 5)
            meshBounds.low.x = 2;
        if (kind == 6)
            maskData.valid = false;
        if (kind == 7)
            data.inputs["aMaskVdbInput"].payload = nullptr;
        if (kind == 8)
            data.inputs["aMaskName"].text = "missing";
        if (kind == 9) {
            maskSource()->kind = openvdb::GRID_LEVEL_SET;
            maskSource()->bg = 0;
        }
        reject(data);
    }
    for (int axis : {-1, 3}) {
        setup();
        auto data = block(0);
        data.inputs["aClipAxis"].integer = axis;
        reject(data);
    }
    for (float value : {-.1f, 1.1f, float(infinity), float(notNumber)}) {
        setup();
        auto data = block(0);
        data.inputs["aClipPosition"].number = value;
        reject(data);
    }
    for (float value : {0.f, -1.f, float(infinity), float(notNumber)}) {
        setup();
        auto data = block(1);
        data.inputs["aClipWidth"].number = value;
        reject(data);
    }
    for (int kind = 0; kind < 7; ++kind) {
        setup();
        auto data = block();
        if (kind == 0)
            source()->values[0] = infinity;
        if (kind == 1)
            source()->bg = notNumber;
        if (kind == 2)
            source()->xform.linear = false;
        if (kind == 3)
            source()->dx[0] = 0;
        if (kind == 4)
            source()->dx[0] = infinity;
        if (kind == 5)
            source()->xform.inverseShift[0] = 3e9;
        if (kind == 6)
            source()->xform.inverse[0][0] = notNumber;
        reject(data);
    }
    setup();
    auto data = block(0);
    activeWorldBox.min()[0] = notNumber;
    reject(data);
    setup();
    data = block();
    meshBounds.high.z = infinity;
    reject(data);
    for (int coordinate : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        setup();
        data = block();
        meshBounds.low.x = meshBounds.high.x = coordinate;
        reject(data);
    }
    for (double scale : {1e-12, 1e12}) {
        setup();
        data = block(3);
        // Keep stored bounds empty to isolate the relative-map admission rule.
        maskSource()->tile = true;
        maskSource()->active = false;
        maskSource()->values[0] = maskSource()->bg;
        maskSource()->xform.forward[0][0] = scale;
        maskSource()->xform.inverse[0][0] = 1 / scale;
        reject(data);
    }
    setup();
    data = block(3);
    maskSource()->xform.inverseShift[1] = 3e9;
    reject(data);
    setup();
    data = block(3);
    maskSource()->values[0] = notNumber;
    reject(data);
}
void budgetsAndFailures() {
    setup();
    auto data = block();
    source()->memory = dommy::vdb::kMaxVdbPayloadBytes / 4;
    reject(data);
    setup();
    data = block();
    meshBounds.low.x = meshBounds.low.y = meshBounds.low.z = 0;
    meshBounds.high.x = meshBounds.high.y = meshBounds.high.z = 10000;
    reject(data);
    setup();
    data = block();
    inputData.grids.clear();
    for (int i = 0; i < 40; ++i)
        inputData.grids.push_back(grid("source"));
    reject(data);
    setup();
    data = block();
    source()->storedBox.reset({0, 0, 0}, {10000, 10000, 10000});
    reject(data);
    setup();
    data = block(3);
    maskSource()->storedBox.reset({0, 0, 0}, {10000, 10000, 10000});
    reject(data);
    setup();
    data = block();
    inputData.grids.push_back(grid("source"));
    const_cast<Grid*>(inputData.grids.back().get())->xform.inverseShift[0] = 3e9;
    reject(data);
    for (int kind = 0; kind < 6; ++kind) {
        setup();
        data = block();
        if (kind == 0)
            nullClip = true;
        if (kind == 1)
            wrongClip = true;
        if (kind == 2)
            aliasClip = true;
        if (kind == 3)
            badClipValue = true;
        if (kind == 4)
            nullOutput = true;
        if (kind == 5)
            invalidOutputPayload = true;
        reject(data, false);
        assert(source()->name == "source");
    }
    setup();
    data = block(3);
    maskSource()->kind = openvdb::GRID_LEVEL_SET;
    nullCopy = true;
    reject(data, false);
    setup();
    data = block();
    inputData.grids.push_back(grid("source"));
    failClipAt = 1;
    reject(data, false);
    assert(clipCalls == 2);
    // Check both directions of a nonuniform affine map and multi-corner shear.
    setup();
    data = block(3);
    maskSource()->xform.forward[0][0] = 2;
    maskSource()->xform.inverse[0][0] = .5;
    assert(compute(data));
    setup();
    data = block();
    source()->xform.inverse[0][1] = -2;
    source()->xform.forward[0][1] = 2;
    assert(compute(data) && lastClipBox.min()[0] == -3 && lastClipBox.max()[0] == 3);
}
void injection() {
    int tested = 0;
    for (int mode = 0; mode < 4; ++mode) {
        setup();
        auto data = block(mode);
        if (mode == 3)
            maskSource()->kind = openvdb::GRID_LEVEL_SET;
        assert(compute(data));
        const int calls = callCount;
        for (int failure = 0; failure < calls; ++failure)
            for (int kind = 0; kind < 3; ++kind) {
                setup();
                data = block(mode);
                if (mode == 3)
                    maskSource()->kind = openvdb::GRID_LEVEL_SET;
                if (kind == 2)
                    attributeThrowAt = failure;
                else {
                    failAt = failure;
                    injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(!compute(data) && invalidReads == 0 && callCount == failure + 1);
                if (failure == calls - 1)
                    assert(publications == 1);
                else
                    assert(publications == 0 && outputData.grids[0]->name == "previous");
                assert(source()->name == "source");
                ++tested;
            }
    }
    std::cout << "Clip compute: " << tested << " failed statuses/exceptions passed\n";
}
int main() {
    contracts();
    invalidInputs();
    budgetsAndFailures();
    injection();
    std::cout
        << "Clip: 8 types, 4 modes, coordinate/relative-map/batch budgets and publication passed\n";
}
