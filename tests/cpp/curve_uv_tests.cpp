#include "curve_utility_doubles.h"
#include "common/utility/DCurveUVMapping.h"
#include "common/utility/DNumericValidation.h"

#include <array>
#include <limits>

namespace {
bool customRamp = false;
std::array<float, 3> rampSamples{{1.0f, 0.5f, 0.0f}};
}

struct UVRamp {
    UVRamp() = default;
    UVRamp(const MPlug&, MStatus* s) {
        *s = api("ramp.bind");
    }
    void getValueAtPosition(float p, float& value, MStatus* s) {
        *s = api("ramp.sample");
        if (*s)
            value = badRamp ? NAN : customRamp
                ? rampSamples.at(static_cast<std::size_t>(std::round(p * 2.0f)))
                : 1 - p;
    }
};
#include "curve_uv.inc"

namespace {
DCurveData inputData;
void setup(int mapping = 0, int calculation = 0) {
    reset();
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    badRamp = false;
    customRamp = false;
    auto curve =
        DCItem(new DCurveInternalData(MPointArray{{0, 0, 0}, {.5, 0, 0}, {1, 0, 0}}, 1, true));
    curve->useGeometryFixture = true;
    curve->geometryFixture.points = curve->points;
    curve->geometryFixture.length = 1;
    curve->geometryFixture.start = 0;
    curve->geometryFixture.end = 1;
    curve->attributes["u"] = {9, 9, 9};
    curve->attributes["marker"] = {3, 4, 5};
    inputData.curves = {curve};
    inputs["inDC"].plugin = &inputData;
    inputs["aUVType"].number = mapping;
    inputs["aCalcType"].number = calculation;
    inputs["aUVAttributeName"].string = "u";
    inputs["aUVRange"].dvec[1] = 1;
    inputs["aScale"].number = 1;
    reset();
}
MStatus compute() {
    DCUVMap node;
    MDataBlock data;
    return node.compute(MPlug({}, DCUVMap::outDC), data);
}
void sourceUnchanged() {
    assert(inputData.curves[0]->attributes.at("u") == std::vector<float>({9, 9, 9}));
    assert(inputData.curves[0]->attributes.at("marker") == std::vector<float>({3, 4, 5}));
}

void testNumericBoundaries() {
    const double limit = std::numeric_limits<float>::max();
    const double tiny = std::numeric_limits<float>::denorm_min();
    const std::array<std::array<double, 2>, 12> ranges{{
        {{-limit, 1}}, {{-limit, -1}}, {{limit, 1}}, {{limit, -1}},
        {{1, -limit}}, {{-1, limit}}, {{-limit, limit}}, {{limit, -limit}},
        {{-tiny, tiny}}, {{-0.0, -0.0}}, {{2.25, -3.5}}, {{0.25, 0.25}}
    }};
    int valuesChecked = 0, failuresChecked = 0;
    for (int mapping : {0, 1}) for (int calculation : {0, 1}) {
        for (const auto& range : ranges) for (double scale : {-1.0, 0.5, 1.0}) {
            setup(mapping, calculation);
            inputs["aUVRange"].dvec[0] = range[0];
            inputs["aUVRange"].dvec[1] = range[1];
            inputs["aScale"].number = scale;
            inputs["aOffset"].number = 0.25;
            customRamp = mapping == 1;
            rampSamples = {{1.0f, 0.0f, 0.25f}};
            assert(compute() == MS::kSuccess && published == 1);
            const auto& actual = outputData.curves[0]->attributes.at("u");
            for (unsigned int index = 0U; index < 3U; ++index) {
                const double t = mapping == 1 ? rampSamples[index] : double(index) / 2.0;
                const double endpoint = t == 0.0 ? range[0] : t == 1.0 ? range[1] :
                    range[0] + (range[1] - range[0]) * t;
                float expected = 0.0f;
                assert(dommy::numeric::checkedFloatCast(endpoint * scale + 0.25, expected));
                assert(actual[index] == expected && std::signbit(actual[index]) == std::signbit(expected));
                ++valuesChecked;
            }
            sourceUnchanged();
        }
        setup(mapping, calculation);
        inputs["aUVRange"].dvec[0] = inputs["aUVRange"].dvec[1] = -0.0;
        inputs["aOffset"].number = -0.0;
        assert(compute() == MS::kSuccess);
        const auto& signedZeros = outputData.curves[0]->attributes.at("u");
        assert(signedZeros[0] == 0.0f && std::signbit(signedZeros[0]));
        assert(signedZeros[1] == 0.0f && !std::signbit(signedZeros[1]));
        assert(signedZeros[2] == 0.0f && std::signbit(signedZeros[2]));
        valuesChecked += 3;
        sourceUnchanged();
        setup(mapping, calculation);
        assert(compute() == MS::kSuccess);
        const auto previous = outputData.curves[0]->attributes.at("u");
        for (double invalid : {
                 std::nextafter(limit, INFINITY), -std::nextafter(limit, INFINITY),
                 1.0e100, -1.0e100}) {
            reset();
            inputs["aUVRange"].dvec[0] = inputs["aUVRange"].dvec[1] = invalid;
            assert(compute() == MS::kInvalidParameter && published == 0 && cleaned == 0);
            assert(outputData.curves[0]->attributes.at("u") == previous);
            sourceUnchanged();
            ++failuresChecked;
            reset();
            inputs["aUVRange"].dvec[0] = 0.0;
            inputs["aUVRange"].dvec[1] = 1.0;
            assert(compute() == MS::kSuccess && published == 1);
            assert(outputData.curves[0]->attributes.at("u") == previous);
        }
    }
    for (int calculation : {0, 1}) {
        setup(1, calculation);
        inputs["aUVRange"].dvec[0] = 2.25;
        inputs["aUVRange"].dvec[1] = -3.5;
        inputs["aScale"].number = 1.25;
        inputs["aOffset"].number = -0.75;
        customRamp = true;
        rampSamples = {{-0.5f, 0.25f, 1.5f}};
        assert(compute() == MS::kSuccess);
        const auto previous = outputData.curves[0]->attributes.at("u");
        for (unsigned int i = 0U; i < 3U; ++i) {
            const double expected = (2.25 + (-3.5 - 2.25) * rampSamples[i]) * 1.25 - 0.75;
            assert(previous[i] == static_cast<float>(expected));
            ++valuesChecked;
        }
        for (unsigned int failedSample = 0U; failedSample < 3U; ++failedSample) {
            for (float bad : {NAN, INFINITY, std::numeric_limits<float>::max()}) {
                reset();
                rampSamples = {{-0.5f, 0.25f, 1.5f}};
                rampSamples[failedSample] = bad;
                assert(compute() != MS::kSuccess && published == 0 && cleaned == 0);
                assert(outputData.curves[0]->attributes.at("u") == previous);
                sourceUnchanged();
                ++failuresChecked;
                reset();
                rampSamples = {{-0.5f, 0.25f, 1.5f}};
                assert(compute() == MS::kSuccess && published == 1);
                assert(outputData.curves[0]->attributes.at("u") == previous);
            }
        }
    }
    std::cout << "Curve UV numeric checks: " << valuesChecked << " boundary/golden values and "
              << failuresChecked << " failure/retry cases passed\n";
}
} // namespace
int main() {
    testNumericBoundaries();
    setup();
    assert(compute() == MS::kSuccess);
    auto trace = operations;
    for (unsigned i = 0; i < trace.size(); ++i) {
        if (trace[i] == "input inDC" || trace[i] == "plugin.read") {
            for (auto s : {MS::kFailure, MS::kInvalidParameter}) {
                setup();
                failAt = i;
                injectedStatus = s;
                assert(compute() == s && published == 0);
            }
        }
    }
    int cases = 0;
    for (int mapping : {0, 1})
        for (int calculation : {0, 1}) {
            setup(mapping, calculation);
            assert(compute() == MS::kSuccess && published == 1);
            sourceUnchanged();
            trace = operations;
            const auto& values = outputData.curves[0]->attributes.at("u");
            assert(values == std::vector<float>({float(mapping), .5f, float(1 - mapping)}));
            for (unsigned i = 0; i < trace.size(); ++i) {
                for (auto s : {MS::kFailure, MS::kInvalidParameter}) {
                    setup(mapping, calculation);
                    failAt = i;
                    injectedStatus = s;
                    assert(compute() != MS::kSuccess);
                    assert(published == (trace[i] == "clean" ? 1 : 0));
                    sourceUnchanged();
                    ++cases;
                }
                setup(mapping, calculation);
                throwAt = i;
                assert(compute() != MS::kSuccess);
                assert(published == (trace[i] == "clean" ? 1 : 0));
                sourceUnchanged();
                ++cases;
                if (trace[i] == "curve.cvs") {
                    setup(mapping, calculation);
                    shortAt = i;
                    assert(compute() != MS::kSuccess && published == 0);
                    ++cases;
                }
            }
        }
    setup(1);
    badRamp = true;
    assert(compute() != MS::kSuccess && published == 0);
    setup();
    inputData.curves.clear();
    assert(compute() == MS::kSuccess && published == 1);
    setup();
    inputs["aUVAttributeName"].string = "";
    assert(compute() == MS::kSuccess);
    assert(outputData.curves[0]->attributes.at("u") == std::vector<float>({9, 9, 9}));
    std::cout << "Curve UV checks: " << cases << " compute failure cases passed\n";
}
