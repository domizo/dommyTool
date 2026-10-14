#include "vdb_edit_doubles.h"
#include "vdb_edit_compute.inc"

GridPtr grid(const std::string& name, DVdbGridType kind = DVdbGridType::kVec3f) {
    auto result = std::make_shared<Grid>();
    result->name = name;
    result->kind = kind;
    result->xform = std::make_shared<openvdb::math::Transform>(
        std::make_shared<openvdb::math::AffineMap>(openvdb::Mat4R::identity()));
    return result;
}
void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    badMapCache = 0;
    badMapAt = -1;
    missingInput = nullCreated = nullGrid = nullMap = failedDispatch = badInverse = false;
    inputData.payloadValid = true;
    inputData.grids = {grid("selected"), grid("keep", DVdbGridType::kFloat)};
    stagedData.grids.clear();
    outputData.grids = {grid("previous")};
    publications = splits = invalidReads = gridCopies = 0;
    mapConstructions = 0;
    resetCalls();
}
MDataBlock block() {
    MDataBlock data;
    data.inputs["aVdbSelectedGridNames"].text = "0";
    data.inputs["aScale"].vector = MFloatVector(1, 1, 1);
    data.inputs["aUniformScale"].number = 1;
    data.inputs["aRemoveSource"].number = 1;
    return data;
}
using Compute = MStatus (*)(MDataBlock&);
MStatus transform(MDataBlock& data) {
    return DOpenVDBTransform().compute(MPlug(DOpenVDBTransform::aVdbOutput), data);
}
MStatus split(MDataBlock& data) {
    return DOpenVDBVectorSplit().compute(MPlug(DOpenVDBVectorSplit::aVdbOutput), data);
}
openvdb::Mat4R matrix(GridCPtr value) {
    return value->transform().baseMap()->getMat4();
}
void sameMatrix(const openvdb::Mat4R& lhs, const openvdb::Mat4R& rhs) {
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            assert(std::abs(lhs[row][col] - rhs[row][col]) <=
                   1e-10 * std::max(1.0, std::abs(rhs[row][col])));
}
void testTransform() {
    for (int invert = 0; invert < 2; ++invert) {
        setup();
        auto data = block();
        data.inputs["aTranslate"].vector = MFloatVector(3, 5, 7);
        data.inputs["aScale"].vector = MFloatVector(-2, 3, 4);
        data.inputs["aUniformScale"].number = 2;
        data.inputs["aInvert"].number = float(invert);
        auto source = std::const_pointer_cast<Grid>(inputData.grids[0]);
        source->xform->map->matrix[3][0] = 11;
        const auto original = matrix(source);
        assert(transform(data) && publications == 1 && outputData.grids.size() == 2);
        assert(outputData.grids[0]->getName() == "keep");
        assert(outputData.grids[1]->getName() == "selected");
        assert(outputData.grids[1] != source && outputData.grids[1]->tree == source->tree);
        auto operationMatrix = openvdb::Mat4R::identity();
        operationMatrix[0][0] = -4;
        operationMatrix[1][1] = 6;
        operationMatrix[2][2] = 8;
        operationMatrix[3][0] = -12;
        operationMatrix[3][1] = 30;
        operationMatrix[3][2] = 56;
        sameMatrix(matrix(outputData.grids[1]),
                   original * (invert ? operationMatrix.inverse() : operationMatrix));
        sameMatrix(matrix(source), original);
    }
    setup();
    auto data = block();
    data.inputs["aRotate"].vector = MFloatVector(0, 0, 90);
    data.inputs["aPivot"].vector = MFloatVector(2, 0, 0);
    assert(transform(data));
    auto expected = openvdb::Mat4R::identity();
    expected[0][0] = expected[1][1] = 0;
    expected[0][1] = 1;
    expected[1][0] = -1;
    expected[3][0] = 2;
    expected[3][1] = -2;
    sameMatrix(matrix(outputData.grids[1]), expected);
    setup();
    data = block();
    data.inputs["aScale"].vector = MFloatVector(1e38f, -1e38f, 1e38f);
    data.inputs["aUniformScale"].number = 10;
    assert(transform(data));
    assert(matrix(outputData.grids[1])[0][0] == double(1e38f) * 10.0);
    assert(matrix(outputData.grids[1])[1][1] == double(-1e38f) * 10.0);
    assert(matrix(outputData.grids[1])[2][2] == double(1e38f) * 10.0);
    for (const char* attr : {"aTranslate", "aRotate", "aPivot", "aScale"}) {
        for (int axis = 0; axis < 3; ++axis) {
            for (float value :
                 {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                  std::numeric_limits<float>::quiet_NaN()}) {
                setup();
                data = block();
                data.inputs[attr].vector[axis] = value;
                assert(!transform(data) && !publications && !gridCopies);
            }
        }
    }
    for (float value :
         {0.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        setup();
        data = block();
        data.inputs["aUniformScale"].number = value;
        assert(!transform(data) && !publications);
    }
    for (int mode = 0; mode < 5; ++mode) {
        setup();
        data = block();
        auto source = std::const_pointer_cast<Grid>(inputData.grids[0]);
        if (mode == 0)
            source->xform->map->matrix[0][0] = std::numeric_limits<double>::infinity();
        if (mode == 1)
            source->xform->map->matrix[0][3] = 1.0;
        if (mode == 2) {
            source->xform->map->matrix[0][0] = 1e308;
            data.inputs["aScale"].vector[0] = 10;
        }
        if (mode == 3)
            nullMap = true;
        if (mode == 4)
            badInverse = true;
        assert(!transform(data) && !publications);
        assert(outputData.grids[0]->getName() == "previous");
        if (mode == 4)
            assert(std::find(operations.begin(), operations.end(), "map.create") ==
                   operations.end());
    }
}
void testNativeMapCacheBoundary() {
    for (int at = 0; at < 2; ++at)
        for (int invalid = 1; invalid <= 4; ++invalid) {
            setup();
            auto data = block();
            badMapAt = at;
            badMapCache = invalid;
            assert(!transform(data) && !publications);
            assert(outputData.grids[0]->getName() == "previous");
        }
}
void testSplit() {
    for (auto type : {DVdbGridType::kVec3f, DVdbGridType::kVec3d, DVdbGridType::kVec3i})
        for (int remove = 0; remove < 2; ++remove)
            for (int inactive = 0; inactive < 2; ++inactive) {
                setup();
                auto data = block();
                std::const_pointer_cast<Grid>(inputData.grids[0])->kind = type;
                data.inputs["aRemoveSource"].number = float(remove);
                data.inputs["aCopyInactiveValue"].number = float(inactive);
                assert(split(data) && splits == 1 && lastCopyInactive == bool(inactive));
                assert(outputData.grids.size() == (remove ? 4U : 5U));
                assert(outputData.grids[0]->getName() == "selected.x");
                assert(outputData.grids[1]->getName() == "selected.y");
                assert(outputData.grids[2]->getName() == "selected.z");
                assert(inputData.grids[0]->getName() == "selected");
                assert(outputData.grids.back() == inputData.grids[1]);
            }
    setup();
    auto data = block();
    inputData.grids[1] = grid("selected");
    assert(split(data) && outputData.grids.size() == 4 &&
           outputData.grids.back() == inputData.grids[1]);
    setup();
    data = block();
    inputData.grids[1] = inputData.grids[0];
    assert(split(data) && outputData.grids.size() == 3); // remove all selected-object aliases
    setup();
    data = block();
    inputData.grids[1] = inputData.grids[0];
    data.inputs["aVdbSelectedGridNames"].text = "*";
    assert(split(data) && splits == 2 && outputData.grids.size() == 6); // two selected entries
    setup();
    data = block();
    std::const_pointer_cast<Grid>(inputData.grids[0])->name.clear();
    assert(split(data) && outputData.grids[0]->getName() == "x");
    for (auto type : {DVdbGridType::kFloat, DVdbGridType::kBool, DVdbGridType::kInvalid}) {
        setup();
        data = block();
        std::const_pointer_cast<Grid>(inputData.grids[0])->kind = type;
        assert(split(data) && !splits && outputData.grids == inputData.grids);
    }
    for (int remove = 0; remove < 2; ++remove) {
        for (int over = 0; over < 2; ++over) {
            setup();
            data = block();
            auto source = inputData.grids[0];
            const int count = remove ? 1365 : 1024;
            inputData.grids.assign(std::size_t(count), source);
            if (remove || over)
                inputData.grids.push_back(grid("retained", DVdbGridType::kFloat));
            if (remove && over)
                inputData.grids.push_back(grid("extra", DVdbGridType::kFloat));
            data.inputs["aVdbSelectedGridNames"].text = "*";
            data.inputs["aRemoveSource"].number = float(remove);
            if (over) {
                assert(!split(data) && !splits && !publications);
            } else {
                assert(split(data) && outputData.grids.size() == 4096 && splits == count);
            }
        }
    }
    setup();
    data = block();
    const GridCPtr repeated = inputData.grids[0];
    inputData.grids.assign(1366, repeated);
    data.inputs["aVdbSelectedGridNames"].text = "*";
    assert(!split(data) && !splits && !publications); // historical 4098-output case
    setup();
    data = block();
    failedDispatch = true;
    assert(!split(data) && !publications);
}
void testCommon() {
    int failures = 0;
    for (auto compute : {transform, split}) {
        for (int mode = 0; mode < 6; ++mode) {
            setup();
            auto data = block();
            if (mode == 0)
                missingInput = true;
            if (mode == 1)
                inputData.grids.push_back(nullptr);
            if (mode == 2)
                inputData.payloadValid = false;
            if (mode == 3)
                nullCreated = true;
            if (mode == 4)
                nullGrid = true;
            if (mode == 5) {
                const GridCPtr repeated = inputData.grids[0];
                inputData.grids.assign(4097, repeated);
            }
            assert(!compute(data) && !publications && !invalidReads);
        }
        setup();
        auto data = block();
        inputData.grids.clear();
        assert(compute(data) && outputData.grids.empty());
        setup();
        data = block();
        data.inputs["aVdbSelectedGridNames"].text = "missing";
        assert(compute(data) && outputData.grids.size() == 2 && !splits);
        setup();
        data = block();
        assert(compute(data));
        const int count = callCount;
        for (int mode = 0; mode < 3; ++mode)
            for (int index = 0; index < count; ++index) {
                setup();
                data = block();
                injectedStatus = mode == 1 ? MS::kInvalidParameter : MS::kFailure;
                if (mode == 2)
                    attributeThrowAt = index;
                else
                    failAt = index;
                const MStatus result = compute(data);
                assert(!result && !invalidReads && callCount == index + 1);
                if (mode != 2 && (operations[index].compare(0, 6, "input ") == 0 ||
                                  operations[index].compare(0, 7, "output.") == 0))
                    assert(result == injectedStatus);
                assert(publications == (index == count - 1 ? 1 : 0));
                if (!publications)
                    assert(outputData.grids[0]->getName() == "previous");
                assert(inputData.grids[0]->getName() == "selected");
                sameMatrix(matrix(inputData.grids[0]), openvdb::Mat4R::identity());
                setup();
                data = block();
                assert(compute(data) && publications == 1);
                ++failures;
            }
    }
    setup();
    auto data = block();
    assert(DOpenVDBTransform().compute(MPlug({"other"}), data) == MS::kUnknownParameter);
    assert(DOpenVDBVectorSplit().compute(MPlug({"other"}), data) == MS::kUnknownParameter);
    assert(!callCount);
    std::cout << "VDB edits: transform/split boundaries and " << failures
              << " compute failure/exception cases passed\n";
}
int main() {
    testTransform();
    testNativeMapCacheBoundary();
    testSplit();
    testCommon();
}
