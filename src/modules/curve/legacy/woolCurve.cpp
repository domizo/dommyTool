#include "common/utility/DMayaEntryPointGuard.h"
#include "woolCurve.h"
#include "common/utility/DComputeGuard.h"
#include "DWoolCurvePlan.h"

#include <cmath>
#include <limits>

#include <maya/MString.h>
#include <maya/MTypeId.h> 
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MPointArray.h>
#include <maya/MVector.h>
#include <maya/MDoubleArray.h>
#include <maya/MGlobal.h>

#include "dommyMath.h"



MTypeId woolCurve::typeId( 0x0044A );


// Attributes
MObject woolCurve::startRadius;
MObject woolCurve::endRadius;
MObject woolCurve::round;
MObject woolCurve::sections;
MObject woolCurve::rotNoise;
MObject woolCurve::axisNoise;

MObject woolCurve::zTrans;

MObject woolCurve::outputCurve;



void *woolCurve::creator() {
    return ::dommy::maya::createObject("woolCurve", [&]() {
        return new woolCurve();
    });
}

MStatus woolCurve::initialize() try
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute   typedAttr;
	MStatus				status;
	
	startRadius = nAttr.create(
		"startRadius", "sr", MFnNumericData::kFloat, 1.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMin(0.0);
	CHECK_MSTATUS_AND_RETURN_IT(status);
 	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	endRadius = nAttr.create(
		"endRadius", "er", MFnNumericData::kFloat, 50.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMin(0.0);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	round = nAttr.create(
		"round", "ro", MFnNumericData::kInt, 50, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMin(1);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMax(
		static_cast<int>(dommy::generation::kMaxCurveSampleCount));
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	sections = nAttr.create(
		"sections", "se", MFnNumericData::kInt, 8, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMin(3);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setMax(
		static_cast<int>(dommy::generation::kMaxCurveSampleCount));
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	rotNoise = nAttr.create(
		"rotNoise", "rn", MFnNumericData::kFloat, 10.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	axisNoise = nAttr.create(
		"axisNoise", "an", MFnNumericData::kFloat, 20.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	
	zTrans = nAttr.create(
		"zTrans", "zt", MFnNumericData::kFloat, 0.0, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setStorable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = nAttr.setKeyable(true);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	
	
    outputCurve = typedAttr.create(
		"outputCurve", "out", MFnNurbsCurveData::kNurbsCurve,
		MObject::kNullObj, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = typedAttr.setStorable(false);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = typedAttr.setWritable(false);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	
	
	status = addAttribute(startRadius);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(endRadius);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(round);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(sections);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(rotNoise);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = addAttribute(axisNoise);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	status = addAttribute(zTrans);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	status = addAttribute(outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
    status = attributeAffects(startRadius, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = attributeAffects(endRadius, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = attributeAffects(round, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = attributeAffects(sections, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = attributeAffects(rotNoise, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
    status = attributeAffects(axisNoise, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	status = attributeAffects(zTrans, outputCurve);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	
	return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("woolCurve", "initialize", std::current_exception());
}



MStatus woolCurve::compute( const MPlug& plug, MDataBlock& data ) try
{
	if (plug != outputCurve) return MS::kUnknownParameter;

	MStatus status;
	MDataHandle control = data.inputValue(startRadius, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double startRadiusValue = control.asDouble();
	control = data.inputValue(endRadius, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double endRadiusValue = control.asDouble();
	control = data.inputValue(round, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int roundValue = control.asInt();
	control = data.inputValue(sections, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const int sectionsValue = control.asInt();
	control = data.inputValue(zTrans, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	const double zTranslationValue = control.asDouble();

	dommy::curve::WoolCurvePlan plan;
	if (!dommy::curve::buildWoolCurvePlan(
			startRadiusValue, endRadiusValue, roundValue, sectionsValue,
			zTranslationValue, plan)) {
		MGlobal::displayError(
			"woolCurve: invalid inputs or generation budget exceeded.");
		return MS::kInvalidParameter;
	}
	if (plan.curveCvCount >
			static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()) ||
		plan.knotCount >
			static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
		return MS::kInvalidParameter;
	}

	MPointArray cvs;
	status = cvs.setLength(
		static_cast<unsigned int>(plan.curveCvCount));
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = cvs.set(MPoint::origin, 0U);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	const double sectionAngle =
		(2.0 * 3.14159265358979323846) /
		static_cast<double>(sectionsValue);
	for (std::size_t sample = 0U;
		 sample < plan.generatedPointCount; ++sample) {
		const double radius =
			startRadiusValue + plan.radiusStep * static_cast<double>(sample);
		const double angle = sectionAngle *
			static_cast<double>(
				sample % static_cast<std::size_t>(sectionsValue));
		const MPoint point(
			std::sin(angle) * radius,
			std::cos(angle) * radius,
			zTranslationValue * radius);
		if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
			!std::isfinite(point.z)) {
			return MS::kInvalidParameter;
		}
		status = cvs.set(
			point, static_cast<unsigned int>(sample + 1U));
		CHECK_MSTATUS_AND_RETURN_IT(status);
	}

	MDoubleArray knots;
	status = knots.setLength(static_cast<unsigned int>(plan.knotCount));
	CHECK_MSTATUS_AND_RETURN_IT(status);
	for (std::size_t knot = 0U; knot < plan.knotCount; ++knot) {
		status = knots.set(
			static_cast<double>(knot), static_cast<unsigned int>(knot));
		CHECK_MSTATUS_AND_RETURN_IT(status);
	}

	MFnNurbsCurveData dataCreator;
	MObject newCurveData = dataCreator.create(&status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (newCurveData.isNull()) return MS::kFailure;

	MFnNurbsCurve curveFn;
	const MObject curve = curveFn.create(
		cvs, knots, plan.degree, MFnNurbsCurve::kOpen, false, false,
		newCurveData, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	if (curve.isNull()) return MS::kFailure;

	MDataHandle curveHandle = data.outputValue(outputCurve, &status);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = curveHandle.set(newCurveData);
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = data.setClean(plug);
	if (!status) {
		MGlobal::displayError(
			MString("woolCurve: failed to mark outputCurve clean: ") +
			status.errorString());
	}
	return status;
}
DOMMY_COMPUTE_CATCH("woolCurve")
