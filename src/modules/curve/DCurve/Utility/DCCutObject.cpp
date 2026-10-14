//
//  DCCutObject.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/18.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCCutObject.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshIntersection.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshInput.h"


const MTypeId DCCutObject::typeId( ID_DC_CUT_OBJECT );

MObject DCCutObject::inDC;

MObject DCCutObject::aInObject;
MObject DCCutObject::aSampling;
MObject DCCutObject::aFitCutPoint;

MObject DCCutObject::outDC;



void *DCCutObject::creator()
{
    return ::dommy::maya::createObject("DCCutObject", [&]() {
        return new DCCutObject();
    });
}



MStatus DCCutObject::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle handle = data.inputValue(aSampling, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int sampling = handle.asInt();
    handle = data.inputValue(aFitCutPoint, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool isFit = handle.asBool();
    std::size_t sampleCount = 0U, work = 0U;
    if (!dommy::generation::checkedCount(sampling,
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
            MPointArray points;
            bool wasInside = false;
            MPoint previousHit;
            const auto appendPoint = [&](const MPoint& point) -> MStatus {
                if (!dommy::maya::isFloatRepresentablePoint(point) ||
                    points.length() >= dommy::generation::kMaxGeneratedPointCount)
                    return MS::kInvalidParameter;
                return points.append(point);
            };
            const auto finishSegment = [&]() -> MStatus {
                if (points.length() >= 2U) {
                    if (!dommy::generation::checkedSum(totalPoints, points.length(),
                            dommy::generation::kMaxGeneratedPointCount, totalPoints))
                        return MS::kInvalidParameter;
                    if (!generatedCurves.append(DCItem(new DCurveInternalData(
                            points, (*curve)->getDegree())))) return MS::kFailure;
                }
                return points.clear();
            };
            for (std::size_t i = 0U; i < sampleCount; ++i) {
                MPoint point;
                MVector tangent;
                status = dommy::curve::sampleAtLength(function,
                    length * (double(i) / double(sampleCount - 1U)), MSpace::kObject, point, tangent);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                MFloatPoint source;
                MFloatVector ray;
                if (!dommy::maya::makeFloatRay(point, tangent, source, ray))
                    return MS::kInvalidParameter;
                MFloatPointArray hits;
                status = dommy::maya::checkedAllIntersections(
                    mesh, source, ray, MSpace::kObject, &accelerator, hits);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                const bool inside = hits.length() % 2U != 0U;
                if (inside) {
                    previousHit = hits[0U];
                    if (isFit && !wasInside) {
                        MFloatPointArray reverseHits;
                        status = dommy::maya::checkedAllIntersections(
                            mesh, source, -ray, MSpace::kObject, &accelerator, reverseHits);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                        if (reverseHits.length()) {
                            status = appendPoint(reverseHits[0U]);
                            CHECK_MSTATUS_AND_RETURN_IT(status);
                        }
                    }
                    status = appendPoint(point);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                } else if (wasInside) {
                    if (isFit) {
                        status = appendPoint(previousHit);
                        CHECK_MSTATUS_AND_RETURN_IT(status);
                    }
                    status = finishSegment();
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                }
                wasInside = inside;
            }
            status = finishSegment();
            CHECK_MSTATUS_AND_RETURN_IT(status);
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
DOMMY_COMPUTE_CATCH("DCCutObject")


MStatus DCCutObject::initialize() {
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

    aFitCutPoint = nAttr.create("FitCutPoint", "fcp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFitCutPoint));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInObject, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFitCutPoint, outDC));

    return MS::kSuccess;
}
