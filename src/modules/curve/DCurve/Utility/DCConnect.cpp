//
//  DCConnect.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/08.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCConnect.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveConnection.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointTransform.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"


#include <cmath>


const MTypeId DCConnect::typeId( ID_DC_CONNECT );

MObject DCConnect::inDC;

MObject DCConnect::aDistance;

MObject DCConnect::aIsAngleCheck;
MObject DCConnect::aAngle;

MObject DCConnect::aIsOpen;
MObject DCConnect::aDegree;

MObject DCConnect::outDC;



void *DCConnect::creator()
{
    return ::dommy::maya::createObject("DCConnect", [&]() {
        return new DCConnect();
    });
}


MStatus DCConnect::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aDistance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double distance = handle.asDouble();
    handle = data.inputValue(aIsAngleCheck, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool checkAngle = handle.asBool();
    handle = data.inputValue(aAngle, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double angle = handle.asDouble(); // Preserve the legacy radian threshold.
    handle = data.inputValue(aDegree, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short degreeType = handle.asShort();
    handle = data.inputValue(aIsOpen, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short openType = handle.asShort();
    if (!std::isfinite(distance) || distance < 0.0 || !std::isfinite(angle) || angle < 0.0 ||
        (degreeType != DC_kLinear && degreeType != DC_kCubic) ||
        (openType != DC_kOpen && openType != DC_kClose)) return MS::kInvalidParameter;

    const std::size_t count = input->numberOfCurves();
    std::size_t work = 0U, totalPoints = 0U;
    if (count > dommy::generation::kMaxGeneratedCurveCount ||
        !dommy::generation::checkedProduct(count, count,
            dommy::generation::kMaxWorkItemCount / 4U, work)) return MS::kInvalidParameter;
    std::vector<dommy::curve::ConnectionEndpoints> endpoints(count);
    std::vector<MMatrix> matrices(count);
    for (std::size_t i = 0U; i < count; ++i) {
        auto curve = input->getCurvePtr(i);
        matrices[i] = curve->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(matrices[i])) return MS::kInvalidParameter;
        const DPointList& points = *curve->getPositionPtrConst();
        if (!dommy::generation::checkedSum(totalPoints, points.size(),
                dommy::generation::kMaxGeneratedPointCount, totalPoints))
            return MS::kInvalidParameter;
        for (unsigned int end = 0U; end < 2U; ++end) {
            const auto& point = end == 0U ? points.front() : points.back();
            if (!dommy::numeric::checkedTransformPoint(point, matrices[i], endpoints[i].points[end]))
                return MS::kInvalidParameter;
        }
        if (checkAngle) {
            MObject object;
            status = generateMCurveFromDcurve(curve, object);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MFnNurbsCurve function(object, &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            double first = 0.0, last = 0.0;
            status = function.getKnotDomain(first, last);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            for (unsigned int end = 0U; end < 2U; ++end) {
                MVector tangent;
                status = dommy::curve::directionAtParameter(function, end == 0U ? first : last,
                    false, MSpace::kObject, tangent);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!dommy::numeric::checkedTransformDirection(
                        tangent, matrices[i], endpoints[i].tangents[end]) ||
                    !dommy::curve::normalizeDirection(endpoints[i].tangents[end]))
                    return MS::kInvalidParameter;
            }
        }
    }
    std::vector<dommy::curve::CurveChain> chains;
    if (!dommy::curve::buildCurveConnectionChains(endpoints, distance, checkAngle, angle, chains))
        return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    DCurveOutputBatch generatedCurves;
    for (const auto& chain : chains) {
        dommy::maya::CheckedArray<MPointArray> points(
            static_cast<unsigned int>(dommy::generation::kMaxGeneratedPointCount));
        if (!dommy::curve::emitCurveChain<MPoint>(chain,
                [&](std::size_t index) -> const DPointList& {
                    return *input->getCurve(index).getPositionPtrConst();
                },
                [&](std::size_t index, const DPoint& value, MPoint& point) {
                    std::array<double, 3> world;
                    if (!dommy::numeric::checkedTransformPoint(value, matrices[index], world)) return false;
                    point = MPoint(world[0], world[1], world[2]);
                    return dommy::maya::isFloatRepresentablePoint(point);
                },
                [&](const MPoint& point) { return points.append(point) == MS::kSuccess; })) return MS::kFailure;
        const short degree = degreeType == DC_kLinear ? 1 : 3;
        if (!generatedCurves.append(DCItem(new DCurveInternalData(
                points, degree, openType == DC_kOpen)))) return MS::kFailure;
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
DOMMY_COMPUTE_CATCH("DCConnect")
        



MStatus DCConnect::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aDistance = nAttr.create("Distance", "dist", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    aIsAngleCheck = nAttr.create("IsAngleCheck", "isang", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsAngleCheck));

    aAngle = nAttr.create("Angle", "ang", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(90.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(6.283));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngle));

    MFnEnumAttribute degEnumFn;
    aDegree = degEnumFn.create("Degree", "deg", DC_kCubic, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField("Linear", DC_kLinear));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.addField("Cubic", DC_kCubic));
    DOMMY_RETURN_IF_FAILURE(degEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDegree));

    MFnEnumAttribute openVEnumFn;
    aIsOpen = openVEnumFn.create("IsOpen", "isop", DC_kOpen, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.addField("Close", DC_kClose));
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.addField("Open", DC_kOpen));
    DOMMY_RETURN_IF_FAILURE(openVEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOpen));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistance, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsAngleCheck, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAngle, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDegree, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsOpen, outDC));

    return MS::kSuccess;
}
