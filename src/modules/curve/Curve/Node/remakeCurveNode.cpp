//
//  remakeCurveNode.cpp
//  dommyTool
//
//  Created by yuzo on 1/9/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "remakeCurveNode.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MPlug.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MGlobal.h>

const MTypeId remakeCurveNode::typeId(0x0046A);

// Attributes
MObject remakeCurveNode::inputCurve;

MObject remakeCurveNode::sampling;
MObject remakeCurveNode::curveLength;
MObject remakeCurveNode::offset;

MObject remakeCurveNode::outputCurve;

void* remakeCurveNode::creator() {
    return ::dommy::maya::createObject("remakeCurveNode", [&]() {
        return new remakeCurveNode();
    });
}

MStatus remakeCurveNode::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outputCurve)
        return MS::kUnknownParameter;

    MStatus status;
    MDataHandle curveHandle = data.inputValue(inputCurve, &status);
    if (!status)
        return status;
    if (curveHandle.type() != MFnData::kNurbsCurve) {
        return MS::kInvalidParameter;
    }

    const MObject inputCurveObject = curveHandle.data();
    if (inputCurveObject.isNull())
        return MS::kInvalidParameter;

    const MDataHandle samplingHandle = data.inputValue(sampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSampling = samplingHandle.asInt();
    const MDataHandle lengthHandle = data.inputValue(curveLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float requestedLength = lengthHandle.asFloat();
    const MDataHandle offsetHandle = data.inputValue(offset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float parameterOffset = offsetHandle.asFloat();

    std::size_t samplingCount = 0U;
    std::size_t pointCount = 0U;
    if (!std::isfinite(requestedLength) || !std::isfinite(parameterOffset) ||
        requestedLength <= std::numeric_limits<float>::epsilon() ||
        !dommy::generation::checkedCount(rawSampling, dommy::generation::kMaxGeneratedPointCount,
                                         samplingCount) ||
        samplingCount < 2U ||
        !dommy::generation::checkedSum(samplingCount, 1U,
                                       dommy::generation::kMaxGeneratedPointCount, pointCount)) {
        MGlobal::displayError("remakeCurveNode: invalid or excessive sampling inputs.");
        return MS::kInvalidParameter;
    }

    MFnNurbsCurve curveFn(inputCurveObject, &status);
    if (!status)
        return status;

    const double sourceLength = curveFn.length(kMFnNurbsEpsilon, &status);
    if (!status)
        return status;
    if (!std::isfinite(sourceLength) || sourceLength <= std::numeric_limits<double>::epsilon()) {
        return MS::kInvalidParameter;
    }
    const double sampledLength = std::min(static_cast<double>(requestedLength), sourceLength);

    double parameterStart = 0.0;
    double parameterEnd = 0.0;
    status = curveFn.getKnotDomain(parameterStart, parameterEnd);
    if (!status)
        return status;
    if (!std::isfinite(parameterStart) || !std::isfinite(parameterEnd) ||
        parameterStart > parameterEnd) {
        return MS::kFailure;
    }

    MPointArray editPoints;
    for (std::size_t index = 0U; index < pointCount; ++index) {
        const double ratio = static_cast<double>(index) / static_cast<double>(samplingCount);
        double parameter = curveFn.findParamFromLength(sampledLength * ratio, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!std::isfinite(parameter))
            return MS::kFailure;
        parameter = std::max(parameterStart, std::min(parameterEnd, parameter + parameterOffset));

        MPoint point;
        status = curveFn.getPointAtParam(parameter, point, MSpace::kWorld);
        if (!status)
            return status;
        if (!dommy::maya::isFinitePoint(point))
            return MS::kFailure;
        status = editPoints.append(point);
        if (!status)
            return status;
    }

    const int sourceDegree = curveFn.degree(&status);
    if (!status)
        return status;
    if (sourceDegree < 1)
        return MS::kFailure;
    const unsigned int outputDegree = static_cast<unsigned int>(
        std::min<std::size_t>(static_cast<std::size_t>(sourceDegree), pointCount - 1U));

    MFnNurbsCurveData dataCreator;
    MObject newCurveData = dataCreator.create(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (newCurveData.isNull())
        return MS::kFailure;

    MFnNurbsCurve outputCurveFn;
    const MObject newCurve = outputCurveFn.createWithEditPoints(
        editPoints, outputDegree, MFnNurbsCurve::kOpen, false, true, true, newCurveData, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (newCurve.isNull())
        return MS::kFailure;

    MDataHandle outputHandle = data.outputValue(outputCurve, &status);
    if (!status)
        return status;
    status = outputHandle.set(newCurveData);
    if (!status)
        return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("remakeCurveNode")

MStatus remakeCurveNode::initialize() {
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnGenericAttribute gAttr;
    MStatus status;

    inputCurve = gAttr.create("inputCurve", "in", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputCurve));

    sampling = nAttr.create("sampling", "s", MFnNumericData::kInt, 10, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    curveLength = nAttr.create("curveLength", "cl", MFnNumericData::kFloat, 1.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));

    offset = nAttr.create("offset", "o", MFnNumericData::kFloat, 0.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));

    outputCurve = tAttr.create("outputCurve", "out", MFnNurbsCurveData::kNurbsCurve,
                               MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputCurve, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(curveLength, outputCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputCurve));

    return MS::kSuccess;
}
