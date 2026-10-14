//
//  DPSignal3D.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/02.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSignal3D.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DSignalMath.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "dpoint_ids.h"

#include "DNodeOperation.h"
#include <cmath>

#include "perlin.h"
#include "module.h"
#include "dommyMath.h"


const MTypeId DPSignal3D::typeId( ID_DP_SIGNAL_3D );


MObject DPSignal3D::aDefaultValue;

//ramp
MObject DPSignal3D::aValueRampX;
MObject DPSignal3D::aValueRampY;
MObject DPSignal3D::aValueRampZ;
MObject DPSignal3D::aRampValueRange;
MObject DPSignal3D::aTimeRange;


//adjust
MObject DPSignal3D::aStrength;
MObject DPSignal3D::aUniformStrength;
MObject DPSignal3D::aPlayMode;
MObject DPSignal3D::aLoopValueMode;


//noise
MObject DPSignal3D::aUseNoise;
MObject DPSignal3D::aMethodNoise;
MObject DPSignal3D::aSeed;
MObject DPSignal3D::aAmplitude;
MObject DPSignal3D::aFrequency;
MObject DPSignal3D::aOctave;
MObject DPSignal3D::aLacunarity;
MObject DPSignal3D::aPersistence;

MObject DPSignal3D::aIsAbsoluteX;
MObject DPSignal3D::aIsAbsoluteY;
MObject DPSignal3D::aIsAbsoluteZ;

//trigonometry
MObject DPSignal3D::aUseTrigonometry;
MObject DPSignal3D::aTrigonometryFunctionX;
MObject DPSignal3D::aTrigonometryFunctionY;
MObject DPSignal3D::aTrigonometryFunctionZ;

MObject DPSignal3D::aTrigonometryMethodX;
MObject DPSignal3D::aTrigonometryMethodY;
MObject DPSignal3D::aTrigonometryMethodZ;
MObject DPSignal3D::aTrigonometryAmplitude;
MObject DPSignal3D::aTrigonometryFrequency;
MObject DPSignal3D::aTrigonometryOffset;
MObject DPSignal3D::aTrigonometryAbsoluteX;
MObject DPSignal3D::aTrigonometryAbsoluteY;
MObject DPSignal3D::aTrigonometryAbsoluteZ;

//time
MObject DPSignal3D::aTimeScale;
MObject DPSignal3D::aCurrentTime;


MObject DPSignal3D::aOutValue;


void *DPSignal3D::creator() {
    return ::dommy::maya::createObject("DPSignal3D", [&]() {
        return new DPSignal3D();
    });
}




MStatus DPSignal3D::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != aOutValue) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle handle = data.inputValue(aDefaultValue, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector defaultValue = handle.asVector();
    handle = data.inputValue(aStrength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MVector strength = handle.asVector();
    handle = data.inputValue(aUniformStrength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double uniformStrength = handle.asDouble();
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
        !std::isfinite(defaultValue.x) || !std::isfinite(defaultValue.y) ||
        !std::isfinite(defaultValue.z) || !std::isfinite(strength.x) ||
        !std::isfinite(strength.y) || !std::isfinite(strength.z) ||
        !std::isfinite(uniformStrength)) {
        return MS::kInvalidParameter;
    }

    dommy::signal::TimeSample timeSample;
    if (!dommy::signal::sampleTime(
            currentTime, timeScale, timeMinimum, timeMaximum,
            playMode == 1, timeSample)) {
        return MS::kInvalidParameter;
    }
    const double calculatedTime = currentTime * timeScale;

    MRampAttribute ramps[] = {
        MRampAttribute(MPlug(thisMObject(), aValueRampX)),
        MRampAttribute(MPlug(thisMObject(), aValueRampY)),
        MRampAttribute(MPlug(thisMObject(), aValueRampZ))};
    double mappedValues[3] = {0.0, 0.0, 0.0};
    for (int axis = 0; axis < 3; ++axis) {
        float rampValue = 0.0f;
        if (!dommy::maya::sampleFiniteRamp(
                ramps[axis], timeSample.position, rampValue) ||
            !dommy::signal::mappedRampValue(
                rampMinimum, rampMaximum, rampValue, mappedValues[axis])) {
            return MS::kFailure;
        }
    }

    double mappedLastValues[3] = {0.0, 0.0, 0.0}, completedCycles = 0.0;
    if (playMode == 1 && loopValueMode == 1) {
        for (int axis = 0; axis < 3; ++axis) {
            float lastRampValue = 0.0f;
            if (!dommy::maya::sampleFiniteRamp(
                    ramps[axis], 1.0, lastRampValue) ||
                !dommy::signal::mappedRampValue(
                    rampMinimum, rampMaximum, lastRampValue,
                    mappedLastValues[axis])) {
                return MS::kFailure;
            }
        }
        completedCycles = timeSample.completedCycles;
    }
    MVector outputValue;
    for (int axis = 0; axis < 3; ++axis)
        if (!dommy::signal::outputValue(defaultValue[axis], mappedValues[axis], mappedLastValues[axis],
                completedCycles, strength[axis], uniformStrength, outputValue[axis])) return MS::kFailure;

    handle = data.inputValue(aUseNoise, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (handle.asBool()) {
        handle = data.inputValue(aMethodNoise, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const short method = handle.asShort();
        handle = data.inputValue(aAmplitude, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MVector amplitude = handle.asVector();
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
        handle = data.inputValue(aIsAbsoluteX, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteX = handle.asBool();
        handle = data.inputValue(aIsAbsoluteY, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteY = handle.asBool();
        handle = data.inputValue(aIsAbsoluteZ, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteZ = handle.asBool();

        if (method < 0 || method > 1 || octave < 1 || octave > 30 ||
            !std::isfinite(amplitude.x) || !std::isfinite(amplitude.y) ||
            !std::isfinite(amplitude.z) || !std::isfinite(frequency) ||
            frequency < 0.0 || !std::isfinite(lacunarity) ||
            lacunarity <= 0.0 || !std::isfinite(persistence)) {
            return MS::kInvalidParameter;
        }

        noise::module::Perlin noiseModules[3];
        MVector noiseValue;
        for (int axis = 0; axis < 3; ++axis) {
            int axisSeed = 0;
            if (!dommy::random::offsetNonnegativeSeed(seed, static_cast<unsigned int>(axis), axisSeed))
                return MS::kInvalidParameter;
            noiseModules[axis].SetSeed(axisSeed);
            noiseModules[axis].SetOctaveCount(octave);
            noiseModules[axis].SetFrequency(frequency);
            noiseModules[axis].SetLacunarity(lacunarity);
            noiseModules[axis].SetPersistence(persistence);
            noiseModules[axis].SetNoiseQuality(noise::QUALITY_BEST);
            noiseValue[axis] =
                noiseModules[axis].GetValue(
                    calculatedTime * 0.01, 0.0, 0.0);
        }
        if (absoluteX) noiseValue.x = std::fabs(noiseValue.x);
        if (absoluteY) noiseValue.y = std::fabs(noiseValue.y);
        if (absoluteZ) noiseValue.z = std::fabs(noiseValue.z);
        if (!std::isfinite(noiseValue.x) || !std::isfinite(noiseValue.y) ||
            !std::isfinite(noiseValue.z)) {
            return MS::kFailure;
        }
        const bool absolute[] = {absoluteX, absoluteY, absoluteZ};
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::signal::applyModulation(outputValue[axis], noiseValue[axis],
                    absolute[axis] ? std::fabs(amplitude[axis]) : amplitude[axis], strength[axis],
                    uniformStrength, method == 1, outputValue[axis])) return MS::kFailure;
        }
    }

    handle = data.inputValue(aUseTrigonometry, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (handle.asBool()) {
        short functions[3] = {0, 0, 0};
        short methods[3] = {0, 0, 0};
        const MObject functionAttributes[3] = {
            aTrigonometryFunctionX, aTrigonometryFunctionY,
            aTrigonometryFunctionZ};
        const MObject methodAttributes[3] = {
            aTrigonometryMethodX, aTrigonometryMethodY,
            aTrigonometryMethodZ};
        for (int axis = 0; axis < 3; ++axis) {
            handle = data.inputValue(functionAttributes[axis], &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            functions[axis] = handle.asShort();
            handle = data.inputValue(methodAttributes[axis], &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            methods[axis] = handle.asShort();
            if (functions[axis] < 0 || functions[axis] > 2 ||
                methods[axis] < 0 || methods[axis] > 1) {
                return MS::kInvalidParameter;
            }
        }

        handle = data.inputValue(aTrigonometryFrequency, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MVector frequency = handle.asVector();
        handle = data.inputValue(aTrigonometryOffset, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MVector offset = handle.asVector();
        handle = data.inputValue(aTrigonometryAmplitude, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MVector amplitude = handle.asVector();
        handle = data.inputValue(aTrigonometryAbsoluteX, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteX = handle.asBool();
        handle = data.inputValue(aTrigonometryAbsoluteY, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteY = handle.asBool();
        handle = data.inputValue(aTrigonometryAbsoluteZ, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const bool absoluteZ = handle.asBool();

        const double finiteValues[] = {
            frequency.x, frequency.y, frequency.z,
            offset.x, offset.y, offset.z,
            amplitude.x, amplitude.y, amplitude.z};
        for (std::size_t index = 0U;
             index < sizeof(finiteValues) / sizeof(finiteValues[0]);
             ++index) {
            if (!std::isfinite(finiteValues[index])) {
                return MS::kInvalidParameter;
            }
        }

        MVector trigonometricValue;
        for (int axis = 0; axis < 3; ++axis) {
            double degrees = 0.0;
            if (!dommy::numeric::checkedProductSum(std::array<double, 2>{{calculatedTime, offset[axis]}},
                    std::array<double, 2>{{frequency[axis], 1.0}}, degrees)) return MS::kInvalidParameter;
            const double radians = dommy::math::degreesToRadians(degrees);
            if (!std::isfinite(radians)) return MS::kInvalidParameter;
            if (functions[axis] == 0) {
                trigonometricValue[axis] =
                    std::sin(radians);
            } else if (functions[axis] == 1) {
                trigonometricValue[axis] =
                    std::cos(radians);
            } else {
                trigonometricValue[axis] =
                    std::tan(radians);
            }
        }
        if (absoluteX) {
            trigonometricValue.x = std::fabs(trigonometricValue.x);
        }
        if (absoluteY) {
            trigonometricValue.y = std::fabs(trigonometricValue.y);
        }
        if (absoluteZ) {
            trigonometricValue.z = std::fabs(trigonometricValue.z);
        }
        if (!std::isfinite(trigonometricValue.x) ||
            !std::isfinite(trigonometricValue.y) ||
            !std::isfinite(trigonometricValue.z)) {
            return MS::kFailure;
        }
        const bool absolute[] = {absoluteX, absoluteY, absoluteZ};
        for (int axis = 0; axis < 3; ++axis) {
            if (!dommy::signal::applyModulation(outputValue[axis], trigonometricValue[axis],
                    absolute[axis] ? std::fabs(amplitude[axis]) : amplitude[axis], strength[axis],
                    uniformStrength, methods[axis] == 1, outputValue[axis])) return MS::kFailure;
        }
    }

    if (!std::isfinite(outputValue.x) || !std::isfinite(outputValue.y) ||
        !std::isfinite(outputValue.z)) {
        return MS::kFailure;
    }
    MDataHandle outputHandle = data.outputValue(aOutValue, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    outputHandle.set(outputValue.x, outputValue.y, outputValue.z);
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPSignal3D")

MStatus DPSignal3D::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnUnitAttribute uniAttr;

    aDefaultValue = nAttr.create("DefaultValue", "dval", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDefaultValue));

    aValueRampX = MRampAttribute::createCurveRamp("ValueRampX", "vrampx", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRampX));

    aValueRampY = MRampAttribute::createCurveRamp("ValueRampY", "vrampy", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRampY));

    aValueRampZ = MRampAttribute::createCurveRamp("ValueRampZ", "vrampz", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aValueRampZ));

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

    aStrength = nAttr.create("Strength", "strng", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStrength));

    aUniformStrength =
        nAttr.create("UniformStrength", "unistrng", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformStrength));

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

    aAmplitude = nAttr.create("Amplitude", "amp", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
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

    aIsAbsoluteX = nAttr.create("IsAbsoluteX", "isabsx", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAbsoluteX));

    aIsAbsoluteY = nAttr.create("IsAbsoluteY", "isabsy", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAbsoluteY));

    aIsAbsoluteZ = nAttr.create("IsAbsoluteZ", "isabsz", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAbsoluteZ));

#pragma mark Trigonometry

    aUseTrigonometry =
        nAttr.create("UseTrigonometry", "ustrig", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseTrigonometry));

    MFnEnumAttribute triFuncEnumFn;
    aTrigonometryFunctionX = triFuncEnumFn.create("TrigonometryFunctionX", "trifuncx", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Sine", 0));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Cosine", 1));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.addField("Tangent", 2));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFunctionX));

    MFnEnumAttribute triFuncEnumFnY;
    aTrigonometryFunctionY = triFuncEnumFnY.create("TrigonometryFunctionY", "trifuncy", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnY.addField("Sine", 0));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnY.addField("Cosine", 1));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnY.addField("Tangent", 2));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnY.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnY.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFunctionY));

    MFnEnumAttribute triFuncEnumFnZ;
    aTrigonometryFunctionZ = triFuncEnumFnZ.create("TrigonometryFunctionZ", "trifuncz", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnZ.addField("Sine", 0));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnZ.addField("Cosine", 1));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnZ.addField("Tangent", 2));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnZ.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triFuncEnumFnZ.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFunctionZ));

    MFnEnumAttribute triMethodEnumFnX;
    aTrigonometryMethodX = triMethodEnumFnX.create("TrigonometryMethodX", "trimthx", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnX.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnX.addField("Multiply", 1));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnX.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnX.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryMethodX));

    MFnEnumAttribute triMethodEnumFnY;
    aTrigonometryMethodY = triMethodEnumFnY.create("TrigonometryMethodY", "trimthy", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnY.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnY.addField("Multiply", 1));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnY.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnY.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryMethodY));

    MFnEnumAttribute triMethodEnumFnZ;
    aTrigonometryMethodZ = triMethodEnumFnZ.create("TrigonometryMethodZ", "trimthz", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnZ.addField("Add", 0));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnZ.addField("Multiply", 1));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnZ.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(triMethodEnumFnZ.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryMethodZ));

    aTrigonometryAmplitude =
        nAttr.create("TrigonometryAmplitude", "triamp", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAmplitude));

    aTrigonometryFrequency =
        nAttr.create("TrigonometryFrequency", "trifreq", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryFrequency));

    aTrigonometryOffset =
        nAttr.create("TrigonometryOffset", "trioff", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryOffset));

    aTrigonometryAbsoluteX =
        nAttr.create("TrigonometryAbsoluteX", "triabsx", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAbsoluteX));

    aTrigonometryAbsoluteY =
        nAttr.create("TrigonometryAbsoluteY", "triabsy", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAbsoluteY));

    aTrigonometryAbsoluteZ =
        nAttr.create("TrigonometryAbsoluteZ", "triabsz", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTrigonometryAbsoluteZ));

#pragma mark output
    aOutValue = nAttr.create("OutValue", "oval", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDefaultValue, aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aValueRampX, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aValueRampY, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aValueRampZ, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRampValueRange, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTimeRange, aOutValue));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStrength, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUniformStrength, aOutValue));
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

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAbsoluteX, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAbsoluteY, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAbsoluteZ, aOutValue));

    // Trigonometry
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseTrigonometry, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFunctionX, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFunctionY, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFunctionZ, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryMethodX, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryMethodY, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryMethodZ, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAmplitude, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryFrequency, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryOffset, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAbsoluteX, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAbsoluteY, aOutValue));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTrigonometryAbsoluteZ, aOutValue));

    return MS::kSuccess;
}
