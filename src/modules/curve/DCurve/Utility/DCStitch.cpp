//
//  DCStitch.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCStitch.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DMayaValueValidation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"



const MTypeId DCStitch::typeId( ID_DC_STITCH );

MObject DCStitch::inDC;

MObject DCStitch::aSampling;

MObject DCStitch::aRange;
MObject DCStitch::aOffset;

MObject DCStitch::outDC;


void *DCStitch::creator()
{
    return ::dommy::maya::createObject("DCStitch", [&]() {
        return new DCStitch();
    });
}



MStatus DCStitch::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aSampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int sampling = handle.asInt();
    handle = data.inputValue(aRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double rangeFirst = handle.asDouble2()[0];
    const double rangeSecond = handle.asDouble2()[1];
    handle = data.inputValue(aOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double offset = handle.asDouble();
    if (!std::isfinite(rangeFirst) || !std::isfinite(rangeSecond) ||
        !std::isfinite(offset) || rangeFirst < 0.0 || rangeFirst > 1.0 ||
        rangeSecond < 0.0 || rangeSecond > 1.0) return MS::kInvalidParameter;
    const double minimum = std::min(rangeFirst, rangeSecond);
    const double range = std::max(rangeFirst, rangeSecond) - minimum;
    dommy::curve::StitchSamplingPlan plan;
    if (!dommy::curve::checkedStitchSamplingPlan(input->numberOfCurves(), sampling, plan))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    DCurveOutputBatch generatedCurves;
    MPointArray previous;
    if (plan.curves != 0U && range > 0.0) {
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
            status = dommy::maya::buildCheckedArray(plan.samples,
                [&](unsigned int i, MPoint& point) -> MStatus {
                    const double fraction = double(i) / double(plan.samples - 1U);
                    const double distance = length * (minimum + range * fraction);
                    if (!std::isfinite(distance)) return MS::kInvalidParameter;
                    MStatus queryStatus;
                    const double parameter = function.findParamFromLength(distance, &queryStatus);
                    if (queryStatus != MS::kSuccess) return queryStatus;
                    const double shifted = parameter + offset;
                    if (!std::isfinite(parameter) || !std::isfinite(shifted)) return MS::kInvalidParameter;
                    queryStatus = function.getPointAtParam(shifted, point, MSpace::kObject);
                    if (queryStatus != MS::kSuccess) return queryStatus;
                    std::array<double, 3> world;
                    if (!dommy::numeric::checkedTransformPoint(point, matrix, world))
                        return MS::kInvalidParameter;
                    point = MPoint(world[0], world[1], world[2]);
                    return dommy::maya::isFloatRepresentablePoint(point)
                        ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
                }, samples);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (index != 0U) {
                if (previous.length() != plan.samples) return MS::kFailure;
                for (unsigned int i = 0U; i < samples.length(); ++i) {
                    MPointArray points;
                    status = dommy::maya::buildCheckedArray(2U,
                        [&](unsigned int endpoint, MPoint& point) {
                            point = endpoint == 0U ? previous[i] : samples[i];
                            return MS::kSuccess;
                        }, points);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    if (!generatedCurves.append(DCItem(new DCurveInternalData(points, 1)))) return MS::kFailure;
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
DOMMY_COMPUTE_CATCH("DCStitch")

        
        
        
        
        


MStatus DCStitch::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aSampling = nAttr.create("Sampling", "smp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aRange = nAttr.create("Range", "rng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRange));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRange, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));

    return MS::kSuccess;
}
