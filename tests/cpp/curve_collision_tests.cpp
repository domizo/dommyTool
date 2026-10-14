#include "curve_collision_doubles.h"
#include "curve_collision.inc"
#include "curve_transform_cases.h"

namespace {
DCurveData inputData;
void setup() {
    reset();
    assert(DCCutObject::initialize() == MS::kSuccess);
    assert(DCReflect::initialize() == MS::kSuccess);
    assert(DCOffset::initialize() == MS::kSuccess);
    assert(DCNormal::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    meshes.clear();
    inputData = DCurveData();
    DCItem curve(new DCurveInternalData(MPointArray{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}},
                                        MDoubleArray{0, 1, 2, 3}, 1));
    curve->useGeometryFixture = true;
    curve->geometryFixture.points = curve->points;
    curve->geometryFixture.degree = 1;
    curve->geometryFixture.start = 0;
    curve->geometryFixture.end = 3;
    curve->geometryFixture.linearSamples = true;
    curve->attributes["marker"] = {1, 2, 3, 4};
    curve->vectorAttributes["dir"] = std::vector<MVector>(4, MVector(0, 1, 0));
    inputData.curves.push_back(curve);
    inputs["inDC"].plugin = &inputData;
    inputs["InObject"].object = {"mesh"};
    inputs["InObject"].type = MFnData::kMesh;
    inputs["InObject"].connected = true;
    meshes["mesh"].points = {{.75, 0, 0}, {2.25, 1, 1}};
    inputs["Sampling"].number = 7;
    inputs["Tolerance"].number = .1;
    inputs["OffsetVector"].dvec[1] = 1;
    inputs["OffsetVectorAttribute"].string = "dir";
    inputs["AimVector"].dvec[1] = 1;
    inputs["UpVector"].dvec[0] = 1;
    inputs["NormalAttributeName"].string = "N";
    inputs["BaseVector"].dvec[1] = 1;
    inputs["GenerateType"].number = DC_NORMAL_FROM_CURVE_TANGENT;
    inputs["Seed"].number = 1;
    acceleratorReleases = 0;
    intersectionSources.clear();
    rebuiltReportedCount = -1;
    reset();
}
template <class Node> MStatus compute() {
    Node node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), Node::outDC), data);
}
void near(double a, double b) {
    assert(std::abs(a - b) < 1e-6);
}
void samePoint(const MPoint& a, const MPoint& b) {
    near(a.x, b.x);
    near(a.y, b.y);
    near(a.z, b.z);
}
void checkInput(const MMatrix& matrix) {
    assert(!inputData.curves.empty());
    for (const auto& curve : inputData.curves) {
        const auto& c = *curve;
        assertSameMatrix(c.primitiveMatrix, matrix);
        assert(c.knots == MDoubleArray({0, 1, 2, 3}) &&
               c.attributes.at("marker") == std::vector<float>({1, 2, 3, 4}));
        assert(c.points.size() == 4 && c.degree == 1 && c.open &&
               c.vectorAttributes.count("N") == 0);
        for (unsigned i = 0; i < 4; ++i)
            samePoint(c.points[i], MPoint(i, 0, 0));
    }
}
void testOffset() {
    for (const auto& matrix : primitiveTransformCases())
        for (double distance : {0., .1}) {
            setup();
            inputData.curves[0]->primitiveMatrix = matrix;
            inputs["Dsitance"].number = distance;
            assert(compute<DCOffset>() == MS::kSuccess && outputData.curves.size() == 1);
            assertSameMatrix(outputData.curves[0]->primitiveMatrix, matrix);
            if (distance == 0)
                samePoint(outputData.curves[0]->points.front(), MPoint(0, 0, 0));
            checkInput(matrix);
        }
}
void testNormal() {
    for (int mode = 0; mode < DC_NORMAL_GEN_TYPE_COUNT; ++mode) {
        setup();
        inputs["GenerateType"].number = mode;
        const auto matrix = primitiveTransformCases()[2];
        inputData.curves[0]->primitiveMatrix = matrix;
        assert(compute<DCNormal>() == MS::kSuccess && outputData.curves.size() == 1);
        assertSameMatrix(outputData.curves[0]->primitiveMatrix, matrix);
        const auto& normals = outputData.curves[0]->vectorAttributes.at("N");
        assert(normals.size() == 4);
        for (const auto& n : normals) {
            near(n.x, mode == DC_NORMAL_FROM_CURVE_TANGENT ? 1 : 0);
            near(n.y, mode == DC_NORMAL_FROM_CURVE_TANGENT ? 0 : 1);
        }
        checkInput(matrix);
    }
}
template <class Node> void testCollision(bool reflect) {
    for (const auto& matrix : primitiveTransformCases()) {
        setup();
        inputData.curves[0]->primitiveMatrix = matrix;
        inputs["InObject"].matrix[3][0] = 20;
        assert(compute<Node>() == MS::kSuccess);
        assert(acceleratorReleases == 1 && intersectionSources.size() == 7);
        for (unsigned i = 0; i < 7; ++i)
            samePoint(intersectionSources[i], MPoint(.5 * i, 0, 0) * matrix);
        if (reflect) {
            assert(outputData.curves.size() == 1);
            assertSameMatrix(outputData.curves[0]->primitiveMatrix, MMatrix());
            samePoint(outputData.curves[0]->points.back(), MPoint(3, 0, 0) * matrix);
        } else
            assert(outputData.curves.empty());
        assert(meshes.count("mesh.transformed") == 1);
        near(meshes.at("mesh.transformed").points.front().x, 20.75);
        near(meshes.at("mesh").points.front().x, .75);
        checkInput(matrix);
    }
    setup();
    meshes["mesh"].analyticSlab = true;
    inputs["FitCutPoint"].number = 1;
    inputData.curves[0]->primitiveMatrix[0][0] = 2;
    inputs["InObject"].matrix[0][0] = 2;
    assert(compute<Node>() == MS::kSuccess && !outputData.curves.empty());
    if (!reflect) {
        samePoint(outputData.curves[0]->points.front(), MPoint(1.5, 0, 0));
        samePoint(outputData.curves[0]->points.back(), MPoint(4.5, 0, 0));
    } else {
        // Both hit distance and remaining/continuation steps use world length.
        const MPointArray expected{{0, 0, 0}, {1, 0, 0}, {1.5, 0, 0}, {1, 0, 0},
                                   {2, 0, 0}, {3, 0, 0}, {4, 0, 0},   {5, 0, 0}};
        const auto& actual = outputData.curves[0]->points;
        assert(actual.size() == expected.size());
        for (unsigned i = 0; i < actual.size(); ++i)
            samePoint(actual[i], expected[i]);
    }
}
template <class Node> int failures() {
    reset();
    acceleratorReleases = 0;
    intersectionSources.clear();
    assert(compute<Node>() == MS::kSuccess);
    const auto trace = operations;
    const MMatrix matrix = inputData.curves[0]->primitiveMatrix;
    const auto inputCount = inputData.curves.size();
    const bool hasMesh = std::find(trace.begin(), trace.end(), "mesh.intersections") != trace.end();
    const int guardStart = std::find(trace.begin(), trace.end(), "curve.fromData") - trace.begin();
    int tests = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = status;
            acceleratorReleases = 0;
            assert(compute<Node>() != MS::kSuccess);
            if (trace[i] != "clean")
                assert(!published);
            assert(inputData.curves.size() == inputCount);
            assert(acceleratorReleases == (hasMesh && i >= guardStart ? 1 : 0));
            checkInput(matrix);
            ++tests;
        }
        reset();
        throwAt = i;
        acceleratorReleases = 0;
        assert(compute<Node>() != MS::kSuccess);
        if (trace[i] != "clean")
            assert(!published);
        assert(inputData.curves.size() == inputCount);
        assert(acceleratorReleases == (hasMesh && i >= guardStart ? 1 : 0));
        checkInput(matrix);
        ++tests;
        // DCurveData::insert has a separately tested transactional contract;
        // exercise its failure/exception above, not a fabricated partial success.
        if (trace[i] == "array.resize" || trace[i] == "array.copy" || trace[i] == "curve.cvs" ||
            trace[i] == "curve.knots" || trace[i] == "mesh.points") {
            reset();
            shortAt = i;
            acceleratorReleases = 0;
            const auto status = compute<Node>();
            if (shortApplied) {
                if (status == MS::kSuccess || published)
                    std::cerr << "Short result accepted at " << trace[i] << " index " << i << "\n";
                assert(status != MS::kSuccess && !published);
                ++tests;
            } else
                assert(status == MS::kSuccess);
            assert(acceleratorReleases == (hasMesh && i >= guardStart ? 1 : 0));
            checkInput(matrix);
        }
    }
    reset();
    acceleratorReleases = 0;
    assert(compute<Node>() == MS::kSuccess && (!hasMesh || acceleratorReleases == 1));
    return tests;
}
template <class Node> void boundaries() {
    setup();
    inputData.curves.clear();
    assert(compute<Node>() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute<Node>() != MS::kSuccess && !published);
    setup();
    Node node;
    MDataBlock data;
    assert(node.compute(MPlug(node.thisMObject(), {"unknown"}), data) == MS::kUnknownParameter &&
           callCount == 0);
}
template <class Node> int multipleCurveFailures() {
    setup();
    inputData.curves.push_back(DCItem(new DCurveInternalData(*inputData.curves[0])));
    for (auto& curve : inputData.curves)
        curve->primitiveMatrix[3][0] = 100;
    meshes["mesh"].analyticSlab = true;
    inputs["InObject"].matrix[3][0] = 100;
    inputs["FitCutPoint"].number = 1;
    return failures<Node>();
}
template <class Node> void collisionBoundaries() {
    for (bool projective : {false, true}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = projective ? 1 : NAN;
        assert(compute<Node>() == MS::kInvalidParameter && !published && acceleratorReleases == 1);
        setup();
        inputs["InObject"].matrix[0][3] = projective ? 1 : NAN;
        assert(compute<Node>() == MS::kInvalidParameter && !published && !acceleratorReleases);
    }
    setup();
    inputData.curves[0]->primitiveMatrix[3][0] = 1e40;
    assert(compute<Node>() == MS::kInvalidParameter && !published && acceleratorReleases == 1);
    setup();
    inputs["InObject"].type = MFnData::kNurbsCurve;
    assert(compute<Node>() == MS::kInvalidParameter && !published && !acceleratorReleases);
    setup();
    meshes["mesh"].points.clear();
    reset();
    assert(compute<Node>() == MS::kInvalidParameter && !published && !acceleratorReleases);
    setup();
    meshes["mesh"].reportedPoints = 10000001;
    assert(compute<Node>() == MS::kInvalidParameter && !published);
    assert(std::find(operations.begin(), operations.end(), "mesh.points") == operations.end());
    setup();
    inputs["Sampling"].number = 1000001;
    assert(compute<Node>() == MS::kInvalidParameter && !published && !acceleratorReleases);
}
template <class Node> void bakedMeshEquivalence() {
    setup();
    meshes["mesh"].analyticSlab = true;
    inputs["FitCutPoint"].number = 1;
    inputData.curves[0]->primitiveMatrix[3][0] = 100;
    inputs["InObject"].matrix[3][0] = 100;
    assert(compute<Node>() == MS::kSuccess && !outputData.curves.empty());
    const DCurveData expected = outputData;
    setup();
    meshes["mesh"].analyticSlab = true;
    inputs["FitCutPoint"].number = 1;
    inputData.curves[0]->primitiveMatrix[3][0] = 100;
    for (auto& p : meshes["mesh"].points)
        p.x += 100;
    assert(compute<Node>() == MS::kSuccess && outputData.curves.size() == expected.curves.size());
    for (unsigned c = 0; c < expected.curves.size(); ++c) {
        assert(outputData.curves[c]->points.size() == expected.curves[c]->points.size());
        for (unsigned p = 0; p < expected.curves[c]->points.size(); ++p)
            samePoint(outputData.curves[c]->points[p], expected.curves[c]->points[p]);
    }
}
void offsetBoundaries() {
    for (bool projective : {false, true}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = projective ? 1 : NAN;
        assert(compute<DCOffset>() == MS::kInvalidParameter && !published);
    }
    setup();
    rebuiltReportedCount = 10000001;
    assert(compute<DCOffset>() == MS::kInvalidParameter && !published);
    assert(std::find(operations.begin(), operations.end(), "curve.cvs") == operations.end());
    setup();
    inputs["Subdivide"].number = 1000001;
    assert(compute<DCOffset>() == MS::kInvalidParameter && !published);
    setup();
    inputs["Dsitance"].number = .1;
    inputs["CheckOverlap"].number = 1;
    inputs["Tolerance"].number = .01;
    // The closest-point double returns the same point: every offset is clipped.
    assert(compute<DCOffset>() == MS::kSuccess && outputData.curves.empty());
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto trace = schemaTrace("DCCutObject", DCCutObject::initialize) +
                       schemaTrace("DCReflect", DCReflect::initialize) +
                       schemaTrace("DCOffset", DCOffset::initialize) +
                       schemaTrace("DCNormal", DCNormal::initialize);
    std::ifstream file(argv[1]);
    assert(file);
    assert(trace ==
           std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
    int initialized = 0;
    for (auto entry : {std::make_pair("DCCutObject", DCCutObject::initialize),
                       std::make_pair("DCReflect", DCReflect::initialize),
                       std::make_pair("DCOffset", DCOffset::initialize),
                       std::make_pair("DCNormal", DCNormal::initialize)}) {
        reset();
        assert(entry.second() == MS::kSuccess);
        const int count = callCount;
        initialized += testFailures(entry.first, entry.second, count);
    }
    testOffset();
    testNormal();
    testCollision<DCCutObject>(false);
    testCollision<DCReflect>(true);
    int cases = 0;
    for (int mode : {0, 1, 2}) {
        setup();
        inputs["Dsitance"].number = .1;
        inputs["DirectionType"].number = mode;
        cases += failures<DCOffset>();
    }
    for (int mode = 0; mode < DC_NORMAL_GEN_TYPE_COUNT; ++mode) {
        setup();
        inputs["GenerateType"].number = mode;
        cases += failures<DCNormal>();
    }
    for (bool collide : {false, true}) {
        setup();
        meshes["mesh"].analyticSlab = collide;
        inputs["FitCutPoint"].number = 1;
        cases += failures<DCCutObject>();
        setup();
        meshes["mesh"].analyticSlab = collide;
        cases += failures<DCReflect>();
    }
    cases += multipleCurveFailures<DCCutObject>();
    cases += multipleCurveFailures<DCReflect>();
    cases += multipleCurveFailures<DCOffset>();
    cases += multipleCurveFailures<DCNormal>();
    boundaries<DCCutObject>();
    boundaries<DCReflect>();
    boundaries<DCOffset>();
    boundaries<DCNormal>();
    collisionBoundaries<DCCutObject>();
    collisionBoundaries<DCReflect>();
    bakedMeshEquivalence<DCCutObject>();
    bakedMeshEquivalence<DCReflect>();
    offsetBoundaries();
    std::cout << "Curve collision checks: " << initialized << " initializer failures, " << cases
              << " compute cases passed\n";
}
