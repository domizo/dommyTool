//
//  DCConvex2D.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCConvex2D.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DAttributeArrayConversion.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DConvexHull2D.h"

const MTypeId DCConvex2D::typeId(ID_DC_CONVEX_2D);

MObject DCConvex2D::inDC;

MObject DCConvex2D::aPieceGroupName;
MObject DCConvex2D::outDC;

void* DCConvex2D::creator() {
    return ::dommy::maya::createObject("DCConvex2D", [&]() {
        return new DCConvex2D();
    });
}

MStatus DCConvex2D::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    MStatus status;
    const MDataHandle groupHandle = data.inputValue(aPieceGroupName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string pieceName = groupHandle.asString().asChar();

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves));
    if (!outputCurves)
        return MS::kFailure;
    const std::size_t curveCount = input->numberOfCurves();
    DCurveOutputBatch generatedCurves;
    DStringList sourcePaths;
    if (curveCount) {
        std::vector<bool> clustered(curveCount, false);
        std::size_t mergedWork = 0U;
        std::vector<std::string> matchedGroups;
        getDCurveMatchedGroups(*input, pieceName, matchedGroups);
        for (const auto& name : matchedGroups) {
            const DPGroupData::ConstPtr group = input->getGrpPtr(name);
            if (!group ||
                !dommy::generation::checkedSum(mergedWork, curveCount,
                                               dommy::generation::kMaxWorkItemCount, mergedWork))
                return MS::kInvalidParameter;
            DPIndexList indices;
            group->getActiveIndexList(indices);
            if (indices.empty())
                continue;
            DPointList mergedPoints;
            for (const auto index : indices) {
                if (index >= curveCount)
                    return MS::kInvalidParameter;
                clustered[index] = true;
                const ConstDCItem curve = input->getCurvePtr(index);
                if (!curve)
                    return MS::kInvalidParameter;
                std::size_t count = 0U;
                if (!dommy::generation::checkedSum(mergedPoints.size(), curve->numberOfPoints(),
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   count) ||
                    !dommy::generation::checkedSum(mergedWork, curve->numberOfPoints(),
                                                   dommy::generation::kMaxWorkItemCount,
                                                   mergedWork) ||
                    !dommy::attribute::validArrayConversionBudget(count, 3U * sizeof(DPoint)))
                    return MS::kInvalidParameter;
                const DPointList points = curve->getPointsInWorld();
                mergedPoints.insert(mergedPoints.end(), points.begin(), points.end());
            }
            DPointList hull;
            if (!dommy::geometry::buildConvexHull2D(mergedPoints, hull))
                return MS::kInvalidParameter;
            // Preserve the existing world-XY hull and collapsed-group skip policy.
            if (hull.size() > 1U) {
                if (!generatedCurves.append(DCItem(new DCurveInternalData(hull, 1))))
                    return MS::kFailure;
                sourcePaths.push_back(name);
            }
        }
        std::size_t index = 0U;
        for (auto curve = input->beginPrims(); curve != input->endPrims(); ++curve, ++index) {
            if (clustered[index])
                continue;
            // Ungrouped curves retain local geometry, attributes and primitive matrices.
            if (!generatedCurves.append(DCItem(new DCurveInternalData(**curve))))
                return MS::kFailure;
            MString path("index:");
            path += static_cast<unsigned int>(index);
            sourcePaths.push_back(path.asChar());
        }
    }
    if (!generatedCurves.publishTo(*outputCurves) ||
        outputCurves->numberOfCurves() != sourcePaths.size())
        return MS::kFailure;
    if (curveCount) {
        outputCurves->addAttr("src_path", DPStringAttr(sourcePaths));
        outputCurves->updateBBox();
    }
    MDataHandle output = data.outputValue(outDC, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCConvex2D")

MStatus DCConvex2D::initialize() {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    const MObject aPieceGroupNameDefault = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceGroupName =
        tAttr.create("PieceGroupName", "picgrpnm", MFnData::kString, aPieceGroupNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceGroupName));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPieceGroupName, outDC));
    return MS::kSuccess;
}
