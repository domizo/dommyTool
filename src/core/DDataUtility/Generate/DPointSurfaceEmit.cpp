//
//  DPointSurfaceEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointSurfaceEmit.h"
#include "common/utility/DStatusOperation.h"



#include "DMayaDebug.h"

#include "dpoint_ids.h"
#include "DPData.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>

#include <cmath>


const MTypeId DPointSurfaceEmit::typeId( ID_DP_SURFACE_EMIT );


MObject DPointSurfaceEmit::inSurfaceObject;

MObject DPointSurfaceEmit::samplingType;

MObject DPointSurfaceEmit::samplingU;
MObject DPointSurfaceEmit::samplingV;

MObject DPointSurfaceEmit::uLength;
MObject DPointSurfaceEmit::vLength;
MObject DPointSurfaceEmit::uOffset;
MObject DPointSurfaceEmit::vOffset;

MObject DPointSurfaceEmit::outDP;


void *DPointSurfaceEmit::creator() {
    return ::dommy::maya::createObject("DPointSurfaceEmit", [&]() {
        return new DPointSurfaceEmit();
    });
}

MStatus DPointSurfaceEmit::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outDP) {
        // アトリビュート値の取得
        const MDataHandle samplTypeValHandle = block.inputValue(samplingType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const D_SURFACE_SAMPLING_TYPE samplTypeVal = samplTypeValHandle.asInt();
        const MDataHandle rawSamplingUHandle = block.inputValue(samplingU, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSamplingU = rawSamplingUHandle.asInt();
        const MDataHandle rawSamplingVHandle = block.inputValue(samplingV, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSamplingV = rawSamplingVHandle.asInt();
        const MDataHandle uLengthValHandle = block.inputValue(uLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float uLengthVal = uLengthValHandle.asFloat();
        const MDataHandle vLengthValHandle = block.inputValue(vLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float vLengthVal = vLengthValHandle.asFloat();
        const MDataHandle uOffsetValHandle = block.inputValue(uOffset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float uOffsetVal = uOffsetValHandle.asFloat();
        const MDataHandle vOffsetValHandle = block.inputValue(vOffset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float vOffsetVal = vOffsetValHandle.asFloat();

        std::size_t samplingUVal = 0;
        std::size_t samplingVVal = 0;
        if (!dommy::generation::checkedCount(
                rawSamplingU, dommy::generation::kMaxGeneratedPointCount, samplingUVal) ||
            !dommy::generation::checkedCount(
                rawSamplingV, dommy::generation::kMaxGeneratedPointCount, samplingVVal) ||
            !std::isfinite(uLengthVal) || !std::isfinite(vLengthVal) ||
            !std::isfinite(uOffsetVal) || !std::isfinite(vOffsetVal) ||
            samplTypeVal < D_SURFACE_SAMPLING_ALL ||
            samplTypeVal >= D_SURFACE_SAMPLING_TYPE_COUNT) {
            MGlobal::displayError("DPointSurfaceEmit: invalid or excessive input value.");
            return MS::kFailure;
        }

        // インプットオブジェクトの読み込み
        MDataHandle surfHnd = block.inputValue(inSurfaceObject, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);

        if (surfHnd.type() != MFnData::kNurbsSurface) {
            EchoWarning(name().asChar(), "Input object is not nurbs surface data!");
            return MS::kFailure;
        }

        MObject inputSurfObject = surfHnd.data();
        if (inputSurfObject.isNull()) {
            EchoWarning(name().asChar(), "Input object is Null!");
            return MS::kFailure;
        }

        // インプットオブジェクトのセット
        MFnNurbsSurface surfFn;
        DOMMY_RETURN_IF_FAILURE(surfFn.setObject(inputSurfObject));

        // emit Dpoint array
        DPointList newPtArray;

        switch (samplTypeVal) {
        case D_SURFACE_SAMPLING_ALL: {
            double startU, endU, startV, endV;

            DOMMY_RETURN_IF_FAILURE(surfFn.getKnotDomain(startU, endU, startV, endV));
            if (!std::isfinite(startU) || !std::isfinite(endU) || !std::isfinite(startV) ||
                !std::isfinite(endV)) {
                MGlobal::displayError("DPointSurfaceEmit: surface domain is not finite.");
                return MS::kFailure;
            }

            std::size_t totalSamplingU = 0;
            std::size_t totalSamplingV = 0;
            std::size_t totalPointCount = 0;
            if (!dommy::generation::checkedSum(
                    samplingUVal, 2U, dommy::generation::kMaxGeneratedPointCount, totalSamplingU) ||
                !dommy::generation::checkedSum(
                    samplingVVal, 2U, dommy::generation::kMaxGeneratedPointCount, totalSamplingV) ||
                !dommy::generation::checkedProduct(totalSamplingU, totalSamplingV,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   totalPointCount)) {
                MGlobal::displayError(
                    "DPointSurfaceEmit: requested point count exceeds the safety limit.");
                return MS::kFailure;
            }
            const double stepLengthU = (endU - startU) / static_cast<double>(totalSamplingU - 1U);
            const double stepLengthV = (endV - startV) / static_cast<double>(totalSamplingV - 1U);
            if (!std::isfinite(stepLengthU) || !std::isfinite(stepLengthV))
                return MS::kInvalidParameter;

            newPtArray.resize(totalPointCount);
            size_t index = 0;
            for (size_t i = 0; i < totalSamplingU; i++) {
                for (size_t j = 0; j < totalSamplingV; j++) {
                    MPoint pt;
                    const double u = startU + stepLengthU * static_cast<double>(i);
                    const double v = startV + stepLengthV * static_cast<double>(j);
                    if (!std::isfinite(u) || !std::isfinite(v))
                        return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(surfFn.getPointAtParam(u, v, pt, MSpace::kWorld));
                    if (!dommy::maya::isFloatRepresentablePoint(pt)) {
                        return MS::kFailure;
                    }
                    newPtArray[index][0] = pt[0];
                    newPtArray[index][1] = pt[1];
                    newPtArray[index][2] = pt[2];
                    index++;
                }
            }
        } break;
        case D_SURFACE_SAMPLING_RANGE: {
            std::size_t totalSamplingU = 0;
            std::size_t totalSamplingV = 0;
            std::size_t totalPointCount = 0;
            if (!dommy::generation::checkedSum(
                    samplingUVal, 2U, dommy::generation::kMaxGeneratedPointCount, totalSamplingU) ||
                !dommy::generation::checkedSum(
                    samplingVVal, 2U, dommy::generation::kMaxGeneratedPointCount, totalSamplingV) ||
                !dommy::generation::checkedProduct(totalSamplingU, totalSamplingV,
                                                   dommy::generation::kMaxGeneratedPointCount,
                                                   totalPointCount)) {
                MGlobal::displayError(
                    "DPointSurfaceEmit: requested point count exceeds the safety limit.");
                return MS::kFailure;
            }

            const double stepLengthU =
                static_cast<double>(uLengthVal) / static_cast<double>(totalSamplingU - 1U);
            const double stepLengthV =
                static_cast<double>(vLengthVal) / static_cast<double>(totalSamplingV - 1U);

            newPtArray.resize(totalPointCount);
            size_t index = 0;
            for (size_t i = 0; i < totalSamplingU; i++) {
                for (size_t j = 0; j < totalSamplingV; j++) {
                    MPoint pt;
                    const double u = stepLengthU * static_cast<double>(i) + uOffsetVal;
                    const double v = stepLengthV * static_cast<double>(j) + vOffsetVal;
                    if (!std::isfinite(u) || !std::isfinite(v))
                        return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(surfFn.getPointAtParam(u, v, pt, MSpace::kWorld));
                    if (!dommy::maya::isFloatRepresentablePoint(pt)) {
                        return MS::kFailure;
                    }
                    newPtArray[index][0] = pt[0];
                    newPtArray[index][1] = pt[1];
                    newPtArray[index][2] = pt[2];
                    index++;
                }
            }
        } break;
        case D_SURFACE_SAMPLING_CV: {
            // Bound the allocation made by getCVs, not just the later copy.
            const int rawCountU = surfFn.numCVsInU(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int rawCountV = surfFn.numCVsInV(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            std::size_t countU = 0U, countV = 0U, expectedCount = 0U;
            if (!dommy::generation::checkedCount(
                    rawCountU, dommy::generation::kMaxGeneratedPointCount, countU) ||
                !dommy::generation::checkedCount(
                    rawCountV, dommy::generation::kMaxGeneratedPointCount, countV) ||
                !dommy::generation::checkedProduct(
                    countU, countV, dommy::generation::kMaxGeneratedPointCount, expectedCount))
                return MS::kInvalidParameter;
            MPointArray pts;
            DOMMY_RETURN_IF_FAILURE(surfFn.getCVs(pts, MSpace::kWorld));

            size_t count = pts.length();
            if (count != expectedCount) {
                return MS::kFailure;
            }
            if (count) {
                newPtArray.resize(count);

                for (size_t i = 0; i < count; ++i) {
                    if (!dommy::maya::isFloatRepresentablePoint(pts[i])) {
                        return MS::kFailure;
                    }
                    newPtArray[i][0] = pts[i][0];
                    newPtArray[i][1] = pts[i][1];
                    newPtArray[i][2] = pts[i][2];
                }
            }

        } break;
        default:
            return MS::kFailure;
        }

        MObject outputObject;
        DPData* dpData = nullptr;
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, dpData));
        if (!dpData)
            return MS::kFailure;
        dpData->setPosition(newPtArray);
        dpData->updateBBox();
        if (dpData->numberOfPoints() != newPtArray.size())
            return MS::kFailure;
        MDataHandle outputData = block.outputValue(outDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::publishPluginDataObject(outputData, outputObject, *dpData));

        return block.setClean(plug);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPointSurfaceEmit")

MStatus DPointSurfaceEmit::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inSurfaceObject =
        tAttr.create("inSurfaceObject", "iso", MFnData::kNurbsSurface, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inSurfaceObject));

    MFnEnumAttribute enumFn;
    samplingType = enumFn.create("samplingType", "sat", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("All", D_SURFACE_SAMPLING_ALL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Range", D_SURFACE_SAMPLING_RANGE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("CVs", D_SURFACE_SAMPLING_CV));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingType));

    samplingU = nAttr.create("samplingU", "sau", MFnNumericData::kInt, 10, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingU));

    samplingV = nAttr.create("samplingV", "sav", MFnNumericData::kInt, 10, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingV));

    uLength = nAttr.create("uLength", "ul", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(uLength));

    vLength = nAttr.create("vLength", "vl", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(vLength));

    uOffset = nAttr.create("uOffset", "uof", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(uOffset));

    vOffset = nAttr.create("vOffset", "vof", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(vOffset));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inSurfaceObject, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingU, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingV, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(uLength, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(vLength, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(uOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(vOffset, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPointSurfaceEmit", "initialize",
                                                  std::current_exception());
}
