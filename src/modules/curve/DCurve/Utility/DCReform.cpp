//
//  DCReform.cpp
//  dommyTool
//
//  Created by yuzo on 1/25/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCReform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPointTransform.h"

#include <cmath>

const MTypeId DCReform::typeId(ID_DC_REFORM);

MObject DCReform::inDC;

MObject DCReform::aLength;
MObject DCReform::aSampling;
MObject DCReform::aOffset;
MObject DCReform::isClamp;

MObject DCReform::aLengthAttributeName;
MObject DCReform::aOffsetAttributeName;

MObject DCReform::outDC;

void* DCReform::creator() {
    return ::dommy::maya::createObject("DCReform", [&]() {
        return new DCReform();
    });
}

MStatus DCReform::compute(const MPlug& plug, MDataBlock& data) try {
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

    DCurveOutputBatch generatedCurves;

    const std::size_t curveCount = inputDCData->numberOfCurves();
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount) {
        return MS::kInvalidParameter;
    }

    if (curveCount != 0) {

#pragma mark get attribute

        const MDataHandle lengthHandle = data.inputValue(aLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float length = lengthHandle.asFloat();

        const MDataHandle rawSamplingHandle = data.inputValue(aSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSampling = rawSamplingHandle.asInt();

        const MDataHandle offsetHandle = data.inputValue(aOffset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float offset = offsetHandle.asFloat();

        const MDataHandle isclampHandle = data.inputValue(isClamp, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isclamp = isclampHandle.asBool();

        std::size_t sampling = 0U;
        std::size_t outputPointCount = 0U;
        if (!std::isfinite(length) || length < 0.0f || !std::isfinite(offset) ||
            !dommy::generation::checkedCount(rawSampling, dommy::generation::kMaxCurveSampleCount,
                                             sampling) ||
            sampling < 2U ||
            !dommy::generation::checkedProduct(curveCount, sampling,
                                               dommy::generation::kMaxGeneratedPointCount,
                                               outputPointCount)) {
            EchoWarning(name().asChar(), "Requested curve reform exceeds the safety limit.");
            return MS::kInvalidParameter;
        }

        const MDataHandle lengthAttrNameHandle = data.inputValue(aLengthAttributeName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string lengthAttrName = lengthAttrNameHandle.asString().asChar();
        const MDataHandle offsetAttrNameHandle = data.inputValue(aOffsetAttributeName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string offsetAttrName = offsetAttrNameHandle.asString().asChar();
        DScalarDataList lengthDataAray(curveCount, length);
        if (inputDCData->isExistAttr(lengthAttrName)) {
            getDCAttributeMDoubleArrayFromScalar(*inputDCData, lengthAttrName, lengthDataAray);
        }

        DScalarDataList offsetDataAray(curveCount, offset);
        if (inputDCData->isExistAttr(offsetAttrName)) {
            getDCAttributeMDoubleArrayFromScalar(*inputDCData, offsetAttrName, offsetDataAray);
        }

        if (lengthDataAray.size() != curveCount || offsetDataAray.size() != curveCount)
            return MS::kInvalidParameter;
        size_t curveOffset = 0;

        auto cIt = inputDCData->beginCurves();
        while (cIt != inputDCData->endCurves()) {

            const MMatrix matrix = (*cIt)->getTransformMatrix();
            if (!dommy::numeric::isFiniteAffineTransform(matrix))
                return MS::kInvalidParameter;

            MObject crvObj;
            stat = generateMCurveFromDcurve(*cIt, crvObj);
            if (!stat)
                return stat;
            MFnNurbsCurve crvFn(crvObj, &stat);
            if (!stat)
                return stat;

            const double curveLength = crvFn.length(kMFnNurbsEpsilon, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (!std::isfinite(curveLength) || curveLength < 0.0 ||
                !std::isfinite(lengthDataAray[curveOffset]) ||
                !std::isfinite(offsetDataAray[curveOffset])) {
                return MS::kInvalidParameter;
            }
            const double newCurveLength = curveLength * lengthDataAray[curveOffset];
            if (!std::isfinite(newCurveLength)) {
                return MS::kInvalidParameter;
            }

            if (newCurveLength <= 0.0) {
                curveOffset++;
                cIt++;
                continue;
            }

            const double step = newCurveLength / static_cast<double>(sampling - 1U);

            double start, end;
            stat = crvFn.getKnotDomain(start, end);
            if (!stat || !std::isfinite(start) || !std::isfinite(end) || end <= start) {
                return stat ? MStatus(MS::kInvalidParameter) : stat;
            }

            MPointArray cvs;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                sampling,
                [&](unsigned int i, MPoint& pt) -> MStatus {
                    double param = crvFn.findParamFromLength(step * double(i), &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    param += offsetDataAray[curveOffset];
                    if (!std::isfinite(param))
                        return MS::kInvalidParameter;
                    if (isclamp)
                        param = std::max(start, std::min(end, param));
                    DOMMY_RETURN_IF_FAILURE(crvFn.getPointAtParam(param, pt, MSpace::kObject));
                    return dommy::maya::isFloatRepresentablePoint(pt)
                               ? MStatus(MS::kSuccess)
                               : MStatus(MS::kInvalidParameter);
                },
                cvs));
            MDoubleArray knots;
            GenerateDCurveKnots(cvs.length(), (*cIt)->getDegree(), knots);

            DCItem result(new DCurveInternalData(cvs, knots, (*cIt)->getDegree()));
            result->setTransformMatrix(matrix);
            if (!generatedCurves.append(result))
                return MS::kFailure;

            curveOffset++;
            cIt++;
        } // finish loop curve

        if (!generatedCurves.publishTo(*outDCrvs))
            return MS::kFailure;
        outDCrvs->updateBBox();
    }

    MDataHandle output = data.outputValue(outDC, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outDCrvs));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCReform")

MStatus DCReform::initialize() {
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

    aLength = nAttr.create("Length", "len", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLength));

    aSampling = nAttr.create("Sampling", "smp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(8));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(4));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxCurveSampleCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    isClamp = nAttr.create("isClamp", "iscmp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isClamp));

    const MObject aLengthAttributeNameDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aLengthAttributeName = tAttr.create("LengthAttributeName", "lenattr", MFnData::kString,
                                        aLengthAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthAttributeName));

    const MObject aOffsetAttributeNameDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOffsetAttributeName = tAttr.create("OffsetAttributeName", "offattr", MFnData::kString,
                                        aOffsetAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetAttributeName));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isClamp, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLengthAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffsetAttributeName, outDC));

    return MS::kSuccess;
}
