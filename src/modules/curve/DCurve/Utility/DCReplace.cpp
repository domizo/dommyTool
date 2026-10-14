//
//  DCReplace.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCReplace.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DPointTransform.h"
#include "DCurveReplacementPlan.h"

#include <map>
#include <utility>



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

namespace {

MStatus preflightCurveReplacements(const DCurveData& input, const DCurveData& target,
                                   const DMatrixArray* restMatrices,
                                   const dommy::curve::CurveReplacementPlan& plan) {
    dommy::curve::CurveBatchBudget budget;
    for (const auto& replacement : plan.curves) {
        if (!restMatrices || replacement.source >= input.numberOfCurves() ||
            replacement.source >= restMatrices->size() ||
            replacement.target >= target.numberOfCurves())
            return MS::kInvalidParameter;
        if (!dommy::numeric::isFiniteAffineTransform(
                input.getCurve(replacement.source).getTransformMatrix()) ||
            !dommy::numeric::isFiniteAffineTransform((*restMatrices)[replacement.source]) ||
            !target.getCurve(replacement.target).appendOperationCost(budget))
            return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DCReplace::typeId( ID_DC_REPLACE );


MObject DCReplace::inDC;

MObject DCReplace::aTargetDC;

MObject DCReplace::aSource;
MObject DCReplace::aRestMatrix;
MObject DCReplace::outDC;


void *DCReplace::creator()
{
    return ::dommy::maya::createObject("DCReplace", [&]() {
        return new DCReplace();
    });
}


MStatus DCReplace::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    const DCurveData* target = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = getValidInputDC(aTargetDC, data, target);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle handle = data.inputValue(aSource, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string sourceName = handle.asString().asChar();
    handle = data.inputValue(aRestMatrix, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string restName = handle.asString().asChar();

    dommy::curve::CurveReplacementPlan plan;
    const DMatrixArray* restMatrices = nullptr;
    if (input->numberOfCurves() && target->numberOfCurves()) {
        const auto pathAttribute = input->getValidTypedAttributedata<std::string>(sourceName);
        const auto restAttribute = input->getValidTypedAttributedata<MMatrix>(restName);
        if (!pathAttribute || !restAttribute) return MS::kInvalidParameter;
        const DStringList* paths = &pathAttribute->value();
        restMatrices = &restAttribute->value();
        if (paths->size() != input->numberOfCurves() ||
            restMatrices->size() != paths->size()) return MS::kInvalidParameter;
        std::map<std::string, DPIndexList> resolved;
        const auto resolve = [&](std::size_t source, const DPIndexList*& matches,
                                  std::size_t& work) -> bool {
            const std::string& path = (*paths)[source];
            if (!dommy::generation::checkedSum(work, path.size(),
                    dommy::generation::kMaxWorkItemCount, work)) return false;
            auto found = resolved.find(path);
            if (found == resolved.end()) {
                DPIndexList indices;
                if (path.compare(0U, 6U, "index:") == 0) {
                    std::size_t index = 0U;
                    if (!dommy::component::detail::parseIndexValue(path.substr(6U), index))
                        return false;
                    // Preserve out-of-range references as an empty contribution.
                    if (index < target->numberOfCurves()) indices.push_back(index);
                } else if (target->isExistGrp(path)) {
                    if (!dommy::generation::checkedSum(work, target->numberOfCurves(),
                            dommy::generation::kMaxWorkItemCount, work)) return false;
                    const auto group = target->getGrpPtr(path);
                    if (!group || group->getGroupIds().size() != target->numberOfCurves())
                        return false;
                    group->getActiveIndexList(indices);
                }
                found = resolved.emplace(path, std::move(indices)).first;
            }
            matches = &found->second;
            return true;
        };
        if (!dommy::curve::buildCurveReplacementPlan(input->numberOfCurves(),
                target->numberOfCurves(), resolve,
                [&](std::size_t index) { return target->getCurve(index).numberOfPoints(); },
                plan)) return MS::kInvalidParameter;
    }

    // Selected target CVs/knots, typed defaults/rows and group masks repeat for
    // every replacement. Admit their complete batch before any clone or factory.
    DOMMY_RETURN_IF_FAILURE(preflightCurveReplacements(*input, *target, restMatrices, plan));

    MFnPluginData pluginData;
    DCurveData* outDCrvs = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outDCrvs);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outDCrvs) return MS::kFailure;
    DCurveOutputBatch generatedCurves;
    for (const dommy::curve::CurveReplacement& replacement : plan.curves) {
        if (!restMatrices) return MS::kFailure;
        const MMatrix matrix = input->getCurve(replacement.source).getTransformMatrix();
        const MMatrix& rest = (*restMatrices)[replacement.source];
        if (!dommy::numeric::isFiniteAffineTransform(matrix) ||
            !dommy::numeric::isFiniteAffineTransform(rest)) return MS::kInvalidParameter;
        DCItem curve(new DCurveInternalData(target->getCurve(replacement.target)));
        DPointList points = curve->getPositionConst();
        // Preserve the archive's translation-only rest adjustment. Do not
        // silently reinterpret restmat as a complete inverse bind transform.
        for (auto& point : points) {
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                if (!dommy::numeric::checkedNumericCast(
                        double(point[axis]) - rest[3][axis], point[axis]))
                    return MS::kInvalidParameter;
            }
        }
        if (!curve->replacePositions(points)) return MS::kFailure;
        curve->setTransformMatrix(matrix);
        if (!generatedCurves.append(curve)) return MS::kFailure;
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
DOMMY_COMPUTE_CATCH("DCReplace")

            
            
            
            



MStatus DCReplace::initialize() {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aTargetDC = tAttr.create("TargetDC", "tdc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDC));

    const MObject aSourceDefault = fnStringData.create("src_path", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSource = tAttr.create("Source", "srcnm", MFnData::kString, aSourceDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSource));

    const MObject aRestMatrixDefault = fnStringData.create("restmat", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRestMatrix = tAttr.create("RestMatrix", "rstmat", MFnData::kString, aRestMatrixDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestMatrix));

    // create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSource, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRestMatrix, outDC));

    return MS::kSuccess;
}
