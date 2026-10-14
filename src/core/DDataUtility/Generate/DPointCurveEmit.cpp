//
//  DPointCurveEmit.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPointCurveEmit.h"
#include "common/utility/DMayaDefaultRamp.h"


#include "DPData.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dpoint_ids.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaPluginDataOutput.h"


#include <algorithm>
#include <random>
#include <cmath>
#include <stdexcept>

#include "dommyMath.h"

#include "perlin.h"
#include "module.h"

#include <maya/MCurveAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MRampAttribute.h>

namespace {

bool finitePoint(const DPoint& point)
{
    return std::isfinite(point[0]) && std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}

void appendPointChecked(DPointList& points, const DPoint& point)
{
    if (!finitePoint(point)) {
        throw std::invalid_argument("DPointCurveEmit generated a non-finite point");
    }
    if (points.size() >= dommy::generation::kMaxGeneratedPointCount) {
        throw std::length_error("DPointCurveEmit point limit exceeded");
    }
    points.push_back(point);
}

void requireWorkBudget(std::size_t curveCount,
                       std::size_t pathSampleCount,
                       std::size_t crossSectionSampleCount)
{
    std::size_t pathWork = 0U;
    std::size_t totalWork = 0U;
    if (!dommy::generation::checkedProduct(
            curveCount, pathSampleCount,
            dommy::generation::kMaxWorkItemCount, pathWork) ||
        !dommy::generation::checkedProduct(
            pathWork, crossSectionSampleCount,
            dommy::generation::kMaxWorkItemCount, totalWork) ||
        totalWork > dommy::generation::kMaxGeneratedPointCount) {
        throw std::length_error("DPointCurveEmit work limit exceeded");
    }
}

MVector normalizedOrZero(const MVector& value)
{
    const double length = value.length();
    if (!std::isfinite(length) || length <= 1.0e-12) return MVector::zero;
    return value / length;
}

void requireValidFrame(const MVector& tangent,
                       const MVector& normal,
                       const MVector& binormal)
{
    if (tangent.length() <= 1.0e-12 || normal.length() <= 1.0e-12 ||
        binormal.length() <= 1.0e-12) {
        throw std::runtime_error(
            "DPointCurveEmit could not construct a curve frame");
    }
}

// Project-owned disk-to-curve placement. The caller supplies an already
// validated frame; sampling policy and noise backend remain separate.
template <typename NoiseField>
void appendPyroDiskCandidate(DPointList& points, NoiseField& noise,
                            const MPoint& center, const MVector& firstAxis,
                            const MVector& secondAxis, double parameter,
                            float diskX, float diskY, float radius, float ramp,
                            float noiseScale, const MFloatVector& noiseOffset,
                            float displacementAmplitude, float densityAmplitude)
{
    if (!dommy::math::isInsideUnitCircle(diskX, diskY)) return;

    const double fieldValue = noise.GetValue(
        double(diskX) / noiseScale + noiseOffset.x,
        double(diskY) / noiseScale + noiseOffset.y,
        parameter / noiseScale + noiseOffset.z);
    if (!std::isfinite(fieldValue)) {
        throw std::runtime_error("DPointCurveEmit noise returned a non-finite value");
    }
    if (!(fieldValue * densityAmplitude > 0.0)) return;

    // Keep the public float radius/ramp policy and separate axis additions.
    // In particular, displacement is ramped but is not scaled by radius.
    const float sectionRadius = radius * ramp;
    const MVector radial = normalizedOrZero(firstAxis * diskX + secondAxis * diskY);
    const double displacement = fieldValue * displacementAmplitude;
    MPoint candidate = center;
    for (int axis = 0; axis != 3; ++axis) {
        candidate[axis] += (firstAxis[axis] * diskX) * sectionRadius;
        candidate[axis] += (secondAxis[axis] * diskY) * sectionRadius;
        candidate[axis] += (radial[axis] * displacement) * ramp;
    }
    appendPointChecked(points, DPoint(candidate[0], candidate[1], candidate[2]));
}

} // namespace



enum {
    D_CURVE_SAMPLING_ALL = 0,
    D_CURVE_SAMPLING_RANGE,
    D_CURVE_SAMPLING_STEP,
    D_CURVE_SAMPLING_CV,
	D_CURVE_SAMPLING_LENGTH_FILL,
	D_CURVE_SAMPLING_TUBE,
    D_CURVE_SAMPLING_TUBE_OUTER,
    D_CURVE_SAMPLING_PYRO,
    D_CURVE_SAMPLING_TYPE_COUNT
};typedef int D_CURVE_SAMPLING_TYPE;



const MTypeId DPointCurveEmit::typeId( ID_DP_CURVE_EMIT );


MObject DPointCurveEmit::inCurve;
MObject DPointCurveEmit::inDC;


MObject DPointCurveEmit::samplingType;

MObject DPointCurveEmit::sampling;
MObject DPointCurveEmit::curveLength;
MObject DPointCurveEmit::stepSize;
MObject DPointCurveEmit::offset;
MObject DPointCurveEmit::aFillLength;


MObject DPointCurveEmit::aTangentMode;
MObject DPointCurveEmit::aTangentVector;
MObject DPointCurveEmit::aInTangentCurve;


MObject DPointCurveEmit::aFillSampling;
MObject DPointCurveEmit::aRadius;
MObject DPointCurveEmit::aRadiusRamp;

MObject DPointCurveEmit::aOuterType;
MObject DPointCurveEmit::aOuterCircleSampling;
MObject DPointCurveEmit::aOuterRandomSampling;


MObject DPointCurveEmit::aDisplaceAmplitude;

MObject DPointCurveEmit::aSeed;
MObject DPointCurveEmit::aAmplitube;
MObject DPointCurveEmit::aFrequency;
MObject DPointCurveEmit::aOctave;
MObject DPointCurveEmit::aLacunarity;
MObject DPointCurveEmit::aPersistence;

MObject DPointCurveEmit::aNoiseScale;
MObject DPointCurveEmit::aNoiseOffset;

MObject DPointCurveEmit::outDP;




void DPointCurveEmit::postConstructor() {
    dommy::maya::initializeNodeRamps("DPointCurveEmit", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aRadiusRamp,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}


void *DPointCurveEmit::creator() {
    return ::dommy::maya::createObject("DPointCurveEmit", [&]() {
        return new DPointCurveEmit();
    });
}


MStatus DPointCurveEmit::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle samplingHandle = data.inputValue(sampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::size_t sampleCount = 0U;
    if (!dommy::generation::checkedCount(samplingHandle.asInt(),
                                         dommy::generation::kMaxGeneratedPointCount, sampleCount))
        return MS::kInvalidParameter;

    DPointList points;
    if (sampleCount != 0U) {
        MPlug curvePlug(thisMObject(), inCurve);
        const bool hasCurve = curvePlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        MObjectArray curves;
        if (hasCurve) {
            MDataHandle handle = data.inputValue(inCurve, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (handle.type() != MFnData::kNurbsCurve || handle.data().isNull())
                return MS::kInvalidParameter;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                1U,
                [&](unsigned int, MObject& object) -> MStatus {
                    object = handle.data();
                    return MS::kSuccess;
                },
                curves));
        } else {
            MPlug curveDataPlug(thisMObject(), inDC);
            const bool hasCurveData = curveDataPlug.isConnected(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (hasCurveData) {
                const DCurveData* input = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
                if (!input || input->numberOfCurves() > dommy::generation::kMaxGeneratedFaceCount)
                    return MS::kInvalidParameter;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
                    input->numberOfCurves(),
                    [&](unsigned int index, MObject& object) -> MStatus {
                        ConstDCItem curve = input->getCurvePtr(index);
                        const MStatus result = generateMCurveFromDcurve(curve, object);
                        if (!result)
                            return result;
                        return object.isNull() ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
                    },
                    curves));
            }
        }
        if (curves.length() != 0U)
            DOMMY_RETURN_IF_FAILURE(processGeneratePoint(data, curves, points));
    }

    MObject outputObject;
    DPData* outputPoints = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::createPluginDataObject(DPData::id, outputObject, outputPoints));
    if (!outputPoints)
        return MS::kFailure;
    outputPoints->setPosition(points);
    outputPoints->updateBBox();
    if (outputPoints->numberOfPoints() != points.size())
        return MS::kFailure;
    MDataHandle output = data.outputValue(outDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputObject, *outputPoints));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPointCurveEmit")



MStatus DPointCurveEmit::processGeneratePoint(MDataBlock& data, const MObjectArray& curveList,
                                              DPointList& pts) {

    MStatus stat;

    // アトリビュート値の取得
    const MDataHandle samplTypeValHandle = data.inputValue(samplingType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const D_CURVE_SAMPLING_TYPE samplTypeVal = samplTypeValHandle.asInt();
    const MDataHandle samplingValHandle = data.inputValue(sampling, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int samplingVal = samplingValHandle.asInt();

    const MDataHandle curveLengthValHandle = data.inputValue(curveLength, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float curveLengthVal = curveLengthValHandle.asFloat();
    const MDataHandle offsetValHandle = data.inputValue(offset, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float offsetVal = offsetValHandle.asFloat();
    const MDataHandle stepSizeValHandle = data.inputValue(stepSize, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float stepSizeVal = stepSizeValHandle.asFloat();
    std::size_t sampleCount = 0U;
    if (samplTypeVal < 0 || samplTypeVal >= D_CURVE_SAMPLING_TYPE_COUNT ||
        !dommy::generation::checkedCount(samplingVal, dommy::generation::kMaxGeneratedPointCount,
                                         sampleCount) ||
        curveList.length() > dommy::generation::kMaxGeneratedFaceCount ||
        !std::isfinite(curveLengthVal) || curveLengthVal < 0.0f || !std::isfinite(offsetVal) ||
        !std::isfinite(stepSizeVal)) {
        throw std::invalid_argument("DPointCurveEmit sampling settings are invalid");
    }

    std::size_t pathSampleCount = sampleCount;
    if (samplTypeVal == D_CURVE_SAMPLING_ALL || samplTypeVal == D_CURVE_SAMPLING_TUBE ||
        samplTypeVal == D_CURVE_SAMPLING_TUBE_OUTER || samplTypeVal == D_CURVE_SAMPLING_PYRO) {
        if (!dommy::generation::checkedSum(
                sampleCount, 1U, dommy::generation::kMaxGeneratedPointCount, pathSampleCount)) {
            throw std::length_error("DPointCurveEmit sample limit exceeded");
        }
    }
    if (samplTypeVal != D_CURVE_SAMPLING_CV && samplTypeVal != D_CURVE_SAMPLING_LENGTH_FILL &&
        samplTypeVal != D_CURVE_SAMPLING_TUBE_OUTER) {
        requireWorkBudget(curveList.length(), pathSampleCount, 1U);
    }
    if (samplTypeVal == D_CURVE_SAMPLING_ALL || samplTypeVal == D_CURVE_SAMPLING_STEP ||
        samplTypeVal == D_CURVE_SAMPLING_RANGE || samplTypeVal == D_CURVE_SAMPLING_CV ||
        samplTypeVal == D_CURVE_SAMPLING_LENGTH_FILL) {

        for (size_t c = 0, N = curveList.length(); c < N; ++c) {

            MFnNurbsCurve curveFn;
            stat = curveFn.setObject(curveList[c]);
            if (!stat)
                throw std::runtime_error("DPointCurveEmit received an invalid curve");

            switch (samplTypeVal) {
            case D_CURVE_SAMPLING_ALL: {
                double totalLength = curveFn.length(kMFnNurbsEpsilon, &stat);
                if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                    throw std::runtime_error("DPointCurveEmit curve length is invalid");
                }

                if (samplingVal == 0) {
                    break;
                } else if (samplingVal == 1) {
                    MPoint pt;
                    const double startParam = curveFn.findParamFromLength(0.0, &stat);
                    if (!stat || !curveFn.getPointAtParam(startParam, pt, MSpace::kWorld)) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                } else if (samplingVal == 2) {
                    MPoint pt;
                    double endpointParam = curveFn.findParamFromLength(0.0, &stat);
                    if (!stat || !curveFn.getPointAtParam(endpointParam, pt, MSpace::kWorld)) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));

                    endpointParam = curveFn.findParamFromLength(totalLength, &stat);
                    if (!stat || !curveFn.getPointAtParam(endpointParam, pt, MSpace::kWorld)) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                } else {
                    float stepLength = totalLength / (float)samplingVal;

                    for (std::size_t i = 0U; i < pathSampleCount; ++i) {
                        double param =
                            curveFn.findParamFromLength(stepLength * static_cast<double>(i), &stat);
                        if (!stat || !std::isfinite(param)) {
                            throw std::runtime_error(
                                "DPointCurveEmit could not convert curve length");
                        }

                        MPoint pt;
                        stat = curveFn.getPointAtParam(param, pt, MSpace::kWorld);
                        if (!stat) {
                            throw std::runtime_error("DPointCurveEmit could not sample a curve");
                        }
                        appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                    }
                }
                break;
            }

            case D_CURVE_SAMPLING_STEP: {

                for (std::size_t i = 0U; i < sampleCount; ++i) {
                    double param = curveFn.findParamFromLength(
                        (stepSizeVal * static_cast<double>(i)) + offsetVal, &stat);
                    if (!stat || !std::isfinite(param)) {
                        throw std::runtime_error("DPointCurveEmit could not convert curve length");
                    }

                    MPoint pt;
                    stat = curveFn.getPointAtParam(param, pt, MSpace::kWorld);
                    if (!stat) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                }

                break;
            }

            case D_CURVE_SAMPLING_RANGE: {
                const double totalLength = curveFn.length(0.0000001, &stat);
                if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                    throw std::runtime_error("DPointCurveEmit curve length is invalid");
                }
                float stepLength = 0.0;

                if (samplingVal > 1) {
                    stepLength = (totalLength * curveLengthVal) / (float)(samplingVal - 1);
                }

                for (std::size_t i = 0U; i < sampleCount; ++i) {
                    double param = curveFn.findParamFromLength(
                        (stepLength * static_cast<double>(i)) + offsetVal, &stat);
                    if (!stat || !std::isfinite(param)) {
                        throw std::runtime_error("DPointCurveEmit could not convert curve length");
                    }

                    MPoint pt;
                    stat = curveFn.getPointAtParam(param, pt, MSpace::kWorld);
                    if (!stat) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                }
                break;
            }

            case D_CURVE_SAMPLING_CV: {
                const int cvCount = curveFn.numCVs(&stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (cvCount < 0 ||
                    static_cast<std::size_t>(cvCount) > dommy::generation::kMaxGeneratedPointCount)
                    return MS::kInvalidParameter;
                MPointArray cvs;
                stat = curveFn.getCVs(cvs, MSpace::kWorld);
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read curve CVs");
                }
                if (cvs.length() != static_cast<unsigned int>(cvCount))
                    return MS::kFailure;

                for (uint cv = 0, ecv = cvs.length(); cv < ecv; ++cv) {
                    appendPointChecked(pts, DPoint(cvs[cv][0], cvs[cv][1], cvs[cv][2]));
                }

                break;
            }
            case D_CURVE_SAMPLING_LENGTH_FILL: {
                const MDataHandle fillLengthHandle = data.inputValue(aFillLength, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const float fillLength = fillLengthHandle.asFloat();
                if (!std::isfinite(fillLength) || fillLength <= 0.0f) {
                    throw std::invalid_argument("DPointCurveEmit fill length is invalid");
                }

                const double totalLength = curveFn.length(kMFnNurbsEpsilon, &stat);

                if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                    throw std::runtime_error("DPointCurveEmit curve length is invalid");
                }
                const double rawSampleCount = std::floor(totalLength / fillLength);
                if (!std::isfinite(rawSampleCount) ||
                    rawSampleCount >
                        static_cast<double>(dommy::generation::kMaxGeneratedPointCount)) {
                    throw std::length_error("DPointCurveEmit fill limit exceeded");
                }
                const std::size_t sampleNum =
                    std::max<std::size_t>(1U, static_cast<std::size_t>(rawSampleCount));
                requireWorkBudget(curveList.length(), sampleNum + 1U, 1U);
                const double step = totalLength / static_cast<double>(sampleNum);

                double start, end;
                stat = curveFn.getKnotDomain(start, end);
                if (!stat || !std::isfinite(start) || !std::isfinite(end)) {
                    throw std::runtime_error("DPointCurveEmit curve domain is invalid");
                }

                for (std::size_t i = 0U; i < sampleNum; ++i) {
                    double param =
                        curveFn.findParamFromLength(step * static_cast<double>(i), &stat);
                    if (!stat || !std::isfinite(param)) {
                        throw std::runtime_error("DPointCurveEmit could not convert curve length");
                    }

                    if (param > end) {
                        break;
                    }
                    param += offsetVal;

                    MPoint pt;
                    stat = curveFn.getPointAtParam(param, pt, MSpace::kWorld);
                    if (!stat) {
                        throw std::runtime_error("DPointCurveEmit could not sample a curve");
                    }
                    appendPointChecked(pts, DPoint(pt[0], pt[1], pt[2]));
                }

                // add last
                MPoint lastpt;
                stat = curveFn.getPointAtParam(end + offsetVal, lastpt, MSpace::kWorld);
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not sample the curve end");
                }
                appendPointChecked(pts, DPoint(lastpt[0], lastpt[1], lastpt[2]));

                break;
            }
            default:
                break;
            }
        }

    } else if (samplTypeVal == D_CURVE_SAMPLING_TUBE) {

        const MDataHandle rawFillSamplingHandle = data.inputValue(aFillSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawFillSampling = rawFillSamplingHandle.asInt();
        std::size_t fillSampling = 0U;
        if (!dommy::generation::checkedCount(
                rawFillSampling, dommy::generation::kMaxGeneratedPointCount, fillSampling)) {
            throw std::invalid_argument("DPointCurveEmit fill sampling is invalid");
        }
        requireWorkBudget(curveList.length(), pathSampleCount, fillSampling);

        const MDataHandle radiusHandle = data.inputValue(aRadius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float radius = radiusHandle.asFloat();
        if (!std::isfinite(radius) || radius < 0.0f) {
            throw std::invalid_argument("DPointCurveEmit radius is invalid");
        }

        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const long seed = seedHandle.asInt();
        std::mt19937 eng(static_cast<unsigned int>(seed));

        std::uniform_real_distribution<float> distribx(-1.0f, 1.0f);
        std::uniform_real_distribution<float> distriby(-1.0f, 1.0f);

        MPlug radRmpPlug(thisMObject(), aRadiusRamp);
        MRampAttribute radiusRampAttr(radRmpPlug, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);

        MFloatArray radiusValueArray;
        radiusRampAttr.sampleValueRamp(static_cast<unsigned int>(pathSampleCount), radiusValueArray,
                                       &stat);
        if (!stat || radiusValueArray.length() != pathSampleCount) {
            throw std::runtime_error("DPointCurveEmit could not sample the radius ramp");
        }

        for (std::size_t c = 0U, cn = curveList.length(); c < cn; ++c) {

            MFnNurbsCurve curveFn;
            stat = curveFn.setObject(curveList[static_cast<unsigned int>(c)]);
            if (!stat) {
                throw std::runtime_error("DPointCurveEmit received an invalid curve");
            }

            const double totalLength = curveFn.length(kMFnNurbsEpsilon, &stat);
            if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                throw std::runtime_error("DPointCurveEmit curve length is invalid");
            }
            const double stepLength = totalLength / static_cast<double>(sampleCount);

            // emit
            for (std::size_t i = 0U; i < pathSampleCount; ++i) {

                const double u =
                    curveFn.findParamFromLength(stepLength * static_cast<double>(i), &stat);
                if (!stat || !std::isfinite(u)) {
                    throw std::runtime_error("DPointCurveEmit could not convert curve length");
                }
                MVector tanVec = normalizedOrZero(curveFn.tangent(u, MSpace::kWorld, &stat));
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read a curve tangent");
                }
                MVector normVec = normalizedOrZero(curveFn.normal(u, MSpace::kWorld, &stat));
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read a curve normal");
                }
                MVector NxT = normalizedOrZero(normVec ^ tanVec);
                requireValidFrame(tanVec, normVec, NxT);

                MPoint wsP;
                stat = curveFn.getPointAtParam(u, wsP, MSpace::kWorld);
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not sample a curve");
                }

                const float rampValue = radiusValueArray[static_cast<unsigned int>(i)];
                if (!std::isfinite(rampValue)) {
                    throw std::runtime_error(
                        "DPointCurveEmit radius ramp returned a non-finite value");
                }
                const float radiusValue = radius * rampValue;

                for (std::size_t j = 0U; j < fillSampling; ++j) {

                    float s = distribx(eng);
                    float t = distriby(eng);

                    if (!dommy::math::isInsideUnitCircle(s, t)) {
                        continue;
                    }

                    MPoint outP = wsP;

                    outP += NxT * s * radiusValue;
                    outP += normVec * t * radiusValue;

                    appendPointChecked(pts, DPoint(outP[0], outP[1], outP[2]));
                }
            }
        }

    } else if (samplTypeVal == D_CURVE_SAMPLING_TUBE_OUTER) {
        const MDataHandle outerTypeHandle = data.inputValue(aOuterType, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int outerType = outerTypeHandle.asInt();
        const MDataHandle rawCircleSamplingHandle = data.inputValue(aOuterCircleSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawCircleSampling = rawCircleSamplingHandle.asInt();
        const MDataHandle rawRandomSamplingHandle = data.inputValue(aOuterRandomSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawRandomSampling = rawRandomSamplingHandle.asInt();
        if (outerType < 0 || outerType > 1) {
            throw std::invalid_argument("DPointCurveEmit outer sampling type is invalid");
        }

        std::size_t circleSampling = 0U;
        std::size_t randomSampling = 0U;
        if (!dommy::generation::checkedCount(
                rawCircleSampling, dommy::generation::kMaxGeneratedPointCount, circleSampling) ||
            !dommy::generation::checkedCount(
                rawRandomSampling, dommy::generation::kMaxGeneratedPointCount, randomSampling)) {
            throw std::invalid_argument("DPointCurveEmit outer sampling count is invalid");
        }
        const std::size_t ringSampling = outerType == 0 ? circleSampling : randomSampling;
        requireWorkBudget(curveList.length(), pathSampleCount, ringSampling);
        if (ringSampling == 0U) {
            return MS::kSuccess;
        }

        const MDataHandle radiusHandle = data.inputValue(aRadius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float radius = radiusHandle.asFloat();
        if (!std::isfinite(radius) || radius < 0.0f) {
            throw std::invalid_argument("DPointCurveEmit radius is invalid");
        }

        MPlug radiusRampPlug(thisMObject(), aRadiusRamp);
        MRampAttribute radiusRamp(radiusRampPlug, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        MFloatArray radiusValues;
        radiusRamp.sampleValueRamp(static_cast<unsigned int>(pathSampleCount), radiusValues, &stat);
        if (!stat || radiusValues.length() != pathSampleCount) {
            throw std::runtime_error("DPointCurveEmit could not sample the radius ramp");
        }

        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const long seed = seedHandle.asInt();
        std::mt19937 engine(static_cast<unsigned int>(seed));
        std::uniform_real_distribution<double> angleDistribution(0.0, 1.0);

        for (std::size_t curveIndex = 0U; curveIndex < curveList.length(); ++curveIndex) {
            MFnNurbsCurve curveFn;
            stat = curveFn.setObject(curveList[static_cast<unsigned int>(curveIndex)]);
            if (!stat) {
                throw std::runtime_error("DPointCurveEmit received an invalid curve");
            }

            const double totalLength = curveFn.length(kMFnNurbsEpsilon, &stat);
            if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                throw std::runtime_error("DPointCurveEmit curve length is invalid");
            }
            const double stepLength = totalLength / static_cast<double>(sampleCount);

            for (std::size_t pathIndex = 0U; pathIndex < pathSampleCount; ++pathIndex) {
                const double parameter =
                    curveFn.findParamFromLength(stepLength * static_cast<double>(pathIndex), &stat);
                if (!stat || !std::isfinite(parameter)) {
                    throw std::runtime_error("DPointCurveEmit could not convert curve length");
                }

                const MVector tangent =
                    normalizedOrZero(curveFn.tangent(parameter, MSpace::kWorld, &stat));
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read a curve tangent");
                }
                const MVector normal =
                    normalizedOrZero(curveFn.normal(parameter, MSpace::kWorld, &stat));
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read a curve normal");
                }
                const MVector binormal = normalizedOrZero(normal ^ tangent);
                requireValidFrame(tangent, normal, binormal);

                MPoint curvePoint;
                stat = curveFn.getPointAtParam(parameter, curvePoint, MSpace::kWorld);
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not sample a curve");
                }

                const float rampValue = radiusValues[static_cast<unsigned int>(pathIndex)];
                if (!std::isfinite(rampValue)) {
                    throw std::runtime_error(
                        "DPointCurveEmit radius ramp returned a non-finite value");
                }
                const double ringRadius = static_cast<double>(radius) * rampValue;

                for (std::size_t ringIndex = 0U; ringIndex < ringSampling; ++ringIndex) {
                    double x = 0.0;
                    double y = 0.0;
                    const bool validOffset =
                        outerType == 0
                            ? dommy::generation::orderedUnitCircleOffset(ringIndex, ringSampling, x,
                                                                         y)
                            : dommy::generation::unitCircleOffset(angleDistribution(engine), x, y);
                    if (!validOffset) {
                        throw std::runtime_error(
                            "DPointCurveEmit could not generate a ring offset");
                    }

                    MPoint outputPoint = curvePoint;
                    outputPoint += binormal * x * ringRadius;
                    outputPoint += normal * y * ringRadius;
                    appendPointChecked(pts, DPoint(outputPoint[0], outputPoint[1], outputPoint[2]));
                }
            }
        }
    } else if (samplTypeVal == D_CURVE_SAMPLING_PYRO) {

        const MDataHandle rawSegmentSamplingHandle = data.inputValue(aFillSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int rawSegmentSampling = rawSegmentSamplingHandle.asInt();
        std::size_t segmentSampling = 0U;
        if (!dommy::generation::checkedCount(
                rawSegmentSampling, dommy::generation::kMaxGeneratedPointCount, segmentSampling)) {
            throw std::invalid_argument("DPointCurveEmit segment sampling is invalid");
        }
        requireWorkBudget(curveList.length(), pathSampleCount, segmentSampling);

        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const long seed = seedHandle.asInt();
        const MDataHandle amplitubeHandle = data.inputValue(aAmplitube, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float amplitube = amplitubeHandle.asFloat();
        const MDataHandle octaveHandle = data.inputValue(aOctave, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int octave = octaveHandle.asInt();
        const MDataHandle freqHandle = data.inputValue(aFrequency, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float freq = freqHandle.asFloat();
        const MDataHandle lacunarityHandle = data.inputValue(aLacunarity, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float lacunarity = lacunarityHandle.asFloat();
        const MDataHandle persistenceHandle = data.inputValue(aPersistence, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float persistence = persistenceHandle.asFloat();

        const MDataHandle offsetVecHandle = data.inputValue(aNoiseOffset, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MFloatVector offsetVec = offsetVecHandle.asFloatVector();
        const MDataHandle scalevalHandle = data.inputValue(aNoiseScale, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float scaleval = scalevalHandle.asFloat();

        const MDataHandle radiusHandle = data.inputValue(aRadius, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float radius = radiusHandle.asFloat();
        const MDataHandle dispAmpHandle = data.inputValue(aDisplaceAmplitude, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const float dispAmp = dispAmpHandle.asFloat();

        const MDataHandle tanTypeHandle = data.inputValue(aTangentMode, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const short tanType = tanTypeHandle.asShort();
        const MDataHandle attrTangentVecHandle = data.inputValue(aTangentVector, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MVector attrTangentVec = attrTangentVecHandle.asVector();
        if (!std::isfinite(amplitube) || octave < 1 || octave > 30 || !std::isfinite(freq) ||
            freq < 0.0f || !std::isfinite(lacunarity) || lacunarity <= 0.0f ||
            !std::isfinite(persistence) || !std::isfinite(scaleval) ||
            std::abs(scaleval) <= 1.0e-12f || !std::isfinite(radius) || radius < 0.0f ||
            !std::isfinite(dispAmp) || tanType < 0 || tanType > 2 || !std::isfinite(offsetVec.x) ||
            !std::isfinite(offsetVec.y) || !std::isfinite(offsetVec.z) ||
            (tanType == 0 && normalizedOrZero(attrTangentVec).length() <= 1.0e-12)) {
            throw std::invalid_argument("DPointCurveEmit pyro settings are invalid");
        }

        // radius
        MPlug radRmpPlug(thisMObject(), aRadiusRamp);
        MRampAttribute radiusRampAttr(radRmpPlug, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);

        MFloatArray radiusValueArray;
        radiusRampAttr.sampleValueRamp(static_cast<unsigned int>(pathSampleCount), radiusValueArray,
                                       &stat);
        if (!stat || radiusValueArray.length() != pathSampleCount) {
            throw std::runtime_error("DPointCurveEmit could not sample the radius ramp");
        }

        MFnDependencyNode thiFn(thisMObject(), &stat);
        if (!stat) {
            throw std::runtime_error("DPointCurveEmit could not access its dependency node");
        }

        MPlug inTanCrvPlug = thiFn.findPlug(aInTangentCurve, true, &stat);
        if (!stat) {
            throw std::runtime_error("DPointCurveEmit could not access the tangent curve plug");
        }

        bool isTanCrveFn = false;
        MFnNurbsCurve tanCrvFn;
        const bool hasTangentCurve = inTanCrvPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (tanType == 2 && hasTangentCurve) {
            MDataHandle objHnd = data.inputValue(aInTangentCurve, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);

            if (objHnd.type() == MFnData::kNurbsCurve) { // Nurbs Surface Object
                stat = tanCrvFn.setObject(objHnd.data());
                DOMMY_RETURN_IF_FAILURE(stat);

                isTanCrveFn = true;
            }
        }

        std::mt19937 eng(static_cast<unsigned int>(seed));

        std::uniform_real_distribution<float> distribx(-1.0f, 1.0f);
        std::uniform_real_distribution<float> distriby(-1.0f, 1.0f);

        noise::module::Perlin noiseMod;
        noiseMod.SetSeed(seed);
        noiseMod.SetOctaveCount(octave);
        noiseMod.SetFrequency(freq);
        noiseMod.SetLacunarity(lacunarity);
        noiseMod.SetPersistence(persistence);
        noiseMod.SetNoiseQuality(noise::QUALITY_BEST);

        for (std::size_t c = 0U; c < curveList.length(); ++c) {

            MFnNurbsCurve curveFn;
            stat = curveFn.setObject(curveList[static_cast<unsigned int>(c)]);
            if (!stat) {
                throw std::runtime_error("DPointCurveEmit received an invalid curve");
            }

            const double totalLength = curveFn.length(kMFnNurbsEpsilon, &stat);
            if (!stat || !std::isfinite(totalLength) || totalLength < 0.0) {
                throw std::runtime_error("DPointCurveEmit curve length is invalid");
            }
            const double stepLength = totalLength / static_cast<double>(sampleCount);

            for (std::size_t i = 0U; i < pathSampleCount; ++i) {

                const double u =
                    curveFn.findParamFromLength(stepLength * static_cast<double>(i), &stat);
                if (!stat || !std::isfinite(u)) {
                    throw std::runtime_error("DPointCurveEmit could not convert curve length");
                }
                MPoint wsP;
                stat = curveFn.getPointAtParam(u, wsP, MSpace::kWorld);
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not sample a curve");
                }

                MVector tanVec = normalizedOrZero(curveFn.tangent(u, MSpace::kWorld, &stat));
                if (!stat) {
                    throw std::runtime_error("DPointCurveEmit could not read a curve tangent");
                }

                MVector normVec = normalizedOrZero(attrTangentVec);
                if (tanType == 1) {
                    normVec = normalizedOrZero(curveFn.normal(u, MSpace::kWorld, &stat));
                    if (!stat) {
                        throw std::runtime_error("DPointCurveEmit could not read a curve normal");
                    }
                } else if (tanType == 2 && isTanCrveFn) {

                    MPoint closestPt =
                        tanCrvFn.closestPoint(wsP, NULL, kMFnNurbsEpsilon, MSpace::kWorld, &stat);
                    if (!stat) {
                        throw std::runtime_error(
                            "DPointCurveEmit could not sample the tangent curve");
                    }
                    normVec = normalizedOrZero(MVector(closestPt - wsP));

                } else if (tanType == 2) {
                    throw std::invalid_argument(
                        "DPointCurveEmit requires a connected tangent curve");
                }

                MVector NxT = normalizedOrZero(normVec ^ tanVec);
                requireValidFrame(tanVec, normVec, NxT);

                const float rampValue = radiusValueArray[static_cast<unsigned int>(i)];
                if (!std::isfinite(rampValue)) {
                    throw std::runtime_error(
                        "DPointCurveEmit radius ramp returned a non-finite value");
                }
                for (std::size_t j = 0U; j < segmentSampling; ++j) {
                    // Two draws per attempt, including rejected candidates.
                    const float diskX = distribx(eng);
                    const float diskY = distriby(eng);
                    appendPyroDiskCandidate(pts, noiseMod, wsP, NxT, normVec, u,
                                            diskX, diskY, radius, rampValue, scaleval,
                                            offsetVec, dispAmp, amplitube);
                }
            }
        }
    }

    return MS::kSuccess;
}

MStatus DPointCurveEmit::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inCurve = tAttr.create("inCurve", "ic", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute enumFn;
    samplingType = enumFn.create("samplingType", "sat", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("All", D_CURVE_SAMPLING_ALL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Range", D_CURVE_SAMPLING_RANGE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Step", D_CURVE_SAMPLING_STEP));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("CVs", D_CURVE_SAMPLING_CV));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Fill Length", D_CURVE_SAMPLING_LENGTH_FILL));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Tube Fill", D_CURVE_SAMPLING_TUBE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Tube Outer", D_CURVE_SAMPLING_TUBE_OUTER));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Pyro", D_CURVE_SAMPLING_PYRO));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(samplingType));

    sampling = nAttr.create("sampling", "sa", MFnNumericData::kInt, 10, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(sampling));

    curveLength = nAttr.create("curveLength", "crl", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));

    stepSize = nAttr.create("stepSize", "stp", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(stepSize));

    offset = nAttr.create("offset", "of", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));

    aFillLength = nAttr.create("FillLength", "fllen", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFillLength));

    // tangent
    MFnEnumAttribute tanEnumFn;
    aTangentMode = tanEnumFn.create("TangentMode", "tanm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.addField("Vecor", 0));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.addField("From Curve", 1));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.addField("Input Curve", 2));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(tanEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentMode));

    aTangentVector = nAttr.create("TangentVector", "tanvec", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));

    aInTangentCurve =
        tAttr.create("InTangentCurve", "intancrv", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInTangentCurve));

    // tube
    aFillSampling = nAttr.create("FillSampling", "fsa", MFnNumericData::kInt, 10, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFillSampling));

    aRadius = nAttr.create("Radius", "rad", MFnNumericData::kFloat, 2.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aRadiusRamp = MRampAttribute::createCurveRamp("RadiusRamp", "radrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusRamp));

    // tube outer
    MFnEnumAttribute outtEnumFn;
    aOuterType = outtEnumFn.create("OuterType", "outt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outtEnumFn.addField("Order Circle", 0));
    DOMMY_RETURN_IF_FAILURE(outtEnumFn.addField("Random Sample", 1));
    DOMMY_RETURN_IF_FAILURE(outtEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(outtEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOuterType));

    aOuterCircleSampling =
        nAttr.create("OuterCircleSampling", "ocsmp", MFnNumericData::kInt, 8, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOuterCircleSampling));

    aOuterRandomSampling =
        nAttr.create("OuterRandomSampling", "orsmp", MFnNumericData::kInt, 100, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1000));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOuterRandomSampling));

    // pyro
    aDisplaceAmplitude =
        nAttr.create("DisplaceAmplitude", "diamp", MFnNumericData::kFloat, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplaceAmplitude));

    // common noise
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    // common noise
    aAmplitube = nAttr.create("Amplitube", "amp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAmplitube));

    aOctave = nAttr.create("Octave", "oct", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(30));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(6));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOctave));

    aFrequency = nAttr.create("Frequency", "freq", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrequency));

    aLacunarity = nAttr.create("Lacunarity", "lac", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLacunarity));

    aPersistence = nAttr.create("Persistence", "per", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPersistence));

    // noise transform
    aNoiseScale = nAttr.create("NoiseScale", "nsc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseScale));

    aNoiseOffset = nAttr.create("NoiseOffset", "nof", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseOffset));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCurve, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(samplingType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(sampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(curveLength, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(stepSize, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFillLength, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentMode, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTangentVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInTangentCurve, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFillSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadiusRamp, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOuterType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOuterCircleSampling, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOuterRandomSampling, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDisplaceAmplitude, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAmplitube, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrequency, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOctave, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLacunarity, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPersistence, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseScale, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNoiseOffset, outDP));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DPointCurveEmit", "initialize",
                                                  std::current_exception());
}
