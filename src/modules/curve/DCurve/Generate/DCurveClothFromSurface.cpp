//
//  DCurveClothFromSurface.cpp
//  dommyTool
//
//  Created by yuzo on 5/6/16.
//

#include "DCurveClothFromSurface.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DCurveClothSamplingPlan.h"
#include "DNodeOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DAttributeLookup.h"

#include "DCurveData.h"
#include "dcurve_ids.h"

#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace {

MStatus querySurfacePoint(MFnNurbsSurface& surface,
                          bool fixedParameterIsU,
                          double fixedParameter,
                          double varyingParameter,
                          MPoint& point)
{
    if (!std::isfinite(fixedParameter) || !std::isfinite(varyingParameter)) {
        return MS::kInvalidParameter;
    }

    const double u = fixedParameterIsU ? fixedParameter : varyingParameter;
    const double v = fixedParameterIsU ? varyingParameter : fixedParameter;
    // The input is geometry data; worldSpace connections are already baked.
    MStatus status = surface.getPointAtParam(u, v, point, MSpace::kObject);
    if (!status) return status;
    return dommy::maya::isFinitePoint(point)
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kFailure);
}

MStatus querySurfaceNormal(MFnNurbsSurface& surface,
                           bool fixedParameterIsU,
                           double fixedParameter,
                           double varyingParameter,
                           MVector& normal)
{
    if (!std::isfinite(fixedParameter) || !std::isfinite(varyingParameter)) {
        return MS::kInvalidParameter;
    }

    const double u = fixedParameterIsU ? fixedParameter : varyingParameter;
    const double v = fixedParameterIsU ? varyingParameter : fixedParameter;
    MStatus status;
    normal = surface.normal(u, v, MSpace::kObject, &status);
    if (!status) return status;
    return dommy::maya::isFiniteVector(normal)
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kFailure);
}

MStatus appendPoint(MPointArray& points, const MPoint& point)
{
    if (!dommy::maya::isFinitePoint(point)) return MS::kInvalidParameter;
    return points.append(point);
}

MStatus buildClothCurve(MFnNurbsSurface& surface,
                        bool fixedParameterIsU,
                        double fixedParameter,
                        double varyingStart,
                        double varyingEnd,
                        double varyingStep,
                        std::size_t varyingSampleCount,
                        std::size_t expectedPointCount,
                        std::size_t lineIndex,
                        double oddSampleFlip,
                        double waveHeight,
                        bool addBiasPoints,
                        double waveWidthBias,
                        bool addNormalAttribute,
                        const std::string& normalAttributeName,
                        DCItem& curve)
{
    MPointArray points;
    DVec3DataList normals;
    if (addNormalAttribute) normals.reserve(expectedPointCount);

    MPoint startPoint;
    MStatus status = querySurfacePoint(
        surface, fixedParameterIsU, fixedParameter, varyingStart, startPoint);
    if (!status) return status;
    status = appendPoint(points, startPoint);
    if (!status) return status;
    if (addNormalAttribute) normals.push_back(DVec3Data(0.0, 0.0, 0.0));

    double previousParameter = varyingStart;
    const double lineFlip = lineIndex % 2U == 0U ? 1.0 : -1.0;
    for (std::size_t sampleIndex = 1U;
         sampleIndex <= varyingSampleCount;
         ++sampleIndex) {
        const double currentParameter =
            varyingStart + varyingStep * static_cast<double>(sampleIndex);
        const double middleParameter =
            previousParameter + (currentParameter - previousParameter) * 0.5;
        const double sampleFlip = sampleIndex % 2U == 0U
            ? -oddSampleFlip
            : oddSampleFlip;
        const double displacementScale = sampleFlip * lineFlip;

        MPoint middlePoint;
        MPoint innerPoint;
        MVector innerNormal;
        status = querySurfacePoint(
            surface, fixedParameterIsU, fixedParameter, middleParameter,
            middlePoint);
        if (!status) return status;
        status = querySurfacePoint(
            surface, fixedParameterIsU, fixedParameter, currentParameter,
            innerPoint);
        if (!status) return status;
        status = querySurfaceNormal(
            surface, fixedParameterIsU, fixedParameter, currentParameter,
            innerNormal);
        if (!status) return status;
        innerPoint += innerNormal * (waveHeight * displacementScale);
        if (!dommy::maya::isFinitePoint(innerPoint)) return MS::kFailure;

        status = appendPoint(points, middlePoint);
        if (!status) return status;
        if (addNormalAttribute) {
            normals.push_back(DVec3Data(0.0, 0.0, 0.0));
        }

        if (addBiasPoints) {
            const double biasDistance =
                (currentParameter - middleParameter) * waveWidthBias;
            const double preParameter = std::max(
                varyingStart,
                std::min(varyingEnd, currentParameter - biasDistance));
            const double postParameter = std::max(
                varyingStart,
                std::min(varyingEnd, currentParameter + biasDistance));

            MPoint prePoint;
            MPoint postPoint;
            MVector preNormal;
            MVector postNormal;
            status = querySurfacePoint(
                surface, fixedParameterIsU, fixedParameter, preParameter,
                prePoint);
            if (!status) return status;
            status = querySurfacePoint(
                surface, fixedParameterIsU, fixedParameter, postParameter,
                postPoint);
            if (!status) return status;
            status = querySurfaceNormal(
                surface, fixedParameterIsU, fixedParameter, preParameter,
                preNormal);
            if (!status) return status;
            status = querySurfaceNormal(
                surface, fixedParameterIsU, fixedParameter, postParameter,
                postNormal);
            if (!status) return status;

            prePoint += preNormal * (waveHeight * displacementScale);
            postPoint += postNormal * (waveHeight * displacementScale);
            status = appendPoint(points, prePoint);
            if (!status) return status;
            status = appendPoint(points, innerPoint);
            if (!status) return status;
            status = appendPoint(points, postPoint);
            if (!status) return status;

            if (addNormalAttribute) {
                normals.push_back(DVec3Data(
                    preNormal.x * displacementScale,
                    preNormal.y * displacementScale,
                    preNormal.z * displacementScale));
                normals.push_back(DVec3Data(
                    innerNormal.x * displacementScale,
                    innerNormal.y * displacementScale,
                    innerNormal.z * displacementScale));
                normals.push_back(DVec3Data(
                    postNormal.x * displacementScale,
                    postNormal.y * displacementScale,
                    postNormal.z * displacementScale));
            }
        } else {
            status = appendPoint(points, innerPoint);
            if (!status) return status;
            if (addNormalAttribute) {
                normals.push_back(DVec3Data(
                    innerNormal.x * displacementScale,
                    innerNormal.y * displacementScale,
                    innerNormal.z * displacementScale));
            }
        }

        previousParameter = currentParameter;
    }

    MPoint endPoint;
    status = querySurfacePoint(
        surface, fixedParameterIsU, fixedParameter, varyingEnd, endPoint);
    if (!status) return status;
    status = appendPoint(points, endPoint);
    if (!status) return status;
    if (addNormalAttribute) normals.push_back(DVec3Data(0.0, 0.0, 0.0));

    if (static_cast<std::size_t>(points.length()) != expectedPointCount ||
        (addNormalAttribute && normals.size() != expectedPointCount)) {
        return MS::kFailure;
    }

    DCItem candidate(new DCurveInternalData(points, 3));
    if (!candidate || !candidate->hasValidCurveTopology()) {
        return MS::kFailure;
    }
    if (addNormalAttribute) {
        candidate->addAttrWithValues(normalAttributeName, normals);
    }
    curve = candidate;
    return MS::kSuccess;
}

} // namespace

const MTypeId DCurveClothFromSurface::typeId(
    ID_DC_CLOTH_FROM_SURFACE_GEN);

MObject DCurveClothFromSurface::inSurface;
MObject DCurveClothFromSurface::samplingU;
MObject DCurveClothFromSurface::samplingV;
MObject DCurveClothFromSurface::waveHeight;
MObject DCurveClothFromSurface::aAddBiasPoint;
MObject DCurveClothFromSurface::aWaveWidthBias;
MObject DCurveClothFromSurface::aAddNormalAttribute;
MObject DCurveClothFromSurface::aNormalAttributeName;
MObject DCurveClothFromSurface::outDC;

void* DCurveClothFromSurface::creator()
{
    return ::dommy::maya::createObject("DCurveClothFromSurface", [&]() {
        return new DCurveClothFromSurface();
    });
}

MStatus DCurveClothFromSurface::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle rawSamplingUHandle = data.inputValue(samplingU, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSamplingU = rawSamplingUHandle.asInt();
    const MDataHandle rawSamplingVHandle = data.inputValue(samplingV, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSamplingV = rawSamplingVHandle.asInt();
    const MDataHandle waveHeightValueHandle = data.inputValue(waveHeight, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double waveHeightValue = waveHeightValueHandle.asFloat();
    const MDataHandle addBiasPointsHandle = data.inputValue(aAddBiasPoint, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool addBiasPoints = addBiasPointsHandle.asBool();
    const MDataHandle waveWidthBiasValueHandle = data.inputValue(aWaveWidthBias, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double waveWidthBiasValue = waveWidthBiasValueHandle.asFloat();
    const MDataHandle addNormalAttributeHandle = data.inputValue(aAddNormalAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool addNormalAttribute = addNormalAttributeHandle.asBool();
    const MDataHandle normalAttributeValueHandle = data.inputValue(aNormalAttributeName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString normalAttributeValue = normalAttributeValueHandle.asString();

    if (!std::isfinite(waveHeightValue) || !std::isfinite(waveWidthBiasValue) ||
        waveWidthBiasValue < 0.0 || waveWidthBiasValue > 1.0) {
        MGlobal::displayError("DCurveClothFromSurface: wave parameters are invalid.");
        return MS::kInvalidParameter;
    }

    std::string normalAttributeName = normalAttributeValue.asChar();
    if (addNormalAttribute && normalAttributeName.empty()) {
        normalAttributeName = "CN";
    }

    MObject surfaceObject;
    if (!DGetMayaSurfaceFromPlug(inSurface, data, surfaceObject)) {
        MGlobal::displayWarning("DCurveClothFromSurface: input is not a NURBS surface.");
        return MS::kFailure;
    }

    MFnNurbsSurface surface;
    status = surface.setObject(surfaceObject);
    if (!status)
        return status;

    double startU = 0.0;
    double endU = 0.0;
    double startV = 0.0;
    double endV = 0.0;
    status = surface.getKnotDomain(startU, endU, startV, endV);
    if (!status)
        return status;

    dommy::curve::ClothSamplingPlan plan;
    if (!dommy::curve::checkedClothSamplingPlan(rawSamplingU, rawSamplingV, addBiasPoints, startU,
                                                endU, startV, endV,
                                                dommy::generation::kMaxGeneratedPointCount,
                                                dommy::generation::kMaxWorkItemCount, plan)) {
        MGlobal::displayError(
            "DCurveClothFromSurface: sampling or surface domain exceeds the safety budget.");
        return MS::kInvalidParameter;
    }

    std::size_t estimatedBytes = plan.estimatedBytes;
    if (addNormalAttribute &&
        (!dommy::attribute::validAttributeName(normalAttributeName) ||
         !dommy::curve::checkedClothNormalStorage(plan, normalAttributeName.size(), estimatedBytes)))
        return MS::kInvalidParameter;

    DCurveData::DCurveList curves;
    curves.reserve(plan.totalCurveCount);
    for (std::size_t uIndex = 0U; uIndex < plan.curveCountU; ++uIndex) {
        const double uParameter = dommy::curve::clothParameterAt(plan.startU, plan.endU, plan.stepU,
                                                                 uIndex, plan.curveCountU);
        DCItem curve;
        status = buildClothCurve(surface, true, uParameter, plan.startV, plan.endV, plan.stepV,
                                 plan.samplingV, plan.pointsPerUCurve, uIndex, -1.0,
                                 waveHeightValue, addBiasPoints, waveWidthBiasValue,
                                 addNormalAttribute, normalAttributeName, curve);
        if (!status)
            return status;
        curves.push_back(curve);
    }

    for (std::size_t vIndex = 0U; vIndex < plan.curveCountV; ++vIndex) {
        const double vParameter = dommy::curve::clothParameterAt(plan.startV, plan.endV, plan.stepV,
                                                                 vIndex, plan.curveCountV);
        DCItem curve;
        status = buildClothCurve(surface, false, vParameter, plan.startU, plan.endU, plan.stepU,
                                 plan.samplingU, plan.pointsPerVCurve, vIndex, 1.0, waveHeightValue,
                                 addBiasPoints, waveWidthBiasValue, addNormalAttribute,
                                 normalAttributeName, curve);
        if (!status)
            return status;
        curves.push_back(curve);
    }

    MFnPluginData pluginData;
    const MObject outputObject = pluginData.create(DCurveData::id, &status);
    if (!status)
        return status;
    if (outputObject.isNull())
        return MS::kFailure;

    DCurveData* outputCurves = NULL;
    status = dommy::maya::getPluginData(pluginData, outputCurves);
    if (!status)
        return status;
    if (!outputCurves->insert(curves) || !outputCurves->hasValidCurveTopology()) {
        return MS::kFailure;
    }
    outputCurves->updateBBox();

    MDataHandle output = data.outputValue(outDC, &status);
    if (!status)
        return status;
    status = dommy::maya::publishTypedPluginDataObject<DCurveData>(output, pluginData.object(),
                                                                   DCurveData::id);
    if (!status)
        return status;
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveClothFromSurface")

MStatus DCurveClothFromSurface::initialize() try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inSurface = genericAttribute.create("inSurface", "ins", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inSurface));

    samplingU = numericAttribute.create("samplingU", "su", MFnNumericData::kInt, 4, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(
        numericAttribute.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount - 2U)));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingU));

    samplingV = numericAttribute.create("samplingV", "sv", MFnNumericData::kInt, 4, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(1));
    DOMMY_RETURN_IF_FAILURE(
        numericAttribute.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount - 2U)));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingV));

    waveHeight =
        numericAttribute.create("waveHeight", "wh", MFnNumericData::kFloat, 0.02f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(waveHeight));

    aAddBiasPoint =
        numericAttribute.create("AddBiasPoint", "adbapt", MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddBiasPoint));

    aWaveWidthBias =
        numericAttribute.create("WaveWidthBias", "wvb", MFnNumericData::kFloat, 0.5f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWaveWidthBias));

    aAddNormalAttribute = numericAttribute.create("AddNormalAttribute", "adnattr",
                                                  MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddNormalAttribute));

    const MObject defaultNormalName = stringData.create("N", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aNormalAttributeName = typedAttribute.create("NormalAttributeName", "nattr", MFnData::kString,
                                                 defaultNormalName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttributeName));

    outDC = typedAttribute.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inSurface, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingU, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(waveHeight, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddBiasPoint, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWaveWidthBias, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddNormalAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalAttributeName, outDC));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveClothFromSurface", "initialize", std::current_exception());
}
