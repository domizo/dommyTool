//
//  DCurveFromSurface.cpp
//  dommyTool
//
//  Created by yuzo on 5/1/16.
//
//

#include "DCurveFromSurface.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "DNodeOperation.h"
#include "common/utility/DGenerationLimits.h"

#include <cmath>

const MTypeId DCurveFromSurface::typeId( ID_DC_FROM_SURFACE_GEN );


MObject DCurveFromSurface::inSurface;

MObject DCurveFromSurface::numU;
MObject DCurveFromSurface::numV;

MObject DCurveFromSurface::sampling;

MObject DCurveFromSurface::offsetU;
MObject DCurveFromSurface::offsetV;

MObject DCurveFromSurface::lengthU;
MObject DCurveFromSurface::lengthV;

MObject DCurveFromSurface::isClampU;
MObject DCurveFromSurface::isClampV;

MObject DCurveFromSurface::aIsOpenU;
MObject DCurveFromSurface::aIsOpenV;

MObject DCurveFromSurface::outDC;

void *DCurveFromSurface::creator()
{
    return ::dommy::maya::createObject("DCurveFromSurface", [&]() {
        return new DCurveFromSurface();
    });
}

MStatus DCurveFromSurface::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle rawNumUHandle = data.inputValue(numU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawNumU = rawNumUHandle.asInt();
    const MDataHandle rawNumVHandle = data.inputValue(numV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawNumV = rawNumVHandle.asInt();
    const MDataHandle rawSamplingHandle = data.inputValue(sampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSampling = rawSamplingHandle.asInt();
    const MDataHandle offsetUValueHandle = data.inputValue(offsetU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double offsetUValue = offsetUValueHandle.asFloat();
    const MDataHandle offsetVValueHandle = data.inputValue(offsetV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double offsetVValue = offsetVValueHandle.asFloat();
    const MDataHandle lengthUValueHandle = data.inputValue(lengthU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double lengthUValue = lengthUValueHandle.asFloat();
    const MDataHandle lengthVValueHandle = data.inputValue(lengthV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double lengthVValue = lengthVValueHandle.asFloat();
    const MDataHandle clampUHandle = data.inputValue(isClampU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool clampU = clampUHandle.asBool();
    const MDataHandle clampVHandle = data.inputValue(isClampV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool clampV = clampVHandle.asBool();
    const MDataHandle openUHandle = data.inputValue(aIsOpenU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short openU = openUHandle.asShort();
    const MDataHandle openVHandle = data.inputValue(aIsOpenV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short openV = openVHandle.asShort();

    dommy::curve::SurfaceSamplingPlan plan;
    if (!dommy::curve::checkedSurfaceSamplingPlan(rawNumU, rawNumV, rawSampling, plan) ||
        !std::isfinite(offsetUValue) || !std::isfinite(offsetVValue) ||
        !std::isfinite(lengthUValue) || !std::isfinite(lengthVValue) || openU < 0 || openU > 1 ||
        openV < 0 || openV > 1) {
        MGlobal::displayError(
            "DCurveFromSurface: invalid parameters or excessive generation count.");
        return MS::kInvalidParameter;
    }

    MObject surfaceObject;
    if (!DGetMayaSurfaceFromPlug(inSurface, data, surfaceObject))
        return MS::kInvalidParameter;
    MFnNurbsSurface surface(surfaceObject, &status);
    if (!status)
        return status;
    double startU = 0.0, endU = 0.0, startV = 0.0, endV = 0.0;
    status = surface.getKnotDomain(startU, endU, startV, endV);
    if (!status)
        return status;
    if (!std::isfinite(startU) || !std::isfinite(endU) || endU < startU || !std::isfinite(startV) ||
        !std::isfinite(endV) || endV < startV)
        return MS::kInvalidParameter;

    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(plan.curveCount);
    if (plan.curveCount != 0U) {
        const int degree = plan.samples < 4U ? 1 : 3;
        MDoubleArray knots;
        GenerateDCurveKnots(plan.samples, degree, knots);
        // Preserve source order: all fixed-U curves, then all fixed-V curves.
        for (unsigned int axis = 0U; axis < 2U; ++axis) {
            const bool fixedU = axis == 0U;
            const std::size_t curveCount = fixedU ? plan.curvesU : plan.curvesV;
            for (std::size_t curveIndex = 0U; curveIndex < curveCount; ++curveIndex) {
                double fixedParameter = 0.0;
                if (!dommy::curve::checkedSurfaceParameter(
                        fixedU ? startU : startV, fixedU ? endU : endV,
                        fixedU ? lengthUValue : lengthVValue, fixedU ? offsetUValue : offsetVValue,
                        curveIndex, curveCount, fixedU ? clampU : clampV, fixedParameter)) {
                    return MS::kInvalidParameter;
                }
                MPointArray cvs;
                status = dommy::maya::buildCheckedArray(
                    plan.samples,
                    [&](unsigned int index, MPoint& point) -> MStatus {
                        double varyingParameter = 0.0;
                        if (!dommy::curve::checkedSurfaceParameter(
                                fixedU ? startV : startU, fixedU ? endV : endU,
                                fixedU ? lengthVValue : lengthUValue,
                                fixedU ? offsetVValue : offsetUValue, index, plan.samples,
                                fixedU ? clampV : clampU, varyingParameter)) {
                            return MS::kInvalidParameter;
                        }
                        // Geometry-data input has no DAG path; worldSpace input is already
                        // transformed.
                        MStatus queryStatus = surface.getPointAtParam(
                            fixedU ? fixedParameter : varyingParameter,
                            fixedU ? varyingParameter : fixedParameter, point, MSpace::kObject);
                        if (!queryStatus)
                            return queryStatus;
                        return dommy::maya::isFloatRepresentablePoint(point)
                                   ? MStatus(MS::kSuccess)
                                   : MStatus(MS::kInvalidParameter);
                    },
                    cvs);
                if (!status)
                    return status;
                generatedCurves.push_back(DCItem(
                    new DCurveInternalData(cvs, knots, degree, (fixedU ? openU : openV) != 0)));
            }
        }
    }

    MObject outputObject;
    DCurveData* outputCurves = nullptr;
    status =
        dommy::maya::createPluginDataObject<DCurveData>(DCurveData::id, outputObject, outputCurves);
    if (!status || !outputCurves)
        return status ? MStatus(MS::kFailure) : status;
    std::size_t actualPointCount = 0U;
    if (!outputCurves->insert(generatedCurves) ||
        outputCurves->numberOfCurves() != plan.curveCount ||
        !outputCurves->getCheckedTotalCvs(actualPointCount) || actualPointCount != plan.pointCount)
        return MS::kFailure;
    generatedCurves.clear();
    outputCurves->updateBBox();

    MDataHandle output = data.outputValue(outDC, &status);
    if (!status)
        return status;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputObject, *outputCurves));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveFromSurface")

MStatus DCurveFromSurface::initialize() try {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inSurface = gAttr.create("inSurface", "ins", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inSurface));

    numU = nAttr.create("numU", "nuu", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(numU));

    numV = nAttr.create("numV", "nuv", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(numV));

    sampling = nAttr.create("sampling", "smp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(8));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    offsetU = nAttr.create("offsetU", "ofu", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetU));

    offsetV = nAttr.create("offsetV", "ofv", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetV));
    lengthU = nAttr.create("lengthU", "lenu", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(lengthU));

    lengthV = nAttr.create("lengthV", "lenv", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(lengthV));

    isClampU = nAttr.create("isClampU", "iscu", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isClampU));

    isClampV = nAttr.create("isClampV", "iscv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isClampV));

    MFnEnumAttribute openUEnumFn;
    aIsOpenU = openUEnumFn.create("IsOpenU", "isopu", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(openUEnumFn.addField("Close", 0));
    DOMMY_RETURN_IF_FAILURE(openUEnumFn.addField("Open", 1));
    DOMMY_RETURN_IF_FAILURE(openUEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOpenU));

    MFnEnumAttribute openVEnumFn;
    aIsOpenV = openVEnumFn.create("IsOpenV", "isopv", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.addField("Close", 0));
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.addField("Open", 1));
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOpenV));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inSurface, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(numU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(numV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(lengthU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(lengthV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isClampU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isClampV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsOpenU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsOpenV, outDC));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveFromSurface", "initialize", std::current_exception());
}
