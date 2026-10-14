//
//  DCurveField.cpp
//  dommyTool
//
//  Created by yuzo on 3/19/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCurveField.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "DCurveFieldMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DStatusOperation.h"
#include "modules/point/DPoint/math/DVectorMath.h"

#include <maya/MArrayDataBuilder.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MCurveAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFloatArray.h>
#include <maya/MRampAttribute.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

MStatus setZeroForces(unsigned int count, MVectorArray& outputForce) {
    return dommy::maya::buildCheckedArray<MVectorArray>(
        count,
        [](unsigned int, MVector& value) -> MStatus {
            value = MVector::zero;
            return MS::kSuccess;
        },
        outputForce);
}

MStatus sampleCurveRamp(MRampAttribute& ramp, float position, float& value) {
    if (!std::isfinite(position) || position < 0.0f || position > 1.0f)
        return MS::kInvalidParameter;
    MStatus status;
    ramp.getValueAtPosition(position, value, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    return std::isfinite(value) ? MS::kSuccess : MS::kInvalidParameter;
}

MStatus finishOutput(MDataBlock& block, const MPlug& plug, unsigned int logicalIndex,
                     const MVectorArray& forces) {
    MStatus status;
    MArrayDataHandle outputArray = block.outputArrayValue(MPxFieldNode::mOutputForce, &status);
    if (!status)
        return status;
    MArrayDataBuilder outputBuilder = outputArray.builder(&status);
    if (!status)
        return status;
    MDataHandle outputHandle = outputBuilder.addElement(logicalIndex, &status);
    if (!status)
        return status;

    MFnVectorArrayData outputData;
    const MObject outputObject = outputData.create(forces, &status);
    if (!status)
        return status;
    status = outputHandle.set(outputObject);
    if (!status)
        return status;
    return block.setClean(plug);
}

} // namespace

MObject DCurveField::inputTargetCurve;

MObject DCurveField::tangentAffectDistance;

MObject DCurveField::areaRadius;
MObject DCurveField::aRadiusRamp;
MObject DCurveField::aDamping;
MObject DCurveField::aAttraction;

MObject DCurveField::aForwardScale;
MObject DCurveField::aToCurveScale;
MObject DCurveField::aForwardRamp;
MObject DCurveField::aToCurveRamp;

MObject DCurveField::aInnerMethod;
MObject DCurveField::aBlend;
MObject DCurveField::aBlendRamp;

MObject DCurveField::aIsAffectTip;

MTypeId DCurveField::id(0x0401A);

void* DCurveField::creator() {
    return ::dommy::maya::createObject("DCurveField", [&]() {
        return new DCurveField;
    });
}

void DCurveField::postConstructor() {
    dommy::maya::initializeNodeRamps("DCurveField", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::initializeDefaultCurve(thisMObject(), aRadiusRamp, {0.0f}, {1.0f}));
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::initializeDefaultCurve(thisMObject(), aForwardRamp, {0.0f}, {1.0f}));
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::initializeDefaultCurve(thisMObject(), aToCurveRamp, {0.0f}, {1.0f}));
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::initializeDefaultCurve(thisMObject(), aBlendRamp, {0.0f}, {1.0f}));
        return MS::kSuccess;
    });
}

MStatus DCurveField::initialize() {
    MStatus status;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    // Preserve the scene schema, but stop on the first failed Maya operation.

    inputTargetCurve =
        tAttr.create("inputTargetCurve", "itc", MFnData::kNurbsCurve, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setHidden(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputTargetCurve));

    tangentAffectDistance =
        nAttr.create("tangentAffectDistance", "tad", MFnNumericData::kDouble, 3.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(tangentAffectDistance));

    areaRadius = nAttr.create("areaRadius", "ard", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(areaRadius));

    aDamping = nAttr.create("Damping", "dmp", MFnNumericData::kDouble, 0.5, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));

    aAttraction = nAttr.create("Attraction", "atrct", MFnNumericData::kDouble, 0.5, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttraction));

    aRadiusRamp = MRampAttribute::createCurveRamp("RadiusRamp", "radrmp", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRamp));

    aIsAffectTip = nAttr.create("IsAffectTip", "iat", MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAffectTip));

    aForwardScale = nAttr.create("ForwardScale", "fwscl", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForwardScale));

    aToCurveScale = nAttr.create("ToCurveScale", "tcrvscl", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aToCurveScale));

    aForwardRamp = MRampAttribute::createCurveRamp("ForwardRamp", "fwrmp", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForwardRamp));
    aToCurveRamp = MRampAttribute::createCurveRamp("ToCurveRamp", "tcrmp", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aToCurveRamp));

    MFnEnumAttribute enumFn;
    aInnerMethod = enumFn.create("InnerMethod", "inmt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Liner", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Ramp Blend", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInnerMethod));

    aBlend = nAttr.create("Blend", "bld", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlend));

    aBlendRamp = MRampAttribute::createCurveRamp("BlendRamp", "blrmp", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlendRamp));

    const MObject affectingAttributes[] = {
        inputTargetCurve, tangentAffectDistance, aRadiusRamp,   aDamping,
        aAttraction,      aForwardScale,         aToCurveScale, aForwardRamp,
        aToCurveRamp,     aIsAffectTip,          aInnerMethod,  aBlend,
        aBlendRamp,
    };
    for (std::size_t index = 0U;
         index < sizeof(affectingAttributes) / sizeof(affectingAttributes[0]); ++index) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(affectingAttributes[index], mOutputForce));
    }

    return MS::kSuccess;
}

MStatus DCurveField::compute(const MPlug& plug, MDataBlock& block) try {
    if (plug != mOutputForce)
        return MS::kUnknownParameter;

    MStatus status;
    const int rawMultiIndex = plug.logicalIndex(&status);
    if (!status)
        return status;
    if (rawMultiIndex < 0)
        return MS::kFailure;
    const unsigned int multiIndex = static_cast<unsigned int>(rawMultiIndex);

    // Field inputs must be read through outputArrayValue to avoid evaluating
    // unrelated logical elements and creating a dependency-graph cycle.
    MArrayDataHandle inputArray = block.outputArrayValue(mInputData, &status);
    if (!status)
        return status;
    status = inputArray.jumpToElement(multiIndex);
    if (!status)
        return status;
    MDataHandle compound = inputArray.inputValue(&status);
    if (!status)
        return status;

    MFnVectorArrayData positionData(compound.child(mInputPositions).data(), &status);
    if (!status)
        return status;
    const MVectorArray points = positionData.array(&status);
    if (!status)
        return status;
    MFnVectorArrayData velocityData(compound.child(mInputVelocities).data(), &status);
    if (!status)
        return status;
    const MVectorArray velocities = velocityData.array(&status);
    if (!status)
        return status;
    MFnDoubleArrayData massData(compound.child(mInputMass).data(), &status);
    if (!status)
        return status;
    const MDoubleArray masses = massData.array(&status);
    if (!status)
        return status;

    const unsigned int receptorCount = points.length();
    if (static_cast<std::size_t>(receptorCount) > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kFailure;
    }

    MVectorArray forceArray;
    DOMMY_RETURN_IF_FAILURE(calculateForces(block, points, velocities, masses, forceArray));
    return finishOutput(block, plug, multiIndex, forceArray);
}
DOMMY_COMPUTE_CATCH("DCurveField")

MStatus DCurveField::calculateForces(MDataBlock& block, const MVectorArray& points,
                                     const MVectorArray& velocities, const MDoubleArray& masses,
                                     MVectorArray& outputForce) {
    const unsigned int count = points.length();
    if (count > dommy::generation::kMaxGeneratedPointCount || velocities.length() != count ||
        !dommy::field::validMassCount(count, masses.length()))
        return MS::kInvalidParameter;

    using dommy::maya::readFiniteDoubleInput;
    double magnitude = 0.0, tangentDistance = 0.0, damping = 0.0, attraction = 0.0;
    double forwardScale = 0.0, toCurveScale = 0.0, blend = 0.0;
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, mMagnitude, magnitude));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, tangentAffectDistance, tangentDistance));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, aDamping, damping));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, aAttraction, attraction));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, aForwardScale, forwardScale));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, aToCurveScale, toCurveScale));
    DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, aBlend, blend));
    if (tangentDistance < 0.0)
        return MS::kInvalidParameter;

    MStatus status;
    MDataHandle tipHandle = block.inputValue(aIsAffectTip, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool affectTip = tipHandle.asBool();
    MDataHandle methodHandle = block.inputValue(aInnerMethod, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short method = methodHandle.asShort();
    if (method < 0 || method > 1)
        return MS::kInvalidParameter;
    MDataHandle distanceHandle = block.inputValue(mUseMaxDistance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useMaxDistance = distanceHandle.asBool();
    double maxDistance = 0.0, attenuation = 0.0;
    if (useMaxDistance) {
        DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, mMaxDistance, maxDistance));
        DOMMY_RETURN_IF_FAILURE(readFiniteDoubleInput(block, mAttenuation, attenuation));
        if (maxDistance < 0.0 || attenuation < 0.0)
            return MS::kInvalidParameter;
    }

    // Invalid input is not a valid zero force, even if a later branch is inactive.
    for (unsigned int index = 0U; index < count; ++index) {
        if (!dommy::maya::isFiniteVector(points[index]) ||
            !dommy::maya::isFiniteVector(velocities[index]) ||
            !std::isfinite(masses[masses.length() == 1U ? 0U : index]))
            return MS::kInvalidParameter;
    }
    if (count == 0U || (useMaxDistance && maxDistance <= 1.0e-12))
        return setZeroForces(count, outputForce);

    MDataHandle curveHandle = block.inputValue(inputTargetCurve, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MObject curveObject = curveHandle.data();
    // Preserve the disconnected-curve no-op, not failed reads or invalid data.
    if (curveObject.isNull())
        return setZeroForces(count, outputForce);
    if (curveHandle.type() != MFnData::kNurbsCurve)
        return MS::kInvalidParameter;
    MFnNurbsCurve curve(curveObject, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MPlug radiusPlug(thisMObject(), aRadiusRamp);
    MRampAttribute radiusRamp(radiusPlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MPlug forwardPlug(thisMObject(), aForwardRamp);
    MRampAttribute forwardRamp(forwardPlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MPlug toCurvePlug(thisMObject(), aToCurveRamp);
    MRampAttribute toCurveRamp(toCurvePlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MPlug blendPlug(thisMObject(), aBlendRamp);
    MRampAttribute blendRamp(blendPlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    double startParameter = 0.0, endParameter = 0.0;
    DOMMY_RETURN_IF_FAILURE(curve.getKnotDomain(startParameter, endParameter));
    float unusedPosition = 0.0f;
    if (!dommy::field::normalizeRange(startParameter, startParameter, endParameter, unusedPosition))
        return MS::kInvalidParameter;

    return dommy::maya::buildCheckedArray<MVectorArray>(
        count,
        [&](unsigned int index, MVector& output) -> MStatus {
            const MVector& point = points[index];
            double parameter = 0.0;
            const MPoint closest = curve.closestPoint(MPoint(point.x, point.y, point.z), &parameter,
                                                      0.01, MSpace::kWorld, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            float rampPosition = 0.0f;
            if (!dommy::maya::isFinitePoint(closest) ||
                !dommy::field::normalizeRange(parameter, startParameter, endParameter,
                                              rampPosition))
                return MS::kInvalidParameter;

            dommy::direction::Delta delta;
            if (!dommy::direction::difference(point, closest, delta))
                return MS::kInvalidParameter;
            // Keep the historical inclusive outer boundary and tiny-radius no-op.
            if (useMaxDistance && delta.length > maxDistance) {
                output = MVector::zero;
                return MS::kSuccess;
            }

            MVector tangent = curve.tangent(parameter, MSpace::kWorld, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!dommy::maya::isFiniteVector(tangent))
                return MS::kInvalidParameter;
            dommy::math::normalized3OrZero(tangent.x, tangent.y, tangent.z, tangent.x, tangent.y,
                                           tangent.z, 1.0e-12);
            MVector direction;
            dommy::math::normalized3OrZero(delta.values[0], delta.values[1], delta.values[2],
                                           direction.x, direction.y, direction.z, 1.0e-12);
            float radiusValue = 0.0f, forwardValue = 0.0f, toCurveValue = 0.0f;
            DOMMY_RETURN_IF_FAILURE(sampleCurveRamp(radiusRamp, rampPosition, radiusValue));
            DOMMY_RETURN_IF_FAILURE(sampleCurveRamp(forwardRamp, rampPosition, forwardValue));
            DOMMY_RETURN_IF_FAILURE(sampleCurveRamp(toCurveRamp, rampPosition, toCurveValue));

            double attenuationWeight = 1.0, falloff = 1.0;
            if (useMaxDistance) {
                const double ratio = dommy::field::clampUnit(delta.length / maxDistance);
                attenuationWeight = std::pow(std::max(0.0, 1.0 - ratio), attenuation);
                falloff = falloffCurve(ratio, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!std::isfinite(attenuationWeight) || !std::isfinite(falloff))
                    return MS::kInvalidParameter;
            }

            const bool tip = affectTip && std::fabs(endParameter - parameter) <= 0.1;
            bool addTangent = tip;
            double curveWeight = 1.0;
            if (!tip) {
                double innerDistance = 0.0;
                if (!dommy::numeric::scaledProduct(
                        std::array<double, 2>{{tangentDistance, double(radiusValue)}},
                        innerDistance))
                    return MS::kInvalidParameter;
                innerDistance = std::max(0.0, innerDistance);
                if (innerDistance > 1.0e-12 && delta.length < innerDistance) {
                    addTangent = true;
                    curveWeight = dommy::field::clampUnit(delta.length / innerDistance);
                    if (method == 1) {
                        float blendValue = 0.0f;
                        DOMMY_RETURN_IF_FAILURE(sampleCurveRamp(
                            blendRamp, static_cast<float>(curveWeight), blendValue));
                        if (!dommy::numeric::scaledProduct(
                                std::array<double, 2>{{blend, double(blendValue)}}, curveWeight))
                            return MS::kInvalidParameter;
                    }
                }
            }

            const double mass = masses[masses.length() == 1U ? 0U : index];
            MVector force;
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                double forward = 0.0, towards = 0.0, drag = 0.0, attractionForce = 0.0;
                if (addTangent &&
                    !dommy::numeric::scaledProduct(
                        std::array<double, 3>{{tangent[axis], double(forwardValue), forwardScale}},
                        forward))
                    return MS::kInvalidParameter;
                if (!tip && !dommy::numeric::scaledProduct(
                                std::array<double, 4>{{direction[axis], double(toCurveValue),
                                                       toCurveScale, attraction}},
                                towards))
                    return MS::kInvalidParameter;
                if (!dommy::numeric::scaledProduct(
                        std::array<double, 2>{{velocities[index][axis], damping}}, drag) ||
                    !dommy::numeric::scaledProduct(
                        std::array<double, 2>{{towards - drag, curveWeight}}, attractionForce) ||
                    !dommy::numeric::scaledProduct(
                        std::array<double, 5>{{forward + attractionForce, magnitude, mass,
                                               attenuationWeight, falloff}},
                        force[axis]))
                    return MS::kInvalidParameter;
            }
            output = force;
            return MS::kSuccess;
        },
        outputForce);
}
