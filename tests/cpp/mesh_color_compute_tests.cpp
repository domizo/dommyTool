#include "legacy_mesh_doubles.h"
#include "common/utility/DMeshInputValidation.h"
#include "Utility/3d/DataStruct/DHashGrid.h"
#include "mesh_color_compute.inc"

MDataBlock setup(int mode) {
    auto block = resetMeshRun();
    block.inputs["inMesh"] = {"base"}; block.inputs["relaxMesh"] = {"target"};
    block.controls["aCalcType"] = mode; block.controls["aSampleRadius"] = 10;
    return block;
}
MStatus evaluate(int kind, MDataBlock& block) {
    if (kind == 0) {
        DPolyThickness node; return node.compute(MPlug(node.thisMObject(), DPolyThickness::outMesh), block);
    }
    DStretchData node; return node.compute(MPlug(node.thisMObject(), DStretchData::outMesh), block);
}

void apiCases(int kind, int mode) {
    auto block = setup(mode);
    assert(evaluate(kind, block));
    const auto baseline = operations;
    for (auto failure : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = failure;
        for (int at = 0; at < int(baseline.size()); ++at) {
            block = setup(mode); failAt = at;
            assert(evaluate(kind, block) == failure);
            assert(callCount == at + 1);
            if (baseline[at] != "data.clean") assert(published == 0);
        }
    }
    for (int at = 0; at < int(baseline.size()); ++at) {
        block = setup(mode); throwAt = at;
        assert(!evaluate(kind, block) && callCount == at + 1);
        if (baseline[at] != "data.clean") assert(published == 0);
        if (baseline[at] == "array.setLength" || baseline[at] == "mesh.points" || baseline[at] == "mesh.topology") {
            block = setup(mode); shortAt = at;
            assert(!evaluate(kind, block) && published == 0);
        }
    }
    for (int which = 0; which < 2; ++which) {
        block = setup(mode); nullCreated = which == 0; nullCopied = which == 1;
        assert(!evaluate(kind, block) && published == 0);
        const auto expected = std::find(baseline.begin(), baseline.end(), which == 0 ? "mesh.create" : "mesh.copy");
        assert(expected != baseline.end() && callCount == std::distance(baseline.begin(), expected) + 1);
    }
    if (mode == 0) for (int which = 0; which < 3; ++which) {
        block = setup(mode); badIndex = which == 0; earlyIterator = which == 1; stuckIterator = which == 2;
        assert(!evaluate(kind, block) && published == 0 && callCount < 200);
    }
    std::cout << "Mesh color compute " << kind << "/" << mode << ": " << baseline.size()
              << " API failure/exception sites passed\n";
}

int main() {
    apiCases(0, 0); apiCases(1, 0); apiCases(1, 1);
    auto block = setup(0);
    assert(evaluate(0, block));
    for (const auto& c : meshes.at(outputs.at("outMesh").name).colors) assert(c.r == 1 && c.g == 1 && c.a == 1);
    block = setup(0); missRay = true; assert(evaluate(0, block));
    for (const auto& c : meshes.at(outputs.at("outMesh").name).colors) assert(c.r == 0 && c.a == 1);
    for (double value : {double(NAN), double(INFINITY), 1e40}) {
        block = setup(0); for (auto& point : meshes["base"].points) point.x = value;
        assert(!evaluate(0, block) && published == 0);
        block = setup(1); for (auto& point : meshes["target"].points) point.x = value;
        assert(!evaluate(1, block) && published == 0);
    }
    block = setup(0); normalValue = NAN; assert(!evaluate(0, block) && published == 0);
    block = setup(0); normalValue = 0; assert(evaluate(0, block));
    block = setup(0); hitValue = NAN; assert(!evaluate(0, block) && published == 0);
    block = setup(0); edgeLengthScale = 1e100; assert(!evaluate(1, block) && published == 0);
    block = setup(0); edgeLengthScale = double(std::numeric_limits<float>::max()) * 1.5;
    assert(!evaluate(1, block) && published == 0);
    block = setup(1); shortenAllPoints = true; assert(!evaluate(1, block) && published == 0);
    block = setup(1); meshes["base"].points[0].x = 1e100;
    assert(!evaluate(1, block) && published == 0);
    block = setup(0); meshes["target"].edges.pop_back(); assert(!evaluate(1, block) && published == 0);
    block = setup(0); meshes["target"].edges[0] = {0, 2}; assert(!evaluate(1, block) && published == 0);
    block = setup(0); meshes["target"].reportedEdges = meshes["base"].reportedEdges = 100000000;
    assert(!evaluate(1, block) && published == 0);
    block = setup(1); meshes["base"].reportedPoints = meshes["target"].reportedPoints = 10000000;
    assert(!evaluate(1, block) && published == 0);
    // Exact legacy edge-channel convention; grid mode uses stretch in red.
    block = setup(0); assert(evaluate(1, block));
    const auto& edgeColors = meshes.at(outputs.at("outMesh").name).colors;
    assert(edgeColors[0].r == .5f && edgeColors[0].g == 0 && edgeColors[0].a == 2);
    block = setup(1); assert(evaluate(1, block));
    const auto& gridColors = meshes.at(outputs.at("outMesh").name).colors;
    assert(gridColors[0].r == 0 && gridColors[0].g > 0 && gridColors[0].a == 1);
    std::cout << "Thickness rays, bounded traversal, Stretch topology/cardinality/float checks passed\n";
}
