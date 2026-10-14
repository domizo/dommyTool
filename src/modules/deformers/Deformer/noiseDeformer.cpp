//
//  noiseDeformer.cpp
//  dommyTool
//
//  Created by yuzo on 11/25/12.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "noiseDeformer.h"
#include "common/utility/DStatusOperation.h"
#include "perlin.h"
#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaDeformerGeometry.h"

#include <maya/MFnMesh.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MItGeometry.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include "DRandomGenerator.h"
#include "PerlinNoise.h"

#include "dommyMath.h"

#include <cmath>

namespace {

const int kMaxNoiseOctaves = noise::kMaxPerlinOctaves;

} // namespace

MTypeId noiseDeformer::typeId(0x0088A);
MString noiseDeformer::typeName("DNoise");

// Attributes
MObject noiseDeformer::seed;
MObject noiseDeformer::amplitude;
MObject noiseDeformer::ampOffset;
MObject noiseDeformer::absOn;
MObject noiseDeformer::frequency;

MObject noiseDeformer::smooth;
MObject noiseDeformer::space;

MObject noiseDeformer::enableTrasform;

MObject noiseDeformer::centerX;
MObject noiseDeformer::centerY;
MObject noiseDeformer::centerZ;
MObject noiseDeformer::center;

MObject noiseDeformer::offsetX;
MObject noiseDeformer::offsetY;
MObject noiseDeformer::offsetZ;
MObject noiseDeformer::offset;

MObject noiseDeformer::scaleX;
MObject noiseDeformer::scaleY;
MObject noiseDeformer::scaleZ;
MObject noiseDeformer::scale;

MObject noiseDeformer::isDropoff;
MObject noiseDeformer::dropoffType;
MObject noiseDeformer::dropoffRadius;
MObject noiseDeformer::attenuation;

void* noiseDeformer::creator() {
    return ::dommy::maya::createObject("noiseDeformer", [&]() {
        return new noiseDeformer();
    });
}

MStatus noiseDeformer::deform(MDataBlock& block, MItGeometry& iter, const MMatrix& localToWorld,
                              unsigned int geomIndex) try {
    MStatus stat;

    MDataHandle envHandle = block.inputValue(envelope, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0) // Deformer has no effect
        return MS::kSuccess;

    MDataHandle seedValHandle = block.inputValue(seed, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const int seedVal = seedValHandle.asInt();
    MDataHandle ampValHandle = block.inputValue(amplitude, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double ampVal = ampValHandle.asDouble();
    MDataHandle ampOffsetValHandle = block.inputValue(ampOffset, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double ampOffsetVal = ampOffsetValHandle.asDouble();
    MDataHandle absOnValHandle = block.inputValue(absOn, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const bool absOnVal = absOnValHandle.asBool();
    MDataHandle freqValHandle = block.inputValue(frequency, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const int freqVal = freqValHandle.asInt();
    MDataHandle smoothValHandle = block.inputValue(smooth, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double smoothVal = smoothValHandle.asDouble();
    MDataHandle spaceValHandle = block.inputValue(space, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double spaceVal = spaceValHandle.asDouble();
    MDataHandle enableTransformValHandle = block.inputValue(enableTrasform, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const bool enableTransformVal = enableTransformValHandle.asBool();

    MDataHandle transData = block.inputValue(center, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double3& centerData = transData.asDouble3();
    const double centerValue[3] = {centerData[0], centerData[1], centerData[2]};

    transData = block.inputValue(offset, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double3& offsetData = transData.asDouble3();
    const double offsetValue[3] = {offsetData[0], offsetData[1], offsetData[2]};

    transData = block.inputValue(scale, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double3& scaleData = transData.asDouble3();
    const double scaleValue[3] = {scaleData[0], scaleData[1], scaleData[2]};

    MDataHandle isDropoffValHandle = block.inputValue(isDropoff, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const bool isDropoffVal = isDropoffValHandle.asBool();
    MDataHandle dropoffTypeHandle = block.inputValue(dropoffType, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const DT_DROPOFF_TYPE dropoffTypeVal = dropoffTypeHandle.asInt();
    MDataHandle dropoffRadiusValHandle = block.inputValue(dropoffRadius, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double dropoffRadiusVal = dropoffRadiusValHandle.asDouble();
    MDataHandle attenuationValHandle = block.inputValue(attenuation, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double attenuationVal = attenuationValHandle.asDouble();
    const double numericValues[] = {
        ampVal,         ampOffsetVal,   smoothVal,      spaceVal,         centerValue[0],
        centerValue[1], centerValue[2], offsetValue[0], offsetValue[1],   offsetValue[2],
        scaleValue[0],  scaleValue[1],  scaleValue[2],  dropoffRadiusVal, attenuationVal};
    for (std::size_t index = 0; index < sizeof(numericValues) / sizeof(numericValues[0]); ++index) {
        if (!std::isfinite(numericValues[index])) {
            return MS::kFailure;
        }
    }
    if (freqVal <= 0 || freqVal > kMaxNoiseOctaves || dropoffRadiusVal < 0.0) {
        return MS::kFailure;
    }
    if (dropoffTypeVal < DT_DROPOFF_CONSTANT || dropoffTypeVal >= DT_DROPOFF_TYPE_COUNT ||
        attenuationVal < 0.0) {
        return MS::kFailure;
    }
    if (enableTransformVal && !dommy::maya::isFiniteMatrix(localToWorld))
        return MS::kInvalidParameter;

    MArrayDataHandle inputDataH = block.inputArrayValue(input, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(inputDataH.jumpToElement(geomIndex));
    MDataHandle iGeoHndElement = inputDataH.inputValue(&stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    MDataHandle iGeoHnd = iGeoHndElement.child(inputGeom);

    MFnMesh meshFn;
    MFnNurbsSurface surfFn;
    MFnNurbsCurve curveFn;

    if (iGeoHnd.type() == MFnData::kNurbsSurface) {
        DOMMY_RETURN_IF_FAILURE(surfFn.setObject(iGeoHnd.data()));
    } else if (iGeoHnd.type() == MFnData::kNurbsCurve) {
        DOMMY_RETURN_IF_FAILURE(curveFn.setObject(iGeoHnd.data()));
    } else if (iGeoHnd.type() == MFnData::kMesh) {
        DOMMY_RETURN_IF_FAILURE(meshFn.setObject(iGeoHnd.data()));
    } else if (iGeoHnd.type() != MFnData::kVectorArray) {
        return MS::kInvalidParameter;
    }
    noise::PerlinOctave pNoise(freqVal, seedVal, smoothVal, spaceVal);

    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int pointIndex, const MPoint& pt, MPoint& result) -> MStatus {
            const float weight = weightValue(block, geomIndex, pointIndex);
            if (!std::isfinite(weight)) {
                return MS::kFailure;
            }
            if (weight == 0.0f)
                return MS::kSuccess;

            // get point position
            double noiseVal = 0.0;
            MPoint noisePt = pt;
            if (enableTransformVal) {
                noisePt *= localToWorld;
            }
            if (!dommy::maya::isFinitePoint(noisePt)) {
                return MS::kFailure;
            }

            // Distance-based falloff around the configured center.
            double dropoffWeight = 1.0;
            if (isDropoffVal) {

                dommy::direction::Delta delta;
                if (!dommy::direction::difference(noisePt, centerValue, delta))
                    return MS::kInvalidParameter;
                dropoffWeight = dommy::deformer::radialFalloff(delta.length, dropoffRadiusVal,
                                                               dropoffTypeVal, attenuationVal);
                if (dropoffWeight <= 0.0f) {
                    return MS::kSuccess;
                }
            }
            if (!pNoise.tryNoise((noisePt.x - offsetValue[0] - centerValue[0]) * scaleValue[0],
                                 (noisePt.y - offsetValue[1] - centerValue[1]) * scaleValue[1],
                                 (noisePt.z - offsetValue[2] - centerValue[2]) * scaleValue[2],
                                 noiseVal)) {
                return MS::kInvalidParameter;
            }
            // get point normal
            MVector normVec;
            if (iGeoHnd.type() == MFnData::kNurbsSurface) {
                double uVal, vVal;

                DOMMY_RETURN_IF_FAILURE(
                    surfFn.getParamAtPoint(pt, uVal, vVal, false, MSpace::kObject, 0.1));
                if (!std::isfinite(uVal) || !std::isfinite(vVal))
                    return MS::kInvalidParameter;
                normVec = surfFn.normal(uVal, vVal, MSpace::kObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
            } else if (iGeoHnd.type() == MFnData::kMesh) {
                DOMMY_RETURN_IF_FAILURE(
                    meshFn.getVertexNormal(pointIndex, true, normVec, MSpace::kObject));
            } else if (iGeoHnd.type() == MFnData::kNurbsCurve) {
                double param = 0.0;
                curveFn.closestPoint(pt, &param, 0.001, MSpace::kObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                if (!std::isfinite(param))
                    return MS::kInvalidParameter;
                MVector cnorm = curveFn.normal(param, MSpace::kObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                MVector ctangent = curveFn.tangent(param, MSpace::kObject, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                normVec = cnorm.rotateBy(
                    ctangent.x, ctangent.y, ctangent.z,
                    dommy::math::degreesToRadians(360.0 * std::remainder(noiseVal, 1.0)));

            } else if (iGeoHnd.type() == MFnData::kVectorArray) { // particle
                const unsigned int directionSeed = dommy::math::randhash(
                    static_cast<unsigned int>(seedVal) ^ dommy::math::randhash(geomIndex) ^
                    dommy::math::randhash(pointIndex));
                dommy::math::RandomGenerator directionGenerator(directionSeed);
                normVec.x = directionGenerator.uniform(-1.0, 1.0);
                normVec.y = directionGenerator.uniform(-1.0, 1.0);
                normVec.z = directionGenerator.uniform(-1.0, 1.0);
            } else {
                normVec.x = 0.0;
                normVec.y = 1.0;
                normVec.z = 0.0;
            }
            if (!dommy::deformer::normalizeVector(normVec))
                return MS::kInvalidParameter;

            if (absOnVal && (noiseVal < 0.0)) {
                noiseVal *= -1.0;
            }
            return dommy::deformer::offsetPoint(pt, normVec, ampVal, noiseVal + ampOffsetVal,
                                                weight, env, dropoffWeight, result)
                       ? MS::kSuccess
                       : MS::kInvalidParameter;
        });
}
DOMMY_DEFORMER_CATCH("noiseDeformer")

MStatus noiseDeformer::initialize() try {
    MStatus status;
    MFnNumericAttribute nAttr;

    seed = nAttr.create("seed", "se", MFnNumericData::kInt, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    amplitude = nAttr.create("amplitude", "am", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    ampOffset = nAttr.create("ampOffset", "aof", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    absOn = nAttr.create("absOn", "aon", MFnNumericData::kBoolean, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    frequency = nAttr.create("frequency", "fr", MFnNumericData::kInt, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(kMaxNoiseOctaves));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    smooth = nAttr.create("smooth", "sm", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    space = nAttr.create("space", "sp", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    enableTrasform = nAttr.create("enableTrasform", "et", MFnNumericData::kBoolean, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    centerX = nAttr.create("centerX", "cx", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    centerY = nAttr.create("centerY", "cy", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    centerZ = nAttr.create("centerZ", "cz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    center = nAttr.create("center", "c", centerX, centerY, centerZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));

    offsetX = nAttr.create("offsetX", "ox", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetY = nAttr.create("offsetY", "oy", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    offsetZ = nAttr.create("offsetZ", "oz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    offset = nAttr.create("offset", "o", offsetX, offsetY, offsetZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));

    scaleX = nAttr.create("scaleX", "sx", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleY = nAttr.create("scaleY", "sy", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    scaleZ = nAttr.create("scaleZ", "sz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    scale = nAttr.create("scale", "s", scaleX, scaleY, scaleZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));

    isDropoff = nAttr.create("isDropoff", "ido", MFnNumericData::kBoolean, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    MFnEnumAttribute enumFn;
    dropoffType = enumFn.create("dropoffType", "dot", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Constant", DT_DROPOFF_CONSTANT));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Linear", DT_DROPOFF_LINEAR));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Quadratic", DT_DROPOFF_QUADRATIC));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));

    dropoffRadius = nAttr.create("dropoffRadius", "dor", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    attenuation = nAttr.create("attenuation", "a", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    // add attribute
    DOMMY_RETURN_IF_FAILURE(addAttribute(seed));
    DOMMY_RETURN_IF_FAILURE(addAttribute(amplitude));
    DOMMY_RETURN_IF_FAILURE(addAttribute(ampOffset));
    DOMMY_RETURN_IF_FAILURE(addAttribute(absOn));
    DOMMY_RETURN_IF_FAILURE(addAttribute(frequency));
    DOMMY_RETURN_IF_FAILURE(addAttribute(smooth));
    DOMMY_RETURN_IF_FAILURE(addAttribute(space));

    DOMMY_RETURN_IF_FAILURE(addAttribute(enableTrasform));

    DOMMY_RETURN_IF_FAILURE(addAttribute(center));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
    DOMMY_RETURN_IF_FAILURE(addAttribute(scale));

    DOMMY_RETURN_IF_FAILURE(addAttribute(isDropoff));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dropoffType));
    DOMMY_RETURN_IF_FAILURE(addAttribute(dropoffRadius));
    DOMMY_RETURN_IF_FAILURE(addAttribute(attenuation));

    // set affectAttribute
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::seed, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::amplitude, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::ampOffset, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::absOn, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::frequency, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::smooth, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::space, noiseDeformer::outputGeom));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::enableTrasform, noiseDeformer::outputGeom));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::centerX, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::centerY, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::centerZ, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::center, noiseDeformer::outputGeom));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::offsetX, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::offsetY, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::offsetZ, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::offset, noiseDeformer::outputGeom));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::scaleX, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::scaleY, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::scaleZ, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::scale, noiseDeformer::outputGeom));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::isDropoff, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::dropoffRadius, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::attenuation, noiseDeformer::outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(noiseDeformer::dropoffType, noiseDeformer::outputGeom));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "noiseDeformer", "initialize", std::current_exception());
}
