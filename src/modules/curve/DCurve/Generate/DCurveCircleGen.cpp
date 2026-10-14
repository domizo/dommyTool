//
//  DCurveCircleGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//

#include "DCurveCircleGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include <cmath>
#include <cstddef>


namespace {

const long double kDegreesPerTurn = 360.0L;
const long double kDegreesToRadians =
    0.017453292519943295769236907684886L;

bool sampleCirclePoint(double radius,
                       double startAngle,
                       double endAngle,
                       double offsetAngle,
                       long double fraction,
                       MPoint& point)
{
    const long double angle =
        static_cast<long double>(startAngle) +
        (static_cast<long double>(endAngle) -
         static_cast<long double>(startAngle)) * fraction +
        static_cast<long double>(offsetAngle);
    if (!std::isfinite(angle)) return false;

    const long double wrappedAngle = std::fmod(angle, kDegreesPerTurn);
    const long double radians = -wrappedAngle * kDegreesToRadians;
    const long double radiusValue = static_cast<long double>(radius);
    const long double x = -radiusValue * std::sin(radians);
    const long double y = radiusValue * std::cos(radians);
    if (!std::isfinite(x) || !std::isfinite(y)) return false;

    point = MPoint(static_cast<double>(x), static_cast<double>(y), 0.0);
    return dommy::maya::isFinitePoint(point);
}

} // namespace


const MTypeId DCurveCircleGen::typeId(ID_DC_CIRCLE_GEN);

MObject DCurveCircleGen::aRadius;
MObject DCurveCircleGen::aSampling;
MObject DCurveCircleGen::aStartAngle;
MObject DCurveCircleGen::aEndAngle;
MObject DCurveCircleGen::aOffsetAngle;

MObject DCurveCircleGen::aIsOpen;
MObject DCurveCircleGen::aDegree;
MObject DCurveCircleGen::aIsMultiEndKnot;

MObject DCurveCircleGen::outDC;


void* DCurveCircleGen::creator()
{
    return ::dommy::maya::createObject("DCurveCircleGen", [&]() {
        return new DCurveCircleGen();
    });
}


MStatus DCurveCircleGen::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle samplingHandle =
        data.inputValue(aSampling, &status);
    if (!status) return status;

    std::size_t sampling = 0U;
    if (!dommy::generation::checkedCount(
            samplingHandle.asInt(),
            dommy::generation::kMaxCurveSampleCount, sampling)) {
        MGlobal::displayError(
            "DCurveCircleGen: sampling exceeds the safety budget.");
        return MS::kInvalidParameter;
    }

    const MDataHandle radiusHandle = data.inputValue(aRadius, &status);
    if (!status) return status;
    const double radius = radiusHandle.asDouble();

    const MDataHandle startAngleHandle =
        data.inputValue(aStartAngle, &status);
    if (!status) return status;
    const double startAngle = startAngleHandle.asDouble();

    const MDataHandle endAngleHandle =
        data.inputValue(aEndAngle, &status);
    if (!status) return status;
    const double endAngle = endAngleHandle.asDouble();

    const MDataHandle offsetAngleHandle =
        data.inputValue(aOffsetAngle, &status);
    if (!status) return status;
    const double offsetAngle = offsetAngleHandle.asDouble();

    if (!std::isfinite(radius) || !std::isfinite(startAngle) ||
        !std::isfinite(endAngle) || !std::isfinite(offsetAngle)) {
        MGlobal::displayError(
            "DCurveCircleGen: radius and angles must be finite.");
        return MS::kInvalidParameter;
    }

    const MDataHandle openHandle = data.inputValue(aIsOpen, &status);
    if (!status) return status;
    const bool isOpen = openHandle.asBool();

    const MDataHandle degreeHandle = data.inputValue(aDegree, &status);
    if (!status) return status;
    const short degreeType = degreeHandle.asShort();
    if (degreeType != 0 && degreeType != 1) {
        MGlobal::displayError(
            "DCurveCircleGen: unsupported degree type.");
        return MS::kInvalidParameter;
    }

    const MDataHandle multipleEndKnotsHandle =
        data.inputValue(aIsMultiEndKnot, &status);
    if (!status) return status;
    const bool multipleEndKnots = multipleEndKnotsHandle.asBool();

    std::size_t outputPointCount = sampling;
    if (!isOpen && sampling > 1U &&
        !dommy::generation::checkedSum(
            sampling, 1U, dommy::generation::kMaxCurveSampleCount,
            outputPointCount)) {
        MGlobal::displayError(
            "DCurveCircleGen: output point count exceeds the safety budget.");
        return MS::kFailure;
    }

    MObject outputObject;
    DCurveData* outputCurves = NULL;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputObject, outputCurves);
    if (!status || !outputCurves) {
        return status ? MStatus(MS::kFailure) : status;
    }

    if (sampling > 1U) {
        MPointArray controlVertices(
            static_cast<unsigned int>(sampling));
        if (controlVertices.length() != sampling) return MS::kFailure;
        const long double denominator = static_cast<long double>(
            isOpen ? sampling - 1U : sampling);

        for (std::size_t index = 0U; index < sampling; ++index) {
            MPoint point;
            if (!sampleCirclePoint(
                    radius, startAngle, endAngle, offsetAngle,
                    static_cast<long double>(index) / denominator,
                    point)) {
                MGlobal::displayError(
                    "DCurveCircleGen: generated a non-finite point.");
                return MS::kFailure;
            }
            controlVertices[static_cast<unsigned int>(index)] = point;
        }

        if (!isOpen) {
            status = controlVertices.append(controlVertices[0]);
            if (!status ||
                controlVertices.length() != outputPointCount) {
                return status ? MStatus(MS::kFailure) : status;
            }
        }

        int degree = degreeType == 0 ? 1 : 3;
        if (controlVertices.length() <=
            static_cast<unsigned int>(degree)) {
            degree = 1;
        }

        MDoubleArray knots;
        GenerateDCurveKnots(
            controlVertices.length(), degree, knots, multipleEndKnots);
        DCItem curve(new DCurveInternalData(
            controlVertices, knots, degree, isOpen));
        if (!curve || !curve->hasValidCurveTopology()) {
            MGlobal::displayError(
                "DCurveCircleGen: generated invalid curve topology.");
            return MS::kFailure;
        }
        if (!outputCurves->insert(curve)) return MS::kFailure;
        outputCurves->updateBBox();
    }

    if (!outputCurves->hasValidCurveTopology()) return MS::kFailure;

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        outputHandle, outputObject, *outputCurves);
    if (!status) return status;
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveCircleGen")


MStatus DCurveCircleGen::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    aRadius = numericAttribute.create(
        "Radius", "rad", MFnNumericData::kDouble, 5.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aSampling = numericAttribute.create(
        "Sampling", "smp", MFnNumericData::kInt, 8, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(60));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aStartAngle = numericAttribute.create(
        "StartAngle", "sang", MFnNumericData::kDouble, 0.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(-360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartAngle));

    aEndAngle = numericAttribute.create(
        "EndAngle", "eang", MFnNumericData::kDouble, 360.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(-360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndAngle));

    aOffsetAngle = numericAttribute.create(
        "OffsetAngle", "eoang", MFnNumericData::kDouble, 0.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(-360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(360.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetAngle));

    aIsOpen = numericAttribute.create(
        "IsOpen", "isopn", MFnNumericData::kBoolean, false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOpen));

    MFnEnumAttribute degreeAttribute;
    aDegree = degreeAttribute.create("Degree", "deg", 1, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.addField("Cubic", 1));
    DOMMY_RETURN_IF_FAILURE(degreeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    aIsMultiEndKnot = numericAttribute.create(
        "IsMultiEndKnot", "ismltkt", MFnNumericData::kBoolean, true,
        &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMultiEndKnot));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartAngle, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndAngle, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAngle, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsOpen, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aIsMultiEndKnot, outDC));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveCircleGen", "initialize", std::current_exception());
}
