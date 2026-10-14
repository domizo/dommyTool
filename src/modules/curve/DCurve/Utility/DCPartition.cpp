//
//  DCPartition.cpp
//  dommyTool
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCPartition.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointTransform.h"
#include "geometry/DPolygonTriangulator.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DMayaHeaders.h"

#include <cstddef>
#include <vector>

namespace {

enum PartitionMode {
    kLegacyYMonotone = 0,
    kConvexPartition = 1,
    kLegacyGreeneConvex = 2,
    kRetiredOptimalConvex = 3
};

} // namespace

const MTypeId DCPartition::typeId(ID_DC_PARTITION);

MObject DCPartition::inDC;
MObject DCPartition::aMode;
MObject DCPartition::outDC;

void* DCPartition::creator()
{
    return ::dommy::maya::createObject("DCPartition", [&]() {
        return new DCPartition();
    });
}

MStatus DCPartition::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));

    const MDataHandle modeHandle = data.inputValue(aMode, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short mode = modeHandle.asShort();
    if (mode < kLegacyYMonotone || mode > kRetiredOptimalConvex) {
        return MS::kInvalidParameter;
    }
    if (mode == kRetiredOptimalConvex) {
        MGlobal::displayError(
            "DCPartition Optimal Convex was retired with its legacy backend. "
            "Use Convex Partition for deterministic dependency-free output.");
        return MS::kInvalidParameter;
    }

    MObject outputObject;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputObject, outputCurves);
    if (!status) return status;
    if (!outputCurves) return MS::kFailure;

    DPIndexList parentIndices;
    DCurveOutputBatch generatedCurves;
    std::size_t curveIndex = 0U;
    std::size_t totalPointCount = 0U;
    dommy::geometry::PolygonWorkBudget polygonBudget;
    for (auto curve = input->beginPrims();
         curve != input->endPrims(); ++curve, ++curveIndex) {
        if (!polygonBudget.consume(1U)) return MS::kFailure;
        const DPointList* const sourcePoints =
            (*curve)->getPositionPtrConst();
        if (!sourcePoints || sourcePoints->size() < 3U ||
            sourcePoints->size() >
                dommy::generation::kMaxGeneratedPointCount) {
            continue;
        }
        const MMatrix primitiveMatrix = (*curve)->getTransformMatrix();
        if (!dommy::numeric::isFiniteAffineTransform(primitiveMatrix))
            return MS::kInvalidParameter;

        if (!polygonBudget.consume(sourcePoints->size())) return MS::kFailure;
        std::vector<dommy::geometry::Point2> polygon;
        polygon.reserve(sourcePoints->size());
        for (DPointList::const_iterator point = sourcePoints->begin();
             point != sourcePoints->end(); ++point) {
            polygon.push_back(dommy::geometry::Point2(
                static_cast<double>((*point)[0]),
                static_cast<double>((*point)[1])));
        }

        dommy::geometry::PolygonTriangulation triangulation;
        if (!dommy::geometry::triangulateSimplePolygon(
                polygon, triangulation,
                dommy::generation::kMaxGeneratedPointCount,
                dommy::generation::kMaxGeneratedFaceCount,
                polygonBudget)) {
            if (polygonBudget.exceeded()) return MS::kFailure;
            continue;
        }
        std::vector<std::vector<std::size_t> > partitions;
        if (!dommy::geometry::convexPartition(
                triangulation, partitions,
                polygonBudget)) {
            return MS::kFailure;
        }

        for (std::vector<std::vector<std::size_t> >::const_iterator
                 partition = partitions.begin();
             partition != partitions.end(); ++partition) {
            if (partition->size() < 3U ||
                parentIndices.size() >=
                    dommy::generation::kMaxGeneratedCurveCount) {
                return MS::kFailure;
            }
            std::size_t nextPointCount = 0U;
            if (!dommy::generation::checkedSum(
                    totalPointCount, partition->size(),
                    dommy::generation::kMaxGeneratedPointCount,
                    nextPointCount)) {
                return MS::kFailure;
            }

            if (!polygonBudget.consume(partition->size())) return MS::kFailure;
            DPointList outputPoints;
            outputPoints.reserve(partition->size());
            for (std::vector<std::size_t>::const_iterator vertex =
                     partition->begin();
                 vertex != partition->end(); ++vertex) {
                if (*vertex >= triangulation.points.size() ||
                    *vertex >= triangulation.sourceIndices.size()) {
                    return MS::kFailure;
                }
                const std::size_t sourceIndex =
                    triangulation.sourceIndices[*vertex];
                if (sourceIndex >= sourcePoints->size()) {
                    return MS::kFailure;
                }
                outputPoints.push_back(DPoint(
                    static_cast<float>(
                        triangulation.points[*vertex].x),
                    static_cast<float>(
                        triangulation.points[*vertex].y),
                    (*sourcePoints)[sourceIndex][2]));
            }

            DCItem outputCurve(
                new DCurveInternalData(outputPoints, 1));
            outputCurve->setTransformMatrix(primitiveMatrix);
            if (!outputCurve->hasValidCurveTopology() ||
                !generatedCurves.append(outputCurve)) {
                return MS::kFailure;
            }
            parentIndices.push_back(curveIndex);
            totalPointCount = nextPointCount;
        }
    }

    if (!generatedCurves.publishTo(*outputCurves) ||
        outputCurves->numberOfCurves() != parentIndices.size()) return MS::kFailure;
    {
        for (auto group = input->beginGrp();
             group != input->endGrp(); ++group) {
            if (!group->second) return MS::kInvalidParameter;
            const DPIdList& sourceIds =
                group->second->getGroupIds();
            if (sourceIds.size() < input->numberOfCurves()) {
                return MS::kInvalidParameter;
            }
            DPIdList outputIds(parentIndices.size(), false);
            for (std::size_t index = 0U;
                 index < parentIndices.size(); ++index) {
                if (parentIndices[index] >= sourceIds.size()) {
                    return MS::kFailure;
                }
                outputIds[index] = sourceIds[parentIndices[index]];
            }
            outputCurves->addGrp(group->first, outputIds);
        }

        DStringList sourcePaths;
        sourcePaths.reserve(parentIndices.size());
        for (DPIndexList::const_iterator parent = parentIndices.begin();
             parent != parentIndices.end(); ++parent) {
            MString path("index:");
            path += static_cast<unsigned int>(*parent);
            sourcePaths.push_back(path.asChar());
        }
        outputCurves->addAttr(
            "src_path", DPStringAttr(sourcePaths));
    }
    outputCurves->updateBBox();

    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(
            output, outputObject, *outputCurves));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCPartition")

MStatus DCPartition::initialize()
{
    MStatus status;
    MFnTypedAttribute typedAttribute;

    inDC = typedAttribute.create(
        "inDC", "idc", DCurveData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute modeAttribute;
    aMode = modeAttribute.create(
        "Mode", "md", kConvexPartition, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField(
        "Convex Partition (legacy Y-Monotone id)",
        kLegacyYMonotone));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField(
        "Convex Partition", kConvexPartition));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField(
        "Convex Partition (legacy Greene id)",
        kLegacyGreeneConvex));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.addField(
        "Retired Optimal Convex", kRetiredOptimalConvex));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(modeAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMode));

    outDC = typedAttribute.create(
        "outDC", "odc", DCurveData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMode, outDC));
    return MS::kSuccess;
}
