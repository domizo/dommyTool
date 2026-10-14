//
//  DCResample.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/22.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCResample.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DCurveSampling.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"

#include <algorithm>
#include <cmath>

const MTypeId DCResample::typeId(ID_DC_RESAMPLE);

MObject DCResample::inDC;

MObject DCResample::aSampleType;

MObject DCResample::aLength;
MObject DCResample::aNumSpan;
MObject DCResample::aDegree;
MObject DCResample::aKeepRange;
MObject DCResample::aEndKnots;

MObject DCResample::aKeepEndPoints;
MObject DCResample::aKeepTangents;
MObject DCResample::aKeepCV;
MObject DCResample::aTolerance;

MObject DCResample::outDC;

void* DCResample::creator() {
    return ::dommy::maya::createObject("DCResample", [&]() {
        return new DCResample();
    });
}

MStatus DCResample::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;
    MStatus stat;

#pragma mark get input dcurve
    const DCurveData* inputDCData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
    if (!inputDCData)
        return MS::kFailure;
    if (inputDCData->numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount ||
        inputDCData->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    MObject outputDataObject;
    DCurveData* outDCrvs = nullptr;
    stat =
        dommy::maya::createPluginDataObject<DCurveData>(DCurveData::id, outputDataObject, outDCrvs);
    if (!stat)
        return stat;

    if (!outDCrvs)
        return MS::kFailure;

    DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));

    const std::size_t curveCount = outDCrvs->numberOfCurves();
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount ||
        outDCrvs->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }

    if (curveCount) {

#pragma mark get attribute
        const MDataHandle sampleTypeHandle = data.inputValue(aSampleType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short sampleType = sampleTypeHandle.asShort();

        const MDataHandle lengthHandle = data.inputValue(aLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double length = lengthHandle.asDouble();

        const MDataHandle rawNumSpanHandle = data.inputValue(aNumSpan, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawNumSpan = rawNumSpanHandle.asInt();

        const MDataHandle degreeHandle = data.inputValue(aDegree, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short degree = degreeHandle.asShort();

        const MDataHandle keepRangeHandle = data.inputValue(aKeepRange, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short keepRange = keepRangeHandle.asShort();

        const MDataHandle endKnotsHandle = data.inputValue(aEndKnots, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short endKnots = endKnotsHandle.asShort();

        const MDataHandle keepEndPointsHandle = data.inputValue(aKeepEndPoints, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool keepEndPoints = keepEndPointsHandle.asBool();

        const MDataHandle keepTangentsHandle = data.inputValue(aKeepTangents, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool keepTangents = keepTangentsHandle.asBool();

        const MDataHandle keepCVHandle = data.inputValue(aKeepCV, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool keepCV = keepCVHandle.asBool();

        const MDataHandle toleranceHandle = data.inputValue(aTolerance, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double tolerance = toleranceHandle.asDouble();

        std::size_t requestedSpanCount = 0U;
        if (sampleType < 0 || sampleType > 2 || degree < 0 || degree > 1 || keepRange < 0 ||
            keepRange > 2 || endKnots < 0 || endKnots > 1 || !std::isfinite(length) ||
            length <= 0.0 || !std::isfinite(tolerance) || tolerance < 0.0 ||
            !dommy::generation::checkedCount(rawNumSpan, dommy::generation::kMaxCurveSampleCount,
                                             requestedSpanCount) ||
            (sampleType == 1 && requestedSpanCount == 0U)) {
            EchoWarning(name().asChar(), "Invalid curve resampling parameters.");
            return MS::kInvalidParameter;
        }

        const unsigned int curveDegree = degree == 0 ? 1U : 3U;
        std::size_t totalOutputPointCount = 0U;

        if (sampleType == 0 || sampleType == 1) {

            for (auto curve = outDCrvs->beginCurves(); curve != outDCrvs->endCurves(); ++curve) {
                DCItem& cIt = *curve;

                MObject crvObj;
                stat = generateMCurveFromDcurve(cIt, crvObj);
                if (!stat)
                    return stat;
                MFnNurbsCurve crvFn(crvObj, &stat);
                if (!stat)
                    return stat;

                std::size_t spanCount = requestedSpanCount;
                if (sampleType == 0) {
                    const double curveLength = crvFn.length(kMFnNurbsEpsilon, &stat);
                    if (!stat)
                        return stat;
                    const double rawSpanCount = std::floor(curveLength / length);
                    if (!std::isfinite(curveLength) || curveLength < 0.0 ||
                        !std::isfinite(rawSpanCount) ||
                        rawSpanCount >
                            static_cast<double>(dommy::generation::kMaxCurveSampleCount)) {
                        return MS::kInvalidParameter;
                    }
                    spanCount = std::max<std::size_t>(1U, static_cast<std::size_t>(rawSpanCount));
                }

                std::size_t estimatedCvCount = 0U;
                std::size_t estimatedTotal = 0U;
                if (!dommy::generation::checkedSum(spanCount, static_cast<std::size_t>(curveDegree),
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   estimatedCvCount) ||
                    !dommy::generation::checkedSum(totalOutputPointCount, estimatedCvCount,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   estimatedTotal)) {
                    return MS::kInvalidParameter;
                }

                MObject newCrvObj =
                    crvFn.rebuild(static_cast<int>(spanCount), curveDegree, keepRange, endKnots,
                                  keepEndPoints, keepTangents, keepCV, tolerance, &stat);
                if (!stat)
                    return stat;

                MFnNurbsCurve newCrvFn(newCrvObj, &stat);
                if (!stat)
                    return stat;

                const int rawCvCount = newCrvFn.numCVs(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                std::size_t cvCount = 0U, plannedTotal = 0U;
                if (!dommy::generation::checkedCount(
                        rawCvCount, dommy::generation::kMaxGeneratedPointCount, cvCount) ||
                    !dommy::generation::checkedSum(totalOutputPointCount, cvCount,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   plannedTotal))
                    return MS::kInvalidParameter;
                MPointArray cvs;
                stat = newCrvFn.getCVs(cvs, MSpace::kObject);
                if (!stat)
                    return stat;
                if (cvs.length() != cvCount)
                    return MS::kFailure;
                for (unsigned int cvIndex = 0U; cvIndex < cvs.length(); ++cvIndex) {
                    if (!dommy::maya::isFloatRepresentablePoint(cvs[cvIndex])) {
                        return MS::kInvalidParameter;
                    }
                }
                if (!dommy::generation::checkedSum(totalOutputPointCount, cvs.length(),
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   totalOutputPointCount)) {
                    return MS::kInvalidParameter;
                }

                MDoubleArray knots;
                stat = newCrvFn.getKnots(knots);
                if (!stat)
                    return stat;

                const int rebuiltDegree = newCrvFn.degree(&stat);
                if (!stat)
                    return stat;
                const MFnNurbsCurve::Form rebuiltForm = newCrvFn.form(&stat);
                if (!stat)
                    return stat;
                if (!cIt->setReplaceCurveData(cvs, knots, rebuiltDegree,
                                              rebuiltForm == MFnNurbsCurve::Form::kOpen,
                                              DCurveReplacementPolicy::ResetMetadata)) {
                    return MS::kFailure;
                }

            } // finish loop curve

        } else {

            for (auto curve = outDCrvs->beginCurves(); curve != outDCrvs->endCurves(); ++curve) {
                DPointList newPoints;
                DPIndexList sourceIndices;
                if (!dommy::curve::resampleCurvePolyline(
                        *(*curve)->getPositionPtrConst(), length,
                        dommy::generation::kMaxGeneratedPointCount - totalOutputPointCount,
                        newPoints, sourceIndices))
                    return MS::kInvalidParameter;

                const int originalDegree = (*curve)->getDegree();
                MDoubleArray knots;
                GenerateDCurveKnots(newPoints.size(), originalDegree, knots);
                if (!dommy::generation::checkedSum(totalOutputPointCount, newPoints.size(),
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   totalOutputPointCount) ||
                    !(*curve)->setRemappedCurveData(newPoints, knots, originalDegree,
                                                    (*curve)->isCurveOpen(), sourceIndices))
                    return MS::kFailure;
            }
        }

        std::size_t actualPointCount = 0U;
        if (!outDCrvs->getCheckedTotalCvs(actualPointCount) ||
            actualPointCount != totalOutputPointCount)
            return MS::kFailure;
        outDCrvs->updateBBox();

    } // check curve count

    MDataHandle output = data.outputValue(outDC, &stat);
    if (!stat)
        return stat;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outDCrvs));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCResample")

MStatus DCResample::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute sampleEnum;
    aSampleType = sampleEnum.create("SampleType", "sty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(sampleEnum.addField("Length (Maya Internal)", 0));
    DOMMY_RETURN_IF_FAILURE(sampleEnum.addField("Num Span (Maya Internal)", 1));
    DOMMY_RETURN_IF_FAILURE(sampleEnum.addField("Step Length(Point to Point)", 2));
    DOMMY_RETURN_IF_FAILURE(sampleEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampleType));

    aLength = nAttr.create("Length", "len", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLength));

    aNumSpan = nAttr.create("NumSpan", "nums", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxCurveSampleCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNumSpan));

    MFnEnumAttribute degEnum;
    aDegree = degEnum.create("Degree", "deg", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(degEnum.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(degEnum.addField("Cubic", 1));
    DOMMY_RETURN_IF_FAILURE(degEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    MFnEnumAttribute kprngEnum;
    aKeepRange = kprngEnum.create("KeepRange", "kprng", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(kprngEnum.addField("0 to 1", 0));
    DOMMY_RETURN_IF_FAILURE(kprngEnum.addField("keep", 1));
    DOMMY_RETURN_IF_FAILURE(kprngEnum.addField("0 to span", 2));
    DOMMY_RETURN_IF_FAILURE(kprngEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepRange));

    MFnEnumAttribute endKnotsEnum;
    aEndKnots = endKnotsEnum.create("EndKnots", "ek", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(endKnotsEnum.addField("uniform end knots", 0));
    DOMMY_RETURN_IF_FAILURE(endKnotsEnum.addField("multiple end knots", 1));
    DOMMY_RETURN_IF_FAILURE(endKnotsEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndKnots));

    aKeepEndPoints = nAttr.create("KeepEndPoints", "kep", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepEndPoints));

    aKeepTangents = nAttr.create("KeepTangents", "ket", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepTangents));

    aKeepCV = nAttr.create("KeepCV", "kecv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepCV));

    aTolerance = nAttr.create("Tolerance", "tol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampleType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNumSpan, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKeepRange, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndKnots, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKeepEndPoints, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKeepTangents, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKeepCV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, outDC));

    return MS::kSuccess;
}
