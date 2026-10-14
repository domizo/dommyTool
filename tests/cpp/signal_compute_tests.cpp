#include "signal_compute_doubles.h"
#include "signal_compute_methods.inc"

namespace {
MDataBlock configure(bool vector, bool features = false) {
    resetCalls(); throwAt = -1; published = cleaned = 0;
    output = {{91.0,92.0,93.0}};
    ramps = {{"aValueRamp", {{0.5f,1.0f}}}, {"aValueRampX", {{0.5f,1.0f}}},
             {"aValueRampY", {{0.5f,1.0f}}}, {"aValueRampZ", {{0.5f,1.0f}}}};
    MDataBlock block;
    auto& in = block.inputs;
    in["aDefaultValue"].number = 2.0; in["aDefaultValue"].vector = {2,2,2};
    in["aStrength"].number = 3.0; in["aStrength"].vector = {3,3,3};
    in["aUniformStrength"].number = 4.0;
    in["aRampValueRange"].range[0] = 2.0; in["aRampValueRange"].range[1] = 6.0;
    in["aTimeRange"].range[1] = 24.0;
    in["aCurrentTime"].number = 12.0; in["aTimeScale"].number = 1.0;
    in["aUseNoise"].number = in["aUseTrigonometry"].number = features;
    in["aAmplitude"].number = 0.5; in["aAmplitude"].vector = {0.5,0.5,0.5};
    in["aOctave"].number = 6.0; in["aFrequency"].number = 2.0;
    in["aLacunarity"].number = 2.0; in["aPersistence"].number = 0.5;
    in["aTrigonometryFrequency"].number = 1.0; in["aTrigonometryFrequency"].vector = {1,1,1};
    in["aTrigonometryAmplitude"].number = 0.25; in["aTrigonometryAmplitude"].vector = {0.25,0.25,0.25};
    (void)vector;
    return block;
}
MStatus evaluate(bool vector, MDataBlock& block, bool unrelated = false) {
    if (vector) {
        DPSignal3D node; return node.compute(MPlug({"node"}, unrelated ? MObject{"other"} : DPSignal3D::aOutValue), block);
    }
    DPSignal node; return node.compute(MPlug({"node"}, unrelated ? MObject{"other"} : DPSignal::aOutValue), block);
}
void checkOutput(bool vector, double expected) {
    for (unsigned i = 0; i < (vector ? 3U : 1U); ++i)
        assert(std::fabs(output[i] - expected) <= std::fabs(expected)*2e-14 + 1e-14);
    assert(published == 1 && cleaned == 2);
}
void ordinaryModulation(bool vector) {
    for (bool multiply : {false,true}) for (bool absolute : {false,true}) {
        auto block = configure(vector, true);
        block.inputs["aMethodNoise"].number = block.inputs["aTrigonometryMethod"].number = multiply;
        block.inputs["aIsAbsolute"].number = block.inputs["aTrigonometryAbsolute"].number = absolute;
        block.inputs["aAmplitude"].number = -0.5; block.inputs["aAmplitude"].vector = {-0.5,-0.5,-0.5};
        block.inputs["aTrigonometryAmplitude"].number = -0.25;
        block.inputs["aTrigonometryAmplitude"].vector = {-0.25,-0.25,-0.25};
        for (const char* name : {"aTrigonometryMethodX","aTrigonometryMethodY","aTrigonometryMethodZ"})
            block.inputs[name].number = multiply;
        for (const char* name : {"aIsAbsoluteX","aIsAbsoluteY","aIsAbsoluteZ",
                                 "aTrigonometryAbsoluteX","aTrigonometryAbsoluteY","aTrigonometryAbsoluteZ"})
            block.inputs[name].number = absolute;
        assert(evaluate(vector, block));
        for (unsigned axis = 0; axis < (vector ? 3U : 1U); ++axis) {
            noise::module::Perlin noise; noise.SetSeed(int(axis)); noise.SetOctaveCount(6);
            noise.SetFrequency(2); noise.SetLacunarity(2); noise.SetPersistence(0.5);
            noise.SetNoiseQuality(noise::QUALITY_BEST);
            double nv = noise.GetValue(vector ? 12.0*0.01 : 12.0/100.0,0,0)*-0.5;
            double tv = std::sin(dommy::math::degreesToRadians(12.0))*-0.25;
            if (absolute) {nv=std::fabs(nv);tv=std::fabs(tv);}
            const double uniform = vector ? 4.0 : 1.0;
            double expected = 2.0+4.0*3.0*uniform;
            if (multiply) {expected*=nv*3.0*uniform;expected*=tv*3.0*uniform;}
            else {expected+=nv*3.0*uniform;expected+=tv*3.0*uniform;}
            assert(std::fabs(output[axis]-expected) <= std::fabs(expected)*2e-14+1e-14);
        }
    }
}
void extremeCases(bool vector) {
    auto block = configure(vector);
    assert(evaluate(vector, block)); checkOutput(vector, vector ? 50.0 : 14.0);
    block = configure(vector); block.inputs["aPlayMode"].number = 1;
    block.inputs["aLoopValueMode"].number = 1; block.inputs["aCurrentTime"].number = 36.0;
    assert(evaluate(vector, block)); checkOutput(vector, vector ? 122.0 : 32.0);

    std::feclearexcept(FE_ALL_EXCEPT);
    block = configure(vector);
    block.inputs["aDefaultValue"].number = -1e308; block.inputs["aDefaultValue"].vector = {-1e308,-1e308,-1e308};
    block.inputs["aStrength"].number = 2.0; block.inputs["aStrength"].vector = {2,2,2};
    block.inputs["aUniformStrength"].number = 1.0;
    block.inputs["aRampValueRange"].range[0] = block.inputs["aRampValueRange"].range[1] = 1e308;
    assert(evaluate(vector, block)); checkOutput(vector, 1e308);
    block = configure(vector);
    block.inputs["aDefaultValue"].number = 0.0; block.inputs["aDefaultValue"].vector = {};
    block.inputs["aStrength"].number = 2.0; block.inputs["aStrength"].vector = {2,2,2};
    block.inputs["aUniformStrength"].number = 2.0;
    block.inputs["aRampValueRange"].range[0] = -1e308; block.inputs["aRampValueRange"].range[1] = 1e308;
    block.inputs["aPlayMode"].number = block.inputs["aLoopValueMode"].number = 1;
    block.inputs["aCurrentTime"].number = 36.0;
    for (auto& ramp : ramps) ramp.second[0] = 0.0f;
    assert(evaluate(vector, block)); checkOutput(vector, 0.0);

    block = configure(vector);
    block.inputs["aTimeRange"].range[0] = -1e308; block.inputs["aTimeRange"].range[1] = 1e308;
    block.inputs["aCurrentTime"].number = 0.0;
    block.inputs["aRampValueRange"].range[0] = -1e308; block.inputs["aRampValueRange"].range[1] = 1e308;
    assert(evaluate(vector, block)); checkOutput(vector, 2.0);

    for (bool multiply : {false, true}) {
        block = configure(vector, true);
        block.inputs["aMethodNoise"].number = multiply;
        block.inputs["aTrigonometryMethod"].number = multiply;
        for (const char* name : {"aTrigonometryMethodX","aTrigonometryMethodY","aTrigonometryMethodZ"})
            block.inputs[name].number = multiply;
        block.inputs["aAmplitude"].number = 1e308; block.inputs["aAmplitude"].vector = {1e308,1e308,1e308};
        block.inputs["aTrigonometryAmplitude"].number = 1e308;
        block.inputs["aTrigonometryAmplitude"].vector = {1e308,1e308,1e308};
        // Tangent amplifies the finite raw sample beyond the double range.
        block.inputs["aTrigonometryFunction"].number = 2;
        for (const char* name : {"aTrigonometryFunctionX","aTrigonometryFunctionY","aTrigonometryFunctionZ"})
            block.inputs[name].number = 2;
        block.inputs["aTrigonometryOffset"].number = 68;
        block.inputs["aTrigonometryOffset"].vector = {68,68,68};
        if (vector) block.inputs["aUniformStrength"].number = 0.0;
        else block.inputs["aStrength"].number = 0.0;
        assert(evaluate(vector, block)); checkOutput(vector, multiply ? 0.0 : 2.0);
    }
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
    block = configure(vector); block.inputs["aStrength"].number = block.inputs["aStrength"].vector.x = NAN;
    assert(!evaluate(vector, block) && published == 0 && cleaned == 0);
    block = configure(vector, true); block.inputs["aAmplitude"].number = block.inputs["aAmplitude"].vector.x = NAN;
    block.inputs["aUniformStrength"].number = 0;
    assert(!evaluate(vector, block) && published == 0 && cleaned == 0);
    block = configure(vector); block.inputs["aRampValueRange"].range[0] = block.inputs["aRampValueRange"].range[1] = 1e308;
    assert(!evaluate(vector, block) && published == 0 && cleaned == 0);
    block = configure(vector); assert(evaluate(vector, block, true) == MS::kUnknownParameter && callCount == 0);
}
void boundaries(bool vector) {
    auto block = configure(vector, true); assert(evaluate(vector, block));
    const int count = callCount; const auto baseline = operations;
    for (auto status : {MS::kFailure,MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int at = 0; at < count; ++at) {
            block = configure(vector, true); failAt = at;
            assert(!evaluate(vector, block));
            assert(callCount == at+1);
            if (baseline[at] != "data.clean") assert(published == 0 && cleaned == 0);
        }
    }
    for (int at = 0; at < count; ++at) {
        block = configure(vector, true); throwAt = at;
        assert(evaluate(vector, block) == MS::kFailure);
        if (baseline[at] != "data.clean") assert(published == 0 && cleaned == 0);
    }
    std::cout << (vector ? "DPSignal3D" : "DPSignal") << ": " << count*2
              << " status failures, " << count << " exceptions passed\n";
}
}
int main() {
    for (bool vector : {false,true}) { ordinaryModulation(vector); extremeCases(vector); boundaries(vector); }
    std::cout << "Complete Signal compute boundary/numeric tests passed\n";
}
