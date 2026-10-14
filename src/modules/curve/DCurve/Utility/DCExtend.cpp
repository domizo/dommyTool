//
//  DCExtend.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/09.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCExtend.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointTransform.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"



const MTypeId DCExtend::typeId( ID_DC_EXTEND );

MObject DCExtend::inDC;

MObject DCExtend::aDirectionType;
MObject DCExtend::aStartLength;
MObject DCExtend::aEndLength;
MObject DCExtend::aStartInvert;
MObject DCExtend::aEndInvert;

MObject DCExtend::aAnotherCurve;
MObject DCExtend::aIsDeleteOriginal;
MObject DCExtend::aStartOffset;
MObject DCExtend::aEndOffset;

MObject DCExtend::outDC;


void *DCExtend::creator()
{
    return ::dommy::maya::createObject("DCExtend", [&]() {
        return new DCExtend();
    });
}


MStatus DCExtend::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aDirectionType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short directionType = handle.asShort();
    handle = data.inputValue(aStartLength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double startLength = handle.asDouble();
    handle = data.inputValue(aEndLength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double endLength = handle.asDouble();
    handle = data.inputValue(aStartInvert, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool startInvert = handle.asBool();
    handle = data.inputValue(aEndInvert, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool endInvert = handle.asBool();
    handle = data.inputValue(aAnotherCurve, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool separate = handle.asBool();
    handle = data.inputValue(aIsDeleteOriginal, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool deleteOriginal = handle.asBool();
    handle = data.inputValue(aStartOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double startOffset = handle.asDouble();
    handle = data.inputValue(aEndOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double endOffset = handle.asDouble();
    if ((directionType != DC_VEC_kTangent && directionType != DC_VEC_kNormal) ||
        !std::isfinite(startLength) || startLength < 0.0 ||
        !std::isfinite(endLength) || endLength < 0.0 ||
        !std::isfinite(startOffset) || !std::isfinite(endOffset)) return MS::kInvalidParameter;

    std::size_t inputPoints = 0U;
    dommy::curve::CurveExtensionPlan plan;
    const unsigned int ends = (startLength > 0.0 ? 1U : 0U) + (endLength > 0.0 ? 1U : 0U);
    if (!input->getCheckedTotalCvs(inputPoints) ||
        !dommy::curve::checkedCurveExtensionPlan(input->numberOfCurves(), inputPoints,
            ends, separate, deleteOriginal, plan)) return MS::kInvalidParameter;
    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    if (!separate || !deleteOriginal)
        DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*input));
    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(plan.addedCurves);
    std::size_t curveIndex = 0U;
    if (ends != 0U) {
        for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve, ++curveIndex) {
            const MMatrix matrix = (*curve)->getTransformMatrix();
            if (!dommy::numeric::isFiniteAffineTransform(matrix)) return MS::kInvalidParameter;
            MObject object;
            status = generateMCurveFromDcurve(*curve, object);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnNurbsCurve function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            double startParameter = 0.0, endParameter = 0.0;
            status = function.getKnotDomain(startParameter, endParameter);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(startParameter) || !std::isfinite(endParameter) ||
                endParameter < startParameter) return MS::kInvalidParameter;
            const auto sourcePoints = (*curve)->getPositionPtrConst();
            if (!sourcePoints || sourcePoints->size() < 2U) return MS::kInvalidParameter;
            for (unsigned int endpoint = 0U; endpoint < 2U; ++endpoint) {
                const bool start = endpoint == 0U;
                const double length = start ? startLength : endLength;
                if (length == 0.0) continue;
                MVector direction;
                status = dommy::curve::directionAtParameter(function,
                    start ? startParameter : endParameter, directionType == DC_VEC_kNormal,
                    MSpace::kObject, direction);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!dommy::curve::normalizeDirection(direction)) return MS::kInvalidParameter;
                if (start ? startInvert : endInvert) direction = -direction;
                const MPoint original = DPToMP(start ? sourcePoints->front() : sourcePoints->back());
                const double offset = separate ? (start ? startOffset : endOffset) : 0.0;
                const MPoint anchor = original + direction * offset;
                const MPoint extended = original + direction * length + direction * offset;
                if (!dommy::maya::isFloatRepresentablePoint(anchor) ||
                    !dommy::maya::isFloatRepresentablePoint(extended)) return MS::kInvalidParameter;
                if (separate) {
                    MPointArray points;
                    status = dommy::maya::buildCheckedArray(2U,
                        [&](unsigned int i, MPoint& point) {
                            point = i == 0U ? anchor : extended;
                            return MS::kSuccess;
                        }, points);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    DCItem generated(new DCurveInternalData(points, 1));
                    generated->setTransformMatrix(matrix);
                    if (!generated->hasValidCurveTopology()) return MS::kInvalidParameter;
                    generatedCurves.push_back(generated);
                } else {
                    DCItem target = outDCrvs->getCurvePtr(curveIndex);
                    if (!target) return MS::kFailure;
                    const DPoint point = MVToDP(extended);
                    if (!(start ? target->insertPoint(point, 0U) : target->appendPoint(point)))
                        return MS::kFailure;
                }
            }
        }
    }
    if (!generatedCurves.empty() && !outDCrvs->insert(generatedCurves)) return MS::kFailure;
    outDCrvs->updateBBox();
    std::size_t outputPoints = 0U;
    if (!outDCrvs->getCheckedTotalCvs(outputPoints) || outputPoints != plan.points ||
        outDCrvs->numberOfCurves() != plan.curves) return MS::kFailure;
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCExtend")






MStatus DCExtend::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute dirEnumFn;
    aDirectionType = dirEnumFn.create("DirectionType", "dirt", DC_VEC_kTangent, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField("Tangent", DC_VEC_kTangent));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField("Normal", DC_VEC_kNormal));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionType));

    aStartLength = nAttr.create("StartLength", "slen", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartLength));

    aEndLength = nAttr.create("EndLength", "elen", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndLength));

    aStartInvert = nAttr.create("StartInvert", "sinv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartInvert));

    aEndInvert = nAttr.create("EndInvert", "einv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndInvert));

    aAnotherCurve = nAttr.create("AnotherCurve", "acrv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAnotherCurve));

    aIsDeleteOriginal =
        nAttr.create("IsDeleteOriginal", "isdelorg", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeleteOriginal));

    aStartOffset = nAttr.create("StartOffset", "soff", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartOffset));

    aEndOffset = nAttr.create("EndOffset", "eoff", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEndOffset));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDirectionType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndLength, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartInvert, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndInvert, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAnotherCurve, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsDeleteOriginal, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aStartOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEndOffset, outDC));

    return MS::kSuccess;
}
