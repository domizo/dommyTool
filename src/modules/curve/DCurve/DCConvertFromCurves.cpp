//
//  DConvertMultiCurves.cpp
//  dommyTool
//
//  Created by yuzo on 2016/05/01.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCConvertFromCurves.h"
#include "common/object/DNodeOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "DCurveConversionPlan.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <utility>

#include <maya/MGlobal.h>

#include <maya/MFnPluginData.h>

#include "DCurveData.h"
#include "DCurveUtil.h"

#include "dcurve_ids.h"

enum { DC_CNV_CURVE = 0, DC_CNV_HIERARCHY, DCConvertCurveType_COUNT };
typedef short DCConvertCurveType;

const MTypeId DCConvertFromCurves::typeId(ID_DC_CONVERT_FROM_CURVE);

MObject DCConvertFromCurves::aConvertType;
MObject DCConvertFromCurves::inCurves;

MObject DCConvertFromCurves::aInHierarchy;
MObject DCConvertFromCurves::aCurrentTime;

MObject DCConvertFromCurves::outDC;

void* DCConvertFromCurves::creator() {
    return ::dommy::maya::createObject("DCConvertFromCurves", [&]() {
        return new DCConvertFromCurves();
    });
}

namespace {

MStatus convertCurveBatch(const MObjectArray& objects, const std::vector<DPIndexList>& groups,
                          std::size_t inputSlots, DCurveData& output) {
    dommy::curve::CurveConversionPlan plan;
    if (!dommy::curve::initializeCurveConversionPlan(objects.length(), groups.size(), inputSlots, plan))
        return MS::kInvalidParameter;
    std::vector<std::pair<int, int>> counts;
    counts.reserve(objects.length());
    // Query sizes only: do not fetch native CV/knot arrays before the complete admission.
    for (unsigned int i = 0U; i < objects.length(); ++i) {
        MStatus status;
        MFnNurbsCurve curve(objects[i], &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int cvs = curve.numCVs(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const int knots = curve.numKnots(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!dommy::curve::admitCurveConversionCounts(cvs, knots, plan)) return MS::kInvalidParameter;
        counts.emplace_back(cvs, knots);
    }
    DCurveData::DCurveList generated;
    generated.reserve(objects.length());
    for (unsigned int i = 0U; i < objects.length(); ++i) {
        DCItem curve;
        DOMMY_RETURN_IF_FAILURE(DCurveInternalData::createFromMayaObject(objects[i], curve));
        if (!curve || curve->numberOfPoints() != static_cast<std::size_t>(counts[i].first) ||
            curve->getKnots().length() != static_cast<unsigned int>(counts[i].second)) return MS::kFailure;
        generated.push_back(curve);
    }
    if (!output.insert(generated)) return MS::kFailure;
    std::size_t actualPoints = 0U;
    if (output.numberOfCurves() != plan.curves || !output.getCheckedTotalCvs(actualPoints) ||
        actualPoints != plan.points) return MS::kFailure;

    const MString prefix("piece");
    for (std::size_t i = 0U; i < groups.size(); ++i) {
        DPIdList mask(plan.curves, false);
        for (std::size_t index : groups[i]) {
            if (index >= mask.size()) return MS::kFailure;
            mask[index] = true;
        }
        const MString name = prefix + i;
        output.addGrp(name.asChar(), mask);
        const DPGroupData::ConstPtr written = output.getGrp(name.asChar());
        if (!written || written->getGroupIds() != mask) return MS::kFailure;
    }
    if (output.numberOfGroups() != groups.size()) return MS::kFailure;
    return MS::kSuccess;
}

} // namespace

MStatus DCConvertFromCurves::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC) return MS::kUnknownParameter;
    MStatus stat;
    MDataHandle control = data.inputValue(aCurrentTime, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    // Preserve the evaluation dependency; this node does not sample another time.
    (void)control.asTime();
    control = data.inputValue(aConvertType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DCConvertCurveType type = control.asShort();
    if (type != DC_CNV_CURVE && type != DC_CNV_HIERARCHY) return MS::kInvalidParameter;

    MObjectArray objects;
    std::vector<DPIndexList> groups;
    std::size_t inputSlots = 0U;
    if (type == DC_CNV_CURVE) {
        MArrayDataHandle inputs = data.inputArrayValue(inCurves, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int count = inputs.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        inputSlots = count;
        dommy::curve::CurveConversionPlan collectionPlan;
        if (!dommy::curve::initializeCurveConversionPlan(0U, 0U, inputSlots, collectionPlan))
            return MS::kInvalidParameter;
        for (unsigned int i = 0U; i < count; ++i) {
            DOMMY_RETURN_IF_FAILURE(inputs.jumpToArrayElement(i));
            MDataHandle input = inputs.inputValue(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (input.type() == MFnData::kNurbsCurve) {
                if (!dommy::curve::initializeCurveConversionPlan(
                        objects.length() + 1U, 0U, inputSlots, collectionPlan)) return MS::kInvalidParameter;
                DOMMY_RETURN_IF_FAILURE(objects.append(input.data()));
            }
        }
        if (objects.length() == 0U) {
            MGlobal::displayWarning("DConvertMultiCurves :Input curve is not have curve data.");
            return MS::kFailure;
        }
    } else {
        MPlug hierarchy(thisMObject(), aInHierarchy);
        const bool isConnected = hierarchy.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!isConnected) return MS::kUnknownParameter; // Retain the legacy empty-hierarchy result.
        MPlugArray connections;
        const bool connected = hierarchy.connectedTo(connections, true, true, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!connected || connections.length() == 0U) return MS::kUnknownParameter;
        const MObject root = connections[0].node(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MFnDagNode parent(root, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const unsigned int childCount = parent.childCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (childCount > dommy::generation::kMaxConnectedGeometryCount) return MS::kInvalidParameter;
        std::size_t remainingVisits = dommy::generation::kMaxConnectedGeometryCount - childCount;
        for (unsigned int i = 0U; i < childCount; ++i) {
            const MObject child = parent.child(i, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            MFnDagNode childNode(child, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MString childType = childNode.typeName(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            bool directCurve = false;
            if (childType == "nurbsCurve") {
                const bool intermediate = childNode.isIntermediateObject(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                directCurve = !intermediate;
            }
            MObjectArray groupCurves;
            if (directCurve) {
                DOMMY_RETURN_IF_FAILURE(groupCurves.append(child));
            } else {
                DOMMY_RETURN_IF_FAILURE(getNurbsCurveNodeFromHierarchy(childNode, groupCurves, &remainingVisits));
            }
            inputSlots = dommy::generation::kMaxConnectedGeometryCount - remainingVisits;
            if (groupCurves.length() == 0U) continue;
            std::size_t nextCount = 0U;
            dommy::curve::CurveConversionPlan collectionPlan;
            if (!dommy::generation::checkedSum(objects.length(), groupCurves.length(),
                    dommy::generation::kMaxGeneratedCurveCount, nextCount) ||
                !dommy::curve::initializeCurveConversionPlan(nextCount, groups.size() + 1U,
                    inputSlots, collectionPlan)) return MS::kInvalidParameter;
            DPIndexList members;
            members.reserve(groupCurves.length());
            for (unsigned int j = 0U; j < groupCurves.length(); ++j) {
                DOMMY_RETURN_IF_FAILURE(objects.append(groupCurves[j]));
                members.push_back(objects.length() - 1U);
            }
            groups.push_back(std::move(members));
        }
        if (objects.length() == 0U) return MS::kUnknownParameter;
    }

    MFnPluginData pluginData;
    DCurveData* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(pluginData, DCurveData::id, output));
    if (!output) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(convertCurveBatch(objects, groups, inputSlots, *output));
    MDataHandle outputHandle = data.outputValue(outDC, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        outputHandle, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCConvertFromCurves")

MStatus DCConvertFromCurves::initialize() try {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    MFnEnumAttribute cnvEnumFn;
    aConvertType = cnvEnumFn.create("ConvertType", "cnvty", DC_CNV_CURVE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cnvEnumFn.addField("Input Curve", DC_CNV_CURVE));
    DOMMY_RETURN_IF_FAILURE(cnvEnumFn.addField("Input Hierarchy", DC_CNV_HIERARCHY));
    DOMMY_RETURN_IF_FAILURE(cnvEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConvertType));

    inCurves = gAttr.create("inCurves", "incs", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurves));

    MFnMessageAttribute fnMsgAttr;
    aInHierarchy = fnMsgAttr.create("InHierarchy", "inhrchy", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(aInHierarchy));

    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create("CurrentTime", "time", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCurves, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConvertType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInHierarchy, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, outDC));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCConvertFromCurves", "initialize", std::current_exception());
}
