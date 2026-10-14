//
//  DCDivide.cpp
//  dommyTool
//
//  Created by yuzo on 1/25/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCDivide.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DCurveDividePlan.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DMayaHeaders.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointTransform.h"

#include <cmath>
#include <limits>

const MTypeId DCDivide::typeId(ID_DC_DIVIDE);

MObject DCDivide::inDC;
MObject DCDivide::aDivideType;
MObject DCDivide::aCount;
MObject DCDivide::aSampling;
MObject DCDivide::aOffset;
MObject DCDivide::aSpaceLength;
MObject DCDivide::aIsEdgeSpace;
MObject DCDivide::aStepLength;
MObject DCDivide::outDC;

void* DCDivide::creator() {
    return ::dommy::maya::createObject("DCDivide", [&]() {
        return new DCDivide();
    });
}

MStatus DCDivide::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;

    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    if (!input)
        return MS::kFailure;

    MStatus status;
    const MDataHandle divideTypeHandle = data.inputValue(aDivideType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short divideType = divideTypeHandle.asShort();
    const MDataHandle countHandle = data.inputValue(aCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int count = countHandle.asInt();
    const MDataHandle samplingHandle = data.inputValue(aSampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int sampling = samplingHandle.asInt();
    const MDataHandle spaceLengthHandle = data.inputValue(aSpaceLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double spaceLength = spaceLengthHandle.asDouble();
    const MDataHandle edgeSpaceHandle = data.inputValue(aIsEdgeSpace, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool edgeSpace = edgeSpaceHandle.asBool();
    const MDataHandle stepLengthHandle = data.inputValue(aStepLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double stepLength = stepLengthHandle.asDouble();
    const MDataHandle offsetHandle = data.inputValue(aOffset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double offset = offsetHandle.asDouble();

    dommy::curve::DividePlan plan;
    if (!dommy::curve::makeDividePlan(divideType, count, sampling, spaceLength, stepLength, offset,
                                      input->numberOfCurves(), plan)) {
        return MS::kInvalidParameter;
    }

    MFnPluginData pluginData;
    DCurveData* output = NULL;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output)
        return MS::kFailure;

    DCurveOutputBatch batch;
    std::size_t generatedCurveCount = 0U, generatedPointCount = 0U;
    for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve) {
        if (!*curve || plan.sampling <= static_cast<std::size_t>((*curve)->getDegree())) {
            return MS::kInvalidParameter;
        }
        const MMatrix matrix = (*curve)->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(matrix))
            return MS::kInvalidParameter;

        MObject curveObject;
        DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*curve, curveObject));
        MFnNurbsCurve curveFn(curveObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const double curveLength = curveFn.length(1.0e-3, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        double segmentLength = 0.0;
        double gapLength = 0.0;
        double sampleStep = 0.0;
        if (!dommy::curve::divideSegmentLengths(plan, curveLength, edgeSpace, segmentLength,
                                                gapLength, sampleStep)) {
            continue;
        }

        for (std::size_t segmentIndex = 0U; segmentIndex < plan.count; ++segmentIndex) {
            MPointArray cvs;
            bool segmentFits = true;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                plan.sampling,
                [&](unsigned int sampleIndex, MPoint& point) -> MStatus {
                    if (!segmentFits)
                        return MS::kSuccess;
                    double sampleLength = 0.0;
                    if (!dommy::curve::divideSampleLength(plan, segmentIndex, sampleIndex,
                                                          edgeSpace, curveLength, segmentLength,
                                                          gapLength, sampleStep, sampleLength)) {
                        segmentFits = false;
                        return MS::kSuccess;
                    }

                    double parameter = curveFn.findParamFromLength(sampleLength, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    parameter += plan.offset;
                    if (!std::isfinite(parameter)) {
                        return MS::kInvalidParameter;
                    }

                    DOMMY_RETURN_IF_FAILURE(
                        curveFn.getPointAtParam(parameter, point, MSpace::kObject));
                    if (!dommy::maya::isFloatRepresentablePoint(point)) {
                        return MS::kFailure;
                    }
                    return MS::kSuccess;
                },
                cvs));
            if (!segmentFits)
                continue;

            MDoubleArray knots;
            GenerateDCurveKnots(cvs.length(), (*curve)->getDegree(), knots);
            DCItem result(new DCurveInternalData(cvs, knots, (*curve)->getDegree()));
            result->setTransformMatrix(matrix);
            if (!result || !result->hasValidCurveTopology()) {
                return MS::kFailure;
            }
            if (!batch.append(result))
                return MS::kFailure;
            ++generatedCurveCount;
            generatedPointCount += cvs.length();
        }
    }

    if (!batch.publishTo(*output))
        return MS::kFailure;
    if (!output->hasValidCurveTopology() || output->numberOfCurves() != generatedCurveCount ||
        output->getTotalCvs() != generatedPointCount ||
        output->numberOfCurves() > plan.outputCurveCount ||
        output->getTotalCvs() > plan.outputPointCount) {
        return MS::kFailure;
    }
    output->updateBBox();

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        outputHandle, pluginData.object(), DCurveData::id));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCDivide")

MStatus DCDivide::initialize() {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDC = typedAttribute.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute divideTypeAttribute;
    aDivideType = divideTypeAttribute.create("DivideType", "divty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(divideTypeAttribute.addField("Divide", 0));
    DOMMY_RETURN_IF_FAILURE(divideTypeAttribute.addField("Step", 1));
    DOMMY_RETURN_IF_FAILURE(divideTypeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDivideType));

    aCount = numericAttribute.create("Count", "cnt", MFnNumericData::kLong, 4, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

    aSampling = numericAttribute.create("Sampling", "smp", MFnNumericData::kLong, 4, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(2));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aOffset = numericAttribute.create("Offset", "off", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    aSpaceLength =
        numericAttribute.create("SpaceLength", "splen", MFnNumericData::kDouble, 1.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpaceLength));

    aIsEdgeSpace =
        numericAttribute.create("IsEdgeSpace", "isesp", MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEdgeSpace));

    aStepLength =
        numericAttribute.create("StepLength", "stplen", MFnNumericData::kDouble, 0.1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1.0e-9));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStepLength));

    outDC = typedAttribute.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDivideType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSpaceLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsEdgeSpace, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStepLength, outDC));
    return MS::kSuccess;
}
