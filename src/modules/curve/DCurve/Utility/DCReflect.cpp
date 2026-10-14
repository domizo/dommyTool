//
//  DCReflect.cpp
//  dommyTool
//
//  Created by yuzo on 2018/10/25.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCReflect.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DCurveReflection.h"
#include "common/utility/DMeshIntersection.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshInput.h"


const MTypeId DCReflect::typeId( ID_DC_REFLECT );

MObject DCReflect::inDC;

MObject DCReflect::aInObject;
MObject DCReflect::aSampling;
MObject DCReflect::aBias;

MObject DCReflect::outDC;



void *DCReflect::creator()
{
    return ::dommy::maya::createObject("DCReflect", [&]() {
        return new DCReflect();
    });
}



MStatus DCReflect::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle handle = data.inputValue(aSampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int sampling = handle.asInt();
    handle = data.inputValue(aBias, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double bias = handle.asDouble();
    std::size_t sampleCount = 0U, work = 0U;
    if (!std::isfinite(bias) || !dommy::generation::checkedCount(sampling,
            dommy::generation::kMaxCurveSampleCount, sampleCount) || sampleCount < 2U ||
        !dommy::generation::checkedProduct(input->numberOfCurves(), sampleCount,
            dommy::generation::kMaxWorkItemCount, work)) return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    DCurveOutputBatch generatedCurves;
    if (input->numberOfCurves()) {
        MObject meshObject;
        status = dommy::maya::getTransformedMeshObject(aInObject, data, meshObject);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        MFnMesh mesh(meshObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        dommy::maya::MeshAcceleratorGuard<MFnMesh> acceleratorGuard(mesh);
        MMeshIsectAccelParams accelerator = mesh.autoUniformGridParams();
        std::size_t totalPoints = 0U;
        for (auto curve = input->beginCurves(); curve != input->endCurves(); ++curve) {
            MObject object;
            status = generateWorldMCurveFromDcurve(*curve, object);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnNurbsCurve function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            const double length = function.length(kMFnNurbsEpsilon, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            if (!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
            const double step = length / double(sampleCount - 1U);
            MPointArray points;
            bool continuing = false;
            MPoint nextPoint, previous;
            MVector continuationNormal;
            const auto appendPoint = [&](const MPoint& point) -> MStatus {
                if (!dommy::maya::isFloatRepresentablePoint(point) ||
                    points.length() >= dommy::generation::kMaxGeneratedPointCount)
                    return MS::kInvalidParameter;
                return points.append(point);
            };
            for (std::size_t i = 0U; i < sampleCount; ++i) {
                if (continuing) {
                    // Preserve the legacy normal-based continuation after the biased first step.
                    const MPoint candidate = nextPoint + continuationNormal * step;
                    status = appendPoint(candidate);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    nextPoint = candidate;
                    continue;
                }
                const double traveled = length * (double(i) / double(sampleCount - 1U));
                MPoint point;
                MVector tangent;
                status = dommy::curve::sampleAtLength(
                    function, traveled, MSpace::kObject, point, tangent);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (i == 0U) previous = point;
                MFloatPoint source;
                MFloatVector ray;
                if (!dommy::maya::makeFloatRay(point, tangent, source, ray))
                    return MS::kInvalidParameter;
                MFloatPointArray hits;
                status = dommy::maya::checkedAllIntersections(
                    mesh, source, ray, MSpace::kObject, &accelerator, hits);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (hits.length() % 2U != 0U) {
                    MFloatPoint selectedHit = hits[0U];
                    if (i != 0U) {
                        MFloatPoint previousSource;
                        MFloatVector previousRay;
                        if (!dommy::maya::makeFloatRay(previous, tangent, previousSource, previousRay))
                            return MS::kInvalidParameter;
                        bool found = false;
                        status = dommy::maya::checkedClosestIntersection(
                            mesh, previousSource, previousRay, MSpace::kObject,
                            &accelerator, found, selectedHit);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                    }
                    MPoint nearPoint;
                    MVector nearNormal;
                    status = dommy::maya::checkedClosestPointAndNormal(
                        mesh, MPoint(selectedHit), MSpace::kObject,
                        &accelerator, nearPoint, nearNormal);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    bool finished = false;
                    MPoint candidate;
                    if (!dommy::curve::evaluateReflectionHit(point, previous, tangent,
                            nearPoint, nearNormal, step, traveled, length, bias,
                            finished, candidate)) return MS::kInvalidParameter;
                    status = appendPoint(nearPoint);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    if (finished) break;
                    status = appendPoint(candidate);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    // Enter continuation only after both points were accepted.
                    nextPoint = candidate;
                    continuationNormal = nearNormal;
                    continuing = true;
                } else {
                    status = appendPoint(point);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                }
                previous = point;
            }
            if (points.length() >= 2U) {
                if (!dommy::generation::checkedSum(totalPoints, points.length(),
                        dommy::generation::kMaxGeneratedPointCount, totalPoints))
                    return MS::kInvalidParameter;
                if (!generatedCurves.append(DCItem(new DCurveInternalData(
                        points, (*curve)->getDegree())))) return MS::kFailure;
            }
        }
    }
    if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;
    outDCrvs->updateBBox();
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCReflect")



MStatus DCReflect::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aInObject = gAttr.create("InObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    aSampling = nAttr.create("Sampling", "smp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    aBias = nAttr.create("Bias", "ba", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBias));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBias, outDC));

    return MS::kSuccess;
}
