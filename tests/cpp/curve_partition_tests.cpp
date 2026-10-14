#include "curve_partition_doubles.h"
#include "curve_partition.inc"
#include "curve_transform_cases.h"

namespace {
DCurveData inputData;
void setup(bool grouped = false) {
    reset();
    assert(DCConvex2D::initialize() == MS::kSuccess);
    assert(DCPartition::initialize() == MS::kSuccess);
    inputData = DCurveData();
    for (const auto& points :
         {MPointArray{{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}},
          MPointArray{{3, 0, 2}, {5, 0, 2}, {4, 2, 2}},
          MPointArray{{0, 4, 0}, {2, 4, 0}, {1, 5, 0}, {2, 6, 0}, {0, 6, 0}}}) {
        auto curve = std::make_shared<DCurveInternalData>(points, 1);
        curve->attributes["marker"] = std::vector<float>(points.size(), 7);
        inputData.curves.push_back(curve);
    }
    if (grouped) {
        inputData.addGrp("piece.0", {true, true, false});
        inputData.addGrp("piece.1", {false, true, false});
        inputData.addGrp("tag", {true, false, true});
    }
    inputs.clear();
    inputs["inDC"].plugin = &inputData;
    inputs["PieceGroupName"].string = "piece.*";
    inputs["Mode"].number = 1;
    reset();
}
template <class Node> MStatus compute() {
    Node node;
    MDataBlock block;
    return node.compute(MPlug(node.thisMObject(), Node::outDC), block);
}
void samePoints(const MPointArray& a, const MPointArray& b) {
    assert(a.size() == b.size());
    for (unsigned i = 0; i < a.size(); ++i) {
        assert(std::abs(a[i].x - b[i].x) < 1e-6);
        assert(std::abs(a[i].y - b[i].y) < 1e-6);
        assert(std::abs(a[i].z - b[i].z) < 1e-6);
    }
}
void unchanged(const DCurveData& before) {
    assert(inputData.curves.size() == before.curves.size());
    assert(inputData.groups.size() == before.groups.size());
    assert(inputData.strings == before.strings && inputData.attributes == before.attributes);
    for (unsigned i = 0; i < before.curves.size(); ++i) {
        const auto& a = *inputData.curves[i];
        const auto& b = *before.curves[i];
        samePoints(a.points, b.points);
        assertSameMatrix(a.primitiveMatrix, b.primitiveMatrix);
        assert(a.knots == b.knots && a.degree == b.degree && a.open == b.open &&
               a.attributes == b.attributes);
    }
    for (const auto& group : before.groups)
        assert(inputData.groups.at(group.first)->ids == group.second->ids);
}
DCurveData snapshot() {
    DCurveData result;
    result.curves.clear();
    for (const auto& curve : inputData.curves)
        result.curves.push_back(std::make_shared<DCurveInternalData>(*curve));
    for (const auto& group : inputData.groups)
        result.groups[group.first] = std::make_shared<TestCurveGroup>(*group.second);
    result.attributes = inputData.attributes;
    result.strings = inputData.strings;
    return result;
}
void convexSemantics() {
    setup();
    inputData.curves[0]->primitiveMatrix[3][0] = 100;
    const auto before = snapshot();
    assert(compute<DCConvex2D>() == MS::kSuccess && outputData.curves.size() == 3);
    for (unsigned i = 0; i < 3; ++i) {
        samePoints(outputData.curves[i]->points, inputData.curves[i]->points);
        assertSameMatrix(outputData.curves[i]->primitiveMatrix,
                         inputData.curves[i]->primitiveMatrix);
        assert(outputData.curves[i]->attributes == inputData.curves[i]->attributes);
    }
    assert(outputData.strings.at("src_path") == DStringList({"index:0", "index:1", "index:2"}));
    unchanged(before);
    const auto trace = operations;
    const int firstInsert = std::find(trace.begin(), trace.end(), "batch.insert") - trace.begin();
    assert(firstInsert < int(trace.size()));
    reset(firstInsert);
    assert(compute<DCConvex2D>() != MS::kSuccess && !published);

    setup(true);
    inputData.curves[0]->primitiveMatrix[3][0] = 10;
    inputData.curves[2]->primitiveMatrix[3][0] = -20;
    assert(compute<DCConvex2D>() == MS::kSuccess && outputData.curves.size() == 3);
    assert(outputData.strings.at("src_path") == DStringList({"piece.0", "piece.1", "index:2"}));
    assert(outputData.groups.empty());
    for (unsigned i = 0; i < 2; ++i) {
        assertSameMatrix(outputData.curves[i]->primitiveMatrix, MMatrix());
        assert(outputData.curves[i]->degree == 1 && outputData.curves[i]->open);
        assert(outputData.curves[i]->attributes.empty());
    }
    bool hasTranslatedPoint = false;
    for (const auto& p : outputData.curves[0]->points)
        if (p.x == 12)
            hasTranslatedPoint = true;
    assert(hasTranslatedPoint);
    assertSameMatrix(outputData.curves[2]->primitiveMatrix, inputData.curves[2]->primitiveMatrix);
    samePoints(outputData.curves[2]->points, inputData.curves[2]->points);
    assert(std::count(operations.begin(), operations.end(), "batch.insert") == 1);

    setup(true);
    inputs["PieceGroupName"].string = "absent";
    assert(compute<DCConvex2D>() == MS::kSuccess && outputData.curves.size() == 3);
    setup(true);
    inputs["PieceGroupName"].string = "[";
    assert(compute<DCConvex2D>() != MS::kSuccess && !published);
    setup(true);
    inputs["PieceGroupName"].string = std::string(4097, 'x');
    assert(compute<DCConvex2D>() != MS::kSuccess && !published);
    // The historical policy consumes grouped curves even if their hull collapses.
    setup();
    inputData.curves[0]->points.assign(4, MPoint(1, 1, 0));
    inputData.addGrp("piece.0", {true, false, false});
    reset();
    assert(compute<DCConvex2D>() == MS::kSuccess && outputData.curves.size() == 2);
    assert(outputData.strings.at("src_path") == DStringList({"index:1", "index:2"}));
}
void partitionSemantics() {
    setup(true);
    assert(compute<DCPartition>() == MS::kSuccess);
    const DCurveData baseline = outputData;
    assert(baseline.curves.size() > 3);
    for (const auto& matrix : primitiveTransformCases())
        for (int mode : {0, 1, 2}) {
            setup(true);
            inputs["Mode"].number = mode;
            inputData.curves[0]->primitiveMatrix = matrix;
            inputData.curves[1]->primitiveMatrix[3][0] = -100;
            const auto before = snapshot();
            assert(compute<DCPartition>() == MS::kSuccess &&
                   outputData.curves.size() == baseline.curves.size());
            const auto& paths = outputData.strings.at("src_path");
            assert(paths == baseline.strings.at("src_path"));
            for (unsigned i = 0; i < outputData.curves.size(); ++i) {
                const unsigned parent = std::stoul(paths[i].substr(6));
                const auto& curve = outputData.curves[i];
                assertSameMatrix(curve->primitiveMatrix, inputData.curves[parent]->primitiveMatrix);
                samePoints(curve->points, baseline.curves[i]->points);
                assert(curve->degree == 1 && curve->open && curve->attributes.empty());
                for (const auto& group : inputData.groups)
                    assert(outputData.groups.at(group.first)->ids[i] == group.second->ids[parent]);
            }
            unchanged(before);
            assert(std::count(operations.begin(), operations.end(), "batch.insert") == 1);
        }
    for (int mode : {-1, 3, 4}) {
        setup();
        inputs["Mode"].number = mode;
        assert(compute<DCPartition>() == MS::kInvalidParameter && !published);
    }
    // A non-simple projected polygon is intentionally skipped, not repaired.
    setup();
    inputData.curves[0]->points = {{0, 0, 0}, {2, 2, 0}, {0, 2, 0}, {2, 0, 0}};
    assert(compute<DCPartition>() == MS::kSuccess);
    for (const auto& path : outputData.strings.at("src_path"))
        assert(path != "index:0");
}
template <class Node> int failures() {
    reset();
    const auto before = snapshot();
    assert(compute<Node>() == MS::kSuccess);
    const auto trace = operations;
    int count = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            const auto result = compute<Node>();
            assert(result != MS::kSuccess);
            if (trace[i].find("input ") == 0 || trace[i] == "plugin.read")
                assert(result == status);
            if (trace[i] != "clean")
                assert(!published && !cleaned);
            unchanged(before);
            ++count;
        }
        reset();
        throwAt = i;
        assert(compute<Node>() != MS::kSuccess);
        if (trace[i] != "clean")
            assert(!published && !cleaned);
        unchanged(before);
        ++count;
        if (trace[i] == "array.resize") {
            reset();
            shortAt = i;
            assert(compute<Node>() != MS::kSuccess && !published);
            unchanged(before);
            ++count;
        }
    }
    reset();
    assert(compute<Node>() == MS::kSuccess);
    unchanged(before);
    return count;
}
template <class Node> void boundaries() {
    setup();
    inputData.curves.clear();
    assert(compute<Node>() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute<Node>() == MS::kInvalidParameter && !published);
    setup();
    Node node;
    MDataBlock data;
    assert(node.compute(MPlug(node.thisMObject(), {"unknown"}), data) == MS::kUnknownParameter &&
           !callCount);
}
void inputBoundaries() {
    for (double value : {double(NAN), double(INFINITY), 1.0}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = value;
        assert(compute<DCPartition>() == MS::kInvalidParameter && !published);
    }
    setup(true);
    inputData.curves[0]->primitiveMatrix[3][0] = 1e40;
    assert(compute<DCConvex2D>() != MS::kSuccess && !published);
    setup(true);
    inputData.groups["piece.0"]->ids = {true, false, false, true};
    assert(compute<DCConvex2D>() == MS::kInvalidParameter && !published);
    setup(true);
    inputData.groups["piece.0"]->ids = {true};
    assert(compute<DCPartition>() == MS::kInvalidParameter && !published);
    setup(true);
    inputData.groups["piece.0"].reset();
    assert(compute<DCConvex2D>() == MS::kInvalidParameter && !published);
    reset();
    assert(compute<DCPartition>() == MS::kInvalidParameter && !published);
    setup();
    inputData.addGrp("piece.empty", {false, false, false});
    reset();
    assert(compute<DCConvex2D>() == MS::kSuccess && outputData.curves.size() == 3);
    setup();
    inputData.curves[0] =
        std::make_shared<DCurveInternalData>(MPointArray{{0, 0, 0}, {1, 1, 0}}, 1);
    assert(compute<DCPartition>() == MS::kSuccess);
    for (const auto& path : outputData.strings.at("src_path"))
        assert(path != "index:0");
}
template <class Node> void largeBatch() {
    setup();
    const DCItem source = inputData.curves[0];
    inputData.curves.assign(512, source);
    assert(compute<Node>() == MS::kSuccess && outputData.curves.size() == 512);
    assert(outputData.strings.at("src_path").size() == 512);
    assert(std::count(operations.begin(), operations.end(), "batch.insert") == 1);
}
void smallPolygonRegression() {
    for (float edge : {1.0e-9f, 1.0e-6f, 1.0e-4f}) {
        setup();
        inputData.curves.resize(1);
        inputData.curves[0]->points = {{0, 0, 3}, {edge, 0, 3},
                                     {edge, edge, 3}, {0, edge, 3}};
        inputData.curves[0]->primitiveMatrix[3][0] = 17;
        inputData.addGrp("tiny", {true});
        const auto before = snapshot();
        assert(compute<DCPartition>() == MS::kSuccess && published == 1);
        assert(outputData.curves.size() == 1U && outputData.curves[0]->points.size() == 4U);
        assertSameMatrix(outputData.curves[0]->primitiveMatrix, inputData.curves[0]->primitiveMatrix);
        assert(outputData.groups.at("tiny")->ids == DPIdList({true}));
        assert(outputData.strings.at("src_path") == DStringList({"index:0"}));
        for (const auto& point : outputData.curves[0]->points) assert(point.z == 3);
        unchanged(before);
    }
    std::cout << "DCPartition preserves small squares, Z, groups and primitive transforms\n";
}

void aggregatePolygonBudget() {
    assert(dommy::geometry::PolygonWorkBudget().remaining() == 256U);
    setup();
    inputData.curves.resize(1);
    assert(compute<DCPartition>() == MS::kSuccess && published == 1);
    setup();
    inputData.curves.assign(3, inputData.curves[0]);
    const auto before = snapshot();
    assert(compute<DCPartition>() == MS::kFailure && !published && !cleaned);
    unchanged(before);
    setup();
    inputData.curves.resize(1);
    MPointArray polygon;
    for (int i = 0; i < 32; ++i)
        polygon.push_back(MPoint(double(i), 0, 0));
    polygon.push_back(MPoint(0, 1, 0));
    inputData.curves[0] = std::make_shared<DCurveInternalData>(polygon, 1);
    assert(compute<DCPartition>() == MS::kFailure && !published && !cleaned);
    setup();
    inputData.curves.resize(1);
    assert(compute<DCPartition>() == MS::kSuccess && published == 1);
    std::cout << "DCPartition shares work across curves and reports exhaustion instead of skipping\n";
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--polygon-budget") {
        aggregatePolygonBudget();
        return 0;
    }
    assert(argc == 2);
    std::string trace;
    int initialized = 0;
    for (auto entry : {std::make_pair("DCConvex2D", DCConvex2D::initialize),
                       std::make_pair("DCPartition", DCPartition::initialize),
                       std::make_pair("DCDelete", DCDelete::initialize),
                       std::make_pair("DCExplodeView", DCExplodeView::initialize),
                       std::make_pair("DCWeightMap", DCWeightMap::initialize),
                       std::make_pair("DCUVMap", DCUVMap::initialize),
                       std::make_pair("DCClumping", DCClumping::initialize)}) {
        trace += schemaTrace(entry.first, entry.second);
    }
    std::ifstream file(argv[1]);
    assert(file);
    assert(trace ==
           std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
    convexSemantics();
    partitionSemantics();
    smallPolygonRegression();
    for (auto entry : {std::make_pair("DCConvex2D", DCConvex2D::initialize),
                       std::make_pair("DCPartition", DCPartition::initialize),
                       std::make_pair("DCDelete", DCDelete::initialize),
                       std::make_pair("DCExplodeView", DCExplodeView::initialize),
                       std::make_pair("DCWeightMap", DCWeightMap::initialize),
                       std::make_pair("DCUVMap", DCUVMap::initialize),
                       std::make_pair("DCClumping", DCClumping::initialize)}) {
        reset();
        assert(entry.second() == MS::kSuccess);
        const int count = callCount;
        initialized += testFailures(entry.first, entry.second, count);
    }
    int cases = 0;
    for (bool grouped : {false, true}) {
        setup(grouped);
        cases += failures<DCConvex2D>();
        setup(grouped);
        cases += failures<DCPartition>();
    }
    boundaries<DCConvex2D>();
    boundaries<DCPartition>();
    inputBoundaries();
    largeBatch<DCConvex2D>();
    largeBatch<DCPartition>();
    std::cout << "Curve partition checks: " << initialized << " initializer failures, " << cases
              << " compute cases passed\n";
}
