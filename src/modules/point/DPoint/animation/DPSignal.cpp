//
//  DPSignal.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/01.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSignal.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DSignalMath.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "dpoint_ids.h"

#include "DNodeOperation.h"
#include <cmath>

#include "perlin.h"
#include "module.h"
#include "dommyMath.h"


const MTypeId DPSignal::typeId( ID_DP_SIGNAL );


MObject DPSignal::aDefaultValue;


//ramp
MObject DPSignal::aValueRamp;
MObject DPSignal::aRampValueRange;
MObject DPSignal::aTimeRange;


//adjust
MObject DPSignal::aStrength;
MObject DPSignal::aPlayMode;
MObject DPSignal::aLoopValueMode;

//noise
MObject DPSignal::aUseNoise;
MObject DPSignal::aMethodNoise;
MObject DPSignal::aSeed;
MObject DPSignal::aAmplitude;
MObject DPSignal::aFrequency;
MObject DPSignal::aOctave;
MObject DPSignal::aLacunarity;
MObject DPSignal::aPersistence;

MObject DPSignal::aIsAbsolute;

//trigonometry
MObject DPSignal::aUseTrigonometry;
MObject DPSignal::aTrigonometryFunction;
MObject DPSignal::aTrigonometryMethod;
MObject DPSignal::aTrigonometryAmplitude;
MObject DPSignal::aTrigonometryFrequency;
MObject DPSignal::aTrigonometryOffset;
MObject DPSignal::aTrigonometryAbsolute;

//time
MObject DPSignal::aTimeScale;
MObject DPSignal::aCurrentTime;

MObject DPSignal::aOutValue;



void *DPSignal::creator() {
    return ::dommy::maya::createObject("DPSignal", [&]() {
        return new DPSignal();
    });
}




MStatus DPSignal::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != aOutValue) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle handle = data.inputValue(aDefaultValue, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double defaultValue = handle.asDouble();
    handle = data.inputValue(aStrength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double strength = handle.asDouble();
    handle = data.inputValue(aPlayMode, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short playMode = handle.asShort();
    handle = data.inputValue(aLoopValueMode, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short loopValueMode = handle.asShort();
    handle = data.inputValue(aRampValueRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double2& rawRampRange = handle.asDouble2();
    const double rampMinimum = rawRampRange[0];
    const double rampMaximum = rawRampRange[1];
    handle = data.inputValue(aTimeRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double2& rawTimeRange = handle.asDouble2();
    const double timeMinimum = rawTimeRange[0];
    const double timeMaximum = rawTimeRange[1];
    handle = data.inputValue(aTimeScale, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double timeScale = handle.asDouble();
    handle = data.inputValue(aCurrentTime, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double currentTime = handle.asTime().value();

    if (playMode < 0 || playMode > 1 ||
        loopValueMode < 0 || loopValueMode > 1 ||
        !std::isfinite(defaultValue) || !std::isfinite(strength)) {
        return MS::kInvalidParameter;
    }

    dommy::signal::TimeSample timeSample;
    if (!dommy::signal::sampleTime(
            currentTime, timeScale, timeMinimum, timeMaximum,
            playMode == 1, timeSample)) {
        return MS::kInvalidParameter;
    }
    const double calculatedTime = currentTime * timeScale;

    MRampAttribute ramp(MPlug(thisMObject(), aValueRamp));
    float rampValue = 0.0f;
    if (!dommy::maya::sampleFiniteRamp(
            ramp, timeSample.position, rampValue)) {
        return MS::kFailure;
    }
    double mappedValue = 0.0;
    if (!dommy::signal::mappedRampValue(
            rampMinimum, rampMaximum, rampValue, mappedValue)) {
        return MS::kInvalidParameter;
    }

    double mappedLastValue = 0.0, completedCycles = 0.0;
    if (playMode == 1 && loopValueMode == 1) {
        float lastRampValue = 0.0f;
        if (!dommy::maya::sampleFiniteRamp(ramp, 1.0, lastRampValue) ||
            !dommy::signal::mappedRampValue(
                rampMinimum, rampMaximum, lastRampValue, mappedLastValue)) {
            return MS::kFailure;
        }
        completedCycles = timeSample.completedCycles;
    }
    double outputValue = 0.0;
    if (!dommy::signal::outputValue(defaultValue, mappedValue, mappedLastValue,
            completedCycles, strength, 1.0, outputValue)) return MS::kFailure;

    handle = data.inputValue(aUseNoise, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (handle.asBool()) {
        handle = data.inputValue(aMethodNoise, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const short method = handle.asShort();
        handle = data.inputValue(aAmplitude, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double amplitude = handle.asDouble();
        handle = data.inputValue(aSeed, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int seed = handle.asInt();
        handle = data.inputValue(aFrequency, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double frequency = handle.asDouble();
        handle = data.inputValue(aOctave, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int octave = handle.asInt();
        handle = data.inputValue(aLacunarity, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double lacunarity = handle.asDouble();
        handle = data.inputValue(aPersistence, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double persistence = handle.asDouble();
        handle = data.inputValue(aIsAbsolute, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absolute = handle.asBool();

        if (method < 0 || method > 1 || octave < 1 || octave > 30 ||
            !std::isfinite(amplitude) || !std::isfinite(frequency) ||
            frequency < 0.0 || !std::isfinite(lacunarity) ||
            lacunarity <= 0.0 || !std::isfinite(persistence)) {
            return MS::kInvalidParameter;
        }

        noise::module::Perlin noiseModule;
        noiseModule.SetSeed(seed);
        noiseModule.SetOctaveCount(octave);
        noiseModule.SetFrequency(frequency);
        noiseModule.SetLacunarity(lacunarity);
        noiseModule.SetPersistence(persistence);
        noiseModule.SetNoiseQuality(noise::QUALITY_BEST);
        double noiseValue = noiseModule.GetValue(calculatedTime / 100.0, 0.0, 0.0);
        if (absolute) noiseValue = std::fabs(noiseValue);
        if (!std::isfinite(noiseValue)) return MS::kFailure;
        const double effectiveAmplitude = absolute ? std::fabs(amplitude) : amplitude;
        if (!dommy::signal::applyModulation(outputValue, noiseValue, effectiveAmplitude, strength,
                1.0, method == 1, outputValue)) return MS::kFailure;
    }

    handle = data.inputValue(aUseTrigonometry, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (handle.asBool()) {
        handle = data.inputValue(aTrigonometryFunction, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const short function = handle.asShort();
        handle = data.inputValue(aTrigonometryMethod, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const short method = handle.asShort();
        handle = data.inputValue(aTrigonometryFrequency, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double frequency = handle.asDouble();
        handle = data.inputValue(aTrigonometryOffset, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double offset = handle.asDouble();
        handle = data.inputValue(aTrigonometryAmplitude, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double amplitude = handle.asDouble();
        handle = data.inputValue(aTrigonometryAbsolute, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absolute = handle.asBool();

        if (function < 0 || function > 2 || method < 0 || method > 1 ||
            !std::isfinite(frequency) || !std::isfinite(offset) ||
            !std::isfinite(amplitude)) {
            return MS::kInvalidParameter;
        }
        double degrees = 0.0;
        if (!dommy::numeric::checkedProductSum(std::array<double, 2>{{calculatedTime, offset}},
                std::array<double, 2>{{frequency, 1.0}}, degrees)) return MS::kInvalidParameter;
        const double radians = dommy::math::degreesToRadians(degrees);
        if (!std::isfinite(radians)) return MS::kInvalidParameter;
        double trigonometricValue = 0.0;
        if (function == 0) {
            trigonometricValue = std::sin(radians);
        } else if (function == 1) {
            trigonometricValue = std::cos(radians);
        } else {
            trigonometricValue = std::tan(radians);
        }
        if (absolute) trigonometricValue = std::fabs(trigonometricValue);
        if (!std::isfinite(trigonometricValue)) return MS::kFailure;
        const double effectiveAmplitude = absolute ? std::fabs(amplitude) : amplitude;
        if (!dommy::signal::applyModulation(outputValue, trigonometricValue, effectiveAmplitude, strength,
                1.0, method == 1, outputValue)) return MS::kFailure;
    }

    if (!std::isfinite(outputValue)) return MS::kFailure;
    MDataHandle outputHandle = data.outputValue(aOutValue, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    outputHandle.set(outputValue);
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPSignal")

MStatus DPSignal::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnUnitAttribute uniAttr;

    aDefaultValue = nAttr.create("DefaultValue", "dval", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDefaultValue));

    aValueRamp = MRampAttribute::createCurveRamp("ValueRamp", "vramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRamp));

    aRampValueRange = nAttr.create("RampValueRange", "ravr", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRampValueRange));

    aTimeRange = nAttr.create("TimeRange", "tr", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 24.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeRange));

    aStrength = nAttr.create("Strength", "strng", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStrength));

    MFnEnumAttribute enumFn;
    aPlayMode = enumFn.create("PlayMode", "plm", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Onece", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Loop", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPlayMode));

    MFnEnumAttribute loopEnumFn;
    aLoopValueMode = loopEnumFn.create("LoopValueMode", "lvm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(loopEnumFn.addField("Reset", 0));
    DOMMY_RETURN_IF_FAILURE(loopEnumFn.addField("Add", 1));
    DOMMY_RETURN_IF_FAILURE(loopEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(loopEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLoopValueMode));

    aTimeScale = nAttr.create("TimeScale", "tms", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeScale));

    aCurrentTime = uniAttr.create("CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

#pragma mark noise

    aUseNoise = nAttr.create("UseNoise", "usno", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseNoise));

    MFnEnumAttribute mnoiseEnumFn;
    aMethodNoise = mnoiseEnumFn.create("MethodNoise", "mtno", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(mnoiseEnumFn.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(mnoiseEnumFn.addField("Multiply", 1));
    DOMMY_RETURN_IF_FAILURE(mnoiseEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(mnoiseEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMethodNoise));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aAmplitude = nAttr.create("Amplitude", "amp", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitude));

    aOctave = nAttr.create("Octave", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(6));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOctave));

    aFrequency = nAttr.create("Frequency", "freq", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrequency));

    aLacunarity = nAttr.create("Lacunarity", "lac", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLacunarity));

    aPersistence = nAttr.create("Persistence", "per", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPersistence));

    aIsAbsolute = nAttr.create("IsAbsolute", "isabs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAbsolute));

#pragma mark Trigonometry

    aUseTrigonometry =
        nAttr.create("UseTrigonometry", "ustrig", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseTrigonometry));

    MFnEnumAttribute triFuncEnumFn;
    aTrigonometryFunction = triFuncEnumFn.create("TrigonometryFunction", "trifunc", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Sine", 0));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Cosine", 1));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Tangent", 2));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFunction));

    MFnEnumAttribute triMethodEnumFn;
    aTrigonometryMethod = triMethodEnumFn.create("TrigonometryMethod", "trimth", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFn.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFn.addField("Multiply", 1));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryMethod));

    aTrigonometryAmplitude =
        nAttr.create("TrigonometryAmplitude", "triamp", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAmplitude));

    aTrigonometryFrequency =
        nAttr.create("TrigonometryFrequency", "trifreq", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFrequency));

    aTrigonometryOffset =
        nAttr.create("TrigonometryOffset", "trioff", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryOffset));

    aTrigonometryAbsolute =
        nAttr.create("TrigonometryAbsolute", "triabs", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAbsolute));

#pragma mark output
    aOutValue = nAttr.create("OutValue", "oval", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDefaultValue, aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aValueRamp, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRampValueRange, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimeRange, aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStrength, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPlayMode, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLoopValueMode, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimeScale, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, aOutValue));

    // noise
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseNoise, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMethodNoise, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitude, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, aOutValue));

    // Trigonometry
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseTrigonometry, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFunction, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryMethod, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAmplitude, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFrequency, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryOffset, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAbsolute, aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAbsolute, aOutValue));

    return MS::kSuccess;
}
