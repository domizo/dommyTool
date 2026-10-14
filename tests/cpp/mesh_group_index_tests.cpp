#include "mesh_group_index_doubles.h"
#include "mesh_group_index.inc"

static void fixture(DComponentType type) {
    resetRun();
    injectedStatus = MS::kFailure;
    invalidInput = nullNative = nullArray = false;
    outputIndices.clear();
    groupInput.meshes.clear();
    for (int i = 0; i < 3; ++i) {
        auto mesh = std::make_shared<GroupIndexMesh>();
        mesh->groups[type] = std::make_shared<GroupIndexMask>();
        mesh->groups[type]->ids.assign(type == D_COMP_FACE ? 1U : 4U, false);
        mesh->groups[type]->ids[0] = i == 2;
        groupInput.meshes.push_back(mesh);
    }
}
static MStatus compute() {
    MDataBlock data;
    data.inputs["GroupName"].string = "selected";
    return DMGroupToIndex().compute(MPlug(DMGroupToIndex::outDataArray), data);
}
static void selections() {
    for (DComponentType type : {D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE}) {
        fixture(type);
        assert(compute() && publications == 1);
        const std::vector<int> expected{type == D_COMP_FACE ? 2 : 8};
        assert(outputIndices == expected);
        fixture(type);
        groupInput.meshes[0]->groups.clear();
        groupInput.meshes[1]->groups.clear();
        assert(compute() && outputIndices == expected);
        // Empty primitives occupy zero indices, even in the native edge domain.
        fixture(type);
        auto& empty = *groupInput.meshes[0];
        empty.points = empty.connections = 0U;
        empty.faces = empty.edges = 0;
        empty.groups[type]->ids.clear();
        assert(compute() && outputIndices == std::vector<int>({type == D_COMP_FACE ? 1 : 4}));
        fixture(type);
        for (auto& mesh : groupInput.meshes) mesh->groups.clear();
        assert(compute() && outputIndices.empty() && publications == 1);
        fixture(type);
        groupInput.meshes.clear();
        assert(compute() && outputIndices.empty());
        fixture(type);
        groupInput.meshes[1]->groups[type]->ids.push_back(false);
        assert(!compute() && publications == 0);
        fixture(type);
        groupInput.meshes[1]->groups[type].reset();
        assert(!compute() && publications == 0);
    }
    fixture(D_COMP_POINT);
    groupInput.meshes[1]->groups.clear();
    groupInput.meshes[1]->groups[D_COMP_EDGE] = std::make_shared<GroupIndexMask>();
    groupInput.meshes[1]->groups[D_COMP_EDGE]->ids.assign(4U, false);
    assert(!compute() && publications == 0);
    fixture(D_COMP_POINT);
    // Preserve the historical point-before-edge priority inside one primitive.
    groupInput.meshes[1]->groups[D_COMP_EDGE] = std::make_shared<GroupIndexMask>();
    groupInput.meshes[1]->groups[D_COMP_EDGE]->ids.assign(4U, true);
    assert(compute() && outputIndices == std::vector<int>({8}));
}
static void boundaries() {
    fixture(D_COMP_POINT);
    invalidInput = true;
    assert(!compute() && publications == 0);
    fixture(D_COMP_POINT);
    groupInput.meshes[0].reset();
    assert(!compute() && publications == 0);
    fixture(D_COMP_POINT);
    groupInput.meshes[0]->points = dommy::generation::kMaxGeneratedPointCount + 1U;
    assert(!compute() && publications == 0);
    for (int edges : {-1, int(dommy::generation::kMaxGeneratedPointCount + 1U)}) {
        fixture(D_COMP_EDGE);
        groupInput.meshes[0]->edges = edges;
        assert(!compute() && publications == 0);
    }
    fixture(D_COMP_EDGE);
    groupInput.meshes[0]->points = 3000000U;
    assert(!compute() && publications == 0);
    assert(std::find(operations.begin(), operations.end(), "mesh.convert") == operations.end());
    fixture(D_COMP_EDGE);
    nullNative = true;
    assert(!compute() && publications == 0);
    fixture(D_COMP_POINT);
    nullArray = true;
    assert(!compute() && publications == 0);
}
static void failures() {
    unsigned int tested = 0;
    for (DComponentType type : {D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE}) {
        fixture(type);
        assert(compute());
        const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) {
            for (int kind = 0; kind < 3; ++kind) {
                fixture(type);
                if (kind == 2) throwAt = int(i);
                else {
                    failAt = int(i);
                    injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(compute() == (kind == 2 ? MS::kFailure : injectedStatus));
                assert(callCount == int(i) + 1);
                if (trace[i] != "output.clean") assert(publications == 0);
                ++tested;
                fixture(type);
                assert(compute());
            }
            if (trace[i] == "index.allocate") {
                fixture(type);
                shortAt = int(i);
                assert(!compute() && publications == 0);
            }
        }
    }
    std::cout << "Group indices: missing/empty groups and component domains verified; "
              << tested << " status/exception injections passed\n";
}
int main() {
    selections();
    boundaries();
    failures();
}
