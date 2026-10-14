#include "vdb_noise_doubles.h"
#include "vdb_noise_compute.inc"

using namespace dommy::vdb;
const double infinity = std::numeric_limits<double>::infinity();
const double nanValue = std::numeric_limits<double>::quiet_NaN();

noise::module::Perlin generator(const NoiseSettings& s) {
    noise::module::Perlin result;
    result.SetOctaveCount(1);
    result.SetFrequency(s.mFreq);
    result.SetLacunarity(s.mLacunarity);
    result.SetPersistence(s.mRoughness);
    result.SetNoiseQuality(noise::QUALITY_BEST);
    return result;
}
// Frozen legacy arithmetic for ordinary finite inputs only. The historical
// inclusive loop, frequency application, signal remapping and signs are intentional.
float legacy(std::array<double, 3> p, const noise::module::Perlin& g, const NoiseSettings& s,
             float multiplier) {
    float result = 0, amp = s.mAmp, freq = s.mFreq * multiplier;
    for (int i = 0; i <= s.mOctaves; ++i) {
        for (double& v : p)
            v *= freq;
        float signal = float(2.0f * g.GetValue(p[0], p[1], p[2]) - 1.0f);
        if (s.mNoiseMode > 0)
            signal = std::pow(std::abs(signal), s.mGain);
        result += signal * amp;
        freq = s.mLacunarity;
        amp *= s.mRoughness;
    }
    return s.mNoiseMode == 1 ? -result : result;
}
struct ConstantGenerator {
    double value;
    mutable int calls = 0;
    explicit ConstantGenerator(double v) : value(v) {}
    double GetValue(double, double, double) const {
        ++calls;
        return value;
    }
};
void testMath() {
    NoiseSettings s;
    assert(validNoiseSettings(s, false) && validNoiseSettings(s, true));
    for (int mode = 0; mode < 3; ++mode) {
        s.mNoiseMode = mode;
        s.mAmp = 0.75f;
        s.mGain = 1.25f;
        s.mRoughness = 0.5f;
        for (int oct : {1, 4, 8}) {
            s.mOctaves = oct;
            const auto g = generator(s);
            for (int i = -5; i <= 5; ++i) {
                const std::array<double, 3> p{{i * 0.17, i * 0.35, 1.5 - i * 0.25}};
                for (float mult : {-0.5f, 0.0f, 1.0f, 2.0f}) {
                    double value = 999;
                    assert(calculateVdbNoise(p, g, s, mult, value));
                    const double old = legacy(p, g, s, mult);
                    assert(std::abs(value - old) < 3e-5 * std::max(1.0, std::abs(old)));
                }
            }
        }
    }
    s = NoiseSettings();
    ConstantGenerator constant(0.0);
    double value = 9;
    assert(calculateVdbNoise({{0, 0, 0}}, constant, s, 1, value) && value == -2 &&
           constant.calls == 2);
    s.mOctaves = kMaxNoiseOctaves;
    constant.calls = 0;
    assert(calculateVdbNoise({{0, 0, 0}}, constant, s, 1, value) &&
           constant.calls == kMaxNoiseOctaves + 1);
    for (int oct : {-1, 0, kMaxNoiseOctaves + 1, std::numeric_limits<int>::max()}) {
        s.mOctaves = oct;
        constant.calls = 0;
        value = 9;
        assert(!calculateVdbNoise({{0, 0, 0}}, constant, s, 1, value) && constant.calls == 0 &&
               value == 9);
    }
    s = NoiseSettings();
    for (double bad : {nanValue, infinity, -infinity}) {
        value = 9;
        assert(!calculateVdbNoise({{bad, 0, 0}}, constant, s, 1, value) && value == 9);
        assert(!calculateVdbNoise({{0, 0, 0}}, constant, s, bad, value));
        assert(!calculateVdbNoise({{0, 0, 0}}, ConstantGenerator(bad), s, 1, value));
    }
    s.mNoiseMode = 1;
    s.mGain = -1;
    assert(!calculateVdbNoise({{0, 0, 0}}, ConstantGenerator(0.5), s, 1, value));
    s = NoiseSettings();
    s.mAmp = std::numeric_limits<float>::max();
    assert(calculateVdbNoise({{0, 0, 0}}, constant, s, 1, value));
    float out = 7;
    assert(!noiseOutputValue(0.0f, value, 1, 0, out) && out == 7);
    double wide = 0;
    assert(noiseOutputValue(0.0, value, 1, 0, wide) && std::isfinite(wide));
    s.mOctaves = kMaxNoiseOctaves;
    s.mRoughness = std::numeric_limits<float>::max();
    assert(!calculateVdbNoise({{0, 0, 0}}, constant, s, 1, value));
    s = NoiseSettings();
    assert(!calculateVdbNoise({{std::numeric_limits<double>::max(), 0, 0}}, constant, s, 1, value));
    assert(!calculateVdbNoise({{0, 0, 0}}, constant, s, std::numeric_limits<double>::max(), value));
    for (double bad : {nanValue, infinity, -infinity}) {
        assert(!noiseOutputValue(float(bad), 1, 1, 0, out));
        assert(!noiseOutputValue(1.0f, bad, 1, 0, out));
        assert(!noiseOutputValue(1.0f, 1, bad, 0, out));
        assert(!noiseOutputValue(1.0f, 1, 1, float(bad), out));
    }
    assert(!noiseOutputValue(1.0f, 1, -1, 0, out));
    assert(!noiseOutputValue(1.0f, 1, 2, 0, out));
    assert(noiseOutputValue(5.0f, -2, 0.5, 1, out) && out == 3.5f);

    std::size_t work = 0;
    assert(addNoiseWork(50000000U, 1, work) && work == 100000000U);
    assert(!addNoiseWork(1U, 1, work) && work == 100000000U);
    work = 1U;
    assert(!addNoiseWork(50000000U, 1, work) && work == 1U);
    assert(!addNoiseWork(std::numeric_limits<std::uint64_t>::max(), 1, work));
    assert(!addNoiseWork(0U, std::numeric_limits<int>::max(), work));

    int coordinate = 0;
    const int minimum = std::numeric_limits<int>::min(), maximum = std::numeric_limits<int>::max();
    assert(noiseMaskCoordinate(-0.75, true, coordinate) && coordinate == 0);
    assert(noiseMaskCoordinate(double(minimum), false, coordinate) && coordinate == minimum);
    assert(!noiseMaskCoordinate(double(minimum), true, coordinate));
    assert(noiseMaskCoordinate(double(minimum) + 1, true, coordinate));
    assert(noiseMaskCoordinate(double(maximum) - 0.25, true, coordinate) &&
           coordinate == maximum - 1);
    for (double bad :
         {double(maximum), double(maximum) + 1, double(minimum) - 1, nanValue, infinity})
        assert(!noiseMaskCoordinate(bad, false, coordinate));
    assert(!noiseStencilCoordinate(minimum) && !noiseStencilCoordinate(maximum));

    s = NoiseSettings();
    for (int mode = 0; mode < 4; ++mode) {
        s.mMaskMode = mode;
        for (float falloff : {0.0f, 0.5f, 2.0f}) {
            s.mFallOff = falloff;
            for (double sample : {-2.0, -0.25, 0.0, 0.25, 2.0}) {
                for (double alignment : {0.0, 0.9, 0.91, 2.0}) {
                    double weight = 9;
                    assert(noiseMaskWeight(sample, s, alignment, weight));
                    assert(weight >= 0 && weight <= 1);
                    if (mode == 2 && sample > 0)
                        assert(weight == (alignment <= 0.9 ? 1.0 : 0.0));
                    if (sample == 0)
                        assert(weight == (mode == 3 || falloff == 0 ? 1.0 : 0.0));
                }
            }
        }
    }
    s.mMaskMode = 2;
    s.mFallOff = 0.5f;
    assert(noiseMaskWeight(-0.25, s, 1, value) && value == 0.5);
    assert(!noiseMaskWeight(infinity, s, 0, value));
    assert(!noiseMaskWeight(0, s, infinity, value));
}

template <typename T = openvdb::FloatGrid> std::shared_ptr<T> grid(const std::string& name) {
    auto result = std::make_shared<T>();
    result->name = name;
    return result;
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
    publications = gridCopies = invalidReads = sampleCalls = stencilCalls = 0;
    nullCopy = false;
}
MDataBlock block(bool masked = true) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbInput"].nonnullObject = true;
    data.inputs["maskVdbInput"].payload = masked ? &maskData : nullptr;
    data.inputs["maskVdbInput"].nonnullObject = masked;
    data.inputs["frequency"].number = 2;
    data.inputs["lacunarity"].number = 2;
    data.inputs["roughness"].number = 1;
    data.inputs["amplitude"].number = 1;
    data.inputs["gain"].number = 1;
    data.inputs["surfaceOffset"].number = 0;
    data.inputs["noiseOffset"].vector = MFloatVector();
    data.inputs["octaves"].integer = 1;
    data.inputs["noiseMode"].integer = 0;
    data.inputs["maskMode"].integer = 2;
    data.inputs["maskThreshold"].number = 0;
    data.inputs["maskFallOff"].number = 0;
    data.inputs["maskName"].text = "*";
    data.inputs["aVdbSelectedGridNames"].text = "0";
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBNoise().compute(MPlug(DOpenVDBNoise::aVdbOutput), data);
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> source() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(inputData.grids[0]));
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> mask() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(maskData.grids[0]));
}
template <typename T = openvdb::FloatGrid> double output() {
    return std::dynamic_pointer_cast<const T>(outputData.grids.back())->active[0];
}
void rejected(MDataBlock& data, bool beforeCopy = true) {
    assert(!compute(data) && !publications && !invalidReads);
    assert(outputData.grids.front()->getName() == "previous");
    if (beforeCopy)
        assert(!gridCopies);
}
void testCompute() {
    for (bool masked : {false, true}) {
        setup();
        auto data = block(masked);
        assert(compute(data) && publications == 1 && gridCopies == 1);
        assert(outputData.grids[0] == inputData.grids[1]);
        assert(outputData.grids.back() != inputData.grids[0]);
        assert(output() == 3 && source()->active[0] == 5 && mask()->active[0] == 5);
    }
    // Exercise the actual compute-side generator configuration and all noise
    // controls at a non-lattice point, where a lost frequency read is visible.
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        auto data = block(false);
        NoiseSettings s;
        s.mAmp = data.inputs["amplitude"].number = 1.25f;
        s.mFreq = data.inputs["frequency"].number = 3.25f;
        s.mLacunarity = data.inputs["lacunarity"].number = 1.75f;
        s.mRoughness = data.inputs["roughness"].number = 0.75f;
        s.mGain = data.inputs["gain"].number = 0.25f;
        s.mOffset = data.inputs["surfaceOffset"].number = 0.5f;
        s.mOctaves = data.inputs["octaves"].integer = 3;
        s.mNoiseMode = data.inputs["noiseMode"].integer = mode;
        data.inputs["noiseOffset"].vector = MFloatVector(0.125f, 0.25f, -0.5f);
        source()->closestPoint = {0.375, 0.25, 0.75};
        double expected = 0;
        assert(calculateVdbNoise({{0.5, 0.5, 0.25}}, generator(s), s, 1, expected));
        assert(compute(data) && std::abs(output() - (5 + expected - s.mOffset)) < 1e-5);
    }
    for (int mode = 0; mode < 4; ++mode) {
        setup();
        auto data = block();
        data.inputs["maskMode"].integer = mode;
        if (mode == 2) {
            source()->gradient = {1, 0, 0};
            mask()->gradient = {1, 0, 0};
        }
        assert(compute(data));
        assert(output() == (mode == 1 || mode == 2 ? 5 : 3));
    }
    setup();
    auto data = block();
    mask()->values.sampleActive = false;
    assert(compute(data) && output() == 3); // Inactive samples are still data.
    setup();
    data = block();
    data.inputs["maskName"].text = "absent";
    data.inputs["maskMode"].integer = -99; // Unused mask controls are not fetched.
    assert(compute(data) && output() == 3 && !sampleCalls);
    setup();
    data = block();
    maskData.grids.push_back(grid<openvdb::DoubleGrid>("ignored"));
    assert(compute(data) && output() == 3); // First selected mask is retained.
    setup();
    data = block();
    source()->active = {5, 5, 5};
    source()->coordinates = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    source()->activeCount = 3;
    assert(compute(data) && output() == 3 && sampleCalls == 3);
    // Two stencil buffers/accessors for the whole grid, not one mask allocation per value.
    assert(std::count(operations.begin(), operations.end(), "stencil.create") == 2);
    setup();
    data = block(false);
    data.inputs["aVdbSelectedGridNames"].text = "*";
    assert(compute(data) && outputData.grids.size() == 2 &&
           outputData.grids[1] == inputData.grids[1]);
    setup();
    data = block(false);
    data.inputs["aVdbSelectedGridNames"].text = "absent";
    assert(compute(data) && !gridCopies && outputData.grids == inputData.grids);
    setup();
    data = block(false);
    inputData.grids.clear();
    assert(compute(data) && outputData.grids.empty());
    setup();
    data = block();
    assert(DOpenVDBNoise().compute(MPlug(MObject{"unrelated"}), data) == MS::kUnknownParameter &&
           callCount == 0);

    // Matching doubles work, including finite mask multipliers outside float range.
    setup();
    data = block();
    inputData.grids[0] = grid<openvdb::DoubleGrid>("source");
    maskData.grids[0] = grid<openvdb::DoubleGrid>("mask");
    data.inputs["maskMode"].integer = 3;
    mask<openvdb::DoubleGrid>()->values.sample = 1e40;
    assert(compute(data) && output<openvdb::DoubleGrid>() == 3);
    for (int scenario = 0; scenario < 4; ++scenario) {
        setup();
        data = block();
        if (scenario == 0)
            maskData.grids[0] = grid<openvdb::DoubleGrid>("wrong");
        if (scenario == 1)
            maskData.grids[0] = grid<Grid>("vector");
        if (scenario == 2)
            inputData.grids[0] = grid<openvdb::DoubleGrid>("wrong");
        if (scenario == 3) {
            inputData.grids.push_back(grid<openvdb::DoubleGrid>("late"));
            data.inputs["aVdbSelectedGridNames"].text = "*";
        }
        rejected(data); // All selected types are checked before the first copy.
    }
    // The native adapter independently guards its casts as well.
    setup();
    NoiseSettings settings;
    const auto g = generator(settings);
    auto wrong = grid<openvdb::DoubleGrid>("wrong");
    assert(!applyNoise<openvdb::FloatGrid>(*wrong, g, settings, nullptr));
    assert(!applyNoise<openvdb::FloatGrid>(*source(), g, settings, wrong.get()));

    for (int scenario = 0; scenario < 6; ++scenario) {
        setup();
        data = block();
        MPxData wrongData;
        if (scenario == 0)
            inputData.valid = false;
        if (scenario == 1)
            maskData.valid = false;
        if (scenario == 2)
            inputData.grids[0] = nullptr;
        if (scenario == 3)
            maskData.grids[0] = nullptr;
        if (scenario == 4)
            data.inputs["aVdbInput"].payload = &wrongData;
        if (scenario == 5)
            data.inputs["maskVdbInput"].payload = &wrongData;
        rejected(data);
    }
    for (const char* name : {"aVdbInput", "maskVdbInput"}) {
        setup();
        data = block();
        data.inputs[name].type = -1;
        rejected(data);
        setup();
        data = block();
        data.inputs[name].payload = nullptr;
        rejected(data);
    }
    for (const char* name : {"frequency", "lacunarity", "roughness", "amplitude", "gain",
                             "surfaceOffset", "maskThreshold", "maskFallOff"}) {
        for (float bad : {float(nanValue), float(infinity), float(-infinity)}) {
            setup();
            data = block();
            data.inputs[name].number = bad;
            rejected(data);
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        setup();
        data = block();
        data.inputs["noiseOffset"].vector.v[axis] = float(nanValue);
        rejected(data);
    }
    for (const char* name :
         {"frequency", "lacunarity", "roughness", "maskThreshold", "maskFallOff"}) {
        setup();
        data = block();
        data.inputs[name].number = -1;
        rejected(data);
    }
    for (int bad : {-1, 3}) {
        setup();
        data = block();
        data.inputs["noiseMode"].integer = bad;
        rejected(data);
    }
    for (int bad : {-1, 4}) {
        setup();
        data = block();
        data.inputs["maskMode"].integer = bad;
        rejected(data);
    }
    for (int bad : {-1, 0, 33, std::numeric_limits<int>::max()}) {
        setup();
        data = block();
        data.inputs["octaves"].integer = bad;
        rejected(data);
    }
    setup();
    data = block(false);
    source()->activeCount = 50000000U;
    assert(compute(data)); // Exact aggregate limit is allowed without a huge allocation in doubles.
    setup();
    data = block(false);
    source()->activeCount = 50000001U;
    rejected(data);
    setup();
    data = block(false);
    source()->activeCount = 25000001U;
    auto late = grid("late");
    late->activeCount = 25000000U;
    inputData.grids.push_back(late);
    data.inputs["aVdbSelectedGridNames"].text = "*";
    rejected(data);
    setup();
    data = block(false);
    data.inputs["amplitude"].number = std::numeric_limits<float>::max();
    rejected(data, false);
    assert(source()->active[0] == 5);
    setup();
    data = block();
    nullCopy = true;
    rejected(data, false);

    for (bool masked : {false, true}) {
        for (int extreme : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            setup();
            data = block(masked);
            source()->coordinates[0] = {extreme, 0, 0};
            rejected(data, false);
            assert(stencilCalls == 0);
        }
    }
    for (int mode = 0; mode < 4; ++mode) {
        for (double bad : {infinity, nanValue, 2147483648.0, 2147483647.0, -2147483649.0}) {
            setup();
            data = block();
            data.inputs["maskMode"].integer = mode;
            mask()->xform.inverseShift = {bad, 0, 0};
            rejected(data, false);
            assert(sampleCalls == 0);
        }
    }
    for (int scenario = 0; scenario < 6; ++scenario) {
        setup();
        data = block();
        if (scenario == 0)
            source()->active[0] = float(nanValue);
        if (scenario == 1)
            mask()->values.sample = float(nanValue);
        if (scenario == 2)
            source()->closestPoint = {infinity, 0, 0};
        if (scenario == 3)
            source()->gradient = {nanValue, 0, 0};
        if (scenario == 4)
            mask()->gradient = {infinity, 0, 0};
        if (scenario == 5)
            source()->xform.shift = {nanValue, 0, 0};
        rejected(data, false);
    }
    // A failure in a later selected tree discards earlier successful candidates.
    setup();
    data = block(false);
    late = grid("late");
    late->active[0] = float(nanValue);
    inputData.grids.push_back(late);
    data.inputs["aVdbSelectedGridNames"].text = "*";
    rejected(data, false);
    assert(gridCopies == 2 && source()->active[0] == 5);
}

void testFailures() {
    for (bool masked : {false, true}) {
        setup();
        auto data = block(masked);
        assert(compute(data));
        const auto calls = operations;
        assert(std::count_if(calls.begin(), calls.end(), [](const std::string& s) {
                   return s.find("input ") == 0;
               }) == (masked ? 16 : 12));
        for (std::size_t failure = 0; failure < calls.size(); ++failure) {
            for (bool throwing : {false, true}) {
                for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
                    setup();
                    data = block(masked);
                    injectedStatus = status;
                    if (throwing)
                        attributeThrowAt = int(failure);
                    else
                        failAt = int(failure);
                    const MStatus result = compute(data);
                    assert(!result && callCount == int(failure) + 1 && invalidReads == 0);
                    if (!throwing && calls[failure].find("input ") == 0)
                        assert(result == status);
                    assert(publications == (calls[failure] == "output.clean" ? 1 : 0));
                    assert(source()->active[0] == 5 && mask()->active[0] == 5);
                }
            }
        }
    }
    setup();
    auto data = block();
    assert(compute(data));
}

int main() {
    testMath();
    testCompute();
    testFailures();
    std::cout << "DVDBNoise: real Perlin math, complete compute/apply with explicit native "
                 "boundaries passed\n";
}
