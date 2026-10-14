//
//  DPointCircleEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointCircleEmit.h"
#include "common/utility/DStatusOperation.h"


#include "dpoint_ids.h"
#include "DPData.h"

#include "dommyMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>
#include <maya/MVector.h>

#include <random>
#include <cmath>


const MTypeId DPointCircleEmit::typeId( ID_DP_CIRCLE_EMIT );

MObject DPointCircleEmit::emitType;

MObject DPointCircleEmit::radius;
MObject DPointCircleEmit::sampling;
MObject DPointCircleEmit::startAngle;
MObject DPointCircleEmit::endAngle;
MObject DPointCircleEmit::offsetAngle;

MObject DPointCircleEmit::aSeed;
//MObject DPointCircleEmit::count;

MObject DPointCircleEmit::outDP;



void *DPointCircleEmit::creator() {
    return ::dommy::maya::createObject("DPointCircleEmit", [&]() {
        return new DPointCircleEmit();
    });
}

MStatus DPointCircleEmit::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {
        // アトリビュート値の取得
        const MDataHandle emitTypeValHandle = data.inputValue(emitType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const D_CIRCLE_EMIT_TYPE emitTypeVal = emitTypeValHandle.asInt();
        const MDataHandle rawSamplingHandle = data.inputValue(sampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSampling = rawSamplingHandle.asInt();
        const MDataHandle radiusValHandle = data.inputValue(radius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float radiusVal = radiusValHandle.asFloat();
        const MDataHandle startAngleValHandle = data.inputValue(startAngle, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float startAngleVal = startAngleValHandle.asFloat();
        const MDataHandle endAngleValHandle = data.inputValue(endAngle, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float endAngleVal = endAngleValHandle.asFloat();
        const MDataHandle offsetAngleValHandle = data.inputValue(offsetAngle, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float offsetAngleVal = offsetAngleValHandle.asFloat();

        std::size_t samplingVal = 0;
        if (!dommy::generation::checkedCount(
                rawSampling, dommy::generation::kMaxGeneratedPointCount, samplingVal) ||
            !std::isfinite(radiusVal) || radiusVal < 0.0f || !std::isfinite(startAngleVal) ||
            !std::isfinite(endAngleVal) || !std::isfinite(offsetAngleVal) ||
            emitTypeVal < D_CIRCLE_RADIAL || emitTypeVal >= D_CIRCLE_EMIT_TYPE_COUNT) {
            MGlobal::displayError("DPointCircleEmit: invalid or excessive input value.");
            return MS::kFailure;
        }

        // emit Dpoint array
        DPointList newPtArray;

        switch (emitTypeVal) {
        case D_CIRCLE_RADIAL: {
            float diffDegree = endAngleVal - startAngleVal;

            if (!std::isfinite(diffDegree) || diffDegree == 0.0f) {
                MGlobal::displayError("DPointCircleEmit: angle range must be finite and non-zero.");
                return MS::kFailure;
            }

            if (samplingVal) {
                float stepDegree = 0.0f;
                if (samplingVal > 1) {
                    stepDegree = diffDegree / static_cast<float>(samplingVal);
                }

                newPtArray.resize(samplingVal);
                for (size_t i = 0; i < samplingVal; ++i) {

                    MVector baseVec(0.0, 0.0, radiusVal);

                    const float angle = -(startAngleVal + stepDegree * i + offsetAngleVal);
                    if (!std::isfinite(angle))
                        return MS::kInvalidParameter;
                    baseVec = baseVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(angle));

                    if (!dommy::maya::isFloatRepresentableVector(baseVec))
                        return MS::kInvalidParameter;
                    newPtArray[i][0] = baseVec.x;
                    newPtArray[i][1] = baseVec.y;
                    newPtArray[i][2] = baseVec.z;
                }
            }

        } break;
        case D_CIRCLE_FILL: {
            const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int seed = seedHandle.asInt();
            if (seed < 0) {
                MGlobal::displayError("DPointCircleEmit: seed must not be negative.");
                return MS::kFailure;
            }

            std::mt19937 eng(static_cast<unsigned int>(seed));

            std::uniform_real_distribution<float> distribx(-1.0f, 1.0f);
            std::uniform_real_distribution<float> distriby(-1.0f, 1.0f);

            newPtArray.resize(samplingVal);
            size_t index = 0;
            std::size_t attempts = 0;
            while (index < samplingVal) {
                if (++attempts > dommy::generation::kMaxWorkItemCount) {
                    MGlobal::displayError(
                        "DPointCircleEmit: random sampling exceeded the work limit.");
                    return MS::kFailure;
                }

                float x = distribx(eng);
                float y = distriby(eng);

                if (withinCircle(x, y)) {
                    newPtArray[index][0] = x * radiusVal;
                    newPtArray[index][1] = 0.0;
                    newPtArray[index][2] = y * radiusVal;
                    index++;
                }
            }

        } break;
        default:
            break;
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
        MDataHandle outputData = data.outputValue(outDP, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        DOMMY_RETURN_IF_FAILURE(
            dommy::maya::publishPluginDataObject(outputData, outputObject, *dpData));

        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPointCircleEmit")

MStatus DPointCircleEmit::initialize() try {
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MStatus stat;

    MFnEnumAttribute enumFn;
    emitType = enumFn.create("emitType", "emt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Radial", D_CIRCLE_RADIAL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Fill", D_CIRCLE_FILL));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(emitType));

    radius = nAttr.create("radius", "rad", MFnNumericData::kFloat, 5.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(radius));

    sampling = nAttr.create("sampling", "sa", MFnNumericData::kInt, 10, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxGeneratedPointCount)));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    startAngle = nAttr.create("startAngle", "san", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(startAngle));

    endAngle = nAttr.create("endAngle", "ean", MFnNumericData::kFloat, 360.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(endAngle));

    offsetAngle = nAttr.create("offsetAngle", "oan", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetAngle));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(emitType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(radius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(startAngle, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(endAngle, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetAngle, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPointCircleEmit", "initialize",
                                                  std::current_exception());
}
