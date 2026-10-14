//
//  DPBlend.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/08.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPBlend.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DPrimitiveCopyBudget.h"
#include "common/utility/DPointPayloadBudget.h"
#include "DMayaHeaders.h"
#include "DPData.h"
#include "dpoint_ids.h"

#include <cmath>
#include <vector>

namespace {

struct PointBlendTarget {
    const DPData* data;
    float weight;
    DPAttributeData::ConstPtr weights;
};

// Missing and non-scalar optional fields historically use the constant Blend.
bool isScalarBlendAttribute(const DPAttributeData::ConstPtr& attribute) {
    if (!attribute) return false;
    switch (attribute->getAttrType()) {
    case DP_ATTR_INT: case DP_ATTR_UINT32: case DP_ATTR_INT64:
    case DP_ATTR_FLOAT: case DP_ATTR_DOUBLE: return true;
    default: return false;
    }
}

} // namespace

const MTypeId DPBlend::typeId(ID_DP_BLEND);
MObject DPBlend::inDP;
MObject DPBlend::aBlendArray;
MObject DPBlend::aTargetDP;
MObject DPBlend::aBlend;
MObject DPBlend::aBlendAttribute;
MObject DPBlend::aWeight;
MObject DPBlend::outDP;

void* DPBlend::creator() {
    return ::dommy::maya::createObject("DPBlend", [&]() {
        return new DPBlend();
    });
}

MStatus DPBlend::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology()) return MS::kInvalidParameter;
    const std::size_t pointCount = input->numberOfPoints();
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*input,
        sizeof(DPoint) + sizeof(float), workspace);
    float globalWeight = 0.0f;
    std::vector<PointBlendTarget> targets;
    if (pointCount != 0U) {
        MArrayDataHandle array = data.inputArrayValue(aBlendArray, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const unsigned int count = array.elementCount(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (count > dommy::generation::kMaxConnectedGeometryCount) return MS::kInvalidParameter;
        const MDataHandle weightHandle = data.inputValue(aWeight, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        globalWeight = weightHandle.asFloat();
        if (!std::isfinite(globalWeight)) return MS::kInvalidParameter;
        dommy::attribute::ValidationBudget budget(pointCount);
        for (unsigned int i = 0U; globalWeight > 0.0f && i < count; ++i) {
            DOMMY_RETURN_IF_FAILURE(array.jumpToArrayElement(i));
            MDataHandle element = array.inputValue(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            const float blend = element.child(aBlend).asFloat();
            if (!std::isfinite(blend)) return MS::kInvalidParameter;
            if (blend == 0.0f) continue;

            // Bound repeated point evaluation before binding/validating targets
            // or allocating output geometry. Array iteration is physical, not logical.
            std::size_t targetWork = 0U;
            if (!dommy::generation::checkedProduct(pointCount, 3U,
                    dommy::generation::kMaxWorkItemCount, targetWork) ||
                !dommy::generation::checkedSum(budget.workItems, targetWork,
                    dommy::generation::kMaxWorkItemCount, budget.workItems))
                return MS::kInvalidParameter;
            const MDataHandle targetHandle = element.child(aTargetDP);
            const DPData* target = nullptr;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(targetHandle, DPData::id, target));
            if (!target) return MS::kInvalidParameter;
            const std::size_t targetCount = target->numberOfPoints();
            if (targetCount == 0) return MS::kInvalidParameter;
            if (!dommy::generation::checkedProduct(targetCount, 3U,
                    dommy::generation::kMaxWorkItemCount, targetWork) ||
                !dommy::generation::checkedSum(budget.workItems, targetWork,
                    dommy::generation::kMaxWorkItemCount, budget.workItems) ||
                !dommy::geometry::addCopyMetadataCost(*target, targetCount, budget) ||
                !target->hasValidPointTopology()) return MS::kInvalidParameter;
            const MString name = element.child(aBlendAttribute).asString();
            if (!dommy::generation::checkedSum(budget.stringBytes, name.length(),
                    dommy::generation::kMaxGeneratedStringByteCount, budget.stringBytes))
                return MS::kInvalidParameter;
            DPAttributeData::ConstPtr weights = target->getAttr(name.asChar());
            if (!isScalarBlendAttribute(weights)) weights.reset();
            if (weights && weights->attrSize() != pointCount) return MS::kInvalidParameter;
            workspace.add(1U, sizeof(PointBlendTarget));
            targets.push_back(PointBlendTarget{target, blend, weights});
        }
    }
    DPointList points;
    if (!targets.empty()) {
        points = *input->getPositionPtrConst();
        std::vector<float> weights;
        for (const auto& target : targets) {
            if (target.weights && !dommy::attribute::convertScalarAttribute(
                    target.weights, pointCount, weights)) return MS::kInvalidParameter;
            const DPointList& targetPoints = *target.data->getPositionPtrConst();
            const std::size_t targetCount = targetPoints.size();
            for (std::size_t p = 0U; p < pointCount; ++p) {
                const double rawWeight = static_cast<double>(
                    target.weights ? weights[p] : target.weight) * globalWeight;
                float weight = 0.0f;
                if (!dommy::numeric::checkedFloatCast(rawWeight, weight)) return MS::kInvalidParameter;
                DPoint candidate;
                for (unsigned int axis = 0U; axis < 3U; ++axis)
                    if (!dommy::attribute::checkedLinearBlend(
                            points[p][axis], targetPoints[p % targetCount][axis],
                            weight, candidate[axis])) return MS::kInvalidParameter;
                points[p] = candidate;
            }
        }
    }
    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, &points](DPData& output) -> MStatus {
            return output.copyRepeatedFrom(*input, 1U,
                [&points](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                    if (!points.empty()) point = points[index];
                    return MS::kSuccess;
                });
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPBlend")

MStatus DPBlend::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aTargetDP = tAttr.create("TargetDP", "tdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDP));

    aBlend = nAttr.create("Blend", "bl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlend));

    MFnStringData fnStringData;
    const MObject defaultWeight = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aBlendAttribute = tAttr.create("BlendAttribute", "wattr", MFnData::kString, defaultWeight, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlendAttribute));

    MFnCompoundAttribute cmpAttr;
    aBlendArray = cmpAttr.create("BlendArray", "blar", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aTargetDP));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aBlend));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aBlendAttribute));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlendArray));

    aWeight = nAttr.create("Weight", "w", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));

    // Output data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBlend, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBlendArray, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeight, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPBlend", "initialize", std::current_exception());
}
