//
//  DCFillBetween.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCFillBetween.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DPointTransform.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include <cmath>


const MTypeId DCFillBetween::typeId( ID_DC_FILL_BETWEEN );

MObject DCFillBetween::inDC;

MObject DCFillBetween::aCurveCount;
MObject DCFillBetween::aSampling;

MObject DCFillBetween::aExportPrimitiveAttribute;
MObject DCFillBetween::aExportPointAttribute;

MObject DCFillBetween::outDC;

void *DCFillBetween::creator()
{
    return ::dommy::maya::createObject("DCFillBetween", [&]() {
        return new DCFillBetween();
    });
}


MStatus DCFillBetween::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle handle = data.inputValue(aCurveCount, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int rawFillCount = handle.asInt();
    handle = data.inputValue(aSampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int rawSampling = handle.asInt();
    const std::size_t inputCurveCount = input->numberOfCurves();
    std::size_t fillCount = 0U, sampling = 0U, outputCurveCount = 0U;
    std::size_t outputPointCount = 0U, sampledInputPointCount = 0U, totalWorkItemCount = 0U;
    if (inputCurveCount > dommy::generation::kMaxGeneratedCurveCount ||
        !dommy::generation::checkedCount(rawFillCount,
            dommy::generation::kMaxGeneratedCurveCount, fillCount) ||
        !dommy::generation::checkedCount(rawSampling,
            dommy::generation::kMaxCurveSampleCount, sampling) || sampling < 2U ||
        !dommy::generation::checkedProduct(inputCurveCount ? inputCurveCount - 1U : 0U,
            fillCount, dommy::generation::kMaxGeneratedCurveCount, outputCurveCount) ||
        !dommy::generation::checkedProduct(outputCurveCount, sampling,
            dommy::generation::kMaxGeneratedPointCount, outputPointCount) ||
        !dommy::generation::checkedProduct(inputCurveCount, sampling,
            dommy::generation::kMaxWorkItemCount, sampledInputPointCount) ||
        !dommy::generation::checkedSum(sampledInputPointCount, outputPointCount,
            dommy::generation::kMaxWorkItemCount, totalWorkItemCount))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    DCurveOutputBatch generatedCurves;
    MPointArray previous;
    if (outputCurveCount != 0U) {
        std::size_t index = 0U;
        for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve, ++index) {
            const MMatrix matrix = (*curve)->getTransformMatrix();
            if (!dommy::numeric::isFiniteAffineTransform(matrix)) return MS::kInvalidParameter;
            MObject object;
            status = generateMCurveFromDcurve(*curve, object);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnNurbsCurve function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const double length = function.length(kMFnNurbsEpsilon, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
            MPointArray samples;
            status = dommy::maya::buildCheckedArray(static_cast<unsigned int>(sampling),
                [&](unsigned int i, MPoint& point) -> MStatus {
                    const double distance = length * (double(i) / double(sampling - 1U));
                    MStatus queryStatus;
                    const double parameter = function.findParamFromLength(distance, &queryStatus);
                    if (queryStatus != MS::kSuccess) return queryStatus;
                    if (!std::isfinite(parameter)) return MS::kInvalidParameter;
                    queryStatus = function.getPointAtParam(parameter, point, MSpace::kObject);
                    if (queryStatus != MS::kSuccess) return queryStatus;
                    std::array<double, 3> world;
                    if (!dommy::numeric::checkedTransformPoint(point, matrix, world))
                        return MS::kInvalidParameter;
                    point = MPoint(world[0], world[1], world[2]);
                    return MS::kSuccess;
                }, samples);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (index != 0U) {
                if (previous.length() != sampling || samples.length() != sampling) return MS::kFailure;
                for (std::size_t fill = 1U; fill <= fillCount; ++fill) {
                    const double fraction = double(fill) / double(fillCount + 1U);
                    MPointArray points;
                    status = dommy::maya::buildCheckedArray(static_cast<unsigned int>(sampling),
                        [&](unsigned int i, MPoint& point) -> MStatus {
                            for (unsigned int axis = 0U; axis < 3U; ++axis)
                                if (!dommy::attribute::checkedLinearBlend(
                                        previous[i][axis], samples[i][axis], fraction, point[axis]))
                                    return MS::kInvalidParameter;
                            return dommy::maya::isFloatRepresentablePoint(point)
                                ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
                        }, points);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    // Keep the historical second-curve degree and generated open form.
                    MDoubleArray knots;
                    GenerateDCurveKnots(points.length(), (*curve)->getDegree(), knots);
                    if (!generatedCurves.append(DCItem(new DCurveInternalData(
                            points, knots, (*curve)->getDegree())))) return MS::kFailure;
                }
            }
            status = dommy::maya::copyCheckedArray(samples, previous);
            CHECK_MSTATUS_AND_RETURN_IT(status);
        }
    }
    if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;
    outDCrvs->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCFillBetween")




MStatus DCFillBetween::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aCurveCount = nAttr.create("CurveCount", "crvcnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveCount));

    aSampling = nAttr.create("Sampling", "smp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    const MObject aExportPrimitiveAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportPrimitiveAttribute = tAttr.create("ExportPrimitiveAttribute", "exprm", MFnData::kString,
                                             aExportPrimitiveAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPrimitiveAttribute));

    const MObject aExportPointAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportPointAttribute = tAttr.create("ExportPointAttribute", "expt", MFnData::kString,
                                         aExportPointAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurveCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportPrimitiveAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportPointAttribute, outDC));

    return MS::kSuccess;
}
