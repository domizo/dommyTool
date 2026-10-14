//
//  DCurlNoiseField.cpp
//  dommyTool
//
//  Created by yuzo on 2015/03/18.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCurlNoiseField.h"

#include <maya/MVectorArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MMatrix.h>
#include <maya/MArrayDataBuilder.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFnDoubleArrayData.h>

#include <maya/MFnArrayAttrsData.h>

#include "PerlinNoise.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DStatusOperation.h"
#include "modules/point/DPoint/math/DVectorMath.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaFieldData.h"
#include "common/utility/DMayaFieldDraw.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

const double kMinimumCurlStep = 1.0e-6;
const int kMaximumPositionNoiseFrequency = noise::kMaxPerlinOctaves;

struct CurlParameters {
    double magnitude = 0.0;
    double step = 0.0;
    double amplitude = 0.0;
    double ageSpeed = 0.0;
    MVector frequency;
    MVector phaseRate;
    MVector phaseOffset;
    MVector positionScale;
    int octaves = 0;
    bool useMaxDistance = false;
    double maxDistance = 0.0;
    double attenuation = 0.0;
};

MStatus readFiniteVector(MDataBlock& block, const MObject& x, const MObject& y, const MObject& z,
                         MVector& result) {
    MVector candidate;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFiniteDoubleInput(block, x, candidate.x));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFiniteDoubleInput(block, y, candidate.y));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFiniteDoubleInput(block, z, candidate.z));
    result = candidate;
    return MS::kSuccess;
}

bool curlWorkWithinLimit(std::size_t pointCount, std::size_t ownerCount, int octaves) {
    if (pointCount > dommy::generation::kMaxGeneratedPointCount ||
        ownerCount > dommy::generation::kMaxGeneratedPointCount || ownerCount == 0U ||
        octaves < 1 || octaves > kMaximumPositionNoiseFrequency)
        return false;
    // Three phase samples per receptor, then six vector samples (18 scalar
    // samples) per owner. Bound actual octave work, not just receptor/owner pairs.
    std::size_t work = 0U;
    const std::size_t limit = dommy::generation::kMaxWorkItemCount;
    return dommy::generation::checkedProduct(ownerCount, 18U, limit, work) &&
           dommy::generation::checkedSum(work, 3U, limit, work) &&
           dommy::generation::checkedProduct(work, pointCount, limit, work) &&
           dommy::generation::checkedProduct(work, static_cast<std::size_t>(octaves), limit, work);
}

bool sampleNoiseDirection(const noise::PerlinOctave& perlin, const MVector& position,
                          MVector& result) {
    MVector candidate;
    if (!perlin.tryNoise(position.x, position.y, position.z, candidate.x) ||
        !perlin.tryNoise(position.y, position.z, position.x, candidate.y) ||
        !perlin.tryNoise(position.z, position.x, position.y, candidate.z))
        return false;
    // A legitimate zero/near-zero potential is not a failed noise evaluation.
    dommy::math::normalized3OrZero(candidate.x, candidate.y, candidate.z, result.x, result.y,
                                   result.z, 1.0e-12);
    return true;
}

bool sampleCurlPhase(const noise::PerlinOctave& perlin, const CurlParameters& parameters,
                     double age, MVector& phase) {
    MVector position;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        double animated = 0.0, offset = 0.0;
        if (!dommy::numeric::scaledProduct(
                std::array<double, 3>{{age, parameters.ageSpeed, parameters.phaseRate[axis]}},
                animated) ||
            !dommy::numeric::scaledProduct(std::array<double, 2>{{parameters.phaseOffset[axis],
                                                                  parameters.positionScale[axis]}},
                                           offset))
            return false;
        position[axis] = animated + offset;
    }
    return sampleNoiseDirection(perlin, position, phase);
}

bool sampleCurl(const noise::PerlinOctave& perlin, const CurlParameters& parameters,
                const MVector& difference, const MVector& phase, MVector& result) {
    MVector position;
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        double displacement = 0.0;
        if (!dommy::numeric::scaledProduct(
                std::array<double, 2>{{parameters.amplitude, phase[axis]}}, displacement) ||
            !dommy::numeric::scaledProduct(std::array<double, 2>{{difference[axis] + displacement,
                                                                  parameters.frequency[axis]}},
                                           position[axis]))
            return false;
    }
    MVector samples[6];
    for (unsigned int index = 0U; index < 6U; ++index) {
        MVector samplePosition = position;
        samplePosition[index % 3U] += index < 3U ? parameters.step : -parameters.step;
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::scaledProduct(
                    std::array<double, 2>{{samplePosition[axis], parameters.positionScale[axis]}},
                    samplePosition[axis]))
                return false;
        }
        if (!sampleNoiseDirection(perlin, samplePosition, samples[index]))
            return false;
    }
    const MVector& dx = samples[0];
    const MVector& dy = samples[1];
    const MVector& dz = samples[2];
    const MVector& dnx = samples[3];
    const MVector& dny = samples[4];
    const MVector& dnz = samples[5];
    result = MVector((dy.z - dny.z) - (dz.y - dnz.y), (dz.x - dnz.x) - (dx.z - dnx.z),
                     (dx.y - dnx.y) - (dy.x - dny.x));
    return dommy::maya::isFiniteVector(result);
}

} // namespace

MObject DCurlNoiseField::curlSpeed;
MObject DCurlNoiseField::curlAmplitude;
MObject DCurlNoiseField::curlAgeSpeed;

MObject DCurlNoiseField::noiseFrequency;
MObject DCurlNoiseField::noiseFrequencyX;
MObject DCurlNoiseField::noiseFrequencyY;
MObject DCurlNoiseField::noiseFrequencyZ;

MObject DCurlNoiseField::phaseRate;
MObject DCurlNoiseField::phaseRateX;
MObject DCurlNoiseField::phaseRateY;
MObject DCurlNoiseField::phaseRateZ;

MObject DCurlNoiseField::phaseOffset;
MObject DCurlNoiseField::phaseOffsetX;
MObject DCurlNoiseField::phaseOffsetY;
MObject DCurlNoiseField::phaseOffsetZ;

MObject DCurlNoiseField::positionNoiseFrequency;
MObject DCurlNoiseField::positionNoiseScale;
MObject DCurlNoiseField::positionNoiseScaleX;
MObject DCurlNoiseField::positionNoiseScaleY;
MObject DCurlNoiseField::positionNoiseScaleZ;

MTypeId DCurlNoiseField::id(0x0400A);

void* DCurlNoiseField::creator() {
    return ::dommy::maya::createObject("DCurlNoiseField", [&]() {
        return new DCurlNoiseField;
    });
}

MStatus DCurlNoiseField::initialize() {
    MStatus status;

    MFnNumericAttribute nAttr;

    // Preserve the scene schema, but stop on the first failed Maya operation.
    curlSpeed = nAttr.create("curlSpeed", "cs", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.8));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(kMinimumCurlStep));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    curlAmplitude = nAttr.create("curlAmplitude", "ca", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    curlAgeSpeed = nAttr.create("curlAgeSpeed", "cas", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    noiseFrequencyX = nAttr.create("noiseFrequencyX", "nfx", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    noiseFrequencyY = nAttr.create("noiseFrequencyY", "nfy", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    noiseFrequencyZ = nAttr.create("noiseFrequencyZ", "nfz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    noiseFrequency = nAttr.create("noiseFrequency", "nf", noiseFrequencyX, noiseFrequencyY,
                                  noiseFrequencyZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.4, 0.4, 0.4));

    phaseRateX = nAttr.create("phaseRateX", "prx", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    phaseRateY = nAttr.create("phaseRateY", "pry", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    phaseRateZ = nAttr.create("phaseRateZ", "prz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    phaseRate = nAttr.create("phaseRate", "pr", phaseRateX, phaseRateY, phaseRateZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.4, 0.4, 0.4));

    phaseOffsetX = nAttr.create("phaseOffsetX", "pox", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    phaseOffsetY = nAttr.create("phaseOffsetY", "poy", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    phaseOffsetZ = nAttr.create("phaseOffsetZ", "poz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    phaseOffset =
        nAttr.create("phaseOffset", "po", phaseOffsetX, phaseOffsetY, phaseOffsetZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));

    positionNoiseFrequency =
        nAttr.create("positionNoiseFrequency", "pnf", MFnNumericData::kInt, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(kMaximumPositionNoiseFrequency));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));

    positionNoiseScaleX =
        nAttr.create("positionNoiseScaleX", "pnx", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    positionNoiseScaleY =
        nAttr.create("positionNoiseScaleY", "pny", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    positionNoiseScaleZ =
        nAttr.create("positionNoiseScaleZ", "pnz", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    positionNoiseScale = nAttr.create("positionNoiseScale", "pns", positionNoiseScaleX,
                                      positionNoiseScaleY, positionNoiseScaleZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5, 0.5, 0.5));

    // add attribute
    DOMMY_RETURN_IF_FAILURE(addAttribute(curlSpeed));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curlAmplitude));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curlAgeSpeed));
    DOMMY_RETURN_IF_FAILURE(addAttribute(noiseFrequency));
    DOMMY_RETURN_IF_FAILURE(addAttribute(phaseRate));
    DOMMY_RETURN_IF_FAILURE(addAttribute(phaseOffset));

    DOMMY_RETURN_IF_FAILURE(addAttribute(positionNoiseFrequency));
    DOMMY_RETURN_IF_FAILURE(addAttribute(positionNoiseScale));

    // Declare both compound parents and children for direct child-plug edits.
    const MObject affectingAttributes[] = {
        curlSpeed, curlAmplitude, curlAgeSpeed,
        noiseFrequency, noiseFrequencyX, noiseFrequencyY, noiseFrequencyZ,
        phaseRate, phaseRateX, phaseRateY, phaseRateZ,
        phaseOffset, phaseOffsetX, phaseOffsetY, phaseOffsetZ,
        positionNoiseFrequency,
        positionNoiseScale, positionNoiseScaleX, positionNoiseScaleY, positionNoiseScaleZ
    };
    for (std::size_t index = 0;
         index < sizeof(affectingAttributes) / sizeof(affectingAttributes[0]); ++index) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(affectingAttributes[index], mOutputForce));
    }

    return MS::kSuccess;
}

MStatus DCurlNoiseField::compute(const MPlug& plug, MDataBlock& block) try {
    if (plug != mOutputForce) return MS::kUnknownParameter;
    MStatus status;
    const unsigned int logicalIndex = plug.logicalIndex(&status);
    DOMMY_RETURN_IF_FAILURE(status);

    dommy::maya::FieldInputArrays receptors;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFieldInputs(
        block, logicalIndex, mInputData, mInputPositions, mInputVelocities,
        mInputMass, receptors));
    const MVectorArray& points = receptors.positions;
    if (receptors.velocities.length() != points.length()) return MS::kInvalidParameter;

    // Age is optional per-particle data; absent/non-finite entries use zero.
    MArrayDataHandle perParticle = block.inputArrayValue(mInputPPData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(perParticle.jumpToElement(logicalIndex));
    MDataHandle particleData = perParticle.inputValue(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    MFnArrayAttrsData attributes(particleData.data(), &status);
    DOMMY_RETURN_IF_FAILURE(status);

    MDoubleArray ages;
    DOMMY_RETURN_IF_FAILURE(ages.setLength(points.length()));
    for (unsigned int index = 0U; index < ages.length(); ++index) ages[index] = 0.0;
    MFnArrayAttrsData::Type kind = MFnArrayAttrsData::kDoubleArray;
    const MString ageName("age");
    const bool hasAge = attributes.checkArrayExist(ageName, kind, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (hasAge) {
        if (kind != MFnArrayAttrsData::kDoubleArray) return MS::kInvalidParameter;
        const MDoubleArray source = attributes.getDoubleData(ageName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const unsigned int count = std::min(points.length(), source.length());
        for (unsigned int index = 0U; index < count; ++index)
            ages[index] = std::isfinite(source[index]) ? source[index] : 0.0;
    }

    MVectorArray forces;
    DOMMY_RETURN_IF_FAILURE(calculateForces(block, points, ages, forces));
    if (forces.length() != points.length()) return MS::kFailure;
    return dommy::maya::publishFieldForces(
        block, plug, mOutputForce, logicalIndex, forces);
}
DOMMY_COMPUTE_CATCH("DCurlNoiseField")

MStatus DCurlNoiseField::calculateForces(MDataBlock& block, const MVectorArray& points,
                                         const MDoubleArray& ages, MVectorArray& outputForce) {
    const unsigned int count = points.length();
    if (count > dommy::generation::kMaxGeneratedPointCount || ages.length() != count)
        return MS::kInvalidParameter;

    CurlParameters parameters;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::readFiniteDoubleInput(block, mMagnitude, parameters.magnitude));
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readFiniteDoubleInput(block, curlSpeed, parameters.step));
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::readFiniteDoubleInput(block, curlAmplitude, parameters.amplitude));
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::readFiniteDoubleInput(block, curlAgeSpeed, parameters.ageSpeed));
    DOMMY_RETURN_IF_FAILURE(readFiniteVector(block, noiseFrequencyX, noiseFrequencyY,
                                             noiseFrequencyZ, parameters.frequency));
    DOMMY_RETURN_IF_FAILURE(
        readFiniteVector(block, phaseRateX, phaseRateY, phaseRateZ, parameters.phaseRate));
    DOMMY_RETURN_IF_FAILURE(
        readFiniteVector(block, phaseOffsetX, phaseOffsetY, phaseOffsetZ, parameters.phaseOffset));
    DOMMY_RETURN_IF_FAILURE(readFiniteVector(block, positionNoiseScaleX, positionNoiseScaleY,
                                             positionNoiseScaleZ, parameters.positionScale));
    MStatus status;
    MDataHandle octaveHandle = block.inputValue(positionNoiseFrequency, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parameters.octaves = octaveHandle.asInt();
    if (parameters.step < kMinimumCurlStep || parameters.octaves < 1 ||
        parameters.octaves > kMaximumPositionNoiseFrequency)
        return MS::kInvalidParameter;
    MDataHandle distanceHandle = block.inputValue(mUseMaxDistance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    parameters.useMaxDistance = distanceHandle.asBool();
    if (parameters.useMaxDistance) {
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::readFiniteDoubleInput(block, mMaxDistance, parameters.maxDistance));
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::readFiniteDoubleInput(block, mAttenuation, parameters.attenuation));
        if (parameters.maxDistance < 0.0 || parameters.attenuation < 0.0)
            return MS::kInvalidParameter;
    }

    MVectorArray owners;
    DOMMY_RETURN_IF_FAILURE(ownerPosition(block, owners));
    if (!curlWorkWithinLimit(count, owners.length(), parameters.octaves))
        return MS::kInvalidParameter;
    MVectorArray candidate;
    DOMMY_RETURN_IF_FAILURE(candidate.setLength(count));
    const noise::PerlinOctave perlin(parameters.octaves, 1, 2.0, 2.0);
    for (unsigned int index = 0U; index < count; ++index) {
        if (!dommy::maya::isFiniteVector(points[index]) || !std::isfinite(ages[index]))
            return MS::kInvalidParameter;
        MVector force = MVector::zero;
        if (parameters.useMaxDistance && parameters.maxDistance == 0.0) {
            DOMMY_RETURN_IF_FAILURE(candidate.set(force, index));
            continue;
        }
        MVector phase;
        if (!sampleCurlPhase(perlin, parameters, ages[index], phase))
            return MS::kInvalidParameter;
        for (unsigned int offset = owners.length(); offset > 0U; --offset) {
            const MVector difference = points[index] - owners[offset - 1U];
            if (!dommy::maya::isFiniteVector(difference))
                return MS::kInvalidParameter;
            double attenuation = 1.0, falloff = 1.0;
            if (parameters.useMaxDistance) {
                const double distance =
                    std::hypot(std::hypot(difference.x, difference.y), difference.z);
                if (distance >= parameters.maxDistance)
                    continue;
                const double ratio = distance / parameters.maxDistance;
                attenuation = std::pow(1.0 - ratio, parameters.attenuation);
                falloff = falloffCurve(ratio, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!std::isfinite(attenuation) || !std::isfinite(falloff))
                    return MS::kInvalidParameter;
            }
            // Both distance modes use this same 3D vector potential. Distance
            // only scales the curl; it must not introduce a separate radial force.
            MVector curl;
            if (!sampleCurl(perlin, parameters, difference, phase, curl))
                return MS::kInvalidParameter;
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                double contribution = 0.0;
                if (!dommy::numeric::scaledProduct(
                        std::array<double, 6>{{curl[axis], parameters.magnitude, 0.5,
                                               1.0 / parameters.step, attenuation, falloff}},
                        contribution))
                    return MS::kInvalidParameter;
                force[axis] += contribution;
                if (!std::isfinite(force[axis]))
                    return MS::kInvalidParameter;
            }
        }
        DOMMY_RETURN_IF_FAILURE(candidate.set(force, index));
    }
    // Do not expose partial results if an owner, sample, or allocation failed.
    return outputForce.copy(candidate);
}

MStatus DCurlNoiseField::ownerPosition(MDataBlock& block, MVectorArray& ownerPositions) {
    MStatus status;
    MDataHandle perVertexHandle = block.inputValue(mApplyPerVertex, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MVectorArray candidate;
    if (perVertexHandle.asBool()) {
        MDataHandle ownerHandle = block.inputValue(mOwnerPosData, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MObject ownerData = ownerHandle.data();
        // A successfully read null/empty owner payload uses the field transform.
        // An API read/bind failure is not the same as an absent owner.
        if (!ownerData.isNull()) {
            MFnVectorArrayData ownerFunction(ownerData, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const MVectorArray positions = ownerFunction.array(&status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (positions.length() > dommy::generation::kMaxGeneratedPointCount)
                return MS::kInvalidParameter;
            for (unsigned int index = 0U; index < positions.length(); ++index)
                if (!dommy::maya::isFiniteVector(positions[index]))
                    return MS::kInvalidParameter;
            if (positions.length() > 0U)
                return ownerPositions.copy(positions);
        }
    } else {
        MVector centroid;
        DOMMY_RETURN_IF_FAILURE(
            readFiniteVector(block, mOwnerCentroidX, mOwnerCentroidY, mOwnerCentroidZ, centroid));
        DOMMY_RETURN_IF_FAILURE(candidate.append(centroid));
        return ownerPositions.copy(candidate);
    }

    MVector worldPosition;
    DOMMY_RETURN_IF_FAILURE(getWorldPosition(block, worldPosition));
    DOMMY_RETURN_IF_FAILURE(candidate.append(worldPosition));
    return ownerPositions.copy(candidate);
}

MStatus DCurlNoiseField::getWorldPosition(MDataBlock& block, MVector& position) {
    MStatus status;
    const MObject node = thisMObject();
    MFnDependencyNode nodeFunction(node, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MObject attribute = nodeFunction.attribute("worldMatrix", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    // Preserve the legacy first-DAG-path convention for an instanced field.
    MPlug matrixPlug(node, attribute);
    matrixPlug = matrixPlug.elementByLogicalIndex(0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle matrixHandle = block.inputValue(matrixPlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MMatrix matrix = matrixHandle.asMatrix();
    if (!dommy::maya::isFiniteMatrix(matrix))
        return MS::kInvalidParameter;
    position = MVector(matrix(3, 0), matrix(3, 1), matrix(3, 2));
    return MS::kSuccess;
}

void DCurlNoiseField::draw(M3dView& view, const MDagPath&, M3dView::DisplayStyle,
                         M3dView::DisplayStatus) try
{
    dommy::maya::drawFieldIcon(view);
}
DOMMY_DRAW_CATCH("DCurlNoiseField")
