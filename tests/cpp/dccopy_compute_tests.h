#pragma once

// Full production compute/helpers, not a separate model of placement/indexing.
namespace {
DPData pointInput;
DCurveData curveInput;

void setupCopy(int kind, bool connected = false, bool switchUv = false, int reference = 0) {
    reset();
    assert(DCCopy::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    geometries["source"] = Geometry();
    geometries["target"] = Geometry();
    geometries["reference"] = Geometry();
    pointInput = DPData();
    pointInput.points = {{{10, 20, 30}}, {{11, 21, 31}}, {{12, 22, 32}}};
    pointInput.attributes["N"] = MVectorArray(3, MVector(0, 1, 0));
    pointInput.attributes["T"] = MVectorArray(3, MVector(1, 0, 0));
    curveInput = DCurveData();
    curveInput.curves = {
        DCItem(new DCurveInternalData(geometries["source"].points, {0, 0, 0, 1, 1, 1}, 3)),
        DCItem(new DCurveInternalData(geometries["source"].points, {0, 0, 0, 1, 1, 1}, 3))};
    const auto scalar = [](MObject a, double v) { inputs[a.name].number = v; };
    scalar(DCCopy::aCountU, 2);
    scalar(DCCopy::aCountV, 3);
    scalar(DCCopy::aConnectType, connected);
    scalar(DCCopy::aConnectUVSwitch, switchUv);
    scalar(DCCopy::aSeed, 1);
    scalar(DCCopy::aUniformScale, 1);
    scalar(DCCopy::aTangentMode, reference == 3 ? 1 : reference ? 2 : 0);
    for (unsigned axis = 0; axis < 3; ++axis)
        inputs[DCCopy::aScale.name].dvec[axis] = 1;
    inputs[DCCopy::aAimVector.name].dvec[0] = 1;
    inputs[DCCopy::aNormalVector.name].dvec[1] = 1;
    inputs[DCCopy::aTangentVector.name].dvec[1] = 1;
    inputs[DCCopy::aDPNormalAttributeName.name].string = "N";
    inputs[DCCopy::aDPTangentAttributeName.name].string = "T";
    inputs[DCCopy::inCopyObject.name].connected = true;
    auto& dp = inputs[DCCopy::inDP.name];
    dp.connected = kind == 0;
    dp.plugin = &pointInput;
    auto& dc = inputs[DCCopy::inDC.name];
    dc.connected = kind == 1;
    dc.plugin = &curveInput;
    auto& target = inputs[DCCopy::inObject.name];
    target.connected = kind >= 2;
    target.object = {"target"};
    target.type = kind == 3 ? MFnData::kNurbsSurface : MFnData::kNurbsCurve;
    auto& ref = inputs[DCCopy::inReferenceObject.name];
    ref.connected = reference == 1 || reference == 2;
    ref.object = {"reference"};
    ref.type = reference == 2 ? MFnData::kNurbsSurface : MFnData::kNurbsCurve;
    reset();
}
MStatus computeCopy() {
    DCCopy node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), DCCopy::outDC), data);
}
void assertNear(double a, double b) {
    assert(std::fabs(a - b) < 1e-10);
}

int testCopyFailures() {
    int cases = 0;
    for (int kind : {0, 1, 2, 3})
        for (bool connected : {false, true})
            for (bool switchUv : {false, true})
                for (int reference : {0, 1, 2, 3}) {
                    setupCopy(kind, connected, switchUv, reference);
                    assert(computeCopy() == MS::kSuccess && published == 1 && cleaned == 2);
                    assert(pointInput.positionCopies == 0U);
                    if (kind == 0) assert(pointInput.positionViews != 0U);
                    const auto trace = operations;
                    const int calls = callCount;
                    const auto expectedCvs = outputData.getTotalCvs();
                    assert(outputData.curves.size() == (kind == 0   ? (connected ? 1 : 3)
                                                        : kind == 1 ? (connected ? 2 : 4)
                                                        : kind == 2 ? (connected ? 1 : 2)
                                                        : connected ? (switchUv ? 3 : 2)
                                                                    : 6));
                    for (int i = 0; i < calls; ++i) {
                        for (auto failure : {MS::kFailure, MS::kInvalidParameter}) {
                            reset(i);
                            injectedStatus = failure;
                            const auto result = computeCopy();
                            assert(result != MS::kSuccess);
                            if (trace[i].find("array.") != 0 && trace[i] != "batch.insert")
                                assert(result == failure);
                            assert(callCount == i + 1);
                            if (trace[i] != "clean")
                                assert(published == 0 && cleaned == 0);
                            ++cases;
                        }
                        reset();
                        throwAt = i;
                        assert(computeCopy() != MS::kSuccess && callCount == i + 1);
                        if (trace[i] != "clean")
                            assert(published == 0 && cleaned == 0);
                        ++cases;
                        if (trace[i] == "array.copy" || trace[i] == "array.resize") {
                            reset();
                            shortAt = i;
                            assert(computeCopy() != MS::kSuccess && published == 0);
                            ++cases;
                        }
                    }
                    reset();
                    assert(computeCopy() == MS::kSuccess && published == 1 && cleaned == 2);
                    assert(outputData.getTotalCvs() == expectedCvs);
                }
    return cases;
}

void testCopyPlacement() {
    for (auto counts :
         {std::array<unsigned, 2>{{2, 3}}, {{3, 2}}, {{1, 3}}, {{3, 1}}, {{1, 1}}, {{10, 1}}})
        for (bool connected : {false, true})
            for (bool switchUv : {false, true}) {
                setupCopy(3, connected, switchUv);
                inputs[DCCopy::aCountU.name].number = counts[0];
                inputs[DCCopy::aCountV.name].number = counts[1];
                assert(computeCopy() == MS::kSuccess);
                const unsigned rows = counts[switchUv ? 1 : 0], cols = counts[switchUv ? 0 : 1];
                assert(outputData.curves.size() == (connected ? rows : rows * cols));
                const auto& source = geometries["source"].points;
                for (unsigned row = 0; row < rows; ++row) {
                    unsigned cvIndex = 0;
                    for (unsigned col = 0; col < cols; ++col) {
                        const auto& curve = outputData.curves[connected ? row : row * cols + col];
                        assert(curve->points.size() == (connected ? 4 * cols + 2 * (cols - 1) : 4));
                        const unsigned u = switchUv ? col : row, v = switchUv ? row : col;
                        const double x = 5 + 4.0 * (u + 1) / (counts[0] + 1),
                                     y = -4 + 2.0 * (v + 1) / (counts[1] + 1);
                        for (unsigned p = 0; p < 4; ++p) {
                            const unsigned repeats = 1 + (connected && col > 0 && p == 0) +
                                                     (connected && col + 1 < cols && p == 3);
                            for (unsigned r = 0; r < repeats; ++r) {
                                const auto& point = curve->points[connected ? cvIndex++ : p];
                                assertNear(point.x, source[p].x + x);
                                assertNear(point.y, source[p].y + y);
                                assertNear(point.z, source[p].z);
                            }
                        }
                    }
                }
            }
    // A lopsided grid stays linear in U*V (formerly allocated U*U).
    setupCopy(3);
    inputs[DCCopy::aCountU.name].number = 10000;
    inputs[DCCopy::aCountV.name].number = 1;
    assert(computeCopy() == MS::kSuccess && outputData.curves.size() == 10000);
    for (int kind : {1, 2}) {
        setupCopy(kind);
        assert(computeCopy() == MS::kSuccess);
        assert(std::count(operations.begin(), operations.end(), "curve.normal") ==
               (kind == 1 ? 2 : 1));
        assertNear(outputData.curves[0]->points[0].x, 5 + 4.0 / 3);
        assertNear(outputData.curves[1]->points[0].x, 5 + 8.0 / 3);
    }
    for (int kind : {0, 1, 2, 3})
        for (bool connected : {false, true}) {
            setupCopy(kind, connected);
            inputs[DCCopy::aCountU.name].number = 0;
            if (kind == 0)
                pointInput.points.clear(), pointInput.attributes.clear();
            assert(computeCopy() == MS::kSuccess && outputData.curves.empty());
        }
    setupCopy(3);
    inputs[DCCopy::aCountV.name].number = 0;
    assert(computeCopy() == MS::kSuccess && outputData.curves.empty());
}

void testCopyFrames() {
    CopyParameters p;
    p.aim = {1, 0, 0};
    p.up = {0, 1, 0};
    for (const MVector& tangent : {MVector(1, 0, 0), MVector(0, 1, 0), MVector(0, 0, 1),
                                   MVector(1e308, 1e308, 0), MVector(1e-200, 0, 0)})
        for (const MVector& normal : {MVector(), tangent, MVector(0, 1, 1)}) {
            MMatrix matrix;
            MVector b;
            assert(makeCopyMatrix(p, {10, 20, 30}, tangent, normal, matrix, &b) == MS::kSuccess);
            assertNear(b.length(), 1);
            for (unsigned r = 0; r < 3; ++r)
                for (unsigned c = 0; c < 3; ++c) {
                    double dot = 0;
                    for (unsigned i = 0; i < 3; ++i)
                        dot += matrix[r][i] * matrix[c][i];
                    assertNear(dot, r == c ? 1 : 0);
                }
        }
    MMatrix output;
    output[3][0] = 99;
    assert(makeCopyMatrix(p, {}, MVector(), {0, 1, 0}, output) != MS::kSuccess &&
           output[3][0] == 99);
    assert(makeCopyMatrix(p, {}, MVector(1, 0, 0), {NAN, 0, 0}, output) != MS::kSuccess);
    p.up = {2, 0, 0};
    assert(makeCopyMatrix(p, {}, MVector(1, 0, 0), {0, 1, 0}, output) != MS::kSuccess);
    for (auto normal : {MVector(), MVector(1, 0, 0)}) {
        setupCopy(2);
        geometries["target"].normal = normal;
        assert(computeCopy() == MS::kSuccess);
    }
    setupCopy(2);
    geometries["target"].normal = {0, 0, 1};
    assert(computeCopy() == MS::kSuccess);
    // Source normal is used on the first sample, then transported to the second.
    for (unsigned i = 0; i < 2; ++i) {
        assertNear(outputData.curves[i]->points[1].y, 0);
        assertNear(outputData.curves[i]->points[1].z, 1);
    }
}

void testCopyInvalidAndTransforms() {
    for (int kind : {0, 1}) {
        setupCopy(kind);
        inputs[(kind == 0 ? DCCopy::inDP : DCCopy::inDC).name].plugin = nullptr;
        assert(computeCopy() != MS::kSuccess && published == 0);
    }
    setupCopy(0);
    pointInput.topologyValid = false;
    assert(computeCopy() != MS::kSuccess && published == 0);
    setupCopy(1);
    curveInput.curves.push_back(DCItem());
    assert(computeCopy() != MS::kSuccess && published == 0);
    setupCopy(0);
    pointInput.attributes["N"].pop_back();
    assert(computeCopy() != MS::kSuccess && published == 0);
    for (const char* attribute : {"inCopyObject", "inObject", "referenceObject"}) {
        setupCopy(2, false, false, 1);
        inputs[attribute].object = MObject::kNullObj;
        assert(computeCopy() != MS::kSuccess && published == 0);
        setupCopy(2, false, false, 1);
        inputs[attribute].matrix[0][0] = NAN;
        assert(computeCopy() != MS::kSuccess && published == 0);
    }
    for (int kind : {2, 3})
        for (double bad : {0.0, double(NAN), double(INFINITY)}) {
            setupCopy(kind);
            geometries["target"].end = bad;
            assert(computeCopy() != MS::kSuccess && published == 0);
        }
    setupCopy(2);
    geometries["target"].tangent = {0, 0, 0};
    assert(computeCopy() != MS::kSuccess && published == 0);
    setupCopy(3);
    inputs[DCCopy::aCountU.name].number = 10000;
    inputs[DCCopy::aCountV.name].number = 10000;
    assert(computeCopy() != MS::kSuccess && published == 0);
    setupCopy(0);
    inputs[DCCopy::inDC.name].connected = true;
    inputs[DCCopy::inDC.name].plugin = nullptr;
    assert(computeCopy() == MS::kSuccess && outputData.curves.size() == 3);
    for (int kind : {2, 3}) {
        setupCopy(kind);
        auto& target = inputs[DCCopy::inObject.name].matrix;
        target[3][0] = 10;
        target[3][1] = 20;
        target[3][2] = 30;
        auto& profile = inputs[DCCopy::inCopyObject.name].matrix;
        profile[3][0] = 1;
        profile[3][1] = 2;
        profile[3][2] = 3;
        assert(computeCopy() == MS::kSuccess);
        const auto& point = outputData.curves[0]->points[0];
        assertNear(point.x, 5 + 4.0 / 3 + 11);
        assertNear(point.y, (kind == 3 ? -3.5 : 0) + 22);
        assertNear(point.z, 33);
        assert(geometries["source"].points[0].x == 0);
    }
    setupCopy(2);
    DCCopy node;
    MDataBlock block;
    assert(node.compute(MPlug(node.thisMObject(), {"unknown"}), block) == MS::kUnknownParameter);
    assert(callCount == 0);
}

void testCopyRandom() {
    for (int kind : {0, 1, 2, 3})
        for (bool switchUv : {false, true})
            for (bool absolute : {false, true}) {
                setupCopy(kind, false, switchUv);
                geometries["source"].points = {{1, 1, 1}, {1, 2, 1}, {1, 3, 1}, {1, 4, 1}};
                assert(computeCopy() == MS::kSuccess);
                const auto baseline = outputData;
                inputs[DCCopy::aRandomScale.name].dvec[0] = 2;
                inputs[DCCopy::aRandomRotate.name].dvec[2] = 3;
                inputs[DCCopy::aIsRandScaleAbsolute.name].number = absolute;
                inputs[DCCopy::aIsRandRotateabsolute.name].number = absolute;
                reset();
                assert(computeCopy() == MS::kSuccess);
                const unsigned perGroup = kind == 0 ? 3 : kind == 3 ? 6 : 2;
                for (unsigned group = 0; group < (kind == 1 ? 2U : 1U); ++group) {
                    std::vector<double> scale, rotation;
                    dommy::random::generateValues(1 + group, absolute ? 0.0 : -2.0, 2.0, perGroup,
                                                  scale, false);
                    dommy::random::generateValues(3 + group, absolute ? 0.0 : -3.0, 3.0, perGroup,
                                                  rotation, false);
                    for (unsigned i = 0; i < perGroup; ++i) {
                        const unsigned index = group * perGroup + i;
                        assertNear(outputData.curves[index]->points[0].x,
                                   baseline.curves[index]->points[0].x + scale[i]);
                        assertNear(outputData.curves[index]->points[0].z,
                                   baseline.curves[index]->points[0].z +
                                       dommy::math::degreesToRadians(rotation[i]));
                    }
                }
                const auto separate = outputData;
                inputs[DCCopy::aConnectType.name].number = 1;
                reset();
                assert(computeCopy() == MS::kSuccess);
                const unsigned rows = kind == 3 ? (switchUv ? 3 : 2) : kind == 1 ? 2 : 1;
                const unsigned cols = static_cast<unsigned>(separate.curves.size()) / rows;
                assert(outputData.curves.size() == rows);
                for (unsigned row = 0; row < rows; ++row) {
                    unsigned cvIndex = 0;
                    for (unsigned col = 0; col < cols; ++col) {
                        for (unsigned cv = 0; cv < 4; ++cv) {
                            const unsigned repeats =
                                1 + (col > 0 && cv == 0) + (col + 1 < cols && cv == 3);
                            const auto& expected = separate.curves[row * cols + col]->points[cv];
                            for (unsigned r = 0; r < repeats; ++r) {
                                const auto& actual = outputData.curves[row]->points[cvIndex++];
                                assertNear(actual.x, expected.x);
                                assertNear(actual.y, expected.y);
                                assertNear(actual.z, expected.z);
                            }
                        }
                    }
                }
            }
}

void testDCCopy(const char* fixture) {
    const auto schema = schemaTrace("DCCopy", DCCopy::initialize);
    std::ifstream file(fixture);
    assert(file);
    const std::string frozen{std::istreambuf_iterator<char>(file), {}};
    assert(schema == frozen);
    const int initializers = testFailures("DCCopy", DCCopy::initialize, 136);
    testCopyPlacement();
    testCopyFrames();
    testCopyInvalidAndTransforms();
    testCopyRandom();
    const int failures = testCopyFailures();
    std::cout << "DCCopy: " << initializers << " initializer and " << failures
              << " full-compute failure/exception/short-array cases passed.\n";
}
} // namespace
