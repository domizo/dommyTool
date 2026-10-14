//
//  DCReplicator.cpp
//  dommyTool
//
//  Created by yuzo on 3/6/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCReplicator.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"

#include "DNoiseOp.h"

#include <cmath>

const MTypeId DCReplicator::typeId(ID_DC_REPLICATOR);

MObject DCReplicator::inDC;

MObject DCReplicator::aCount;
MObject DCReplicator::aRadius;
MObject DCReplicator::aGenerateType;
MObject DCReplicator::outDC;

void* DCReplicator::creator() {
    return ::dommy::maya::createObject("DCReplicator", [&]() {
        return new DCReplicator();
    });
}

MStatus DCReplicator::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;
    MStatus stat;

#pragma mark attribute

    const MDataHandle rawRepCountHandle = data.inputValue(aCount, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int rawRepCount = rawRepCountHandle.asInt();

    const MDataHandle radiusHandle = data.inputValue(aRadius, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float radius = radiusHandle.asFloat();
    const MDataHandle genTypeHandle = data.inputValue(aGenerateType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short genType = genTypeHandle.asShort();

    std::size_t repCount = 0U;
    if (!dommy::generation::checkedCount(rawRepCount, dommy::generation::kMaxGeneratedCurveCount,
                                         repCount) ||
        !std::isfinite(radius) || radius < 0.0f || (genType != 0 && genType != 1)) {
        EchoWarning(name().asChar(), "Invalid replication parameters.");
        return MS::kInvalidParameter;
    }

#pragma mark get input dcurve
    const DCurveData* inputDCData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
    if (!inputDCData)
        return MS::kFailure;

    DCurveOutputBatch generatedCurves;

    const std::size_t curveCount = inputDCData->numberOfCurves();
    const std::size_t totalCvs = inputDCData->getTotalCvs();
    std::size_t outputCurveCount = 0U;
    std::size_t outputCvCount = 0U;
    std::size_t workItemCount = 0U;
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount ||
        totalCvs > dommy::generation::kMaxGeneratedPointCount ||
        !dommy::generation::checkedProduct(
            curveCount, repCount, dommy::generation::kMaxGeneratedCurveCount, outputCurveCount) ||
        !dommy::generation::checkedProduct(
            totalCvs, repCount, dommy::generation::kMaxGeneratedPointCount, outputCvCount) ||
        !dommy::generation::checkedProduct(outputCvCount, 4U, dommy::generation::kMaxWorkItemCount,
                                           workItemCount)) {
        EchoWarning(name().asChar(), "Requested replication exceeds the safety limit.");
        return MS::kInvalidParameter;
    }

    MObject outputDataObject;
    DCurveData* outDCrvs = nullptr;
    stat =
        dommy::maya::createPluginDataObject<DCurveData>(DCurveData::id, outputDataObject, outDCrvs);
    if (!stat)
        return stat;
    if (!outDCrvs)
        return MS::kFailure;

    if (curveCount && repCount) {

        // Preserve the historical random streams for each replication mode.
        const int seed = 1;

        switch (genType) {
        case 0: {
            // curve base fast

            // One translation per copy.
            DScalarDataList rndXValues(repCount, 0.0);
            DScalarDataList rndYValues(repCount, 0.0);
            DScalarDataList rndZValues(repCount, 0.0);

            size_t curveOffset = 0U;
            for (auto cIt = inputDCData->beginCurves(), eIt = inputDCData->endCurves(); cIt != eIt;
                 ++cIt) {

                if (radius != 0.0f) {
                    const int curveSeed =
                        dommy::random::requireOffsetNonnegativeSeed(seed, curveOffset);
                    const int curveSeedY =
                        dommy::random::requireOffsetNonnegativeSeed(curveSeed, 1U);
                    const int curveSeedZ =
                        dommy::random::requireOffsetNonnegativeSeed(curveSeed, 2U);
                    DGenerateRandomValue(curveSeed, -radius, radius, repCount, rndXValues);
                    DGenerateRandomValue(curveSeedY, -radius, radius, repCount, rndYValues);
                    DGenerateRandomValue(curveSeedZ, -radius, radius, repCount, rndZValues);
                }

                const DPointList* ptsPtr = (*cIt)->getPositionPtrConst();
                size_t ptCount = ptsPtr->size();

                for (size_t d = 0U; d < repCount; ++d) {

                    DVec3Data repVec((rndXValues)[d], (rndYValues)[d], (rndZValues)[d]);

                    DPointList cvs;
                    cvs.reserve(ptCount);

                    for (const auto& pIt : *ptsPtr) {
                        cvs.push_back(pIt + repVec);
                    }

                    if (!generatedCurves.append(DCItem(new DCurveInternalData(
                            cvs, (*cIt)->getKnots(), (*cIt)->getDegree(), (*cIt)->isCurveOpen()))))
                        return MS::kFailure;
                }

                curveOffset++;
            }

        } break;
        case 1: {
            // point base slow

            size_t curveOffset = 0U;
            DScalarDataList rndXValues(outputCvCount, 0.0);
            DScalarDataList rndYValues(outputCvCount, 0.0);
            DScalarDataList rndZValues(outputCvCount, 0.0);
            if (radius != 0.0f) {
                const int curveSeed =
                    dommy::random::requireOffsetNonnegativeSeed(seed, curveOffset);
                const int curveSeedY = dommy::random::requireOffsetNonnegativeSeed(curveSeed, 1U);
                const int curveSeedZ = dommy::random::requireOffsetNonnegativeSeed(curveSeed, 2U);
                DGenerateRandomValue(curveSeed, -radius, radius, outputCvCount, rndXValues);
                DGenerateRandomValue(curveSeedY, -radius, radius, outputCvCount, rndYValues);
                DGenerateRandomValue(curveSeedZ, -radius, radius, outputCvCount, rndZValues);
            }
            size_t pIndex = 0;
            for (auto cIt = inputDCData->beginCurves(), eIt = inputDCData->endCurves(); cIt != eIt;
                 ++cIt) {

                const DPointList* ptsPtr = (*cIt)->getPositionPtrConst();
                size_t ptCount = ptsPtr->size();

                for (size_t d = 0; d < repCount; ++d) {

                    DPointList cvs;
                    cvs.reserve(ptCount);

                    for (const auto& pIt : *ptsPtr) {
                        cvs.push_back(pIt + DVec3Data((rndXValues)[pIndex], (rndYValues)[pIndex],
                                                      (rndZValues)[pIndex]));
                        pIndex++;
                    }

                    if (!generatedCurves.append(DCItem(new DCurveInternalData(
                            cvs, (*cIt)->getKnots(), (*cIt)->getDegree(), (*cIt)->isCurveOpen()))))
                        return MS::kFailure;
                }

                curveOffset++;
            }

        } break;
        default:
            break;
        }

        if (!generatedCurves.publishTo(*outDCrvs))
            return MS::kFailure;
        outDCrvs->updateBBox();
    }

#pragma mark finish output data

    MDataHandle output = data.outputValue(outDC, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outDCrvs));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCReplicator")

MStatus DCReplicator::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aCount = nAttr.create("Count", "cnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedCurveCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCount));

    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    MFnEnumAttribute genEnumFn;
    aGenerateType = genEnumFn.create("GenerateType", "genty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(genEnumFn.addField("Curve", 0));
    DOMMY_RETURN_IF_FAILURE(genEnumFn.addField("Point", 1));
    DOMMY_RETURN_IF_FAILURE(genEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(genEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGenerateType));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCount, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGenerateType, outDC));

    return MS::kSuccess;
}
