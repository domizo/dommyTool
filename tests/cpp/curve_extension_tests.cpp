#include "curve_extension_doubles.h"
#include "curve_extension.inc"
#include "curve_transform_cases.h"

namespace {
DCurveData inputData;
void setup(bool separate = true, bool remove = true) {
    reset();
    assert(DCExtend::initialize() == MS::kSuccess);
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    for (int i = 0; i < 2; ++i) {
        auto c =
            DCItem(new DCurveInternalData(MPointArray{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}},
                                          MDoubleArray{0, 0, 0, 1, 1, 1}, 3, false));
        c->attributes["marker"] = {1, 2, 3, 4};
        inputData.curves.push_back(c);
    }
    inputs["inDC"].plugin = &inputData;
    inputs["AnotherCurve"].number = separate;
    inputs["IsDeleteOriginal"].number = remove;
    inputs["StartLength"].number = 1;
    inputs["EndLength"].number = 2;
    inputs["StartInvert"].number = 1;
    inputs["StartOffset"].number = .5;
    inputs["EndOffset"].number = .25;
    reset();
}
MStatus compute() {
    DCExtend node;
    MDataBlock data;
    return node.compute(MPlug(node.thisMObject(), node.outDC), data);
}
void near(double a, double b) {
    assert(std::abs(a - b) < 1e-6);
}
void checkInput() {
    for (const auto& c : inputData.curves) {
        assert(c->points.size() == 4 && c->degree == 3 && !c->open);
        assert(c->attributes.at("marker") == std::vector<float>({1, 2, 3, 4}));
        assert(c->knots == MDoubleArray({0, 0, 0, 1, 1, 1}));
        for (unsigned i = 0; i < 4; ++i)
            near(c->points[i].x, i);
    }
}
void testTransforms() {
    for (bool separate : {false, true})
        for (bool remove : {false, true}) {
            for (const auto& matrix : primitiveTransformCases()) {
                setup(separate, remove);
                inputData.curves[0]->setTransformMatrix(matrix);
                MMatrix other = matrix;
                other[3][0] += 10;
                inputData.curves[1]->setTransformMatrix(other);
                assert(compute() == MS::kSuccess);
                const std::size_t first = separate && !remove ? 2 : 0;
                if (separate) {
                    assert(outputData.curves.size() == first + 4);
                    for (unsigned i = 0; i < 4; ++i) {
                        const auto& c = outputData.curves[first + i];
                        assertSameMatrix(c->getTransformMatrix(), i < 2 ? matrix : other);
                        assert(c->points.size() == 2 && c->degree == 1 && c->open);
                        near(c->points.front().x, i % 2 ? 3.25 : -.5);
                        near(c->points.back().x, i % 2 ? 5.25 : -1.5);
                    }
                    if (!remove) {
                        assertSameMatrix(outputData.curves[0]->getTransformMatrix(), matrix);
                        assertSameMatrix(outputData.curves[1]->getTransformMatrix(), other);
                    }
                } else {
                    assert(outputData.curves.size() == 2);
                    for (unsigned i = 0; i < 2; ++i) {
                        assertSameMatrix(outputData.curves[i]->getTransformMatrix(),
                                         i ? other : matrix);
                        assert(outputData.curves[i]->points.size() == 6 &&
                               !outputData.curves[i]->open);
                        near(outputData.curves[i]->points.front().x, -1);
                        near(outputData.curves[i]->points.back().x, 5);
                    }
                }
                checkInput();
                assertSameMatrix(inputData.curves[0]->getTransformMatrix(), matrix);
                assertSameMatrix(inputData.curves[1]->getTransformMatrix(), other);
            }
        }
    for (bool projective : {false, true}) {
        setup();
        inputData.curves[0]->primitiveMatrix[0][3] = projective ? 1 : NAN;
        assert(compute() == MS::kInvalidParameter && published == 0);
    }
}
int failures(bool separate, bool remove) {
    setup(separate, remove);
    const MMatrix firstMatrix = primitiveTransformCases()[2];
    const MMatrix secondMatrix = primitiveTransformCases()[5];
    inputData.curves[0]->setTransformMatrix(firstMatrix);
    inputData.curves[1]->setTransformMatrix(secondMatrix);
    assert(compute() == MS::kSuccess);
    const auto trace = operations;
    int tests = 0;
    for (int i = 0; i < int(trace.size()); ++i) {
        for (auto s : {MS::kFailure, MS::kInvalidParameter}) {
            reset(i);
            injectedStatus = s;
            assert(compute() != MS::kSuccess && callCount == i + 1);
            if (trace[i] != "clean")
                assert(!published && !cleaned);
            ++tests;
        }
        reset();
        throwAt = i;
        assert(compute() != MS::kSuccess && callCount == i + 1);
        if (trace[i] != "clean")
            assert(!published && !cleaned);
        ++tests;
        if (trace[i] == "array.resize" || trace[i] == "array.copy" || trace[i] == "batch.insert") {
            reset();
            shortAt = i;
            assert(compute() != MS::kSuccess && !published);
            ++tests;
        }
        checkInput();
        assertSameMatrix(inputData.curves[0]->getTransformMatrix(), firstMatrix);
        assertSameMatrix(inputData.curves[1]->getTransformMatrix(), secondMatrix);
    }
    reset();
    assert(compute() == MS::kSuccess);
    return tests;
}
} // namespace
int main(int argc, char** argv) {
    assert(argc == 2);
    testTransforms();
    const auto trace = schemaTrace("DCStitch", DCStitch::initialize) +
                       schemaTrace("DCExtend", DCExtend::initialize) +
                       schemaTrace("DCFillBetween", DCFillBetween::initialize) +
                       schemaTrace("DCConnect", DCConnect::initialize) +
                       schemaTrace("DCReplace", DCReplace::initialize);
    std::ifstream file(argv[1]);
    assert(file);
    assert(trace ==
           std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
    int initialized = testFailures("DCStitch", DCStitch::initialize, 30) +
                      testFailures("DCExtend", DCExtend::initialize, 63) +
                      testFailures("DCFillBetween", DCFillBetween::initialize, 35) +
                      testFailures("DCConnect", DCConnect::initialize, 38) +
                      testFailures("DCReplace", DCReplace::initialize, 24);
    int cases = failures(true, true) + failures(true, false) + failures(false, false);
    setup();
    inputs["StartLength"].number = inputs["EndLength"].number = 0;
    assert(compute() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputData.curves.clear();
    assert(compute() == MS::kSuccess && outputData.curves.empty());
    setup();
    inputs["inDC"].plugin = nullptr;
    assert(compute() == MS::kInvalidParameter && !published);
    setup();
    DCExtend node;
    MDataBlock block;
    assert(node.compute(MPlug(node.thisMObject(), MObject{"unknown"}), block) ==
               MS::kUnknownParameter &&
           callCount == 0);
    std::cout << "curve extension safety: " << initialized << " initializer failures, " << cases
              << " compute cases passed\n";
}
